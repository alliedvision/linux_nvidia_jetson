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

#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <linux/pci_regs.h>
#if defined(MODS_HAS_TEGRA) && defined(CONFIG_OF) && defined(CONFIG_OF_IRQ)
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#endif

#define PCI_VENDOR_ID_NVIDIA 0x10de
#define INDEX_IRQSTAT(irq)	(irq / BITS_NUM)
#define POS_IRQSTAT(irq)	 (irq & (BITS_NUM - 1))

/* MSI */
#define PCI_MSI_MASK_BIT	16
#define MSI_CONTROL_REG(base)		(base + PCI_MSI_FLAGS)
#define IS_64BIT_ADDRESS(control)	(!!(control & PCI_MSI_FLAGS_64BIT))
#define MSI_DATA_REG(base, is64bit) \
	((is64bit == 1) ? base + PCI_MSI_DATA_64 : base + PCI_MSI_DATA_32)
#define TOP_TKE_TKEIE_WDT_MASK(i)	(1 << (16 + 4 * (i)))
#define TOP_TKE_TKEIE(i)		(0x100 + 4 * (i))

/*********************
 * PRIVATE FUNCTIONS *
 *********************/
static struct mods_priv mp;

struct mutex *mods_get_irq_mutex(void)
{
	return &mp.mtx;
}

#ifdef CONFIG_PCI
int mods_enable_device(struct mods_client   *client,
		       struct pci_dev       *dev,
		       struct en_dev_entry **dev_entry)
{
	int                  err   = OK;
	struct en_dev_entry *dpriv = client->enabled_devices;

	WARN_ON(!mutex_is_locked(&mp.mtx));

	dpriv = pci_get_drvdata(dev);
	if (dpriv) {
		if (dpriv->client_id == client->client_id) {
			if (dev_entry)
				*dev_entry = dpriv;
			return OK;
		}

		cl_error(
			"invalid client for dev %04x:%02x:%02x.%x, expected %u\n",
			pci_domain_nr(dev->bus),
			dev->bus->number,
			PCI_SLOT(dev->devfn),
			PCI_FUNC(dev->devfn),
			dpriv->client_id);
		return -EBUSY;
	}

	dpriv = kzalloc(sizeof(*dpriv), GFP_KERNEL | __GFP_NORETRY);
	if (unlikely(!dpriv))
		return -ENOMEM;
	atomic_inc(&client->num_allocs);

	err = pci_enable_device(dev);
	if (unlikely(err)) {
		cl_error("failed to enable dev %04x:%02x:%02x.%x\n",
			 pci_domain_nr(dev->bus),
			 dev->bus->number,
			 PCI_SLOT(dev->devfn),
			 PCI_FUNC(dev->devfn));
		kfree(dpriv);
		atomic_dec(&client->num_allocs);
		return err;
	}

	cl_info("enabled dev %04x:%02x:%02x.%x\n",
		pci_domain_nr(dev->bus),
		dev->bus->number,
		PCI_SLOT(dev->devfn),
		PCI_FUNC(dev->devfn));

	dpriv->client_id        = client->client_id;
	dpriv->dev              = pci_dev_get(dev);
	dpriv->next             = client->enabled_devices;
	client->enabled_devices = dpriv;
	pci_set_drvdata(dev, dpriv);

	if (dev_entry)
		*dev_entry = dpriv;
	return OK;
}

void mods_disable_device(struct mods_client *client,
			 struct pci_dev     *dev)
{
	struct en_dev_entry *dpriv = pci_get_drvdata(dev);

	WARN_ON(!mutex_is_locked(&mp.mtx));

#ifdef MODS_HAS_SRIOV
	if (dpriv && dpriv->num_vfs)
		pci_disable_sriov(dev);
#endif

	if (dpriv) {
		pci_set_drvdata(dev, NULL);
		pci_dev_put(dev);
	}

	pci_disable_device(dev);

	cl_info("disabled dev %04x:%02x:%02x.%x\n",
		pci_domain_nr(dev->bus),
		dev->bus->number,
		PCI_SLOT(dev->devfn),
		PCI_FUNC(dev->devfn));
}
#endif

static unsigned int get_cur_time(void)
{
	/* This is not very precise, sched_clock() would be better */
	return jiffies_to_usecs(jiffies);
}

static u64 irq_reg_read(const struct irq_mask_info *m, void __iomem *reg)
{
	if (m->mask_type == MODS_MASK_TYPE_IRQ_DISABLE64)
		return readq(reg);
	else
		return readl(reg);
}

static void irq_reg_write(const struct irq_mask_info *m,
			  u64                         value,
			  void               __iomem *reg)
{
	if (m->mask_type == MODS_MASK_TYPE_IRQ_DISABLE64)
		writeq(value, reg);
	else
		writel((u32)value, reg);
}

static u64 read_irq_state(const struct irq_mask_info *m)
{
	return irq_reg_read(m, m->dev_irq_state);
}

static u64 read_irq_mask(const struct irq_mask_info *m)
{
	return irq_reg_read(m, m->dev_irq_mask_reg);
}

static void write_irq_disable(u64 value, const struct irq_mask_info *m)
{
	irq_reg_write(m, value, m->dev_irq_disable_reg);
}

static int mods_check_interrupt(struct dev_irq_map *t)
{
	int ii    = 0;
	int valid = 0;

	/* For MSI - we always treat it as pending (must rearm later). */
	/* For non-GPU devices - we can't tell. */
	if (t->mask_info_cnt == 0)
		return true;

	for (ii = 0; ii < t->mask_info_cnt; ii++) {
		const struct irq_mask_info *const m = &t->mask_info[ii];

		if (!m->dev_irq_state || !m->dev_irq_mask_reg)
			continue;

		/* GPU device */
		valid |= (read_irq_state(m) && read_irq_mask(m)) != 0;
	}

	return valid;
}

static void mods_disable_interrupts(struct dev_irq_map *t)
{
	u32 ii = 0;

	for (ii = 0; ii < t->mask_info_cnt; ii++) {
		const struct irq_mask_info *const m = &t->mask_info[ii];
		u64 cur_mask = 0;

		if (!m->dev_irq_disable_reg)
			continue;

		if (m->irq_and_mask == 0) {
			write_irq_disable(m->irq_or_mask, m);
			continue;
		}

		cur_mask = read_irq_mask(m);
		cur_mask &= m->irq_and_mask;
		cur_mask |= m->irq_or_mask;
		write_irq_disable(cur_mask, m);
	}

	if ((ii == 0) && t->type == MODS_IRQ_TYPE_CPU) {
		mods_debug_printk(DEBUG_ISR,
				  "disable_irq_nosync %u",
				  t->apic_irq);
		disable_irq_nosync(t->apic_irq);
	}
}

