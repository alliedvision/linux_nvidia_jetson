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
#include <nvgpu/utils.h>

#include "ctxsw_prog_ga10b.h"

#include <nvgpu/hw/ga10b/hw_ctxsw_prog_ga10b.h>

#ifdef CONFIG_NVGPU_DEBUGGER
u32 ga10b_ctxsw_prog_hw_get_gpccs_header_size(void)
{
	return ctxsw_prog_gpccs_header_stride_v();
}

bool ga10b_ctxsw_prog_check_main_image_header_magic(u32 *context)
{
	u32 magic_1, magic_2, magic_3, magic_4, magic_5;

	magic_1 = *(context + (ctxsw_prog_main_image_magic_value_1_o() >> 2));
	magic_2 = *(context + (ctxsw_prog_main_image_magic_value_2_o() >> 2));
	magic_3 = *(context + (ctxsw_prog_main_image_magic_value_3_o() >> 2));
	magic_4 = *(context + (ctxsw_prog_main_image_magic_value_4_o() >> 2));
	magic_5 = *(context + (ctxsw_prog_main_image_magic_value_5_o() >> 2));

	if (magic_1 != ctxsw_prog_main_image_magic_value_1_v_value_v()) {
		return false;
	}
	if (magic_2 != ctxsw_prog_main_image_magic_value_2_v_value_v()) {
		return false;
	}
	if (magic_3 != ctxsw_prog_main_image_magic_value_3_v_value_v()) {
		return false;
	}
	if (magic_4 != ctxsw_prog_main_image_magic_value_4_v_value_v()) {
		return false;
	}
	if (magic_5 != ctxsw_prog_main_image_magic_value_5_v_value_v()) {
		return false;
	}
	return true;
}

bool ga10b_ctxsw_prog_check_local_header_magic(u32 *context)
{
	u32 magic_1, magic_2, magic_3, magic_4, magic_5;

	magic_1 = *(context + (ctxsw_prog_local_magic_value_1_o() >> 2));
	magic_2 = *(context + (ctxsw_prog_local_magic_value_2_o() >> 2));
	magic_3 = *(context + (ctxsw_prog_local_magic_value_3_o() >> 2));
	magic_4 = *(context + (ctxsw_prog_local_magic_value_4_o() >> 2));
	magic_5 = *(context + (ctxsw_prog_local_magic_value_5_o() >> 2));

	if (magic_1 != ctxsw_prog_local_magic_value_1_v_value_v()) {
		return false;
	}
	if (magic_2 != ctxsw_prog_local_magic_value_2_v_value_v()) {
		return false;
	}
	if (magic_3 != ctxsw_prog_local_magic_value_3_v_value_v()) {
		return false;
	}
	if (magic_4 != ctxsw_prog_local_magic_value_4_v_value_v()) {
		return false;
	}
	if (magic_5 != ctxsw_prog_local_magic_value_5_v_value_v()) {
		return false;
	}
	return true;
}

#endif /* CONFIG_NVGPU_DEBUGGER */
