/*
 * drivers/misc/tegra-cec/tegra_cec.c
 *
 * Copyright (c) 2012-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/of_device.h>
#include <linux/version.h>

#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/clk/tegra.h>
#include <linux/of.h>

#if defined(CONFIG_TEGRA_POWERGATE)
#include <soc/tegra/tegra_powergate.h>
#endif
#include "tegra_cec.h"

#include "dc.h"
#include "dc_priv.h"

#define LOGICAL_ADDRESS_RESERVED2 0xD
#define LOGICAL_ADDRESS_TV 0x0
#define LOGICAL_ADDRESS_BROADCAST 0xF
#define TEXT_VIEW_ON 0x0D
#define ACTIVE_SOURCE 0x82
/*
 * 400 ms is the time it takes for one 16 byte message to be
 * transferred and 5 is the maximum number of retries. Add
 * another 100 ms as a margin.
 */
#define CEC_XFER_TIMEOUT_MS (5 * 400 + 100)

static bool post_recovery, text_view_on_sent;
static u8 text_view_on_command[] = {
	LOGICAL_ADDRESS_RESERVED2 << 4 | LOGICAL_ADDRESS_TV,
	TEXT_VIEW_ON
};
static u8 active_source_command[] = {
	LOGICAL_ADDRESS_RESERVED2 << 4 | LOGICAL_ADDRESS_BROADCAST,
	ACTIVE_SOURCE,
	0x00,
	0x00
};

static ssize_t cec_logical_addr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t cec_logical_addr_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static DEVICE_ATTR(cec_logical_addr_config, S_IWUSR | S_IRUGO,
		cec_logical_addr_show, cec_logical_addr_store);

/* keeping this for debug support to track register read/writes */
struct tegra_cec *tegra_cec_global;

/* Try again in case of reset control failure */
static int reset_retry_count = 5;

static void tegra_cec_writel(u32 value, void __iomem *addr)
{
	/* TODO, for T23x, find out why this delay required */
	if (tegra_cec_global->soc->cec_always_on)
		mdelay(1);
	writel(value, addr);
}

static u32 tegra_cec_readl(const void __iomem *addr)
{
	return readl(addr);
}

static int tegra_cec_open(struct inode *inode, struct file *file)
{
	struct miscdevice *miscdev = file->private_data;
	struct tegra_cec *cec = container_of(miscdev,
		struct tegra_cec, misc_dev);
	int ret = 0;

	dev_dbg(cec->dev, "%s\n", __func__);

	ret = wait_event_interruptible(cec->init_waitq,
	    atomic_read(&cec->init_done) == 1);
	if (ret)
		return ret;
	file->private_data = cec;

	return ret;
}

static int tegra_cec_release(struct inode *inode, struct file *file)
{
	struct tegra_cec *cec = file->private_data;

	dev_dbg(cec->dev, "%s\n", __func__);

	return 0;
}

static inline void tegra_cec_native_tx(const struct tegra_cec *cec, u32 block)
{
	tegra_cec_writel(block, cec->cec_base + TEGRA_CEC_TX_REGISTER);
	tegra_cec_writel(TEGRA_CEC_INT_STAT_TX_REGISTER_EMPTY,
		cec->cec_base + TEGRA_CEC_INT_STAT);

}

static inline void tegra_cec_error_recovery(struct tegra_cec *cec)
{
	u32 hw_ctrl;

	hw_ctrl = tegra_cec_readl(cec->cec_base + TEGRA_CEC_HW_CONTROL);
	tegra_cec_writel(0x0, cec->cec_base + TEGRA_CEC_HW_CONTROL);
	if (cec->soc->cec_always_on)
		tegra_cec_writel(0xFFFFFFFE, cec->cec_base + TEGRA_CEC_INT_STAT);
	else
		tegra_cec_writel(0xFFFFFFFF, cec->cec_base + TEGRA_CEC_INT_STAT);
	tegra_cec_writel(hw_ctrl, cec->cec_base + TEGRA_CEC_HW_CONTROL);
}