#ifdef CONFIG_PCI
static const char *mods_irq_type_name(u8 irq_type)
{
	switch (irq_type) {
	case MODS_IRQ_TYPE_INT:
		return "INTx";
	case MODS_IRQ_TYPE_MSI:
		return "MSI";
	case MODS_IRQ_TYPE_CPU:
		return "CPU";
	case MODS_IRQ_TYPE_MSIX:
		return "MSI-X";
	default:
		return "unknown";
	}
}
#endif

static struct mods_client *client_from_id(u8 client_id)
{
	return &mp.clients[client_id - 1];
}

static void wake_up_client(struct dev_irq_map *t)
{
	struct mods_client *client = client_from_id(t->client_id);

	if (client)
		wake_up_interruptible(&client->interrupt_event);
}

static int rec_irq_done(struct mods_client *client,
			struct dev_irq_map *t,
			unsigned int        irq_time)
{
	/* Get interrupt queue */
	struct irq_q_info *q = &client->irq_queue;

	/* Don't do anything if the IRQ has already been recorded */
	if (q->head != q->tail) {
		unsigned int i;

		for (i = q->head; i != q->tail; i++) {
			struct irq_q_data *pd
					= q->data+(i & (MODS_MAX_IRQS - 1));

			if ((pd->irq == t->apic_irq) &&
			    (!t->dev || (pd->dev == t->dev)))
				return false;
		}
	}

	/* Print an error if the queue is full */
	/* This is deadly! */
	if (q->tail - q->head == MODS_MAX_IRQS) {
		mods_error_printk("IRQ queue is full\n");
		return false;
	}

	/* Record the device which generated the IRQ in the queue */
	q->data[q->tail & (MODS_MAX_IRQS - 1)].dev       = t->dev;
	q->data[q->tail & (MODS_MAX_IRQS - 1)].irq       = t->apic_irq;
	q->data[q->tail & (MODS_MAX_IRQS - 1)].irq_index = t->entry;
	q->data[q->tail & (MODS_MAX_IRQS - 1)].time      = irq_time;
	q->tail++;

#ifdef CONFIG_PCI
	if (t->dev) {
		mods_debug_printk(DEBUG_ISR_DETAILED,
				  "dev %04x:%02x:%02x.%x %s IRQ 0x%x time=%uus\n",
				  pci_domain_nr(t->dev->bus),
				  t->dev->bus->number,
				  PCI_SLOT(t->dev->devfn),
				  PCI_FUNC(t->dev->devfn),
				  mods_irq_type_name(t->type),
				  t->apic_irq,
				  irq_time);
	} else
#endif
		mods_debug_printk(DEBUG_ISR_DETAILED,
				  "CPU IRQ 0x%x, time=%uus\n",
				  t->apic_irq,
				  irq_time);

	return true;
}

/* mods_irq_handle - interrupt function */
static irqreturn_t mods_irq_handle(int irq, void *data)
{
	struct dev_irq_map *t        = (struct dev_irq_map *)data;
	int                 serviced = false;

	if (unlikely(!t))
		mods_error_printk("received irq %d, but no context for it\n",
			irq);
	else if (unlikely(t->apic_irq != irq))
		mods_error_printk("received irq %d which doesn't match registered irq %d\n",
			irq, t->apic_irq);
	else {
		unsigned long       flags    = 0;
		int                 recorded = false;
		unsigned int        irq_time = get_cur_time();
		struct mods_client *client   = client_from_id(t->client_id);

		spin_lock_irqsave(&client->irq_lock, flags);

		/* Check if the interrupt is still pending (shared INTA) */
		if (mods_check_interrupt(t)) {

			/* Disable interrupts on this device to avoid interrupt
			 * storm
			 */
			mods_disable_interrupts(t);

			/* Record IRQ for MODS and wake MODS up */
			recorded = rec_irq_done(client, t, irq_time);

			serviced = true;
		}

		spin_unlock_irqrestore(&client->irq_lock, flags);

		if (recorded)
			wake_up_client(t);
	}

	return IRQ_RETVAL(serviced);
}

static int mods_lookup_cpu_irq(u8 client_id, unsigned int irq)
{
	u8  client_idx;
	int ret = IRQ_NOT_FOUND;

	LOG_ENT();

	for (client_idx = 1; client_idx <= MODS_MAX_CLIENTS; client_idx++) {
		struct dev_irq_map *t    = NULL;
		struct dev_irq_map *next = NULL;

		if (!test_bit(client_idx - 1, &mp.client_flags))
			continue;

		list_for_each_entry_safe(t,
					 next,
					 &client_from_id(client_idx)->irq_list,
					 list) {

			if (t->apic_irq == irq) {
				ret = (!client_id || client_id == client_idx)
				      ? IRQ_FOUND : IRQ_NOT_FOUND;

				/* Break out of the outer loop */
				client_idx = MODS_MAX_CLIENTS;
				break;
			}
		}
	}

	LOG_EXT();
	return ret;
}

#ifdef CONFIG_PCI
static int is_nvidia_gpu(struct pci_dev *dev)
{
	unsigned short class_code, vendor_id;

	pci_read_config_word(dev, PCI_CLASS_DEVICE, &class_code);
	pci_read_config_word(dev, PCI_VENDOR_ID, &vendor_id);
	if (((class_code == PCI_CLASS_DISPLAY_VGA) ||
	    (class_code == PCI_CLASS_DISPLAY_3D)) && (vendor_id == 0x10DE)) {
		return true;
	}
	return false;
}

static void setup_mask_info(struct dev_irq_map *newmap,
			    struct MODS_REGISTER_IRQ_4 *p,
			    struct pci_dev *dev)
{
	/* account for legacy adapters */
	u8 __iomem *bar = newmap->dev_irq_aperture;
	u32         ii  = 0;

	if ((p->mask_info_cnt == 0) && is_nvidia_gpu(dev)) {
		struct irq_mask_info *const m = &newmap->mask_info[0];

		newmap->mask_info_cnt = 1;
		m->dev_irq_mask_reg    = (void __iomem *)(bar + 0x140);
		m->dev_irq_disable_reg = (void __iomem *)(bar + 0x140);
		m->dev_irq_state       = (void __iomem *)(bar + 0x100);
		m->irq_and_mask        = 0;
		m->irq_or_mask         = 0;
		return;
	}

