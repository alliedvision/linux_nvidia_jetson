/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef DCE_MEMORY_IDS_H
#define DCE_MEMORY_IDS_H

/*
 * Defines the varous memory IDs used for mapping memory regions
 * for DCE.
 *
 * XXX: TODO
 * Rename some of the IDs to better represent what they're used for
 */

#define DCE_MAP_DRAM_ID		0U	// FW DRAM
#define DCE_MAP_BPMP_ID		1U	// BPMP communications area
#define DCE_MAP_CONFIG_DATA_ID	2U	// device tree
#define DCE_MAP_IPC_ID		3U	// memory region for IPC
#define DCE_MAP_MSG_ID		4U	// extra: rename at some point
#define DCE_MAP_UTILITY_ID	5U	// extra: rename at some point
#define DCE_MAP_RM_ID		6U	// RM communications area
#define DCE_MAP_RM_DATA_ID	7U	// extra RM data area

#endif
