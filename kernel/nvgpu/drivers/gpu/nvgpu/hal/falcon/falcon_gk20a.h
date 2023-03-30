/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_FALCON_GK20A_H
#define NVGPU_FALCON_GK20A_H

#include <nvgpu/falcon.h>

/* Falcon Register index */
#define FALCON_REG_R0		(0U)
#define FALCON_REG_R1		(1U)
#define FALCON_REG_R2		(2U)
#define FALCON_REG_R3		(3U)
#define FALCON_REG_R4		(4U)
#define FALCON_REG_R5		(5U)
#define FALCON_REG_R6		(6U)
#define FALCON_REG_R7		(7U)
#define FALCON_REG_R8		(8U)
#define FALCON_REG_R9		(9U)
#define FALCON_REG_R10		(10U)
#define FALCON_REG_R11		(11U)
#define FALCON_REG_R12		(12U)
#define FALCON_REG_R13		(13U)
#define FALCON_REG_R14		(14U)
#define FALCON_REG_R15		(15U)
#define FALCON_REG_IV0		(16U)
#define FALCON_REG_IV1		(17U)
#define FALCON_REG_UNDEFINED	(18U)
#define FALCON_REG_EV		(19U)
#define FALCON_REG_SP		(20U)
#define FALCON_REG_PC		(21U)
#define FALCON_REG_IMB		(22U)
#define FALCON_REG_DMB		(23U)
#define FALCON_REG_CSW		(24U)
#define FALCON_REG_CCR		(25U)
#define FALCON_REG_SEC		(26U)
#define FALCON_REG_CTX		(27U)
#define FALCON_REG_EXCI		(28U)
#define FALCON_REG_RSVD0	(29U)
#define FALCON_REG_RSVD1	(30U)
#define FALCON_REG_RSVD2	(31U)
#define FALCON_REG_SIZE		(32U)

#define FALCON_DMEM_BLKSIZE2	8U

u32 gk20a_falcon_dmemc_blk_mask(void);
u32 gk20a_falcon_imemc_blk_field(u32 blk);
void gk20a_falcon_reset(struct nvgpu_falcon *flcn);
bool gk20a_is_falcon_cpu_halted(struct nvgpu_falcon *flcn);
bool gk20a_is_falcon_idle(struct nvgpu_falcon *flcn);
bool gk20a_is_falcon_scrubbing_done(struct nvgpu_falcon *flcn);
u32 gk20a_falcon_get_mem_size(struct nvgpu_falcon *flcn,
		enum falcon_mem_type mem_type);
u8 gk20a_falcon_get_ports_count(struct nvgpu_falcon *flcn,
		enum falcon_mem_type mem_type);
int gk20a_falcon_copy_to_dmem(struct nvgpu_falcon *flcn,
		u32 dst, u8 *src, u32 size, u8 port);
int gk20a_falcon_copy_to_imem(struct nvgpu_falcon *flcn, u32 dst,
		u8 *src, u32 size, u8 port, bool sec, u32 tag);
void gk20a_falcon_bootstrap(struct nvgpu_falcon *flcn,
	u32 boot_vector);
u32 gk20a_falcon_mailbox_read(struct nvgpu_falcon *flcn,
		u32 mailbox_index);
void gk20a_falcon_mailbox_write(struct nvgpu_falcon *flcn,
		u32 mailbox_index, u32 data);
void gk20a_falcon_set_irq(struct nvgpu_falcon *flcn, bool enable,
				 u32 intr_mask, u32 intr_dest);

#ifdef CONFIG_NVGPU_FALCON_DEBUG
void gk20a_falcon_dump_stats(struct nvgpu_falcon *flcn);
#endif

#if defined(CONFIG_NVGPU_FALCON_DEBUG) || defined(CONFIG_NVGPU_FALCON_NON_FUSA)
int gk20a_falcon_copy_from_dmem(struct nvgpu_falcon *flcn,
		u32 src, u8 *dst, u32 size, u8 port);
#endif

#ifdef CONFIG_NVGPU_FALCON_NON_FUSA
bool gk20a_falcon_clear_halt_interrupt_status(struct nvgpu_falcon *flcn);
int gk20a_falcon_copy_from_imem(struct nvgpu_falcon *flcn, u32 src,
	u8 *dst, u32 size, u8 port);
void gk20a_falcon_get_ctls(struct nvgpu_falcon *flcn, u32 *sctl,
				  u32 *cpuctl);
#endif

#endif /* NVGPU_FALCON_GK20A_H */
