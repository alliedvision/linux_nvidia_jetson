/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/gpio.h>
#include <linux/list.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <asm/arch_timer.h>
#include <linux/platform/tegra/ptp-notifier.h>
#include <linux/time.h>
#include <linux/version.h>
#include <uapi/linux/nvpps_ioctl.h>
#include <linux/tegra-gte.h>



/* the following contrl flags are for
 * debugging pirpose only
 */
/* #define NVPPS_ARM_COUNTER_PROFILING */
/* #define NVPPS_EQOS_REG_PROFILING */


#define MAX_NVPPS_SOURCES	1
#define NVPPS_DEF_MODE		NVPPS_MODE_GPIO

/* statics */
static struct class	*s_nvpps_class;
static dev_t		s_nvpps_devt;
static DEFINE_MUTEX(s_nvpps_lock);
static DEFINE_IDR(s_nvpps_idr);



/* platform device instance data */
struct nvpps_device_data {
	struct platform_device	*pdev;
	struct cdev		cdev;
	struct device		*dev;
	unsigned int		id;
	unsigned int		gpio_pin;
	int			irq;
	bool			irq_registered;
	bool			use_gpio_int_timesatmp;

	bool			pps_event_id_valid;
	unsigned int		pps_event_id;
	u32			actual_evt_mode;
	u64			tsc;
	u64			phc;
	u64			irq_latency;
	u64			tsc_res_ns;
	raw_spinlock_t		lock;
	struct mutex		ts_lock;

	u32			evt_mode;
	u32			tsc_mode;

	struct timer_list	timer;

	volatile bool		timer_inited;

	wait_queue_head_t	pps_event_queue;
	struct fasync_struct	*pps_event_async_queue;
	struct tegra_gte_ev_desc *gte_ev_desc;

	bool		memmap_phc_regs;
	u64			mac_base_addr;
	u32			sts_offset;
	u32			stns_offset;
};


/* file instance data */
struct nvpps_file_data {
	struct nvpps_device_data	*pdev_data;
	unsigned int			pps_event_id_rd;
};


/* MAC Base addrs */
#define T194_EQOS_BASE_ADDR		0x2490000
#define T234_EQOS_BASE_ADDR		0x2310000
#define EQOS_STSR_OFFSET		0xb08
#define EQOS_STNSR_OFFSET		0xb0c
#define T234_MGBE0_BASE_ADDR	0x6810000
#define T234_MGBE1_BASE_ADDR	0x6910000
#define T234_MGBE2_BASE_ADDR	0x6a10000
#define T234_MGBE3_BASE_ADDR	0x6b10000
#define MGBE_STSR_OFFSET		0xd08
#define MGBE_STNSR_OFFSET		0xd0c

#define BASE_ADDRESS pdev_data->mac_base_addr
#define MAC_STNSR_TSSS_LPOS 0
#define MAC_STNSR_TSSS_HPOS 30

#define GET_VALUE(data, lbit, hbit) ((data >> lbit) & (~(~0<<(hbit-lbit+1))))
#define MAC_STNSR_OFFSET ((volatile u32 *)(BASE_ADDRESS + pdev_data->stns_offset))
#define MAC_STNSR_RD(data) do {\
	(data) = ioread32((void *)MAC_STNSR_OFFSET);\
} while (0)
#define MAC_STSR_OFFSET ((volatile u32 *)(BASE_ADDRESS + pdev_data->sts_offset))
#define MAC_STSR_RD(data) do {\
	(data) = ioread32((void *)MAC_STSR_OFFSET);\
} while (0)


#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 2, 0)
static inline u64 __arch_counter_get_cntvct(void)
{
	u64 cval;

	asm volatile("mrs %0, cntvct_el0" : "=r" (cval));

	return cval;
}
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(5, 2, 0) */


static inline u64 get_systime(struct nvpps_device_data *pdev_data, u64 *tsc)
{
	u64 ns1, ns2, ns;
	u32 varmac_stnsr1, varmac_stnsr2;
	u32 varmac_stsr;

	/* read the PHC */
	MAC_STNSR_RD(varmac_stnsr1);
	MAC_STSR_RD(varmac_stsr);
	/* read the TSC */
	*tsc = __arch_counter_get_cntvct();

	/* read the nsec part of the PHC one more time */
	MAC_STNSR_RD(varmac_stnsr2);

	ns1 = GET_VALUE(varmac_stnsr1, MAC_STNSR_TSSS_LPOS, MAC_STNSR_TSSS_HPOS);
	ns2 = GET_VALUE(varmac_stnsr2, MAC_STNSR_TSSS_LPOS, MAC_STNSR_TSSS_HPOS);

	/* if ns1 is greater than ns2, it means nsec counter rollover
	 * happened. In that case read the updated sec counter again
	 */
	if (ns1 > ns2) {
		/* let's read the TSC again */
		*tsc = __arch_counter_get_cntvct();
		/* read the second portion of the PHC */
		MAC_STSR_RD(varmac_stsr);
		/* convert sec/high time value to nanosecond */
		ns = ns2 + (varmac_stsr * 1000000000ull);
	} else {
		/* convert sec/high time value to nanosecond */
		ns = ns1 + (varmac_stsr * 1000000000ull);
	}

	return ns;
}



