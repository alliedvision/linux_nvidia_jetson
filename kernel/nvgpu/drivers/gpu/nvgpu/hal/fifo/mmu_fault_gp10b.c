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

#include <nvgpu/gk20a.h>
#include <nvgpu/timers.h>
#include <nvgpu/log.h>
#include <nvgpu/io.h>
#include <nvgpu/fifo.h>
#include <nvgpu/engines.h>
#include <nvgpu/string.h>

#include <hal/fifo/mmu_fault_gp10b.h>

#include <nvgpu/hw/gp10b/hw_fifo_gp10b.h>

/* fault info/descriptions */
static const char * const gp10b_fault_type_descs[] = {
	 "pde", /*fifo_intr_mmu_fault_info_type_pde_v() == 0 */
	 "pde size",
	 "pte",
	 "va limit viol",
	 "unbound inst",
	 "priv viol",
	 "ro viol",
	 "wo viol",
	 "pitch mask",
	 "work creation",
	 "bad aperture",
	 "compression failure",
	 "bad kind",
	 "region viol",
	 "dual ptes",
	 "poisoned",
	 "atomic violation",
};

static const char * const gp10b_hub_client_descs[] = {
	"vip", "ce0", "ce1", "dniso", "fe", "fecs", "host", "host cpu",
	"host cpu nb", "iso", "mmu", "mspdec", "msppp", "msvld",
	"niso", "p2p", "pd", "perf", "pmu", "raster twod", "scc",
	"scc nb", "sec", "ssync", "gr copy", "xv", "mmu nb",
	"msenc", "d falcon", "sked", "a falcon", "n/a",
	"hsce0", "hsce1", "hsce2", "hsce3", "hsce4", "hsce5",
	"hsce6", "hsce7", "hsce8", "hsce9", "hshub",
	"ptp x0", "ptp x1", "ptp x2", "ptp x3", "ptp x4",
	"ptp x5", "ptp x6", "ptp x7", "vpr scrubber0", "vpr scrubber1",
};

/* fill in mmu fault desc */
void gp10b_fifo_get_mmu_fault_desc(struct mmu_fault_info *mmufault)
{
	if (mmufault->fault_type >= ARRAY_SIZE(gp10b_fault_type_descs)) {
		WARN_ON(mmufault->fault_type >=
				ARRAY_SIZE(gp10b_fault_type_descs));
	} else {
		mmufault->fault_type_desc =
			 gp10b_fault_type_descs[mmufault->fault_type];
	}
}

/* fill in mmu fault client description */
void gp10b_fifo_get_mmu_fault_client_desc(struct mmu_fault_info *mmufault)
{
	if (mmufault->client_id >= ARRAY_SIZE(gp10b_hub_client_descs)) {
		WARN_ON(mmufault->client_id >=
				ARRAY_SIZE(gp10b_hub_client_descs));
	} else {
		mmufault->client_id_desc =
			 gp10b_hub_client_descs[mmufault->client_id];
	}
}

void gp10b_fifo_get_mmu_fault_info(struct gk20a *g, u32 mmu_fault_id,
			struct mmu_fault_info *mmufault)
{
	u32 fault_info;
	u32 addr_lo, addr_hi;

	nvgpu_log_fn(g, "mmu_fault_id %d", mmu_fault_id);

	(void) memset(mmufault, 0, sizeof(*mmufault));

	fault_info = nvgpu_readl(g,
		fifo_intr_mmu_fault_info_r(mmu_fault_id));
	mmufault->fault_type =
		fifo_intr_mmu_fault_info_type_v(fault_info);
	mmufault->access_type =
		fifo_intr_mmu_fault_info_access_type_v(fault_info);
	mmufault->client_type =
		fifo_intr_mmu_fault_info_client_type_v(fault_info);
	mmufault->client_id =
		fifo_intr_mmu_fault_info_client_v(fault_info);

	addr_lo = nvgpu_readl(g, fifo_intr_mmu_fault_lo_r(mmu_fault_id));
	addr_hi = nvgpu_readl(g, fifo_intr_mmu_fault_hi_r(mmu_fault_id));
	mmufault->fault_addr = hi32_lo32_to_u64(addr_hi, addr_lo);
	/* note:ignoring aperture */
	mmufault->inst_ptr = fifo_intr_mmu_fault_inst_ptr_v(
		 nvgpu_readl(g, fifo_intr_mmu_fault_inst_r(mmu_fault_id)));
	/* note: inst_ptr is a 40b phys addr.  */
	mmufault->inst_ptr <<= fifo_intr_mmu_fault_inst_ptr_align_shift_v();
}