static
int tegra_cec_native_write_l(struct tegra_cec *cec, const u8 *buf, size_t cnt)
{
	int ret;
	size_t i;
	u32 start, mode, eom;
	u32 mask;

	/*
	 * In case previous transmission was interrupted by signal,
	 *  driver will try to complete the frame anyway.  However,
	 *  this means we have to wait for it to finish before beginning
	 *  subsequent transmission.
	 */
	ret = wait_event_interruptible_timeout(cec->tx_waitq, cec->tx_wake == 1,
			msecs_to_jiffies(CEC_XFER_TIMEOUT_MS));
	if (ret == 0)
		return -ETIME;
	else if (ret < 0)
		return ret;

	mode = TEGRA_CEC_LADDR_MODE(buf[0]) << TEGRA_CEC_TX_REG_ADDR_MODE_SHIFT;

	cec->tx_wake = 0;
	cec->tx_error = 0;
	cec->tx_buf_cur = 0;
	cec->tx_buf_cnt = cnt;

	for (i = 0; i < cnt; i++) {
		start = i == 0 ? (1 << TEGRA_CEC_TX_REG_START_BIT_SHIFT) : 0;
		eom   = i == cnt-1 ? (1 << TEGRA_CEC_TX_REG_EOM_SHIFT) : 0;
		cec->tx_buf[i] = start | mode | eom | buf[i];
	}

	mask = tegra_cec_readl(cec->cec_base + TEGRA_CEC_INT_MASK);
	tegra_cec_writel(mask | TEGRA_CEC_INT_MASK_TX_REGISTER_EMPTY,
		cec->cec_base + TEGRA_CEC_INT_MASK);

	ret = wait_event_interruptible_timeout(cec->tx_waitq, cec->tx_wake == 1,
			msecs_to_jiffies(CEC_XFER_TIMEOUT_MS));
	if (ret > 0) {
		ret = cec->tx_error;
	} else if (ret == 0) {
		dev_err(cec->dev, "timeout in %s:%d.", __func__, __LINE__);
		tegra_cec_error_recovery(cec);
		cec->tx_wake = 1;
		ret = -ETIME;
	}

	return ret;
}

static ssize_t tegra_cec_write(struct file *file, const char __user *buf,
	size_t count, loff_t *ppos)
{
	u8 tx_buf[TEGRA_CEC_FRAME_MAX_LENGTH];
	struct tegra_cec *cec = file->private_data;
	ssize_t ret;

	if (count == 0 || count > TEGRA_CEC_FRAME_MAX_LENGTH)
		return -EMSGSIZE;

	ret = wait_event_interruptible(cec->init_waitq,
	    atomic_read(&cec->init_done) == 1);
	if (ret)
		return ret;

	if (copy_from_user(tx_buf, buf, count))
		return -EFAULT;

	mutex_lock(&cec->tx_lock);
	ret = tegra_cec_native_write_l(cec, tx_buf, count);
	mutex_unlock(&cec->tx_lock);
	if (ret)
		return ret;
	else {
		dev_dbg(cec->dev, "%s: %*phC", __func__, (int)count, tx_buf);
		return count;
	}
}

static ssize_t tegra_cec_read(struct file *file, char  __user *buffer,
	size_t count, loff_t *ppos)
{
	struct tegra_cec *cec = file->private_data;
	ssize_t ret;

	count = sizeof(cec->rx_buffer);

	ret = wait_event_interruptible(cec->init_waitq,
	    atomic_read(&cec->init_done) == 1);
	if (ret)
		return ret;

	if (cec->rx_wake == 0)
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;

	ret = wait_event_interruptible(cec->rx_waitq, cec->rx_wake == 1);
	if (ret)
		return ret;

	if (copy_to_user(buffer, &(cec->rx_buffer), count))
		return -EFAULT;

	dev_dbg(cec->dev, "%s: %*phC", __func__, (int)count,
		&(cec->rx_buffer));
	cec->rx_buffer = 0x0;
	cec->rx_wake = 0;
	return count;
}

