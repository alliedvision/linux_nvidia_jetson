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
#include <nvgpu/gk20a.h>
#include <nvgpu/io.h>
#include <nvgpu/falcon.h>
#include <nvgpu/string.h>
#include <nvgpu/static_analysis.h>

#include "falcon_gk20a.h"

#include <nvgpu/hw/gm20b/hw_falcon_gm20b.h>

u32 gk20a_falcon_dmemc_blk_mask(void)
{
	return falcon_falcon_dmemc_blk_m();
}

u32 gk20a_falcon_imemc_blk_field(u32 blk)
{
	return falcon_falcon_imemc_blk_f(blk);
}

void gk20a_falcon_reset(struct nvgpu_falcon *flcn)
{
	u32 unit_status = 0U;

	/* do falcon CPU hard reset */
	unit_status = nvgpu_falcon_readl(flcn, falcon_falcon_cpuctl_r());
	nvgpu_falcon_writel(flcn, falcon_falcon_cpuctl_r(),
			    (unit_status | falcon_falcon_cpuctl_hreset_f(1)));
}

bool gk20a_is_falcon_cpu_halted(struct nvgpu_falcon *flcn)
{
	return ((nvgpu_falcon_readl(flcn, falcon_falcon_cpuctl_r()) &
			falcon_falcon_cpuctl_halt_intr_m()) != 0U);
}

bool gk20a_is_falcon_idle(struct nvgpu_falcon *flcn)
{
	u32 unit_status = 0U;
	bool status = false;

	unit_status = nvgpu_falcon_readl(flcn, falcon_falcon_idlestate_r());

	if ((falcon_falcon_idlestate_falcon_busy_v(unit_status) == 0U) &&
		(falcon_falcon_idlestate_ext_busy_v(unit_status) == 0U)) {
		status = true;
	} else {
		status = false;
	}

	return status;
}

bool gk20a_is_falcon_scrubbing_done(struct nvgpu_falcon *flcn)
{
	u32 unit_status = 0U;
	bool status = false;

	unit_status = nvgpu_falcon_readl(flcn, falcon_falcon_dmactl_r());

	if ((unit_status &
		(falcon_falcon_dmactl_dmem_scrubbing_m() |
		 falcon_falcon_dmactl_imem_scrubbing_m())) != 0U) {
		status = false;
	} else {
		status = true;
	}

	return status;
}

u32 gk20a_falcon_get_mem_size(struct nvgpu_falcon *flcn,
		enum falcon_mem_type mem_type)
{
	u32 mem_size = 0U;
	u32 hwcfg_val = 0U;

	hwcfg_val = nvgpu_falcon_readl(flcn, falcon_falcon_hwcfg_r());

	if (mem_type == MEM_DMEM) {
		mem_size = falcon_falcon_hwcfg_dmem_size_v(hwcfg_val)
			<< FALCON_DMEM_BLKSIZE2;
	} else {
		mem_size = falcon_falcon_hwcfg_imem_size_v(hwcfg_val)
			<< FALCON_DMEM_BLKSIZE2;
	}

	return mem_size;
}

u8 gk20a_falcon_get_ports_count(struct nvgpu_falcon *flcn,
		enum falcon_mem_type mem_type)
{
	u8 ports = 0U;
	u32 hwcfg1_val = 0U;

	hwcfg1_val = nvgpu_falcon_readl(flcn, falcon_falcon_hwcfg1_r());

	if (mem_type == MEM_DMEM) {
		ports = (u8) falcon_falcon_hwcfg1_dmem_ports_v(hwcfg1_val);
	} else {
		ports = (u8) falcon_falcon_hwcfg1_imem_ports_v(hwcfg1_val);
	}

	return ports;
}

#define FALCON_UNALIGNED_MEMCPY_BLOCK_SIZE	256U

