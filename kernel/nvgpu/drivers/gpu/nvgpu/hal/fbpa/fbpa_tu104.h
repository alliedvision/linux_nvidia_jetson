/*
 * TU104 FBPA
 *
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

#ifndef NVGPU_FBPA_TU104_H
#define NVGPU_FBPA_TU104_H

struct gk20a;

int tu104_fbpa_init(struct gk20a *g);
void tu104_fbpa_handle_intr(struct gk20a *g, u32 fbpa_id);

/**
 * @brief Allocate and initialize error counters for all fbpa instances.
 *
 * @param g [in] The GPU driver struct.
 * @param stat [out] Pointer to array of tpc error counters.
 * @param name [in] Unique name for error counter.
 *
 * Calculates the total number of fbpa instances, allocates memory for each
 * instance of error counter, initializes the counter with 0 and the specified
 * string identifier. Finally the counter is added to the stats_list of
 * struct nvgpu_ecc.
 *
 * @return 0 in case of success, less than 0 for failure.
 */
int nvgpu_ecc_counter_init_per_fbpa(struct gk20a *g,
		struct nvgpu_ecc_stat **stat, const char *name);
#define NVGPU_ECC_COUNTER_INIT_PER_FBPA(stat) \
	nvgpu_ecc_counter_init_per_fbpa(g, &g->ecc.fbpa.stat, #stat)

int tu104_fbpa_ecc_init(struct gk20a *g);
void tu104_fbpa_ecc_free(struct gk20a *g);

#endif /* NVGPU_FBPA_TU104_H */
