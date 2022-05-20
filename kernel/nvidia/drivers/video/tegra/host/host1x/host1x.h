/*
 * drivers/video/tegra/host/host1x/host1x.h
 *
 * Tegra Graphics Host Driver Entrypoint
 *
 * Copyright (c) 2010-2020, NVIDIA Corporation. All rights reserved.
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

#ifndef __NVHOST_HOST1X_H
#define __NVHOST_HOST1X_H

#include <linux/cdev.h>
#include <linux/nvhost.h>
#include <uapi/linux/nvhost_ioctl.h>

#include "nvhost_syncpt.h"
#include "nvhost_channel.h"
#include "nvhost_intr.h"

#define TRACE_MAX_LENGTH	128U
#define IFACE_NAME		"nvhost"

struct nvhost_chip_support;
struct nvhost_channel;
struct mem_mgr;

extern long linsim_cl;

/*
 * Policy determines how do we store the syncpts,
 * i.e. either per channel (in struct nvhost_channel)
 * or per channel instance (in struct nvhost_channel_userctx)
 */
enum nvhost_syncpt_policy {
	SYNCPT_PER_CHANNEL = 0,
	SYNCPT_PER_CHANNEL_INSTANCE,
};

/*
 * Policy determines when to map HW channel to device,
 * i.e. either on channel device node open time
 * or on work submission time
 */
enum nvhost_channel_policy {
	MAP_CHANNEL_ON_OPEN = 0,
	MAP_CHANNEL_ON_SUBMIT,
};

struct host1x_device_info {
	/* Channel info */
	int		nb_channels;	/* host1x: num channels supported */
	int		ch_base;	/* host1x: channel base */
	int		ch_limit;	/* host1x: channel limit */
	enum nvhost_channel_policy channel_policy; /* host1x: channel policy */

	/* Syncpoint info */
	int		nb_hw_pts;	/* host1x: num syncpoints supported
					   in h/w */
	int		nb_pts;		/* host1x: num syncpoints supported
					   in s/w where nb_pts <= nb_hw_pts */
	int		pts_base;	/* host1x: syncpoint base */
	int		pts_limit;	/* host1x: syncpoint limit */
	int		nb_syncpt_irqs; /* host1x: number of syncpoint irqs */
	enum nvhost_syncpt_policy syncpt_policy; /* host1x: syncpoint policy */
	int		nb_mlocks;	/* host1x: number of mlocks */
	int		(*initialize_chip_support)(struct nvhost_master *,
						struct nvhost_chip_support *);
	int		nb_actmons;
	/* true if host1x access direct but engines are not owned */
	bool		vmserver_owns_engines;
	/* true if hw supports remote syncpoint interrupts */
	bool		use_cross_vm_interrupts;
	/* host1x: reg resources */
	char		*resources[NVHOST_MODULE_MAX_IORESOURCE_MEM];
	/* host1x: number of resources */
	int		nb_resources;
	/* cmdfifo only accessible from hypervisor? */
	bool		secure_cmdfifo;
	/* ctrl device node name if not default */
	const char	*ctrl_name;
	/* Size of a syncpoint page in the syncpoint<->mss interface */
	uint32_t	syncpt_page_size;
	/* If MLOCK locked-state can be written through register */
	bool		rw_mlock_register;
};

struct nvhost_master {
	void __iomem *aperture;
	void __iomem *sync_aperture;
	struct class *nvhost_class;
	struct cdev cdev;
	struct device *ctrl;
	struct nvhost_syncpt syncpt;
	struct nvhost_intr intr;
	struct platform_device *dev;
	atomic_t clientid;
	struct host1x_device_info info;
	struct nvhost_characteristics nvhost_char;
	struct kobject *caps_kobj;
	struct nvhost_capability_node *caps_nodes;
	int major;
	int next_minor;
	struct mutex chrdev_mutex;
	struct mutex timeout_mutex;

	struct nvhost_channel **chlist;	/* channel list */
	struct mutex chlist_mutex;	/* mutex for channel list */
	struct mutex ch_alloc_mutex;	/* mutex for channel allocation */
	struct semaphore free_channels; /* Semaphore tracking free channels */
	unsigned long allocated_channels[2];