	/* setup for new adapters */
	newmap->mask_info_cnt = p->mask_info_cnt;
	for (ii = 0; ii < p->mask_info_cnt; ii++) {
		struct irq_mask_info         *const m = &newmap->mask_info[ii];
		const struct mods_mask_info2 *const in_m = &p->mask_info[ii];

		const u32 pend_offs = in_m->irq_pending_offset;
		const u32 stat_offs = in_m->irq_enabled_offset;
		const u32 dis_offs  = in_m->irq_disable_offset;

		m->dev_irq_state       = (void __iomem *)(bar + pend_offs);
		m->dev_irq_mask_reg    = (void __iomem *)(bar + stat_offs);
		m->dev_irq_disable_reg = (void __iomem *)(bar + dis_offs);
		m->irq_and_mask        = in_m->and_mask;
		m->irq_or_mask         = in_m->or_mask;
		m->mask_type           = in_m->mask_type;
	}
}
#endif

static int add_irq_map(struct mods_client         *client,
		       struct pci_dev             *dev,
		       struct MODS_REGISTER_IRQ_4 *p,
		       u32                         irq,
		       u32                         entry)
{
	struct dev_irq_map *newmap     = NULL;
	u64                 valid_mask = IRQF_TRIGGER_NONE;
	u64                 irq_flags  = MODS_IRQ_FLAG_FROM_FLAGS(p->irq_flags);
	u32                 irq_type   = MODS_IRQ_TYPE_FROM_FLAGS(p->irq_flags);

	LOG_ENT();

	/* Get the flags based on the interrupt type*/
	switch (irq_type) {
	case MODS_IRQ_TYPE_INT:
		irq_flags = IRQF_SHARED;
		break;

	case MODS_IRQ_TYPE_CPU:
		valid_mask = IRQF_TRIGGER_RISING |
			     IRQF_TRIGGER_FALLING |
			     IRQF_SHARED;

		/* Either use a valid flag bit or no flags */
		if (irq_flags & ~valid_mask) {
			cl_error("invalid device interrupt flag %llx\n",
				 (long long)irq_flags);
			return -EINVAL;
		}
		break;

	default:
		irq_flags = IRQF_TRIGGER_NONE;
		break;
	}

	/* Allocate memory for the new entry */
	newmap = kzalloc(sizeof(*newmap), GFP_KERNEL | __GFP_NORETRY);
	if (unlikely(!newmap)) {
		LOG_EXT();
		return -ENOMEM;
	}
	atomic_inc(&client->num_allocs);

	/* Fill out the new entry */
	newmap->apic_irq         = irq;
	newmap->dev              = dev;
	newmap->client_id        = client->client_id;
	newmap->dev_irq_aperture = NULL;
	newmap->mask_info_cnt    = 0;
	newmap->type             = irq_type;
	newmap->entry            = entry;

	/* Enable IRQ for this device in the kernel */
	if (request_irq(irq,
			&mods_irq_handle,
			irq_flags,
			"nvidia mods",
			newmap)) {
		cl_error("unable to enable IRQ 0x%x with flags %llx\n",
			 irq,
			 (long long)irq_flags);
		kfree(newmap);
		atomic_dec(&client->num_allocs);
		LOG_EXT();
		return -EPERM;
	}

	/* Add the new entry to the list of all registered interrupts */
	list_add(&newmap->list, &client->irq_list);

#ifdef CONFIG_PCI
	/* Map BAR0 to be able to disable interrupts */
	if ((irq_type == MODS_IRQ_TYPE_INT) &&
	    (p->aperture_addr != 0) &&
	    (p->aperture_size != 0)) {
		u8 __iomem *bar = ioremap(p->aperture_addr, p->aperture_size);

		if (!bar) {
			cl_debug(DEBUG_ISR,
				 "failed to remap aperture: 0x%llx size=0x%x\n",
				 p->aperture_addr,
				 p->aperture_size);
			LOG_EXT();
			return -EPERM;
		}

		newmap->dev_irq_aperture = bar;
		setup_mask_info(newmap, p, dev);
	}

	if (dev)
		pci_dev_get(dev);
#endif

	/* Print out successful registration string */
	if (irq_type == MODS_IRQ_TYPE_CPU)
		cl_debug(DEBUG_ISR,
			 "registered CPU IRQ 0x%x with flags %llx\n",
			 irq,
			 (long long)irq_flags);
#ifdef CONFIG_PCI
	else if ((irq_type == MODS_IRQ_TYPE_INT) ||
		 (irq_type == MODS_IRQ_TYPE_MSI) ||
		 (irq_type == MODS_IRQ_TYPE_MSIX)) {
		cl_debug(DEBUG_ISR,
			 "dev %04x:%02x:%02x.%x registered %s IRQ 0x%x\n",
			 dev ? pci_domain_nr(dev->bus) : 0U,
			 dev ? dev->bus->number : 0U,
			 dev ? PCI_SLOT(dev->devfn) : 0U,
			 dev ? PCI_FUNC(dev->devfn) : 0U,
			 mods_irq_type_name(irq_type),
			 irq);
	}
#endif
#ifdef CONFIG_PCI_MSI
	else if (irq_type == MODS_IRQ_TYPE_MSI) {
		u16 control;
		u16 data;
		int cap_pos = pci_find_capability(dev, PCI_CAP_ID_MSI);

		pci_read_config_word(dev, MSI_CONTROL_REG(cap_pos), &control);
		if (IS_64BIT_ADDRESS(control))
			pci_read_config_word(dev,
						 MSI_DATA_REG(cap_pos, 1),
						 &data);
		else
			pci_read_config_word(dev,
						 MSI_DATA_REG(cap_pos, 0),
						 &data);
		cl_debug(DEBUG_ISR,
			 "dev %04x:%02x:%02x.%x registered MSI IRQ 0x%x data:0x%02x\n",
			 pci_domain_nr(dev->bus),
			 dev->bus->number,
			 PCI_SLOT(dev->devfn),
			 PCI_FUNC(dev->devfn),
			 irq,
			 data);
	} else if (irq_type == MODS_IRQ_TYPE_MSIX) {
		cl_debug(DEBUG_ISR,
			 "dev %04x:%02x:%02x.%x registered MSI-X IRQ 0x%x\n",
			 pci_domain_nr(dev->bus),
			 dev->bus->number,
			 PCI_SLOT(dev->devfn),
			 PCI_FUNC(dev->devfn),
			 irq);
	}
#endif

	LOG_EXT();
	return OK;
}

