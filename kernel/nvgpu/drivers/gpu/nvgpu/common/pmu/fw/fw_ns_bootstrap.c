/*
 * Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/pmu.h>
#include <nvgpu/dma.h>
#include <nvgpu/firmware.h>
#include <nvgpu/pmu/fw.h>
#include <nvgpu/pmu/clk/clk.h>
#include <nvgpu/string.h>
#include <nvgpu/falcon.h>

static void pmu_free_ns_ucode_blob(struct gk20a *g)
{
	struct nvgpu_pmu *pmu = g->pmu;
	struct mm_gk20a *mm = &g->mm;
	struct vm_gk20a *vm = mm->pmu.vm;
	struct pmu_rtos_fw *rtos_fw = pmu->fw;

	nvgpu_log_fn(g, " ");

	if (nvgpu_mem_is_valid(&rtos_fw->ucode)) {
		nvgpu_dma_unmap_free(vm, &rtos_fw->ucode);
	}
}

int nvgpu_pmu_ns_fw_bootstrap(struct gk20a *g, struct nvgpu_pmu *pmu)
{
	int err;
	u32 args_offset = 0;

	/* prepare blob for non-secure PMU boot */
	err = pmu->fw->ops.prepare_ns_ucode_blob(g);
	if (err != 0) {
		nvgpu_err(g, "non secure ucode blop consrtuct failed");
		return err;
	}

	/* Do non-secure PMU boot */
	err = nvgpu_falcon_reset(pmu->flcn);
	if (err != 0) {
		nvgpu_err(g, "falcon reset failed");
		/* free the ns ucode blob */
		pmu_free_ns_ucode_blob(g);
		return err;
	}

	nvgpu_pmu_enable_irq(g, true);

	nvgpu_mutex_acquire(&pmu->isr_mutex);
	pmu->isr_enabled = true;
	nvgpu_mutex_release(&pmu->isr_mutex);

	g->ops.pmu.setup_apertures(g);

#if defined(CONFIG_NVGPU_NON_FUSA)
	if (nvgpu_is_enabled(g, NVGPU_PMU_NEXT_CORE_ENABLED)) {
		nvgpu_pmu_next_core_rtos_args_setup(g, pmu);

#ifdef CONFIG_NVGPU_FALCON_DEBUG
		err = nvgpu_falcon_dbg_buf_init(g->pmu->flcn,
				NV_RISCV_DMESG_BUFFER_SIZE,
				g->ops.pmu.pmu_get_queue_head(NV_RISCV_DEBUG_BUFFER_QUEUE),
				g->ops.pmu.pmu_get_queue_tail(NV_RISCV_DEBUG_BUFFER_QUEUE));
		if (err != 0) {
			nvgpu_err(g,
				"Failed to allocate NVRISCV PMU debug buffer status=0x%x)",
				err);
			return err;
		}
#endif

	} else
#endif
	{
		nvgpu_pmu_rtos_cmdline_args_init(g, pmu);
		nvgpu_pmu_fw_get_cmd_line_args_offset(g, &args_offset);

		err = nvgpu_falcon_copy_to_dmem(pmu->flcn, args_offset,
			(u8 *)(pmu->fw->ops.get_cmd_line_args_ptr(pmu)),
			pmu->fw->ops.get_cmd_line_args_size(pmu), 0);
		if (err != 0) {
			nvgpu_err(g, "NS PMU ucode setup failed");
			return err;
		}
	}

	return g->ops.pmu.pmu_ns_bootstrap(g, pmu, args_offset);
}