/*
 * Report the PPS event
 */
static void nvpps_get_ts(struct nvpps_device_data *pdev_data, bool in_isr)
{
	u64		tsc;
	u64		tsc1, tsc2;
	u64		irq_tsc = 0;
	u64		phc = 0;
	u64		irq_latency = 0;
	unsigned long	flags;

	if (in_isr) {
		/* initialize irq_tsc to the current TSC just in case the
		 * gpio_timestamp_read call failed so the irq_tsc can be
		 * closer to when the interrupt actually occured
		 */
		irq_tsc = __arch_counter_get_cntvct();
		if (pdev_data->use_gpio_int_timesatmp) {
			int	err;
			int	gte_event_found = 0;
			int	safety = 33; /* GTE driver fifo depth is 32, plus one for margin */
			struct tegra_gte_ev_detail hts;

			/* 1PPS TSC timestamp is isochronous in nature
			 * we only need the last event
			 */
			do {
				err = tegra_gte_retrieve_event(pdev_data->gte_ev_desc, &hts);
				if (!err) {
					irq_tsc = hts.ts_raw;
					gte_event_found = 1;
				}
				/* decrement the count so we don't risk looping
				 * here forever
				 */
				safety--;
			} while (!err && (safety >= 0));
			if (gte_event_found == 0) {
				dev_err(pdev_data->dev, "failed to read timestamp data err(%d)\n", err);
			}
			if (safety < 0) {
				dev_err(pdev_data->dev, "tegra_gte_retrieve_event succeed beyond its fifo size err(%d)!)\n", err);
			}
		}
	}

	/* get the PTP timestamp */
	if (pdev_data->memmap_phc_regs) {
		/* get both the phc(using memmap reg) and tsc */
		phc = get_systime(pdev_data, &tsc);
	} else {
		/* get the TSC time before the function call */
		tsc1 = __arch_counter_get_cntvct();
		/* get the phc(using ptp notifier) from eqos driver */
		get_ptp_hwtime(&phc);
		/* get the TSC time after the function call */
		tsc2 = __arch_counter_get_cntvct();
		/* we do not know the latency of the get_ptp_hwtime() function
		 * so we are measuring the before and after and use the two
		 * samples average to approximate the time when the PTP clock
		 * is sampled
		 */
		tsc = (tsc1 + tsc2) >> 1;
	}

#ifdef NVPPS_ARM_COUNTER_PROFILING
	{
	u64	tmp;
	int	i;
	irq_tsc = __arch_counter_get_cntvct();
	for (i = 0; i < 98; i++) {
		tmp = __arch_counter_get_cntvct();
	}
		tsc = __arch_counter_get_cntvct();
	}
#endif /* NVPPS_ARM_COUNTER_PROFILING */

#ifdef NVPPS_EQOS_REG_PROFILING
	{
	u32	varmac_stnsr;
	u32	varmac_stsr;
	int	i;
	irq_tsc = __arch_counter_get_cntvct();
	for (i = 0; i < 100; i++) {
		MAC_STNSR_RD(varmac_stnsr);
		MAC_STSR_RD(varmac_stsr)
	}
	tsc = __arch_counter_get_cntvct();
	}
#endif /* NVPPS_EQOS_REG_PROFILING */

	/* get the interrupt latency */
	if (irq_tsc) {
		irq_latency = (tsc - irq_tsc) * pdev_data->tsc_res_ns;
	}

	raw_spin_lock_irqsave(&pdev_data->lock, flags);
	pdev_data->pps_event_id_valid = true;
	pdev_data->pps_event_id++;
	pdev_data->tsc = irq_tsc ? irq_tsc : tsc;
	/* adjust the ptp time for the interrupt latency */
#if defined (NVPPS_ARM_COUNTER_PROFILING) || defined (NVPPS_EQOS_REG_PROFILING)
	pdev_data->phc = phc;
#else /* !NVPPS_ARM_COUNTER_PROFILING && !NVPPS_EQOS_REG_PROFILING */
	pdev_data->phc = phc ? phc - irq_latency : phc;
#endif /* NVPPS_ARM_COUNTER_PROFILING || NVPPS_EQOS_REG_PROFILING */
	pdev_data->irq_latency = irq_latency;
	pdev_data->actual_evt_mode = in_isr ? NVPPS_MODE_GPIO : NVPPS_MODE_TIMER;
	raw_spin_unlock_irqrestore(&pdev_data->lock, flags);

	/* event notification */
	wake_up_interruptible(&pdev_data->pps_event_queue);
	kill_fasync(&pdev_data->pps_event_async_queue, SIGIO, POLL_IN);
}


