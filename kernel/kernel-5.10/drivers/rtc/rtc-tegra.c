// SPDX-License-Identifier: GPL-2.0+
/*
 * drivers/rtc/rtc-tegra.c
 *
 * Copyright (c) 2010-2020, NVIDIA Corporation.
 */

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/rtc.h>
#include <linux/rtc/rtc-tegra.h>
#define CREATE_TRACE_POINTS
#include <trace/events/tegra_rtc.h>
#include <asm/mach/time.h>

/* Set to 1 = busy every eight 32 kHz clocks during copy of sec+msec to AHB. */
#define TEGRA_RTC_REG_BUSY			0x004
#define TEGRA_RTC_REG_SECONDS			0x008
/* When msec is read, the seconds are buffered into shadow seconds. */
#define TEGRA_RTC_REG_SHADOW_SECONDS		0x00c
#define TEGRA_RTC_REG_MILLI_SECONDS		0x010
#define TEGRA_RTC_REG_SECONDS_ALARM0		0x014
#define TEGRA_RTC_REG_SECONDS_ALARM1		0x018
#define TEGRA_RTC_REG_MILLI_SECONDS_ALARM0	0x01c
#define TEGRA_RTC_REG_MSEC_CDN_ALARM0		0x024
#define TEGRA_RTC_REG_INTR_MASK			0x028
/* write 1 bits to clear status bits */
#define TEGRA_RTC_REG_INTR_STATUS		0x02c

/* bits in INTR_MASK */
#define TEGRA_RTC_INTR_MASK_MSEC_CDN_ALARM	(1<<4)
#define TEGRA_RTC_INTR_MASK_SEC_CDN_ALARM	(1<<3)
#define TEGRA_RTC_INTR_MASK_MSEC_ALARM		(1<<2)
#define TEGRA_RTC_INTR_MASK_SEC_ALARM1		(1<<1)
#define TEGRA_RTC_INTR_MASK_SEC_ALARM0		(1<<0)

/* bits in INTR_STATUS */
#define TEGRA_RTC_INTR_STATUS_MSEC_CDN_ALARM	(1<<4)
#define TEGRA_RTC_INTR_STATUS_SEC_CDN_ALARM	(1<<3)
#define TEGRA_RTC_INTR_STATUS_MSEC_ALARM	(1<<2)
#define TEGRA_RTC_INTR_STATUS_SEC_ALARM1	(1<<1)
#define TEGRA_RTC_INTR_STATUS_SEC_ALARM0	(1<<0)

/* Reference selection */
#define TEGRA_RTC_RTCRSR			0x038
#define TEGRA_RTC_RTCRSR_FR			(1<<0)
#define TEGRA_RTC_RTCRSR_MBS(x)			(((x) & 3) << 4)
#define TEGRA_RTC_RTCDR				0x03c
#define TEGRA_RTC_RTCDR_D(x)			(((x) & 0xffff) << 16)
#define TEGRA_RTC_RTCDR_N(x)			((x) & 0xffff)

/* Recommended values for reference and dividor */
/* RTC follows MTSC bit 11 (9+2) */
#define TEGRA_RTC_RTCRSR_USE_MTSC		(0x20)
/* N=1024 D=15625 assuming FNOM=31250 program n-1 */
#define TEGRA_RTC_RTCDR_USE_MTSC		(0x3D0803ff)


struct tegra_rtc_chip_data {
	bool has_clock;
	bool follow_tsc;
};

struct tegra_rtc_info {
	struct platform_device *pdev;
	struct rtc_device *rtc;
	void __iomem *base; /* NULL if not initialized */
	int irq; /* alarm and periodic IRQ */
	spinlock_t lock;
	bool is_tegra_rtc_suspended;
};

static struct tegra_rtc_info *tegra_rtc_dev;

/*
 * tegra_rtc_read - Reads the Tegra RTC registers
 * Care must be taken that this funciton is not called while the
 * tegra_rtc driver could be executing to avoid race conditions
 * on the RTC shadow register
 */
