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

#include <linux/device.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/pci.h>
#if defined(MODS_HAS_DMA_OPS)
#include <linux/dma-mapping.h>
#endif

int mods_is_pci_dev(struct pci_dev        *dev,
		    struct mods_pci_dev_2 *pcidev)
{
	unsigned int devfn = PCI_DEVFN(pcidev->device, pcidev->function);

	return dev &&
	       pci_domain_nr(dev->bus) == pcidev->domain &&
	       dev->bus->number == pcidev->bus &&
	       dev->devfn == devfn;
}

int mods_find_pci_dev(struct mods_client    *client,
		      struct mods_pci_dev_2 *pcidev,
		      struct pci_dev	   **retdev)
{
	struct pci_dev *dev;
	int             err;

	if (unlikely(mutex_lock_interruptible(&client->mtx)))
		return -EINTR;

	dev = client->cached_dev;

	if (mods_is_pci_dev(dev, pcidev)) {
		*retdev = pci_dev_get(dev);
		mutex_unlock(&client->mtx);
		return OK;
	}

	mutex_unlock(&client->mtx);

	dev = NULL;

#ifdef MODS_HAS_NEW_ACPI_WALK
	dev = pci_get_domain_bus_and_slot(pcidev->domain,
					  pcidev->bus,
					  PCI_DEVFN(pcidev->device,
						    pcidev->function));
#else
	while ((dev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, dev)))
		if (mods_is_pci_dev(dev, pcidev))
			break;
#endif

	if (dev) {
		if (unlikely(mutex_lock_interruptible(&client->mtx))) {
			pci_dev_put(dev);
			return -EINTR;
		}

		if (dev != client->cached_dev) {
			pci_dev_put(client->cached_dev);
			client->cached_dev = pci_dev_get(dev);
		}

		mutex_unlock(&client->mtx);

		err = OK;
	} else
		err = -ENODEV;

	*retdev = dev;
	return err;
}

static int find_pci_dev_impl(struct mods_client            *client,
			     struct MODS_FIND_PCI_DEVICE_2 *p,
			     int                            enum_non_zero_dom)
{
	struct pci_dev *dev   = NULL;
	int             index = -1;

	LOG_ENT();

	cl_debug(DEBUG_PCI,
		 "find pci dev %04x:%04x, index %u\n",
		 p->vendor_id,
		 p->device_id,
		 p->index);

	do {
		dev = pci_get_device(p->vendor_id, p->device_id, dev);
		if (!dev) {
			LOG_EXT();
			return -EINVAL;
		}

		if (enum_non_zero_dom || !pci_domain_nr(dev->bus))
			++index;
	} while (index < (int)(p->index));

	p->pci_device.domain   = pci_domain_nr(dev->bus);
	p->pci_device.bus      = dev->bus->number;
	p->pci_device.device   = PCI_SLOT(dev->devfn);
	p->pci_device.function = PCI_FUNC(dev->devfn);

	pci_dev_put(dev);
	LOG_EXT();
	return OK;
}

int esc_mods_find_pci_dev_2(struct mods_client            *client,
			    struct MODS_FIND_PCI_DEVICE_2 *p)
{
	return find_pci_dev_impl(client, p, 1);
}

int esc_mods_find_pci_dev(struct mods_client          *client,
			  struct MODS_FIND_PCI_DEVICE *p)
{
	struct MODS_FIND_PCI_DEVICE_2 p2;
	int                           err;

	p2.device_id = p->device_id;
	p2.vendor_id = p->vendor_id;
	p2.index     = p->index;

	err = find_pci_dev_impl(client, &p2, 0);

	if (!err) {
		p->bus_number      = p2.pci_device.bus;
		p->device_number   = p2.pci_device.device;
		p->function_number = p2.pci_device.function;
	}

	return err;
}

