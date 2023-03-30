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

#include <nvgpu/enabled.h>
#include <nvgpu/bitops.h>
#include <nvgpu/log.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/utils.h>

/**
 * Array of flag names
 */
#define DEFINE_FLAG(flag, desc) [flag] = nvgpu_stringify(flag)
static const char *enabled_flag_names[NVGPU_MAX_ENABLED_BITS + 1U] = {
	ENABLED_FLAGS
};
#undef DEFINE_FLAG

/**
 * Array of flag descriptions
 */
#define DEFINE_FLAG(flag, desc) [flag] = desc
static const char *enabled_flag_desc[NVGPU_MAX_ENABLED_BITS + 1U] = {
	ENABLED_FLAGS
};
#undef DEFINE_FLAG

void nvgpu_print_enabled_flags(struct gk20a *g)
{
	u32 i;

	nvgpu_log(g, gpu_dbg_info, "NVGPU support flags status");
	nvgpu_log(g, gpu_dbg_info, "%-55.55s %-6.6s %s",
			"Flag", "Status", "Description");
	nvgpu_log(g, gpu_dbg_info, "%-55.55s %-6.6s %s",
			"----", "------", "-----------");

	for (i = 0U; i < U32(NVGPU_MAX_ENABLED_BITS); i++) {
		nvgpu_log(g, gpu_dbg_info, "%-55.55s %-6.6s %s",
			enabled_flag_names[i],
			nvgpu_is_enabled(g, i) ? "true" : "false",
			enabled_flag_desc[i]);
	}
}

int nvgpu_init_enabled_flags(struct gk20a *g)
{
	/*
	 * Zero all flags initially. Flags that should be set to non-zero states
	 * can be done so during driver init.
	 */
	g->enabled_flags = nvgpu_kzalloc(g,
				BITS_TO_LONGS(U32(NVGPU_MAX_ENABLED_BITS)) *
				sizeof(unsigned long));
	if (g->enabled_flags == NULL) {
		return -ENOMEM;
	}

	return 0;
}

/*
 * Call this on driver shutdown!
 */
void nvgpu_free_enabled_flags(struct gk20a *g)
{
	nvgpu_kfree(g, g->enabled_flags);
}

bool nvgpu_is_enabled(struct gk20a *g, u32 flag)
{
	if (flag < NVGPU_MAX_ENABLED_BITS) {
		return nvgpu_test_bit(flag, g->enabled_flags);
	} else {
		return 0;
	}
}

void nvgpu_set_enabled(struct gk20a *g, u32 flag, bool state)
{
	if (flag >= NVGPU_MAX_ENABLED_BITS) {
		return;
	}

	if (state) {
		nvgpu_set_bit(flag, g->enabled_flags);
	} else {
		nvgpu_clear_bit(flag, g->enabled_flags);
	}
}
