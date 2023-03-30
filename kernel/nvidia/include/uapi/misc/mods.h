/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * mods.h - This file is part of NVIDIA MODS kernel driver.
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

#ifndef _UAPI_MODS_H_
#define _UAPI_MODS_H_

#include <linux/types.h>

/* Driver version */
#define MODS_DRIVER_VERSION_MAJOR 4
#define MODS_DRIVER_VERSION_MINOR 8
#define MODS_DRIVER_VERSION ((MODS_DRIVER_VERSION_MAJOR << 8) | \
			     ((MODS_DRIVER_VERSION_MINOR / 10) << 4) | \
			     (MODS_DRIVER_VERSION_MINOR % 10))

/* Structures defined below are used with ioctls.  We force 1-byte packing so
 * that the structures are compatible between architectures (e.g. 32-bit and
 * 64-bit userland) and compilers.
 *
 * The structure members are manually ordered in such a way that all members are
 * naturally aligned, e.g. 32-bit data members are aligned on 4 bytes and
 * so on.
 */
#pragma pack(push, 1)

/* Used by ioctls:
 * - MODS_ESC_ACQUIRE_ACCESS_TOKEN,
 * - MODS_ESC_RELEASE_ACCESS_TOKEN
 * - MODS_ESC_VERIFY_ACCESS_TOKEN
 *
 * These ioctls are used to establish a session for known clients of the
 * driver.  This is to prevent another random client to connect to the driver
 * while it is in use.  This also allows multiple clients to connect to the
 * driver and use it.  Without this, only one client can use the driver at
 * any time.  (Unless the driver has been loaded with multi_instance=1
 * option, which is deprecated.)
 *
 * The first client has to make a MODS_ESC_ACQUIRE_ACCESS_TOKEN request.
 * This returns a new token generated randomly.
 *
 * Further clients can open the driver and communicate with it, but the first
 * request of any such client after opening the device node must be
 * MODS_ESC_VERIFY_ACCESS_TOKEN and the client needs to provide a token which
 * has already been established via MODS_ESC_ACQUIRE_ACCESS_TOKEN, otherwise
 * all requests to the driver will fail.
 *
 * After the last client is done, it has to issue a
 * MODS_ESC_RELEASE_ACCESS_TOKEN ioctl with the abovementioned token value
 * in order to release the token.  Then other clients will be able to connect
 * and start again.
 */
struct MODS_ACCESS_TOKEN {
	/* IN/OUT */
	__u32 token;
};

/* Special value used to indicate that access token has not been set */
#define MODS_ACCESS_TOKEN_NONE ~0U

/* PCI device location */
struct mods_pci_dev_2 {
	__u16 domain;
	__u16 bus;
	__u16 device;
	__u16 function;
};

/* PCI device location (legacy) */
struct mods_pci_dev {
	__u16 bus;
	__u8  device;
	__u8  function;
};

/* Used by MODS_ESC_ALLOC_PAGES_2 ioctl.
 *
 * It is used to allocate system memory as physical CPU pages.
 *
 * The memory can be allocated as contiguous or non-contiguous, albeit
 * the kernel limits how much contiguous memory can be allocated in
 * one shot (typically max is around 2MB).
 *
 * The memory can be allocated from any NUMA node, from the selected
 * NUMA node or from the NUMA node corresponding to the specified
 * PCI device.
 *
 * If specified using one of the flags defined below, the memory can
 * be automatically mapped to the PCI device using IOMMU, so that no
 * additional ioctls are needed.
 *
 * The caching flags passed to this ioctl apply only to the CPU pages
 * and are in effect when the memory is mapped with mmap().
 *
 * Here are common operations on memory allocated with this ioctl:
 * - MODS_ESC_DMA_MAP_MEMORY: maps memory to device via IOMMU.
 * - MODS_ESC_GET_PHYSICAL_ADDRESS_2: retrieves physical addresses.
 * - mmap(): maps memory to userspace.
 * - MODS_ESC_FREE_PAGES: frees the memory.
 */
struct MODS_ALLOC_PAGES_2 {
	/* IN */
	__u64                 num_bytes;
	__u32                 flags; /* MODS_ALLOC_* flags */
	__s32                 numa_node;
	struct mods_pci_dev_2 pci_device;

	/* OUT */
	__u64                 memory_handle;
};

/* Special value for numa_node member above to indicate that memory can be
 * allocated on any NUMA node.
 */
#define MODS_ANY_NUMA_NODE (-1)

/* Bit flags for the flags member above */
#define MODS_ALLOC_CACHED        0   /* Default WB cache attr */
#define MODS_ALLOC_UNCACHED      1   /* UC cache attr */
#define MODS_ALLOC_WRITECOMBINE  2   /* WC cache attr */
#define MODS_ALLOC_CACHE_MASK    7U  /* The first three bits are cache attr */

#define MODS_ALLOC_DMA32         8   /* Force 32-bit PA, else any PA */
#define MODS_ALLOC_CONTIGUOUS    16  /* Force contiguous, else non-contig */
#define MODS_ALLOC_USE_NUMA      32  /* Use numa_node member instead of PCI dev
				      * for NUMA node hint
				      */
#define MODS_ALLOC_FORCE_NUMA    64  /* Force memory to be from a given NUMA
				      * node (specified by PCI dev or
				      * numa_node).  Otherwise use PCI dev or
				      * numa_node as a hint only.
				      */
#define MODS_ALLOC_MAP_DEV       128 /* DMA map to PCI device */

/* Used by MODS_ESC_ALLOC_PAGES ioctl */
struct MODS_ALLOC_PAGES {
	/* IN */
	__u32 num_bytes;
	__u32 contiguous;
	__u32 address_bits;
	__u32 attrib;

	/* OUT */
	__u64 memory_handle;
};

/* Used by legacy MODS_ESC_DEVICE_ALLOC_PAGES_2 ioctl */
struct MODS_DEVICE_ALLOC_PAGES_2 {
	/* IN */
	__u32                 num_bytes;
	__u32                 contiguous;
	__u32                 address_bits;
	__u32                 attrib;
	struct mods_pci_dev_2 pci_device;

	/* OUT */
	__u64                 memory_handle;
};

/* Used by legacy MODS_ESC_DEVICE_ALLOC_PAGES ioctl */
struct MODS_DEVICE_ALLOC_PAGES {
	/* IN */
	__u32		    num_bytes;
	__u32		    contiguous;
	__u32		    address_bits;
	__u32		    attrib;
	struct mods_pci_dev pci_device;

	/* OUT */
	__u64		    memory_handle;
};

/* Used by MODS_ESC_FREE_PAGES ioctl.
 *
 * Frees pages allocated using any of the calls described above.
 *
 * Note that if the memory allocated earlier has been mmapped() to
 * userland process, it will still be usable, even if it is freed
 * with this ioctl or even if the driver is closed.
 *
 * Otherwise all allocated memory is automatically freed when the
 * driver is closed for whatever reason (including a crash).
 */
struct MODS_FREE_PAGES {
	/* IN */
	__u64	memory_handle;
};

#define MODS_MAX_MERGE_HANDLES 64

/* Used by MODS_ESC_MERGE_PAGES ioctl.
 *
 * This can be used to merge multiple, adjacent allocations into one.
 *
 * The memory regions described by the input handles must be physically
 * adjacent (i.e. adjacent in physical address space).
 */
struct MODS_MERGE_PAGES {
	/* IN */
	__u64 in_memory_handles[MODS_MAX_MERGE_HANDLES];
	__u32 num_in_handles;
	__u32 dummy_align;

	/* OUT */
	__u64 memory_handle;
};

/* Used by legacy ioctls:
 * - MODS_ESC_GET_PHYSICAL_ADDRESS
 * - MODS_ESC_GET_MAPPED_PHYSICAL_ADDRESS
 */
struct MODS_GET_PHYSICAL_ADDRESS {
	/* IN */
	__u64	memory_handle;
	__u32	offset;

	/* OUT */
	__u64	physical_address;
};

/* Used by legacy MODS_ESC_GET_MAPPED_PHYSICAL_ADDRESS_2 ioctl */
struct MODS_GET_PHYSICAL_ADDRESS_2 {
	/* IN */
	__u64                 memory_handle;
	__u32	              offset;
	struct mods_pci_dev_2 pci_device;

	/* OUT */
	__u64                 physical_address;
};

/* Used by ioctls:
 * - MODS_ESC_GET_PHYSICAL_ADDRESS_2
 * - MODS_ESC_GET_MAPPED_PHYSICAL_ADDRESS_3
 *
 * MODS_ESC_GET_PHYSICAL_ADDRESS_2 retrieves physical address for a given
 * memory allocation.
 *
 * MODS_ESC_GET_MAPPED_PHYSICAL_ADDRESS_3 retrieves IO virtual address
 * (DMA address) usable with the specified PCI device for a given memory
 * allocation.
 */
struct MODS_GET_PHYSICAL_ADDRESS_3 {
	/* IN */
	__u64                 memory_handle;
		/* Page offset from the beginning of the allocation */
	__u64                 offset;
		/* PCI device only for MODS_ESC_GET_MAPPED_PHYSICAL_ADDRESS_3 */
	struct mods_pci_dev_2 pci_device;

	/* OUT */
	__u64                 physical_address;
};

/* Used by MODS_ESC_DMABUF_GET_PHYSICAL_ADDRESS ioctl.
 *
 * Retrieves physical address for a given dmabuf surface at a given offset.
 *
 * Available only on Tegra.
 */
struct MODS_DMABUF_GET_PHYSICAL_ADDRESS {
	/* IN */
	__s32 buf_fd;
	__u32 padding;
	__u64 offset;

	/* OUT */
	__u64 physical_address;
	__u64 segment_size;
};

/* Used by MODS_ESC_VIRTUAL_TO_PHYSICAL ioctl.
 *
 * Converts a given userspace virtual address obtained with mmap() on /dev/mods
 * to a system physical address.
 *
 * This call is discouraged, because it is expensive and when IOMMU is
 * enabled, it is not very useful.  Use MODS_ESC_GET_MAPPED_PHYSICAL_ADDRESS_3
 * instead.
 */
