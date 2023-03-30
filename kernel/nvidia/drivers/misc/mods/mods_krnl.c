// SPDX-License-Identifier: GPL-2.0
/*
 * This file is part of NVIDIA MODS kernel driver.
 *
 * Copyright (c) 2008-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA MODS kernel driver is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * NVIDIA MODS kernel driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NVIDIA MODS kernel driver.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "mods_internal.h"

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/pagemap.h>
#include <linux/poll.h>
#include <linux/random.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#ifdef MODS_HAS_CONSOLE_LOCK
#   include <linux/console.h>
#   include <linux/kd.h>
#   include <linux/tty.h>
#   include <linux/console_struct.h>
#   include <linux/vt_kern.h>
#endif
#ifdef CONFIG_X86
#   include <linux/screen_info.h>
#   include <asm/msr.h>
#endif

/***********************************************************************
 * mods_krnl_* functions, driver interfaces called by the Linux kernel *
 ***********************************************************************/
static int mods_krnl_open(struct inode *, struct file *);
static int mods_krnl_close(struct inode *, struct file *);
static POLL_TYPE mods_krnl_poll(struct file *, poll_table *);
static int mods_krnl_mmap(struct file *, struct vm_area_struct *);
static long mods_krnl_ioctl(struct file *, unsigned int, unsigned long);

/* character driver entry points */
static const struct file_operations mods_fops = {
	.owner          = THIS_MODULE,
	.open           = mods_krnl_open,
	.release        = mods_krnl_close,
	.poll           = mods_krnl_poll,
	.mmap           = mods_krnl_mmap,
	.unlocked_ioctl = mods_krnl_ioctl,
#if defined(HAVE_COMPAT_IOCTL)
	.compat_ioctl   = mods_krnl_ioctl,
#endif
};

#define DEVICE_NAME "mods"

static struct miscdevice mods_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = DEVICE_NAME,
	.fops  = &mods_fops
};

#if defined(CONFIG_PCI)
static pci_ers_result_t mods_pci_error_detected(struct pci_dev *,
						pci_channel_state_t);
static pci_ers_result_t mods_pci_mmio_enabled(struct pci_dev *);
static void mods_pci_resume(struct pci_dev *);

static struct pci_error_handlers mods_pci_error_handlers = {
	.error_detected	= mods_pci_error_detected,
	.mmio_enabled	= mods_pci_mmio_enabled,
	.resume		= mods_pci_resume,
};

static const struct pci_device_id mods_pci_table[] = {
	{
		.vendor		= PCI_VENDOR_ID_NVIDIA,
		.device		= PCI_ANY_ID,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.class		= (PCI_CLASS_DISPLAY_VGA << 8),
		.class_mask	= ~0
	},
	{
		.vendor		= PCI_VENDOR_ID_NVIDIA,
		.device		= PCI_ANY_ID,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.class		= (PCI_CLASS_DISPLAY_3D << 8),
		.class_mask	= ~0
	},
	{
		.vendor		= PCI_VENDOR_ID_NVIDIA,
		.device		= PCI_ANY_ID,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.class		= (PCI_CLASS_BRIDGE_OTHER << 8),
		.class_mask	= ~0
	},
	{ 0 }
};

static int mods_pci_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
	mods_debug_printk(DEBUG_PCI,
			  "probed dev %04x:%02x:%02x.%x vendor %04x device %04x\n",
			  pci_domain_nr(dev->bus),
			  dev->bus->number,
			  PCI_SLOT(dev->devfn),
			  PCI_FUNC(dev->devfn),
			  dev->vendor, dev->device);

	return 0;
}

#if defined(CONFIG_PCI) && defined(MODS_HAS_SRIOV)
static int mods_pci_sriov_configure(struct pci_dev *dev, int numvfs);
#endif

static struct pci_driver mods_pci_driver = {
	.name            = DEVICE_NAME,
	.id_table        = mods_pci_table,
	.probe           = mods_pci_probe,
	.err_handler     = &mods_pci_error_handlers,
#ifdef MODS_HAS_SRIOV
	.sriov_configure = mods_pci_sriov_configure,
#endif
};
#endif

/***********************************************
 * module wide parameters and access functions *
 * used to avoid globalization of variables    *
 ***********************************************/

#ifdef MODS_HAS_TEGRA
#       define MODS_MULTI_INSTANCE_DEFAULT_VALUE 1
#else
#       define MODS_MULTI_INSTANCE_DEFAULT_VALUE 0
#endif

static int debug;
static int multi_instance = MODS_MULTI_INSTANCE_DEFAULT_VALUE;
static u32 access_token = MODS_ACCESS_TOKEN_NONE;

#if defined(CONFIG_PCI) && defined(MODS_HAS_SRIOV)
static int mods_pci_sriov_configure(struct pci_dev *dev, int numvfs)
{
	int totalvfs;
	int err = 0;

	LOG_ENT();

	totalvfs = pci_sriov_get_totalvfs(dev);

	if (numvfs > 0) {
		err = pci_enable_sriov(dev, numvfs);

		if (unlikely(err)) {
			mods_error_printk(
				"failed to enable sriov on dev %04x:%02x:%02x.%x %s numvfs=%d (totalvfs=%d), err=%d\n",
				pci_domain_nr(dev->bus),
				dev->bus->number,
				PCI_SLOT(dev->devfn),
				PCI_FUNC(dev->devfn),
				dev->is_physfn ? "physfn" : "virtfn",
				numvfs,
				totalvfs,
				err);
			numvfs = err;
		} else {
			mods_info_printk(
				"enabled sriov on dev %04x:%02x:%02x.%x %s numvfs=%d (totalvfs=%d)\n",
				pci_domain_nr(dev->bus),
				dev->bus->number,
				PCI_SLOT(dev->devfn),
				PCI_FUNC(dev->devfn),
				dev->is_physfn ? "physfn" : "virtfn",
				numvfs,
				totalvfs);
		}

	} else {
		pci_disable_sriov(dev);

		numvfs = 0;

		mods_info_printk(
			"disabled sriov on dev %04x:%02x:%02x.%x %s (totalvfs=%d)\n",
			pci_domain_nr(dev->bus),
			dev->bus->number,
			PCI_SLOT(dev->devfn),
			PCI_FUNC(dev->devfn),
			dev->is_physfn ? "physfn" : "virtfn",
			totalvfs);
	}

	/* If this function has been invoked via an ioctl, remember numvfs */
	if (!err) {
		struct en_dev_entry *dpriv = pci_get_drvdata(dev);

		if (dpriv)
			dpriv->num_vfs = numvfs;
	}

	LOG_EXT();
	return numvfs;
}

static int esc_mods_set_num_vf(struct mods_client     *client,
			       struct MODS_SET_NUM_VF *p)
{
	int                  err;
	struct pci_dev      *dev = NULL;
	struct en_dev_entry *dpriv;

	LOG_ENT();

	/* Get the PCI device structure for the specified device from kernel */
	err = mods_find_pci_dev(client, &p->dev, &dev);
	if (unlikely(err)) {
		if (err == -ENODEV)
			cl_error("dev %04x:%02x:%02x.%x not found\n",
				 p->dev.domain,
				 p->dev.bus,
				 p->dev.device,
				 p->dev.function);
		LOG_EXT();
		return err;
	}

	dpriv = pci_get_drvdata(dev);
	if (!dpriv) {
		cl_error(
			"failed to enable sriov, dev %04x:%02x:%02x.%x was not enabled\n",
			pci_domain_nr(dev->bus),
			dev->bus->number,
			PCI_SLOT(dev->devfn),
			PCI_FUNC(dev->devfn));
		err = -EBUSY;
		goto error;
	}
	if (dpriv->client_id != client->client_id) {
		cl_error(
			"invalid client for dev %04x:%02x:%02x.%x, expected %u\n",
			pci_domain_nr(dev->bus),
			dev->bus->number,
			PCI_SLOT(dev->devfn),
			PCI_FUNC(dev->devfn),
			dpriv->client_id);
		err = -EBUSY;
		goto error;
	}

	err = mods_pci_sriov_configure(dev, p->numvfs);

error:
	pci_dev_put(dev);
	LOG_EXT();
	return err;
}

static int esc_mods_set_total_vf(struct mods_client     *client,
				 struct MODS_SET_NUM_VF *p)
{
	int                  err;
	struct pci_dev      *dev = NULL;
	struct en_dev_entry *dpriv;

	LOG_ENT();

	/* Get the PCI device structure for the specified device from kernel */
	err = mods_find_pci_dev(client, &p->dev, &dev);
	if (unlikely(err)) {
		if (err == -ENODEV)
			cl_error("dev %04x:%02x:%02x.%x not found\n",
				 p->dev.domain,
				 p->dev.bus,
				 p->dev.device,
				 p->dev.function);
		LOG_EXT();
		return -EINVAL;
	}

	dpriv = pci_get_drvdata(dev);
	if (!dpriv) {
		cl_error(
			"failed to enable sriov, dev %04x:%02x:%02x.%x was not enabled\n",
			pci_domain_nr(dev->bus),
			dev->bus->number,
			PCI_SLOT(dev->devfn),
			PCI_FUNC(dev->devfn));
		err = -EBUSY;
		goto error;
	}
	if (dpriv->client_id != client->client_id) {
		cl_error(
			"invalid client for dev %04x:%02x:%02x.%x, expected %u\n",
			pci_domain_nr(dev->bus),
			dev->bus->number,
			PCI_SLOT(dev->devfn),
			PCI_FUNC(dev->devfn),
			dpriv->client_id);
		err = -EBUSY;
		goto error;
	}

	err = pci_sriov_set_totalvfs(dev, p->numvfs);

	if (unlikely(err)) {
		cl_error(
			"failed to set totalvfs=%d on dev %04x:%02x:%02x.%x, err=%d\n",
			p->numvfs,
			p->dev.domain,
			p->dev.bus,
			p->dev.device,
			p->dev.function,
			err);
	} else
		cl_info("set totalvfs %d on dev %04x:%02x:%02x.%x\n",
			p->numvfs,
			p->dev.domain,
			p->dev.bus,
			p->dev.device,
			p->dev.function);

error:
	pci_dev_put(dev);
	LOG_EXT();
	return err;
}
#endif

#if defined(CONFIG_PPC64)
static int ppc_tce_bypass = MODS_PPC_TCE_BYPASS_ON;

void mods_set_ppc_tce_bypass(int bypass)
{
	ppc_tce_bypass = bypass;
}

int mods_get_ppc_tce_bypass(void)
{
	return ppc_tce_bypass;
}
#endif

void mods_set_debug_level(int mask)
{
	debug = mask;
}

int mods_get_debug_level(void)
{
	return debug;
}

int mods_check_debug_level(int mask)
{
	return ((debug & mask) == mask) ? 1 : 0;
}

void mods_set_multi_instance(int mi)
{
	multi_instance = (mi > 0) ? 1 : -1;
}

int mods_get_multi_instance(void)
{
	return multi_instance > 0;
}

u32 mods_get_access_token(void)
{
	return access_token;
}