u64 tegra_rtc_read_ms(void)
{
	u32 ms = readl(tegra_rtc_dev->base + RTC_MILLISECONDS);
	u32 s = readl(tegra_rtc_dev->base + RTC_SHADOW_SECONDS);

	return (u64)s * MSEC_PER_SEC + ms;
}
EXPORT_SYMBOL(tegra_rtc_read_ms);

/*
 * RTC hardware is busy when it is updating its values over AHB once every
 * eight 32 kHz clocks (~250 us). Outside of these updates the CPU is free to
 * write. CPU is always free to read.
 */
static inline u32 tegra_rtc_check_busy(struct tegra_rtc_info *info)
{
	return readl(info->base + TEGRA_RTC_REG_BUSY) & 1;
}

/*
 * Wait for hardware to be ready for writing. This function tries to maximize
 * the amount of time before the next update. It does this by waiting for the
 * RTC to become busy with its periodic update, then returning once the RTC
 * first becomes not busy.
 *
 * This periodic update (where the seconds and milliseconds are copied to the
 * AHB side) occurs every eight 32 kHz clocks (~250 us). The behavior of this
 * function allows us to make some assumptions without introducing a race,
 * because 250 us is plenty of time to read/write a value.
 */
static int tegra_rtc_wait_while_busy(struct device *dev, bool is_read)
{
	struct tegra_rtc_info *info = dev_get_drvdata(dev);
	int retries = 500; /* ~490 us is the worst case, ~250 us is best */

	/*
	 * First wait for the RTC to become busy. This is when it posts its
	 * updated seconds+msec registers to AHB side.
	 */
	while (tegra_rtc_check_busy(info)) {
		if (!retries--)
			goto retry_failed;

		udelay(1);
	}

	/* Updated value can take nearly 250us to reflect in shadow
	 * registers. Wait to read latest value.
	 */
	if (is_read)
		udelay(250);

	/* now we have about 250 us to manipulate registers */
	return 0;

retry_failed:
	dev_err(dev, "write failed: retry count exceeded\n");
	return -ETIMEDOUT;
}

static int tegra_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct tegra_rtc_info *info = dev_get_drvdata(dev);
	unsigned long flags;
	u32 sec;
	int ret;

	/* Ensure there is no pending write to get latest time */
	ret = tegra_rtc_wait_while_busy(dev, true);
	if (ret)
		dev_warn(dev, "Reading old value\n");

	/*
	 * RTC hardware copies seconds to shadow seconds when a read of
	 * milliseconds occurs. use a lock to keep other threads out.
	 */
	spin_lock_irqsave(&info->lock, flags);

	readl(info->base + TEGRA_RTC_REG_MILLI_SECONDS);
	sec = readl(info->base + TEGRA_RTC_REG_SHADOW_SECONDS);

	spin_unlock_irqrestore(&info->lock, flags);

	rtc_time64_to_tm(sec, tm);

	dev_vdbg(dev, "time read as %u, %ptR\n", sec, tm);

	return 0;
}

static int tegra_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct tegra_rtc_info *info = dev_get_drvdata(dev);
	u32 sec;
	int ret;

	/* convert tm to seconds */
	sec = rtc_tm_to_time64(tm);

	dev_vdbg(dev, "time set to %u, %ptR\n", sec, tm);

	/* seconds only written if wait succeeded */
	ret = tegra_rtc_wait_while_busy(dev, false);
	if (!ret)
		writel(sec, info->base + TEGRA_RTC_REG_SECONDS);

	dev_vdbg(dev, "time read back as %d\n",
		 readl(info->base + TEGRA_RTC_REG_SECONDS));

	return ret;
}

static int tegra_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	struct tegra_rtc_info *info = dev_get_drvdata(dev);
	u32 sec, value;
	int ret;

	ret = tegra_rtc_wait_while_busy(dev, true);
	if (ret)
		dev_warn(dev, "Reading old value\n");

	sec = readl(info->base + TEGRA_RTC_REG_SECONDS_ALARM0);

	if (sec == 0) {
		/* alarm is disabled */
		alarm->enabled = 0;
	} else {
		/* alarm is enabled */
		alarm->enabled = 1;
		rtc_time64_to_tm(sec, &alarm->time);
	}

	value = readl(info->base + TEGRA_RTC_REG_INTR_STATUS);
	alarm->pending = (value & TEGRA_RTC_INTR_STATUS_SEC_ALARM0) != 0;

	return 0;
}

