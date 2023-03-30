/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>

#include <nvgpu/hw/tu104/hw_pbdma_tu104.h>

#include "pbdma_gm20b.h"
#include "pbdma_tu104.h"

void tu104_pbdma_reset_header(struct gk20a *g, u32 pbdma_id)
{
	gm20b_pbdma_reset_header(g, pbdma_id);
	nvgpu_writel(g, pbdma_data0_r(pbdma_id), 0);
}

u32 tu104_pbdma_read_data(struct gk20a *g, u32 pbdma_id)
{
	u32 pb_inst;
	u32 pb_header, pb_header_type;
	u32 pb_count;

	/*
	 * In order to determine the location of the PB entry that cause the
	 * interrupt, NV_PPBDMA_PB_HEADER and NV_PPBDMA_PB_COUNT need to be
	 * checked. If the TYPE field of the NV_PPBDMA_PB_HEADER is IMMD or the
	 * VALUE field of the NV_PPBDMA_PB_COUNT is zero, then the raw PB
	 * instruction stored in NV_PPBDMA_PB_DATA0 is the one that triggered
	 * the interrupt. Otherwise, the raw PB instruction that triggered the
	 * interrupt is stored in NV_PPBDMA_HDR_SHADOW and NV_PPBDMA_PB_HEADER
	 * stores the decoded version.
	 */

	pb_header = nvgpu_readl(g, pbdma_pb_header_r(pbdma_id));
	pb_count = nvgpu_readl(g, pbdma_pb_count_r(pbdma_id));

	pb_header_type = pb_header & pbdma_pb_header_type_m();
	if ((pbdma_pb_count_value_v(pb_count) == pbdma_pb_count_value_zero_f())
	    || (pb_header_type == pbdma_pb_header_type_immd_f())) {
		pb_inst = nvgpu_readl(g, pbdma_data0_r(pbdma_id));
	} else {
		pb_inst = nvgpu_readl(g, pbdma_hdr_shadow_r(pbdma_id));
	}

	return pb_inst;
}