static void falcon_copy_to_dmem_unaligned_src(struct nvgpu_falcon *flcn,
					      u8 *src, u32 size, u8 port)
{
	u32 src_tmp[FALCON_UNALIGNED_MEMCPY_BLOCK_SIZE];
	u32 bytes_extra = 0U;
	u32 elem_size = 0U;
	u32 offset = 0U;
	u32 elems = 0U;
	u32 i = 0U;

	while ((offset + sizeof(src_tmp)) <= size) {
		nvgpu_memcpy((u8 *)&src_tmp[0], &src[offset],
			     sizeof(src_tmp));
		for (i = 0; i < ARRAY_SIZE(src_tmp); i++) {
			nvgpu_falcon_writel(flcn,
					    falcon_falcon_dmemd_r(port),
					    src_tmp[i]);
		}
		offset += (u32) sizeof(src_tmp);
	}

	if (offset < size) {
		bytes_extra = size - offset;
		elem_size =
			nvgpu_safe_cast_u64_to_u32(sizeof(src_tmp[0]));
		elems = bytes_extra / elem_size;

		nvgpu_memcpy((u8 *)&src_tmp[0], &src[offset],
			     (u64)elems * elem_size);
		for (i = 0; i < elems; i++) {
			nvgpu_falcon_writel(flcn,
					    falcon_falcon_dmemd_r(port),
					    src_tmp[i]);
		}
	}
}

int gk20a_falcon_copy_to_dmem(struct nvgpu_falcon *flcn,
		u32 dst, u8 *src, u32 size, u8 port)
{
	struct gk20a *g = flcn->g;
	u32 i = 0U, words = 0U, bytes = 0U;
	u32 data = 0U, addr_mask = 0U;
	u32 *src_u32 = NULL;

	nvgpu_log_fn(flcn->g, "dest dmem offset - %x, size - %x", dst, size);

	words = size >> 2U;
	bytes = size & 0x3U;

	addr_mask = falcon_falcon_dmemc_offs_m() |
		g->ops.falcon.dmemc_blk_mask();

	dst &= addr_mask;

	nvgpu_falcon_writel(flcn, falcon_falcon_dmemc_r(port),
			    dst | falcon_falcon_dmemc_aincw_f(1));

	if (likely(nvgpu_mem_is_word_aligned(flcn->g, src))) {
NVGPU_COV_WHITELIST(deviate, NVGPU_MISRA(Rule, 11_3), "TID-415")
		src_u32 = (u32 *)src;

		for (i = 0; i < words; i++) {
			nvgpu_falcon_writel(flcn, falcon_falcon_dmemd_r(port),
					    src_u32[i]);
		}
	} else {
		falcon_copy_to_dmem_unaligned_src(flcn, src, size, port);
	}

	if (bytes > 0U) {
		data = 0;
		nvgpu_memcpy((u8 *)&data, &src[words << 2U], bytes);
		nvgpu_falcon_writel(flcn, falcon_falcon_dmemd_r(port), data);
	}

	size = NVGPU_ALIGN(size, 4U);
	data = nvgpu_falcon_readl(flcn, falcon_falcon_dmemc_r(port)) &
		addr_mask;
	if (data != (nvgpu_safe_add_u32(dst, size) & addr_mask)) {
		nvgpu_warn(flcn->g, "copy failed. bytes written %d, expected %d",
			data - dst, size);
		return -EIO;
	}

	return 0;
}

static void falcon_copy_to_imem_unaligned_src(struct nvgpu_falcon *flcn,
					      u8 *src, u32 size, u8 port,
					      u32 tag)
{
	u32 src_tmp[FALCON_UNALIGNED_MEMCPY_BLOCK_SIZE];
	u32 bytes_extra = 0U;
	u32 elem_size = 0U;
	u32 offset = 0U;
	u32 elems = 0U;
	u32 i = 0U;
	u32 j = 0U;

	while ((offset + sizeof(src_tmp)) <= size) {
		nvgpu_memcpy((u8 *)&src_tmp[0], &src[offset],
			     sizeof(src_tmp));
		if (!flcn->is_falcon2_enabled) {
			for (i = 0U; i < ARRAY_SIZE(src_tmp); i++) {
				if ((j++ % 64U) == 0U) {
					/* tag is always 256B aligned */
					nvgpu_falcon_writel(flcn,
							falcon_falcon_imemt_r(port), tag);
					tag = nvgpu_safe_add_u32(tag, 1U);
				}

				nvgpu_falcon_writel(flcn, falcon_falcon_imemd_r(port),
					src_tmp[i]);
			}
		} else {
			for (i = 0U; i < ARRAY_SIZE(src_tmp); i++) {
				nvgpu_falcon_writel(flcn, falcon_falcon_imemd_r(port),
						src_tmp[i]);
			}
		}
		offset += (u32) sizeof(src_tmp);
	}

	if (offset < size) {
		bytes_extra = size - offset;
		elem_size =
			nvgpu_safe_cast_u64_to_u32(sizeof(src_tmp[0]));
		elems = bytes_extra / elem_size;

		nvgpu_memcpy((u8 *)&src_tmp[0], &src[offset],
			     (u64)elems * elem_size);
		if (!flcn->is_falcon2_enabled) {
			for (i = 0U; i < elems; i++) {
				if ((j++ % 64U) == 0U) {
					/* tag is always 256B aligned */
					nvgpu_falcon_writel(flcn,
						falcon_falcon_imemt_r(port), tag);
					tag = nvgpu_safe_add_u32(tag, 1U);
				}

				nvgpu_falcon_writel(flcn, falcon_falcon_imemd_r(port),
					src_tmp[i]);
			}
		} else {
			for (i = 0U; i < elems; i++) {
				nvgpu_falcon_writel(flcn, falcon_falcon_imemd_r(port),
						src_tmp[i]);
			}
		}
	}

	/* WARNING : setting remaining bytes in block to 0x0 */
	while ((j % 64U) != 0U) {
		nvgpu_falcon_writel(flcn,
				    falcon_falcon_imemd_r(port), 0);
		j++;
	}
}