static int tegra_rtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	struct tegra_rtc_info *info = dev_get_drvdata(dev);
	unsigned long flags;
	u32 status;
	int ret;

	ret = tegra_rtc_wait_while_busy(dev, false);
	if (ret) {
		dev_err(dev, "Timeout accessing RTC\n");
		return ret;
	}

	spin_lock_irqsave(&info->lock, flags);

	/* read the original value, and OR in the flag */
	status = readl(info->base + TEGRA_RTC_REG_INTR_MASK);
	if (enabled)
		status |= TEGRA_RTC_INTR_MASK_SEC_ALARM0; /* set it */
	else
		status &= ~TEGRA_RTC_INTR_MASK_SEC_ALARM0; /* clear it */

	writel(status, info->base + TEGRA_RTC_REG_INTR_MASK);

	spin_unlock_irqrestore(&info->lock, flags);

	return 0;
}

static int __tegra_rtc_set_alarm(struct device *dev, unsigned long period, bool enabled)
{
	struct tegra_rtc_info *info = dev_get_drvdata(dev);
	u32 sec, msec;
	int ret;

	ret = tegra_rtc_wait_while_busy(dev, false);
	if (ret) {
		dev_err(dev, "Timeout accessing RTC\n");
		return ret;
	}
	writel(sec, info->base + TEGRA_RTC_REG_SECONDS_ALARM0);
	dev_vdbg(dev, "alarm read back as %d\n",
		 readl(info->base + TEGRA_RTC_REG_SECONDS_ALARM0));

	msec = readl(info->base + TEGRA_RTC_REG_MILLI_SECONDS);
	sec = readl(info->base + TEGRA_RTC_REG_SHADOW_SECONDS);
	if (period < sec)
		dev_warn(dev, "alarm time set in past\n");

	writel(period, info->base + TEGRA_RTC_REG_SECONDS_ALARM0);

	ret = tegra_rtc_alarm_irq_enable(dev, enabled);
	if (ret < 0) {
		dev_err(dev, "rtc_set_alarm: Failed to enable rtc alarm\n");
		return ret;
	}

	trace_tegra_rtc_set_alarm(sec * MSEC_PER_SEC + msec, period * MSEC_PER_SEC);

	dev_dbg(dev, "alarm set to fire after %lu sec\n", (period - sec));

	return 0;
}

static int tegra_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	unsigned long period = rtc_tm_to_time64(&alarm->time);

	return __tegra_rtc_set_alarm(dev, period, alarm->enabled);
}

static int tegra_rtc_proc(struct device *dev, struct seq_file *seq)
{
	if (!dev || !dev->driver)
		return 0;

	seq_printf(seq, "name\t\t: %s\n", dev_name(dev));

	return 0;
}

static irqreturn_t tegra_rtc_irq_handler(int irq, void *data)
{
	struct device *dev = data;
	struct tegra_rtc_info *info = dev_get_drvdata(dev);
	unsigned int sec, msec;
	unsigned long flags;
	u32 status, mask;
	int ret;

	tegra_rtc_alarm_irq_enable(dev, 0);
	msec = readl(info->base + TEGRA_RTC_REG_MILLI_SECONDS);
	sec = readl(info->base + TEGRA_RTC_REG_SHADOW_SECONDS);
	trace_tegra_rtc_irq_handler(__func__, sec * MSEC_PER_SEC + msec);

	status = readl(info->base + TEGRA_RTC_REG_INTR_STATUS);
	mask = readl(info->base + TEGRA_RTC_REG_INTR_MASK);
	mask &= ~status;
	if (status) {
		/* clear the interrupt masks and status on any irq. */
		ret = tegra_rtc_wait_while_busy(dev, false);
		if (ret)
			dev_warn(dev, "Reading old value\n");

		spin_lock_irqsave(&info->lock, flags);
		writel(mask, info->base + TEGRA_RTC_REG_INTR_MASK);
		spin_unlock_irqrestore(&info->lock, flags);

		ret = tegra_rtc_wait_while_busy(dev, false);
		if (ret)
			dev_warn(dev, "Reading old value\n");

		spin_lock_irqsave(&info->lock, flags);
		writel(status, info->base + TEGRA_RTC_REG_INTR_STATUS);
		spin_unlock_irqrestore(&info->lock, flags);
	}

	rtc_update_irq(info->rtc, 1, RTC_IRQF | RTC_UF);

	/* If we are in suspend state and we get an RTC interrupt, wake up */
	if (device_may_wakeup(dev) && info->is_tegra_rtc_suspended)
		pm_wakeup_event(dev, 0);

	return IRQ_HANDLED;
}

