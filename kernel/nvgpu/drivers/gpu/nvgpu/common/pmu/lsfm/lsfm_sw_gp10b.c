/*
 * Copyright (c) 2015-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/timers.h>
#include <nvgpu/pmu.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/bug.h>
#include <nvgpu/string.h>
#include <nvgpu/pmu/cmd.h>
#include <nvgpu/pmu/lsfm.h>

#include "lsfm_sw_gm20b.h"
#include "lsfm_sw_gp10b.h"

static int gp10b_pmu_lsfm_bootstrap_falcon(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_lsfm *lsfm,
	u32 falconidmask, u32 flags)
{
	struct pmu_cmd cmd;
	size_t tmp_size;

	nvgpu_log_fn(g, " ");

	lsfm->loaded_falcon_id = 0U;

	nvgpu_pmu_dbg(g, "wprinit status = %x", lsfm->is_wpr_init_done);

	/* send message to load FECS falcon */
	(void) memset(&cmd, 0, sizeof(struct pmu_cmd));
	cmd.hdr.unit_id = PMU_UNIT_ACR;

	tmp_size = PMU_CMD_HDR_SIZE +
		sizeof(struct pmu_acr_cmd_bootstrap_multiple_falcons);
	nvgpu_assert(tmp_size <= (size_t)U8_MAX);
	cmd.hdr.size = (u8)tmp_size;

	cmd.cmd.acr.boot_falcons.cmd_type =
			PMU_ACR_CMD_ID_BOOTSTRAP_MULTIPLE_FALCONS;
	cmd.cmd.acr.boot_falcons.flags = flags;
	cmd.cmd.acr.boot_falcons.falconidmask = falconidmask;
	cmd.cmd.acr.boot_falcons.usevamask = 0;
	cmd.cmd.acr.boot_falcons.wprvirtualbase.lo = 0x0U;
	cmd.cmd.acr.boot_falcons.wprvirtualbase.hi = 0x0U;

	nvgpu_pmu_dbg(g, "PMU_ACR_CMD_ID_BOOTSTRAP_MULTIPLE_FALCONS:%x",
		falconidmask);

	return nvgpu_pmu_cmd_post(g, &cmd, NULL, PMU_COMMAND_QUEUE_HPQ,
		gm20b_pmu_lsfm_handle_bootstrap_falcon_msg, pmu);
}

static int gp10b_pmu_lsfm_bootstrap_ls_falcon(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_lsfm *lsfm, u32 falcon_id_mask)
{
	u32 flags = PMU_ACR_CMD_BOOTSTRAP_FALCON_FLAGS_RESET_YES;
	int err = 0;

	/* GP10B PMU supports loading FECS and GPCCS only */
	if (falcon_id_mask == 0U) {
		err = -EINVAL;
		goto done;
	}

	if ((falcon_id_mask &
		~(BIT32(FALCON_ID_FECS) | BIT32(FALCON_ID_GPCCS))) != 0U) {
		err = -EINVAL;
		goto done;
	}

	lsfm->loaded_falcon_id = 0U;

	/* bootstrap falcon(s) */
	err = gp10b_pmu_lsfm_bootstrap_falcon(g, pmu, lsfm,
			falcon_id_mask, flags);
	if (err != 0) {
		err = -EINVAL;
		goto done;
	}

	nvgpu_assert(falcon_id_mask <= U8_MAX);
	pmu_wait_message_cond(g->pmu, nvgpu_get_poll_timeout(g),
		&lsfm->loaded_falcon_id, (u8)falcon_id_mask);
	if (lsfm->loaded_falcon_id != falcon_id_mask) {
		err = -ETIMEDOUT;
	}

done:
	return err;
}

void nvgpu_gp10b_lsfm_sw_init(struct gk20a *g, struct nvgpu_pmu_lsfm *lsfm)
{
	nvgpu_log_fn(g, " ");

	lsfm->is_wpr_init_done = false;
	lsfm->loaded_falcon_id = 0U;

	lsfm->init_wpr_region = gm20b_pmu_lsfm_init_acr_wpr_region;
	lsfm->bootstrap_ls_falcon = gp10b_pmu_lsfm_bootstrap_ls_falcon;
	lsfm->ls_pmu_cmdline_args_copy = gm20b_pmu_lsfm_pmu_cmd_line_args_copy;
}
