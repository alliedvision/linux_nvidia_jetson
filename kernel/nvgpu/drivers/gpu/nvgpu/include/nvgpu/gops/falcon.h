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
#ifndef NVGPU_GOPS_FALCON_H
#define NVGPU_GOPS_FALCON_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * Falcon HAL interface.
 */
struct gk20a;

struct gops_falcon {
	/** @cond DOXYGEN_SHOULD_SKIP_THIS */

	int (*falcon_sw_init)(struct gk20a *g, u32 flcn_id);
	void (*falcon_sw_free)(struct gk20a *g, u32 flcn_id);
	void (*reset)(struct nvgpu_falcon *flcn);
	bool (*is_falcon_cpu_halted)(struct nvgpu_falcon *flcn);
	bool (*is_falcon_idle)(struct nvgpu_falcon *flcn);
	bool (*is_falcon_scrubbing_done)(struct nvgpu_falcon *flcn);
	u32 (*get_mem_size)(struct nvgpu_falcon *flcn,
		enum falcon_mem_type mem_type);
	u8 (*get_ports_count)(struct nvgpu_falcon *flcn,
		enum falcon_mem_type mem_type);

	int (*copy_to_dmem)(struct nvgpu_falcon *flcn,
			    u32 dst, u8 *src, u32 size, u8 port);
	int (*copy_to_imem)(struct nvgpu_falcon *flcn,
			    u32 dst, u8 *src, u32 size, u8 port,
			    bool sec, u32 tag);
	void (*set_bcr)(struct nvgpu_falcon *flcn);
	void (*dump_brom_stats)(struct nvgpu_falcon *flcn);
	u32  (*get_brom_retcode)(struct nvgpu_falcon *flcn);
	bool (*is_priv_lockdown)(struct nvgpu_falcon *flcn);
	u32 (*dmemc_blk_mask)(void);
	bool (*check_brom_passed)(u32 retcode);
	bool (*check_brom_failed)(u32 retcode);
	void (*brom_config)(struct nvgpu_falcon *flcn, u64 fmc_code_addr,
			u64 fmc_data_addr, u64 manifest_addr);
	u32 (*imemc_blk_field)(u32 blk);
	void (*bootstrap)(struct nvgpu_falcon *flcn,
			 u32 boot_vector);
	u32 (*mailbox_read)(struct nvgpu_falcon *flcn,
			    u32 mailbox_index);
	void (*mailbox_write)(struct nvgpu_falcon *flcn,
			      u32 mailbox_index, u32 data);
	void (*set_irq)(struct nvgpu_falcon *flcn, bool enable,
			u32 intr_mask, u32 intr_dest);
#ifdef CONFIG_NVGPU_FALCON_DEBUG
	void (*dump_falcon_stats)(struct nvgpu_falcon *flcn);
#endif
#if defined(CONFIG_NVGPU_FALCON_DEBUG) || defined(CONFIG_NVGPU_FALCON_NON_FUSA)
	int (*copy_from_dmem)(struct nvgpu_falcon *flcn,
			      u32 src, u8 *dst, u32 size, u8 port);
#endif
#ifdef CONFIG_NVGPU_FALCON_NON_FUSA
	bool (*clear_halt_interrupt_status)(struct nvgpu_falcon *flcn);
	int (*copy_from_imem)(struct nvgpu_falcon *flcn,
			      u32 src, u8 *dst, u32 size, u8 port);
	void (*get_falcon_ctls)(struct nvgpu_falcon *flcn,
				u32 *sctl, u32 *cpuctl);
#endif

	/** @endcond DOXYGEN_SHOULD_SKIP_THIS */

};

#endif
