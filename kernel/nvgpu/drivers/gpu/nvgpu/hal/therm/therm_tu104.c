/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/pmu/therm.h>

#include "therm_tu104.h"

#include <nvgpu/hw/tu104/hw_therm_tu104.h>

#include <nvgpu/utils.h>

void tu104_get_internal_sensor_limits(s32 *max_24_8, s32 *min_24_8)
{
	*max_24_8 = (0x87 << 8);
	*min_24_8 = (s32)(((u32)-216) << 8);
}

void tu104_get_internal_sensor_curr_temp(struct gk20a *g, u32 *temp_f24_8)
{
	u32 read_val;

	read_val = nvgpu_readl(g, therm_i2cs_sensor_00_r());

	/* Convert from celsius to f24_8 format*/
	*temp_f24_8 = (read_val << 8);
}