struct MODS_VIRTUAL_TO_PHYSICAL {
	/* IN */
	__u64	virtual_address;

	/* OUT */
	__u64	physical_address;
};

/* Used by MODS_ESC_PHYSICAL_TO_VIRTUAL ioctl.
 *
 * Converts a given system physical address to a userspace virtual address
 * obtained with mmap() on /dev/mods.
 *
 * This call is discouraged, because it is expensive and when IOMMU is
 * enabled, it is not very useful.
 */
struct MODS_PHYSICAL_TO_VIRTUAL {
	/* IN */
	__u64	physical_address;

	/* OUT */
	__u64	virtual_address;
};

/* Used by MODS_ESC_FLUSH_CPU_CACHE_RANGE ioctl.
 *
 * This call is available only on ARM architecture and can be used to flush
 * or invalidate cache for memory ranges allocated with WB cache attr.
 *
 * This call is useful only for devices which do not support I/O coherency.
 */
struct MODS_FLUSH_CPU_CACHE_RANGE {
	/* IN */
	__u64 virt_addr_start;
	__u64 virt_addr_end;
	__u32 flags;
};

/* Flags for the flags member above */
#define MODS_FLUSH_CPU_CACHE	  1
#define MODS_INVALIDATE_CPU_CACHE 2

/* Used by ioctls:
 * - MODS_ESC_DMA_MAP_MEMORY
 * - MODS_ESC_DMA_UNMAP_MEMORY
 *
 * MODS_ESC_DMA_MAP_MEMORY maps the specified memory to IOMMU for the specified
 * PCI device.
 *
 * - This is a no-op if the memory has already been mapped.
 *
 * MODS_ESC_DMA_UNMAP_MEMORY unmaps memory from IOMMU for the specified PCI
 * device if it has already been mapped.
 *
 * - This applies only to non-default mappings done with
 *   MODS_ESC_DMA_MAP_MEMORY.
 *
 * - If the memory was mapped to a PCI device during allocation with
 *   MODS_ALLOC_MAP_DEV flag, then this call will not unmap it.
 *
 * - If multiple calls to MODS_ESC_DMA_MAP_MEMORY have been made to map the
 *   same memory region to the same PCI device, the first call to
 *   MODS_ESC_DMA_UNMAP_MEMORY will unmap it, there is no reference counting
 *   or any other protection in place.
 */
struct MODS_DMA_MAP_MEMORY {
	/* IN */
	__u64                 memory_handle;
	struct mods_pci_dev_2 pci_device;
};

/* Used by MODS_ESC_PCI_SET_DMA_MASK ioctl.
 *
 * Tells the kernel that the PCI device supports a DMA address of the given
 * bit width.  By default the kernel assumes that every device supports
 * 32-bit addresses.  If the device is intended to be used with physical
 * addresses or IOMMU virtual addresses exceeding 32 bits, this call must
 * be made to set that up.
 */
struct MODS_PCI_DMA_MASK {
	/* IN */
	struct mods_pci_dev_2 pci_device;
	__u32                 num_bits;
};

/* Used by ioctls:
 * - MODS_ESC_GET_IOMMU_STATE_2
 * - MODS_ESC_GET_IOMMU_STATE (deprecated)
 *
 * MODS_ESC_GET_IOMMU_STATE_2 attempts to determine whether IOMMU
 * has been enabled for the given PCI device.
 *
 * This call returns useful information only on certain kernel versions and
 * certain architectures.
 */
struct MODS_GET_IOMMU_STATE {
	/* IN */
	struct mods_pci_dev_2 pci_device;
	/* OUT */
	__u32                 state;
};

/* Flags for the state member defined above */
#define MODS_SWIOTLB_DISABLED      0 /* Hardware IOMMU may be enabled */
#define MODS_SWIOTLB_ACTIVE        1 /* Hardware IOMMU is disabled */
#define MODS_SWIOTLB_INDETERMINATE 2 /* Unable to tell the state of SWIOTLB */

/* Used by MODS_ESC_FIND_PCI_DEVICE_2 ioctl.
 *
 * Searches for PCI devices with the specified vendor id and device id.
 *
 * The first call to this ioctl is performed with index=0 and then for
 * every successful call index is incremented until this call fails.
 */
struct MODS_FIND_PCI_DEVICE_2 {
	/* IN */
	__u32 device_id;
	__u32 vendor_id;
	__u32 index;

	/* OUT */
	struct mods_pci_dev_2 pci_device;
};

/* Used by MODS_ESC_BPMP_SET_PCIE_STATE ioctl.
 *
 * Set PCIE state through BPMP Uphy driver
 */
struct MODS_SET_PCIE_STATE {
	/* IN */
	__u32 controller;
	__u32 enable;
};

/* Used by MODS_ESC_BPMP_INIT_PCIE_EP_PLL ioctl.
 *
 * Initialize PCIE EP PLL through BPMP Uphy driver
 */
struct MODS_INIT_PCIE_EP_PLL {
	/* IN */
	__u32 ep_id;
};

/* Used by legacy MODS_ESC_FIND_PCI_DEVICE ioctl */
struct MODS_FIND_PCI_DEVICE {
	/* IN */
	__u32 device_id;
	__u32 vendor_id;
	__u32 index;

	/* OUT */
	__u32 bus_number;
	__u32 device_number;
	__u32 function_number;
};

/* Used by MODS_ESC_FIND_PCI_CLASS_CODE_2 ioctl.
 *
 * Searches for PCI devices with the specified PCI class code.
 *
 * The first call to this ioctl is performed with index=0 and then for
 * every successful call index is incremented until this call fails.
 */
struct MODS_FIND_PCI_CLASS_CODE_2 {
	/* IN */
	__u32 class_code;
	__u32 index;

	/* OUT */
	struct mods_pci_dev_2 pci_device;
};

/* Used by legacy MODS_ESC_FIND_PCI_CLASS_CODE ioctl */
struct MODS_FIND_PCI_CLASS_CODE {
	/* IN */
	__u32 class_code;
	__u32 index;

	/* OUT */
	__u32 bus_number;
	__u32 device_number;
	__u32 function_number;
};

/* Used by MODS_ESC_PCI_GET_BAR_INFO_2 ioctl.
 *
 * Returns physical address and size of the queried PCI BAR.
 */
struct MODS_PCI_GET_BAR_INFO_2 {
	/* IN */
	struct mods_pci_dev_2 pci_device;
	__u32                 bar_index;

	/* OUT */
	__u64 base_address;
	__u64 bar_size;
};

/* Used by legacy MODS_ESC_PCI_GET_BAR_INFO ioctl */
struct MODS_PCI_GET_BAR_INFO {
	/* IN */
	struct mods_pci_dev pci_device;
	__u32               bar_index;

	/* OUT */
	__u64 base_address;
	__u64 bar_size;
};

/* Used by MODS_ESC_PCI_GET_IRQ_2 ioctl.
 *
 * Returns IRQ number assigned to the PCI device.  This is typically an APIC
 * IRQ number.  The value returned may or may not be relevant.  This value
 * matters only for legacy interrupts and is irrelevant for MSI.  This value
 * is for information purposes only.
 */
struct MODS_PCI_GET_IRQ_2 {
	/* IN */
	struct mods_pci_dev_2 pci_device;

	/* OUT */
	__u32 irq;
};

/* Used by legacy MODS_ESC_PCI_GET_IRQ ioctl */
struct MODS_PCI_GET_IRQ {
	/* IN */
	struct mods_pci_dev pci_device;

	/* OUT */
	__u32 irq;
};

/* Used by MODS_ESC_PCI_READ_2 ioctl.
 *
 * Reads data from PCI config space of a PCI device.
 * data_size specifies how many bytes to read at the specified address.
 * It can be 1, 2 or 4.
 */
struct MODS_PCI_READ_2 {
	/* IN */
	struct mods_pci_dev_2 pci_device;
	__u32                 address;
	__u32                 data_size;

	/* OUT */
	__u32                 data;
};

/* Used by legacy MODS_ESC_PCI_READ ioctl */
struct MODS_PCI_READ {
	/* IN */
	__u32 bus_number;
	__u32 device_number;
	__u32 function_number;
	__u32 address;
	__u32 data_size;

	/* OUT */
	__u32 data;
};

/* Used by MODS_ESC_PCI_WRITE_2 ioctl.
 *
 * Writes data into PCI config space of a PCI device.
 * data_size specifies how many bytes to write at the specified address.
 * It can be 1, 2 or 4.
 */
struct MODS_PCI_WRITE_2 {
	/* IN */
	struct mods_pci_dev_2 pci_device;
	__u32                 address;
	__u32                 data;
	__u32                 data_size;
};

/* Used by legacy MODS_ESC_PCI_WRITE ioctl */
struct MODS_PCI_WRITE {
	/* IN */
	__u32 bus_number;
	__u32 device_number;
	__u32 function_number;
	__u32 address;
	__u32 data;
	__u32 data_size;
};

/* Used by MODS_ESC_PCI_HOT_RESET ioctl.
 *
 * This call is implemented only for ppc64le architecture and it is currently
 * unused.
 */
struct MODS_PCI_HOT_RESET {
	/* IN */
	struct mods_pci_dev_2 pci_device;
};

/* Used by MODS_ESC_PCI_BUS_REMOVE_DEV ioctl.
 *
 * Stops and removes a PCI device from the bus.
 */
struct MODS_PCI_BUS_REMOVE_DEV {
	/* IN */
	struct mods_pci_dev_2 pci_device;
};

/* Used by MODS_ESC_PCI_BUS_RESCAN ioctl.
 *
 * Initiates a PCI bus scan for the specified PCI bus.
 * This is necessary e.g. to discover any new PCI devices which may have been
 * hot-plugged.
 */
struct MODS_PCI_BUS_RESCAN {
	/* IN */
	__u16 domain;
	__u16 bus;
};

