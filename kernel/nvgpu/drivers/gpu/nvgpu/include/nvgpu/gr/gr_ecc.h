/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_GR_ECC_H
#define NVGPU_GR_ECC_H

struct gk20a;
struct nvgpu_ecc_stat;

/**
 * @brief Allocate and initialize error counter specified by name for all
 * gpc-tpc instances.
 *
 * @param g [in] The GPU driver struct.
 * @param stat [out] Pointer to array of pointers of error counters.
 * @param name [in] Unique name for error counter.
 *
 * Calculates the total number of tpcs across all gpcs within the gr unit.
 * Then allocates, initializes memory to hold error counters associated with all
 * tpcs, which is then added to the stats_list in struct nvgpu_ecc.
 *
 * @return 0 in case of success, less than 0 for failure.
 * @retval -ENOMEM if memory allocation fails for any counter.
 */
int nvgpu_ecc_counter_init_per_tpc(struct gk20a *g,
		struct nvgpu_ecc_stat ***stat, const char *name);
/*
 * @brief Allocate and initialize counter for memories common across a TPC.
 *
 * @param stat [in] Address of pointer to struct nvgpu_ecc_stat.
 *
 */
#define NVGPU_ECC_COUNTER_INIT_PER_TPC_OR_RETURN(stat)		\
	do {						\
		int err = 0;				\
		err = nvgpu_ecc_counter_init_per_tpc(g,	\
				&g->ecc.gr.stat, #stat);\
		if (err != 0) {				\
			return err;			\
		}					\
	} while (false)

/**
 * @brief Free error counter of all tpc instances in all gpc instances.
 *
 * @param g [in] The GPU driver struct.
 * @param stats_p [out] Pointer to 2D array of error counters in tpcs in gpcs.
 *
 * Removes the error counter of all gpc instances from stats_list in struct
 * nvgpu_ecc and frees the memory allocated for it.
 */
void nvgpu_ecc_counter_deinit_per_tpc(struct gk20a *g,
				      struct nvgpu_ecc_stat ***stats_p);

/*
 * @brief Frees counters for memories shared across a TPCs in GPCs.
 *
 * @param stat [in] error counter member from g->ecc.gr.
 *
 */
#define NVGPU_ECC_COUNTER_DEINIT_PER_TPC(stat) \
	nvgpu_ecc_counter_deinit_per_tpc(g, &g->ecc.gr.stat)

/**
 * @brief Allocate and initialize error counter specified by name for all gpc
 * instances.
 *
 * @param g [in] The GPU driver struct.
 * @param stat [out] Pointer to array of tpc error counters.
 * @param name [in] Unique name for error counter.
 *
 * Calculates the total number of gpcs within the gr unit. Then allocates,
 * initializes memory to hold error counters associated with all gpcs, which is
 * then added to the stats_list in struct nvgpu_ecc.
 *
 * @return 0 in case of success, less than 0 for failure.
 * @retval -ENOMEM if memory allocation fails for any counter.
 */
int nvgpu_ecc_counter_init_per_gpc(struct gk20a *g,
		struct nvgpu_ecc_stat **stat, const char *name);
/*
 * @brief Allocate and initialize counters for memories shared across a GPC.
 *
 * @param stat [in] error counter member from g->ecc.gr.
 *
 */
#define NVGPU_ECC_COUNTER_INIT_PER_GPC(stat) \
	nvgpu_ecc_counter_init_per_gpc(g, &g->ecc.gr.stat, #stat)

/**
 * @brief Free error counter of all gpc instances.
 *
 * @param g [in] The GPU driver struct.
 * @param stats_p [out] Pointer to array of gpc error counters.
 *
 * Removes the error counter of all gpc instances from stats_list in struct
 * nvgpu_ecc and frees the memory allocated for it.
 */
void nvgpu_ecc_counter_deinit_per_gpc(struct gk20a *g,
				      struct nvgpu_ecc_stat **stats_p);

/*
 * @brief Frees counters for memories shared across a GPC.
 *
 * @param stat [in] error counter member from g->ecc.gr.
 *
 */
#define NVGPU_ECC_COUNTER_DEINIT_PER_GPC(stat) \
	nvgpu_ecc_counter_deinit_per_gpc(g, &g->ecc.gr.stat)

/**
 * @brief Allocate and initialize error counter specified by name for all gr
 * instances.
 *
 * @param g [in] The GPU driver struct.
 * @param stat [out] Pointer to array of error counters.
 * @param name [in] Unique name for error counter.
 *
 * Initialize the memory to hold error counters associated with each gr
 * instance. Counter name is set in the form of gr<gr_index>_<provided_name>.
 *
 * @return 0 in case of success, less than 0 for failure.
 */
int nvgpu_ecc_counter_init_per_gr(struct gk20a *g,
		struct nvgpu_ecc_stat **stat, const char *name);

/*
 * @brief Allocate and initialize counters for memories shared within GR.
 *
 * @param stat [in] Address of pointer to struct nvgpu_ecc_stat.
 *
 */
#define NVGPU_ECC_COUNTER_INIT_PER_GR(stat) \
	nvgpu_ecc_counter_init_per_gr(g, &g->ecc.gr.stat, #stat)

/**
 * @brief Free error counter of all gr instances.
 *
 * @param g [in] The GPU driver struct.
 * @param stats_p [out] Pointer to array of gr error counters.
 *
 * Removes the error counter of all gr instances from stats_list in struct
 * nvgpu_ecc and frees the memory allocated for it.
 */
void nvgpu_ecc_counter_deinit_per_gr(struct gk20a *g,
				     struct nvgpu_ecc_stat **stats_p);

/*
 * @brief Frees counters for memories shared across a GR instances.
 *
 * @param stat [in] error counter member from g->ecc.gr.
 *
 */
#define NVGPU_ECC_COUNTER_DEINIT_PER_GR(stat) \
	nvgpu_ecc_counter_deinit_per_gr(g, &g->ecc.gr.stat)

/**
 * @brief Release all GR ECC stats counters.
 *
 * @param g [in] The GPU driver struct.
 *
 * Frees all error counters associated with all gpcs in the GR unit.
 */
void nvgpu_gr_ecc_free(struct gk20a *g);

#endif
