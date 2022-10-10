/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef __PCI_CLIENT_H__
#define __PCI_CLIENT_H__

#include <uapi/misc/nvscic2c-pcie-ioctl.h>

#include "common.h"
#include "module.h"

/* forward declaration.*/
struct vm_area_struct;
struct dma_buf;
struct dma_buf_attachment;

/*
 * PCI client initialization parameters. The fields must remain persistent
 * till deinitialization (exit).
 */
struct pci_client_params {
	struct dma_buff_t *self_mem;
	struct pci_aper_t *peer_mem;
	/*
	 * @DRV_MODE_EPC: &pci_dev->dev
	 * @DRV_MODE_EPF: epf->epc->dev.parent.
	 */
	struct device *dev;
};

/* Initialize PCI client either for @DRV_MODE_EPF or @DRV_MODE_EPC. */
int
pci_client_init(struct pci_client_params *params, void **pci_client_h);

/* Teardown of PCI client. */
void
pci_client_deinit(void **pci_client_h);

/* reserve iova using the iova-manager.*/
int
pci_client_alloc_iova(void *pci_client_h, size_t size, u64 *address,
		      size_t *offset, void **block_h);

/* free the reserved iova.*/
int
pci_client_free_iova(void *pci_client_h, void **block_h);

int
pci_client_map_addr(void *pci_client_h, u64 to_iova, phys_addr_t paddr,
		    size_t size, int prot);

size_t
pci_client_unmap_addr(void *pci_client_h, u64 from_iova, size_t size);

/* get the pci aperture for a given offset.*/
int
pci_client_get_peer_aper(void *pci_client_h, size_t offsetof, size_t size,
			 phys_addr_t *phys_addr);

/* attach dma-buf to pci device.*/
struct dma_buf_attachment *
pci_client_dmabuf_attach(void *pci_client_h, struct dma_buf *dmabuff);

/* detach dma-buf to pci device.*/
void
pci_client_dmabuf_detach(void *pci_client_h, struct dma_buf *dmabuff,
			 struct dma_buf_attachment *attach);
/*
 * Users/Units can register for PCI link events as received by
 * @DRV_MODE_EPF or @DRV_MODE_EPC module sbstraction.
 */
int
pci_client_register_for_link_event(void *pci_client_h,
				   struct callback_ops *ops, u32 *id);

/* Unregister for PCI link events. - teardown only. */
int
pci_client_unregister_for_link_event(void *pci_client_h, u32 id);

/*
 * Update the PCI link status as received in @DRV_MODE_EPF or @DRV_MODE_EPC
 * module abstraction. Propagate the link status to registered users.
 */
int
pci_client_change_link_status(void *pci_client_h,
			      enum nvscic2c_pcie_link status);

/* Helper function to mmap the PCI link status memory to user-space.*/
int
pci_client_mmap_link_mem(void *pci_client_h, struct vm_area_struct *vma);

/* Query PCI link status. */
enum nvscic2c_pcie_link
pci_client_query_link_status(void *pci_client_h);

/*
 * Helper functions to set and set driver context from pci_client_h
 */

/* Save driver context of DRV_MODE_EPF or DRV_MODE_EPC */
int
pci_client_save_driver_ctx(void *pci_client_h, struct driver_ctx_t *drv_ctx);

/* Getter driver context of DRV_MODE_EPF or DRV_MODE_EPC */
struct driver_ctx_t *
pci_client_get_driver_ctx(void *pci_client_h);
/*getter drv mode   */
enum drv_mode_t
pci_client_get_drv_mode(void *pci_client_h);
/* save peer cpu sent by boottrap msg  */
int
pci_client_save_peer_cpu(void *pci_client_h, enum peer_cpu_t peer_cpu);

/* Getter the soc/arch of rp */
enum peer_cpu_t
pci_client_get_peer_cpu(void *pci_client_h);

/* Get allocated edma rx desc iova  */
dma_addr_t
pci_client_get_edma_rx_desc_iova(void *pci_client_h);

/* pci client raise irq to rp */
int
pci_client_raise_irq(void *pci_client_h, enum pci_epc_irq_type type, u16 num);
#endif // __PCI_CLIENT_H__
