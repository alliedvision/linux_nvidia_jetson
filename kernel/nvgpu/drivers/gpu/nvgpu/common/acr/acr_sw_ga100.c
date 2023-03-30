/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/falcon.h>

#include "common/acr/acr_priv.h"
#include "common/acr/acr_sw_tu104.h"
#include "acr_sw_ga100.h"

static u32* ga100_get_versioned_sig(struct gk20a *g, struct nvgpu_acr *acr,
	u32 *sig, u32 *sig_size)
{
	u32 ucode_version = 0U;
	u32 sig_size_words = 0U;
	u32 sig_idx = 0;

	nvgpu_log_fn(g, " ");

	g->ops.fuse.read_ucode_version(g, FALCON_ID_SEC2, &ucode_version);

	*sig_size = *sig_size/acr->num_of_sig;

	sig_idx = (!ucode_version) ? 1U : 0U;

	sig_size_words = *sig_size/4U;

	sig = sig + nvgpu_safe_mult_u32(sig_idx, sig_size_words);

	return sig;
}

void nvgpu_ga100_acr_sw_init(struct gk20a *g, struct nvgpu_acr *acr)
{
	nvgpu_log_fn(g, " ");

	acr->num_of_sig = 2U;
	nvgpu_tu104_acr_sw_init(g, acr);
	acr->get_versioned_sig = ga100_get_versioned_sig;
}