static int mods_find_pci_class_code(struct mods_client                *client,
				    struct MODS_FIND_PCI_CLASS_CODE_2 *p,
				    int enum_non_zero_dom)
{
	struct pci_dev *dev   = NULL;
	int             index = -1;

	LOG_ENT();

	cl_debug(DEBUG_PCI,
		 "find pci class code %04x, index %u\n",
		 p->class_code,
		 p->index);

	do {
		dev = pci_get_class(p->class_code, dev);
		if (!dev) {
			LOG_EXT();
			return -EINVAL;
		}

		if (enum_non_zero_dom || !pci_domain_nr(dev->bus))
			++index;
	} while (index < (int)(p->index));

	p->pci_device.domain   = pci_domain_nr(dev->bus);
	p->pci_device.bus      = dev->bus->number;
	p->pci_device.device   = PCI_SLOT(dev->devfn);
	p->pci_device.function = PCI_FUNC(dev->devfn);

	pci_dev_put(dev);
	LOG_EXT();
	return OK;
}

int esc_mods_find_pci_class_code_2(struct mods_client                *client,
				   struct MODS_FIND_PCI_CLASS_CODE_2 *p)
{
	return mods_find_pci_class_code(client, p, 1);
}

int esc_mods_find_pci_class_code(struct mods_client              *client,
				 struct MODS_FIND_PCI_CLASS_CODE *p)
{
	struct MODS_FIND_PCI_CLASS_CODE_2 p2;
	int                               err;

	p2.class_code = p->class_code;
	p2.index      = p->index;

	err = mods_find_pci_class_code(client, &p2, 0);

	if (!err) {
		p->bus_number      = p2.pci_device.bus;
		p->device_number   = p2.pci_device.device;
		p->function_number = p2.pci_device.function;
	}

	return err;
}

int esc_mods_pci_get_bar_info_2(struct mods_client             *client,
				struct MODS_PCI_GET_BAR_INFO_2 *p)
{
	struct pci_dev *dev;
	unsigned int bar_resource_offset;
	unsigned int i;
	int err;
#if !defined(MODS_HAS_IORESOURCE_MEM_64)
	__u32 temp;
#endif

	LOG_ENT();

	err = mods_find_pci_dev(client, &p->pci_device, &dev);
	if (unlikely(err)) {
		LOG_EXT();
		return err;
	}

	cl_debug(DEBUG_PCI,
		 "pci get bar info dev %04x:%02x:%02x:%x, bar index %u\n",
		 p->pci_device.domain,
		 p->pci_device.bus,
		 p->pci_device.device,
		 p->pci_device.function,
		 p->bar_index);

#if defined(CONFIG_PPC64)
	if (unlikely(mutex_lock_interruptible(mods_get_irq_mutex()))) {
		pci_dev_put(dev);
		LOG_EXT();
		return -EINTR;
	}

	/* Enable device on the PCI bus */
	err = mods_enable_device(client, dev, NULL);
	if (err) {
		cl_error("unable to enable dev %04x:%02x:%02x.%x\n",
			 p->pci_device.domain,
			 p->pci_device.bus,
			 p->pci_device.device,
			 p->pci_device.function);
		mutex_unlock(mods_get_irq_mutex());
		pci_dev_put(dev);
		LOG_EXT();
		return err;
	}

	mutex_unlock(mods_get_irq_mutex());
#endif

	bar_resource_offset = 0;
	for (i = 0; i < p->bar_index; i++) {
#if defined(MODS_HAS_IORESOURCE_MEM_64)
		if (pci_resource_flags(dev, bar_resource_offset)
		    & IORESOURCE_MEM_64) {
#else
		pci_read_config_dword(dev,
				      (PCI_BASE_ADDRESS_0
				       + (bar_resource_offset * 4)),
				      &temp);
		if (temp & PCI_BASE_ADDRESS_MEM_TYPE_64) {
#endif
			bar_resource_offset += 2;
		} else {
			bar_resource_offset += 1;
		}
	}
	p->base_address = pci_resource_start(dev, bar_resource_offset);
	p->bar_size	= pci_resource_len(dev, bar_resource_offset);

	pci_dev_put(dev);
	LOG_EXT();
	return OK;
}

int esc_mods_pci_get_bar_info(struct mods_client           *client,
			      struct MODS_PCI_GET_BAR_INFO *p)
{
	int err;
	struct MODS_PCI_GET_BAR_INFO_2 get_bar_info = { {0} };