/* Used by legacy MODS_ESC_PCI_BUS_ADD_DEVICES ioctl */
struct MODS_PCI_BUS_ADD_DEVICES {
	/* IN */
	__u32 bus;
};

/* Used by ioctls:
 * - MODS_ESC_SET_NUM_VF
 * - MODS_ESC_SET_TOTAL_VF
 *
 * MODS_ESC_SET_NUM_VF is used to enable or disable SR-IOV on the specified
 * PCI device.
 *
 * To disable SR-IOV, MODS_ESC_SET_NUM_VF is called with numvfs set to 0.
 *
 * MODS_ESC_SET_TOTAL_VF is used to set maximum number of virtual
 * functions.
 */
struct MODS_SET_NUM_VF {
	/* IN */
	struct mods_pci_dev_2 dev;
	__u32                 numvfs;  /* Number of virtual functions */
};

/* Used by legacy MODS_ESC_PCI_MAP_RESOURCE ioctl */
struct MODS_PCI_MAP_RESOURCE {
	/* IN */
	struct mods_pci_dev_2 local_pci_device;
	struct mods_pci_dev_2 remote_pci_device;
	__u32                 resource_index;
	__u64                 page_count;

	/* IN/OUT */
	__u64                 va;
};

/* Used by legacy MODS_ESC_PCI_UNMAP_RESOURCE ioctl */
struct MODS_PCI_UNMAP_RESOURCE {
	/* IN */
	struct mods_pci_dev_2 pci_device;
	__u64                 va;
};

/* Used by MODS_ESC_PIO_READ ioctl.
 *
 * Reads data from legacy x86 I/O port.  This ioctl is x86-specific.
 */
struct MODS_PIO_READ {
	/* IN */
	__u16 port;
	__u32 data_size;

	/* OUT */
	__u32 data;
};

/* Used by MODS_ESC_PIO_WRITE ioctl.
 *
 * Writes data to legacy x86 I/O port.  This ioctl is x86-specific.
 */
struct MODS_PIO_WRITE {
	/* IN */
	__u16 port;
	__u32 data;
	__u32 data_size;
};

/* Used by ioctls:
 * - MODS_ESC_READ_MSR
 * - MODS_ESC_WRITE_MSR
 *
 * These ioctls are used to read and write model specific registers (MSR)
 * and are available only on x86 architecture.
 */
struct MODS_MSR {
	/* IN */
	__u32 reg;
	__u32 cpu_num;

	/* IN/OUT */
	__u32 low;
	__u32 high;
};

/* Deprecated */
struct mods_irq_data {
	__u32 irq;
	__u32 delay;
};

#define INQ_CNT 8

/* Deprecated */
struct mods_irq_status {
	struct mods_irq_data data[INQ_CNT];
	__u32 irqbits:INQ_CNT;
	__u32 otherirq:1;
};

/* Used by several deprecated ioctls */
struct MODS_IRQ {
	/* IN */
	__u32                  cmd;
	__u32                  size;
	__u32                  irq;

	/* IN OUT */
	__u32                  channel;

	/* OUT */
	struct mods_irq_status stat;
	__u64                  phys;
};

/* Mask used to disable incoming legacy interrupts on x86.
 *
 * The driver propagates interrupts to userspace where they are serviced.
 * For legacy interrupts, the driver uses these masks to disable interrupts
 * in order to prevent interrupt storm, because as soon as the interrupt
 * service routine exits, the interrupt is still enabled and will keep
 * popping up until userspace has a chance to service it and clear it.
 *
 * When a legacy interrupt is triggered, the driver maps a region of physical
 * address space described by aperture_addr and aperture_size, then reads data
 * (32-bit or 64-bit, depending on mask_type) at offset specified by the
 * irq_pending_offset field to determine whether the interrupt is pending.
 * This is because legacy interrupts are shared and multiple devices may
 * be triggering them (e.g. an interrupt line can be shared between a network
 * adapter and a disk drive).  The driver also reads data (of the same bit
 * width) from offset specified by the irq_enabled_offset field to determine
 * whether the interrupt is actually enabled on the device.  If both fields
 * read a non-zero value, the interrupt is considered as pending (i.e. if the
 * interrupt is both pending and enabled).
 *
 * Once the driver determines that the interrupt is pending using the
 * irq_pending_offset and irq_enabled_offset fields, the driver applies the
 * mask at offset specified with the irq_disable_offset field.  This is either
 * a 32-bit or 64-bit value, depending on mask_type.  The value read from that
 * field is AND-ed with and_mask and then OR-ed with or-mask, then written
 * back at irq_disable_offset.
 *
 * All offsets are relative to aperture_addr.
 */
struct mods_mask_info2 {
	__u8  mask_type;   /* Mask size: 32-bit or 64-bit (see below) */
	__u8  reserved[7]; /* Force 64-bit member alignment */
	__u32 irq_pending_offset; /* Register to read IRQ pending status */
	__u32 irq_enabled_offset; /* Register to read IRQ enabled status */
	__u32 irq_enable_offset;  /* Unused */
	__u32 irq_disable_offset; /* Register to write to disable IRQs */
	__u64 and_mask;   /* AND mask for clearing bits in this register */
	__u64 or_mask;    /* OR mask for setting bit in this register */
};

/* Values for the mask_type field above */
#define MODS_MASK_TYPE_IRQ_DISABLE   0 /* 32-bit mask */
#define MODS_MASK_TYPE_IRQ_DISABLE64 1 /* 64-bit mask */

#define MODS_IRQ_MAX_MASKS 16

/* Used by MODS_ESC_REGISTER_IRQ_4 ioctl.
 *
 * Hooks an interrupt in the driver for the specified device.
 *
 * The userspace "driver" will subsequently invoke poll() on the open device
 * file descriptor to wait for incoming interrupts.  When poll() is released,
 * userspace calls MODS_ESC_QUERY_IRQ_3 to retrieve pending interrupt
 * vectors.  Userspace is responsible for properly servicing and rearming or
 * re-enabling the interrupts.
 *
 * The MODS_ESC_UNREGISTER_IRQ_2 ioctl is used to unhook interrupts.
 *
 * The interrupts will also be unhooked automatically when the driver is
 * closed, e.g. in the event of a crash in userspace, however this does not
 * de-configure the interrupts on the device.  In case of legacy interrupts,
 * the driver will mask the interrupts using the masks provided to prevent
 * an interrupt storm, but the interrupts may remain pending on the device.
 */
struct MODS_REGISTER_IRQ_4 {
	/* IN */

	/* PCI device for which interrupt is being hooked, if irq_flags
	 * has MODS_IRQ_TYPE_INT, _MSI or _MSIX flag.
	 *
	 * For CPU interrupts, the bus member holds the index of the
	 * interrupt to hook.
	 */
	struct mods_pci_dev_2 dev;

	/* Information used to mask incoming interrupts.  This is used
	 * primarily for legacy interrupts (MODS_IRQ_TYPE_INT), although
	 * it can be used for CPU interrupts as well.
	 */

	/* Physical address of aperture to map */
	__u64 aperture_addr;
	/* Size of the mapped region */
	__u32 aperture_size;
	/* Number of entries in mask_info[] */
	__u32 mask_info_cnt;
	/* Interrupt masks */
	struct mods_mask_info2 mask_info[MODS_IRQ_MAX_MASKS];

	/* Number of interrupt vectors to allocate.  This can be greater than
	 * 1 for MODS_IRQ_TYPE_MSIX and must be 1 for all other types.
	 */
	__u32 irq_count;

	/* Interrupt type and flags */
	__u32 irq_flags;
};

/* Flags for the irq_flags member above */
#define MODS_IRQ_TYPE_INT  0    /* Legacy interrupt, only x86 */
#define MODS_IRQ_TYPE_MSI  1    /* MSI */
#define MODS_IRQ_TYPE_CPU  2    /* CPU interrupt vector, only Tegra */
#define MODS_IRQ_TYPE_MSIX 3    /* MSI-X */
#define MODS_IRQ_TYPE_MASK 0xff /* Mask for the type field in irq_flags */

#define MODS_IRQ_TYPE_FROM_FLAGS(flags) ((flags) & MODS_IRQ_TYPE_MASK)
#define MODS_IRQ_FLAG_FROM_FLAGS(flags) ((flags) >> 8)

/* Used by legacy MODS_ESC_REGISTER_IRQ_3 ioctl */
struct MODS_REGISTER_IRQ_3 {
	/* IN */
	struct mods_pci_dev_2  dev;
	__u64                  aperture_addr;
	__u32                  aperture_size;
	__u32                  mask_info_cnt;
	struct mods_mask_info2 mask_info[MODS_IRQ_MAX_MASKS];
	__u8                   irq_type;
	__u8                   reserved[7];
};

/* Used by ioctls:
 * - MODS_ESC_UNREGISTER_IRQ_2
 * - MODS_ESC_REGISTER_IRQ_2 (deprectated)
 * - MODS_ESC_IRQ_HANDLED_2 (only Tegra)
 *
 * MODS_ESC_UNREGISTER_IRQ_2 is used to unhook interrupt for the given device.
 */
struct MODS_REGISTER_IRQ_2 {
	/* IN */
	struct mods_pci_dev_2 dev;
	__u8                  type; /* MODS_IRQ_TYPE_* */
};

/* Used by legacy ioctls:
 * - MODS_ESC_REGISTER_IRQ
 * - MODS_ESC_UNREGISTER_IRQ
 * - MODS_ESC_IRQ_HANDLED
 */
struct MODS_REGISTER_IRQ {
	/* IN */
	struct mods_pci_dev dev;
	__u8                type;
};

/* Describes a pending interrupt */
struct mods_irq_3 {
	struct mods_pci_dev_2 dev; /* Device which generated the interrupt */
	__u32  irq_index;          /* Index of MSI-X interrupt, or else 0 */
	__u32  delay;              /* Nanoseconds since the irq occurred */
};

/* Deprecated */
struct mods_irq_2 {
	__u32                 delay;
	struct mods_pci_dev_2 dev;
};

/* Deprecated */
struct mods_irq {
	__u32               delay;
	struct mods_pci_dev dev;
};

