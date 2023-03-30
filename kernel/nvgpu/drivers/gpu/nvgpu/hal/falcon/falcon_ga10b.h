/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_FALCON_GA10B_H
#define NVGPU_FALCON_GA10B_H

#include <nvgpu/falcon.h>

#define FALCON_DMEM_BLKSIZE2	8U
u32 ga10b_falcon_dmemc_blk_mask(void);
u32 ga10b_falcon_imemc_blk_field(u32 blk);
u32 ga10b_falcon_get_mem_size(struct nvgpu_falcon *flcn,
		enum falcon_mem_type mem_type);
bool ga10b_falcon_is_cpu_halted(struct nvgpu_falcon *flcn);
void ga10b_falcon_set_bcr(struct nvgpu_falcon *flcn);
void ga10b_falcon_bootstrap(struct nvgpu_falcon *flcn, u32 boot_vector);
void ga10b_falcon_dump_brom_stats(struct nvgpu_falcon *flcn);
u32  ga10b_falcon_get_brom_retcode(struct nvgpu_falcon *flcn);
bool ga10b_falcon_is_priv_lockdown(struct nvgpu_falcon *flcn);
bool ga10b_falcon_check_brom_passed(u32 retcode);
bool ga10b_falcon_check_brom_failed(u32 retcode);
void ga10b_falcon_brom_config(struct nvgpu_falcon *flcn, u64 fmc_code_addr,
		u64 fmc_data_addr, u64 manifest_addr);
void ga10b_falcon_dump_stats(struct nvgpu_falcon *flcn);
bool ga10b_is_falcon_scrubbing_done(struct nvgpu_falcon *flcn);
bool ga10b_is_falcon_idle(struct nvgpu_falcon *flcn);
#endif /* NVGPU_FALCON_GA10B_H */
