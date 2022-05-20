/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef NVGPU_POSIX_MOCK_REGS_H
#define NVGPU_POSIX_MOCK_REGS_H

#include <nvgpu/types.h>

struct gk20a;

struct nvgpu_mock_iospace {
	u32             base;
	size_t          size;
	const uint32_t *data;
};

#define MOCK_REGS_GR		0U
#define MOCK_REGS_FUSE		1U
#define MOCK_REGS_MASTER	2U
#define MOCK_REGS_TOP		3U
#define MOCK_REGS_FIFO		4U
#define MOCK_REGS_PRI		5U
#define MOCK_REGS_PBDMA		6U
#define MOCK_REGS_CCSR		7U
#define MOCK_REGS_USERMODE	8U
#define MOCK_REGS_CE		9U
#define MOCK_REGS_PBUS		10U
#define MOCK_REGS_HSHUB		11U
#define MOCK_REGS_FB		12U
#define MOCK_REGS_LAST		13U

/**
 * Load a mocked register list into the passed IO space description.
 */
int nvgpu_get_mock_reglist(struct gk20a *g, u32 reg_space,
			   struct nvgpu_mock_iospace *iospace);

#endif