	get_bar_info.pci_device.domain	 = 0;
	get_bar_info.pci_device.bus	 = p->pci_device.bus;
	get_bar_info.pci_device.device	 = p->pci_device.device;
	get_bar_info.pci_device.function = p->pci_device.function;
	get_bar_info.bar_index		 = p->bar_index;

	err = esc_mods_pci_get_bar_info_2(client, &get_bar_info);

	if (likely(!err)) {
		p->base_address	= get_bar_info.base_address;
		p->bar_size	= get_bar_info.bar_size;
	}

	return err;
}

int esc_mods_pci_get_irq_2(struct mods_client        *client,
			   struct MODS_PCI_GET_IRQ_2 *p)
{
	struct pci_dev *dev;
	int err;

	LOG_ENT();

	err = mods_find_pci_dev(client, &p->pci_device, &dev);
	if (unlikely(err)) {
		LOG_EXT();
		return err;
	}

	cl_debug(DEBUG_PCI,
		 "pci get irq dev %04x:%02x:%02x:%x irq=%u\n",
		 p->pci_device.domain,
		 p->pci_device.bus,
		 p->pci_device.device,
		 p->pci_device.function,
		 dev->irq);

	p->irq = dev->irq;

	pci_dev_put(dev);
	LOG_EXT();
	return OK;
}

int esc_mods_pci_get_irq(struct mods_client      *client,
			 struct MODS_PCI_GET_IRQ *p)
{
	int err;
	struct MODS_PCI_GET_IRQ_2 get_irq = { {0} };

	get_irq.pci_device.domain   = 0;
	get_irq.pci_device.bus	    = p->pci_device.bus;
	get_irq.pci_device.device   = p->pci_device.device;
	get_irq.pci_device.function = p->pci_device.function;

	err = esc_mods_pci_get_irq_2(client, &get_irq);

	if (likely(!err))
		p->irq = get_irq.irq;

	return err;
}

int esc_mods_pci_read_2(struct mods_client *client, struct MODS_PCI_READ_2 *p)
{
	struct pci_dev *dev;
	int err;
	int dbdf;

	LOG_ENT();

	err = mods_find_pci_dev(client, &p->pci_device, &dev);
	if (unlikely(err)) {
		LOG_EXT();
		return err;
	}

	p->data = 0;
	switch (p->data_size) {
	case 1: {
			u8 value;

			pci_read_config_byte(dev, p->address, &value);
			p->data = value;
		}
		break;
	case 2: {
			u16 value;

			pci_read_config_word(dev, p->address, &value);
			p->data = value;
		}
		break;
	case 4:
		pci_read_config_dword(dev, p->address, (u32 *) &p->data);
		break;
	default:
		err = -EINVAL;
		break;
	}

	cl_debug(DEBUG_PCI | DEBUG_DETAILED,
		 "pci read dev %04x:%02x:%02x.%x, addr 0x%04x, size %u, data 0x%x\n",
		 p->pci_device.domain,
		 p->pci_device.bus,
		 p->pci_device.device,
		 p->pci_device.function,
		 p->address,
		 p->data_size,
		 p->data);

	dbdf = (int)(((u32)p->pci_device.domain << 16) |
		     ((u32)(p->pci_device.bus & 0xFFU) << 8) |
		     (u32)(p->pci_device.device & 0xFFU));

	/* Usually one of the first reads from PCI config space occurs
	 * at address 0 and or 2 to read PCI device vendor/id.
	 * If this reads all Fs, the device probably fell off the bus.
	 */
	if (p->address <= 4 && (p->data == ~0U || p->data == 0xFFFFU)) {
		if (dbdf != atomic_read(&client->last_bad_dbdf))
			cl_warn("pci read dev %04x:%02x:%02x.%x, addr 0x%04x, size %u, data 0x%x\n",
				p->pci_device.domain,
				p->pci_device.bus,
				p->pci_device.device,
				p->pci_device.function,
				p->address,
				p->data_size,
				p->data);
		atomic_set(&client->last_bad_dbdf, dbdf);
	} else if (dbdf == atomic_read(&client->last_bad_dbdf))
		atomic_set(&client->last_bad_dbdf, -1);

	pci_dev_put(dev);
	LOG_EXT();
	return err;
}

int esc_mods_pci_read(struct mods_client *client, struct MODS_PCI_READ *p)
{
	int err;
	struct MODS_PCI_READ_2 pci_read = { {0} };

	pci_read.pci_device.domain	= 0;
	pci_read.pci_device.bus		= p->bus_number;
	pci_read.pci_device.device	= p->device_number;
	pci_read.pci_device.function	= p->function_number;
	pci_read.address		= p->address;
	pci_read.data_size		= p->data_size;

	err = esc_mods_pci_read_2(client, &pci_read);

	if (likely(!err))
		p->data = pci_read.data;

	return err;
}

int esc_mods_pci_write_2(struct mods_client *client, struct MODS_PCI_WRITE_2 *p)
{
	struct pci_dev *dev;
	int err;

	LOG_ENT();

	cl_debug(DEBUG_PCI | DEBUG_DETAILED,
		 "pci write dev %04x:%02x:%02x.%x, addr 0x%04x, size %u, data 0x%x\n",
		 p->pci_device.domain,
		 p->pci_device.bus,
		 p->pci_device.device,
		 p->pci_device.function,
		 p->address,
		 p->data_size,
		 p->data);

	err = mods_find_pci_dev(client, &p->pci_device, &dev);
	if (unlikely(err)) {
		if (err == -ENODEV)
			cl_error("dev %04x:%02x:%02x.%x not found\n",
				 p->pci_device.domain,
				 p->pci_device.bus,
				 p->pci_device.device,
				 p->pci_device.function);
		LOG_EXT();
		return err;
	}

	switch (p->data_size) {
	case 1:
		pci_write_config_byte(dev, p->address, p->data);
		break;
	case 2:
		pci_write_config_word(dev, p->address, p->data);
		break;
	case 4:
		pci_write_config_dword(dev, p->address, p->data);
		break;
	default:
		err = -EINVAL;
		break;
	}

	pci_dev_put(dev);
	LOG_EXT();
	return err;
}

int esc_mods_pci_write(struct mods_client    *client,
		       struct MODS_PCI_WRITE *p)
{
	struct MODS_PCI_WRITE_2 pci_write = { {0} };