static irqreturn_t nvpps_gpio_isr(int irq, void *data)
{
	struct nvpps_device_data	*pdev_data = (struct nvpps_device_data *)data;

	/* get timestamps for this event */
	nvpps_get_ts(pdev_data, true);

	return IRQ_HANDLED;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0)
static void nvpps_timer_callback(unsigned long data)
{
	struct nvpps_device_data        *pdev_data = (struct nvpps_device_data *)data;
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0) */
static void nvpps_timer_callback(struct timer_list *t)
{
        struct nvpps_device_data        *pdev_data = (struct nvpps_device_data *)from_timer(pdev_data, t, timer);
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0) */
	/* get timestamps for this event */
	nvpps_get_ts(pdev_data, false);

	/* set the next expire time */
	if (pdev_data->timer_inited) {
		mod_timer(&pdev_data->timer, jiffies + msecs_to_jiffies(1000));
	}
}



static int set_mode(struct nvpps_device_data *pdev_data, u32 mode)
{
	int	err = 0;
	if (mode != pdev_data->evt_mode) {
		switch (mode) {
			case NVPPS_MODE_GPIO:
				if (pdev_data->timer_inited) {
					pdev_data->timer_inited = false;
					del_timer_sync(&pdev_data->timer);
				}
				if (!pdev_data->irq_registered) {
					/* register IRQ handler */
					err = devm_request_irq(pdev_data->dev, pdev_data->irq, nvpps_gpio_isr,
							IRQF_TRIGGER_RISING | IRQF_NO_THREAD, "nvpps_isr", pdev_data);
					if (err) {
						dev_err(pdev_data->dev, "failed to acquire IRQ %d\n", pdev_data->irq);
					} else {
						pdev_data->irq_registered = true;
						dev_info(pdev_data->dev, "Registered IRQ %d for nvpps\n", pdev_data->irq);
					}
				}
				break;

			case NVPPS_MODE_TIMER:
				if (pdev_data->irq_registered) {
					/* unregister IRQ handler */
					devm_free_irq(pdev_data->dev, pdev_data->irq, pdev_data);
					pdev_data->irq_registered = false;
					dev_info(pdev_data->dev, "removed IRQ %d for nvpps\n", pdev_data->irq);
				}
				if (!pdev_data->timer_inited) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
					setup_timer(&pdev_data->timer,
							nvpps_timer_callback,
							(unsigned long)pdev_data);
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0) */
					timer_setup(&pdev_data->timer,
							nvpps_timer_callback,
							0);
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0) */
					pdev_data->timer_inited = true;
					/* setup timer interval to 1000 msecs */
					mod_timer(&pdev_data->timer, jiffies + msecs_to_jiffies(1000));
				}
				break;

			default:
				return -EINVAL;
		}
	}
	return err;
}



/* Character device stuff */
static unsigned int nvpps_poll(struct file *file, poll_table *wait)
{
	struct nvpps_file_data		*pfile_data = (struct nvpps_file_data *)file->private_data;
	struct nvpps_device_data	*pdev_data = pfile_data->pdev_data;

	poll_wait(file, &pdev_data->pps_event_queue, wait);
	if (pdev_data->pps_event_id_valid &&
		(pfile_data->pps_event_id_rd != pdev_data->pps_event_id)) {
		return POLLIN | POLLRDNORM;
	} else {
		return 0;
	}
}


static int nvpps_fasync(int fd, struct file *file, int on)
{
	struct nvpps_file_data		*pfile_data = (struct nvpps_file_data *)file->private_data;
	struct nvpps_device_data	*pdev_data = pfile_data->pdev_data;

	return fasync_helper(fd, file, on, &pdev_data->pps_event_async_queue);
}


