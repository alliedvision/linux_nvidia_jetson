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
#include <stdlib.h>
#include <unistd.h>
#include <unit/unit.h>
#include <unit/io.h>
#include <nvgpu/types.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/hal_init.h>
#include <nvgpu/dma.h>
#include <nvgpu/posix/io.h>
#include <os/posix/os_posix.h>
#include <nvgpu/posix/posix-fault-injection.h>
#include <nvgpu/posix/posix-nvhost.h>
#include <nvgpu/posix/posix-channel.h>
#include <nvgpu/runlist.h>
#include <nvgpu/device.h>
#include <nvgpu/channel.h>
#include <nvgpu/rc.h>
#include <nvgpu/pbdma_status.h>
#include <nvgpu/error_notifier.h>
#include <nvgpu/cic_rm.h>

#include "../fifo/nvgpu-fifo-common.h"
#include "../fifo/nvgpu-fifo-gv11b.h"
#include "nvgpu-rc.h"

#define NV_PMC_BOOT_0_ARCHITECTURE_GV110        (0x00000015 << \
						NVGPU_GPU_ARCHITECTURE_SHIFT)
#define NV_PMC_BOOT_0_IMPLEMENTATION_B          0xB

#define assert(cond)	unit_assert(cond, goto done)

static u32 stub_gv11b_gr_init_get_no_of_sm(struct gk20a *g)
{
	return 8;
}

static struct nvgpu_channel *ch = NULL;
static struct nvgpu_tsg *tsg = NULL;

static int verify_error_notifier(struct nvgpu_channel *ch, u32 error_notifier)
{
	struct nvgpu_posix_channel *cp = ch->os_priv;
	if (cp == NULL) {
		return UNIT_FAIL;
	} else if (cp->err_notifier.error == error_notifier &&
		cp->err_notifier.status == 0xffff) {
		return UNIT_SUCCESS;
	} else {
		return UNIT_FAIL;
	}
}

static void clear_error_notifier(struct nvgpu_channel *ch)
{
	struct nvgpu_posix_channel *cp = ch->os_priv;
	if (cp != NULL) {
		cp->err_notifier.error = 0U;
		cp->err_notifier.status = 0U;
	}
}

int test_rc_init(struct unit_module *m, struct gk20a *g, void *args)
{
	int ret = 0;
	struct nvgpu_posix_channel *posix_channel = NULL;

	ret = test_fifo_setup_gv11b_reg_space(m, g);
	if (ret != 0) {
		unit_return_fail(m, "fifo reg_space failure");
	}

	ret = nvgpu_pd_cache_init(g);
	if (ret != 0) {
		unit_return_fail(m, "PD cache initialization failure");
	}

	nvgpu_device_init(g);

	ret = nvgpu_cic_rm_setup(g);
	if (ret != 0) {
		unit_return_fail(m, "CIC-rm init failed\n");
	}

	ret = nvgpu_cic_rm_init_vars(g);
	if (ret != 0) {
		unit_return_fail(m, "CIC-rm vars init failed\n");
	}

	g->ops.gr.init.get_no_of_sm = stub_gv11b_gr_init_get_no_of_sm;

	g->ops.ecc.ecc_init_support(g);
	g->ops.mm.init_mm_support(g);

	ret = nvgpu_fifo_init_support(g);
	nvgpu_assert(ret == 0);

	/* Do not allocate from vidmem */
	nvgpu_set_enabled(g, NVGPU_MM_UNIFIED_MEMORY, true);

	ret = nvgpu_runlist_setup_sw(g);
	nvgpu_assert(ret == 0);

	tsg = nvgpu_tsg_open(g, getpid());
	nvgpu_assert(tsg != NULL);

	ch = nvgpu_channel_open_new(g, NVGPU_INVALID_RUNLIST_ID, false,
			getpid(), getpid());
	if (ch == NULL) {
		ret = UNIT_FAIL;
		unit_err(m, "failed channel open");
		goto clear_tsg;
	}

	posix_channel = malloc(sizeof(struct nvgpu_posix_channel));
	if (posix_channel == NULL) {
		unit_err(m, "failed to allocate memory for posix channel");
		goto clear_channel;
	}

	ch->os_priv = posix_channel;

	ret = nvgpu_tsg_bind_channel(tsg, ch);
	if (ret) {
		unit_err(m, "failed to bind channel");
		goto clear_posix_channel;
	}

	return UNIT_SUCCESS;

clear_posix_channel:
	free(posix_channel);
clear_channel:
	nvgpu_channel_close(ch);
	ch = NULL;
clear_tsg:
	nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);
	tsg = NULL;

	return ret;
}