static int validate_client(struct mods_client *client)
{
	if (!client) {
		mods_error_printk("invalid client\n");
		return false;
	}

	if (client->client_id < 1 || client->client_id > MODS_MAX_CLIENTS) {
		cl_error("invalid client id\n");
		return false;
	}

	return true;
}

static int mods_set_access_token(u32 tok)
{
	/* When setting a null token, the existing token must match the
	 * provided token, when setting a non-null token the existing token
	 * must be null, use atomic compare/exchange to set it
	 */
	u32 req_old_token =
	    (tok == MODS_ACCESS_TOKEN_NONE) ?
		access_token : MODS_ACCESS_TOKEN_NONE;

	if (cmpxchg(&access_token, req_old_token, tok) != req_old_token)
		return -EFAULT;
	return OK;
}

static int mods_check_access_token(struct mods_client *client)
{
	if (client->access_token != mods_get_access_token()) {
		cl_error("invalid access token %u\n", client->access_token);
		return -EFAULT;
	}

	return OK;
}

/******************************
 * INIT/EXIT MODULE FUNCTIONS *
 ******************************/
static int __init mods_init_module(void)
{
	int rc;

	LOG_ENT();

	mods_init_irq();

	rc = misc_register(&mods_dev);
	if (rc < 0)
		return -EBUSY;

#if defined(CONFIG_PCI)
	rc = pci_register_driver(&mods_pci_driver);
	if (rc < 0)
		return -EBUSY;
#endif

#if defined(MODS_HAS_TEGRA) && defined(CONFIG_COMMON_CLK) && \
	defined(CONFIG_OF_RESOLVE) && defined(CONFIG_OF_DYNAMIC)
	mods_init_clock_api();
#endif

	rc = mods_create_debugfs(&mods_dev);
	if (rc < 0)
		return rc;

	rc = mods_init_dmabuf();
	if (rc < 0)
		return rc;

#if defined(MODS_HAS_TEGRA)
	rc = smmu_driver_init();
	if (rc < 0)
		return rc;

	/* tegra prod */
	mods_tegra_prod_init(&mods_dev);

#if defined(CONFIG_DMA_ENGINE)
	mods_init_dma();
#endif
#endif

	mods_info_printk("*** WARNING: DIAGNOSTIC DRIVER LOADED ***\n");
	mods_info_printk("driver loaded, version %x.%02x\n",
			 (MODS_DRIVER_VERSION >> 8),
			 (MODS_DRIVER_VERSION & 0xFF));

	if (debug)
		mods_info_printk("debug level 0x%x\n", debug);

	LOG_EXT();
	return OK;
}

static void __exit mods_exit_module(void)
{
	LOG_ENT();

	mods_exit_dmabuf();

	mods_remove_debugfs();

	mods_cleanup_irq();

#if defined(MODS_HAS_TEGRA)
#if defined(CONFIG_DMA_ENGINE)
	mods_exit_dma();
#endif
	smmu_driver_exit();
#endif

#if defined(CONFIG_PCI)
	pci_unregister_driver(&mods_pci_driver);
#endif

	misc_deregister(&mods_dev);

#if defined(MODS_HAS_TEGRA) && defined(CONFIG_COMMON_CLK) && \
	defined(CONFIG_OF_RESOLVE) && defined(CONFIG_OF_DYNAMIC)
	mods_shutdown_clock_api();
#endif

	mods_info_printk("driver unloaded\n");
	LOG_EXT();
}

/***************************
 * KERNEL INTERFACE SET UP *
 ***************************/
module_init(mods_init_module);
module_exit(mods_exit_module);

MODULE_LICENSE("GPL");

#define STRING_VALUE(x) #x
#define MAKE_MODULE_VERSION(x, y) STRING_VALUE(x) "." STRING_VALUE(y)
MODULE_VERSION(MAKE_MODULE_VERSION(MODS_DRIVER_VERSION_MAJOR,
				   MODS_DRIVER_VERSION_MINOR));

module_param(debug, int, 0644);
MODULE_PARM_DESC(debug,
	"debug bitflags (2=ioctl 4=pci 8=acpi 16=irq 32=mem 64=fun +256=detailed)");

module_param(multi_instance, int, 0644);
MODULE_PARM_DESC(multi_instance,
	"allows more than one client to simultaneously open the driver");

#if defined(CONFIG_PPC64)
module_param(ppc_tce_bypass, int, 0644);
MODULE_PARM_DESC(ppc_tce_bypass,
	"PPC TCE bypass (0=sys default, 1=force bypass, 2=force non bypass)");
#endif

/********************
 * HELPER FUNCTIONS *
 ********************/
static void mods_disable_all_devices(struct mods_client *client)
{
#ifdef CONFIG_PCI
	if (unlikely(mutex_lock_interruptible(mods_get_irq_mutex())))
		return;

	while (client->enabled_devices != NULL) {
		struct en_dev_entry *old = client->enabled_devices;

		mods_disable_device(client, old->dev);
		client->enabled_devices = old->next;
		kfree(old);
		atomic_dec(&client->num_allocs);
	}

	mutex_unlock(mods_get_irq_mutex());

	if (client->cached_dev) {
		pci_dev_put(client->cached_dev);
		client->cached_dev = NULL;
	}
#else
	WARN_ON(client->enabled_devices != NULL);
#endif
}

#if defined(MODS_HAS_CONSOLE_LOCK)
static int mods_resume_console(struct mods_client *client);
#else
static inline int mods_resume_console(struct mods_client *client) { return 0; }
#endif

/*********************
 * MAPPING FUNCTIONS *
 *********************/
static int mods_register_mapping(
	struct mods_client   *client,
	struct MODS_MEM_INFO *p_mem_info,
	u64                   dma_addr,
	u64                   virtual_address,
	u64                   mapping_length)
{
	struct SYS_MAP_MEMORY *p_map_mem;

	LOG_ENT();

	p_map_mem = kzalloc(sizeof(*p_map_mem), GFP_KERNEL | __GFP_NORETRY);
	if (unlikely(!p_map_mem)) {
		LOG_EXT();
		return -ENOMEM;
	}
	atomic_inc(&client->num_allocs);

	p_map_mem->dma_addr = dma_addr;
	p_map_mem->virtual_addr = virtual_address;
	p_map_mem->mapping_length = mapping_length;
	p_map_mem->p_mem_info = p_mem_info;

	list_add(&p_map_mem->list, &client->mem_map_list);

	cl_debug(DEBUG_MEM_DETAILED,
		 "map alloc %p as %p: phys 0x%llx, virt 0x%llx, size 0x%llx\n",
		 p_mem_info,
		 p_map_mem,
		 dma_addr,
		 virtual_address,
		 mapping_length);

	LOG_EXT();
	return OK;
}

static void mods_unregister_mapping(struct mods_client *client,
				    u64                 virtual_address)
{
	struct SYS_MAP_MEMORY *p_map_mem;
	struct list_head      *head   = &client->mem_map_list;
	struct list_head      *iter;

	LOG_ENT();

	list_for_each(iter, head) {
		p_map_mem = list_entry(iter, struct SYS_MAP_MEMORY, list);

		if (p_map_mem->virtual_addr == virtual_address) {
			/* remove from the list */
			list_del(iter);

			/* free our data struct which keeps track of mapping */
			kfree(p_map_mem);
			atomic_dec(&client->num_allocs);

			return;
		}
	}

	LOG_EXT();
}

#ifdef CONFIG_HAVE_IOREMAP_PROT
static struct SYS_MAP_MEMORY *mods_find_mapping(struct mods_client *client,
						u64 virtual_address)
{
	struct SYS_MAP_MEMORY *p_map_mem;
	struct list_head      *head   = &client->mem_map_list;
	struct list_head      *iter;

	LOG_ENT();

	list_for_each(iter, head) {
		p_map_mem = list_entry(iter, struct SYS_MAP_MEMORY, list);

		if (p_map_mem->virtual_addr == virtual_address) {
			LOG_EXT();
			return p_map_mem;
		}
	}

	LOG_EXT();

	return NULL;
}
#endif

static void mods_unregister_all_mappings(struct mods_client *client)
{
	struct SYS_MAP_MEMORY *p_map_mem;
	struct list_head      *head   = &client->mem_map_list;
	struct list_head      *iter;
	struct list_head      *tmp;

	LOG_ENT();

	list_for_each_safe(iter, tmp, head) {
		p_map_mem = list_entry(iter, struct SYS_MAP_MEMORY, list);
		mods_unregister_mapping(client, p_map_mem->virtual_addr);
	}

	LOG_EXT();
}

static pgprot_t mods_get_prot(struct mods_client *client,
			      u8                  mem_type,
			      pgprot_t            prot)
{
	switch (mem_type) {
	case MODS_ALLOC_CACHED:
		return prot;

	case MODS_ALLOC_UNCACHED:
		return MODS_PGPROT_UC(prot);

	case MODS_ALLOC_WRITECOMBINE:
		return MODS_PGPROT_WC(prot);

	default:
		cl_warn("unsupported memory type: %u\n", mem_type);
		return prot;
	}
}

static pgprot_t mods_get_prot_for_range(struct mods_client *client,
					u64                 dma_addr,
					u64                 size,
					pgprot_t            prot)
{
	if ((dma_addr == client->mem_type.dma_addr) &&
		(size == client->mem_type.size)) {

		return mods_get_prot(client, client->mem_type.type, prot);
	}
	return prot;
}

const char *mods_get_prot_str(u8 mem_type)
{
	switch (mem_type) {
	case MODS_ALLOC_CACHED:
		return "WB";

	case MODS_ALLOC_UNCACHED:
		return "UC";

	case MODS_ALLOC_WRITECOMBINE:
		return "WC";

	default:
		return "unknown";
	}
}

static const char *mods_get_prot_str_for_range(struct mods_client *client,
					       u64                 dma_addr,
					       u64                 size)
{
	if ((dma_addr == client->mem_type.dma_addr) &&
		(size == client->mem_type.size)) {

		return mods_get_prot_str(client->mem_type.type);
	}
	return "default";
}
/********************
 * PCI ERROR FUNCTIONS *
 ********************/
#if defined(CONFIG_PCI)
static pci_ers_result_t mods_pci_error_detected(struct pci_dev *dev,
						pci_channel_state_t state)
{
	mods_debug_printk(DEBUG_PCI,
			  "pci_error_detected dev %04x:%02x:%02x.%x\n",
			  pci_domain_nr(dev->bus),
			  dev->bus->number,
			  PCI_SLOT(dev->devfn),
			  PCI_FUNC(dev->devfn));

	return PCI_ERS_RESULT_CAN_RECOVER;
}

static pci_ers_result_t mods_pci_mmio_enabled(struct pci_dev *dev)
{
	mods_debug_printk(DEBUG_PCI,
			  "pci_mmio_enabled dev %04x:%02x:%02x.%x\n",
			  pci_domain_nr(dev->bus),
			  dev->bus->number,
			  PCI_SLOT(dev->devfn),
			  PCI_FUNC(dev->devfn));

	return PCI_ERS_RESULT_NEED_RESET;
}