#define MODS_MAX_IRQS 32

/* Used by MODS_ESC_QUERY_IRQ_3 ioctl.
 *
 * Userspace typically waits on the open device file descriptor using poll()
 * and when poll() returns, userspace uses this ioctl to retrieve pending
 * interrupts.
 *
 * For every pending interrupt, one entry in irq_list specifies which
 * device triggered the interrupt:
 * - For legacy interrupts and MSI, the dev entry is the PCI device which
 *   raised the interrupt and irq_index is 0.
 * - For MSI-X, the dev entry is the PCI device which raised the interrupt
 *   and irq_index is the interrupt (message) index.
 * - For CPU interrupts, dev.bus is the index of the interrupt which was
 *   triggered and irq_index is 0.
 *
 * Valid fields have dev.bus different than 0xFFFF.  Entries in the structure
 * which haven't been filled by the driver have dev.bus set to 0xFFFF.
 *
 * If there are more interrupts pending than the structure can handle, the
 * more field is set to a non-zero value, in which case the userspace needs
 * to invoke this ioctl again.  If more field is 0, then userspace can
 * continue polling to wait for more interrupts to occur.
 */
struct MODS_QUERY_IRQ_3 {
	/* OUT */
	struct mods_irq_3 irq_list[MODS_MAX_IRQS];
	__u8              more;
};

/* Used by legacy MODS_ESC_QUERY_IRQ_2 ioctl */
struct MODS_QUERY_IRQ_2 {
	/* OUT */
	struct mods_irq_2 irq_list[MODS_MAX_IRQS];
	__u8               more;
};

/* Used by legacy MODS_ESC_QUERY_IRQ ioctl */
struct MODS_QUERY_IRQ {
	/* OUT */
	struct mods_irq irq_list[MODS_MAX_IRQS];
	__u8            more;
};

/* Deprecated */
struct mods_mask_info {
	__u8  mask_type;  /*mask type 32/64 bit access*/
	__u8  reserved[3];
	__u32 reg_offset; /* offset of register within the bar aperture*/
	__u64 and_mask;   /*and mask for clearing bits in this register */
	__u64 or_mask;    /*or value for setting bit in this register */
};

/* Used by legacy MODS_ESC_SET_IRQ_MULTIMASK ioctl */
struct MODS_SET_IRQ_MULTIMASK {
	/* IN */
	__u64                 aperture_addr;
	__u32                 aperture_size;
	struct mods_pci_dev_2 dev;
	__u32                 mask_info_cnt;
	struct mods_mask_info mask_info[MODS_IRQ_MAX_MASKS];
	__u8                  irq_type;
};

/* Used by legacy MODS_ESC_SET_IRQ_MASK_2 ioctl */
struct MODS_SET_IRQ_MASK_2 {
	/* IN */
	__u64                 aperture_addr;
	__u32                 aperture_size;
	__u32                 reg_offset;
	__u64                 and_mask;
	__u64                 or_mask;
	struct mods_pci_dev_2 dev;
	__u8                  irq_type;
	__u8                  mask_type;
};

/* Used by legacy MODS_ESC_SET_IRQ_MASK ioctl */
struct MODS_SET_IRQ_MASK {
	/* IN */
	__u64               aperture_addr;
	__u32               aperture_size;
	__u32               reg_offset;
	__u32               and_mask;
	__u32               or_mask;
	struct mods_pci_dev dev;
	__u8                irq_type;
	__u8                mask_type;
};

#define MAX_DT_SIZE 64
#define MAX_FULL_SIZE 128

/* Used by MODS_ESC_MAP_INTERRUPT ioctl.
 *
 * Finds GIC interrupt number for device by looking it up in device tree.
 *
 * Available only on Tegra.
 */
struct MODS_DT_INFO {
	/* OUT */

	/* GIC irq number*/
	__u32 irq;

	/* IN */

	/* Name of device tree node, for lookup */
	char  dt_name[MAX_DT_SIZE];
	/* Full name of node as in device tree */
	char  full_name[MAX_FULL_SIZE];
	/* Interrupt index corresponding to physical irq */
	__u32 index;
};

#define MAX_GPIO_NAME_SIZE 256

/* Used by MODS_ESC_MAP_GPIO ioctl.
 *
 * Finds GIC interrupt number for a GPIO owned by specific device.
 *
 * Available only on Tegra.
 */
struct MODS_GPIO_INFO {
	/* OUTPUT */

	/* GIC irq number*/
	__u32 irq;

	/* IN */

	/* Name of GPIO */
	char name[MAX_GPIO_NAME_SIZE];
	/* Name of device tree node owning the GPIO, for lookup */
	char dt_name[MAX_DT_SIZE];
	/* Full name of node owning the GPIO, as in device tree */
	char full_name[MAX_DT_SIZE];
};

/* Describes parameter to an ACPI function */
union ACPI_ARGUMENT {
	__u32 type;

	struct {
		__u32 type;
		__u32 value;
	} integer;

	struct {
		__u32 type;
		__u32 length; /* Number of bytes */
		__u32 offset; /* Offset in in_buffer or out_buffer */
	} buffer;

	struct {
		__u32 type;
		__u64 handle;
	} method;
};

/* Argument type (for the type field above) */
#define ACPI_MODS_TYPE_INTEGER   1
#define ACPI_MODS_TYPE_BUFFER    2
#define ACPI_MODS_TYPE_METHOD    3

#define ACPI_MAX_BUFFER_LENGTH   4096
#define ACPI_MAX_DEV_CHILDREN    16
#define ACPI_MAX_METHOD_LENGTH   12
#define ACPI_MAX_ARGUMENT_NUMBER 12

/* Used by MODS_ESC_EVAL_ACPI_METHOD ioctl.
 *
 * Executes the specified ACPI method.  The kernel looks up the specified
 * method in the ACPI tables and then executes it.
 *
 * This variant is for ACPI methods not related to any PCI devices.
 */
struct MODS_EVAL_ACPI_METHOD {
	/* IN */
	char                method_name[ACPI_MAX_METHOD_LENGTH];
	__u32               argument_count;
	union ACPI_ARGUMENT argument[ACPI_MAX_ARGUMENT_NUMBER];
	__u8                in_buffer[ACPI_MAX_BUFFER_LENGTH];

	/* IN OUT */
	__u32               out_data_size;

	/* OUT */
	__u8                out_buffer[ACPI_MAX_BUFFER_LENGTH];
	__u32               out_status;
};

/* Used by MODS_ESC_EVAL_DEV_ACPI_METHOD_3 ioctl.
 *
 * Executes the specified ACPI method.  The kernel looks up the specified
 * method in the ACPI tables and then executes it.
 *
 * This variant is for ACPI methods related to a specific PCI device.
 */
struct MODS_EVAL_DEV_ACPI_METHOD_3 {
	/* IN OUT */
	struct MODS_EVAL_ACPI_METHOD method;

	/* IN */
	struct mods_pci_dev_2        device;
	__u32                        acpi_id;
};

/* Used in the acpi_id member above to indicate that there is no ACPI id
 * associated with this method.
 */
#define ACPI_MODS_IGNORE_ACPI_ID 0xffffffff

/* Used by MODS_ESC_EVAL_DEV_ACPI_METHOD_2 ioctl.
 *
 * Executes the specified ACPI method.  The kernel looks up the specified
 * method in the ACPI tables and then executes it.
 *
 * This variant is for ACPI methods related to a specific PCI device.
 */
struct MODS_EVAL_DEV_ACPI_METHOD_2 {
	/* IN OUT */
	struct MODS_EVAL_ACPI_METHOD method;

	/* IN */
	struct mods_pci_dev_2 device;
};

/* Used by legacy MODS_ESC_EVAL_DEV_ACPI_METHOD ioctl */
struct MODS_EVAL_DEV_ACPI_METHOD {
	/* IN OUT */
	struct MODS_EVAL_ACPI_METHOD method;

	/* IN */
	struct mods_pci_dev device;
};

/* Used by MODS_ESC_ACPI_GET_DDC_2 ioctl.
 *
 * Finds an LCD device in the ACPI tables using _ADR ACPI method for the
 * specified PCI device, then uses the _DDC ACPI method to retrieve
 * EDID for that LCD device.
 */
struct MODS_ACPI_GET_DDC_2 {
	/* OUT */
	__u32                 out_data_size;
	__u8                  out_buffer[ACPI_MAX_BUFFER_LENGTH];

	/* IN */
	struct mods_pci_dev_2 device;
};

/* Used by legacy MODS_ESC_ACPI_GET_DDC ioctl */
struct MODS_ACPI_GET_DDC {
	/* OUT */
	__u32               out_data_size;
	__u8                out_buffer[ACPI_MAX_BUFFER_LENGTH];

	/* IN */
	struct mods_pci_dev device;
};

/* Used by MODS_ESC_GET_ACPI_DEV_CHILDREN ioctl */
struct MODS_GET_ACPI_DEV_CHILDREN {
	/* OUT */
	__u32                 num_children;
	__u32                 children[ACPI_MAX_DEV_CHILDREN];

	/* IN */
	struct mods_pci_dev_2 device;
};

/* Used by ioctls:
 * - MODS_ESC_GET_API_VERSION
 * - MODS_ESC_GET_KERNEL_VERSION (deprecated)
 *
 * MODS_ESC_GET_API_VERSION returns driver version.
 */
struct MODS_GET_VERSION {
	/* OUT */
	__u64 version;
};

/* Used by legacy MODS_ESC_SET_DRIVER_PARA ioctl */
struct MODS_SET_PARA {
	/* IN */
	__u64 Highmem4g;
	__u64 debug;
};

/* Used by MODS_ESC_SET_MEMORY_TYPE ioctl.
 *
 * This ioctl is used with non-memory regions of physical address space,
 * typically referred to as I/O ranges, to advertise a specific caching
 * attribute.
 *
 * The caching attribute takes effect when mmap() is executed on the
 * device's file descriptor with the physical address range for mapping
 * which fits in the range described by this ioctl.
 */
