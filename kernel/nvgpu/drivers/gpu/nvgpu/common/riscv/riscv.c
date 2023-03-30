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
#include <nvgpu/gk20a.h>
#include <nvgpu/io.h>
#include <nvgpu/falcon.h>
#include <nvgpu/riscv.h>
#include <nvgpu/timers.h>
#include <nvgpu/firmware.h>

static bool is_falcon_valid(struct nvgpu_falcon *flcn)
{
	if (flcn == NULL) {
		return false;
	}

	if (!flcn->is_falcon_supported) {
		nvgpu_err(flcn->g, "Core-id %d not supported", flcn->flcn_id);
		return false;
	}

	return true;
}

u32 nvgpu_riscv_readl(struct nvgpu_falcon *flcn, u32 offset)
{
	return nvgpu_readl(flcn->g,
			   nvgpu_safe_add_u32(flcn->flcn2_base, offset));
}

void nvgpu_riscv_writel(struct nvgpu_falcon *flcn,
				       u32 offset, u32 val)
{
	nvgpu_writel(flcn->g, nvgpu_safe_add_u32(flcn->flcn2_base, offset), val);
}

int nvgpu_riscv_hs_ucode_load_bootstrap(struct nvgpu_falcon *flcn,
						struct nvgpu_firmware *manifest_fw,
						struct nvgpu_firmware *code_fw,
						struct nvgpu_firmware *data_fw,
						u64 ucode_sysmem_desc_addr)
{
	struct gk20a *g;
	u32 dmem_size = 0U;
	int err = 0;

	if (!is_falcon_valid(flcn)) {
		return -EINVAL;
	}

	g = flcn->g;

	/* core reset */
	err = nvgpu_falcon_reset(flcn);
	if (err != 0) {
		nvgpu_err(g, "core reset failed err=%d", err);
		return err;
	}

	/* Copy dmem desc address to mailbox */
	nvgpu_falcon_mailbox_write(flcn, FALCON_MAILBOX_0,
					u64_lo32(ucode_sysmem_desc_addr));
	nvgpu_falcon_mailbox_write(flcn, FALCON_MAILBOX_1,
					u64_hi32(ucode_sysmem_desc_addr));

	g->ops.falcon.set_bcr(flcn);
	err = nvgpu_falcon_get_mem_size(flcn, MEM_DMEM, &dmem_size);
	err = nvgpu_falcon_copy_to_imem(flcn, 0x0, code_fw->data,
					(u32)code_fw->size, 0, true, 0x0);

	if (err != 0) {
		nvgpu_err(g, "RISCV code copy to IMEM failed");
		goto exit;
	}

	err = nvgpu_falcon_copy_to_dmem(flcn, 0x0, data_fw->data,
					(u32)data_fw->size, 0x0);
	if (err != 0) {
		nvgpu_err(g, "RISCV data copy to DMEM failed");
		goto exit;
	}

	err = nvgpu_falcon_copy_to_dmem(flcn, (u32)(dmem_size - manifest_fw->size),
				manifest_fw->data, (u32)manifest_fw->size, 0x0);
	if (err != 0) {
		nvgpu_err(g, "RISCV manifest copy to DMEM failed");
		goto exit;
	}

	g->ops.falcon.bootstrap(flcn, 0x0);
exit:
	return err;
}

void nvgpu_riscv_dump_brom_stats(struct nvgpu_falcon *flcn)
{
	if (!is_falcon_valid(flcn)) {
		return;
	}

	flcn->g->ops.falcon.dump_brom_stats(flcn);
}

