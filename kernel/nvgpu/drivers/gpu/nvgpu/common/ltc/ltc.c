/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/ltc.h>
#include <nvgpu/dma.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/string.h>

void nvgpu_ltc_remove_support(struct gk20a *g)
{
	struct nvgpu_ltc *ltc = g->ltc;

	nvgpu_log_fn(g, " ");

	if (ltc == NULL) {
		return;
	}

	nvgpu_kfree(g, ltc);
	g->ltc = NULL;
}

int nvgpu_init_ltc_support(struct gk20a *g)
{
	struct nvgpu_ltc *ltc = g->ltc;
	int err;

	nvgpu_log_fn(g, " ");

	if (ltc == NULL) {
		ltc = nvgpu_kzalloc(g, sizeof(*ltc));
		if (ltc == NULL) {
			return -ENOMEM;
		}
		g->ltc = ltc;
#if defined(CONFIG_NVGPU_NON_FUSA) || defined(CONFIG_NVGPU_KERNEL_MODE_SUBMIT)
		nvgpu_spinlock_init(&g->ltc->ltc_enabled_lock);
		g->mm.ltc_enabled_current = true;
		g->mm.ltc_enabled_target = true;
#endif
	}

	if (g->ops.ltc.init_fs_state != NULL) {
		g->ops.ltc.init_fs_state(g);
	}

	if ((g->ops.ltc.ecc_init != NULL) && !g->ecc.initialized) {
		err = g->ops.ltc.ecc_init(g);
		if (err != 0) {
			nvgpu_kfree(g, ltc);
			g->ltc = NULL;
			return err;
		}
	}

	if (g->ops.ltc.intr.configure != NULL) {
		nvgpu_cic_mon_intr_stall_unit_config(g, NVGPU_CIC_INTR_UNIT_LTC,
						NVGPU_CIC_INTR_ENABLE);
		g->ops.ltc.intr.configure(g);
	}

	return 0;
}

#if defined(CONFIG_NVGPU_NON_FUSA) || defined(CONFIG_NVGPU_KERNEL_MODE_SUBMIT)
void nvgpu_ltc_sync_enabled(struct gk20a *g)
{
	if (g->ops.ltc.set_enabled == NULL) {
		return;
	}

	nvgpu_spinlock_acquire(&g->ltc->ltc_enabled_lock);
	if (g->mm.ltc_enabled_current != g->mm.ltc_enabled_target) {
		g->ops.ltc.set_enabled(g, g->mm.ltc_enabled_target);
		g->mm.ltc_enabled_current = g->mm.ltc_enabled_target;
	}
	nvgpu_spinlock_release(&g->ltc->ltc_enabled_lock);
}
#endif

u32 nvgpu_ltc_get_ltc_count(struct gk20a *g)
{
	return g->ltc->ltc_count;
}

u32 nvgpu_ltc_get_slices_per_ltc(struct gk20a *g)
{
	return g->ltc->slices_per_ltc;
}

u32 nvgpu_ltc_get_cacheline_size(struct gk20a *g)
{
	return g->ltc->cacheline_size;
}

int nvgpu_ecc_counter_init_per_lts(struct gk20a *g,
		struct nvgpu_ecc_stat ***stat, const char *name)
{
	struct nvgpu_ecc_stat **stats;
	u32 ltc, lts;
	char ltc_str[10] = {0}, lts_str[10] = {0};
	int err = 0;
	u32 ltc_count = nvgpu_ltc_get_ltc_count(g);
	u32 slices_per_ltc = nvgpu_ltc_get_slices_per_ltc(g);

	stats = nvgpu_kzalloc(g, nvgpu_safe_mult_u64(sizeof(*stats),
						     ltc_count));
	if (stats == NULL) {
		return -ENOMEM;
	}