static irqreturn_t tegra_cec_irq_handler(int irq, void *data)
{
	struct device *dev = data;
	struct tegra_cec *cec = dev_get_drvdata(dev);
	u32 status, mask;

	status = tegra_cec_readl(cec->cec_base + TEGRA_CEC_INT_STAT);
	mask = tegra_cec_readl(cec->cec_base + TEGRA_CEC_INT_MASK);

	status &= mask;

	if (!status)
		goto out;

	if (status & TEGRA_CEC_INT_STAT_TX_REGISTER_UNDERRUN) {
		dev_err(dev, "TX underrun, interrupt timing issue!\n");

		tegra_cec_error_recovery(cec);
		tegra_cec_writel(mask & ~TEGRA_CEC_INT_MASK_TX_REGISTER_EMPTY,
			cec->cec_base + TEGRA_CEC_INT_MASK);

		cec->tx_error = -EIO;
		cec->tx_wake = 1;

		wake_up_interruptible(&cec->tx_waitq);

		goto out;
	} else if ((status & TEGRA_CEC_INT_STAT_TX_ARBITRATION_FAILED) ||
		   (status & TEGRA_CEC_INT_STAT_TX_BUS_ANOMALY_DETECTED)) {
		tegra_cec_error_recovery(cec);
		tegra_cec_writel(mask & ~TEGRA_CEC_INT_MASK_TX_REGISTER_EMPTY,
			cec->cec_base + TEGRA_CEC_INT_MASK);

		cec->tx_error = -ECOMM;
		cec->tx_wake = 1;

		wake_up_interruptible(&cec->tx_waitq);

		goto out;
	} else if (status & TEGRA_CEC_INT_STAT_TX_FRAME_TRANSMITTED) {
		tegra_cec_writel((TEGRA_CEC_INT_STAT_TX_FRAME_TRANSMITTED),
			cec->cec_base + TEGRA_CEC_INT_STAT);

		if (status & TEGRA_CEC_INT_STAT_TX_FRAME_OR_BLOCK_NAKD) {
			tegra_cec_error_recovery(cec);

			cec->tx_error = TEGRA_CEC_LADDR_MODE(cec->tx_buf[0]) ?
					-ECONNRESET : -EHOSTUNREACH;
		}
		cec->tx_wake = 1;

		wake_up_interruptible(&cec->tx_waitq);

		goto out;
	} else if (status & TEGRA_CEC_INT_STAT_TX_FRAME_OR_BLOCK_NAKD)
		dev_warn(dev, "TX NAKed on the fly!\n");

	if (status & TEGRA_CEC_INT_STAT_TX_REGISTER_EMPTY) {
		if (cec->tx_buf_cur == cec->tx_buf_cnt)
			tegra_cec_writel(mask & ~TEGRA_CEC_INT_MASK_TX_REGISTER_EMPTY,
				cec->cec_base + TEGRA_CEC_INT_MASK);
		else
			tegra_cec_native_tx(cec,
				cec->tx_buf[cec->tx_buf_cur++]);
	}

	if (status & (TEGRA_CEC_INT_STAT_RX_REGISTER_OVERRUN |
		TEGRA_CEC_INT_STAT_RX_BUS_ANOMALY_DETECTED |
		TEGRA_CEC_INT_STAT_RX_START_BIT_DETECTED |
		TEGRA_CEC_INT_STAT_RX_BUS_ERROR_DETECTED)) {
		tegra_cec_writel((TEGRA_CEC_INT_STAT_RX_REGISTER_OVERRUN |
			TEGRA_CEC_INT_STAT_RX_BUS_ANOMALY_DETECTED |
			TEGRA_CEC_INT_STAT_RX_START_BIT_DETECTED |
			TEGRA_CEC_INT_STAT_RX_BUS_ERROR_DETECTED),
			cec->cec_base + TEGRA_CEC_INT_STAT);
	} else if (status & TEGRA_CEC_INT_STAT_RX_REGISTER_FULL) {
		tegra_cec_writel(TEGRA_CEC_INT_STAT_RX_REGISTER_FULL,
			cec->cec_base + TEGRA_CEC_INT_STAT);
		cec->rx_buffer = readw(cec->cec_base + TEGRA_CEC_RX_REGISTER);
		cec->rx_wake = 1;
		wake_up_interruptible(&cec->rx_waitq);
	}

out:
	return IRQ_HANDLED;
}

static int tegra_cec_dump_registers(struct tegra_cec *cec)
{
	int value, i;

	dev_info(cec->dev, "base address = %llx\n", (u64)cec->cec_base);
	for (i = 0; i <= cec->soc->offset; i += 4) {
		value = tegra_cec_readl(cec->cec_base + i);
		dev_info(cec->dev, "offset %08x: %08x\n", i, value);
	}
	return i;

}

static int tegra_cec_unpowergate(struct tegra_cec *cec)
{
	int ret = 0;

	if (cec->soc->cec_always_on)
		return 0;

	if (!tegra_dc_is_nvdisplay())
		return 0;

#if defined(CONFIG_TEGRA_POWERGATE)
	ret = tegra_unpowergate_partition(cec->soc->powergate_id);
#else
	ret = pm_runtime_get(cec->dev);
#endif
	if (IS_ERR(ERR_PTR(ret)))
		dev_err(cec->dev, "Failed to unpowergate DISP,err = %d\n", ret);

	return ret;
}

static void tegra_cec_powergate(struct tegra_cec *cec)
{
	if (cec->soc->cec_always_on)
		return;

	if (!tegra_dc_is_nvdisplay())
		return;

#if defined(CONFIG_TEGRA_POWERGATE)
	tegra_powergate_partition(cec->soc->powergate_id);
#else
	pm_runtime_put(cec->dev);
#endif
}

static int tegra_cec_set_rx_snoop(struct tegra_cec *cec, u32 enable)
{
	u32 state;

	if (!atomic_read(&cec->init_done))
		return -EAGAIN;
	state = tegra_cec_readl(cec->cec_base + TEGRA_CEC_HW_CONTROL);
	if (((state & TEGRA_CEC_HWCTRL_RX_SNOOP) != 0) ^ (enable != 0)) {
		state ^= TEGRA_CEC_HWCTRL_RX_SNOOP;
		tegra_cec_writel(state, cec->cec_base + TEGRA_CEC_HW_CONTROL);
	}
	return 0;
}

static int tegra_cec_get_rx_snoop(struct tegra_cec *cec, u32 *state)
{
	if (!atomic_read(&cec->init_done))
		return -EAGAIN;
	*state = (readl(cec->cec_base + TEGRA_CEC_HW_CONTROL) & TEGRA_CEC_HWCTRL_RX_SNOOP) >> 15;
	return 0;
}

static int tegra_cec_access_ok(bool write, unsigned long arg, size_t size)
{
	int err = 0;

#if KERNEL_VERSION(5, 4, 0) > LINUX_VERSION_CODE
	u8 __maybe_unused access_type = write ? VERIFY_WRITE : VERIFY_READ;
	err = !access_ok(access_type, arg, size);
#else
	err = !access_ok((void *)arg, size);
#endif

	return err;
}