int test_rc_deinit(struct unit_module *m, struct gk20a *g, void *args)
{
	struct nvgpu_posix_channel *posix_channel = ch->os_priv;
	int ret = nvgpu_tsg_unbind_channel(tsg, ch, true);
	if (ret != 0) {
		ret = UNIT_FAIL;
		unit_err(m , "channel already unbound");
	}

	if (ch != NULL && posix_channel != NULL) {
		free(posix_channel);
	}

	if (ch != NULL) {
		nvgpu_channel_close(ch);
	}

	if (tsg != NULL) {
		nvgpu_ref_put(&tsg->refcount, nvgpu_tsg_release);
	}

	if (g->fifo.remove_support) {
		g->fifo.remove_support(&g->fifo);
	}

	return ret;
}

int test_rc_fifo_recover(struct unit_module *m, struct gk20a *g, void *args)
{
	g->sw_quiesce_pending = true;
	clear_error_notifier(ch);
	nvgpu_rc_fifo_recover(g, 0U, 0U, false, false, false, 0U);

	g->sw_quiesce_pending = false;

	return UNIT_SUCCESS;
}

int test_rc_ctxsw_timeout(struct unit_module *m, struct gk20a *g, void *args)
{
	g->sw_quiesce_pending = true;
	clear_error_notifier(ch);
	nvgpu_rc_ctxsw_timeout(g, 0U, tsg, false);

	g->sw_quiesce_pending = false;
	return verify_error_notifier(ch, NVGPU_ERR_NOTIFIER_FIFO_ERROR_IDLE_TIMEOUT);
}

int test_rc_runlist_update(struct unit_module *m, struct gk20a *g, void *args)
{
	g->sw_quiesce_pending = true;
	nvgpu_rc_runlist_update(g, 0U);

	g->sw_quiesce_pending = false;
	return UNIT_SUCCESS;
}

int test_rc_preempt_timeout(struct unit_module *m, struct gk20a *g, void *args)
{
	g->sw_quiesce_pending = true;
	clear_error_notifier(ch);
	nvgpu_rc_preempt_timeout(g, tsg);

	g->sw_quiesce_pending = false;
	return verify_error_notifier(ch, NVGPU_ERR_NOTIFIER_FIFO_ERROR_IDLE_TIMEOUT);
}

int test_rc_gr_fault(struct unit_module *m, struct gk20a *g, void *args)
{
	g->sw_quiesce_pending = true;
	clear_error_notifier(ch);
	nvgpu_rc_gr_fault(g, tsg, ch);

	g->sw_quiesce_pending = false;
	return UNIT_SUCCESS;
}

int test_rc_sched_error_bad_tsg(struct unit_module *m, struct gk20a *g, void *args)
{
	g->sw_quiesce_pending = true;
	clear_error_notifier(ch);
	nvgpu_rc_sched_error_bad_tsg(g);

	g->sw_quiesce_pending = false;
	return UNIT_SUCCESS;
}

int test_rc_tsg_and_related_engines(struct unit_module *m, struct gk20a *g, void *args)
{
	g->sw_quiesce_pending = true;
	nvgpu_rc_tsg_and_related_engines(g, tsg, false, RC_TYPE_SCHED_ERR);

	g->sw_quiesce_pending = false;
	return UNIT_SUCCESS;
}

#define F_RC_MMU_FAULT_ID_INVALID  0
#define F_RC_MMU_FAULT_ID_TYPE_TSG 1
#define F_RC_MMU_FAULT_ID_TYPE_NOT_TSG 2

static const char *f_rc_mmu_fault[] = {
	"id_invalid",
	"id_type_tsg",
	"id_type_not_tsg",
};

int test_rc_mmu_fault(struct unit_module *m, struct gk20a *g, void *args)
{
	u32 branches;
	u32 id = NVGPU_INVALID_TSG_ID;
	u32 id_type = F_RC_MMU_FAULT_ID_TYPE_NOT_TSG;
	g->sw_quiesce_pending = true;
	clear_error_notifier(ch);

	for (branches = 0U; branches <= F_RC_MMU_FAULT_ID_TYPE_NOT_TSG; branches++) {
		if (branches != F_RC_MMU_FAULT_ID_INVALID) {
			id = tsg->tsgid;
			id_type = ID_TYPE_UNKNOWN;
		}

		if (branches == F_RC_MMU_FAULT_ID_TYPE_TSG) {
			id_type = ID_TYPE_TSG;
		}

		unit_info(m, "%s branch: %s\n", __func__, f_rc_mmu_fault[branches]);

		nvgpu_rc_mmu_fault(g, 0U, id, id_type, RC_TYPE_MMU_FAULT, NULL);
	}

	g->sw_quiesce_pending = false;
	return UNIT_SUCCESS;
}

#define F_RC_IS_CHSW_VALID_OR_SAVE  0U
#define F_RC_IS_CHSW_LOAD_OR_SWITCH 1U
#define F_RC_IS_CHSW_INVALID        2U