	pci_write.pci_device.domain	= 0;
	pci_write.pci_device.bus	= p->bus_number;
	pci_write.pci_device.device	= p->device_number;
	pci_write.pci_device.function	= p->function_number;
	pci_write.address		= p->address;
	pci_write.data			= p->data;
	pci_write.data_size		= p->data_size;

	return esc_mods_pci_write_2(client, &pci_write);
}

int esc_mods_pci_bus_add_dev(struct mods_client              *client,
			     struct MODS_PCI_BUS_ADD_DEVICES *scan)
{
	struct MODS_PCI_BUS_RESCAN rescan = { 0, scan->bus };

	return esc_mods_pci_bus_rescan(client, &rescan);
}

int esc_mods_pci_bus_rescan(struct mods_client         *client,
			    struct MODS_PCI_BUS_RESCAN *rescan)
{
#ifndef MODS_HASNT_PCI_RESCAN_BUS
	struct pci_bus *bus;
	int    err = OK;

	LOG_ENT();

	cl_info("scanning pci bus %04x:%02x\n", rescan->domain, rescan->bus);

	bus = pci_find_bus(rescan->domain, rescan->bus);

	if (likely(bus)) {
#ifndef MODS_HASNT_PCI_LOCK_RESCAN_REMOVE
		pci_lock_rescan_remove();
#endif
		pci_rescan_bus(bus);
#ifndef MODS_HASNT_PCI_LOCK_RESCAN_REMOVE
		pci_unlock_rescan_remove();
#endif
	} else {
		cl_error("bus %04x:%02x not found\n",
			 rescan->domain,
			 rescan->bus);
		err = -EINVAL;
	}

	LOG_EXT();

	return err;
#else
	return -EINVAL;
#endif
}

int esc_mods_pci_bus_remove_dev(struct mods_client             *client,
				struct MODS_PCI_BUS_REMOVE_DEV *p)
{
#if !defined(MODS_HASNT_PCI_BUS_REMOVE_DEV)
	struct pci_dev *dev;
	int err;

	LOG_ENT();

