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

#include <nvgpu/bitops.h>

#include "mm_gm20b.h"

bool gm20b_mm_is_bar1_supported(struct gk20a *g)
{
	(void)g;
	return true;
}

void gm20b_mm_get_default_va_sizes(u64 *aperture_size,
			u64 *user_size, u64 *kernel_size)
{
	/*
	 * The maximum GPU VA range supported.
	 * Max VA Bits = 40, refer dev_mmu.ref.
	 */
	if (aperture_size != NULL) {
		*aperture_size = BIT64(40);
	}

	/*
	 * The default userspace-visible GPU VA size.
	 */
	if (user_size != NULL) {
		*user_size = BIT64(37);
	}

	/*
	 * The default kernel-reserved GPU VA size.
	 */
	if (kernel_size != NULL) {
		*kernel_size = BIT64(32);
	}
}