static long tegra_cec_ioctl(struct file *file, unsigned int cmd,
		 unsigned long arg)
{
	int err;
	u32 state;

	struct tegra_cec *cec = file->private_data;

	if (_IOC_TYPE(cmd) != TEGRA_CEC_IOC_MAGIC)
		return  -EINVAL;

	switch (cmd) {
	case TEGRA_CEC_IOCTL_ERROR_RECOVERY:
		mutex_lock(&cec->recovery_lock);
		tegra_cec_error_recovery(cec);
		mutex_unlock(&cec->recovery_lock);
		break;
	case TEGRA_CEC_IOCTL_DUMP_REGISTERS:
		tegra_cec_dump_registers(cec);
		break;
	case TEGRA_CEC_IOCTL_SET_RX_SNOOP:
		err = tegra_cec_access_ok(false, arg, sizeof(u32));
		if (err)
			return -EFAULT;
		if (copy_from_user((u32 *) &state, (u32 *) arg, sizeof(u32)))
			return -EFAULT;
		tegra_cec_set_rx_snoop(cec, state);
		break;
	case TEGRA_CEC_IOCTL_GET_RX_SNOOP:
		err = tegra_cec_access_ok(true, arg, sizeof(u32));
		if (err)
			return -EFAULT;
		err = tegra_cec_get_rx_snoop(cec, &state);
		if (!err) {
			if (copy_to_user((u32 *) arg, &state, sizeof(u32)))
				return -EFAULT;
		}
		break;
	case TEGRA_CEC_IOCTL_GET_POST_RECOVERY:
		err = tegra_cec_access_ok(true, arg, sizeof(u32));
		if (err)
			return -EFAULT;
		if (copy_to_user((bool *) arg, &post_recovery, sizeof(bool)))
				return -EFAULT;
		break;
	default:
		dev_err(cec->dev, "unsupported ioctl\n");
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations tegra_cec_fops = {
	.owner = THIS_MODULE,
	.open = tegra_cec_open,
	.release = tegra_cec_release,
	.read = tegra_cec_read,
	.write = tegra_cec_write,
	.unlocked_ioctl = tegra_cec_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl =  tegra_cec_ioctl,
#endif
};

static int tegra_cec_send_one_touch_play(struct tegra_cec *cec)
{
	int res = 0;
	u8 phy_address[2] = {0};
	u32 state = 0;

	text_view_on_sent = true;

	if (cec->soc->use_tegra_dc) {
		res = tegra_dc_get_source_physical_address(phy_address);
		if (res) {
			dev_notice(cec->dev, "Can't find physical address.\n");
			return res;
		}
	} else {
		/*
		 * When tegradc is absent, UEFI suppose to write physical address
		 * at register TEGRA_CEC_HW_SPARE.
		 */
		state = tegra_cec_readl(cec->cec_base + TEGRA_CEC_HW_SPARE);
		phy_address[0] = state & 0x000F;
		phy_address[1] = state & 0x00F0;
	}

	dev_info(cec->dev, "physical address: %02x:%02x.\n",
		phy_address[0], phy_address[1]);

	if ((phy_address[0] == 0) && (phy_address[1] == 0)) {
		dev_err(cec->dev, "Can't find physical address.\n");
		return 0;
	}

	active_source_command[2] = phy_address[0];
	active_source_command[3] = phy_address[1];

	mutex_lock(&cec->tx_lock);
	res = tegra_cec_native_write_l(cec, text_view_on_command,
		sizeof(text_view_on_command));
	dev_notice(cec->dev, "Sent <Text View On> res: %d.\n", res);
	if (!res) {
		res = tegra_cec_native_write_l(cec, active_source_command,
			sizeof(active_source_command));
		dev_notice(cec->dev,
			"Broadcast <Active Source> res: %d.\n", res);
	}
	mutex_unlock(&cec->tx_lock);

	return res;
}

static void tegra_cec_init(struct tegra_cec *cec)
{
	u32 state;
	cec->rx_wake = 0;
	cec->tx_wake = 1;
	cec->tx_buf_cnt = 0;
	cec->tx_buf_cur = 0;
	cec->tx_error = 0;

	tegra_cec_global = cec;
	dev_notice(cec->dev, "%s started\n", __func__);

	tegra_cec_writel(0x00, cec->cec_base + TEGRA_CEC_HW_CONTROL);
	tegra_cec_writel(0x00, cec->cec_base + TEGRA_CEC_INT_MASK);

#ifdef CONFIG_PM
	if (wait_event_interruptible_timeout(cec->suspend_waitq,
				atomic_xchg(&cec->init_cancel, 0) == 1,
				msecs_to_jiffies(1000)) > 0)
		return;
#else
	msleep(1000);
#endif

	tegra_cec_writel(0x00, cec->cec_base + TEGRA_CEC_SW_CONTROL);

	cec->logical_addr = TEGRA_CEC_HWCTRL_RX_LADDR_UNREG;

	/* CEC initialization settings till T194 */
	if (!cec->soc->cec_always_on) {

		tegra_cec_writel(0xffffffff, cec->cec_base + TEGRA_CEC_INT_STAT);

		tegra_cec_writel(TEGRA_CEC_HWCTRL_RX_LADDR(cec->logical_addr) |
			TEGRA_CEC_HWCTRL_TX_NAK_MODE |
			TEGRA_CEC_HWCTRL_TX_RX_MODE,
			cec->cec_base + TEGRA_CEC_HW_CONTROL);

		tegra_cec_writel((1U << 31) | 0x20, cec->cec_base + TEGRA_CEC_INPUT_FILTER);

		tegra_cec_writel((0x7a << TEGRA_CEC_RX_TIMING_0_RX_START_BIT_MAX_LO_TIME_MASK) |
		   (0x6d << TEGRA_CEC_RX_TIMING_0_RX_START_BIT_MIN_LO_TIME_MASK) |
		   (0x93 << TEGRA_CEC_RX_TIMING_0_RX_START_BIT_MAX_DURATION_MASK) |
		   (0x86 << TEGRA_CEC_RX_TIMING_0_RX_START_BIT_MIN_DURATION_MASK),
		   cec->cec_base + TEGRA_CEC_RX_TIMING_0);

		tegra_cec_writel((0x35 << TEGRA_CEC_RX_TIMING_1_RX_DATA_BIT_MAX_LO_TIME_MASK) |
		   (0x21 << TEGRA_CEC_RX_TIMING_1_RX_DATA_BIT_SAMPLE_TIME_MASK) |
		   (0x56 << TEGRA_CEC_RX_TIMING_1_RX_DATA_BIT_MAX_DURATION_MASK) |
		   (0x40 << TEGRA_CEC_RX_TIMING_1_RX_DATA_BIT_MIN_DURATION_MASK),
		   cec->cec_base + TEGRA_CEC_RX_TIMING_1);

		tegra_cec_writel((0x50 << TEGRA_CEC_RX_TIMING_2_RX_END_OF_BLOCK_TIME_MASK),
		   cec->cec_base + TEGRA_CEC_RX_TIMING_2);

		tegra_cec_writel((0x74 << TEGRA_CEC_TX_TIMING_0_TX_START_BIT_LO_TIME_MASK) |
		   (0x8d << TEGRA_CEC_TX_TIMING_0_TX_START_BIT_DURATION_MASK) |
		   (0x08 << TEGRA_CEC_TX_TIMING_0_TX_BUS_XITION_TIME_MASK) |
		   (0x71 << TEGRA_CEC_TX_TIMING_0_TX_BUS_ERROR_LO_TIME_MASK),
		   cec->cec_base + TEGRA_CEC_TX_TIMING_0);

		tegra_cec_writel((0x2f << TEGRA_CEC_TX_TIMING_1_TX_LO_DATA_BIT_LO_TIME_MASK) |
		   (0x13 << TEGRA_CEC_TX_TIMING_1_TX_HI_DATA_BIT_LO_TIME_MASK) |
		   (0x4b << TEGRA_CEC_TX_TIMING_1_TX_DATA_BIT_DURATION_MASK) |
		   (0x21 << TEGRA_CEC_TX_TIMING_1_TX_ACK_NAK_BIT_SAMPLE_TIME_MASK),
		   cec->cec_base + TEGRA_CEC_TX_TIMING_1);

		tegra_cec_writel(
		   (0x07 << TEGRA_CEC_TX_TIMING_2_BUS_IDLE_TIME_ADDITIONAL_FRAME_MASK) |
		   (0x05 << TEGRA_CEC_TX_TIMING_2_BUS_IDLE_TIME_NEW_FRAME_MASK) |
		   (0x03 << TEGRA_CEC_TX_TIMING_2_BUS_IDLE_TIME_RETRY_FRAME_MASK),
		   cec->cec_base + TEGRA_CEC_TX_TIMING_2);

	} else {

		tegra_cec_writel(TEGRA_CEC_HWCTRL_RX_LADDR(cec->logical_addr),
			cec->cec_base + TEGRA_CEC_HW_CONTROL);

		tegra_cec_writel(0x1, cec->cec_base + TEGRA_CEC_MESSAGE_FILTER_CTRL);

		state = (0xff << TEGRA_CEC_RX_TIMING_1_RX_DATA_BIT_MAX_LO_TIME_MASK) |
			(0x22 << TEGRA_CEC_RX_TIMING_1_RX_DATA_BIT_SAMPLE_TIME_MASK) |
			(0xe0 << TEGRA_CEC_RX_TIMING_1_RX_DATA_BIT_MAX_DURATION_MASK) |
			(0x41 << TEGRA_CEC_RX_TIMING_1_RX_DATA_BIT_MIN_DURATION_MASK);
		tegra_cec_writel(state, cec->cec_base + TEGRA_CEC_RX_TIMING_1);

		state = (0x7 << TEGRA_CEC_TX_TIMING_2_BUS_IDLE_TIME_ADDITIONAL_FRAME_MASK) |
			(0x1 << TEGRA_CEC_TX_TIMING_2_BUS_IDLE_TIME_NEW_FRAME_MASK) |
			(0x3 << TEGRA_CEC_TX_TIMING_2_BUS_IDLE_TIME_RETRY_FRAME_MASK);
		tegra_cec_writel(state, cec->cec_base + TEGRA_CEC_TX_TIMING_2);

		/*
		 * By default, keep RX buffer depth to 2 bytes like previous chips.
		 * Value 1 = 2 bytes (1 fifo depth)
		 * Value 0x40 = 128 bytes (64 fifo depth)
		 */
		tegra_cec_writel(0x1, cec->cec_base + TEGRA_CEC_RX_BUFFER_AFULL_CFG_0);

		state = tegra_cec_readl(cec->cec_base + TEGRA_CEC_HW_CONTROL);
		state |= TEGRA_CEC_HWCTRL_TX_RX_MODE;
		tegra_cec_writel(state, cec->cec_base + TEGRA_CEC_HW_CONTROL);

	}

	tegra_cec_writel(TEGRA_CEC_INT_MASK_TX_REGISTER_UNDERRUN |
		TEGRA_CEC_INT_MASK_TX_FRAME_OR_BLOCK_NAKD |
		TEGRA_CEC_INT_MASK_TX_ARBITRATION_FAILED |
		TEGRA_CEC_INT_MASK_TX_BUS_ANOMALY_DETECTED |
		TEGRA_CEC_INT_MASK_TX_FRAME_TRANSMITTED |
		TEGRA_CEC_INT_MASK_RX_REGISTER_FULL |
		TEGRA_CEC_INT_MASK_RX_REGISTER_OVERRUN,
		cec->cec_base + TEGRA_CEC_INT_MASK);

	atomic_set(&cec->init_done, 1);
	wake_up_interruptible(&cec->init_waitq);

	if (!text_view_on_sent && !post_recovery)
		tegra_cec_send_one_touch_play(cec);
	dev_notice(cec->dev, "%s Done.\n", __func__);
}

static void tegra_cec_init_worker(struct work_struct *work)
{
	struct tegra_cec *cec = container_of(work, struct tegra_cec, work);

	tegra_cec_init(cec);
}

static ssize_t cec_logical_addr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct tegra_cec *cec = dev_get_drvdata(dev);

	if (!atomic_read(&cec->init_done))
		return -EAGAIN;

	if (buf)
		return sprintf(buf, "0x%x\n", (u32)cec->logical_addr);
	return 1;
}

static ssize_t cec_logical_addr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret;
	u32 state;
	u16 addr;
	struct tegra_cec *cec;

	if (!buf || !count)
		return -EINVAL;

	cec = dev_get_drvdata(dev);
	if (!atomic_read(&cec->init_done))
		return -EAGAIN;

	ret = kstrtou16(buf, 0, &addr);
	if (ret)
		return ret;

	dev_info(dev, "set logical address: 0x%x\n", (u32)addr);
	cec->logical_addr = addr;

	if (cec->soc->cec_always_on) {
		// clear TX_RX_MODE
		state = tegra_cec_readl(cec->cec_base + TEGRA_CEC_HW_CONTROL);
		state &= ~TEGRA_CEC_HWCTRL_TX_RX_MODE;
		tegra_cec_writel(state, cec->cec_base + TEGRA_CEC_HW_CONTROL);
		// write logical address
		state = tegra_cec_readl(cec->cec_base + TEGRA_CEC_HW_CONTROL);
		state &= ~TEGRA_CEC_HWCTRL_RX_LADDR_MASK;
		state |= TEGRA_CEC_HWCTRL_RX_LADDR(cec->logical_addr);
		tegra_cec_writel(state, cec->cec_base + TEGRA_CEC_HW_CONTROL);
		// enable tx_rx mode
		state = tegra_cec_readl(cec->cec_base + TEGRA_CEC_HW_CONTROL);
		state |= TEGRA_CEC_HWCTRL_TX_RX_MODE;
		tegra_cec_writel(state, cec->cec_base + TEGRA_CEC_HW_CONTROL);
	} else {
		state = tegra_cec_readl(cec->cec_base + TEGRA_CEC_HW_CONTROL);
		state &= ~TEGRA_CEC_HWCTRL_RX_LADDR_MASK;
		state |= TEGRA_CEC_HWCTRL_RX_LADDR(cec->logical_addr);
		tegra_cec_writel(state, cec->cec_base + TEGRA_CEC_HW_CONTROL);
	}

	return count;
}