	for (ltc = 0; ltc < ltc_count; ltc++) {
		stats[ltc] = nvgpu_kzalloc(g,
			nvgpu_safe_mult_u64(sizeof(*stats[ltc]),
					    slices_per_ltc));
		if (stats[ltc] == NULL) {
			err = -ENOMEM;
			goto fail;
		}
	}

	for (ltc = 0; ltc < ltc_count; ltc++) {
		for (lts = 0; lts < slices_per_ltc; lts++) {
			/**
			 * Store stats name as below:
			 * ltc<ltc_value>_lts<lts_value>_<name_string>
			 */
			(void)strcpy(stats[ltc][lts].name, "ltc");
			(void)nvgpu_strnadd_u32(ltc_str, ltc,
							sizeof(ltc_str), 10U);
			(void)strncat(stats[ltc][lts].name, ltc_str,
						NVGPU_ECC_STAT_NAME_MAX_SIZE -
						strlen(stats[ltc][lts].name));
			(void)strncat(stats[ltc][lts].name, "_lts",
						NVGPU_ECC_STAT_NAME_MAX_SIZE -
						strlen(stats[ltc][lts].name));
			(void)nvgpu_strnadd_u32(lts_str, lts,
							sizeof(lts_str), 10U);
			(void)strncat(stats[ltc][lts].name, lts_str,
						NVGPU_ECC_STAT_NAME_MAX_SIZE -
						strlen(stats[ltc][lts].name));
			(void)strncat(stats[ltc][lts].name, "_",
						NVGPU_ECC_STAT_NAME_MAX_SIZE -
						strlen(stats[ltc][lts].name));
			(void)strncat(stats[ltc][lts].name, name,
						NVGPU_ECC_STAT_NAME_MAX_SIZE -
						strlen(stats[ltc][lts].name));

			nvgpu_ecc_stat_add(g, &stats[ltc][lts]);
		}
	}

	*stat = stats;

fail:
	if (err != 0) {
		while (ltc-- > 0u) {
			nvgpu_kfree(g, stats[ltc]);
		}

		nvgpu_kfree(g, stats);
	}

	return err;
}

static void ltc_ecc_free_lts_slices(struct gk20a *g, struct nvgpu_ecc_stat **ecc_stat)
{
	u32 slices_per_ltc;
	u32 ltc_count;
	u32 ltc, lts;
	struct nvgpu_ecc_stat *stat = NULL;

	ltc_count = nvgpu_ltc_get_ltc_count(g);
	slices_per_ltc = nvgpu_ltc_get_slices_per_ltc(g);

	if (ecc_stat != NULL) {
		for (ltc = 0; ltc < ltc_count; ltc++) {
			if (ecc_stat[ltc] != NULL) {
				for (lts = 0; lts < slices_per_ltc; lts++) {
					stat = &ecc_stat[ltc][lts];
					nvgpu_ecc_stat_del(g, stat);
				}
				nvgpu_kfree(g, ecc_stat[ltc]);
			}
		}
		nvgpu_kfree(g, ecc_stat);
	}
}

void nvgpu_ltc_ecc_free(struct gk20a *g)
{
	struct nvgpu_ecc *ecc = &g->ecc;

	if (g->ltc == NULL) {
		return;
	}

	ltc_ecc_free_lts_slices(g, ecc->ltc.ecc_sec_count);
	ltc_ecc_free_lts_slices(g, ecc->ltc.ecc_ded_count);
	ltc_ecc_free_lts_slices(g, ecc->ltc.rstg_ecc_parity_count);
	ltc_ecc_free_lts_slices(g, ecc->ltc.tstg_ecc_parity_count);
	ltc_ecc_free_lts_slices(g, ecc->ltc.dstg_be_ecc_parity_count);

	ecc->ltc.ecc_sec_count = NULL;
	ecc->ltc.ecc_ded_count = NULL;
	ecc->ltc.rstg_ecc_parity_count = NULL;
	ecc->ltc.tstg_ecc_parity_count = NULL;
	ecc->ltc.dstg_be_ecc_parity_count = NULL;
}