static long nvpps_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct nvpps_file_data		*pfile_data = (struct nvpps_file_data *)file->private_data;
	struct nvpps_device_data	*pdev_data = pfile_data->pdev_data;
	struct nvpps_params		params;
	void __user			*uarg = (void __user *)arg;
	int				err;

	switch (cmd) {
		case NVPPS_GETVERSION: {
			struct nvpps_version	version;

			dev_dbg(pdev_data->dev, "NVPPS_GETVERSION\n");

			/* Get the current parameters */
			version.version.major = NVPPS_VERSION_MAJOR;
			version.version.minor = NVPPS_VERSION_MINOR;
			version.api.major = NVPPS_API_MAJOR;
			version.api.minor = NVPPS_API_MINOR;

			err = copy_to_user(uarg, &version, sizeof(struct nvpps_version));
			if (err) {
				return -EFAULT;
			}
			break;
		}

		case NVPPS_GETPARAMS:
			dev_dbg(pdev_data->dev, "NVPPS_GETPARAMS\n");

			/* Get the current parameters */
			params.evt_mode = pdev_data->evt_mode;
			params.tsc_mode = pdev_data->tsc_mode;

			err = copy_to_user(uarg, &params, sizeof(struct nvpps_params));
			if (err) {
				return -EFAULT;
			}
			break;

		case NVPPS_SETPARAMS:
			dev_dbg(pdev_data->dev, "NVPPS_SETPARAMS\n");

			err = copy_from_user(&params, uarg, sizeof(struct nvpps_params));
			if (err) {
				return -EFAULT;
			}
			err = set_mode(pdev_data, params.evt_mode);
			if (err) {
				dev_dbg(pdev_data->dev, "switch_mode to %d failed err(%d)\n", params.evt_mode, err);
				return err;
			}
			pdev_data->evt_mode = params.evt_mode;
			pdev_data->tsc_mode = params.tsc_mode;
			break;

		case NVPPS_GETEVENT: {
			struct nvpps_timeevent	time_event;
			unsigned long		flags;

			dev_dbg(pdev_data->dev, "NVPPS_GETEVENT\n");

			/* Return the fetched timestamp */
			raw_spin_lock_irqsave(&pdev_data->lock, flags);
			pfile_data->pps_event_id_rd = pdev_data->pps_event_id;
			time_event.evt_nb = pdev_data->pps_event_id;
			time_event.tsc = pdev_data->tsc;
			time_event.ptp = pdev_data->phc;
			time_event.irq_latency = pdev_data->irq_latency;
			raw_spin_unlock_irqrestore(&pdev_data->lock, flags);
			if (NVPPS_TSC_NSEC == pdev_data->tsc_mode) {
				time_event.tsc *= pdev_data->tsc_res_ns;
			}
			time_event.tsc_res_ns = pdev_data->tsc_res_ns;
			/* return the mode when the time event actually occured */
			time_event.evt_mode = pdev_data->actual_evt_mode;
			time_event.tsc_mode = pdev_data->tsc_mode;

			err = copy_to_user(uarg, &time_event, sizeof(struct nvpps_timeevent));
			if (err) {
				return -EFAULT;
			}

			break;
		}

		case NVPPS_GETTIMESTAMP: {
			struct nvpps_timestamp_struct	time_stamp;
			u64	ns;
			u32	reminder;
			u64	tsc1, tsc2;

			tsc1 = __arch_counter_get_cntvct();

			dev_dbg(pdev_data->dev, "NVPPS_GETTIMESTAMP\n");

			err = copy_from_user(&time_stamp, uarg,
				sizeof(struct nvpps_timestamp_struct));
			if (err)
				return -EFAULT;

			mutex_lock(&pdev_data->ts_lock);
			switch (time_stamp.clockid) {
			case CLOCK_REALTIME:
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
				ktime_get_real_ts(&time_stamp.kernel_ts);
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0) */
				ktime_get_real_ts64(&time_stamp.kernel_ts);
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0) */
				break;

			case CLOCK_MONOTONIC:
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
				ktime_get_ts(&time_stamp.kernel_ts);
#else /* LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0) */
				ktime_get_ts64(&time_stamp.kernel_ts);
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0) */
				break;

			default:
				dev_dbg(pdev_data->dev,
					"ioctl: Unsupported clockid\n");
			}

			err = get_ptp_hwtime(&ns);
			mutex_unlock(&pdev_data->ts_lock);
			if (err) {
				dev_dbg(pdev_data->dev,
					"pdev_data->dev, HW PTP not running\n");
				return err;
			}
			time_stamp.hw_ptp_ts.tv_sec = div_u64_rem(ns,
							1000000000ULL,
							&reminder);
			time_stamp.hw_ptp_ts.tv_nsec = reminder;

			tsc2 = __arch_counter_get_cntvct();
			time_stamp.extra[0] =
				(tsc2 - tsc1) * pdev_data->tsc_res_ns;

			err = copy_to_user(uarg, &time_stamp,
				sizeof(struct nvpps_timestamp_struct));
			if (err)
				return -EFAULT;
			break;
		}

		default:
			return -ENOTTY;
	}

	return 0;
}