struct MODS_MEMORY_TYPE {
	/* IN */
	__u64 physical_address; /* First address of the memory range */
	__u64 size;             /* Size of the memory range */
	__u32 type;             /* Caching attribute (see below) */
};

/* Caching attributes for the type member above */
#define MODS_MEMORY_CACHED       5
#define MODS_MEMORY_UNCACHED     1
#define MODS_MEMORY_WRITECOMBINE 2

#define MAX_CPU_MASKS_3 128 /* CPU indices can be at most 4096 apart */

/* Used by MODS_ESC_DEVICE_NUMA_INFO_3 ioctl.
 *
 * Retrieves information about a NUMA node to which the specified
 * PCI device is physically attached.
 */
struct MODS_DEVICE_NUMA_INFO_3 {
	/* IN */
	struct mods_pci_dev_2 pci_device;

	/* OUT */
	__s32 node;       /* Index of the NUMA node where the PCI device is.
			   * -1 if NUMA is disabled.
			   */
	__u32 node_count; /* Total number of NUMA nodes */
	__u32 cpu_count;  /* Number of CPUs in the NUMA node */
			  /* Relative index for node_cpu_mask. */
	__u32 first_cpu_mask_offset;
			  /* Bitmask which specifies which CPUs are on this NUMA
			   * node.  Bit 0 in the first element of this array
			   * corresponds to CPU 32*first_cpu_mask_offset.  For
			   * example, if first_cpu_mask_offset is 2, then
			   * node_cpu_mask[0] has bit flags for CPUs 64..95.
			   * This allows for retrieving CPU indices when there
			   * are lots of CPUs in the system.
			   */
	__u32 node_cpu_mask[MAX_CPU_MASKS_3];
};

#if defined(__powerpc64__)
#define MAX_CPU_MASKS 64  /* 32 masks of 32bits = 2048 CPUs max */
#else
#define MAX_CPU_MASKS 32  /* 32 masks of 32bits = 1024 CPUs max */
#endif

/* Used by MODS_ESC_DEVICE_NUMA_INFO_2 ioctl.
 *
 * Retrieves information about a NUMA node to which the specified
 * PCI device is physically attached.
 */
struct MODS_DEVICE_NUMA_INFO_2 {
	/* IN */
	struct mods_pci_dev_2 pci_device;

	/* OUT */
	__s32 node;       /* Index of the NUMA node where the PCI device is.
			   * -1 if NUMA is disabled.
			   */
	__u32 node_count; /* Total number of NUMA nodes */
	__u32 node_cpu_mask[MAX_CPU_MASKS];
			  /* Zero-based bitmask which specifies which CPUs are
			   * on this NUMA node
			   */
	__u32 cpu_count;  /* Number of CPUs in the NUMA node */
};

/* Used by legacy MODS_ESC_DEVICE_NUMA_INFO ioctl */
struct MODS_DEVICE_NUMA_INFO {
	/* IN */
	struct mods_pci_dev pci_device;

	/* OUT */
	__s32 node;
	__u32 node_count;
	__u32 node_cpu_mask[MAX_CPU_MASKS];
	__u32 cpu_count;
};

/* Used by MODS_ESC_GET_RESET_HANDLE ioctl.
 * Used to get an index in reset array corresponding to a reset name
 * so that a client can reference it easily in future calls to toggle
 * reset
 */
struct MODS_GET_RESET_HANDLE {
	/* OUT */
	__u32 reset_handle;

	/* IN */
	char  reset_name[MAX_DT_SIZE];
};

/* Used by MODS_ESC_RESET_ASSERT */
struct MODS_RESET_HANDLE {
	/* IN */
	__u32 handle;
	__u8  assert;
};

/* Used by legacy MODS_ESC_GET_SCREEN_INFO ioctl and as a member of
 * MODS_SCREEN_INFO_2.
 */
struct MODS_SCREEN_INFO {
	/* OUT */
	__u8  orig_video_mode;
	__u8  orig_video_is_vga;
	__u16 lfb_width;
	__u16 lfb_height;
	__u16 lfb_depth;
	__u32 lfb_base;
	__u32 lfb_size;
	__u16 lfb_linelength;
};

/* Used by MODS_ESC_GET_SCREEN_INFO_2 ioctl.
 *
 * Retrieves information about system console, especially framebuffer console
 * when the system has booted in EFI mode.
 *
 * Available only on x86.
 */
struct MODS_SCREEN_INFO_2 {
	/* OUT */
	struct MODS_SCREEN_INFO info;
	__u32                   ext_lfb_base;
};

/* Used by MODS_ESC_SET_PPC_TCE_BYPASS ioctl.
 *
 * This call is specific to the ppc64le architecture.
 * It configures the TCE bypass.
 */
struct MODS_SET_PPC_TCE_BYPASS {
	/* IN */
	__u8                  mode;
	__u8                  _dummy_align[7];
	struct mods_pci_dev_2 pci_device;
	__u64                 device_dma_mask;

	/* OUT */
	__u64                 dma_base_address;
};

/* Values for the mode member above */
#define MODS_PPC_TCE_BYPASS_DEFAULT 0 /* Use default value from module param */
#define MODS_PPC_TCE_BYPASS_ON      1 /* Enable TCE bypass */
#define MODS_PPC_TCE_BYPASS_OFF     2 /* Disable TCE bypass */

/* Used by MODS_ESC_GET_ATS_ADDRESS_RANGE ioctl.
 *
 * Retrieves information about NPU device location and address translation
 * services (part of IOMMU).
 *
 * This call is specific to ppc64le architecture.
 */
struct MODS_GET_ATS_ADDRESS_RANGE {
	/* IN */
	struct mods_pci_dev_2 pci_device;
	__s32                 npu_index;
	__u8                  reserved[4]; /* Alignment */

	/* OUT */
	struct mods_pci_dev_2 npu_device;
	__u64                 phys_addr;
	__u64                 guest_addr;
	__u64                 aperture_size;
	__s32                 numa_memory_node;
};

/* Used by MODS_ESC_SET_NVLINK_SYSMEM_TRAINED ioctl.
 *
 * Tells the driver whether NVLINK connection to system memory has been trained.
 *
 * This call is specific to ppc64le architecture.
 */
struct MODS_SET_NVLINK_SYSMEM_TRAINED {
	/* IN */
	struct mods_pci_dev_2 pci_device;
	__u8                  trained;
};

/* Used by MODS_ESC_GET_NVLINK_LINE_RATE ioctl.
 *
 * Retrieves NVLINK speed from device tree.
 *
 * This call is specific to ppc64le architecture.
 */
struct MODS_GET_NVLINK_LINE_RATE {
	/* IN */
	struct mods_pci_dev_2 pci_device;
	__s32                 npu_index;

	/* OUT */
	__u32                 speed;
};

#define MODS_MAX_SYSFS_PATH_BUF_SIZE 512
#define MODS_MAX_SYSFS_FILE_SIZE 4096

/* Used by MODS_ESC_WRITE_SYSFS_NODE ioctl.
 *
 * Writes specified contents to the given sysfs node.
 *
 * 'path' parameter is relative to /sys/.
 */
struct MODS_SYSFS_NODE {
	/* IN */
	char path[MODS_MAX_SYSFS_PATH_BUF_SIZE];
	char contents[MODS_MAX_SYSFS_FILE_SIZE];
	__u32 size; /* Size of the contents buffer, in bytes */
};

/* Used by MODS_ESC_SYSCTL_WRITE_INT ioctl.
 *
 * Writes specified integer value into a node under /proc/sys/.
 *
 * 'path' parameter is relative to /proc/sys/.
 */
struct MODS_SYSCTL_INT {
	/* IN */
	char  path[MODS_MAX_SYSFS_PATH_BUF_SIZE];
	__s64 value;
};

#define MODS_DRIVER_STATS_VERSION 1

/* Used by MODS_ESC_MODS_GET_DRIVER_STATS ioctl.
 *
 * Returns driver operation statistics.
 */
struct MODS_GET_DRIVER_STATS {
	/* OUT */
	__u64 version;
	__u64 num_allocs;
	__u64 num_pages;
	__u64 reserved[13];
};

#define MAX_CLOCK_HANDLE_NAME 64

/* Used by MODS_ESC_GET_CLOCK_HANDLE ioctl.
 *
 * Available only on Tegra.
 */
struct MODS_GET_CLOCK_HANDLE {
	/* OUT */
	__u32 clock_handle;

	/* IN */
	char  device_name[MAX_CLOCK_HANDLE_NAME];
	char  controller_name[MAX_CLOCK_HANDLE_NAME];
};

/* Used by ioctls:
 * - MODS_ESC_SET_CLOCK_RATE
 * - MODS_ESC_GET_CLOCK_RATE
 * - MODS_ESC_GET_CLOCK_MAX_RATE
 * - MODS_ESC_SET_CLOCK_MAX_RATE
 *
 * Available only on Tegra.
 */
struct MODS_CLOCK_RATE {
	/* IN/OUT */
	__u64 clock_rate_hz;

	/* IN */
	__u32 clock_handle;
};

/* Used by ioctls:
 * - MODS_ESC_SET_CLOCK_PARENT
 * - MODS_ESC_GET_CLOCK_PARENT
 *
 * Available only on Tegra.
 */
struct MODS_CLOCK_PARENT {
	/* IN */
	__u32 clock_handle;

	/* IN/OUT */
	__u32 clock_parent_handle;
};

/* Used by ioctls:
 * - MODS_ESC_ENABLE_CLOCK
 * - MODS_ESC_DISABLE_CLOCK
 * - MODS_ESC_CLOCK_RESET_ASSERT
 * - MODS_ESC_CLOCK_RESET_DEASSERT
 *
 * Available only on Tegra.
 */
struct MODS_CLOCK_HANDLE {
	/* IN */
	__u32 clock_handle;
};

/* Used by MODS_ESC_IS_CLOCK_ENABLED ioctl.
 *
 * Available only on Tegra.
 */
struct MODS_CLOCK_ENABLED {
	/* IN */
	__u32 clock_handle;