	/* nvhost vm specific structures */
	struct list_head vm_list;
	struct mutex vm_mutex;
	struct mutex vm_alloc_mutex;

	/* for nvhost_masters list */
	struct list_head list;

	struct rb_root syncpt_backing_head;
};

#ifdef CONFIG_DEBUG_FS
void nvhost_debug_init(struct nvhost_master *master);
void nvhost_device_debug_init(struct platform_device *dev);
void nvhost_device_debug_deinit(struct platform_device *dev);
void nvhost_debug_dump(struct nvhost_master *master);
#else
static inline void nvhost_debug_init(struct nvhost_master *master)
{
}
static inline void nvhost_device_debug_init(struct platform_device *dev)
{
}
static inline void nvhost_device_debug_deinit(struct platform_device *dev)
{
}
static inline void nvhost_debug_dump(struct nvhost_master *master)
{
}
#endif

int nvhost_host1x_finalize_poweron(struct platform_device *dev);
int nvhost_host1x_prepare_poweroff(struct platform_device *dev);

void nvhost_set_chanops(struct nvhost_channel *ch);

int nvhost_gather_filter_enabled(struct nvhost_syncpt *sp);

int nvhost_update_characteristics(struct platform_device *dev);

static inline void *nvhost_get_falcon_data(struct platform_device *_dev)
{
	struct nvhost_device_data *pdata =
		(struct nvhost_device_data *)platform_get_drvdata(_dev);
	WARN_ON(!pdata);
	return pdata ? pdata->falcon_data : NULL;
}

static inline void nvhost_set_falcon_data(struct platform_device *_dev,
	void *priv_data)
{
	struct nvhost_device_data *pdata =
		(struct nvhost_device_data *)platform_get_drvdata(_dev);
	WARN_ON(!pdata);
	pdata->falcon_data = priv_data;
}


static inline void *nvhost_get_private_data(struct platform_device *_dev)
{
	struct nvhost_device_data *pdata =
		(struct nvhost_device_data *)platform_get_drvdata(_dev);
	WARN_ON(!pdata);
	return pdata ? pdata->private_data : NULL;
}

static inline void *nvhost_get_private_data_nowarn(struct platform_device *_dev)
{
	struct nvhost_device_data *pdata =
		(struct nvhost_device_data *)platform_get_drvdata(_dev);
	return pdata ? pdata->private_data : NULL;
}

static inline void nvhost_set_private_data(struct platform_device *_dev,
	void *priv_data)
{
	struct nvhost_device_data *pdata =
		(struct nvhost_device_data *)platform_get_drvdata(_dev);
	WARN_ON(!pdata);
	pdata->private_data = priv_data;
}

struct nvhost_master *nvhost_get_prim_host(void);

static inline struct nvhost_master *nvhost_get_host(
	struct platform_device *_dev)
{
	struct device *parent = _dev->dev.parent;
	struct device *dev = &_dev->dev;

	/*
	 * host1x has no parent dev on non-DT configuration or has
	 * platform_bus on DT configuration. So search for a device
	 * whose parent is NULL or platform_bus
	 */
	while (parent && parent != &platform_bus) {
		dev = parent;
		parent = parent->parent;
	}

	return nvhost_get_private_data(to_platform_device(dev));
}

static inline struct nvhost_master *nvhost_get_host_nowarn(
	struct platform_device *_dev)
{
	struct device *parent = _dev->dev.parent;
	struct device *dev = &_dev->dev;

	/*
	 * host1x has no parent dev on non-DT configuration or has
	 * platform_bus on DT configuration. So search for a device
	 * whose parent is NULL or platform_bus
	 */
	while (parent && parent != &platform_bus) {
		dev = parent;
		parent = parent->parent;
	}

	return nvhost_get_private_data_nowarn(to_platform_device(dev));
}

static inline struct platform_device *nvhost_get_parent(
	struct platform_device *_dev)
{
	return (_dev->dev.parent && _dev->dev.parent != &platform_bus)
		? to_platform_device(_dev->dev.parent) : NULL;
}

struct nvhost_master *nvhost_get_syncpt_owner(u32 id);
struct nvhost_syncpt *nvhost_get_syncpt_owner_struct(u32 id,
	struct nvhost_syncpt *default_syncpt);

#endif