static void mods_pci_resume(struct pci_dev *dev)
{
	mods_debug_printk(DEBUG_PCI,
			  "pci_resume dev %04x:%02x:%02x.%x\n",
			  pci_domain_nr(dev->bus),
			  dev->bus->number,
			  PCI_SLOT(dev->devfn),
			  PCI_FUNC(dev->devfn));
}
#endif

/********************
 * KERNEL FUNCTIONS *
 ********************/
static void mods_krnl_vma_open(struct vm_area_struct *vma)
{
	struct mods_vm_private_data *priv;

	LOG_ENT();
	mods_debug_printk(DEBUG_MEM_DETAILED,
			  "open vma, virt 0x%lx, phys 0x%llx\n",
			  vma->vm_start,
			  (u64)vma->vm_pgoff << PAGE_SHIFT);

	priv = vma->vm_private_data;
	if (priv)
		atomic_inc(&priv->usage_count);

	LOG_EXT();
}

static void mods_krnl_vma_close(struct vm_area_struct *vma)
{
	struct mods_vm_private_data *priv;

	LOG_ENT();

	priv = vma->vm_private_data;
	if (priv && atomic_dec_and_test(&priv->usage_count)) {
		struct mods_client *client = priv->client;

		if (unlikely(mutex_lock_interruptible(
					&client->mtx))) {
			LOG_EXT();
			return;
		}

		/* we need to unregister the mapping */
		mods_unregister_mapping(client, vma->vm_start);
		mods_debug_printk(DEBUG_MEM_DETAILED,
				  "closed vma, virt 0x%lx\n",
				  vma->vm_start);
		vma->vm_private_data = NULL;
		kfree(priv);
		atomic_dec(&client->num_allocs);

		mutex_unlock(&client->mtx);
	}

	LOG_EXT();
}

#ifdef CONFIG_HAVE_IOREMAP_PROT
static int mods_krnl_vma_access(struct vm_area_struct *vma,
				unsigned long          addr,
				void                  *buf,
				int                    len,
				int                    write)
{
	int                          err  = OK;
	struct mods_vm_private_data *priv = vma->vm_private_data;
	struct mods_client          *client;
	struct SYS_MAP_MEMORY       *p_map_mem;
	u64                          map_offs;

	LOG_ENT();

	if (!priv) {
		LOG_EXT();
		return -EINVAL;
	}

	client = priv->client;

	cl_debug(DEBUG_MEM_DETAILED,
		 "access vma, virt 0x%lx, phys 0x%llx\n",
		 vma->vm_start,
		 (u64)vma->vm_pgoff << PAGE_SHIFT);

	if (unlikely(mutex_lock_interruptible(&client->mtx))) {
		LOG_EXT();
		return -EINTR;
	}

	p_map_mem = mods_find_mapping(client, vma->vm_start);

	if (unlikely(!p_map_mem || addr < p_map_mem->virtual_addr ||
		     addr + len > p_map_mem->virtual_addr +
				  p_map_mem->mapping_length)) {
		mutex_unlock(&client->mtx);
		LOG_EXT();
		return -ENOMEM;
	}

	map_offs = addr - vma->vm_start;

	if (p_map_mem->p_mem_info) {
		struct MODS_MEM_INFO   *p_mem_info = p_map_mem->p_mem_info;
		struct MODS_PHYS_CHUNK *chunk;
		struct MODS_PHYS_CHUNK *end_chunk;

		chunk     = &p_mem_info->pages[0];
		end_chunk = chunk + p_mem_info->num_chunks;

		for ( ; chunk < end_chunk; chunk++) {
			const u32 chunk_size = PAGE_SIZE << chunk->order;

			if (!chunk->p_page) {
				chunk = end_chunk;
				break;
			}

			if (map_offs < chunk_size)
				break;

			map_offs -= chunk_size;
		}

		if (unlikely(chunk >= end_chunk))
			err = -ENOMEM;
		else {
			void        *ptr;
			struct page *p_page = chunk->p_page +
					      (map_offs >> PAGE_SHIFT);

			map_offs &= ~PAGE_MASK;

			if (map_offs + len > PAGE_SIZE)
				len = PAGE_SIZE - map_offs;

			ptr = kmap(p_page);
			if (ptr) {
				char *bptr = (char *)ptr + map_offs;

				if (write)
					memcpy(bptr, buf, len);
				else
					memcpy(buf, bptr, len);
				kunmap(ptr);

				err = len;
			} else
				err = -ENOMEM;
		}
	} else if (!write) {
		char __iomem *ptr;
		u64           pa;

		map_offs += (u64)vma->vm_pgoff << PAGE_SHIFT;
		pa = map_offs & PAGE_MASK;
		map_offs &= ~PAGE_MASK;

		if (map_offs + len > PAGE_SIZE)
			len = PAGE_SIZE - map_offs;

		ptr = ioremap(pa, PAGE_SIZE);

		if (ptr) {
			memcpy_fromio(buf, ptr + map_offs, len);

			iounmap(ptr);

			err = len;
		} else
			err = -ENOMEM;
	} else
		/* Writing to device memory from gdb is not supported */
		err = -ENOMEM;

	mutex_unlock(&client->mtx);

	LOG_EXT();
	return err;
}
#endif

static const struct vm_operations_struct mods_krnl_vm_ops = {
#ifdef CONFIG_HAVE_IOREMAP_PROT
	.access = mods_krnl_vma_access,
#endif
	.open	= mods_krnl_vma_open,
	.close	= mods_krnl_vma_close
};

static int mods_krnl_open(struct inode *ip, struct file *fp)
{
	struct mods_client *client;

	LOG_ENT();

	client = mods_alloc_client();
	if (client == NULL) {
		mods_error_printk("too many clients\n");
		LOG_EXT();
		return -EBUSY;
	}

	fp->private_data = client;

	cl_info("driver opened, pid=%d\n", current->pid);
	LOG_EXT();
	return OK;
}

static int mods_krnl_close(struct inode *ip, struct file *fp)
{
	struct mods_client *client    = fp->private_data;
	int                 final_err = OK;
	int                 err       = OK;
	u8                  client_id;

	LOG_ENT();

	if (!validate_client(client)) {
		LOG_EXT();
		return -EINVAL;
	}

	client_id = client->client_id;

	mods_free_client_interrupts(client);

	mods_resume_console(client);

	mods_unregister_all_mappings(client);
	err = mods_unregister_all_alloc(client);
	if (err)
		cl_error("failed to free all memory\n");
	final_err = err;

#if defined(CONFIG_PPC64)
	err = mods_unregister_all_ppc_tce_bypass(client);
	if (err)
		cl_error("failed to restore dma bypass\n");
	if (!final_err)
		final_err = err;

	err = mods_unregister_all_nvlink_sysmem_trained(client);
	if (err)
		cl_error("failed to free nvlink trained\n");
	if (!final_err)
		final_err = err;
#endif

	mods_disable_all_devices(client);

	{
		unsigned long num_allocs = atomic_read(&client->num_allocs);
		unsigned long num_pages  = atomic_read(&client->num_pages);

		if (num_allocs || num_pages) {
			cl_error(
				"not all allocations have been freed, allocs=%lu, pages=%lu\n",
				num_allocs, num_pages);
			if (!final_err)
				final_err = -ENOMEM;
		}
	}

	if (client->work_queue) {
		destroy_workqueue(client->work_queue);
		client->work_queue = NULL;
	}

	mods_free_client(client_id);

	pr_info("mods [%d]: driver closed\n", client_id);

	LOG_EXT();
	return final_err;
}

static POLL_TYPE mods_krnl_poll(struct file *fp, poll_table *wait)
{
	struct mods_client *client = fp->private_data;
	POLL_TYPE           mask   = 0;
	int                 err;

	if (!validate_client(client))
		return POLLERR;

	err = mods_check_access_token(client);
	if (err < 0)
		return POLLERR;

	if (!(fp->f_flags & O_NONBLOCK)) {
		cl_debug(DEBUG_ISR_DETAILED, "poll wait\n");
		poll_wait(fp, &client->interrupt_event, wait);
	}

	/* if any interrupts pending then check intr, POLLIN on irq */
	mask |= mods_irq_event_check(client->client_id);

	cl_debug(DEBUG_ISR_DETAILED, "poll mask 0x%x\n", mask);

	return mask;
}

static int mods_krnl_map_inner(struct mods_client    *client,
			       struct vm_area_struct *vma);

static int mods_krnl_mmap(struct file *fp, struct vm_area_struct *vma)
{
	struct mods_vm_private_data *vma_private_data;
	struct mods_client          *client = fp->private_data;
	int err;

	LOG_ENT();

	if (!validate_client(client)) {
		LOG_EXT();
		return -EINVAL;
	}

	err = mods_check_access_token(client);
	if (err < 0) {
		LOG_EXT();
		return err;
	}

	vma->vm_ops = &mods_krnl_vm_ops;

	vma_private_data = kzalloc(sizeof(*vma_private_data),
				   GFP_KERNEL | __GFP_NORETRY);
	if (unlikely(!vma_private_data)) {
		LOG_EXT();
		return -ENOMEM;
	}
	atomic_inc(&client->num_allocs);

	/* set private data for vm_area_struct */
	atomic_set(&vma_private_data->usage_count, 0);
	vma_private_data->client = client;
	vma->vm_private_data = vma_private_data;

	/* call for the first time open function */
	mods_krnl_vma_open(vma);

	{
		int err = OK;

		if (unlikely(mutex_lock_interruptible(&client->mtx)))
			err = -EINTR;
		else {
			err = mods_krnl_map_inner(client, vma);
			mutex_unlock(&client->mtx);
		}
		LOG_EXT();
		return err;
	}
}

static int mods_krnl_map_inner(struct mods_client    *client,
			       struct vm_area_struct *vma)
{
	const u64             req_pa     = (u64)vma->vm_pgoff << PAGE_SHIFT;
	struct MODS_MEM_INFO *p_mem_info = mods_find_alloc(client, req_pa);
	const u64             vma_size   = (u64)(vma->vm_end - vma->vm_start);
	const u32             req_pages  = vma_size >> PAGE_SHIFT;

	if ((req_pa   & ~PAGE_MASK) != 0 ||
	    (vma_size & ~PAGE_MASK) != 0) {
		cl_error("requested mapping is not page-aligned\n");
		return -EINVAL;
	}

