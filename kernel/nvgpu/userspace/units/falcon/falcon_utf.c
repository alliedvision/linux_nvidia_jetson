/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <unit/io.h>

#include <nvgpu/falcon.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/kmem.h>
#include <nvgpu/hw/gm20b/hw_falcon_gm20b.h>

#include "falcon_utf.h"

#include <nvgpu/posix/posix-fault-injection.h>

struct nvgpu_posix_fault_inj *nvgpu_utf_falcon_memcpy_get_fault_injection(void)
{
	struct nvgpu_posix_fault_inj_container *c =
		nvgpu_posix_fault_injection_get_container();

	return &c->falcon_memcpy_fi;
}

void nvgpu_utf_falcon_writel_access_reg_fn(struct gk20a *g,
					   struct utf_falcon *flcn,
					   struct nvgpu_reg_access *access)
{
	u32 addr_mask = falcon_falcon_dmemc_offs_m() |
			falcon_falcon_dmemc_blk_m();
	u32 flcn_base;
	u32 ctrl_r;
	u32 offset;

	flcn_base = flcn->flcn->flcn_base;

	if (access->addr == (flcn_base + falcon_falcon_imemd_r(0))) {
		ctrl_r = nvgpu_posix_io_readl_reg_space(g,
				flcn_base + falcon_falcon_imemc_r(0));

		if (ctrl_r & falcon_falcon_imemc_aincw_f(1)) {
			offset = ctrl_r & addr_mask;

			*((u32 *) ((u8 *)flcn->imem + offset)) = access->value;
			offset += 4U;

			ctrl_r &= ~(addr_mask);
			ctrl_r |= offset;
			nvgpu_posix_io_writel_reg_space(g,
				flcn_base + falcon_falcon_imemc_r(0), ctrl_r);
		}
	} else if (access->addr == (flcn_base + falcon_falcon_dmemd_r(0))) {
		ctrl_r = nvgpu_posix_io_readl_reg_space(g,
				flcn_base + falcon_falcon_dmemc_r(0));

		if (ctrl_r & falcon_falcon_dmemc_aincw_f(1)) {
			offset = ctrl_r & addr_mask;

			*((u32 *) ((u8 *)flcn->dmem + offset)) = access->value;
			offset += 4U;

			ctrl_r &= ~(addr_mask);
			ctrl_r |= offset;
			nvgpu_posix_io_writel_reg_space(g,
				flcn_base + falcon_falcon_dmemc_r(0), ctrl_r);
		}
	} else if (access->addr == (flcn_base + falcon_falcon_cpuctl_r())) {

		if (access->value == falcon_falcon_cpuctl_halt_intr_m()) {
			access->value = nvgpu_posix_io_readl_reg_space(g,
						access->addr);
			access->value |= falcon_falcon_cpuctl_halt_intr_m();
			nvgpu_posix_io_writel_reg_space(g, access->addr,
							access->value);
		} else if (access->value == falcon_falcon_cpuctl_startcpu_f(1)) {
			access->value = nvgpu_posix_io_readl_reg_space(g,
							access->addr);
			access->value |= falcon_falcon_cpuctl_startcpu_f(1);
			nvgpu_posix_io_writel_reg_space(g, access->addr,
							access->value);
			/* set falcon mailbox0 to value 0 */
			nvgpu_posix_io_writel_reg_space(g, flcn_base +
					falcon_falcon_mailbox0_r(), 0);
		}
	}
	nvgpu_posix_io_writel_reg_space(g, access->addr, access->value);
}

void nvgpu_utf_falcon_readl_access_reg_fn(struct gk20a *g,
					  struct utf_falcon *flcn,
					  struct nvgpu_reg_access *access)
{
	u32 addr_mask = falcon_falcon_dmemc_offs_m() |
			falcon_falcon_dmemc_blk_m();
	u32 flcn_base;
	u32 ctrl_r;
	u32 offset;

	flcn_base = flcn->flcn->flcn_base;

