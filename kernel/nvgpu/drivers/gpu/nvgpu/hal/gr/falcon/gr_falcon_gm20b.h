/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_GR_FALCON_GM20B_H
#define NVGPU_GR_FALCON_GM20B_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_fecs_method_op;
struct nvgpu_fecs_host_intr_status;
struct nvgpu_gr_falcon_query_sizes;

void gm20b_gr_falcon_fecs_ctxsw_clear_mailbox(struct gk20a *g,
					u32 reg_index, u32 clear_val);
u32 gm20b_gr_falcon_read_mailbox_fecs_ctxsw(struct gk20a *g, u32 reg_index);
void gm20b_gr_falcon_fecs_host_clear_intr(struct gk20a *g, u32 fecs_intr);
u32 gm20b_gr_falcon_fecs_host_intr_status(struct gk20a *g,
			struct nvgpu_fecs_host_intr_status *fecs_host_intr);
u32 gm20b_gr_falcon_fecs_base_addr(void);
u32 gm20b_gr_falcon_gpccs_base_addr(void);
void gm20b_gr_falcon_dump_stats(struct gk20a *g);
u32 gm20b_gr_falcon_get_fecs_ctx_state_store_major_rev_id(struct gk20a *g);
u32 gm20b_gr_falcon_get_fecs_ctxsw_mailbox_size(void);
void gm20b_gr_falcon_start_gpccs(struct gk20a *g);
void gm20b_gr_falcon_start_fecs(struct gk20a *g);
u32 gm20b_gr_falcon_get_gpccs_start_reg_offset(void);
void gm20b_gr_falcon_bind_instblk(struct gk20a *g,
				struct nvgpu_mem *mem, u64 inst_ptr);
int gm20b_gr_falcon_wait_mem_scrubbing(struct gk20a *g);
int gm20b_gr_falcon_wait_ctxsw_ready(struct gk20a *g);
int gm20b_gr_falcon_submit_fecs_method_op(struct gk20a *g,
	struct nvgpu_fecs_method_op op, u32 flags);
int gm20b_gr_falcon_ctrl_ctxsw(struct gk20a *g, u32 fecs_method,
						u32 data, u32 *ret_val);
int gm20b_gr_falcon_ctrl_ctxsw_internal(struct gk20a *g, u32 fecs_method,
						u32 data, u32 *ret_val);
void gm20b_gr_falcon_set_current_ctx_invalid(struct gk20a *g);
u32 gm20b_gr_falcon_get_current_ctx(struct gk20a *g);
u32 gm20b_gr_falcon_get_ctx_ptr(u32 ctx);
u32 gm20b_gr_falcon_get_fecs_current_ctx_data(struct gk20a *g,
						struct nvgpu_mem *inst_block);
int gm20b_gr_falcon_init_ctx_state(struct gk20a *g,
		struct nvgpu_gr_falcon_query_sizes *sizes);
u32 gm20b_gr_falcon_read_status0_fecs_ctxsw(struct gk20a *g);
u32 gm20b_gr_falcon_read_status1_fecs_ctxsw(struct gk20a *g);
#ifdef CONFIG_NVGPU_GRAPHICS
int gm20b_gr_falcon_submit_fecs_sideband_method_op(struct gk20a *g,
				struct nvgpu_fecs_method_op op);
#endif
#ifdef CONFIG_NVGPU_GR_FALCON_NON_SECURE_BOOT
void gm20b_gr_falcon_load_ctxsw_ucode_header(struct gk20a *g,
	u32 reg_offset, u32 boot_signature, u32 addr_code32,
	u32 addr_data32, u32 code_size, u32 data_size);
void gm20b_gr_falcon_load_ctxsw_ucode_boot(struct gk20a *g,
	u32 reg_offset, u32 boot_entry, u32 addr_load32, u32 blocks,
	u32 dst);
void gm20b_gr_falcon_load_gpccs_dmem(struct gk20a *g,
			const u32 *ucode_u32_data, u32 ucode_u32_size);
void gm20b_gr_falcon_gpccs_dmemc_write(struct gk20a *g, u32 port, u32 offs,
	u32 blk, u32 ainc);
void gm20b_gr_falcon_load_fecs_dmem(struct gk20a *g,
			const u32 *ucode_u32_data, u32 ucode_u32_size);
void gm20b_gr_falcon_fecs_dmemc_write(struct gk20a *g, u32 reg_offset, u32 port,
	u32 offs, u32 blk, u32 ainc);
void gm20b_gr_falcon_load_gpccs_imem(struct gk20a *g,
			const u32 *ucode_u32_data, u32 ucode_u32_size);
void gm20b_gr_falcon_gpccs_imemc_write(struct gk20a *g, u32 port, u32 offs,
	u32 blk, u32 ainc);
void gm20b_gr_falcon_load_fecs_imem(struct gk20a *g,
			const u32 *ucode_u32_data, u32 ucode_u32_size);
void gm20b_gr_falcon_fecs_imemc_write(struct gk20a *g, u32 port, u32 offs,
	u32 blk, u32 ainc);
void gm20b_gr_falcon_start_ucode(struct gk20a *g);
void gm20b_gr_falcon_fecs_host_int_enable(struct gk20a *g);
#endif
#ifdef CONFIG_NVGPU_SIM
void gm20b_gr_falcon_configure_fmodel(struct gk20a *g);
#endif
#endif /* NVGPU_GR_FALCON_GM20B_H */