static int tegra_cec_probe(struct platform_device *pdev)
{
	struct tegra_cec *cec;
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;
	int ret = 0;
	struct reset_control *rst = NULL;

	cec = devm_kzalloc(&pdev->dev, sizeof(struct tegra_cec), GFP_KERNEL);

	if (!cec)
		return -ENOMEM;

	cec->soc = of_device_get_match_data(&pdev->dev);
	if (cec->soc->cec_always_on && reset_retry_count != 0) {
		rst = devm_reset_control_get(&pdev->dev, "cec");
		if (IS_ERR(rst)) {
			/* BPMP reset mechanism not available, return and retry again */
			dev_info(&pdev->dev, "reset control is not found, deferring probe to retry again.\n");
			devm_kfree(&pdev->dev, cec);
			reset_retry_count--;
			return -EPROBE_DEFER;
		}

		/* take CEC engine out of reset */
		if (reset_control_reset(rst) != 0) {
			dev_info(&pdev->dev, "reset control reset failed, deferring probe to retry again.\n");
			devm_kfree(&pdev->dev, cec);
			reset_retry_count--;
			return -EPROBE_DEFER;
		}
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!res) {
		dev_err(&pdev->dev,
		    "Unable to allocate resources for device.\n");
		ret = -EBUSY;
		goto cec_error;
	}

	if (!devm_request_mem_region(&pdev->dev, res->start, resource_size(res),
		pdev->name)) {
		dev_err(&pdev->dev,
			"Unable to request mem region for device.\n");
		ret = -EBUSY;
		goto cec_error;
	}

	cec->tegra_cec_irq = platform_get_irq(pdev, 0);

	if (cec->tegra_cec_irq <= 0) {
		ret = -EBUSY;
		goto cec_error;
	}

	cec->cec_base = devm_ioremap(&pdev->dev, res->start,
		resource_size(res));

	if (!cec->cec_base) {
		dev_err(&pdev->dev, "Unable to grab IOs for device.\n");
		ret = -EBUSY;
		goto cec_error;
	}

	dev_info(&pdev->dev, "dt=%d start=0x%08llX end=0x%08llX irq=%d\n",
		(pdev->dev.of_node != NULL),
		res->start, res->end,
		cec->tegra_cec_irq);

	atomic_set(&cec->init_done, 0);
	mutex_init(&cec->tx_lock);
	mutex_init(&cec->recovery_lock);
	cec->dev = &pdev->dev;

	if (!cec->soc->cec_always_on) {

#if !defined(CONFIG_TEGRA_POWERGATE)
		if (tegra_dc_is_nvdisplay())
			pm_runtime_enable(&pdev->dev);
#endif

		ret = tegra_cec_unpowergate(cec);
		if (IS_ERR(ERR_PTR(ret)))
			goto clk_error;
		dev_info(&pdev->dev, "Unpowergated DISP\n");

		if (tegra_dc_is_nvdisplay()) {
			if (np)
				cec->clk = of_clk_get_by_name(np, "cec");
		} else {
			cec->clk = clk_get(&pdev->dev, "cec");
		}

		if (IS_ERR_OR_NULL(cec->clk)) {
			dev_err(&pdev->dev, "can't get clock for CEC\n");
			ret = -ENOENT;
			goto clk_error;
		}

		ret = clk_prepare_enable(cec->clk);
		dev_info(&pdev->dev, "Enable clock result: %d.\n", ret);

	}

	/* set context info. */
	init_waitqueue_head(&cec->rx_waitq);
	init_waitqueue_head(&cec->tx_waitq);
	init_waitqueue_head(&cec->init_waitq);

#ifdef CONFIG_PM
	init_waitqueue_head(&cec->suspend_waitq);
	atomic_set(&cec->init_cancel, 0);
#endif

	platform_set_drvdata(pdev, cec);
	/* clear out the hardware. */

	INIT_WORK(&cec->work, tegra_cec_init_worker);
	schedule_work(&cec->work);

	device_init_wakeup(&pdev->dev, 1);

	cec->misc_dev.minor = MISC_DYNAMIC_MINOR;
	cec->misc_dev.name = TEGRA_CEC_NAME;
	cec->misc_dev.fops = &tegra_cec_fops;
	cec->misc_dev.parent = &pdev->dev;

	if (misc_register(&cec->misc_dev)) {
		printk(KERN_WARNING "Couldn't register device , %s.\n", TEGRA_CEC_NAME);
		goto cec_error;
	}

	ret = devm_request_irq(&pdev->dev, cec->tegra_cec_irq,
		tegra_cec_irq_handler, 0x0, "cec_irq", &pdev->dev);

	if (ret) {
		dev_err(&pdev->dev,
			"Unable to request interrupt for device (err=%d).\n", ret);
		goto cec_error;
	}

	/*
	 * Create a symlink for tegra_cec if it is not under platform bus or
	 * it has been created with different name.
	 */
	if ((pdev->dev.parent != &platform_bus) ||
			strcmp(dev_name(&pdev->dev), TEGRA_CEC_NAME)) {
		ret = sysfs_create_link(&platform_bus.kobj,
				&pdev->dev.kobj, TEGRA_CEC_NAME);
		if (ret)
			dev_warn(&pdev->dev, "Could not create sysfs link.\n");
	}

	ret = sysfs_create_file(
		&pdev->dev.kobj, &dev_attr_cec_logical_addr_config.attr);
	dev_info(&pdev->dev, "cec_add_sysfs ret=%d\n", ret);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to add sysfs: %d\n", ret);
		goto cec_error;
	}

	dev_notice(&pdev->dev, "probed\n");

	return 0;

