/*
 * GV11B TPC
 *
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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
#include "tpc_gv11b.h"

#ifdef CONFIG_NVGPU_STATIC_POWERGATE
/*
 * validate the requested tpc pg mask
 * with respect to the current hw value.
 */
int gv11b_tpc_pg(struct gk20a *g, bool *can_tpc_pg)
{
	int err = 0;
	u32 fuse_status = 0x0;

	if (g->ops.fuse.fuse_status_opt_tpc_gpc != NULL) {
		fuse_status = g->ops.fuse.fuse_status_opt_tpc_gpc(g, 0);
	}

	if (fuse_status == 0x0) {
		/*
		 * fuse_status = 0x0 means all TPCs are active
		 * thus, tpc_pg_mask passed by user or DT
		 * can be applied to powergate the TPC(s).
		 */
		*can_tpc_pg = true;
	} else {
		/* if hardware has already floorswept any TPC
		 * (fuse_status !=  0x0) and if TPC PG mask
		 * sent from userspace/DT node is 0x0 (it is trying to
		 * set all TPCs active), GPU will be powered on
		 * with the default HW fuse_status setting. It cannot
		 * un-floorsweep any TPC.
		 * thus, set g->tpc_pg_mask to fuse_status value
		 * and boot with default HW fuse settings.
		 */
		if (g->tpc_pg_mask[PG_GPC0] == 0x0U) {
			*can_tpc_pg = true;
			g->tpc_pg_mask[PG_GPC0] = fuse_status;

		} else if (fuse_status == g->tpc_pg_mask[PG_GPC0]) {
			*can_tpc_pg = true;

		} else if ((fuse_status & g->tpc_pg_mask[PG_GPC0]) ==
					fuse_status) {
			/*
			 * if HW has already floorswept any TPC
			 * check if the tpc_pg_mask sent by user
			 * is floorsweeping only additional TPCs
			 */
			*can_tpc_pg = true;
		} else {
			/* If userspace sends a TPC PG mask such that
			 * it tries to un-floorsweep any TPC which is
			 * already powergated from hardware, then
			 * such mask is invalid.
			 * In this case set tpc pg mask to 0x0
			 * Return -EINVAL here and halt GPU poweron.
			 */
			nvgpu_err(g, "Invalid TPC_PG mask: 0x%x",
						g->tpc_pg_mask[PG_GPC0]);
			*can_tpc_pg = false;
			g->tpc_pg_mask[PG_GPC0] = 0x0;
			err = -EINVAL;
		}
	}

	return err;
}

/* To-do: check to rename this function */
void gv11b_gr_pg_tpc(struct gk20a *g)
{
	u32 tpc_pg_status = g->ops.fuse.fuse_status_opt_tpc_gpc(g, PG_GPC0);

	/*
	 * if the fuse status for tpc is same as tpc_pg_mask
	 * passed, then do nothing and return
	 */
	if (tpc_pg_status == g->tpc_pg_mask[PG_GPC0]) {
		return;
	}

	/*
	 * write to fuse_ctrl register to update the fuse settings
	 * as per the tpc_pg_mask
	 */
	g->ops.fuse.fuse_ctrl_opt_tpc_gpc(g, PG_GPC0,
					g->tpc_pg_mask[PG_GPC0]);

	/*
	 * To confirm that the write was successful
	 * read the fuse_status register.
	 * The write done is previous step may take some time to
	 * get update in fuse_status register
	 */
	do {
		tpc_pg_status = g->ops.fuse.fuse_status_opt_tpc_gpc(g, PG_GPC0);
	} while (tpc_pg_status != g->tpc_pg_mask[PG_GPC0]);

	return;
}
#endif
