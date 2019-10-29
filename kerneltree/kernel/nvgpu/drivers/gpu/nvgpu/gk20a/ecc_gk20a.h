/*
 * GK20A ECC
 *
 * Copyright (c) 2017-2018, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef ECC_GK20A_H
#define ECC_GK20A_H

struct gk20a_ecc_stat {
	char **names;
	u32 *counters;
	u32 count;
#ifdef CONFIG_SYSFS
	struct hlist_node hash_node;
	struct device_attribute *attr_array;
#endif
};

struct ecc_gk20a {
	/* Stats per engine */
	struct {
		struct gk20a_ecc_stat sm_lrf_single_err_count;
		struct gk20a_ecc_stat sm_lrf_double_err_count;

		struct gk20a_ecc_stat sm_shm_sec_count;
		struct gk20a_ecc_stat sm_shm_sed_count;
		struct gk20a_ecc_stat sm_shm_ded_count;

		struct gk20a_ecc_stat tex_total_sec_pipe0_count;
		struct gk20a_ecc_stat tex_total_ded_pipe0_count;
		struct gk20a_ecc_stat tex_unique_sec_pipe0_count;
		struct gk20a_ecc_stat tex_unique_ded_pipe0_count;
		struct gk20a_ecc_stat tex_total_sec_pipe1_count;
		struct gk20a_ecc_stat tex_total_ded_pipe1_count;
		struct gk20a_ecc_stat tex_unique_sec_pipe1_count;
		struct gk20a_ecc_stat tex_unique_ded_pipe1_count;

		struct gk20a_ecc_stat sm_l1_tag_corrected_err_count;
		struct gk20a_ecc_stat sm_l1_tag_uncorrected_err_count;
		struct gk20a_ecc_stat sm_cbu_corrected_err_count;
		struct gk20a_ecc_stat sm_cbu_uncorrected_err_count;
		struct gk20a_ecc_stat sm_l1_data_corrected_err_count;
		struct gk20a_ecc_stat sm_l1_data_uncorrected_err_count;
		struct gk20a_ecc_stat sm_icache_corrected_err_count;
		struct gk20a_ecc_stat sm_icache_uncorrected_err_count;
		struct gk20a_ecc_stat gcc_l15_corrected_err_count;
		struct gk20a_ecc_stat gcc_l15_uncorrected_err_count;
		struct gk20a_ecc_stat fecs_corrected_err_count;
		struct gk20a_ecc_stat fecs_uncorrected_err_count;
		struct gk20a_ecc_stat gpccs_corrected_err_count;
		struct gk20a_ecc_stat gpccs_uncorrected_err_count;
		struct gk20a_ecc_stat mmu_l1tlb_corrected_err_count;
		struct gk20a_ecc_stat mmu_l1tlb_uncorrected_err_count;
	} gr;

	struct {
		struct gk20a_ecc_stat l2_sec_count;
		struct gk20a_ecc_stat l2_ded_count;
		struct gk20a_ecc_stat l2_cache_corrected_err_count;
		struct gk20a_ecc_stat l2_cache_uncorrected_err_count;
	} ltc;

	struct {
		struct gk20a_ecc_stat mmu_l2tlb_corrected_err_count;
		struct gk20a_ecc_stat mmu_l2tlb_uncorrected_err_count;
		struct gk20a_ecc_stat mmu_hubtlb_corrected_err_count;
		struct gk20a_ecc_stat mmu_hubtlb_uncorrected_err_count;
		struct gk20a_ecc_stat mmu_fillunit_corrected_err_count;
		struct gk20a_ecc_stat mmu_fillunit_uncorrected_err_count;
	} fb;

	struct {
		struct gk20a_ecc_stat pmu_corrected_err_count;
		struct gk20a_ecc_stat pmu_uncorrected_err_count;
	} pmu;

	struct {
		struct gk20a_ecc_stat fbpa_sec_err_count;
		struct gk20a_ecc_stat fbpa_ded_err_count;
	} fbpa;

};

#endif /*__ECC_GK20A_H__*/