static void mods_free_map(struct mods_client *client,
			  struct dev_irq_map *del)
{
	unsigned long       flags  = 0;

	LOG_ENT();

	WARN_ON(client->client_id != del->client_id);

	/* Disable interrupts on the device */
	spin_lock_irqsave(&client->irq_lock, flags);
	mods_disable_interrupts(del);
	spin_unlock_irqrestore(&client->irq_lock, flags);

	/* Unhook interrupts in the kernel */
	free_irq(del->apic_irq, del);

	/* Unmap aperture used for masking irqs */
	if (del->dev_irq_aperture)
		iounmap(del->dev_irq_aperture);

#ifdef CONFIG_PCI
	pci_dev_put(del->dev);
#endif

	/* Free memory */
	kfree(del);
	atomic_dec(&client->num_allocs);

	LOG_EXT();
}

void mods_init_irq(void)
{
	LOG_ENT();

	memset(&mp, 0, sizeof(mp));

	mutex_init(&mp.mtx);

	LOG_EXT();
}

void mods_cleanup_irq(void)
{
	int i;

	LOG_ENT();
	for (i = 0; i < MODS_MAX_CLIENTS; i++) {
		if (mp.client_flags & (1 << i))
			mods_free_client(i + 1);
	}
	LOG_EXT();
}

POLL_TYPE mods_irq_event_check(u8 client_id)
{
	struct irq_q_info *q = &client_from_id(client_id)->irq_queue;
	unsigned int pos = (1 << (client_id - 1));

	if (!(mp.client_flags & pos))
		return POLLERR; /* irq has quit */

	if (q->head != q->tail)
		return POLLIN; /* irq generated */

	return 0;
}

struct mods_client *mods_alloc_client(void)
{
	u8 idx;
	u8 max_clients = 1;

	LOG_ENT();

	if (mods_get_multi_instance() ||
	    (mods_get_access_token() != MODS_ACCESS_TOKEN_NONE))
		max_clients = MODS_MAX_CLIENTS;

	for (idx = 1; idx <= max_clients; idx++) {
		if (!test_and_set_bit(idx - 1, &mp.client_flags)) {
			struct mods_client *client = client_from_id(idx);

			memset(client, 0, sizeof(*client));
			client->client_id    = idx;
			client->access_token = MODS_ACCESS_TOKEN_NONE;
			atomic_set(&client->last_bad_dbdf, -1);

			cl_debug(DEBUG_IOCTL,
				 "open client (bit mask 0x%lx)\n",
				 mp.client_flags);

			mutex_init(&client->mtx);
			spin_lock_init(&client->irq_lock);
			init_waitqueue_head(&client->interrupt_event);
			INIT_LIST_HEAD(&client->irq_list);
			INIT_LIST_HEAD(&client->mem_alloc_list);
			INIT_LIST_HEAD(&client->mem_map_list);
			INIT_LIST_HEAD(&client->free_mem_list);
#if defined(CONFIG_PPC64)
			INIT_LIST_HEAD(&client->ppc_tce_bypass_list);
			INIT_LIST_HEAD(&client->nvlink_sysmem_trained_list);
#endif

			LOG_EXT();
			return client;
		}
	}

	LOG_EXT();
	return NULL;
}

static int mods_free_irqs(struct mods_client *client,
			  struct pci_dev     *dev)
{
#ifdef CONFIG_PCI
	struct dev_irq_map  *del    = NULL;
	struct dev_irq_map  *next;
	struct en_dev_entry *dpriv;
	unsigned int         irq_type;

	LOG_ENT();

	if (unlikely(mutex_lock_interruptible(&mp.mtx))) {
		LOG_EXT();
		return -EINTR;
	}

	dpriv = pci_get_drvdata(dev);

	if (!dpriv) {
		mutex_unlock(&mp.mtx);
		LOG_EXT();
		return OK;
	}

	if (dpriv->client_id != client->client_id) {
		cl_error(
			"invalid client for dev %04x:%02x:%02x.%x, expected %u\n",
			pci_domain_nr(dev->bus),
			dev->bus->number,
			PCI_SLOT(dev->devfn),
			PCI_FUNC(dev->devfn),
			dpriv->client_id);
		mutex_unlock(&mp.mtx);
		LOG_EXT();
		return -EINVAL;
	}

	cl_debug(DEBUG_ISR_DETAILED,
		 "free IRQ for dev %04x:%02x:%02x.%x irq_flags=0x%x nvecs=%d\n",
		 pci_domain_nr(dev->bus),
		 dev->bus->number,
		 PCI_SLOT(dev->devfn),
		 PCI_FUNC(dev->devfn),
		 dpriv->irq_flags,
		 dpriv->nvecs);

	/* Delete device interrupts from the list */
	list_for_each_entry_safe(del, next, &client->irq_list, list) {
		if (dev == del->dev) {
			u8 type = del->type;

			list_del(&del->list);
			cl_debug(DEBUG_ISR,
				 "unregistered %s IRQ 0x%x dev %04x:%02x:%02x.%x\n",
				 mods_irq_type_name(type),
				 del->apic_irq,
				 pci_domain_nr(dev->bus),
				 dev->bus->number,
				 PCI_SLOT(dev->devfn),
				 PCI_FUNC(dev->devfn));
			mods_free_map(client, del);

			WARN_ON(MODS_IRQ_TYPE_FROM_FLAGS(dpriv->irq_flags)
				!= type);
			if (type != MODS_IRQ_TYPE_MSIX)
				break;
		}
	}

	cl_debug(DEBUG_ISR_DETAILED, "before disable\n");
#ifdef CONFIG_PCI_MSI
	irq_type = MODS_IRQ_TYPE_FROM_FLAGS(dpriv->irq_flags);

	if (irq_type == MODS_IRQ_TYPE_MSIX) {
		pci_disable_msix(dev);
		kfree(dpriv->msix_entries);
		if (dpriv->msix_entries)
			atomic_dec(&client->num_allocs);
		dpriv->msix_entries = NULL;
	} else if (irq_type == MODS_IRQ_TYPE_MSI) {
		pci_disable_msi(dev);
	}
#endif

	dpriv->nvecs = 0;
	cl_debug(DEBUG_ISR_DETAILED, "irqs freed\n");
#endif