	/* system memory */
	if (p_mem_info) {
		u32                     first, i;
		struct MODS_PHYS_CHUNK *chunks     = p_mem_info->pages;
		u32                     have_pages = 0;
		unsigned long           map_va     = 0;
		const pgprot_t          prot       = mods_get_prot(client,
				    p_mem_info->cache_type, vma->vm_page_prot);

		/* Find the beginning of the requested range */
		for (first = 0; first < p_mem_info->num_chunks; first++) {
			u64 dma_addr = chunks[first].dma_addr;
			u32 size     = PAGE_SIZE << chunks[first].order;

			if ((req_pa >= dma_addr) &&
			    (req_pa <  dma_addr + size)) {
				break;
			}
		}

		if (first == p_mem_info->num_chunks) {
			cl_error("can't satisfy requested mapping\n");
			return -EINVAL;
		}

		/* Count how many remaining pages we have in the allocation */
		for (i = first; i < p_mem_info->num_chunks; i++) {
			if (i == first) {
				u64 aoffs      = req_pa - chunks[i].dma_addr;
				u32 skip_pages = aoffs >> PAGE_SHIFT;

				have_pages -= skip_pages;
			}
			have_pages += 1U << chunks[i].order;
		}

		if (have_pages < req_pages) {
			cl_error("requested mapping exceeds bounds\n");
			return -EINVAL;
		}

		/* Map pages into VA space */
		map_va     = vma->vm_start;
		have_pages = req_pages;
		for (i = first; have_pages > 0; i++) {
			u64 map_pa    = MODS_DMA_TO_PHYS(chunks[i].dma_addr);
			u32 map_size  = PAGE_SIZE << chunks[i].order;
			u32 map_pages = 1U << chunks[i].order;

			if (i == first) {
				u64 aoffs = req_pa - chunks[i].dma_addr;

				map_pa    += aoffs;
				map_size  -= aoffs;
				map_pages -= aoffs >> PAGE_SHIFT;
			}

			if (map_pages > have_pages) {
				map_size  = have_pages << PAGE_SHIFT;
				map_pages = have_pages;
			}

			cl_debug(DEBUG_MEM_DETAILED,
				 "remap va 0x%lx pfn 0x%x size 0x%x pages 0x%x\n",
				 map_va, (unsigned int)(map_pa>>PAGE_SHIFT),
				 map_size,
				 map_pages);

			if (remap_pfn_range(vma,
					    map_va,
					    map_pa>>PAGE_SHIFT,
					    map_size,
					    prot)) {
				cl_error("failed to map memory\n");
				return -EAGAIN;
			}

			map_va     += map_size;
			have_pages -= map_pages;
		}

		mods_register_mapping(client,
				      p_mem_info,
				      chunks[first].dma_addr,
				      vma->vm_start,
				      vma_size);

	} else {
		/* device memory */

		cl_debug(DEBUG_MEM,
			 "map dev: phys 0x%llx, virt 0x%lx, size 0x%lx, %s\n",
			 req_pa,
			 (unsigned long)vma->vm_start,
			 (unsigned long)vma_size,
			 mods_get_prot_str_for_range(client, req_pa, vma_size));

		if (io_remap_pfn_range(
				vma,
				vma->vm_start,
				req_pa>>PAGE_SHIFT,
				vma_size,
				mods_get_prot_for_range(
					client,
					req_pa,
					vma_size,
					vma->vm_page_prot))) {
			cl_error("failed to map device memory\n");
			return -EAGAIN;
		}

		mods_register_mapping(client,
				      NULL,
				      req_pa,
				      vma->vm_start,
				      vma_size);
	}
	return OK;
}

#if defined(CONFIG_X86)
static void mods_get_screen_info(struct MODS_SCREEN_INFO *p)
{
	p->orig_video_mode = screen_info.orig_video_mode;
	p->orig_video_is_vga = screen_info.orig_video_isVGA;
	p->lfb_width = screen_info.lfb_width;
	p->lfb_height = screen_info.lfb_height;
	p->lfb_depth = screen_info.lfb_depth;
	p->lfb_base = screen_info.lfb_base;
	p->lfb_size = screen_info.lfb_size;
	p->lfb_linelength = screen_info.lfb_linelength;
}
#endif

/*************************
 * ESCAPE CALL FUNCTIONS *
 *************************/

static int esc_mods_get_api_version(struct mods_client      *client,
				    struct MODS_GET_VERSION *p)
{
	p->version = MODS_DRIVER_VERSION;
	return OK;
}

static int esc_mods_get_kernel_version(struct mods_client      *client,
				       struct MODS_GET_VERSION *p)
{
	p->version = MODS_KERNEL_VERSION;
	return OK;
}

#if defined(CONFIG_X86)
static int esc_mods_get_screen_info(struct mods_client      *client,
				    struct MODS_SCREEN_INFO *p)
{
	mods_get_screen_info(p);

#if defined(VIDEO_CAPABILITY_64BIT_BASE)
	if (screen_info.ext_lfb_base)
		return -EOVERFLOW;
#endif

	return OK;
}

static int esc_mods_get_screen_info_2(struct mods_client        *client,
				      struct MODS_SCREEN_INFO_2 *p)
{
#if defined(CONFIG_FB)
	unsigned int i;
	bool         found = false;
#endif

	mods_get_screen_info(&p->info);

#if defined(VIDEO_CAPABILITY_64BIT_BASE)
	p->ext_lfb_base = screen_info.ext_lfb_base;
#else
	p->ext_lfb_base = 0;
#endif

#if defined(CONFIG_FB)
	if (screen_info.orig_video_isVGA != VIDEO_TYPE_EFI)
		return OK;

	/* With pci=realloc on the kernel command line, GPU BAR1 can be
	 * reassigned after the OS console is allocated.  When this
	 * occurs the lfb_base variable is *not* updated for an EFI
	 * console.  The incorrect lfb_base variable will prevent other
	 * drivers or user space applications from identifying memory
	 * in use by the console and potentially using it themselves.

	 * For an EFI console, pull the FB base address from the FB
	 * driver registered_fb data instead of screen_info
	 */
	for (i = 0; i < ARRAY_SIZE(registered_fb); i++) {
		bool skipped = true;

		if (!registered_fb[i])
			continue;

		if (!strcmp(registered_fb[i]->fix.id, "EFI VGA") && !found) {
			p->info.lfb_base =
			 registered_fb[i]->fix.smem_start & 0xFFFFFFFF;
			p->ext_lfb_base =
			 registered_fb[i]->fix.smem_start >> 32;
			found = true;
			skipped = false;
		}

		cl_info("%s fb%d '%s' @0x%llx\n",
			skipped ? "skip" : "found",
			i, registered_fb[i]->fix.id,
			(u64)registered_fb[i]->fix.smem_start);
	}
#endif

	return OK;
}
#endif

#if defined(MODS_HAS_CONSOLE_LOCK)
static atomic_t console_is_locked;

static int esc_mods_lock_console(struct mods_client *client)
{
	if (atomic_cmpxchg(&console_is_locked, 0, 1)) {
		cl_error("console is already locked\n");
		return -EINVAL;
	}

	atomic_set(&client->console_is_locked, 1);
	console_lock();
	return OK;
}

static int esc_mods_unlock_console(struct mods_client *client)
{
	if (!atomic_cmpxchg(&client->console_is_locked, 1, 0)) {
		cl_error("console is not locked by this client\n");
		return -EINVAL;
	}

	console_unlock();
	atomic_set(&console_is_locked, 0);
	return OK;
}

static int esc_mods_suspend_console(struct mods_client *client)
{
	int err = -EINVAL;
#if defined(CONFIG_FB)
	unsigned int i;
#endif

	LOG_ENT();

	if (atomic_cmpxchg(&console_is_locked, 0, 1)) {
		cl_error("cannot suspend console, console is locked\n");
		LOG_EXT();
		return -EINVAL;
	}

#if defined(CONFIG_FB)
	/* tell the os to block fb accesses */
	for (i = 0; i < ARRAY_SIZE(registered_fb); i++) {
		bool suspended = false;

		if (!registered_fb[i])
			continue;

		console_lock();
		if (registered_fb[i]->state != FBINFO_STATE_SUSPENDED) {
			fb_set_suspend(registered_fb[i], 1);
			client->mods_fb_suspended[i] = 1;
			suspended = true;
		}
		console_unlock();
		err = OK;

		if (suspended)
			cl_info("suspended fb%u '%s'\n", i,
				registered_fb[i]->fix.id);
	}
#endif

#if defined(MODS_HAS_CONSOLE_BINDING)
	if (&vga_con == vc_cons[fg_console].d->vc_sw) {
		/* if the current console is the vga console driver,
		 * have the dummy driver take over.
		 */
		console_lock();
		do_take_over_console(&dummy_con, 0, 0, 0);
		console_unlock();
		err = OK;

		cl_info("switched console to dummy\n");
	}
#endif

	if (err)
		cl_warn("no methods to suspend console available\n");

	atomic_set(&console_is_locked, 0);

	LOG_EXT();

	return err;
}

static int esc_mods_resume_console(struct mods_client *client)
{
	return mods_resume_console(client);
}

static int mods_resume_console(struct mods_client *client)
{
	int err = -EINVAL;
#if defined(CONFIG_FB)
	unsigned int i;
#endif

	LOG_ENT();

	if (atomic_cmpxchg(&client->console_is_locked, 1, 0)) {
		cl_warn("console was not properly unlocked\n");
		console_unlock();
	} else if (atomic_cmpxchg(&console_is_locked, 0, 1)) {
		cl_error("cannot resume console, console is locked\n");
		LOG_EXT();
		return -EINVAL;
	}

#if defined(CONFIG_FB)
	for (i = 0; i < ARRAY_SIZE(registered_fb); i++) {
		bool resumed = false;

		if (!registered_fb[i])
			continue;

		console_lock();
		if (client->mods_fb_suspended[i]) {
			fb_set_suspend(registered_fb[i], 0);
			client->mods_fb_suspended[i] = 0;
			resumed = true;
		}
		console_unlock();
		err = OK;

		if (resumed)
			cl_info("resumed fb%u\n", i);
	}
#endif

#if defined(MODS_HAS_CONSOLE_BINDING)
	if (&dummy_con == vc_cons[fg_console].d->vc_sw) {
		/* try to unbind the dummy driver,
		 * the system driver should take over.
		 */
		console_lock();
		do_unbind_con_driver(vc_cons[fg_console].d->vc_sw, 0, 0, 0);
		console_unlock();
		err = OK;

		cl_info("restored vga console\n");
	}
#endif
	atomic_set(&console_is_locked, 0);

	LOG_EXT();

	return err;
}
#endif

static int esc_mods_acquire_access_token(struct mods_client       *client,
					 struct MODS_ACCESS_TOKEN *ptoken)
{
	int err = -EINVAL;

	LOG_ENT();

	if (mods_get_multi_instance()) {
		cl_error(
			"access token ops not supported with multi_instance=1\n");
		LOG_EXT();
		return err;
	}

	get_random_bytes(&ptoken->token, sizeof(ptoken->token));
	err = mods_set_access_token(ptoken->token);
	if (err)
		cl_error("unable to set access token\n");
	else {
		cl_info("set access token %u\n", ptoken->token);
		client->access_token = ptoken->token;
	}

	LOG_EXT();

	return err;
}

static int esc_mods_release_access_token(struct mods_client       *client,
					 struct MODS_ACCESS_TOKEN *ptoken)
{
	int err = -EINVAL;

	LOG_ENT();

	if (mods_get_multi_instance()) {
		cl_error(
			"access token ops not supported with multi_instance=1\n");
		LOG_EXT();
		return err;
	}