cec_error:
	cancel_work_sync(&cec->work);
	if (!cec->soc->cec_always_on) {
		clk_disable(cec->clk);
		clk_put(cec->clk);
		tegra_cec_powergate(cec);
	}
clk_error:
	return ret;
}

static int tegra_cec_remove(struct platform_device *pdev)
{
	struct tegra_cec *cec = platform_get_drvdata(pdev);

	if (!cec->soc->cec_always_on) {

		clk_disable(cec->clk);
		clk_put(cec->clk);
		tegra_cec_powergate(cec);
#if !defined(CONFIG_TEGRA_POWERGATE)
		if (tegra_dc_is_nvdisplay())
			pm_runtime_disable(&pdev->dev);
#endif
	}

	misc_deregister(&cec->misc_dev);
	cancel_work_sync(&cec->work);

	return 0;
}

#ifdef CONFIG_PM
static int tegra_cec_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct tegra_cec *cec = platform_get_drvdata(pdev);

	atomic_set(&cec->init_cancel, 1);
	wmb();

	wake_up_interruptible(&cec->suspend_waitq);

	/* cancel the work queue */
	cancel_work_sync(&cec->work);

	atomic_set(&cec->init_done, 0);
	atomic_set(&cec->init_cancel, 0);

	if (!cec->soc->cec_always_on) {
		clk_disable(cec->clk);
		tegra_cec_powergate(cec);
	} else {
		/*
		 * TODO:
		 * 1. Program TEGRA_CEC_RX_BUFFER_AFULL_CFG_0 for 0x40
		 * 2. Program TEGRA_CEC_MESSAGE_FILTER_CTRL,
		 *    TEGRA_CEC_RX_PHYSICAL_ADDR_0,
		 *    TEGRA_CEC_RX_OPCODE_0/1/2/3/4.
		 */
	}

	dev_notice(&pdev->dev, "suspended\n");
	return 0;
}