static int nvpps_open(struct inode *inode, struct file *file)
{
	struct nvpps_device_data	*pdev_data = container_of(inode->i_cdev, struct nvpps_device_data, cdev);
	struct nvpps_file_data		*pfile_data;

	pfile_data = kzalloc(sizeof(struct nvpps_file_data), GFP_KERNEL);
	if (!pfile_data) {
		dev_err(&pdev_data->pdev->dev, "nvpps_open kzalloc() failed\n");
		return -ENOMEM;
	}

	pfile_data->pdev_data = pdev_data;
	pfile_data->pps_event_id_rd = (unsigned int)-1;

	file->private_data = pfile_data;
	kobject_get(&pdev_data->dev->kobj);
	return 0;
}



static int nvpps_close(struct inode *inode, struct file *file)
{
	struct nvpps_device_data	*pdev_data = container_of(inode->i_cdev, struct nvpps_device_data, cdev);

	if (file->private_data) {
		kfree(file->private_data);
	}
	kobject_put(&pdev_data->dev->kobj);
	return 0;
}



static const struct file_operations nvpps_fops = {
	.owner		= THIS_MODULE,
	.poll		= nvpps_poll,
	.fasync		= nvpps_fasync,
	.unlocked_ioctl	= nvpps_ioctl,
	.open		= nvpps_open,
	.release	= nvpps_close,
};



static void nvpps_dev_release(struct device *dev)
{
	struct nvpps_device_data	*pdev_data = dev_get_drvdata(dev);

	cdev_del(&pdev_data->cdev);

	mutex_lock(&s_nvpps_lock);
	idr_remove(&s_nvpps_idr, pdev_data->id);
	mutex_unlock(&s_nvpps_lock);

	kfree(dev);
	kfree(pdev_data);
}

static int nvpps_fill_mac_phc_info(struct platform_device *pdev,
								   struct nvpps_device_data *pdev_data)
{
	struct device_node *np = pdev->dev.of_node;
	char *eth_iface = NULL;
	bool use_eqos_mac = false;
	int err = 0;

	eth_iface = (char *)of_get_property(np, "interface", NULL);
	pdev_data->memmap_phc_regs = of_property_read_bool(np, "memmap_phc_regs");