	err = mods_set_access_token(MODS_ACCESS_TOKEN_NONE);
	if (err)
		cl_error("unable to clear access token\n");
	else {
		cl_info("released access token %u\n", client->access_token);
		client->access_token = MODS_ACCESS_TOKEN_NONE;
	}

	LOG_EXT();

	return err;
}

static int esc_mods_verify_access_token(struct mods_client       *client,
					struct MODS_ACCESS_TOKEN *ptoken)
{
	int err = -EINVAL;

	LOG_ENT();

	if (ptoken->token == mods_get_access_token()) {
		client->access_token = ptoken->token;
		err = OK;
	} else
		cl_error("invalid access token %u\n", client->access_token);

	LOG_EXT();

	return err;
}

struct mods_file_work {
	struct work_struct work;
	const char        *path;
	const char        *data;
	__u32              data_size;
	int                err;
};

static void sysfs_write_task(struct work_struct *w)
{
	struct mods_file_work *task = container_of(w,
						   struct mods_file_work,
						   work);
	struct file *f;

	LOG_ENT();

	task->err = -EINVAL;

	f = filp_open(task->path, O_WRONLY, 0);
	if (IS_ERR(f))
		task->err = PTR_ERR(f);
	else {
#ifndef MODS_HAS_KERNEL_WRITE
		mm_segment_t old_fs = get_fs();
#endif

		f->f_pos = 0;
#ifdef MODS_HAS_KERNEL_WRITE
		task->err = kernel_write(f,
					 task->data,
					 task->data_size,
					 &f->f_pos);
#else
		set_fs(KERNEL_DS);

		task->err = vfs_write(f,
				      (__force const char __user *)task->data,
				      task->data_size,
				      &f->f_pos);

		set_fs(old_fs);
#endif
		filp_close(f, NULL);
	}

	LOG_EXT();
}

static int create_work_queue(struct mods_client *client)
{
	int err = 0;

	if (unlikely(mutex_lock_interruptible(&client->mtx)))
		return -EINTR;

	if (!client->work_queue) {
		client->work_queue = create_singlethread_workqueue("mods_wq");
		if (!client->work_queue) {
			cl_error("failed to create work queue\n");
			err = -ENOMEM;
		}
	}

	mutex_unlock(&client->mtx);

	return err;
}

static int run_write_task(struct mods_client    *client,
			  struct mods_file_work *task)
{
	int err = create_work_queue(client);

	if (err)
		return err;

	cl_info("write %.*s to %s\n", task->data_size, task->data, task->path);

	INIT_WORK(&task->work, sysfs_write_task);
	queue_work(client->work_queue, &task->work);
	flush_workqueue(client->work_queue);

	if (task->err < 0)
		cl_error("failed to write %.*s to %s\n",
			 task->data_size, task->data, task->path);

	return (task->err > 0) ? 0 : task->err;
}

static int esc_mods_write_sysfs_node(struct mods_client     *client,
				     struct MODS_SYSFS_NODE *pdata)
{
	int                   err;
	struct mods_file_work task;

	LOG_ENT();

	if (pdata->size > MODS_MAX_SYSFS_FILE_SIZE) {
		cl_error("invalid data size %u, max allowed is %u\n",
			 pdata->size, MODS_MAX_SYSFS_FILE_SIZE);
		LOG_EXT();
		return -EINVAL;
	}

	memmove(&pdata->path[5], pdata->path, sizeof(pdata->path) - 5);
	memcpy(pdata->path, "/sys/", 5);
	pdata->path[sizeof(pdata->path) - 1] = 0;

	memset(&task, 0, sizeof(task));
	task.path      = pdata->path;
	task.data      = pdata->contents;
	task.data_size = pdata->size;

	err = run_write_task(client, &task);

	LOG_EXT();
	return err;
}

static int esc_mods_sysctl_write_int(struct mods_client     *client,
				     struct MODS_SYSCTL_INT *pdata)
{
	int                   err;
	struct mods_file_work task;
	char                  data[21];
	int                   data_size;

	LOG_ENT();

	memmove(&pdata->path[10], pdata->path, sizeof(pdata->path)  - 10);
	memcpy(pdata->path, "/proc/sys/", 10);
	pdata->path[sizeof(pdata->path) - 1] = 0;

	data_size = snprintf(data, sizeof(data),
			     "%lld", (long long)pdata->value);

	if (unlikely(data_size < 0)) {
		err = data_size;
		goto error;
	}

	memset(&task, 0, sizeof(task));
	task.path      = pdata->path;
	task.data      = data;
	task.data_size = data_size;

	err = run_write_task(client, &task);

error:
	LOG_EXT();
	return err;
}

#ifdef CONFIG_X86
static int esc_mods_read_msr(struct mods_client *client, struct MODS_MSR *p)
{
	int err = -EINVAL;

	LOG_ENT();

	err = rdmsr_safe_on_cpu(p->cpu_num, p->reg, &p->low, &p->high);
	if (err)
		cl_error("could not read MSR %u\n", p->reg);

	LOG_EXT();
	return err;
}

static int esc_mods_write_msr(struct mods_client *client, struct MODS_MSR *p)
{
	int err = -EINVAL;

	LOG_ENT();

	err = wrmsr_safe_on_cpu(p->cpu_num, p->reg, p->low, p->high);
	if (err)
		cl_error("could not write MSR %u\n", p->reg);

	LOG_EXT();
	return err;
}
#endif

static int esc_mods_get_driver_stats(struct mods_client *client,
				     struct MODS_GET_DRIVER_STATS *p)
{
	LOG_ENT();

	memset(p, 0, sizeof(*p));
	p->version = MODS_DRIVER_STATS_VERSION;
	p->num_allocs = atomic_read(&client->num_allocs);
	p->num_pages = atomic_read(&client->num_pages);

	LOG_EXT();
	return 0;
}

/**************
 * IO control *
 **************/