static int tegra_cec_resume(struct platform_device *pdev)
{
	struct tegra_cec *cec = platform_get_drvdata(pdev);

	dev_notice(&pdev->dev, "Resuming\n");

	if (!cec->soc->cec_always_on) {
		tegra_cec_unpowergate(cec);
		clk_enable(cec->clk);
	} else {
		/* TODO:
		 * 1. Read TEGRA_CEC_RX_BUFFER_STAT_0 value and read RX buffer data
		 * 2. Configure TEGRA_CEC_RX_BUFFER_AFULL_CFG_0 back to 0x1.
		 */
	}

	schedule_work(&cec->work);

	return 0;
}
#endif

static int __init check_post_recovery(char *options)
{
	post_recovery = true;

	pr_info("tegra_cec: the post_recovery is %d .\n", post_recovery);

	return 0;
}

early_param("post_recovery", check_post_recovery);

static struct tegra_cec_soc tegra210_soc_data = {
#if defined(CONFIG_TEGRA_POWERGATE)
	.powergate_id = TEGRA210_POWER_DOMAIN_DISA,
#endif
	.offset = TEGRA_CEC_HW_SPARE,
	.use_tegra_dc = true,
	.cec_always_on = false,
};

static struct tegra_cec_soc tegra186_soc_data = {
#if defined(CONFIG_TEGRA_POWERGATE)
	.powergate_id = TEGRA186_POWER_DOMAIN_DISP,
#endif
	.offset = TEGRA_CEC_HW_SPARE,
	.use_tegra_dc = true,
	.cec_always_on = false,
};