static int tegra_rtc_msec_alarm_irq_enable(struct tegra_rtc_info *info,
					   unsigned int enable)
{
	u32 status;

	/* read the original value, and OR in the flag. */
	status = readl(info->base + TEGRA_RTC_REG_INTR_MASK);
	if (enable)
		status |= TEGRA_RTC_INTR_MASK_MSEC_CDN_ALARM; /* set it */
	else
		status &= ~TEGRA_RTC_INTR_MASK_MSEC_CDN_ALARM; /* clear it */

	writel(status, info->base + TEGRA_RTC_REG_INTR_MASK);

	return 0;
}

void tegra_rtc_set_trigger(unsigned long cycles)
{
	struct tegra_rtc_info *info = tegra_rtc_dev;
	struct device *dev = &info->pdev->dev;
	unsigned long msec;
	unsigned long now;
	unsigned long tgt;
	int ret;

	/* Convert to msec */
	msec = cycles / 1000;

	if (msec)
		msec = 0x80000000UL | (0x0fffffff & msec);

	ret = tegra_rtc_wait_while_busy(dev, true);
	if (ret)
		dev_warn(dev, "Reading old value\n");

	now = readl(info->base + TEGRA_RTC_REG_MILLI_SECONDS);
	now += readl(info->base + TEGRA_RTC_REG_SHADOW_SECONDS) * MSEC_PER_SEC;
	tgt = now + cycles / 1000;

	writel(msec, info->base + TEGRA_RTC_REG_MSEC_CDN_ALARM0);
	trace_tegra_rtc_set_alarm(now, tgt);

	ret = tegra_rtc_wait_while_busy(dev, false);
	if (ret)
		dev_warn(dev, "Reading old value\n");

	tegra_rtc_msec_alarm_irq_enable(info, msec ? 1 : 0);
}
EXPORT_SYMBOL(tegra_rtc_set_trigger);

#ifdef CONFIG_PM_SLEEP
static void tegra_rtc_debug_set_alarm(struct device *dev, unsigned int period)
{
	struct tegra_rtc_info *info = dev_get_drvdata(dev);
	unsigned int sec;
	int ret;

	sec = readl(info->base + TEGRA_RTC_REG_SECONDS);
	ret = __tegra_rtc_set_alarm(dev, (sec + period), 1);
	if (ret < 0)
		pr_err("Tegra RTC: setting debug alarm failed\n");
}
#endif

static const struct rtc_class_ops tegra_rtc_ops = {
	.read_time = tegra_rtc_read_time,
	.set_time = tegra_rtc_set_time,
	.read_alarm = tegra_rtc_read_alarm,
	.set_alarm = tegra_rtc_set_alarm,
	.proc = tegra_rtc_proc,
	.alarm_irq_enable = tegra_rtc_alarm_irq_enable,
};

static unsigned int alarm_period;
static unsigned int alarm_period_msec;