	err = mods_find_pci_dev(client, &p->pci_device, &dev);
	if (unlikely(err)) {
		if (err == -ENODEV)
			cl_error(
				"pci_remove cannot find dev %04x:%02x:%02x.%x\n",
				p->pci_device.domain,
				p->pci_device.bus,
				p->pci_device.device,
				p->pci_device.function);
		LOG_EXT();
		return err;
	}

	cl_debug(DEBUG_PCI,
		 "pci remove on dev %04x:%02x:%02x.%x\n",
		 p->pci_device.domain,
		 p->pci_device.bus,
		 p->pci_device.device,
		 p->pci_device.function);

	pci_stop_and_remove_bus_device(dev);
	LOG_EXT();
	return err;
#else
	return -EINVAL;
#endif
}

/************************
 * PIO ESCAPE FUNCTIONS *
 ************************/

int esc_mods_pio_read(struct mods_client *client, struct MODS_PIO_READ *p)
{
	LOG_ENT();
	switch (p->data_size) {
	case 1:
		p->data = inb(p->port);
		break;
	case 2:
		p->data = inw(p->port);
		break;
	case 4:
		p->data = inl(p->port);
		break;
	default:
		return -EINVAL;
	}
	LOG_EXT();
	return OK;
}

int esc_mods_pio_write(struct mods_client *client, struct MODS_PIO_WRITE  *p)
{
	LOG_ENT();
	switch (p->data_size) {
	case 1:
		outb(p->data, p->port);
		break;
	case 2:
		outw(p->data, p->port);
		break;
	case 4:
		outl(p->data, p->port);
		break;
	default:
		return -EINVAL;
	}
	LOG_EXT();
	return OK;
}

int esc_mods_device_numa_info_3(struct mods_client             *client,
				struct MODS_DEVICE_NUMA_INFO_3 *p)
{
	struct pci_dev *dev;
	int err;

	LOG_ENT();

	err = mods_find_pci_dev(client, &p->pci_device, &dev);
	if (unlikely(err)) {
		if (err == -ENODEV)
			cl_error("dev %04x:%02x:%02x.%x not found\n",
				 p->pci_device.domain,
				 p->pci_device.bus,
				 p->pci_device.device,
				 p->pci_device.function);
		LOG_EXT();
		return err;
	}

	p->node = dev_to_node(&dev->dev);
	if (p->node != -1) {
		u32                  first_offset = ~0U;
		unsigned int         i;
		const unsigned long *maskp;

		maskp = cpumask_bits(cpumask_of_node(p->node));

		memset(&p->node_cpu_mask, 0, sizeof(p->node_cpu_mask));

		for (i = 0; i < nr_cpumask_bits; i += 32) {

			const u32 word     = i / BITS_PER_LONG;
			const u32 bit      = i % BITS_PER_LONG;
			const u32 cur_mask = (u32)(maskp[word] >> bit);
			u32       mask_idx;

			if (first_offset == ~0U) {
				if (cur_mask) {
					first_offset             = i / 32;
					p->first_cpu_mask_offset = first_offset;
				} else
					continue;
			}

			mask_idx = (i / 32) - first_offset;

			if (cur_mask && mask_idx >= MAX_CPU_MASKS_3) {

				cl_error("too many CPUs (%d) for mask bits\n",
					 nr_cpumask_bits);
				pci_dev_put(dev);
				LOG_EXT();
				return -EINVAL;
			}

			if (mask_idx < MAX_CPU_MASKS_3)
				p->node_cpu_mask[mask_idx] = cur_mask;
		}

		if (first_offset == ~0U)
			p->first_cpu_mask_offset = 0;
	}
	p->node_count = num_possible_nodes();
	p->cpu_count  = num_possible_cpus();

	pci_dev_put(dev);
	LOG_EXT();
	return OK;
}

int esc_mods_device_numa_info_2(struct mods_client             *client,
				struct MODS_DEVICE_NUMA_INFO_2 *p)
{
	int err;
	struct MODS_DEVICE_NUMA_INFO_3 numa_info = { {0} };

	numa_info.pci_device = p->pci_device;

	err = esc_mods_device_numa_info_3(client, &numa_info);

