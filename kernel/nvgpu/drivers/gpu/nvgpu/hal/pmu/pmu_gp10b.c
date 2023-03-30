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

#include <nvgpu/pmu.h>
#include <nvgpu/log.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/bug.h>
#include <nvgpu/pmu/cmd.h>

#include "pmu_gk20a.h"
#include "pmu_gp10b.h"

#include <nvgpu/hw/gp10b/hw_pwr_gp10b.h>

/* PROD settings for ELPG sequencing registers*/
static struct pg_init_sequence_list _pginitseq_gp10b[] = {
		{0x0010ab10U, 0x0000868BU} ,
		{0x0010e118U, 0x8590848FU} ,
		{0x0010e000U, 0x0U} ,
		{0x0010e06cU, 0x000000A3U} ,
		{0x0010e06cU, 0x000000A0U} ,
		{0x0010e06cU, 0x00000095U} ,
		{0x0010e06cU, 0x000000A6U} ,
		{0x0010e06cU, 0x0000008CU} ,
		{0x0010e06cU, 0x00000080U} ,
		{0x0010e06cU, 0x00000081U} ,
		{0x0010e06cU, 0x00000087U} ,
		{0x0010e06cU, 0x00000088U} ,
		{0x0010e06cU, 0x0000008DU} ,
		{0x0010e06cU, 0x00000082U} ,
		{0x0010e06cU, 0x00000083U} ,
		{0x0010e06cU, 0x00000089U} ,
		{0x0010e06cU, 0x0000008AU} ,
		{0x0010e06cU, 0x000000A2U} ,
		{0x0010e06cU, 0x00000097U} ,
		{0x0010e06cU, 0x00000092U} ,
		{0x0010e06cU, 0x00000099U} ,
		{0x0010e06cU, 0x0000009BU} ,
		{0x0010e06cU, 0x0000009DU} ,
		{0x0010e06cU, 0x0000009FU} ,
		{0x0010e06cU, 0x000000A1U} ,
		{0x0010e06cU, 0x00000096U} ,
		{0x0010e06cU, 0x00000091U} ,
		{0x0010e06cU, 0x00000098U} ,
		{0x0010e06cU, 0x0000009AU} ,
		{0x0010e06cU, 0x0000009CU} ,
		{0x0010e06cU, 0x0000009EU} ,
		{0x0010ab14U, 0x00000000U} ,
		{0x0010e024U, 0x00000000U} ,
		{0x0010e028U, 0x00000000U} ,
		{0x0010e11cU, 0x00000000U} ,
		{0x0010ab1cU, 0x140B0BFFU} ,
		{0x0010e020U, 0x0E2626FFU} ,
		{0x0010e124U, 0x251010FFU} ,
		{0x0010ab20U, 0x89abcdefU} ,
		{0x0010ab24U, 0x00000000U} ,
		{0x0010e02cU, 0x89abcdefU} ,
		{0x0010e030U, 0x00000000U} ,
		{0x0010e128U, 0x89abcdefU} ,
		{0x0010e12cU, 0x00000000U} ,
		{0x0010ab28U, 0x7FFFFFFFU} ,
		{0x0010ab2cU, 0x70000000U} ,
		{0x0010e034U, 0x7FFFFFFFU} ,
		{0x0010e038U, 0x70000000U} ,
		{0x0010e130U, 0x7FFFFFFFU} ,
		{0x0010e134U, 0x70000000U} ,
		{0x0010ab30U, 0x00000000U} ,
		{0x0010ab34U, 0x00000001U} ,
		{0x00020004U, 0x00000000U} ,
		{0x0010e138U, 0x00000000U} ,
		{0x0010e040U, 0x00000000U} ,
		{0x0010e168U, 0x00000000U} ,
		{0x0010e114U, 0x0000A5A4U} ,
		{0x0010e110U, 0x00000000U} ,
		{0x0010e10cU, 0x8590848FU} ,
		{0x0010e05cU, 0x00000000U} ,
		{0x0010e044U, 0x00000000U} ,
		{0x0010a644U, 0x0000868BU} ,
		{0x0010a648U, 0x00000000U} ,
		{0x0010a64cU, 0x00829493U} ,
		{0x0010a650U, 0x00000000U} ,
		{0x0010e000U, 0x0U} ,
		{0x0010e068U, 0x000000A3U} ,
		{0x0010e068U, 0x000000A0U} ,
		{0x0010e068U, 0x00000095U} ,
		{0x0010e068U, 0x000000A6U} ,
		{0x0010e068U, 0x0000008CU} ,
		{0x0010e068U, 0x00000080U} ,
		{0x0010e068U, 0x00000081U} ,
		{0x0010e068U, 0x00000087U} ,
		{0x0010e068U, 0x00000088U} ,
		{0x0010e068U, 0x0000008DU} ,
		{0x0010e068U, 0x00000082U} ,
		{0x0010e068U, 0x00000083U} ,
		{0x0010e068U, 0x00000089U} ,
		{0x0010e068U, 0x0000008AU} ,
		{0x0010e068U, 0x000000A2U} ,
		{0x0010e068U, 0x00000097U} ,
		{0x0010e068U, 0x00000092U} ,
		{0x0010e068U, 0x00000099U} ,
		{0x0010e068U, 0x0000009BU} ,
		{0x0010e068U, 0x0000009DU} ,
		{0x0010e068U, 0x0000009FU} ,
		{0x0010e068U, 0x000000A1U} ,
		{0x0010e068U, 0x00000096U} ,
		{0x0010e068U, 0x00000091U} ,
		{0x0010e068U, 0x00000098U} ,
		{0x0010e068U, 0x0000009AU} ,
		{0x0010e068U, 0x0000009CU} ,
		{0x0010e068U, 0x0000009EU} ,
		{0x0010e000U, 0x0U} ,
		{0x0010e004U, 0x0000008EU},
};

void gp10b_pmu_setup_elpg(struct gk20a *g)
{
	size_t reg_writes;
	size_t index;

	nvgpu_log_fn(g, " ");

	if (g->can_elpg && g->elpg_enabled) {
		reg_writes = ARRAY_SIZE(_pginitseq_gp10b);
		/* Initialize registers with production values*/
		for (index = 0; index < reg_writes; index++) {
			gk20a_writel(g, _pginitseq_gp10b[index].regaddr,
				_pginitseq_gp10b[index].writeval);
		}
	}

	nvgpu_log_fn(g, "done");
}

void gp10b_write_dmatrfbase(struct gk20a *g, u32 addr)
{
	gk20a_writel(g, pwr_falcon_dmatrfbase_r(),
				addr);
	gk20a_writel(g, pwr_falcon_dmatrfbase1_r(),
				0x0U);
}

bool gp10b_is_pmu_supported(struct gk20a *g)
{
	(void)g;
	return true;
}

u32 gp10b_pmu_queue_head_r(u32 i)
{
	return pwr_pmu_queue_head_r(i);
}

u32 gp10b_pmu_queue_head__size_1_v(void)
{
	return pwr_pmu_queue_head__size_1_v();
}

u32 gp10b_pmu_queue_tail_r(u32 i)
{
	return pwr_pmu_queue_tail_r(i);
}

u32 gp10b_pmu_queue_tail__size_1_v(void)
{
	return pwr_pmu_queue_tail__size_1_v();
}

u32 gp10b_pmu_mutex__size_1_v(void)
{
	return pwr_pmu_mutex__size_1_v();
}