	if (access->addr == (flcn_base + falcon_falcon_imemd_r(0))) {
		ctrl_r = nvgpu_posix_io_readl_reg_space(g,
				flcn_base + falcon_falcon_imemc_r(0));

		if (ctrl_r & falcon_falcon_dmemc_aincr_f(1)) {
			offset = ctrl_r & addr_mask;

			access->value = *((u32 *) ((u8 *)flcn->imem + offset));
			offset += 4U;

			ctrl_r &= ~(addr_mask);
			ctrl_r |= offset;
			nvgpu_posix_io_writel_reg_space(g,
				flcn_base + falcon_falcon_imemc_r(0), ctrl_r);
		}
	} else if (access->addr == (flcn_base + falcon_falcon_dmemd_r(0))) {
		ctrl_r = nvgpu_posix_io_readl_reg_space(g,
				flcn_base + falcon_falcon_dmemc_r(0));

		if (ctrl_r & falcon_falcon_dmemc_aincr_f(1)) {
			offset = ctrl_r & addr_mask;

			access->value = *((u32 *) ((u8 *)flcn->dmem + offset));
			offset += 4U;

			ctrl_r &= ~(addr_mask);
			ctrl_r |= offset;
			nvgpu_posix_io_writel_reg_space(g,
				flcn_base + falcon_falcon_dmemc_r(0), ctrl_r);
		}
	} else if (access->addr == (flcn_base + falcon_falcon_dmemc_r(0))) {
		ctrl_r = nvgpu_posix_io_readl_reg_space(g,
				flcn_base + falcon_falcon_dmemc_r(0));

		if (nvgpu_posix_fault_injection_handle_call(
			nvgpu_utf_falcon_memcpy_get_fault_injection())) {
			access->value = 0;
			return;
		}

		access->value = ctrl_r & addr_mask;
	} else {
		access->value = nvgpu_posix_io_readl_reg_space(g, access->addr);
	}
}

struct utf_falcon *nvgpu_utf_falcon_init(struct unit_module *m,
					 struct gk20a *g, u32 flcn_id)
{
	struct utf_falcon *utf_flcn;
	struct nvgpu_falcon *flcn;
	u32 flcn_size;
	u32 flcn_base;
	u32 hwcfg_r, hwcfg1_r, ports;

	if (nvgpu_falcon_sw_init(g, flcn_id) != 0) {
		unit_err(m, "nvgpu Falcon init failed!\n");
		return NULL;
	}

	flcn = nvgpu_falcon_get_instance(g, flcn_id);

	utf_flcn = (struct utf_falcon *) malloc(sizeof(struct utf_falcon));
	if (!utf_flcn) {
		return NULL;
	}

	utf_flcn->flcn = flcn;

	flcn_base = flcn->flcn_base;
	if (nvgpu_posix_io_add_reg_space(g,
					 flcn_base,
					 UTF_FALCON_MAX_REG_OFFSET) != 0) {
		unit_err(m, "Falcon add reg space failed!\n");
		goto out;
	}

	/*
	 * Initialize IMEM & DMEM size that will be needed by NvGPU for
	 * bounds check.
	 */
	hwcfg_r = flcn_base + falcon_falcon_hwcfg_r();
	flcn_size = UTF_FALCON_IMEM_DMEM_SIZE / FALCON_BLOCK_SIZE;
	flcn_size = (flcn_size << 9) | flcn_size;
	nvgpu_posix_io_writel_reg_space(g, hwcfg_r, flcn_size);

	/* set imem and dmem ports count. */
	hwcfg1_r = flcn_base + falcon_falcon_hwcfg1_r();
	ports = (1 << 8) | (1 << 12);
	nvgpu_posix_io_writel_reg_space(g, hwcfg1_r, ports);

	utf_flcn->imem = (u32 *) nvgpu_kzalloc(g, UTF_FALCON_IMEM_DMEM_SIZE);
	if (utf_flcn->imem == NULL) {
		unit_err(m, "Falcon imem alloc failed!\n");
		goto out_reg_space;
	}

	utf_flcn->dmem = (u32 *) nvgpu_kzalloc(g, UTF_FALCON_IMEM_DMEM_SIZE);
	if (utf_flcn->dmem == NULL) {
		unit_err(m, "Falcon dmem alloc failed!\n");
		goto free_imem;
	}

	return utf_flcn;

free_imem:
	nvgpu_kfree(g, utf_flcn->imem);
out_reg_space:
	nvgpu_posix_io_delete_reg_space(g, flcn_base);
out:
	nvgpu_falcon_sw_free(g, flcn_id);
	free(utf_flcn);

	return NULL;
}

void nvgpu_utf_falcon_free(struct gk20a *g, struct utf_falcon *utf_flcn)
{
	if (utf_flcn == NULL || utf_flcn->flcn == NULL)
		return;

	nvgpu_kfree(g, utf_flcn->dmem);
	nvgpu_kfree(g, utf_flcn->imem);
	nvgpu_posix_io_delete_reg_space(g, utf_flcn->flcn->flcn_base);
	nvgpu_falcon_sw_free(g, utf_flcn->flcn->flcn_id);
	free(utf_flcn);
}

void nvgpu_utf_falcon_set_dmactl(struct gk20a *g, struct utf_falcon *utf_flcn,
				 u32 reg_data)
{
	u32 flcn_base;

	flcn_base = utf_flcn->flcn->flcn_base;

	nvgpu_posix_io_writel_reg_space(g,
		flcn_base + falcon_falcon_dmactl_r(), reg_data);
}