	mutex_unlock(&mp.mtx);
	LOG_EXT();
	return 0;
}

void mods_free_client_interrupts(struct mods_client *client)
{
	struct en_dev_entry *dpriv = client->enabled_devices;

	LOG_ENT();

	/* Release all interrupts */
	while (dpriv) {
		mods_free_irqs(client, dpriv->dev);
		dpriv = dpriv->next;
	}

	LOG_EXT();
}

void mods_free_client(u8 client_id)
{
	struct mods_client *client = client_from_id(client_id);

	LOG_ENT();

	memset(client, 0, sizeof(*client));

	/* Indicate the client_id is free */
	clear_bit(client_id - 1, &mp.client_flags);

	cl_debug(DEBUG_IOCTL, "closed client\n");
	LOG_EXT();
}

#ifdef CONFIG_PCI
static int mods_allocate_irqs(struct mods_client *client,
			      struct pci_dev     *dev,
			      u32                 nvecs,
			      u32                 flags)
{
	struct en_dev_entry *dpriv;
	unsigned int         irq_type = MODS_IRQ_TYPE_FROM_FLAGS(flags);
	int                  err      = OK;

	LOG_ENT();

	cl_debug(DEBUG_ISR_DETAILED,
		 "allocate %u IRQs on dev %04x:%02x:%02x.%x, flags=0x%x\n",
		 nvecs,
		 pci_domain_nr(dev->bus),
		 dev->bus->number,
		 PCI_SLOT(dev->devfn),
		 PCI_FUNC(dev->devfn),
		 flags);

	/* Determine if the device supports requested interrupt type */
	if (irq_type == MODS_IRQ_TYPE_MSI) {
#ifdef CONFIG_PCI_MSI
		if (pci_find_capability(dev, PCI_CAP_ID_MSI) == 0) {
			cl_error("dev %04x:%02x:%02x.%x does not support MSI\n",
				 pci_domain_nr(dev->bus),
				 dev->bus->number,
				 PCI_SLOT(dev->devfn),
				 PCI_FUNC(dev->devfn));
			LOG_EXT();
			return -ENOENT;
		}
#else
		cl_error("the kernel does not support MSI\n");
		LOG_EXT();
		return -EINVAL;
#endif
	} else if (irq_type == MODS_IRQ_TYPE_MSIX) {
#ifdef CONFIG_PCI_MSI
		if (pci_find_capability(dev, PCI_CAP_ID_MSIX) == 0) {
			cl_error(
				"dev %04x:%02x:%02x.%x does not support MSI-X\n",
				pci_domain_nr(dev->bus),
				dev->bus->number,
				PCI_SLOT(dev->devfn),
				PCI_FUNC(dev->devfn));
			LOG_EXT();
			return -ENOENT;
		}
#else
		cl_error("the kernel does not support MSI-X\n");
		LOG_EXT();
		return -EINVAL;
#endif
	}

	/* Enable device on the PCI bus */
	err = mods_enable_device(client, dev, &dpriv);
	if (err) {
		LOG_EXT();
		return err;
	}

	if (irq_type == MODS_IRQ_TYPE_INT) {
		/* use legacy irq */
		if (nvecs != 1) {
			cl_error(
				"INTA: only 1 INTA vector supported, requested %u\n",
				nvecs);
			LOG_EXT();
			return -EINVAL;
		}
		dpriv->nvecs = 1;
	}
	/* Enable MSI */
#ifdef CONFIG_PCI_MSI
	else if (irq_type == MODS_IRQ_TYPE_MSI) {
		if (nvecs != 1) {
			cl_error(
				"MSI: only 1 MSI vector supported, requested %u\n",
				nvecs);
			LOG_EXT();
			return -EINVAL;
		}
		err = pci_enable_msi(dev);
		if (err) {
			cl_error(
				"unable to enable MSI on dev %04x:%02x:%02x.%x\n",
				pci_domain_nr(dev->bus),
				dev->bus->number,
				PCI_SLOT(dev->devfn),
				PCI_FUNC(dev->devfn));
			LOG_EXT();
			return err;
		}
		dpriv->nvecs = 1;
	} else if (irq_type == MODS_IRQ_TYPE_MSIX) {
		struct msix_entry *entries;
		int i = 0, cnt = 1;

		entries = kcalloc(nvecs, sizeof(struct msix_entry),
				GFP_KERNEL | __GFP_NORETRY);

		if (!entries) {
			cl_error("could not allocate %d MSI-X entries\n",
				 nvecs);
			LOG_EXT();
			return -ENOMEM;
		}
		atomic_inc(&client->num_allocs);

		for (i = 0; i < nvecs; i++)
			entries[i].entry = (uint16_t)i;

#ifdef MODS_HAS_MSIX_RANGE
		cnt = pci_enable_msix_range(dev, entries, nvecs, nvecs);

		if (cnt < 0) {
			/* returns number of interrupts allocated
			 * < 0 indicates a failure.
			 */
			cl_error(
				"could not allocate the requested number of MSI-X vectors=%d return=%d!\n",
				nvecs, cnt);
			kfree(entries);
			atomic_dec(&client->num_allocs);
			LOG_EXT();
			return cnt;
		}
#else
		cnt = pci_enable_msix(dev, entries, nvecs);

		if (cnt) {
			/*  A return of < 0 indicates a failure.
			 *  A return of > 0 indicates that driver request is
			 *  exceeding the number of irqs or MSI-X
			 *  vectors available
			 */
			cl_error(
				"could not allocate the requested number of MSI-X vectors=%d return=%d!\n",
				nvecs, cnt);
			kfree(entries);
			atomic_dec(&client->num_allocs);
			LOG_EXT();
			if (cnt > 0)
				cnt = -ENOSPC;
			return cnt;
		}
#endif

		cl_debug(DEBUG_ISR,
			 "allocated %d irq's of type %s(%d)\n",
			 nvecs,
			 mods_irq_type_name(irq_type),
			 irq_type);

		for (i = 0; i < nvecs; i++)
			cl_debug(DEBUG_ISR,
				 "vec %d %x\n",
				 entries[i].entry,
				 entries[i].vector);

		dpriv->nvecs = nvecs;
		dpriv->msix_entries = entries;
	}
#endif
	else {
		cl_error("unsupported irq_type %u dev %04x:%02x:%02x.%x\n",
			 irq_type,
			 pci_domain_nr(dev->bus),
			 dev->bus->number,
			 PCI_SLOT(dev->devfn),
			 PCI_FUNC(dev->devfn));
		LOG_EXT();
		return -EINVAL;
	}

	WARN_ON(dpriv->client_id != client->client_id);
	dpriv->irq_flags = flags;
	LOG_EXT();
	return OK;
}