	/* For orin, only use memmap method of accessing MAC PHC regs */
	if (of_machine_is_compatible("nvidia,tegra234")) {
		if (pdev_data->memmap_phc_regs) {
			dev_info(&pdev->dev, "using mem mapped MAC PHC reg method\n");
			if (eth_iface == NULL) {
				dev_warn(&pdev->dev, "interface property not provided. Using default interface(eqos)\n");
				use_eqos_mac = true;
			} else {
				if (!strncmp(eth_iface, "eqos", sizeof("eqos"))) {
					use_eqos_mac = true;
				} else if (!strncmp(eth_iface, "mgbe0", sizeof("mgbe0"))) {
					/* remap base address for mgbe0 */
					pdev_data->mac_base_addr = (u64)devm_ioremap(&pdev->dev, T234_MGBE0_BASE_ADDR, SZ_4K);
					dev_info(&pdev->dev, "map MGBE0 to (%p)\n", (void *)pdev_data->mac_base_addr);
					pdev_data->sts_offset = MGBE_STSR_OFFSET;
					pdev_data->stns_offset = MGBE_STNSR_OFFSET;
				} else if (!strncmp(eth_iface, "mgbe1", sizeof("mgbe1"))) {
					/* remap base address for mgbe1 */
					pdev_data->mac_base_addr = (u64)devm_ioremap(&pdev->dev, T234_MGBE1_BASE_ADDR, SZ_4K);
					dev_info(&pdev->dev, "map MGBE1 to (%p)\n", (void *)pdev_data->mac_base_addr);
					pdev_data->sts_offset = MGBE_STSR_OFFSET;
					pdev_data->stns_offset = MGBE_STNSR_OFFSET;
				} else if (!strncmp(eth_iface, "mgbe2", sizeof("mgbe2"))) {
					/* remap base address for mgbe2 */
					pdev_data->mac_base_addr = (u64)devm_ioremap(&pdev->dev, T234_MGBE2_BASE_ADDR, SZ_4K);
					dev_info(&pdev->dev, "map MGBE2 to (%p)\n", (void *)pdev_data->mac_base_addr);
					pdev_data->sts_offset = MGBE_STSR_OFFSET;
					pdev_data->stns_offset = MGBE_STNSR_OFFSET;
				} else if (!strncmp(eth_iface, "mgbe3", sizeof("mgbe3"))) {
					/* remap base address for mgbe3 */
					pdev_data->mac_base_addr = (u64)devm_ioremap(&pdev->dev, T234_MGBE3_BASE_ADDR, SZ_4K);
					dev_info(&pdev->dev, "map MGBE3 to (%p)\n", (void *)pdev_data->mac_base_addr);
					pdev_data->sts_offset = MGBE_STSR_OFFSET;
					pdev_data->stns_offset = MGBE_STNSR_OFFSET;
				} else {
					dev_warn(&pdev->dev, "Invalid interface(%s). Using default interface(eqos)\n", eth_iface);
					use_eqos_mac = true;
				}
			}

			if (use_eqos_mac) {
				/* remap base address for eqos */
				pdev_data->mac_base_addr = (u64)devm_ioremap(&pdev->dev, T234_EQOS_BASE_ADDR, SZ_4K);
				dev_info(&pdev->dev, "map EQOS to (%p)\n", (void *)pdev_data->mac_base_addr);
				pdev_data->sts_offset = EQOS_STSR_OFFSET;
				pdev_data->stns_offset = EQOS_STNSR_OFFSET;
			}
		} else {
			dev_info(&pdev->dev, "using ptp notifier method with default interface(eqos)\n");
		}
	} else {
		if (pdev_data->memmap_phc_regs) {
			if (eth_iface && strncmp(eth_iface, "eqos", sizeof("eqos")))
				dev_warn(&pdev->dev, "Invalid interface(%s). Using default interface(eqos)\n", eth_iface);

			dev_info(&pdev->dev, "using mem mapped MAC PHC reg method with eqos MAC\n");
			/* remap base address for eqos */
			pdev_data->mac_base_addr = (u64)devm_ioremap(&pdev->dev, T194_EQOS_BASE_ADDR, SZ_4K);
			dev_info(&pdev->dev, "map EQOS to (%p)\n", (void *)pdev_data->mac_base_addr);
			pdev_data->sts_offset = EQOS_STSR_OFFSET;
			pdev_data->stns_offset = EQOS_STNSR_OFFSET;
		} else {
			dev_info(&pdev->dev, "using ptp notifier method with default interface(eqos)\n");
		}
	}

	return err;
}