#define F_RC_ID_TYPE_TSG     	    0U
#define F_RC_ID_TYPE_CH             1U
#define F_RC_ID_TYPE_INVALID        2U

#define F_RC_ID_TYPE_CH_NULL_CHANNEL  0U
#define F_RC_ID_TYPE_CH_NULL_TSG      1U
#define F_RC_ID_TYPE_CH_FULL	      2U

static const char *f_rc_chsw[] = {
	"is_chsw_valid_or_save",
	"is_chsw_load_or_switch",
	"is_chsw_invalid",
};

static const char *f_rc_id_type[] = {
	"id_type_tsg",
	"id_type_ch",
	"id_type_invalid",
};

static const char *f_rc_id_ch_subbranch[] = {
	"null_channel",
	"null_tsg",
	"full",
};

static void set_pbdma_info_id_type(u32 chsw_branches,
		struct nvgpu_pbdma_status_info *info,
		struct nvgpu_channel *ch_without_tsg,
		u32 id_type_branches,
		u32 id_type_ch_branches)
{
	if (id_type_branches == F_RC_ID_TYPE_TSG) {
		info->id = (chsw_branches == F_RC_IS_CHSW_VALID_OR_SAVE) ?
			tsg->tsgid : PBDMA_STATUS_ID_INVALID;
		info->id_type = (chsw_branches == F_RC_IS_CHSW_VALID_OR_SAVE) ?
			PBDMA_STATUS_ID_TYPE_TSGID : PBDMA_STATUS_ID_TYPE_INVALID;
		info->next_id = (chsw_branches == F_RC_IS_CHSW_LOAD_OR_SWITCH) ?
			tsg->tsgid : PBDMA_STATUS_ID_INVALID;
		info->next_id_type = (chsw_branches == F_RC_IS_CHSW_LOAD_OR_SWITCH) ?
			PBDMA_STATUS_NEXT_ID_TYPE_TSGID : PBDMA_STATUS_NEXT_ID_TYPE_INVALID;
	} else if (id_type_branches == F_RC_ID_TYPE_CH) {
		if (id_type_ch_branches == F_RC_ID_TYPE_CH_NULL_CHANNEL) {
			info->id = NVGPU_INVALID_CHANNEL_ID;
			info->id_type = PBDMA_STATUS_ID_TYPE_CHID;
			info->next_id = NVGPU_INVALID_CHANNEL_ID;
			info->next_id_type = PBDMA_STATUS_NEXT_ID_TYPE_CHID;
		} else if (id_type_ch_branches == F_RC_ID_TYPE_CH_NULL_TSG) {
			/* Use ch_without_tsg for NULL TSG branch */
			info->id = (chsw_branches == F_RC_IS_CHSW_VALID_OR_SAVE) ?
				ch_without_tsg->chid : PBDMA_STATUS_ID_INVALID;
			info->id_type = (chsw_branches == F_RC_IS_CHSW_VALID_OR_SAVE) ?
				PBDMA_STATUS_ID_TYPE_CHID : PBDMA_STATUS_ID_TYPE_INVALID;
			info->next_id = (chsw_branches == F_RC_IS_CHSW_LOAD_OR_SWITCH) ?
				ch_without_tsg->chid : PBDMA_STATUS_ID_INVALID;
			info->next_id_type = (chsw_branches == F_RC_IS_CHSW_LOAD_OR_SWITCH) ?
				PBDMA_STATUS_NEXT_ID_TYPE_CHID : PBDMA_STATUS_NEXT_ID_TYPE_INVALID;
		} else {
			/* Use ch for full path */
			info->id = (chsw_branches == F_RC_IS_CHSW_VALID_OR_SAVE) ?
				ch->chid : PBDMA_STATUS_ID_INVALID;
			info->id_type = (chsw_branches == F_RC_IS_CHSW_VALID_OR_SAVE) ?
				PBDMA_STATUS_ID_TYPE_CHID : PBDMA_STATUS_ID_TYPE_INVALID;
			info->next_id = (chsw_branches == F_RC_IS_CHSW_LOAD_OR_SWITCH) ?
				ch->chid : PBDMA_STATUS_ID_INVALID;
			info->next_id_type = (chsw_branches == F_RC_IS_CHSW_LOAD_OR_SWITCH) ?
				PBDMA_STATUS_NEXT_ID_TYPE_CHID : PBDMA_STATUS_NEXT_ID_TYPE_INVALID;
		}
	} else {
		info->id_type = PBDMA_STATUS_ID_INVALID;
		info->next_id_type = PBDMA_STATUS_ID_INVALID;
	}
}