static int mods_register_pci_irq(struct mods_client         *client,
				 struct MODS_REGISTER_IRQ_4 *p)
{
	int                  err = OK;
	unsigned int         irq_type = MODS_IRQ_TYPE_FROM_FLAGS(p->irq_flags);
	struct pci_dev      *dev;
	struct en_dev_entry *dpriv;
	int                  i;

	LOG_ENT();

	if (unlikely(!p->irq_count)) {
		cl_error("no irq's requested\n");
		LOG_EXT();
		return -EINVAL;
	}

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

	if (unlikely(mutex_lock_interruptible(&mp.mtx))) {
		pci_dev_put(dev);
		LOG_EXT();
		return -EINTR;
	}

	dpriv = pci_get_drvdata(dev);
	if (dpriv) {
		if (dpriv->client_id != client->client_id) {
			cl_error(
				"dev %04x:%02x:%02x.%x already owned by client %u\n",
				p->dev.domain,
				p->dev.bus,
				p->dev.device,
				p->dev.function,
				dpriv->client_id);
			mutex_unlock(&mp.mtx);
			pci_dev_put(dev);
			LOG_EXT();
			return -EBUSY;
		}
		if (dpriv->nvecs) {
			cl_error(
				"interrupt for dev %04x:%02x:%02x.%x already registered\n",
				p->dev.domain,
				p->dev.bus,
				p->dev.device,
				p->dev.function);
			mutex_unlock(&mp.mtx);
			pci_dev_put(dev);
			LOG_EXT();
			return -EINVAL;
		}
	}

	err = mods_allocate_irqs(client, dev, p->irq_count,
				 p->irq_flags);
	if (err) {
		cl_error("could not allocate irqs for irq_type %d\n",
			 irq_type);
		mutex_unlock(&mp.mtx);
		pci_dev_put(dev);
		LOG_EXT();
		return err;
	}

	dpriv = pci_get_drvdata(dev);

	for (i = 0; i < p->irq_count; i++) {
		u32 irq = ((irq_type == MODS_IRQ_TYPE_INT) ||
				(irq_type == MODS_IRQ_TYPE_MSI)) ? dev->irq :
			   dpriv->msix_entries[i].vector;

		err = add_irq_map(client, dev, p, irq, i);
		if (unlikely(err)) {
#ifdef CONFIG_PCI_MSI
			if (irq_type == MODS_IRQ_TYPE_MSI)
				pci_disable_msi(dev);
			else if (irq_type == MODS_IRQ_TYPE_MSIX)
				pci_disable_msix(dev);
#endif
			break;
		}
	}

	mutex_unlock(&mp.mtx);
	pci_dev_put(dev);
	LOG_EXT();
	return err;
}
#endif /* CONFIG_PCI */

static int mods_register_cpu_irq(struct mods_client         *client,
				 struct MODS_REGISTER_IRQ_4 *p)
{
	u32 irq = p->dev.bus;
	int err;

	LOG_ENT();

	if (unlikely(mutex_lock_interruptible(&mp.mtx))) {
		LOG_EXT();
		return -EINTR;
	}

	/* Determine if the interrupt is already hooked */
	if (mods_lookup_cpu_irq(0, irq) == IRQ_FOUND) {
		cl_error("CPU IRQ 0x%x has already been registered\n", irq);
		mutex_unlock(&mp.mtx);
		LOG_EXT();
		return -EINVAL;
	}

	/* Register interrupt */
	err = add_irq_map(client, NULL, p, irq, 0);

	mutex_unlock(&mp.mtx);
	LOG_EXT();
	return err;
}

#ifdef CONFIG_PCI
static int mods_unregister_pci_irq(struct mods_client         *client,
				   struct MODS_REGISTER_IRQ_2 *p)
{
	struct pci_dev *dev;
	int             err = OK;

	LOG_ENT();

	/* Get the PCI device structure for the specified device from kernel */
	err = mods_find_pci_dev(client, &p->dev, &dev);
	if (unlikely(err)) {
		LOG_EXT();
		return err;
	}

	err = mods_free_irqs(client, dev);

	pci_dev_put(dev);
	LOG_EXT();
	return err;
}
#endif

static int mods_unregister_cpu_irq(struct mods_client *client,
				   struct MODS_REGISTER_IRQ_2 *p)
{
	struct dev_irq_map *del = NULL;
	struct dev_irq_map *next;
	unsigned int        irq;

	LOG_ENT();

	irq = p->dev.bus;

	if (unlikely(mutex_lock_interruptible(&mp.mtx))) {
		LOG_EXT();
		return -EINTR;
	}

	/* Determine if the interrupt is already hooked by this client */
	if (mods_lookup_cpu_irq(client->client_id, irq) == IRQ_NOT_FOUND) {
		cl_error("IRQ 0x%x not hooked, can't unhook\n", irq);
		mutex_unlock(&mp.mtx);
		LOG_EXT();
		return -EINVAL;
	}

	/* Delete device interrupt from the list */
	list_for_each_entry_safe(del, next, &client->irq_list, list) {
		if ((irq == del->apic_irq) && (del->dev == NULL)) {
			if (del->type != p->type) {
				cl_error("wrong IRQ type passed\n");
				mutex_unlock(&mp.mtx);
				LOG_EXT();
				return -EINVAL;
			}
			list_del(&del->list);
			cl_debug(DEBUG_ISR,
				 "unregistered CPU IRQ 0x%x\n",
				 irq);
			mods_free_map(client, del);
			break;
		}
	}

	mutex_unlock(&mp.mtx);
	LOG_EXT();
	return OK;
}

/*************************
 * ESCAPE CALL FUNCTIONS *
 *************************/

int esc_mods_register_irq_4(struct mods_client         *client,
			    struct MODS_REGISTER_IRQ_4 *p)
{
	u32 irq_type = MODS_IRQ_TYPE_FROM_FLAGS(p->irq_flags);

	if (irq_type == MODS_IRQ_TYPE_CPU)
		return mods_register_cpu_irq(client, p);
#ifdef CONFIG_PCI
	return mods_register_pci_irq(client, p);
#else
	cl_error("PCI not available\n");
	return -EINVAL;
#endif
}