static struct tegra_cec_soc tegra194_soc_data = {
#if defined(CONFIG_TEGRA_POWERGATE)
	.powergate_id = TEGRA194_POWER_DOMAIN_DISP,
#endif
	.offset = TEGRA_CEC_HW_SPARE,
	.use_tegra_dc = true,
	.cec_always_on = false,
};

static struct tegra_cec_soc tegra234_soc_data = {
	.offset = TEGRA_CEC_RX_OPCODE_4,
	.use_tegra_dc = false,
	.cec_always_on = true,
};

static struct of_device_id tegra_cec_of_match[] = {
	{ .compatible = "nvidia,tegra210-cec", .data = &tegra210_soc_data },
	{ .compatible = "nvidia,tegra186-cec", .data = &tegra186_soc_data },
	{ .compatible = "nvidia,tegra194-cec", .data = &tegra194_soc_data },
	{ .compatible = "nvidia,tegra234-cec", .data = &tegra234_soc_data },
	{},
};

static struct platform_driver tegra_cec_driver = {
	.driver = {
		.name = TEGRA_CEC_NAME,
		.owner = THIS_MODULE,

		.of_match_table = of_match_ptr(tegra_cec_of_match),
	},
	.probe = tegra_cec_probe,
	.remove = tegra_cec_remove,

#ifdef CONFIG_PM
	.suspend = tegra_cec_suspend,
	.resume = tegra_cec_resume,
#endif
};

module_platform_driver(tegra_cec_driver);
