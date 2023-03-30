/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/log.h>
#include <nvgpu/io.h>

#include "ce_tu104.h"

#include <nvgpu/hw/tu104/hw_ce_tu104.h>

void tu104_ce_set_pce2lce_mapping(struct gk20a *g)
{
	/*
	 * By default GRCE0 and GRCE1 share PCE0.
	 * Do not change PCE0 config until GRCEs are remapped to PCE1/PCE3.
	 */

	/* PCE1 (HSHUB) is assigned to LCE4 */
	nvgpu_writel(g, ce_pce2lce_config_r(1),
		ce_pce2lce_config_pce_assigned_lce_f(4));
	/* GRCE1 shares with LCE4 */
	nvgpu_writel(g, ce_grce_config_r(1),
		ce_grce_config_shared_lce_f(4) |
		ce_grce_config_shared_f(1));

	/* PCE2 (FBHUB) is assigned to LCE2 */
	nvgpu_writel(g, ce_pce2lce_config_r(2),
		ce_pce2lce_config_pce_assigned_lce_f(2));

	/* PCE3 (FBHUB) is assigned to LCE3 */
	nvgpu_writel(g, ce_pce2lce_config_r(3),
		ce_pce2lce_config_pce_assigned_lce_f(3));
	/* GRCE0 shares with LCE3 */
	nvgpu_writel(g, ce_grce_config_r(0),
		ce_grce_config_shared_lce_f(3) |
		ce_grce_config_shared_f(1));

	/* PCE0 (HSHUB) is unconnected */
	nvgpu_writel(g, ce_pce2lce_config_r(0),
		ce_pce2lce_config_pce_assigned_lce_none_f());

}