int esc_mods_register_irq_3(struct mods_client         *client,
			    struct MODS_REGISTER_IRQ_3 *p)
{
	struct MODS_REGISTER_IRQ_4 irq_data = { {0} };
	u32 ii = 0;

	irq_data.dev = p->dev;
	irq_data.aperture_addr = p->aperture_addr;
	irq_data.aperture_size = p->aperture_size;
	irq_data.mask_info_cnt = p->mask_info_cnt;
	for (ii = 0; ii < p->mask_info_cnt; ii++) {
		irq_data.mask_info[ii].mask_type = p->mask_info[ii].mask_type;
		irq_data.mask_info[ii].irq_pending_offset =
			p->mask_info[ii].irq_pending_offset;
		irq_data.mask_info[ii].irq_enabled_offset =
			p->mask_info[ii].irq_enabled_offset;
		irq_data.mask_info[ii].irq_enable_offset =
			p->mask_info[ii].irq_enable_offset;
		irq_data.mask_info[ii].irq_disable_offset =
			p->mask_info[ii].irq_disable_offset;
		irq_data.mask_info[ii].and_mask =
			p->mask_info[ii].and_mask;
		irq_data.mask_info[ii].or_mask =
			p->mask_info[ii].or_mask;
	}
	irq_data.irq_count = 1;
	irq_data.irq_flags = p->irq_type;

	return esc_mods_register_irq_4(client, &irq_data);
}

int esc_mods_register_irq_2(struct mods_client         *client,
			    struct MODS_REGISTER_IRQ_2 *p)
{
	struct MODS_REGISTER_IRQ_4 irq_data = { {0} };

	irq_data.dev = p->dev;
	irq_data.irq_count = 1;
	irq_data.irq_flags = p->type;

#ifdef CONFIG_PCI
	{
		/* Get the PCI device structure */
		struct pci_dev *dev;
		int             err;

		err = mods_find_pci_dev(client, &p->dev, &dev);
		if (unlikely(err))
			return err;

		irq_data.aperture_addr = pci_resource_start(dev, 0);
		irq_data.aperture_size = pci_resource_len(dev, 0);

		pci_dev_put(dev);
	}
#endif

	return esc_mods_register_irq_4(client, &irq_data);
}

int esc_mods_register_irq(struct mods_client       *client,
			  struct MODS_REGISTER_IRQ *p)
{
	struct MODS_REGISTER_IRQ_2 register_irq = { {0} };

	register_irq.dev.domain		= 0;
	register_irq.dev.bus		= p->dev.bus;
	register_irq.dev.device		= p->dev.device;
	register_irq.dev.function	= p->dev.function;
	register_irq.type		= p->type;

	return esc_mods_register_irq_2(client, &register_irq);
}

int esc_mods_unregister_irq_2(struct mods_client         *client,
			      struct MODS_REGISTER_IRQ_2 *p)
{
	if (p->type == MODS_IRQ_TYPE_CPU)
		return mods_unregister_cpu_irq(client, p);
#ifdef CONFIG_PCI
	return mods_unregister_pci_irq(client, p);
#else
	return -EINVAL;
#endif
}

int esc_mods_unregister_irq(struct mods_client       *client,
			    struct MODS_REGISTER_IRQ *p)
{
	struct MODS_REGISTER_IRQ_2 register_irq = { {0} };

	register_irq.dev.domain   = 0;
	register_irq.dev.bus      = p->dev.bus;
	register_irq.dev.device   = p->dev.device;
	register_irq.dev.function = p->dev.function;
	register_irq.type         = p->type;

	return esc_mods_unregister_irq_2(client, &register_irq);
}

int esc_mods_query_irq_3(struct mods_client      *client,
			 struct MODS_QUERY_IRQ_3 *p)
{
	struct irq_q_info  *q        = NULL;
	unsigned int        i        = 0;
	unsigned long       flags    = 0;
	unsigned int        cur_time = get_cur_time();

	LOG_ENT();

	/* Clear return array */
	memset(p->irq_list, 0xFF, sizeof(p->irq_list));

	/* Lock IRQ queue */
	spin_lock_irqsave(&client->irq_lock, flags);

	/* Fill in return array with IRQ information */
	q = &client->irq_queue;
	for (i = 0;
		 (q->head != q->tail) && (i < MODS_MAX_IRQS);
		 q->head++, i++) {

		unsigned int    index = q->head & (MODS_MAX_IRQS - 1);
		struct pci_dev *dev   = q->data[index].dev;

		if (dev) {
			p->irq_list[i].dev.domain = pci_domain_nr(dev->bus);
			p->irq_list[i].dev.bus = dev->bus->number;
			p->irq_list[i].dev.device = PCI_SLOT(dev->devfn);
			p->irq_list[i].dev.function = PCI_FUNC(dev->devfn);
		} else {
			p->irq_list[i].dev.domain = 0;
			p->irq_list[i].dev.bus = q->data[index].irq;
			p->irq_list[i].dev.device = 0xFFU;
			p->irq_list[i].dev.function = 0xFFU;
		}
		p->irq_list[i].irq_index = q->data[index].irq_index;
		p->irq_list[i].delay = cur_time - q->data[index].time;

		/* Print info about IRQ status returned */
		if (dev) {
			cl_debug(DEBUG_ISR_DETAILED,
				 "retrieved IRQ index=%d dev %04x:%02x:%02x.%x, time=%uus, delay=%uus\n",
				 p->irq_list[i].irq_index,
				 p->irq_list[i].dev.domain,
				 p->irq_list[i].dev.bus,
				 p->irq_list[i].dev.device,
				 p->irq_list[i].dev.function,
				 q->data[index].time,
				 p->irq_list[i].delay);
		} else {
			cl_debug(DEBUG_ISR_DETAILED,
				 "retrieved IRQ 0x%x, time=%uus, delay=%uus\n",
				 (unsigned int)p->irq_list[i].dev.bus,
				 q->data[index].time,
				 p->irq_list[i].delay);
		}
	}

	/* Indicate if there are more IRQs pending */
	if (q->head != q->tail)
		p->more = 1;

	/* Unlock IRQ queue */
	spin_unlock_irqrestore(&client->irq_lock, flags);

	LOG_EXT();
	return OK;
}

int esc_mods_query_irq_2(struct mods_client      *client,
			 struct MODS_QUERY_IRQ_2 *p)
{
	int retval, i;
	struct MODS_QUERY_IRQ_3 query_irq = { { { { 0 } } } };

	retval = esc_mods_query_irq_3(client, &query_irq);
	if (retval)
		return retval;