	if (likely(!err)) {
		int i;

		p->node	      = numa_info.node;
		p->node_count = numa_info.node_count;
		p->cpu_count  = numa_info.cpu_count;

		memset(&p->node_cpu_mask, 0, sizeof(p->node_cpu_mask));

		for (i = 0; i < MAX_CPU_MASKS_3; i++) {

			const u32 cur_mask = numa_info.node_cpu_mask[i];
			const u32 dst      = i +
					     numa_info.first_cpu_mask_offset;

			if (cur_mask && dst >= MAX_CPU_MASKS) {
				cl_error("too many CPUs (%d) for mask bits\n",
					 nr_cpumask_bits);
				err = -EINVAL;
				break;
			}

			if (dst < MAX_CPU_MASKS)
				p->node_cpu_mask[dst]
					= numa_info.node_cpu_mask[i];
		}
	}

	return err;
}

int esc_mods_device_numa_info(struct mods_client           *client,
			      struct MODS_DEVICE_NUMA_INFO *p)
{
	int err;
	struct MODS_DEVICE_NUMA_INFO_3 numa_info = { {0} };

	numa_info.pci_device.domain    = 0;
	numa_info.pci_device.bus       = p->pci_device.bus;
	numa_info.pci_device.device    = p->pci_device.device;
	numa_info.pci_device.function  = p->pci_device.function;

	err = esc_mods_device_numa_info_3(client, &numa_info);

	if (likely(!err)) {
		int i;

		p->node	      = numa_info.node;
		p->node_count = numa_info.node_count;
		p->cpu_count  = numa_info.cpu_count;

		memset(&p->node_cpu_mask, 0, sizeof(p->node_cpu_mask));

		for (i = 0; i < MAX_CPU_MASKS_3; i++) {

			const u32 cur_mask = numa_info.node_cpu_mask[i];
			const u32 dst      = i +
					     numa_info.first_cpu_mask_offset;

			if (cur_mask && dst >= MAX_CPU_MASKS) {
				cl_error("too many CPUs (%d) for mask bits\n",
					 nr_cpumask_bits);
				err = -EINVAL;
				break;
			}

			if (dst < MAX_CPU_MASKS)
				p->node_cpu_mask[dst]
					= numa_info.node_cpu_mask[i];
		}
	}

	return err;
}

int esc_mods_get_iommu_state(struct mods_client          *client,
			     struct MODS_GET_IOMMU_STATE *state)
{
	int err = esc_mods_get_iommu_state_2(client, state);

	if (!err)
		state->state = (state->state == MODS_SWIOTLB_DISABLED) ? 1 : 0;

	return err;
}

int esc_mods_get_iommu_state_2(struct mods_client          *client,
			       struct MODS_GET_IOMMU_STATE *state)
{
#if !defined(CONFIG_SWIOTLB)
	state->state = MODS_SWIOTLB_DISABLED;
#elif defined(MODS_HAS_DMA_OPS)

	const struct dma_map_ops *ops;
	struct pci_dev           *dev;
	int                       err;

	LOG_ENT();

	err = mods_find_pci_dev(client, &state->pci_device, &dev);
	if (unlikely(err)) {
		LOG_EXT();
		return err;
	}

	ops = get_dma_ops(&dev->dev);

	state->state = ops->map_sg != swiotlb_map_sg_attrs
		       ? MODS_SWIOTLB_DISABLED : MODS_SWIOTLB_ACTIVE;

	pci_dev_put(dev);
	LOG_EXT();

#else
	/* No way to detect it */
	state->state = MODS_SWIOTLB_INDETERMINATE;
#endif
	return OK;
}

int esc_mods_pci_set_dma_mask(struct mods_client       *client,
			      struct MODS_PCI_DMA_MASK *dma_mask)
{
	int             err;
	struct pci_dev *dev;
	u64             mask;

	LOG_ENT();

	if (unlikely(dma_mask->num_bits > 64)) {
		cl_error("num_bits=%u exceeds 64\n",
			 dma_mask->num_bits);
		LOG_EXT();
		return -EINVAL;
	}

	err = mods_find_pci_dev(client, &dma_mask->pci_device, &dev);
	if (unlikely(err)) {
		if (err == -ENODEV)
			cl_error("dev %04x:%02x:%02x.%x not found\n",
				 dma_mask->pci_device.domain,
				 dma_mask->pci_device.bus,
				 dma_mask->pci_device.device,
				 dma_mask->pci_device.function);
		LOG_EXT();
		return err;
	}