static int alarm_set(void *data, u64 val)
{
	alarm_period = val;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(alarm_fops, NULL, alarm_set, "%llu\n");

static int alarm_msec_set(void *data, u64 val)
{
	alarm_period_msec = val;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(alarm_msec_fops, NULL, alarm_msec_set, "%llu\n");

static struct dentry *pm_dentry;

static int debugfs_init(void)
{
	struct dentry *root = NULL;

	root = debugfs_create_dir("tegra-rtc", NULL);
	if (!root)
		return -ENOMEM;

	if (!debugfs_create_file("alarm", 0200, root, NULL, &alarm_fops))
		goto err_out;

	if (!debugfs_create_file("alarm_msec", 0200, root, NULL, &alarm_msec_fops))
		goto err_out;

	pm_dentry = root;
	return 0;

err_out:
	debugfs_remove_recursive(root);
	return -ENOMEM;
}

/*
 * tegra_read_persistent_clock - Return time from a persistent clock.
 *
 * Reads the time from a source which isn't disabled during PM, the
 * 32k sync timer.  Convert the cycles elapsed since last read into
 * nsecs and adds to a monotonically increasing timespec.
 * Care must be taken that this funciton is not called while the
 * tegra_rtc driver could be executing to avoid race conditions
 * on the RTC shadow register
 */
static void tegra_rtc_read_persistent_clock(struct timespec64 *ts)
{
	ts->tv_nsec = NSEC_PER_MSEC * readl(tegra_rtc_dev->base + RTC_MILLISECONDS);
	ts->tv_sec = readl(tegra_rtc_dev->base + RTC_SHADOW_SECONDS);
}

static struct tegra_rtc_chip_data t18x_rtc_cdata = {
	.has_clock = false,
	.follow_tsc = false,
};

static struct tegra_rtc_chip_data tegra_rtc_cdata = {
	.has_clock = true,
	.follow_tsc = false,
};

static struct tegra_rtc_chip_data t20x_rtc_cdata = {
	.has_clock = false,
	.follow_tsc = false,
};

static const struct of_device_id tegra_rtc_dt_match[] = {
	{
		.compatible = "nvidia,tegra-rtc",
		.data = &tegra_rtc_cdata,
	},
	{
		.compatible = "nvidia,tegra18-rtc",
		.data = &t18x_rtc_cdata,
	},
	{
		.compatible = "nvidia,tegra20-rtc",
		.data = &t20x_rtc_cdata,
	},
	{}
};
MODULE_DEVICE_TABLE(of, tegra_rtc_dt_match);

static void tegra_rtc_follow_tsc(struct device *dev)
{
	struct tegra_rtc_info *info = dev_get_drvdata(dev);
	int ret;

	ret = tegra_rtc_wait_while_busy(dev, false);
	if (ret) {
		dev_err(&info->pdev->dev, "Timeout accessing Tegra RTC\n");
		return;
	}

	writel(TEGRA_RTC_RTCDR_USE_MTSC, info->base + TEGRA_RTC_RTCDR);

	ret = tegra_rtc_wait_while_busy(dev, false);
	if (ret) {
		dev_err(&info->pdev->dev, "Timeout accessing Tegra RTC\n");
		return;
	}

	writel(TEGRA_RTC_RTCRSR_USE_MTSC, info->base + TEGRA_RTC_RTCRSR);
}

static int __init tegra_rtc_probe(struct platform_device *pdev)
{
	struct tegra_rtc_info *info;
	const struct of_device_id *match;
	const struct tegra_rtc_chip_data *cdata = NULL;
	struct clk *clk;
	int ret;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		match = of_match_device(tegra_rtc_dt_match, &pdev->dev);
		if (match)
			cdata = match->data;
	}

	if (!cdata)
		return -EINVAL;

	info->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(info->base))
		return PTR_ERR(info->base);

	tegra_rtc_dev = info;

	ret = platform_get_irq(pdev, 0);
	if (ret <= 0)
		return ret;

	info->irq = ret;

	info->rtc = devm_rtc_allocate_device(&pdev->dev);
	if (IS_ERR(info->rtc))
		return PTR_ERR(info->rtc);

	info->rtc->ops = &tegra_rtc_ops;
	info->rtc->range_max = U32_MAX;

	/* set context info */
	info->pdev = pdev;
	spin_lock_init(&info->lock);

	platform_set_drvdata(pdev, info);

	if (cdata->has_clock) {
		clk = devm_clk_get(&pdev->dev, "rtc");
		if (IS_ERR(clk))
			clk = clk_get_sys("rtc-tegra", NULL);
		if (IS_ERR(clk))
			dev_warn(&pdev->dev, "Unable to get rtc-tegra clock\n");
		else
			clk_prepare_enable(clk);
	}

	/* clear out the hardware. */
	ret = tegra_rtc_wait_while_busy(&pdev->dev, false);
	if (ret)
		dev_warn(&pdev->dev, "Reading old value\n");
	writel(0, info->base + TEGRA_RTC_REG_MSEC_CDN_ALARM0);
	ret = tegra_rtc_wait_while_busy(&pdev->dev, false);
	if (ret)
		dev_warn(&pdev->dev, "Reading old value\n");
	writel(0xffffffff, info->base + TEGRA_RTC_REG_INTR_STATUS);
	ret = tegra_rtc_wait_while_busy(&pdev->dev, false);
	if (ret)
		dev_warn(&pdev->dev, "Reading old value\n");
	writel(0, info->base + TEGRA_RTC_REG_INTR_MASK);

	if (cdata->follow_tsc)
		tegra_rtc_follow_tsc(&pdev->dev);

	ret = devm_request_threaded_irq(&pdev->dev, info->irq,
					NULL, tegra_rtc_irq_handler,
					IRQF_ONESHOT | IRQF_EARLY_RESUME,
					"tegra_rtc", &pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register RTC IRQ: %d\n", ret);
		return ret;
	}
	device_init_wakeup(&pdev->dev, 1);

	ret = rtc_register_device(info->rtc);
	if (ret)
		return ret;

	ret = debugfs_init();
	if (ret) {
		pr_err("%s: Can't init debugfs", __func__);
		return ret;
	}
	register_persistent_clock(tegra_rtc_read_persistent_clock);

	dev_notice(&pdev->dev, "Tegra internal Real Time Clock\n");

	return 0;
}