int gk20a_falcon_copy_to_imem(struct nvgpu_falcon *flcn, u32 dst,
		u8 *src, u32 size, u8 port, bool sec, u32 tag)
{
	struct gk20a *g = flcn->g;
	u32 *src_u32 = NULL;
	u32 words = 0U;
	u32 blk = 0U;
	u32 i = 0U;

	nvgpu_log_info(flcn->g, "upload %d bytes to 0x%x", size, dst);

	words = size >> 2U;
	blk = dst >> 8;

	nvgpu_log_info(flcn->g, "upload %d words to 0x%x block %d, tag 0x%x",
			words, dst, blk, tag);

	nvgpu_falcon_writel(flcn, falcon_falcon_imemc_r(port),
			falcon_falcon_imemc_offs_f(dst >> 2) |
			g->ops.falcon.imemc_blk_field(blk) |
			/* Set Auto-Increment on write */
			falcon_falcon_imemc_aincw_f(1) |
			falcon_falcon_imemc_secure_f(sec ? 1U : 0U));

	if (likely(nvgpu_mem_is_word_aligned(flcn->g, src))) {
NVGPU_COV_WHITELIST(deviate, NVGPU_MISRA(Rule, 11_3), "TID-415")
		src_u32 = (u32 *)src;

		if (!flcn->is_falcon2_enabled) {
			for (i = 0U; i < words; i++) {
				if (((i % 64U) == 0U)) {
				/* tag is always 256B aligned */
					nvgpu_falcon_writel(flcn,
							falcon_falcon_imemt_r(port), tag);
					tag = nvgpu_safe_add_u32(tag, 1U);
				}

				nvgpu_falcon_writel(flcn, falcon_falcon_imemd_r(port),
					    src_u32[i]);
			}
		} else {
			for (i = 0U; i < words; i++) {
				nvgpu_falcon_writel(flcn, falcon_falcon_imemd_r(port),
					src_u32[i]);
			}
		}

		/* WARNING : setting remaining bytes in block to 0x0 */
		while ((i % 64U) != 0U) {
			nvgpu_falcon_writel(flcn,
					    falcon_falcon_imemd_r(port), 0);
			i++;
		}
	} else {
		falcon_copy_to_imem_unaligned_src(flcn, src, size,
						  port, tag);
	}

	return 0;
}

void gk20a_falcon_bootstrap(struct nvgpu_falcon *flcn,
	u32 boot_vector)
{
	nvgpu_log_info(flcn->g, "boot vec 0x%x", boot_vector);

	nvgpu_falcon_writel(flcn, falcon_falcon_dmactl_r(),
		falcon_falcon_dmactl_require_ctx_f(0));

	nvgpu_falcon_writel(flcn, falcon_falcon_bootvec_r(),
		falcon_falcon_bootvec_vec_f(boot_vector));

	nvgpu_falcon_writel(flcn, falcon_falcon_cpuctl_r(),
		falcon_falcon_cpuctl_startcpu_f(1));
}