int test_rc_pbdma_fault(struct unit_module *m, struct gk20a *g, void *args)
{
	u32 chsw_branches, id_type_branches;
	u32 chsw_subbranch;

	struct nvgpu_channel *ch_without_tsg = NULL;

	ch_without_tsg = nvgpu_channel_open_new(g, NVGPU_INVALID_RUNLIST_ID, false,
			getpid(), getpid());
	if (ch_without_tsg == NULL) {
		unit_err(m, "failed channel open");
		return UNIT_FAIL;
	}

	g->sw_quiesce_pending = true;

	for (chsw_branches = F_RC_IS_CHSW_VALID_OR_SAVE;
		chsw_branches <= F_RC_IS_CHSW_INVALID; chsw_branches++) {
		struct nvgpu_pbdma_status_info info = {0};

		if (chsw_branches == F_RC_IS_CHSW_INVALID) {
			info.chsw_status = NVGPU_PBDMA_CHSW_STATUS_INVALID;
			unit_info(m, "%s branch: %s\n", __func__, f_rc_chsw[chsw_branches]);
			nvgpu_rc_pbdma_fault(g, 0U, NVGPU_ERR_NOTIFIER_PBDMA_ERROR, &info);
			continue;
		}

		for (chsw_subbranch = 0U; chsw_subbranch < 2U; chsw_subbranch++) {
			if (chsw_branches == F_RC_IS_CHSW_VALID_OR_SAVE) {
				info.chsw_status =
					(chsw_subbranch * NVGPU_PBDMA_CHSW_STATUS_VALID) +
					((1 - chsw_subbranch) * NVGPU_PBDMA_CHSW_STATUS_SAVE);
			} else {
				info.chsw_status =
					(chsw_subbranch * NVGPU_PBDMA_CHSW_STATUS_LOAD) +
					((1 - chsw_subbranch) * NVGPU_PBDMA_CHSW_STATUS_SWITCH);
			}
		}

		for (id_type_branches = F_RC_ID_TYPE_TSG; id_type_branches <= F_RC_ID_TYPE_INVALID;
				id_type_branches++) {
			u32 id_type_ch_sub_branches = 0U;
			if (id_type_branches == F_RC_ID_TYPE_CH) {
				for (id_type_ch_sub_branches = F_RC_ID_TYPE_CH_NULL_CHANNEL;
					id_type_ch_sub_branches <= F_RC_ID_TYPE_CH_FULL; id_type_ch_sub_branches++) {
					set_pbdma_info_id_type(chsw_branches, &info, ch_without_tsg,
						id_type_branches, id_type_ch_sub_branches);

					unit_info(m, "%s branch: %s - %s - %s\n", __func__,
						f_rc_chsw[chsw_branches],
						f_rc_id_type[id_type_branches],
						f_rc_id_ch_subbranch[id_type_ch_sub_branches]);

					nvgpu_rc_pbdma_fault(g, 0U, NVGPU_ERR_NOTIFIER_PBDMA_ERROR, &info);
				}
			} else {
				set_pbdma_info_id_type(chsw_branches, &info, ch_without_tsg,
					id_type_branches, id_type_ch_sub_branches);


				unit_info(m, "%s branch: %s - %s\n", __func__,
					f_rc_chsw[chsw_branches],
					f_rc_id_type[id_type_branches]);

				nvgpu_rc_pbdma_fault(g, 0U, NVGPU_ERR_NOTIFIER_PBDMA_ERROR, &info);
			}
		}
	}

	g->sw_quiesce_pending = false;

	nvgpu_channel_close(ch_without_tsg);

	return UNIT_SUCCESS;
}

struct unit_module_test nvgpu_rc_tests[] = {
	UNIT_TEST(rc_init, test_rc_init, NULL, 0),
	UNIT_TEST(rc_fifo_recover, test_rc_fifo_recover, NULL, 0),
	UNIT_TEST(rc_ctxsw_timeout, test_rc_ctxsw_timeout, NULL, 0),
	UNIT_TEST(rc_runlist_update, test_rc_runlist_update, NULL, 0),
	UNIT_TEST(rc_preempt_timeout, test_rc_preempt_timeout, NULL, 0),
	UNIT_TEST(rc_gr_fault, test_rc_gr_fault, NULL, 0),
	UNIT_TEST(rc_sched_error_bad_tsg, test_rc_sched_error_bad_tsg, NULL, 0),
	UNIT_TEST(rc_tsg_and_related_engines, test_rc_tsg_and_related_engines, NULL, 0),
	UNIT_TEST(rc_mmu_fault, test_rc_mmu_fault, NULL, 0),
	UNIT_TEST(rc_pbdma_fault, test_rc_pbdma_fault, NULL, 0),
	UNIT_TEST(rc_deinit, test_rc_deinit, NULL, 0),
};

UNIT_MODULE(nvgpu-rc, nvgpu_rc_tests, UNIT_PRIO_NVGPU_TEST);