	/* OUT */
	__u32 enable_count;
};

struct MODS_TEGRA_DC_WINDOW {
	__s32 index;
	__u32 flags;
	__u32 x;
	__u32 y;
	__u32 w;
	__u32 h;
	__u32 out_x;
	__u32 out_y;
	__u32 out_w;
	__u32 out_h;
	__u32 pixformat; /* NVDC pix format */

	__u32 bandwidth;
};

#define MODS_TEGRA_DC_WINDOW_FLAG_ENABLED   (1 << 0)
#define MODS_TEGRA_DC_WINDOW_FLAG_TILED     (1 << 1)
#define MODS_TEGRA_DC_WINDOW_FLAG_SCAN_COL  (1 << 2)
#define MODS_TEGRA_DC_MAX_WINDOWS           (6)

/* Used by MODS_ESC_TEGRA_DC_CONFIG_POSSIBLE ioctl.
 *
 * Available only on Tegra.
 */
struct MODS_TEGRA_DC_CONFIG_POSSIBLE {
	/* IN/OUT */
	struct MODS_TEGRA_DC_WINDOW windows[MODS_TEGRA_DC_MAX_WINDOWS];

	/* IN */
	__u8 head;
	__u8 win_num;

	/* OUT */
	__u8  possible;
};

#define MODS_TEGRA_DC_SETUP_SD_LUT_SIZE  9
#define MODS_TEGRA_DC_SETUP_BLTF_SIZE   16

/* Used by MODS_ESC_TEGRA_DC_SETUP_SD ioctl
 *
 * Available only on Tegra.
 */
struct MODS_TEGRA_DC_SETUP_SD {
	/* IN */
	__u8 head;
	__u8 enable;

	__u8 use_vid_luma;
	__u8 csc_r;
	__u8 csc_g;
	__u8 csc_b;
	__u8 aggressiveness;
	__u8 bin_width_log2;

	__u32 lut[MODS_TEGRA_DC_SETUP_SD_LUT_SIZE];
	__u32 bltf[MODS_TEGRA_DC_SETUP_BLTF_SIZE];

	__u32 klimit;
	__u32 soft_clipping_threshold;
	__u32 smooth_k_inc;
	__u8  k_init_bias;


	__u32 win_x;
	__u32 win_y;
	__u32 win_w;
	__u32 win_h;
};

#define MODS_ADSP_APP_NAME_SIZE 64
#define MODS_ADSP_APP_MAX_PARAM 128

/* Used by MODS_ESC_ADSP_RUN_APP ioctl.
 *
 * Available only on Tegra.
 */
struct MODS_ADSP_RUN_APP_INFO {
	char app_name[MODS_ADSP_APP_NAME_SIZE];
	char app_file_name[MODS_ADSP_APP_NAME_SIZE];
	__u32 argc;
	__u32 argv[MODS_ADSP_APP_MAX_PARAM];
	__u32 timeout;
};

enum MODS_DMA_TRANSACTION_TYPE {
	MODS_DMA_MEMCPY,
	MODS_DMA_XOR,
	MODS_DMA_PQ,
	MODS_DMA_XOR_VAL,
	MODS_DMA_PQ_VAL,
	MODS_DMA_MEMSET,
	MODS_DMA_MEMSET_SG,
	MODS_DMA_INTERRUPT,
	MODS_DMA_SG,
	MODS_DMA_PRIVATE,
	MODS_DMA_ASYNC_TX,
	MODS_DMA_SLAVE,
	MODS_DMA_CYCLIC,
	MODS_DMA_INTERLEAVE,
/* last transaction type for creation of the capabilities mask */
	MODS_DMA_TX_TYPE_END
};

/* Used by ioctls:
 * - MODS_ESC_DMA_REQUEST_HANDLE
 * - MODS_ESC_DMA_RELEASE_HANDLE
 * - MODS_ESC_DMA_ISSUE_PENDING
 *
 * Available only on Tegra.
 */
struct MODS_DMA_HANDLE {
	/* IN */
	__u32 dma_type; /* Indicate the DMA Type*/
	/* OUT */
	__u32	dma_id; /* Inditify for the DMA */
};

enum MODS_DMA_TRANSFER_DIRECTION {
	MODS_DMA_MEM_TO_MEM,
	MODS_DMA_MEM_TO_DEV,
	MODS_DMA_DEV_TO_MEM,
	MODS_DMA_DEV_TO_DEV,
	MODS_DMA_TRANS_NONE
};

enum MODS_DMA_BUSWIDTH {
	MODS_DMA_BUSWIDTH_UNDEFINED = 0,
	MODS_DMA_BUSWIDTH_1_BYTE = 1,
	MODS_DMA_BUSWIDTH_2_BYTES = 2,
	MODS_DMA_BUSWIDTH_4_BYTES = 4,
	MODS_DMA_BUSWIDTH_8_BYTES = 8
};

/* Used by MODS_ESC_DMA_SET_CONFIG ioctl.
 *
 * Available only on Tegra.
 */
struct MODS_DMA_CHANNEL_CONFIG {
	__u64 src_addr;
	__u64 dst_addr;
	struct MODS_DMA_HANDLE handle;
	__u32 direction;
	__u32 src_addr_width;
	__u32 dst_addr_width;
	__u32 src_maxburst;
	__u32 dst_maxburst;
	__u32 slave_id;
	__u32 device_fc;
};

/* Node: Only support SINGLE MODS so far*/
enum MODS_DMA_TX_MODE {
	MODS_DMA_SINGLE = 0,
	MODS_DMA_TX_CYCLIC,
	MODS_DMA_INTERLEAVED /* Common to Slave as well as M2M clients. */
};

/* Deprecated */
typedef __s32 mods_dma_cookie_t;

/* Used by MODS_ESC_DMA_TX_SUBMIT ioctl.
 *
 * Available only on Tegra.
 */
struct MODS_DMA_TX_DESC	{
	/* IN */
	__u64 phys;
	__u64 phys_2; /* only valid for MEMCPY */
	struct MODS_DMA_HANDLE handle;
	__u32 mode;
	__u32 data_dir;
	__u32 length;
	__u32 flags;
	/* OUT */
	__s32 cookie;
};

enum MODS_DMA_WAIT_TYPE {
	MODS_DMA_SYNC_WAIT,     /* wait until finished */
	MODS_DMA_ASYNC_WAIT     /* just check tx status */
};

/* Used by MODS_ESC_DMA_TX_WAIT ioctl.
 *
 * Available only on Tegra.
 */
struct MODS_DMA_WAIT_DESC {
	struct MODS_DMA_HANDLE handle;
	__s32 cookie;
	__u32 type;
	/* OUT */
	__u32 tx_complete;
};

#define MAX_NET_DEVICE_NAME_LENGTH 16

/* Used by MODS_ESC_NET_FORCE_LINK ioctl.
 *
 * Available only on Tegra.
 */
struct MODS_NET_DEVICE_NAME {
	/* in */
	char device_name[MAX_NET_DEVICE_NAME_LENGTH];
};

/* Used by ioctls:
 * - MODS_ESC_DMA_ALLOC_COHERENT
 * - MODS_ESC_DMA_FREE_COHERENT
 *
 * Available only on Tegra.
 */
struct MODS_DMA_COHERENT_MEM_HANDLE {
	__u32	num_bytes;
	__u32	attrib;
	__u64	memory_handle_phys;
	__u64	memory_handle_virt;
};

/* Used by MODS_ESC_DMA_COPY_TO_USER ioctl.
 *
 * Available only on Tegra.
 */
struct MODS_DMA_COPY_TO_USER {
	__u32	num_bytes;
	__u32	attrib;
	__u64	memory_handle_src;
	__u64	memory_handle_dst;
};

/* Used by iocls:
 * - MODS_ESC_TEGRA_PROD_SET_PROD_ALL
 * - MODS_ESC_TEGRA_PROD_SET_PROD_BOOT
 * - MODS_ESC_TEGRA_PROD_SET_PROD_BY_NAME
 * - MODS_ESC_TEGRA_PROD_SET_PROD_EXACT
 *
 * Available only on Tegra.
 */
struct MODS_TEGRA_PROD_SET_TUPLE {
	/* IN */
	__u64 prod_dev_handle;
	__u64 ctrl_dev_handle;
	char prod_name[MAX_DT_SIZE];
	__u32 index;
	__u32 offset;
	__u32 mask;
};

/* Used by MODS_ESC_TEGRA_PROD_IS_SUPPORTED ioctl.
 *
 * Available only on Tegra.
 */
struct MODS_TEGRA_PROD_IS_SUPPORTED {
	/* IN */
	__u64 prod_dev_handle;
	char prod_name[MAX_DT_SIZE];
	/* OUT */
	__u32 is_supported;
};

/* Used by MODS_ESC_TEGRA_PROD_ITERATE_DT ioctl.
 *
 * Available only on Tegra.
 */
struct MODS_TEGRA_PROD_ITERATOR {
	/* IN */
	__u64 device_handle;
	char name[MAX_DT_SIZE];
	char next_name[MAX_DT_SIZE];
	__u32 index;
	__u32 is_leaf;
	/* OUT */
	__u64 next_device_handle;
};

/* Used by MODS_ESC_IOMMU_DMA_MAP_MEMORY ioctl.
 *
 * Available only on Tegra.
 */
struct MODS_IOMMU_DMA_MAP_MEMORY {
	/* IN */
	__u64 memory_handle;
	char  dev_name[MAX_DT_SIZE];
	__u8  flags;
	__u8  reserved[7]; /* Force 64-bit member alignment */

	/* OUT */
	__u64 physical_address;
};

#define MAX_TZ_BUFFER_SIZE 512
/* Used by MODS_ESC_SEND_TZ_MSG.
 *
 * Available only on Tegra.
 */
struct MODS_TZ_PARAMS {
	/* IN */
	__u8 buf[MAX_TZ_BUFFER_SIZE];
	__u32 buf_size;
	__u32 cmd;

	/* OUT */
	int status;
};