static int nvpps_probe(struct platform_device *pdev)
{
	struct nvpps_device_data	*pdev_data;
	struct device_node		*np = pdev->dev.of_node;
	dev_t				devt;
	int				err;
	struct device_node              *np_gte;

	dev_info(&pdev->dev, "%s\n", __FUNCTION__);

	if (!np) {
		dev_err(&pdev->dev, "no valid device node, probe failed\n");
		return -EINVAL;
	}

	pdev_data = kzalloc(sizeof(struct nvpps_device_data), GFP_KERNEL);
	if (!pdev_data) {
		return -ENOMEM;
	}

	err = of_get_gpio(np, 0);
	if (err < 0) {
		dev_err(&pdev->dev, "unable to get GPIO from device tree\n");
		return err;
	} else {
		pdev_data->gpio_pin = (unsigned int)err;
		dev_info(&pdev->dev, "gpio_pin(%d)\n", pdev_data->gpio_pin);
	}

	/* GPIO setup */
	if (gpio_is_valid(pdev_data->gpio_pin)) {
		err = devm_gpio_request(&pdev->dev, pdev_data->gpio_pin, "gpio_pps");
		if (err) {
			dev_err(&pdev->dev, "failed to request GPIO %u\n",
				pdev_data->gpio_pin);
			return err;
		}

		err = gpio_direction_input(pdev_data->gpio_pin);
		if (err) {
			dev_err(&pdev->dev, "failed to set pin direction\n");
			return -EINVAL;
		}

		/* IRQ setup */
		err = gpio_to_irq(pdev_data->gpio_pin);
		if (err < 0) {
			dev_err(&pdev->dev, "failed to map GPIO to IRQ: %d\n", err);
			return -EINVAL;
		}
		pdev_data->irq = err;
		dev_info(&pdev->dev, "gpio_to_irq(%d)\n", pdev_data->irq);
	}

	nvpps_fill_mac_phc_info(pdev, pdev_data);

	init_waitqueue_head(&pdev_data->pps_event_queue);
	raw_spin_lock_init(&pdev_data->lock);
	mutex_init(&pdev_data->ts_lock);
	pdev_data->pdev = pdev;
	pdev_data->evt_mode = 0; /* NVPPS_MODE_GPIO */
	pdev_data->tsc_mode = NVPPS_TSC_NSEC;
	#define _PICO_SECS (1000000000000ULL)
	pdev_data->tsc_res_ns = (_PICO_SECS / (u64)arch_timer_get_cntfrq()) / 1000;
	#undef _PICO_SECS
	dev_info(&pdev->dev, "tsc_res_ns(%llu)\n", pdev_data->tsc_res_ns);

	/* character device setup */
#ifndef NVPPS_NO_DT
	s_nvpps_class = class_create(THIS_MODULE, "nvpps");
	if (IS_ERR(s_nvpps_class)) {
		dev_err(&pdev->dev, "failed to allocate class\n");
		return PTR_ERR(s_nvpps_class);
	}

	err = alloc_chrdev_region(&s_nvpps_devt, 0, MAX_NVPPS_SOURCES, "nvpps");
	if (err < 0) {
		dev_err(&pdev->dev, "failed to allocate char device region\n");
		class_destroy(s_nvpps_class);
		return err;
	}
#endif /* !NVPPS_NO_DT */

	/* get an idr for the device */
	mutex_lock(&s_nvpps_lock);
	err = idr_alloc(&s_nvpps_idr, pdev_data, 0, MAX_NVPPS_SOURCES, GFP_KERNEL);
	if (err < 0) {
		if (err == -ENOSPC) {
			dev_err(&pdev->dev, "nvpps: out of idr \n");
			err = -EBUSY;
		}
		mutex_unlock(&s_nvpps_lock);
		return err;
	}
	pdev_data->id = err;
	mutex_unlock(&s_nvpps_lock);

	/* associate the cdev with the file operations */
	cdev_init(&pdev_data->cdev, &nvpps_fops);

	/* build up the device number */
	devt = MKDEV(MAJOR(s_nvpps_devt), pdev_data->id);
	pdev_data->cdev.owner = THIS_MODULE;

	/* create the device node */
	pdev_data->dev = device_create(s_nvpps_class, NULL, devt, pdev_data, "nvpps%d", pdev_data->id);
	if (IS_ERR(pdev_data->dev)) {
		err = PTR_ERR(pdev_data->dev);
		goto error_ret;
	}

	pdev_data->dev->release = nvpps_dev_release;

	err = cdev_add(&pdev_data->cdev, devt, 1);
	if (err) {
		dev_err(&pdev->dev, "nvpps: failed to add char device %d:%d\n",
			MAJOR(s_nvpps_devt), pdev_data->id);
		device_destroy(s_nvpps_class, pdev_data->dev->devt);
		goto error_ret;
	}

	dev_info(&pdev->dev, "nvpps cdev(%d:%d)\n", MAJOR(s_nvpps_devt), pdev_data->id);
	platform_set_drvdata(pdev, pdev_data);

	np_gte = of_find_compatible_node(NULL, NULL, "nvidia,tegra234-gte-aon");
	if (!np_gte) {
		np_gte = of_find_compatible_node(NULL, NULL, "nvidia,tegra194-gte-aon");
	}
	if (!np_gte) {
		pdev_data->use_gpio_int_timesatmp = false;
		dev_err(&pdev->dev, "of_find_compatible_node failed\n");
	} else {
		pdev_data->gte_ev_desc = tegra_gte_register_event(np_gte, pdev_data->gpio_pin);
		if (IS_ERR(pdev_data->gte_ev_desc)) {
			pdev_data->use_gpio_int_timesatmp = false;
			dev_err(&pdev->dev, "tegra_gte_register_event err = %d\n", (int)PTR_ERR(pdev_data->gte_ev_desc));
		} else {
			pdev_data->use_gpio_int_timesatmp = true;
			dev_info(pdev_data->dev, "tegra_gte_register_event succeed\n");
		}
	}

	/* setup PPS event hndler */
	err = set_mode(pdev_data, NVPPS_DEF_MODE);
	if (err) {
		dev_err(&pdev->dev, "set_mode failed err = %d\n", err);
		device_destroy(s_nvpps_class, pdev_data->dev->devt);
		goto error_ret;
	}
	pdev_data->evt_mode = NVPPS_DEF_MODE;

	return 0;

error_ret:
	cdev_del(&pdev_data->cdev);
	mutex_lock(&s_nvpps_lock);
	idr_remove(&s_nvpps_idr, pdev_data->id);
	mutex_unlock(&s_nvpps_lock);
	return err;
}