u32 gk20a_falcon_mailbox_read(struct nvgpu_falcon *flcn,
		u32 mailbox_index)
{
	return nvgpu_falcon_readl(flcn, (mailbox_index != 0U) ?
					falcon_falcon_mailbox1_r() :
					falcon_falcon_mailbox0_r());
}

void gk20a_falcon_mailbox_write(struct nvgpu_falcon *flcn,
		u32 mailbox_index, u32 data)
{
	nvgpu_falcon_writel(flcn, (mailbox_index != 0U) ?
					falcon_falcon_mailbox1_r() :
					falcon_falcon_mailbox0_r(), data);
}

void gk20a_falcon_set_irq(struct nvgpu_falcon *flcn, bool enable,
	u32 intr_mask, u32 intr_dest)
{
	if (enable) {
		nvgpu_falcon_writel(flcn, falcon_falcon_irqmset_r(), intr_mask);
		nvgpu_falcon_writel(flcn, falcon_falcon_irqdest_r(), intr_dest);
	} else {
		nvgpu_falcon_writel(flcn, falcon_falcon_irqmclr_r(),
				    0xffffffffU);
	}
}

#if defined(CONFIG_NVGPU_FALCON_DEBUG) || defined(CONFIG_NVGPU_FALCON_NON_FUSA)
int gk20a_falcon_copy_from_dmem(struct nvgpu_falcon *flcn,
		u32 src, u8 *dst, u32 size, u8 port)
{
	struct gk20a *g = flcn->g;
	u32 base_addr = flcn->flcn_base;
	u32 i, words, bytes;
	u32 data, addr_mask;
	u32 *dst_u32 = (u32 *)dst;

	nvgpu_log_fn(g, " src dmem offset - %x, size - %x", src, size);

	words = size >> 2U;
	bytes = size & 0x3U;

	addr_mask = falcon_falcon_dmemc_offs_m() |
				g->ops.falcon.dmemc_blk_mask();

	src &= addr_mask;

	nvgpu_writel(g, base_addr + falcon_falcon_dmemc_r(port),
		src | falcon_falcon_dmemc_aincr_f(1));

	for (i = 0; i < words; i++) {
		dst_u32[i] = nvgpu_readl(g,
			base_addr + falcon_falcon_dmemd_r(port));
	}

	if (bytes > 0U) {
		data = nvgpu_readl(g, base_addr + falcon_falcon_dmemd_r(port));
		nvgpu_memcpy(&dst[words << 2U], (u8 *)&data, bytes);
	}

	return 0;
}
#endif

#ifdef CONFIG_NVGPU_FALCON_DEBUG
static void gk20a_falcon_dump_imblk(struct nvgpu_falcon *flcn)
{
	struct gk20a *g = NULL;
	u32 i = 0U, j = 0U;
	u32 data[8] = {0U};
	u32 block_count = 0U;

	g = flcn->g;

	block_count = falcon_falcon_hwcfg_imem_size_v(
				nvgpu_falcon_readl(flcn,
						   falcon_falcon_hwcfg_r()));

	/* block_count must be multiple of 8 */
	block_count &= ~0x7U;
	nvgpu_err(g, "FALCON IMEM BLK MAPPING (PA->VA) (%d TOTAL):",
		block_count);

	for (i = 0U; i < block_count; i += 8U) {
		for (j = 0U; j < 8U; j++) {
			nvgpu_falcon_writel(flcn, falcon_falcon_imctl_debug_r(),
			falcon_falcon_imctl_debug_cmd_f(0x2) |
			falcon_falcon_imctl_debug_addr_blk_f(i + j));

			data[j] = nvgpu_falcon_readl(flcn,
						     falcon_falcon_imstat_r());
		}

		nvgpu_err(g, " %#04x: %#010x %#010x %#010x %#010x",
				i, data[0], data[1], data[2], data[3]);
		nvgpu_err(g, " %#04x: %#010x %#010x %#010x %#010x",
				i + 4U, data[4], data[5], data[6], data[7]);
	}
}