/* Used by MODS_ESC_OIST_STATUS ioctl.
 *
 * Available only on Tegra.
 */
struct MODS_TEGRA_OIST_STATUS {
	/* IN */
	__u64 smc_func_id;
	/* IN */
	__u64 a1;
	/* IN */
	__u64 a2;
	/* OUT */
	__u64 smc_status;
};

#define MODS_IOMMU_MAP_CONTIGUOUS 1

#pragma pack(pop)

#define MODS_IOC_MAGIC 'x'

#define MODSIO(dir, num, type) _IO##dir(MODS_IOC_MAGIC, num, struct type)

#define MODS_ESC_ALLOC_PAGES MODSIO(WR, 0, MODS_ALLOC_PAGES)
#define MODS_ESC_FREE_PAGES MODSIO(WR, 1, MODS_FREE_PAGES)
/* Deprecated */
#define MODS_ESC_GET_PHYSICAL_ADDRESS MODSIO(WR, 2, MODS_GET_PHYSICAL_ADDRESS)
#define MODS_ESC_VIRTUAL_TO_PHYSICAL MODSIO(WR, 3, MODS_VIRTUAL_TO_PHYSICAL)
#define MODS_ESC_PHYSICAL_TO_VIRTUAL MODSIO(WR, 4, MODS_PHYSICAL_TO_VIRTUAL)
/* Deprecated */
#define MODS_ESC_FIND_PCI_DEVICE MODSIO(WR, 5, MODS_FIND_PCI_DEVICE)
/* Deprecated */
#define MODS_ESC_FIND_PCI_CLASS_CODE MODSIO(WR, 6, MODS_FIND_PCI_CLASS_CODE)
/* Deprecated */
#define MODS_ESC_PCI_READ MODSIO(WR, 7, MODS_PCI_READ)
/* Deprecated */
#define MODS_ESC_PCI_WRITE MODSIO(WR, 8, MODS_PCI_WRITE)
#define MODS_ESC_PIO_READ MODSIO(WR, 9, MODS_PIO_READ)
#define MODS_ESC_PIO_WRITE MODSIO(WR, 10, MODS_PIO_WRITE)
/* Deprecated */
#define MODS_ESC_IRQ_REGISTER MODSIO(WR, 11, MODS_IRQ)
/* Deprecated */
#define MODS_ESC_IRQ_FREE MODSIO(WR, 12, MODS_IRQ)
/* Deprecated */
#define MODS_ESC_IRQ_INQUIRY MODSIO(WR, 13, MODS_IRQ)
#define MODS_ESC_EVAL_ACPI_METHOD MODSIO(WR_BAD, 16, MODS_EVAL_ACPI_METHOD)
#define MODS_ESC_GET_API_VERSION MODSIO(WR, 17, MODS_GET_VERSION)
/* Deprecated */
#define MODS_ESC_GET_KERNEL_VERSION MODSIO(WR, 18, MODS_GET_VERSION)
/* Deprecated */
#define MODS_ESC_SET_DRIVER_PARA MODSIO(WR, 19, MODS_SET_PARA)
/* Deprecated */
#define MODS_ESC_MSI_REGISTER MODSIO(WR, 20, MODS_IRQ)
/* Deprecated */
#define MODS_ESC_REARM_MSI MODSIO(WR, 21, MODS_IRQ)
#define MODS_ESC_SET_MEMORY_TYPE MODSIO(W, 22, MODS_MEMORY_TYPE)
/* Deprecated */
#define MODS_ESC_PCI_BUS_ADD_DEVICES MODSIO(W, 23, MODS_PCI_BUS_ADD_DEVICES)
/* Deprecated */
#define MODS_ESC_REGISTER_IRQ MODSIO(W, 24, MODS_REGISTER_IRQ)
/* Deprecated */
#define MODS_ESC_UNREGISTER_IRQ MODSIO(W, 25, MODS_REGISTER_IRQ)
#define MODS_ESC_QUERY_IRQ MODSIO(R, 26, MODS_QUERY_IRQ)
/* Deprecated */
#define MODS_ESC_EVAL_DEV_ACPI_METHOD MODSIO(WR_BAD, 27, \
					     MODS_EVAL_DEV_ACPI_METHOD)
#define MODS_ESC_ACPI_GET_DDC MODSIO(WR, 28, MODS_ACPI_GET_DDC)
#define MODS_ESC_GET_CLOCK_HANDLE MODSIO(WR, 29, MODS_GET_CLOCK_HANDLE)
#define MODS_ESC_SET_CLOCK_RATE MODSIO(W, 30, MODS_CLOCK_RATE)
#define MODS_ESC_GET_CLOCK_RATE MODSIO(WR, 31, MODS_CLOCK_RATE)
#define MODS_ESC_SET_CLOCK_PARENT MODSIO(W, 32, MODS_CLOCK_PARENT)
#define MODS_ESC_GET_CLOCK_PARENT MODSIO(WR, 33, MODS_CLOCK_PARENT)
#define MODS_ESC_ENABLE_CLOCK MODSIO(W, 34, MODS_CLOCK_HANDLE)
#define MODS_ESC_DISABLE_CLOCK MODSIO(W, 35, MODS_CLOCK_HANDLE)
#define MODS_ESC_IS_CLOCK_ENABLED MODSIO(WR, 36, MODS_CLOCK_ENABLED)
#define MODS_ESC_CLOCK_RESET_ASSERT MODSIO(W, 37, MODS_CLOCK_HANDLE)
#define MODS_ESC_CLOCK_RESET_DEASSERT MODSIO(W, 38, MODS_CLOCK_HANDLE)
/* Deprecated */
#define MODS_ESC_SET_IRQ_MASK MODSIO(W, 39, MODS_SET_IRQ_MASK)
/* Deprecated */
#define MODS_ESC_MEMORY_BARRIER _IO(MODS_IOC_MAGIC, 40)
/* Deprecated */
#define MODS_ESC_IRQ_HANDLED MODSIO(W, 41, MODS_REGISTER_IRQ)
#define MODS_ESC_FLUSH_CPU_CACHE_RANGE MODSIO(W, 42, MODS_FLUSH_CPU_CACHE_RANGE)
#define MODS_ESC_GET_CLOCK_MAX_RATE MODSIO(WR, 43, MODS_CLOCK_RATE)
#define MODS_ESC_SET_CLOCK_MAX_RATE MODSIO(W, 44, MODS_CLOCK_RATE)
#define MODS_ESC_DEVICE_ALLOC_PAGES MODSIO(WR, 45, MODS_DEVICE_ALLOC_PAGES)
/* Deprecated */
#define MODS_ESC_DEVICE_NUMA_INFO MODSIO(WR, 46, MODS_DEVICE_NUMA_INFO)
#define MODS_ESC_TEGRA_DC_CONFIG_POSSIBLE MODSIO(WR, 47, \
						 MODS_TEGRA_DC_CONFIG_POSSIBLE)
#define MODS_ESC_TEGRA_DC_SETUP_SD MODSIO(W, 48, MODS_TEGRA_DC_SETUP_SD)
#define MODS_ESC_DMABUF_GET_PHYSICAL_ADDRESS MODSIO(WR, 49, \
					MODS_DMABUF_GET_PHYSICAL_ADDRESS)
#define MODS_ESC_ADSP_LOAD _IO(MODS_IOC_MAGIC, 50)
#define MODS_ESC_ADSP_START _IO(MODS_IOC_MAGIC, 51)
#define MODS_ESC_ADSP_STOP _IO(MODS_IOC_MAGIC, 52)
#define MODS_ESC_ADSP_RUN_APP MODSIO(W, 53, MODS_ADSP_RUN_APP_INFO)
/* Deprecated */
#define MODS_ESC_PCI_GET_BAR_INFO MODSIO(WR, 54, MODS_PCI_GET_BAR_INFO)
/* Deprecated */
#define MODS_ESC_PCI_GET_IRQ MODSIO(WR, 55, MODS_PCI_GET_IRQ)
/* Deprecated */
#define MODS_ESC_GET_MAPPED_PHYSICAL_ADDRESS MODSIO(WR, 56, \
						    MODS_GET_PHYSICAL_ADDRESS)
#define MODS_ESC_DEVICE_ALLOC_PAGES_2 MODSIO(WR, 57, MODS_DEVICE_ALLOC_PAGES_2)
#define MODS_ESC_FIND_PCI_DEVICE_2 MODSIO(WR, 58, MODS_FIND_PCI_DEVICE_2)
#define MODS_ESC_FIND_PCI_CLASS_CODE_2 MODSIO(WR, 59, \
					      MODS_FIND_PCI_CLASS_CODE_2)
#define MODS_ESC_PCI_GET_BAR_INFO_2 MODSIO(WR, 60, MODS_PCI_GET_BAR_INFO_2)
#define MODS_ESC_PCI_GET_IRQ_2 MODSIO(WR, 61, MODS_PCI_GET_IRQ_2)
#define MODS_ESC_PCI_READ_2 MODSIO(WR, 62, MODS_PCI_READ_2)
#define MODS_ESC_PCI_WRITE_2 MODSIO(W, 63, MODS_PCI_WRITE_2)
/* Deprecated */
#define MODS_ESC_REGISTER_IRQ_2 MODSIO(W, 64, MODS_REGISTER_IRQ_2)
#define MODS_ESC_UNREGISTER_IRQ_2 MODSIO(W, 65, MODS_REGISTER_IRQ_2)
#define MODS_ESC_IRQ_HANDLED_2 MODSIO(W, 66, MODS_REGISTER_IRQ_2)
/* Deprecated */
#define MODS_ESC_QUERY_IRQ_2 MODSIO(R, 67, MODS_QUERY_IRQ_2)
/* Deprecated */
#define MODS_ESC_SET_IRQ_MASK_2 MODSIO(W, 68, MODS_SET_IRQ_MASK_2)
#define MODS_ESC_EVAL_DEV_ACPI_METHOD_2 MODSIO(WR_BAD, 69, \
					       MODS_EVAL_DEV_ACPI_METHOD_2)