static int nvpps_remove(struct platform_device *pdev)
{
	struct nvpps_device_data	*pdev_data = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "%s\n", __FUNCTION__);

	if (pdev_data) {
		if (pdev_data->timer_inited) {
			pdev_data->timer_inited = false;
			del_timer_sync(&pdev_data->timer);
		}
		if (pdev_data->use_gpio_int_timesatmp) {
			if (!IS_ERR_OR_NULL(pdev_data->gte_ev_desc)) {
				tegra_gte_unregister_event(pdev_data->gte_ev_desc);
			}
			pdev_data->use_gpio_int_timesatmp = false;
		}
		if (pdev_data->memmap_phc_regs) {
			devm_iounmap(&pdev->dev, (void *)pdev_data->mac_base_addr);
			dev_info(&pdev->dev, "unmap MAC reg space %p for nvpps\n",
				(void *)pdev_data->mac_base_addr);
		}
		device_destroy(s_nvpps_class, pdev_data->dev->devt);
	}

#ifndef NVPPS_NO_DT
	class_unregister(s_nvpps_class);
	class_destroy(s_nvpps_class);
	unregister_chrdev_region(s_nvpps_devt, MAX_NVPPS_SOURCES);
#endif /* !NVPPS_NO_DT */
	return 0;
}


#ifdef CONFIG_PM
static int nvpps_suspend(struct platform_device *pdev, pm_message_t state)
{
	/* struct nvpps_device_data	*pdev_data = platform_get_drvdata(pdev); */

	return 0;
}

static int nvpps_resume(struct platform_device *pdev)
{
	/* struct nvpps_device_data	*pdev_data = platform_get_drvdata(pdev); */

	return 0;
}
#endif /* CONFIG_PM */


#ifndef NVPPS_NO_DT
static const struct of_device_id nvpps_of_table[] = {
	{ .compatible = "nvidia,tegra194-nvpps", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, nvpps_of_table);
#endif /* !NVPPS_NO_DT */


static struct platform_driver nvpps_plat_driver = {
	.driver = {
		.name = KBUILD_MODNAME,
		.owner = THIS_MODULE,
#ifndef NVPPS_NO_DT
		.of_match_table = of_match_ptr(nvpps_of_table),
#endif /* !NVPPS_NO_DT */
	},
	.probe = nvpps_probe,
	.remove = nvpps_remove,
#ifdef CONFIG_PM
	.suspend = nvpps_suspend,
	.resume = nvpps_resume,
#endif /* CONFIG_PM */
};


#ifdef NVPPS_NO_DT
/* module init
 */
static int __init nvpps_init(void)
{
	int err;

	printk("%s\n", __FUNCTION__);

	s_nvpps_class = class_create(THIS_MODULE, "nvpps");
	if (IS_ERR(s_nvpps_class)) {
		printk("nvpps: failed to allocate class\n");
		return PTR_ERR(s_nvpps_class);
	}

	err = alloc_chrdev_region(&s_nvpps_devt, 0, MAX_NVPPS_SOURCES, "nvpps");
	if (err < 0) {
		printk("nvpps: failed to allocate char device region\n");
		class_destroy(s_nvpps_class);
		return err;
	}

	printk("nvpps registered\n");

	return platform_driver_register(&nvpps_plat_driver);
}


/* module fini
 */
static void __exit nvpps_exit(void)
{
	printk("%s\n", __FUNCTION__);
	platform_driver_unregister(&nvpps_plat_driver);

	class_unregister(s_nvpps_class);
	class_destroy(s_nvpps_class);
	unregister_chrdev_region(s_nvpps_devt, MAX_NVPPS_SOURCES);
}

#endif /* NVPPS_NO_DT */


#ifdef NVPPS_NO_DT
module_init(nvpps_init);
module_exit(nvpps_exit);
#else /* !NVPPS_NO_DT */
module_platform_driver(nvpps_plat_driver);
#endif /* NVPPS_NO_DT */

MODULE_DESCRIPTION("NVidia Tegra PPS Driver");
MODULE_AUTHOR("David Tao tehyut@nvidia.com");
MODULE_LICENSE("GPL");
