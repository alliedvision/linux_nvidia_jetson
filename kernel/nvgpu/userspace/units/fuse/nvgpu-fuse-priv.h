/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __UNIT_NVGPU_FUSE_PRIV_H__
#define __UNIT_NVGPU_FUSE_PRIV_H__

extern u32 gcplex_config;
int read_gcplex_config_fuse_pass(struct gk20a *g, u32 *val);
int read_gcplex_config_fuse_fail(struct gk20a *g, u32 *val);

struct fuse_test_args {
	u32 gpu_arch;
	u32 gpu_impl;
	u32 fuse_base_addr;
	u32 sec_fuse_addr;
};

#define GM20B_FUSE_REG_BASE			0x00021000U
#define GM20B_TOP_NUM_GPCS			(GM20B_FUSE_REG_BASE+0x1430U)
#define GM20B_MAX_GPC_COUNT                     24U
#endif /* __UNIT_NVGPU_FUSE_PRIV_H__ */