static void gk20a_falcon_dump_pc_trace(struct nvgpu_falcon *flcn)
{
	struct gk20a *g = NULL;
	u32 trace_pc_count = 0U;
	u32 pc = 0U;
	u32 i = 0U;

	g = flcn->g;

	if ((nvgpu_falcon_readl(flcn, falcon_falcon_sctl_r()) & 0x02U) != 0U) {
		nvgpu_err(g, " falcon is in HS mode, PC TRACE dump not supported");
		return;
	}

	trace_pc_count = falcon_falcon_traceidx_maxidx_v(
				nvgpu_falcon_readl(flcn,
						   falcon_falcon_traceidx_r()));
	nvgpu_err(g,
		"PC TRACE (TOTAL %d ENTRIES. entry 0 is the most recent branch):",
		trace_pc_count);

	for (i = 0; i < trace_pc_count; i++) {
		nvgpu_falcon_writel(flcn, falcon_falcon_traceidx_r(),
				    falcon_falcon_traceidx_idx_f(i));

		pc = falcon_falcon_tracepc_pc_v(
			nvgpu_falcon_readl(flcn, falcon_falcon_tracepc_r()));
		nvgpu_err(g, "FALCON_TRACEPC(%d)  :  %#010x", i, pc);
	}
}

void gk20a_falcon_dump_stats(struct nvgpu_falcon *flcn)
{
	struct gk20a *g = NULL;
	unsigned int i;

	g = flcn->g;

	nvgpu_err(g, "<<< FALCON id-%d DEBUG INFORMATION - START >>>",
			flcn->flcn_id);

	/* imblk dump */
	gk20a_falcon_dump_imblk(flcn);
	/* PC trace dump */
	gk20a_falcon_dump_pc_trace(flcn);

	nvgpu_err(g, "FALCON ICD REGISTERS DUMP");

	for (i = 0U; i < 4U; i++) {
		nvgpu_falcon_writel(flcn,
			falcon_falcon_icd_cmd_r(),
			falcon_falcon_icd_cmd_opc_rreg_f() |
			falcon_falcon_icd_cmd_idx_f(FALCON_REG_PC));
		nvgpu_err(g, "FALCON_REG_PC : 0x%x",
			nvgpu_falcon_readl(flcn, falcon_falcon_icd_rdata_r()));

		nvgpu_falcon_writel(flcn, falcon_falcon_icd_cmd_r(),
			falcon_falcon_icd_cmd_opc_rreg_f() |
			falcon_falcon_icd_cmd_idx_f(FALCON_REG_SP));
		nvgpu_err(g, "FALCON_REG_SP : 0x%x",
			nvgpu_falcon_readl(flcn, falcon_falcon_icd_rdata_r()));
	}

	nvgpu_falcon_writel(flcn, falcon_falcon_icd_cmd_r(),
		falcon_falcon_icd_cmd_opc_rreg_f() |
		falcon_falcon_icd_cmd_idx_f(FALCON_REG_IMB));
	nvgpu_err(g, "FALCON_REG_IMB : 0x%x",
		nvgpu_falcon_readl(flcn, falcon_falcon_icd_rdata_r()));

	nvgpu_falcon_writel(flcn, falcon_falcon_icd_cmd_r(),
		falcon_falcon_icd_cmd_opc_rreg_f() |
		falcon_falcon_icd_cmd_idx_f(FALCON_REG_DMB));
	nvgpu_err(g, "FALCON_REG_DMB : 0x%x",
		nvgpu_falcon_readl(flcn, falcon_falcon_icd_rdata_r()));

	nvgpu_falcon_writel(flcn, falcon_falcon_icd_cmd_r(),
		falcon_falcon_icd_cmd_opc_rreg_f() |
		falcon_falcon_icd_cmd_idx_f(FALCON_REG_CSW));
	nvgpu_err(g, "FALCON_REG_CSW : 0x%x",
		nvgpu_falcon_readl(flcn, falcon_falcon_icd_rdata_r()));

	nvgpu_falcon_writel(flcn, falcon_falcon_icd_cmd_r(),
		falcon_falcon_icd_cmd_opc_rreg_f() |
		falcon_falcon_icd_cmd_idx_f(FALCON_REG_CTX));
	nvgpu_err(g, "FALCON_REG_CTX : 0x%x",
		nvgpu_falcon_readl(flcn, falcon_falcon_icd_rdata_r()));

	nvgpu_falcon_writel(flcn, falcon_falcon_icd_cmd_r(),
		falcon_falcon_icd_cmd_opc_rreg_f() |
		falcon_falcon_icd_cmd_idx_f(FALCON_REG_EXCI));
	nvgpu_err(g, "FALCON_REG_EXCI : 0x%x",
		nvgpu_falcon_readl(flcn, falcon_falcon_icd_rdata_r()));

	for (i = 0U; i < 6U; i++) {
		nvgpu_falcon_writel(flcn, falcon_falcon_icd_cmd_r(),
			falcon_falcon_icd_cmd_opc_rreg_f() |
			falcon_falcon_icd_cmd_idx_f(
			falcon_falcon_icd_cmd_opc_rstat_f()));
		nvgpu_err(g, "FALCON_REG_RSTAT[%d] : 0x%x", i,
			nvgpu_falcon_readl(flcn, falcon_falcon_icd_rdata_r()));
	}

	nvgpu_err(g, " FALCON REGISTERS DUMP");
	nvgpu_err(g, "falcon_falcon_os_r : %d",
		nvgpu_falcon_readl(flcn, falcon_falcon_os_r()));
	nvgpu_err(g, "falcon_falcon_cpuctl_r : 0x%x",
		nvgpu_falcon_readl(flcn, falcon_falcon_cpuctl_r()));
	nvgpu_err(g, "falcon_falcon_idlestate_r : 0x%x",
		nvgpu_falcon_readl(flcn, falcon_falcon_idlestate_r()));
	nvgpu_err(g, "falcon_falcon_mailbox0_r : 0x%x",
		nvgpu_falcon_readl(flcn, falcon_falcon_mailbox0_r()));
	nvgpu_err(g, "falcon_falcon_mailbox1_r : 0x%x",
		nvgpu_falcon_readl(flcn, falcon_falcon_mailbox1_r()));
	nvgpu_err(g, "falcon_falcon_irqstat_r : 0x%x",
		nvgpu_falcon_readl(flcn, falcon_falcon_irqstat_r()));
	nvgpu_err(g, "falcon_falcon_irqmode_r : 0x%x",
		nvgpu_falcon_readl(flcn, falcon_falcon_irqmode_r()));
	nvgpu_err(g, "falcon_falcon_irqmask_r : 0x%x",
		nvgpu_falcon_readl(flcn, falcon_falcon_irqmask_r()));
	nvgpu_err(g, "falcon_falcon_irqdest_r : 0x%x",
		nvgpu_falcon_readl(flcn, falcon_falcon_irqdest_r()));
	nvgpu_err(g, "falcon_falcon_debug1_r : 0x%x",
		nvgpu_falcon_readl(flcn, falcon_falcon_debug1_r()));
	nvgpu_err(g, "falcon_falcon_debuginfo_r : 0x%x",
		nvgpu_falcon_readl(flcn, falcon_falcon_debuginfo_r()));
	nvgpu_err(g, "falcon_falcon_bootvec_r : 0x%x",
		nvgpu_falcon_readl(flcn, falcon_falcon_bootvec_r()));
	nvgpu_err(g, "falcon_falcon_hwcfg_r : 0x%x",
		nvgpu_falcon_readl(flcn, falcon_falcon_hwcfg_r()));
	nvgpu_err(g, "falcon_falcon_engctl_r : 0x%x",
		nvgpu_falcon_readl(flcn, falcon_falcon_engctl_r()));
	nvgpu_err(g, "falcon_falcon_curctx_r : 0x%x",
		nvgpu_falcon_readl(flcn, falcon_falcon_curctx_r()));
	nvgpu_err(g, "falcon_falcon_nxtctx_r : 0x%x",
		nvgpu_falcon_readl(flcn, falcon_falcon_nxtctx_r()));
	/*
	 * Common Falcon code accesses each engine's falcon registers
	 * using engine's falcon base address + offset.
	 * So generate offset for falcon_falcon_exterrstat_r()
	 * and falcon_falcon_exterraddr_r() registers by applying
	 * the mask 0xFFF
	 */
	nvgpu_err(g, "falcon_falcon_exterrstat_r : 0x%x",
		nvgpu_falcon_readl(flcn,
			(falcon_falcon_exterrstat_r() & 0x0FFF)));
	nvgpu_err(g, "falcon_falcon_exterraddr_r : 0x%x",
		nvgpu_falcon_readl(flcn,
			(falcon_falcon_exterraddr_r() & 0x0FFF)));
}
#endif
