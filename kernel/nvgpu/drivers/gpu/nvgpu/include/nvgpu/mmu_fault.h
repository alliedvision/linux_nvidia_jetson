/*
 * Copyright (c) 2017-2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef NVGPU_MMU_FAULT_H
#define NVGPU_MMU_FAULT_H

/**
 * @file
 *
 * GMMU fault buffer information.
 */

#include <nvgpu/types.h>

/** Index of non-replayable faults in GMMU fault information buffer. */
#define NVGPU_MMU_FAULT_NONREPLAY_INDX	0U

/** Index of replayable faults in GMMU fault information buffer. */
#define NVGPU_MMU_FAULT_REPLAY_INDX		1U

/** Maximum number of valid index in GMMU fault information buffer. */
#define NVGPU_MMU_FAULT_TYPE_NUM			2U

/** Register index of non-replayable faults in BAR0 aperture. */
#define NVGPU_MMU_FAULT_NONREPLAY_REG_INDX		0U

/** Register index of replayable faults in BAR0 aperture. */
#define NVGPU_MMU_FAULT_REPLAY_REG_INDX			1U

/** State which is used for disabling the GMMU fault hardware support. */
#define NVGPU_MMU_FAULT_BUF_DISABLED			0U

/** State which is used for enabling the GMMU fault hardware support. */
#define NVGPU_MMU_FAULT_BUF_ENABLED			1U

/** S/w defined mmu engine id type. */
#define NVGPU_MMU_ENGINE_ID_TYPE_OTHER			0U

/** S/w defined mmu engine id type. */
#define NVGPU_MMU_ENGINE_ID_TYPE_BAR2			1U

/** S/w defined mmu engine id type. */
#define NVGPU_MMU_ENGINE_ID_TYPE_PHYSICAL		2U

/**
 * Forward declared opaque placeholder type that does not really exist, but
 * helps the compiler about getting types right.
 */
struct nvgpu_channel;

/**
 * This structure describes the various debug information reported
 * by GMMU during MMU fault exceptions.
 *
 *Fault buffer format
 *-------------------
 *
 * 31    28     24 23           16 15            8 7     4       0
 *.-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-.
 *|              inst_lo                  |0 0|apr|0 0 0 0 0 0 0 0|
 *`-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-'
 *|                             inst_hi                           |
 *`-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-'
 *|              addr_31_12               |                   |AP |
 *`-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-'
 *|                            addr_63_32                         |
 *`-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-'
 *|                          timestamp_lo                         |
 *`-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-'
 *|                          timestamp_hi                         |
 *`-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-'
 *|                           (reserved)        |    engine_id    |
 *`-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-'
 *|V|R|P|  gpc_id |0 0 0|t|0|acctp|0|   client    |RF0 0|faulttype|
 *
 */
struct mmu_fault_info {
	/** The faulting context's instance pointer physical address. */
	u64	inst_ptr;

	/**
	 * Aperture (SYSMEM or VIDMEM) of faulting context's
	 * instance pointer.
	 */
	u32	inst_aperture;

	/** Faulting GMMU virtual address. */
	u64	fault_addr;

	/** Aperture (SYSMEM or VIDMEM) of faulting GMMU virtual address. */
	u32	fault_addr_aperture;

	/**
	 * The time instant at which the fault has occurred. This variable
	 * contains the LSB 32-bit value of actual gpu time.
	 */
	u32	timestamp_lo;

	/**
	 * The time instant at which the fault has occurred. This variable
	 * contains the MSB 32-bit value of actual gpu time.
	 */
	u32	timestamp_hi;

	/**
	 * The MMU engine ID's (i.e. virtual address spaces) has experienced
	 * a GMMU fault (i.e. GRAPHICS, CE0, HOST0, ...).
	 */
	u32	mmu_engine_id;

	/**
	 * The s/w defined mmu_engine_id type (BAR2, PHYSICAL).
	 */

	u32	mmu_engine_id_type;

	/** GPC id if client type is gpc. For gv11b, NUM_GPCS = 1. */
	u32	gpc_id;

	/**
	 * Client type indicates whether the faulting request originated
	 * in a GPC, or if it came from another type of HUB client.
	 */
	u32	client_type;

	/**
	 * Indicates which MMU client generated the faulting request.
	 * Index in #gv11b_gpc_client_descs/#gv11b_hub_client_descs
	 * const string array which tells the user understandable string of
	 * faulting engine and subengine information
	 * (i.e. gr copy, ce shim, pe 0, ...).
	 */
	u32	client_id;

	/**
	 * The fault_type field indicates whether the faulting request was
	 * a read or a  write.
	 */
	u32	fault_type;

	/**
	 * Indicates the type of the faulting request.
	 * Index in #gv11b_fault_access_type_descs
	 * const string array which tells the user understandable string of
	 * access type information
	 * (i.e. virt read, virt write, phys read, phys write, ...).
	 */
	u32	access_type;

	/**
	 * This fault indicates an illegal access to a protected region.
	 */
	u32	protected_mode;

	/**
	 * This fault indicates whether the fault type is replayable or
	 * non-replayable.
	 */
	bool	replayable_fault;

	/**
	 * Set to true if replayable fault is enabled for any client in the
	 * instance block.  It does not indicate whether the fault is
	 * replayable.
	 */
	u32	replay_fault_en;

	/**
	 * Indicates that this current buffer entry is valid or not.
	 */
	bool	valid;

	/** PBDMA id if faulting MMU client is a PBDMA. */
	u32	faulted_pbdma;

	/** Engine id if faulting MMU client is an engine (GR, CE, ...). */
	u32	faulted_engine;

	/**
	 * Sub engine id if faulting MMU client is an engine
	 * (i.e. GPC_L1_0, GPC_PE_0, ...).
	 */
	u32	faulted_subid;

	/** Faulting channel identifier. */
	u32	chid;

	/** Pointer to a faulting channel structure. */
	struct nvgpu_channel *refch;

	/**
	 * Pointer to a client type description in
	 * #gv11b_fault_client_type_descs const string array
	 * (gpc and hub).
	 */
	const char *client_type_desc;

	/**
	 * Pointer to a client access type description in
	 * #gv11b_fault_access_type_descs const string array
	 * (i.e. virt read, virt write, phys read, phys write, ...).
	 */
	const char *fault_type_desc;

	/**
	 * Pointer to a client type description in
	 * #gv11b_gpc_client_descs/#gv11b_hub_client_descs const string array
	 * (i.e. gr copy, ce shim, pe 0, ...).
	 */
	const char *client_id_desc;
};

#endif /* NVGPU_MMU_FAULT_H */
