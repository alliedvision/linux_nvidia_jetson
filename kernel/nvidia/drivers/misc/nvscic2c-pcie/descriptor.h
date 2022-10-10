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

#ifndef __DESCRIPTOR_H__
#define __DESCRIPTOR_H__

#include <linux/errno.h>

#include "common.h"

/* Magic code for descriptor.*/
#define DESC_MAGIC_CODE_32BIT	(0x69152734)

/*
 * Format of Export Descriptor (at the moment)
 * 0xXXXXXXXXRRRREIII
 * 32bit(XXXXXXXX00000000): Reserved.
 * 04bit(00000000B0000000): Peer Board Id.
 * 04bit(000000000S000000): Peer SoC Id.
 * 04bit(0000000000C00000): Peer PCIe Controller Id.
 * 04bit(00000000000E0000): Endpoint Id.
 * 04bit(000000000000X000): Reserved.
 * 12bit(0000000000000III): Obj type(1bit) + Obj Id(11bits).
 *                          (Bit 11 : ObjType - Mem/Sync)
 *                          (Bit 0-10 : ObjId - Mem or Sync Obj Id)
 *
 * Board Id and SoC Id together can be a Node Id to allow for cases, where SoC
 * on a single board: [0-63] and number of boards: [0-3]. Essentially uniquely
 * identifying each SoC inter-connected within or across the boards.
 */

/*
 * Topology can have a:
 * A or a Set of boards
 *   - (Assumed [0, 15]).
 * Each Board can have a or a set of SoC(s)
 *   - ID : [0, 15].
 * Each SoC can have a or a set of PCIe controllers either in RP or EP mode.
 *   - ID:  [0, 15].
 * Each Controller can have a or a set of NvSciIpc INTER_CHIP endpoints.
 *   - ID:  [0, 15].
 * Each NvSciIpc INTER_CHIP can export either a Mem object or Sync object
 *   - STREAM_OBJ_TYPE_MEM or STREAM_OBJ_TYPE_SYNC
 *   - Type: [0, 1].
 * Each NvSciIpc INTER_CHIP can export a set of either Mem or Sync objects.
 *   - ID:   [0, 2047].
 */
struct descriptor_bit_t {
	u64 reserved1     : 32;
	u64 board_id      : 4;
	u64 soc_id        : 4;
	u64 cntrlr_id     : 4;
	u64 endpoint_id   : 4;
	u64 reserved2     : 4;
	u64 handle_type   : 1;
	u64 handle_id     : 11;
};

/* bit-field manipulation. */
union descriptor_t {
	u64 value;
	struct descriptor_bit_t bit;
};

/* Generate a descriptor (auth token) */
static inline u64
gen_desc(u32 peer_board_id, u32 peer_soc_id, u32 peer_cntrlr_id, u32 ep_id,
	 u32 handle_type, u32 handle_id)
{
	union descriptor_t desc;

	desc.bit.reserved1 = DESC_MAGIC_CODE_32BIT;
	desc.bit.board_id = peer_board_id;
	desc.bit.soc_id = peer_soc_id;
	desc.bit.cntrlr_id = peer_cntrlr_id;
	desc.bit.endpoint_id = ep_id;
	desc.bit.handle_type = handle_type;
	desc.bit.handle_id = handle_id;

	return desc.value;
}

/* Validate a descriptor (auth token) */
static inline int
validate_desc(u64 in_desc, u32 local_board_id, u32 local_soc_id,
	      u32 local_cntrlr_id, u32 ep_id)
{
	int ret = 0;
	union descriptor_t desc;

	desc.value = in_desc;
	if (desc.bit.reserved1 != DESC_MAGIC_CODE_32BIT ||
	    desc.bit.board_id != local_board_id ||
	    desc.bit.soc_id != local_soc_id ||
	    desc.bit.cntrlr_id != local_cntrlr_id ||
	    desc.bit.endpoint_id != ep_id) {
		ret = -EINVAL;
	}

	return ret;
}

/* return handle type embedded in the descriptor (auth token) */
static inline u32
get_handle_type_from_desc(u64 in_desc)
{
	union descriptor_t desc;

	desc.value = in_desc;
	return (u32)desc.bit.handle_type;
}

/*
 * Board Id, SoC Id, PCIe Controller Id should not be beyond 16 [0-15] - We have
 * reserved 4b each for boardId to generate export descriptor.
 */
#if MAX_BOARDS > (0xF + 1)
	#error MAX_BOARDS assumed to be less-than or equal to (16)
#endif
#if MAX_SOCS > (0xF + 1)
	#error MAX_SOCS assumed to be less-than or equal to (16)
#endif
#if MAX_PCIE_CNTRLRS > (0xF + 1)
	#error MAX_PCIE_CNTRLRS assumed to be less-than or equal to (16)
#endif

/*
 * Endpoints should not be beyond 16 [0-15] - We have reserved 4b for
 * endpoint Id to generate export descriptor (although we could use
 * up the reserved2 if needed).
 */
#if MAX_ENDPOINTS > (0xF + 1)
	#error MAX_ENDPOINTS to be less than or equal to (16)
#endif

/*
 * Memory or Sync object indicator in descriptor should not be beyond 1 [0-1].
 * The value must be less than 1 as the descriptor accounts just 1b (1bit).
 */
#if STREAM_OBJ_TYPE_MEM > (0x1)
	#error STREAM_OBJ_TYPE_MEM to be less-than or equal-to (1)
#endif
#if STREAM_OBJ_TYPE_SYNC > (0x1)
	#error STREAM_OBJ_TYPE_SYNC to be less-than or equal-to (1)
#endif

/*
 * Mem objects should not be beyond 2048 [0-2047] - We have reserved 11b for
 * Obj Id to generate export descriptor.
 */
#if MAX_STREAM_MEMOBJS > (0x7FF + 1)
	#error MAX_STREAM_MEMOBJS to be less than or equal to (2048)
#endif

/*
 * Sync objects should not be beyond 2048 [0-2047] - We have reserved 11b for
 * Obj Id to generate export descriptor.
 */
#if MAX_STREAM_SYNCOBJS > (0x7FF + 1)
	#error MAX_STREAM_SYNCOBJS to be less than or equal to (2048)
#endif
#endif //__DESCRIPTOR_H__