#define MODS_ESC_DEVICE_NUMA_INFO_2 MODSIO(WR, 70, MODS_DEVICE_NUMA_INFO_2)
#define MODS_ESC_ACPI_GET_DDC_2 MODSIO(WR, 71, MODS_ACPI_GET_DDC_2)
/* Deprecated */
#define MODS_ESC_GET_SCREEN_INFO MODSIO(R, 72, MODS_SCREEN_INFO)
#define MODS_ESC_PCI_HOT_RESET MODSIO(W, 73, MODS_PCI_HOT_RESET)
#define MODS_ESC_SET_PPC_TCE_BYPASS MODSIO(WR, 74, MODS_SET_PPC_TCE_BYPASS)
#define MODS_ESC_DMA_MAP_MEMORY MODSIO(W, 75, MODS_DMA_MAP_MEMORY)
#define MODS_ESC_DMA_UNMAP_MEMORY MODSIO(W, 76, MODS_DMA_MAP_MEMORY)
/* Deprecated */
#define MODS_ESC_GET_MAPPED_PHYSICAL_ADDRESS_2 MODSIO(WR, 77, \
						MODS_GET_PHYSICAL_ADDRESS_2)
/* Deprecated */
#define MODS_ESC_PCI_MAP_RESOURCE MODSIO(WR, 78, MODS_PCI_MAP_RESOURCE)
/* Deprecated */
#define MODS_ESC_PCI_UNMAP_RESOURCE MODSIO(W, 79, MODS_PCI_UNMAP_RESOURCE)
#define MODS_ESC_DMA_REQUEST_HANDLE MODSIO(R,  80, MODS_DMA_HANDLE)
#define MODS_ESC_DMA_RELEASE_HANDLE MODSIO(W,  81, MODS_DMA_HANDLE)
#define MODS_ESC_DMA_SET_CONFIG MODSIO(W,  82, MODS_DMA_CHANNEL_CONFIG)
#define MODS_ESC_DMA_TX_SUBMIT MODSIO(W,  83, MODS_DMA_TX_DESC)
#define MODS_ESC_DMA_TX_WAIT MODSIO(WR, 84, MODS_DMA_WAIT_DESC)
#define MODS_ESC_DMA_ISSUE_PENDING MODSIO(W,  85, MODS_DMA_HANDLE)
/* Deprecated */
#define MODS_ESC_SET_IRQ_MULTIMASK MODSIO(W, 86, MODS_SET_IRQ_MULTIMASK)
#define MODS_ESC_NET_FORCE_LINK MODSIO(W, 87, MODS_NET_DEVICE_NAME)
/* Deprecated */
#define MODS_ESC_REGISTER_IRQ_3 MODSIO(W, 88, MODS_REGISTER_IRQ_3)
#define MODS_ESC_DMA_ALLOC_COHERENT MODSIO(WR, 89, MODS_DMA_COHERENT_MEM_HANDLE)
#define MODS_ESC_DMA_FREE_COHERENT MODSIO(WR, 90, MODS_DMA_COHERENT_MEM_HANDLE)
#define MODS_ESC_DMA_COPY_TO_USER MODSIO(WR, 91, MODS_DMA_COPY_TO_USER)
#define MODS_ESC_MAP_INTERRUPT MODSIO(WR, 92, MODS_DT_INFO)
/* Locks system console */
#define MODS_ESC_LOCK_CONSOLE _IO(MODS_IOC_MAGIC, 93)
/* Unlocks system console */
#define MODS_ESC_UNLOCK_CONSOLE _IO(MODS_IOC_MAGIC, 94)
#define MODS_ESC_TEGRA_PROD_IS_SUPPORTED MODSIO(WR, 95, \
						MODS_TEGRA_PROD_IS_SUPPORTED)
#define MODS_ESC_TEGRA_PROD_SET_PROD_ALL MODSIO(W, 96, \
						MODS_TEGRA_PROD_SET_TUPLE)
#define MODS_ESC_TEGRA_PROD_SET_PROD_BOOT MODSIO(W, 97, \
						 MODS_TEGRA_PROD_SET_TUPLE)
#define MODS_ESC_TEGRA_PROD_SET_PROD_BY_NAME MODSIO(W, 98, \
						    MODS_TEGRA_PROD_SET_TUPLE)
#define MODS_ESC_TEGRA_PROD_SET_PROD_EXACT MODSIO(W, 99, \
						  MODS_TEGRA_PROD_SET_TUPLE)
#define MODS_ESC_TEGRA_PROD_ITERATE_DT MODSIO(WR, 100, MODS_TEGRA_PROD_ITERATOR)
#define MODS_ESC_GET_ATS_ADDRESS_RANGE MODSIO(WR, 101, \
					      MODS_GET_ATS_ADDRESS_RANGE)
#define MODS_ESC_SET_NVLINK_SYSMEM_TRAINED MODSIO(W, 102, \
						MODS_SET_NVLINK_SYSMEM_TRAINED)
#define MODS_ESC_GET_NVLINK_LINE_RATE MODSIO(WR, 103, MODS_GET_NVLINK_LINE_RATE)
/* Suspends system console */
#define MODS_ESC_SUSPEND_CONSOLE _IO(MODS_IOC_MAGIC, 104)
/* Resumes system console */
#define MODS_ESC_RESUME_CONSOLE _IO(MODS_IOC_MAGIC, 105)
#define MODS_ESC_GET_SCREEN_INFO_2 MODSIO(R, 106, MODS_SCREEN_INFO_2)
#define MODS_ESC_ACQUIRE_ACCESS_TOKEN MODSIO(R, 107, MODS_ACCESS_TOKEN)
#define MODS_ESC_RELEASE_ACCESS_TOKEN MODSIO(W, 108, MODS_ACCESS_TOKEN)
#define MODS_ESC_VERIFY_ACCESS_TOKEN MODSIO(W, 109, MODS_ACCESS_TOKEN)
/* Deprecated */
#define MODS_ESC_GET_IOMMU_STATE MODSIO(WR, 110, MODS_GET_IOMMU_STATE)
#define MODS_ESC_WRITE_SYSFS_NODE MODSIO(W, 111, MODS_SYSFS_NODE)
#define MODS_ESC_GET_PHYSICAL_ADDRESS_2 MODSIO(WR, 112, \
					       MODS_GET_PHYSICAL_ADDRESS_3)
#define MODS_ESC_GET_MAPPED_PHYSICAL_ADDRESS_3 MODSIO(WR, 113, \
						MODS_GET_PHYSICAL_ADDRESS_3)
#define MODS_ESC_REGISTER_IRQ_4 MODSIO(W, 114, MODS_REGISTER_IRQ_4)
#define MODS_ESC_QUERY_IRQ_3 MODSIO(R, 115, MODS_QUERY_IRQ_3)
#define MODS_ESC_SET_NUM_VF MODSIO(W, 116, MODS_SET_NUM_VF)
#define MODS_ESC_SET_TOTAL_VF MODSIO(W, 117, MODS_SET_NUM_VF)
#define MODS_ESC_PCI_SET_DMA_MASK MODSIO(W, 118, MODS_PCI_DMA_MASK)
#define MODS_ESC_GET_IOMMU_STATE_2 MODSIO(WR, 119, MODS_GET_IOMMU_STATE)
#define MODS_ESC_READ_MSR MODSIO(WR, 120, MODS_MSR)
#define MODS_ESC_WRITE_MSR MODSIO(W, 121, MODS_MSR)
#define MODS_ESC_EVAL_DEV_ACPI_METHOD_3 MODSIO(WR_BAD, 122, \
					       MODS_EVAL_DEV_ACPI_METHOD_3)
#define MODS_ESC_PCI_BUS_REMOVE_DEV MODSIO(W, 123, MODS_PCI_BUS_REMOVE_DEV)
#define MODS_ESC_ALLOC_PAGES_2 MODSIO(WR, 124, MODS_ALLOC_PAGES_2)
#define MODS_ESC_MERGE_PAGES MODSIO(WR, 125, MODS_MERGE_PAGES)
#define MODS_ESC_DEVICE_NUMA_INFO_3 MODSIO(WR, 126, MODS_DEVICE_NUMA_INFO_3)
#define MODS_ESC_PCI_BUS_RESCAN MODSIO(W, 127, MODS_PCI_BUS_RESCAN)
#define MODS_ESC_MAP_GPIO MODSIO(WR, 128, MODS_GPIO_INFO)
#define MODS_ESC_IOMMU_DMA_MAP_MEMORY MODSIO(W, 129, MODS_IOMMU_DMA_MAP_MEMORY)
#define MODS_ESC_IOMMU_DMA_UNMAP_MEMORY MODSIO(W, 130, \
					       MODS_IOMMU_DMA_MAP_MEMORY)
#define MODS_ESC_RESET_ASSERT MODSIO(W, 131, MODS_RESET_HANDLE)
#define MODS_ESC_GET_RESET_HANDLE MODSIO(WR, 132, MODS_GET_RESET_HANDLE)
#define MODS_ESC_SYSCTL_WRITE_INT MODSIO(W, 133, MODS_SYSCTL_INT)
#define MODS_ESC_PCI_RESET_FUNCTION MODSIO(W, 134, mods_pci_dev_2)
#define MODS_ESC_MODS_GET_DRIVER_STATS MODSIO(R, 135, MODS_GET_DRIVER_STATS)
#define MODS_ESC_BPMP_SET_PCIE_STATE MODSIO(W, 136, MODS_SET_PCIE_STATE)
#define MODS_ESC_BPMP_INIT_PCIE_EP_PLL MODSIO(W, 137, MODS_INIT_PCIE_EP_PLL)
#define MODS_ESC_GET_ACPI_DEV_CHILDREN MODSIO(WR, 138, MODS_GET_ACPI_DEV_CHILDREN)
#define MODS_ESC_SEND_TZ_MSG MODSIO(WR, 139, MODS_TZ_PARAMS)
#define MODS_ESC_OIST_STATUS MODSIO(WR, 140, MODS_TEGRA_OIST_STATUS)

#endif /* _UAPI_MODS_H_  */
