/*
 * Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
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