	mask = dma_mask->num_bits == 64 ? ~0ULL : (1ULL<<dma_mask->num_bits)-1;

	err = pci_set_dma_mask(dev, mask);
	if (err) {
		cl_error(
			"failed to set dma mask 0x%llx (%u) for dev %04x:%02x:%02x.%x\n",
			mask,
			dma_mask->num_bits,
			dma_mask->pci_device.domain,
			dma_mask->pci_device.bus,
			dma_mask->pci_device.device,
			dma_mask->pci_device.function);
#if defined(CONFIG_PPC64)
		/* Ignore error if TCE bypass is on */
		if (dev->dma_mask == ~0ULL)
			err = OK;
#endif
	} else {
		err = pci_set_consistent_dma_mask(dev, mask);
		if (err)
			cl_error(
				"failed to set consistent dma mask 0x%llx (%u) for dev %04x:%02x:%02x.%x\n",
				mask,
				dma_mask->num_bits,
				dma_mask->pci_device.domain,
				dma_mask->pci_device.bus,
				dma_mask->pci_device.device,
				dma_mask->pci_device.function);
	}

	if (!err)
		cl_info("set dma mask %u for dev %04x:%02x:%02x.%x\n",
			dma_mask->num_bits,
			dma_mask->pci_device.domain,
			dma_mask->pci_device.bus,
			dma_mask->pci_device.device,
			dma_mask->pci_device.function);

	pci_dev_put(dev);
	LOG_EXT();
	return err;
}

int esc_mods_pci_reset_function(struct mods_client    *client,
				struct mods_pci_dev_2 *pcidev)
{
#if defined(MODS_HAS_FLR_SUPPORT)
	int             err;
	struct pci_dev *dev;
	u32             cap;
	const struct pci_error_handlers *err_handler;

	LOG_ENT();

	err = mods_find_pci_dev(client, pcidev, &dev);
	if (unlikely(err)) {
		if (err == -ENODEV)
			cl_error("dev %04x:%02x:%02x.%x not found\n",
				 pcidev->domain,
				 pcidev->bus,
				 pcidev->device,
				 pcidev->function);
		LOG_EXT();
		return err;
	}

	pcie_capability_read_dword(dev, PCI_EXP_DEVCAP, &cap);
	if ((dev->dev_flags & PCI_DEV_FLAGS_NO_FLR_RESET) ||
	    !(cap & PCI_EXP_DEVCAP_FLR)) {
		cl_error(
			 "function level reset not supported on dev %04x:%02x:%02x.%x\n",
			 pcidev->domain,
			 pcidev->bus,
			 pcidev->device,
			 pcidev->function);
		err = -ENOTTY;
		goto error;
	}

	pci_cfg_access_lock(dev);
	device_lock(&dev->dev);

	err_handler = dev->driver ? dev->driver->err_handler : NULL;
	if (err_handler && err_handler->reset_prepare)
		err_handler->reset_prepare(dev);

	pci_set_power_state(dev, PCI_D0);
	pci_save_state(dev);
	pci_write_config_word(dev, PCI_COMMAND, PCI_COMMAND_INTX_DISABLE);

#if defined(MODS_PCIE_FLR_HAS_ERR)
	err = pcie_flr(dev);

	if (unlikely(err))
		cl_error("pcie_flr failed on dev %04x:%02x:%02x.%x\n",
			 pcidev->domain,
			 pcidev->bus,
			 pcidev->device,
			 pcidev->function);
#else
	pcie_flr(dev);
#endif

	if (!err)
		cl_info("pcie_flr succeeded on dev %04x:%02x:%02x.%x\n",
			pcidev->domain,
			pcidev->bus,
			pcidev->device,
			pcidev->function);

	pci_restore_state(dev);

	if (err_handler && err_handler->reset_done)
		err_handler->reset_done(dev);

	device_unlock(&dev->dev);
	pci_cfg_access_unlock(dev);
error:
	pci_dev_put(dev);
	LOG_EXT();
	return err;
#else
	return -EINVAL;
#endif
}
