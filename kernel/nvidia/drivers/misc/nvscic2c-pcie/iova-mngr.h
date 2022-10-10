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

#ifndef __IOVA_MNGR_H__
#define __IOVA_MNGR_H__

#include <linux/types.h>

/*
 * iova_mngr_block_reserve
 *
 * Reserves a block from the free IOVA regions. Once reserved, the block
 * is marked reserved and appended in the reserved list. Use
 * iova_mngr_block_get_address to fetch the address of the block reserved.
 */
int
iova_mngr_block_reserve(void *mngr_handle, size_t size,
			u64 *address, size_t *offset,
			void **block_handle);

/*
 * iova_mngr_block_release
 *
 * Release an already reserved IOVA block/chunk by the caller back to
 * free list.
 */
int
iova_mngr_block_release(void *mngr_handle, void **block_handle);

/*
 * iova_mngr_print
 *
 * DEBUG only.
 *
 * Helper function to print all the reserved and free blocks with
 * their names, size and start address.
 */
void iova_mngr_print(void *handle);

/*
 * iova_mngr_init
 *
 * Initialises the IOVA space manager with the base address + size
 * provided. IOVA manager would use two lists for book-keeping reserved
 * memory blocks and free memory blocks.
 *
 * When initialised all of the IOVA region: base_address + size is free.
 */
int
iova_mngr_init(char *name, u64 base_address, size_t size, void **mngr_handle);

/*
 * iova_mngr_deinit
 *
 * deinitialize the IOVA space manager. Any blocks unreturned from the client
 * (caller) shall become dangling.
 */
void
iova_mngr_deinit(void **handle);

#endif //__IOVA_MNGR_H__