static int tegra_rtc_remove(struct platform_device *pdev)
{
	debugfs_remove_recursive(pm_dentry);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tegra_rtc_suspend(struct device *dev)
{
	struct tegra_rtc_info *info = dev_get_drvdata(dev);
	int ret;

	ret = tegra_rtc_wait_while_busy(dev, false);
	if (ret) {
		dev_err(dev, "Timeout accessing RTC\n");
		return ret;
	}

	dev_vdbg(dev, "Suspend (device_may_wakeup=%d) IRQ:%d\n",
		 device_may_wakeup(dev), info->irq);

	if (alarm_period)
		tegra_rtc_debug_set_alarm(dev, alarm_period);

	if (alarm_period_msec)
		tegra_rtc_set_trigger(alarm_period_msec * 1000);

	/* leave the alarms on as a wake source */
	if (alarm_period || alarm_period_msec || device_may_wakeup(dev))
		enable_irq_wake(info->irq);

	info->is_tegra_rtc_suspended = true;

	return 0;
}

static int tegra_rtc_resume(struct device *dev)
{
	struct tegra_rtc_info *info = dev_get_drvdata(dev);

	dev_vdbg(dev, "Resume (device_may_wakeup=%d)\n",
		 device_may_wakeup(dev));

	/* alarms were left on as a wake source, turn them off */
	if (alarm_period || alarm_period_msec || device_may_wakeup(dev))
		disable_irq_wake(info->irq);

	info->is_tegra_rtc_suspended = false;

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(tegra_rtc_pm_ops, tegra_rtc_suspend, tegra_rtc_resume);

static void tegra_rtc_shutdown(struct platform_device *pdev)
{
	dev_vdbg(&pdev->dev, "disabling interrupts\n");
	tegra_rtc_alarm_irq_enable(&pdev->dev, 0);
}

static struct platform_driver tegra_rtc_driver = {
	.remove = tegra_rtc_remove,
	.shutdown = tegra_rtc_shutdown,
	.driver = {
		.name = "tegra_rtc",
		.of_match_table = tegra_rtc_dt_match,
		.pm = &tegra_rtc_pm_ops,
	},
};
module_platform_driver_probe(tegra_rtc_driver, tegra_rtc_probe);

MODULE_AUTHOR("Jon Mayo <jmayo@nvidia.com>");
MODULE_DESCRIPTION("driver for Tegra internal RTC");
MODULE_LICENSE("GPL");