	for (i = 0; i < MODS_MAX_IRQS; i++) {
		p->irq_list[i].dev   = query_irq.irq_list[i].dev;
		p->irq_list[i].delay = query_irq.irq_list[i].delay;
	}
	p->more = query_irq.more;
	return OK;
}

int esc_mods_query_irq(struct mods_client    *client,
		       struct MODS_QUERY_IRQ *p)
{
	int retval, i;
	struct MODS_QUERY_IRQ_3 query_irq = { { { { 0 } } } };

	retval = esc_mods_query_irq_3(client, &query_irq);
	if (retval)
		return retval;

	for (i = 0; i < MODS_MAX_IRQS; i++) {
		p->irq_list[i].dev.bus    = query_irq.irq_list[i].dev.bus;
		p->irq_list[i].dev.device = query_irq.irq_list[i].dev.device;
		p->irq_list[i].dev.function
					  = query_irq.irq_list[i].dev.function;
		p->irq_list[i].delay	  = query_irq.irq_list[i].delay;
	}
	p->more = query_irq.more;
	return OK;
}

int esc_mods_irq_handled_2(struct mods_client         *client,
			   struct MODS_REGISTER_IRQ_2 *p)
{
	unsigned long       flags = 0;
	u32                 irq = p->dev.bus;
	struct dev_irq_map *t = NULL;
	struct dev_irq_map *next = NULL;
	int                 err = -EINVAL;

	if (p->type != MODS_IRQ_TYPE_CPU)
		return -EINVAL;

	LOG_ENT();

	/* Print info */
	cl_debug(DEBUG_ISR_DETAILED, "mark CPU IRQ 0x%x handled\n", irq);

	/* Lock IRQ queue */
	spin_lock_irqsave(&client->irq_lock, flags);

	list_for_each_entry_safe(t, next, &client->irq_list, list) {
		if (t->apic_irq == irq) {
			if (t->type != p->type) {
				cl_error(
					"IRQ type doesn't match registered IRQ\n");
			} else {
				enable_irq(irq);
				err = OK;
			}
			break;
		}
	}

	/* Unlock IRQ queue */
	spin_unlock_irqrestore(&client->irq_lock, flags);

	LOG_EXT();
	return err;
}

int esc_mods_irq_handled(struct mods_client       *client,
			 struct MODS_REGISTER_IRQ *p)
{
	struct MODS_REGISTER_IRQ_2 register_irq = { {0} };

	register_irq.dev.domain   = 0;
	register_irq.dev.bus      = p->dev.bus;
	register_irq.dev.device   = p->dev.device;
	register_irq.dev.function = p->dev.function;
	register_irq.type         = p->type;

	return esc_mods_irq_handled_2(client, &register_irq);
}

#if defined(MODS_HAS_TEGRA) && defined(CONFIG_OF_IRQ) && defined(CONFIG_OF)
int esc_mods_map_irq(struct mods_client  *client,
		     struct MODS_DT_INFO *p)
{
	int err = 0;
	/* the physical irq */
	int hwirq;
	/* platform device handle */
	struct platform_device *pdev = NULL;
	/* irq parameters */
	struct of_phandle_args oirq;
	/* Search for the node by device tree name */
	struct device_node *np = of_find_node_by_name(NULL, p->dt_name);

	if (!np) {
		cl_error("node %s is not valid\n", p->full_name);
		err = -EINVAL;
		goto error;
	}

	/* Can be multiple nodes that share the same dt name, */
	/* make sure you get the correct node matched by the device's full */
	/* name in device tree (i.e. watchdog@30c0000 as opposed */
	/* to watchdog) */
	while (of_node_cmp(np->full_name, p->full_name)) {
		np = of_find_node_by_name(np, p->dt_name);
		if (!np) {
			cl_error("node %s is not valid\n", p->full_name);
			err = -EINVAL;
			goto error;
		}
	}

	p->irq = irq_of_parse_and_map(np, p->index);
	err = of_irq_parse_one(np, p->index, &oirq);
	if (err) {
		cl_error("could not parse IRQ\n");
		goto error;
	}

	hwirq = oirq.args[1];

	/* Get the platform device handle */
	pdev = of_find_device_by_node(np);

	if (of_node_cmp(p->dt_name, "watchdog") == 0) {
		/* Enable and unmask interrupt for watchdog */
		struct resource *res_src =
			platform_get_resource(pdev, IORESOURCE_MEM, 0);
		struct resource *res_tke =
			platform_get_resource(pdev, IORESOURCE_MEM, 2);
		void __iomem    *wdt_tke = NULL;
		int              wdt_index;

		if (res_tke && res_src) {
			wdt_tke = devm_ioremap(&pdev->dev, res_tke->start,
					       resource_size(res_tke));
			wdt_index = ((res_src->start >> 16) & 0xF) - 0xc;
		}

		if (wdt_tke) {
			writel(TOP_TKE_TKEIE_WDT_MASK(wdt_index),
			       wdt_tke + TOP_TKE_TKEIE(hwirq));
		}
	}

error:
	of_node_put(np);
	/* enable the interrupt */
	return err;
}

int esc_mods_map_irq_to_gpio(struct mods_client    *client,
			     struct MODS_GPIO_INFO *p)
{
	//TODO: Make sure you are allocating gpio properly
	int gpio_handle;
	int irq;
	int err = 0;

	struct device_node *np = of_find_node_by_name(NULL, p->dt_name);

	if (!np) {
		cl_error("node %s is not valid\n", p->full_name);
		err = -EINVAL;
		goto error;
	}

	while (of_node_cmp(np->full_name, p->full_name)) {
		np = of_find_node_by_name(np, p->dt_name);
		if (!np) {
			cl_error("node %s is not valid\n", p->full_name);
			err = -EINVAL;
			goto error;
		}
	}

	gpio_handle = of_get_named_gpio(np, p->name, 0);
	if (!gpio_is_valid(gpio_handle)) {
		cl_error("gpio %s is missing\n", p->name);
		err = gpio_handle;
		goto error;
	}

	err = gpio_direction_input(gpio_handle);
	if (err < 0) {
		cl_error("pex_rst_gpio input direction change failed\n");
		goto error;
	}

	irq = gpio_to_irq(gpio_handle);
	if (irq < 0) {
		cl_error("Unable to get irq for pex_rst_gpio\n");
		err = -EINVAL;
		goto error;
	}
	p->irq = irq;

error:
	of_node_put(np);
	return err;
}
#endif