static long mods_krnl_ioctl(struct file  *fp,
			    unsigned int  cmd,
			    unsigned long i_arg)
{
	int                 err      = 0;
	void               *arg_copy = NULL;
	void        __user *arg      = (void __user *)i_arg;
	struct mods_client *client   = fp->private_data;
	int                 arg_size;
	char                buf[64];

	LOG_ENT();

	if (!validate_client(client)) {
		LOG_EXT();
		return -EINVAL;
	}

	if ((cmd != MODS_ESC_VERIFY_ACCESS_TOKEN) &&
	    (cmd != MODS_ESC_GET_API_VERSION)) {
		err = mods_check_access_token(client);
		if (err) {
			LOG_EXT();
			return err;
		}
	}

	arg_size = _IOC_SIZE(cmd);

	if (arg_size > (int)sizeof(buf)) {
		arg_copy = kzalloc(arg_size, GFP_KERNEL | __GFP_NORETRY);
		if (unlikely(!arg_copy)) {
			LOG_EXT();
			return -ENOMEM;
		}
		atomic_inc(&client->num_allocs);
	} else if (arg_size > 0)
		arg_copy = buf;

	if ((arg_size > 0) && copy_from_user(arg_copy, arg, arg_size)) {
		cl_error("failed to copy ioctl data\n");
		if (arg_size > (int)sizeof(buf)) {
			kfree(arg_copy);
			atomic_dec(&client->num_allocs);
		}
		LOG_EXT();
		return -EFAULT;
	}

#define MODS_IOCTL(code, function, argtype)\
	({\
	do {\
		cl_debug(DEBUG_IOCTL, "ioctl(" #code ")\n");\
		if (arg_size != sizeof(struct argtype)) {\
			err = -EINVAL;\
			cl_error("invalid parameter passed to ioctl " #code\
				 "\n");\
		} else {\
			err = function(client, (struct argtype *)arg_copy);\
			if ((err == OK) && \
			    copy_to_user(arg, arg_copy, arg_size)) {\
				err = -EFAULT;\
				cl_error("copying return value for ioctl " \
					 #code " to user space failed\n");\
			} \
		} \
	} while (0);\
	})

#define MODS_IOCTL_NORETVAL(code, function, argtype)\
	({\
	do {\
		cl_debug(DEBUG_IOCTL, "ioctl(" #code ")\n");\
		if (arg_size != sizeof(struct argtype)) {\
			err = -EINVAL;\
			cl_error("invalid parameter passed to ioctl " #code\
				 "\n");\
		} else {\
			err = function(client, (struct argtype *)arg_copy);\
		} \
	} while (0);\
	})

#define MODS_IOCTL_VOID(code, function)\
	({\
	do {\
		cl_debug(DEBUG_IOCTL, "ioctl(" #code ")\n");\
		if (arg_size != 0) {\
			err = -EINVAL;\
			cl_error("invalid parameter passed to ioctl " #code\
				 "\n");\
		} else {\
			err = function(client);\
		} \
	} while (0);\
	})

	switch (cmd) {

#ifdef CONFIG_PCI
	case MODS_ESC_FIND_PCI_DEVICE:
		MODS_IOCTL(MODS_ESC_FIND_PCI_DEVICE,
			   esc_mods_find_pci_dev, MODS_FIND_PCI_DEVICE);
		break;

	case MODS_ESC_FIND_PCI_DEVICE_2:
		MODS_IOCTL(MODS_ESC_FIND_PCI_DEVICE_2,
			   esc_mods_find_pci_dev_2,
			   MODS_FIND_PCI_DEVICE_2);
		break;

	case MODS_ESC_FIND_PCI_CLASS_CODE:
		MODS_IOCTL(MODS_ESC_FIND_PCI_CLASS_CODE,
			   esc_mods_find_pci_class_code,
			   MODS_FIND_PCI_CLASS_CODE);
		break;

	case MODS_ESC_FIND_PCI_CLASS_CODE_2:
		MODS_IOCTL(MODS_ESC_FIND_PCI_CLASS_CODE_2,
			   esc_mods_find_pci_class_code_2,
			   MODS_FIND_PCI_CLASS_CODE_2);
		break;

	case MODS_ESC_PCI_GET_BAR_INFO:
		MODS_IOCTL(MODS_ESC_PCI_GET_BAR_INFO,
			   esc_mods_pci_get_bar_info,
			   MODS_PCI_GET_BAR_INFO);
		break;

	case MODS_ESC_PCI_GET_BAR_INFO_2:
		MODS_IOCTL(MODS_ESC_PCI_GET_BAR_INFO_2,
			   esc_mods_pci_get_bar_info_2,
			   MODS_PCI_GET_BAR_INFO_2);
		break;

	case MODS_ESC_PCI_GET_IRQ:
		MODS_IOCTL(MODS_ESC_PCI_GET_IRQ,
			   esc_mods_pci_get_irq,
			   MODS_PCI_GET_IRQ);
		break;

	case MODS_ESC_PCI_GET_IRQ_2:
		MODS_IOCTL(MODS_ESC_PCI_GET_IRQ_2,
			   esc_mods_pci_get_irq_2,
			   MODS_PCI_GET_IRQ_2);
		break;

	case MODS_ESC_PCI_READ:
		MODS_IOCTL(MODS_ESC_PCI_READ, esc_mods_pci_read, MODS_PCI_READ);
		break;

	case MODS_ESC_PCI_READ_2:
		MODS_IOCTL(MODS_ESC_PCI_READ_2,
			   esc_mods_pci_read_2, MODS_PCI_READ_2);
		break;

	case MODS_ESC_PCI_WRITE:
		MODS_IOCTL_NORETVAL(MODS_ESC_PCI_WRITE,
				    esc_mods_pci_write, MODS_PCI_WRITE);
		break;

	case MODS_ESC_PCI_WRITE_2:
		MODS_IOCTL_NORETVAL(MODS_ESC_PCI_WRITE_2,
				    esc_mods_pci_write_2,
				    MODS_PCI_WRITE_2);
		break;

	case MODS_ESC_PCI_BUS_RESCAN:
		MODS_IOCTL_NORETVAL(MODS_ESC_PCI_BUS_RESCAN,
				    esc_mods_pci_bus_rescan,
				    MODS_PCI_BUS_RESCAN);
		break;

	case MODS_ESC_PCI_BUS_ADD_DEVICES:
		MODS_IOCTL_NORETVAL(MODS_ESC_PCI_BUS_ADD_DEVICES,
				    esc_mods_pci_bus_add_dev,
				    MODS_PCI_BUS_ADD_DEVICES);
		break;

	case MODS_ESC_PCI_BUS_REMOVE_DEV:
		MODS_IOCTL_NORETVAL(MODS_ESC_PCI_BUS_REMOVE_DEV,
			   esc_mods_pci_bus_remove_dev,
			   MODS_PCI_BUS_REMOVE_DEV);
		break;

	case MODS_ESC_PIO_READ:
		MODS_IOCTL(MODS_ESC_PIO_READ,
			   esc_mods_pio_read, MODS_PIO_READ);
		break;

	case MODS_ESC_PIO_WRITE:
		MODS_IOCTL_NORETVAL(MODS_ESC_PIO_WRITE,
				    esc_mods_pio_write, MODS_PIO_WRITE);
		break;

	case MODS_ESC_DEVICE_NUMA_INFO:
		MODS_IOCTL(MODS_ESC_DEVICE_NUMA_INFO,
			   esc_mods_device_numa_info,
			   MODS_DEVICE_NUMA_INFO);
		break;

	case MODS_ESC_DEVICE_NUMA_INFO_2:
		MODS_IOCTL(MODS_ESC_DEVICE_NUMA_INFO_2,
			   esc_mods_device_numa_info_2,
			   MODS_DEVICE_NUMA_INFO_2);
		break;

	case MODS_ESC_DEVICE_NUMA_INFO_3:
		MODS_IOCTL(MODS_ESC_DEVICE_NUMA_INFO_3,
			   esc_mods_device_numa_info_3,
			   MODS_DEVICE_NUMA_INFO_3);
		break;

	case MODS_ESC_GET_IOMMU_STATE:
		MODS_IOCTL(MODS_ESC_GET_IOMMU_STATE,
			   esc_mods_get_iommu_state,
			   MODS_GET_IOMMU_STATE);
		break;

	case MODS_ESC_GET_IOMMU_STATE_2:
		MODS_IOCTL(MODS_ESC_GET_IOMMU_STATE_2,
			   esc_mods_get_iommu_state_2,
			   MODS_GET_IOMMU_STATE);
		break;

	case MODS_ESC_PCI_SET_DMA_MASK:
		MODS_IOCTL(MODS_ESC_PCI_SET_DMA_MASK,
			   esc_mods_pci_set_dma_mask,
			   MODS_PCI_DMA_MASK);
		break;

	case MODS_ESC_PCI_RESET_FUNCTION:
		MODS_IOCTL(MODS_ESC_PCI_RESET_FUNCTION,
			   esc_mods_pci_reset_function,
			   mods_pci_dev_2);
		break;
#endif

	case MODS_ESC_ALLOC_PAGES:
		MODS_IOCTL(MODS_ESC_ALLOC_PAGES,
			   esc_mods_alloc_pages, MODS_ALLOC_PAGES);
		break;

	case MODS_ESC_DEVICE_ALLOC_PAGES:
		MODS_IOCTL(MODS_ESC_DEVICE_ALLOC_PAGES,
			   esc_mods_device_alloc_pages,
			   MODS_DEVICE_ALLOC_PAGES);
		break;

	case MODS_ESC_DEVICE_ALLOC_PAGES_2:
		MODS_IOCTL(MODS_ESC_DEVICE_ALLOC_PAGES_2,
			   esc_mods_device_alloc_pages_2,
			   MODS_DEVICE_ALLOC_PAGES_2);
		break;

	case MODS_ESC_ALLOC_PAGES_2:
		MODS_IOCTL(MODS_ESC_ALLOC_PAGES_2,
			   esc_mods_alloc_pages_2,
			   MODS_ALLOC_PAGES_2);
		break;

	case MODS_ESC_FREE_PAGES:
		MODS_IOCTL(MODS_ESC_FREE_PAGES,
			   esc_mods_free_pages, MODS_FREE_PAGES);
		break;

	case MODS_ESC_MERGE_PAGES:
		MODS_IOCTL(MODS_ESC_MERGE_PAGES,
			   esc_mods_merge_pages, MODS_MERGE_PAGES);
		break;

	case MODS_ESC_GET_PHYSICAL_ADDRESS:
		MODS_IOCTL(MODS_ESC_GET_PHYSICAL_ADDRESS,
			   esc_mods_get_phys_addr,
			   MODS_GET_PHYSICAL_ADDRESS);
		break;

	case MODS_ESC_GET_PHYSICAL_ADDRESS_2:
		MODS_IOCTL(MODS_ESC_GET_PHYSICAL_ADDRESS_2,
			   esc_mods_get_phys_addr_2,
			   MODS_GET_PHYSICAL_ADDRESS_3);
		break;

	case MODS_ESC_GET_MAPPED_PHYSICAL_ADDRESS:
		MODS_IOCTL(MODS_ESC_GET_MAPPED_PHYSICAL_ADDRESS,
			   esc_mods_get_mapped_phys_addr,
			   MODS_GET_PHYSICAL_ADDRESS);
		break;

	case MODS_ESC_GET_MAPPED_PHYSICAL_ADDRESS_2:
		MODS_IOCTL(MODS_ESC_GET_MAPPED_PHYSICAL_ADDRESS_2,
			   esc_mods_get_mapped_phys_addr_2,
			   MODS_GET_PHYSICAL_ADDRESS_2);
		break;

	case MODS_ESC_GET_MAPPED_PHYSICAL_ADDRESS_3:
		MODS_IOCTL(MODS_ESC_GET_MAPPED_PHYSICAL_ADDRESS_3,
			   esc_mods_get_mapped_phys_addr_3,
			   MODS_GET_PHYSICAL_ADDRESS_3);
		break;

	case MODS_ESC_SET_MEMORY_TYPE:
		MODS_IOCTL_NORETVAL(MODS_ESC_SET_MEMORY_TYPE,
				    esc_mods_set_mem_type,
				    MODS_MEMORY_TYPE);
		break;

	case MODS_ESC_VIRTUAL_TO_PHYSICAL:
		MODS_IOCTL(MODS_ESC_VIRTUAL_TO_PHYSICAL,
			   esc_mods_virtual_to_phys,
			   MODS_VIRTUAL_TO_PHYSICAL);
		break;

	case MODS_ESC_PHYSICAL_TO_VIRTUAL:
		MODS_IOCTL(MODS_ESC_PHYSICAL_TO_VIRTUAL,
			   esc_mods_phys_to_virtual, MODS_PHYSICAL_TO_VIRTUAL);
		break;

#if defined(CONFIG_PPC64)
	case MODS_ESC_PCI_HOT_RESET:
		MODS_IOCTL_NORETVAL(MODS_ESC_PCI_HOT_RESET,
				    esc_mods_pci_hot_reset,
				    MODS_PCI_HOT_RESET);
		break;

	case MODS_ESC_SET_PPC_TCE_BYPASS:
		MODS_IOCTL(MODS_ESC_SET_PPC_TCE_BYPASS,
			   esc_mods_set_ppc_tce_bypass,
			   MODS_SET_PPC_TCE_BYPASS);
		break;

	case MODS_ESC_GET_ATS_ADDRESS_RANGE:
		MODS_IOCTL(MODS_ESC_GET_ATS_ADDRESS_RANGE,
			   esc_mods_get_ats_address_range,
			   MODS_GET_ATS_ADDRESS_RANGE);
		break;

	case MODS_ESC_SET_NVLINK_SYSMEM_TRAINED:
		MODS_IOCTL(MODS_ESC_SET_NVLINK_SYSMEM_TRAINED,
			   esc_mods_set_nvlink_sysmem_trained,
			   MODS_SET_NVLINK_SYSMEM_TRAINED);
		break;

	case MODS_ESC_GET_NVLINK_LINE_RATE:
		MODS_IOCTL(MODS_ESC_GET_NVLINK_LINE_RATE,
			   esc_mods_get_nvlink_line_rate,
			   MODS_GET_NVLINK_LINE_RATE);
		break;
#endif

#ifdef CONFIG_PCI
	case MODS_ESC_DMA_MAP_MEMORY:
		MODS_IOCTL(MODS_ESC_DMA_MAP_MEMORY,
			   esc_mods_dma_map_memory,
			   MODS_DMA_MAP_MEMORY);
		break;

	case MODS_ESC_DMA_UNMAP_MEMORY:
		MODS_IOCTL(MODS_ESC_DMA_UNMAP_MEMORY,
			   esc_mods_dma_unmap_memory,
			   MODS_DMA_MAP_MEMORY);
		break;
#endif

	case MODS_ESC_IRQ_REGISTER:
	case MODS_ESC_MSI_REGISTER:
		err = -EINVAL;
		break;

#if defined(MODS_HAS_TEGRA) && defined(CONFIG_OF) && defined(CONFIG_OF_IRQ)
	case MODS_ESC_MAP_INTERRUPT:
		MODS_IOCTL(MODS_ESC_MAP_INTERRUPT,
				esc_mods_map_irq, MODS_DT_INFO);
		break;

	case MODS_ESC_MAP_GPIO:
		MODS_IOCTL(MODS_ESC_MAP_GPIO,
		esc_mods_map_irq_to_gpio, MODS_GPIO_INFO);
		break;
#endif

	case MODS_ESC_REGISTER_IRQ:
		MODS_IOCTL_NORETVAL(MODS_ESC_REGISTER_IRQ,
				esc_mods_register_irq, MODS_REGISTER_IRQ);
		break;

	case MODS_ESC_REGISTER_IRQ_2:
		MODS_IOCTL_NORETVAL(MODS_ESC_REGISTER_IRQ_2,
				esc_mods_register_irq_2, MODS_REGISTER_IRQ_2);
		break;

	case MODS_ESC_REGISTER_IRQ_3:
		MODS_IOCTL_NORETVAL(MODS_ESC_REGISTER_IRQ_3,
				esc_mods_register_irq_3, MODS_REGISTER_IRQ_3);
		break;

	case MODS_ESC_UNREGISTER_IRQ:
		MODS_IOCTL_NORETVAL(MODS_ESC_UNREGISTER_IRQ,
				    esc_mods_unregister_irq, MODS_REGISTER_IRQ);
		break;

	case MODS_ESC_UNREGISTER_IRQ_2:
		MODS_IOCTL_NORETVAL(MODS_ESC_UNREGISTER_IRQ_2,
				    esc_mods_unregister_irq_2,
				    MODS_REGISTER_IRQ_2);
		break;

	case MODS_ESC_QUERY_IRQ:
		MODS_IOCTL(MODS_ESC_QUERY_IRQ,
			   esc_mods_query_irq, MODS_QUERY_IRQ);
		break;

	case MODS_ESC_QUERY_IRQ_2:
		MODS_IOCTL(MODS_ESC_QUERY_IRQ_2,
			   esc_mods_query_irq_2, MODS_QUERY_IRQ_2);
		break;

	case MODS_ESC_IRQ_HANDLED:
		MODS_IOCTL_NORETVAL(MODS_ESC_IRQ_HANDLED,
				    esc_mods_irq_handled, MODS_REGISTER_IRQ);
		break;

	case MODS_ESC_IRQ_HANDLED_2:
		MODS_IOCTL_NORETVAL(MODS_ESC_IRQ_HANDLED_2,
				    esc_mods_irq_handled_2,
				    MODS_REGISTER_IRQ_2);
		break;

#ifdef CONFIG_ACPI
	case MODS_ESC_EVAL_ACPI_METHOD:
		MODS_IOCTL(MODS_ESC_EVAL_ACPI_METHOD,
			   esc_mods_eval_acpi_method, MODS_EVAL_ACPI_METHOD);
		break;

	case MODS_ESC_EVAL_DEV_ACPI_METHOD:
		MODS_IOCTL(MODS_ESC_EVAL_DEV_ACPI_METHOD,
			   esc_mods_eval_dev_acpi_method,
			   MODS_EVAL_DEV_ACPI_METHOD);
		break;

	case MODS_ESC_EVAL_DEV_ACPI_METHOD_2:
		MODS_IOCTL(MODS_ESC_EVAL_DEV_ACPI_METHOD_2,
			   esc_mods_eval_dev_acpi_method_2,
			   MODS_EVAL_DEV_ACPI_METHOD_2);
		break;

	case MODS_ESC_EVAL_DEV_ACPI_METHOD_3:
		MODS_IOCTL(MODS_ESC_EVAL_DEV_ACPI_METHOD_3,
			   esc_mods_eval_dev_acpi_method_3,
			   MODS_EVAL_DEV_ACPI_METHOD_3);
		break;

	case MODS_ESC_ACPI_GET_DDC:
		MODS_IOCTL(MODS_ESC_ACPI_GET_DDC,
			   esc_mods_acpi_get_ddc, MODS_ACPI_GET_DDC);
		break;

	case MODS_ESC_ACPI_GET_DDC_2:
		MODS_IOCTL(MODS_ESC_ACPI_GET_DDC_2,
			   esc_mods_acpi_get_ddc_2, MODS_ACPI_GET_DDC_2);
		break;

	case MODS_ESC_GET_ACPI_DEV_CHILDREN:
		MODS_IOCTL(MODS_ESC_GET_ACPI_DEV_CHILDREN,
			   esc_mods_get_acpi_dev_children, MODS_GET_ACPI_DEV_CHILDREN);
		break;
#else
	case MODS_ESC_EVAL_ACPI_METHOD:
		/* fallthrough */
	case MODS_ESC_EVAL_DEV_ACPI_METHOD:
		/* fallthrough */
	case MODS_ESC_EVAL_DEV_ACPI_METHOD_2:
		/* fallthrough */
	case MODS_ESC_EVAL_DEV_ACPI_METHOD_3:
		/* fallthrough */
	case MODS_ESC_ACPI_GET_DDC:
		/* fallthrough */
	case MODS_ESC_ACPI_GET_DDC_2:
		/* fallthrough */
	case MODS_ESC_GET_ACPI_DEV_CHILDREN:
		/* Silent failure to avoid clogging kernel log */
		err = -EINVAL;
		break;
#endif
	case MODS_ESC_GET_API_VERSION:
		MODS_IOCTL(MODS_ESC_GET_API_VERSION,
			   esc_mods_get_api_version, MODS_GET_VERSION);
		break;

	case MODS_ESC_GET_KERNEL_VERSION:
		MODS_IOCTL(MODS_ESC_GET_KERNEL_VERSION,
			   esc_mods_get_kernel_version, MODS_GET_VERSION);
		break;

#if defined(MODS_HAS_TEGRA) && defined(CONFIG_COMMON_CLK) && \
	defined(CONFIG_OF_RESOLVE) && defined(CONFIG_OF_DYNAMIC)
	case MODS_ESC_GET_CLOCK_HANDLE:
		MODS_IOCTL(MODS_ESC_GET_CLOCK_HANDLE,
			   esc_mods_get_clock_handle, MODS_GET_CLOCK_HANDLE);
		break;

	case MODS_ESC_SET_CLOCK_RATE:
		MODS_IOCTL_NORETVAL(MODS_ESC_SET_CLOCK_RATE,
				    esc_mods_set_clock_rate, MODS_CLOCK_RATE);
		break;

	case MODS_ESC_GET_CLOCK_RATE:
		MODS_IOCTL(MODS_ESC_GET_CLOCK_RATE,
			   esc_mods_get_clock_rate, MODS_CLOCK_RATE);
		break;

	case MODS_ESC_GET_CLOCK_MAX_RATE:
		MODS_IOCTL(MODS_ESC_GET_CLOCK_MAX_RATE,
			   esc_mods_get_clock_max_rate, MODS_CLOCK_RATE);
		break;

	case MODS_ESC_SET_CLOCK_MAX_RATE:
		MODS_IOCTL_NORETVAL(MODS_ESC_SET_CLOCK_MAX_RATE,
				    esc_mods_set_clock_max_rate,
				    MODS_CLOCK_RATE);
		break;

	case MODS_ESC_SET_CLOCK_PARENT:
		MODS_IOCTL_NORETVAL(MODS_ESC_SET_CLOCK_PARENT,
				    esc_mods_set_clock_parent,
				    MODS_CLOCK_PARENT);
		break;

	case MODS_ESC_GET_CLOCK_PARENT:
		MODS_IOCTL(MODS_ESC_GET_CLOCK_PARENT,
			   esc_mods_get_clock_parent, MODS_CLOCK_PARENT);
		break;

	case MODS_ESC_ENABLE_CLOCK:
		MODS_IOCTL_NORETVAL(MODS_ESC_ENABLE_CLOCK,
				    esc_mods_enable_clock, MODS_CLOCK_HANDLE);
		break;

	case MODS_ESC_DISABLE_CLOCK:
		MODS_IOCTL_NORETVAL(MODS_ESC_DISABLE_CLOCK,
				    esc_mods_disable_clock, MODS_CLOCK_HANDLE);
		break;

	case MODS_ESC_IS_CLOCK_ENABLED:
		MODS_IOCTL(MODS_ESC_IS_CLOCK_ENABLED,
			   esc_mods_is_clock_enabled, MODS_CLOCK_ENABLED);
		break;

	case MODS_ESC_CLOCK_RESET_ASSERT:
		MODS_IOCTL_NORETVAL(MODS_ESC_CLOCK_RESET_ASSERT,
				    esc_mods_clock_reset_assert,
				    MODS_CLOCK_HANDLE);
		break;

	case MODS_ESC_CLOCK_RESET_DEASSERT:
		MODS_IOCTL_NORETVAL(MODS_ESC_CLOCK_RESET_DEASSERT,
				    esc_mods_clock_reset_deassert,
				    MODS_CLOCK_HANDLE);
		break;

	case MODS_ESC_RESET_ASSERT:
		MODS_IOCTL_NORETVAL(MODS_ESC_RESET_ASSERT,
				    esc_mods_reset_assert,
				    MODS_RESET_HANDLE);
		break;

	case MODS_ESC_GET_RESET_HANDLE:
		MODS_IOCTL(MODS_ESC_GET_RESET_HANDLE,
			   esc_mods_get_rst_handle,
			   MODS_GET_RESET_HANDLE);
		break;
#endif
#if defined(MODS_HAS_TEGRA)
	case MODS_ESC_BPMP_SET_PCIE_STATE:
		MODS_IOCTL(MODS_ESC_BPMP_SET_PCIE_STATE,
			   esc_mods_bpmp_set_pcie_state,
			   MODS_SET_PCIE_STATE);
		break;

	case MODS_ESC_BPMP_INIT_PCIE_EP_PLL:
		MODS_IOCTL(MODS_ESC_BPMP_INIT_PCIE_EP_PLL,
			   esc_mods_bpmp_init_pcie_ep_pll,
			   MODS_INIT_PCIE_EP_PLL);
		break;
	case MODS_ESC_DMA_ALLOC_COHERENT:
		MODS_IOCTL(MODS_ESC_DMA_ALLOC_COHERENT,
			   esc_mods_dma_alloc_coherent,
			   MODS_DMA_COHERENT_MEM_HANDLE);
		break;
	case MODS_ESC_DMA_FREE_COHERENT:
		MODS_IOCTL(MODS_ESC_DMA_FREE_COHERENT,
			   esc_mods_dma_free_coherent,
			   MODS_DMA_COHERENT_MEM_HANDLE);
		break;
	case MODS_ESC_DMA_COPY_TO_USER:
		MODS_IOCTL(MODS_ESC_DMA_COPY_TO_USER,
			   esc_mods_dma_copy_to_user,
			   MODS_DMA_COPY_TO_USER);
		break;
	case MODS_ESC_IOMMU_DMA_MAP_MEMORY:
		MODS_IOCTL(MODS_ESC_IOMMU_DMA_MAP_MEMORY,
			   esc_mods_iommu_dma_map_memory,
			   MODS_IOMMU_DMA_MAP_MEMORY);
		break;

	case MODS_ESC_IOMMU_DMA_UNMAP_MEMORY:
		MODS_IOCTL(MODS_ESC_IOMMU_DMA_UNMAP_MEMORY,
			   esc_mods_iommu_dma_unmap_memory,
			   MODS_IOMMU_DMA_MAP_MEMORY);
		break;
#if defined(CONFIG_DMA_ENGINE)
	case MODS_ESC_DMA_REQUEST_HANDLE:
		MODS_IOCTL(MODS_ESC_DMA_REQUEST_HANDLE,
			   esc_mods_dma_request_channel,
			   MODS_DMA_HANDLE);
		break;
	case MODS_ESC_DMA_RELEASE_HANDLE:
		MODS_IOCTL_NORETVAL(MODS_ESC_DMA_RELEASE_HANDLE,
			   esc_mods_dma_release_channel,
			   MODS_DMA_HANDLE);
		break;
	case MODS_ESC_DMA_ISSUE_PENDING:
		MODS_IOCTL_NORETVAL(MODS_ESC_DMA_ISSUE_PENDING,
				    esc_mods_dma_async_issue_pending,
				    MODS_DMA_HANDLE);
		break;
	case MODS_ESC_DMA_SET_CONFIG:
		MODS_IOCTL_NORETVAL(MODS_ESC_DMA_SET_CONFIG,
				    esc_mods_dma_set_config,
				    MODS_DMA_CHANNEL_CONFIG);
		break;
	case MODS_ESC_DMA_TX_SUBMIT:
		MODS_IOCTL(MODS_ESC_DMA_TX_SUBMIT,
			   esc_mods_dma_submit_request,
			   MODS_DMA_TX_DESC);
		break;
	case MODS_ESC_DMA_TX_WAIT:
		MODS_IOCTL(MODS_MODS_ESC_DMA_TX_WAIT,
			   esc_mods_dma_wait,
			   MODS_DMA_WAIT_DESC);
		break;
#endif
#ifdef CONFIG_TEGRA_DC
	case MODS_ESC_TEGRA_DC_CONFIG_POSSIBLE:
		MODS_IOCTL(MODS_ESC_TEGRA_DC_CONFIG_POSSIBLE,
				   esc_mods_tegra_dc_config_possible,
				   MODS_TEGRA_DC_CONFIG_POSSIBLE);
		break;
#endif
#if defined(MODS_HAS_TEGRA) && defined(CONFIG_NET)
	case MODS_ESC_NET_FORCE_LINK:
		MODS_IOCTL(MODS_ESC_NET_FORCE_LINK,
			   esc_mods_net_force_link, MODS_NET_DEVICE_NAME);
		break;
#endif
#endif
#ifdef CONFIG_ARM
	case MODS_ESC_MEMORY_BARRIER:
		MODS_IOCTL_VOID(MODS_ESC_MEMORY_BARRIER,
				esc_mods_memory_barrier);
		break;
#endif
#ifdef CONFIG_ARM64
	case MODS_ESC_FLUSH_CPU_CACHE_RANGE:
		MODS_IOCTL_NORETVAL(MODS_ESC_FLUSH_CPU_CACHE_RANGE,
				    esc_mods_flush_cpu_cache_range,
				    MODS_FLUSH_CPU_CACHE_RANGE);
		break;
#endif
#ifdef MODS_HAS_TEGRA
	case MODS_ESC_DMABUF_GET_PHYSICAL_ADDRESS:
		MODS_IOCTL(MODS_ESC_DMABUF_GET_PHYSICAL_ADDRESS,
			   esc_mods_dmabuf_get_phys_addr,
			   MODS_DMABUF_GET_PHYSICAL_ADDRESS);
		break;
#endif
#ifdef CONFIG_TEGRA_NVADSP
	case MODS_ESC_ADSP_LOAD:
		MODS_IOCTL_VOID(MODS_ESC_ADSP_LOAD,
				esc_mods_adsp_load);
		break;

	case MODS_ESC_ADSP_START:
		MODS_IOCTL_VOID(MODS_ESC_ADSP_START,
				esc_mods_adsp_start);
		break;

	case MODS_ESC_ADSP_STOP:
		MODS_IOCTL_VOID(MODS_ESC_ADSP_STOP,
				esc_mods_adsp_stop);
		break;

	case MODS_ESC_ADSP_RUN_APP:
		MODS_IOCTL_NORETVAL(MODS_ESC_ADSP_RUN_APP,
				    esc_mods_adsp_run_app,
				    MODS_ADSP_RUN_APP_INFO);
		break;
#endif

#ifdef CONFIG_X86
	case MODS_ESC_GET_SCREEN_INFO:
		MODS_IOCTL(MODS_ESC_GET_SCREEN_INFO,
			   esc_mods_get_screen_info, MODS_SCREEN_INFO);
		break;
	case MODS_ESC_GET_SCREEN_INFO_2:
		MODS_IOCTL(MODS_ESC_GET_SCREEN_INFO_2,
			   esc_mods_get_screen_info_2, MODS_SCREEN_INFO_2);
		break;
#endif

#if defined(MODS_HAS_CONSOLE_LOCK)
	case MODS_ESC_LOCK_CONSOLE:
		MODS_IOCTL_VOID(MODS_ESC_LOCK_CONSOLE,
			   esc_mods_lock_console);
		break;
	case MODS_ESC_UNLOCK_CONSOLE:
		MODS_IOCTL_VOID(MODS_ESC_UNLOCK_CONSOLE,
			   esc_mods_unlock_console);
		break;
	case MODS_ESC_SUSPEND_CONSOLE:
		MODS_IOCTL_VOID(MODS_ESC_SUSPEND_CONSOLE,
			   esc_mods_suspend_console);
		break;
	case MODS_ESC_RESUME_CONSOLE:
		MODS_IOCTL_VOID(MODS_ESC_RESUME_CONSOLE,
			   esc_mods_resume_console);
		break;
#endif

#if defined(MODS_HAS_TEGRA)
	case MODS_ESC_TEGRA_PROD_IS_SUPPORTED:
		MODS_IOCTL(MODS_ESC_TEGRA_PROD_IS_SUPPORTED,
			   esc_mods_tegra_prod_is_supported,
			   MODS_TEGRA_PROD_IS_SUPPORTED);
		break;

	case MODS_ESC_TEGRA_PROD_SET_PROD_ALL:
		MODS_IOCTL_NORETVAL(MODS_ESC_TEGRA_PROD_SET_PROD_ALL,
				    esc_mods_tegra_prod_set_prod_all,
				    MODS_TEGRA_PROD_SET_TUPLE);
		break;

	case MODS_ESC_TEGRA_PROD_SET_PROD_BOOT:
		MODS_IOCTL_NORETVAL(MODS_ESC_TEGRA_PROD_SET_PROD_BOOT,
				    esc_mods_tegra_prod_set_prod_boot,
				    MODS_TEGRA_PROD_SET_TUPLE);
		break;

	case MODS_ESC_TEGRA_PROD_SET_PROD_BY_NAME:
		MODS_IOCTL_NORETVAL(MODS_ESC_TEGRA_PROD_SET_PROD_BY_NAME,
				    esc_mods_tegra_prod_set_prod_by_name,
				    MODS_TEGRA_PROD_SET_TUPLE);
		break;

	case MODS_ESC_TEGRA_PROD_SET_PROD_EXACT:
		MODS_IOCTL_NORETVAL(MODS_ESC_TEGRA_PROD_SET_PROD_EXACT,
				    esc_mods_tegra_prod_set_prod_exact,
				    MODS_TEGRA_PROD_SET_TUPLE);
		break;

	case MODS_ESC_TEGRA_PROD_ITERATE_DT:
		MODS_IOCTL(MODS_ESC_TEGRA_PROD_ITERATE_DT,
			   esc_mods_tegra_prod_iterate_dt,
			   MODS_TEGRA_PROD_ITERATOR);
		break;

#ifdef CONFIG_TRUSTY
	case MODS_ESC_SEND_TZ_MSG:
		MODS_IOCTL(MODS_ESC_SEND_TZ_MSG,
			esc_mods_send_trustzone_msg, MODS_TZ_PARAMS);
		break;
#endif

	case MODS_ESC_OIST_STATUS:
		MODS_IOCTL(MODS_ESC_OIST_STATUS,
			   esc_mods_oist_status, MODS_TEGRA_OIST_STATUS);
		break;
#endif

	case MODS_ESC_ACQUIRE_ACCESS_TOKEN:
		MODS_IOCTL(MODS_ESC_ACQUIRE_ACCESS_TOKEN,
			   esc_mods_acquire_access_token,
			   MODS_ACCESS_TOKEN);
		break;

	case MODS_ESC_RELEASE_ACCESS_TOKEN:
		MODS_IOCTL_NORETVAL(MODS_ESC_RELEASE_ACCESS_TOKEN,
				    esc_mods_release_access_token,
				    MODS_ACCESS_TOKEN);
		break;

	case MODS_ESC_VERIFY_ACCESS_TOKEN:
		MODS_IOCTL_NORETVAL(MODS_ESC_VERIFY_ACCESS_TOKEN,
				    esc_mods_verify_access_token,
				    MODS_ACCESS_TOKEN);
		break;

	case MODS_ESC_WRITE_SYSFS_NODE:
		MODS_IOCTL_NORETVAL(MODS_ESC_WRITE_SYSFS_NODE,
				    esc_mods_write_sysfs_node,
				    MODS_SYSFS_NODE);
		break;

	case MODS_ESC_SYSCTL_WRITE_INT:
		MODS_IOCTL_NORETVAL(MODS_ESC_SYSCTL_WRITE_INT,
				    esc_mods_sysctl_write_int,
				    MODS_SYSCTL_INT);
		break;

	case MODS_ESC_REGISTER_IRQ_4:
		MODS_IOCTL_NORETVAL(MODS_ESC_REGISTER_IRQ_4,
				esc_mods_register_irq_4, MODS_REGISTER_IRQ_4);
		break;

	case MODS_ESC_QUERY_IRQ_3:
		MODS_IOCTL(MODS_ESC_QUERY_IRQ_3,
			   esc_mods_query_irq_3, MODS_QUERY_IRQ_3);
		break;

#if defined(CONFIG_PCI) && defined(MODS_HAS_SRIOV)
	case MODS_ESC_SET_NUM_VF:
		MODS_IOCTL_NORETVAL(MODS_ESC_SET_NUM_VF,
			   esc_mods_set_num_vf, MODS_SET_NUM_VF);
		break;

	case MODS_ESC_SET_TOTAL_VF:
		MODS_IOCTL_NORETVAL(MODS_ESC_SET_TOTAL_VF,
			   esc_mods_set_total_vf, MODS_SET_NUM_VF);
		break;
#endif

#ifdef CONFIG_X86
	case MODS_ESC_READ_MSR:
		MODS_IOCTL(MODS_ESC_READ_MSR,
			   esc_mods_read_msr, MODS_MSR);
		break;

	case MODS_ESC_WRITE_MSR:
		MODS_IOCTL_NORETVAL(MODS_ESC_WRITE_MSR,
			   esc_mods_write_msr, MODS_MSR);
		break;
#endif

	case MODS_ESC_MODS_GET_DRIVER_STATS:
		MODS_IOCTL(MODS_ESC_MODS_GET_DRIVER_STATS,
			   esc_mods_get_driver_stats, MODS_GET_DRIVER_STATS);
		break;

	default:
		cl_error(
			"unrecognized ioctl 0x%x, dir %u, type 0x%x, nr %u, size 0x%x\n",
			cmd,
			_IOC_DIR(cmd),
			_IOC_TYPE(cmd),
			_IOC_NR(cmd),
			_IOC_SIZE(cmd));
		err = -EINVAL;
		break;
	}

	if (arg_size > (int)sizeof(buf)) {
		kfree(arg_copy);
		atomic_dec(&client->num_allocs);
	}

	LOG_EXT();
	return err;
}
