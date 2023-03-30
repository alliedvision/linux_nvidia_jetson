/*
 * Copyright (c) 2011-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_RC_H
#define NVGPU_RC_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * Some hardware faults that halt the gpu are recoverable i.e. after the fault
 * is hit, some hardware/software sequence has to be followed by the nvgpu
 * driver which can make the gpu resume its operation. This file describes such
 * recovery APIs.
 */

/**
 * @defgroup NVGPU_RC_TYPES_DEFINES
 * @ingroup NVGPU_RC_TYPES_DEFINES
 * @{
 */

/**
 * No recovery.
 */
#define RC_TYPE_NO_RC			0U
/**
 * MMU fault recovery.
 */
#define RC_TYPE_MMU_FAULT		1U
/**
 * PBDMA fault recovery.
 */
#define RC_TYPE_PBDMA_FAULT		2U
/**
 * GR fault recovery.
 */
#define RC_TYPE_GR_FAULT		3U
/**
 * Preemption timeout recovery.
 */
#define RC_TYPE_PREEMPT_TIMEOUT		4U
/**
 * CTXSW timeout recovery.
 */
#define RC_TYPE_CTXSW_TIMEOUT		5U
/**
 * Runlist update timeout recovery.
 */
#define RC_TYPE_RUNLIST_UPDATE_TIMEOUT	6U
/**
 * Forced recovery.
 */
#define RC_TYPE_FORCE_RESET		7U
/**
 * Scheduler error recovery.
 */
#define RC_TYPE_SCHED_ERR		8U
/**
 * Copy-engine error recovery.
 */
#define RC_TYPE_CE_FAULT		9U

/**
 * Invalid recovery id.
 */
#define INVAL_ID			(~U32(0U))

/**
 * @}
 */

/*
 * Requires a string literal for the format - notice the string
 * concatination.
 */
#define rec_dbg(g, fmt, args...)					\
	nvgpu_log((g), gpu_dbg_rec, "REC | " fmt, ##args)

struct gk20a;
struct nvgpu_fifo;
struct nvgpu_tsg;
struct nvgpu_channel;
struct nvgpu_pbdma_status_info;
struct mmu_fault_info;

static inline const char *nvgpu_rc_type_to_str(unsigned int rc_type)
{
	const char *str = NULL;

	switch (rc_type) {
	case RC_TYPE_NO_RC:
		str = "None";
		break;
	case RC_TYPE_MMU_FAULT:
		str = "MMU fault";
		break;
	case RC_TYPE_PBDMA_FAULT:
		str = "PBDMA fault";
		break;
	case RC_TYPE_GR_FAULT:
		str = "GR fault";
		break;
	case RC_TYPE_PREEMPT_TIMEOUT:
		str = "Preemption timeout";
		break;
	case RC_TYPE_CTXSW_TIMEOUT:
		str = "CTXSW timeout";
		break;
	case RC_TYPE_RUNLIST_UPDATE_TIMEOUT:
		str = "RL Update timeout";
		break;
	case RC_TYPE_FORCE_RESET:
		str = "Force reset";
		break;
	case RC_TYPE_SCHED_ERR:
		str = "Sched err";
		break;
	case RC_TYPE_CE_FAULT:
		str = "Copy engine err";
		break;
	default:
		str = "Unknown";
		break;
	}

	return str;
}

/**
 * @brief Do recovery processing for ctxsw timeout.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param eng_bitmask [in]	Engine bitmask.
 * @param tsg [in]		Pointer to TSG struct.
 * @param debug_dump [in]	Whether debug dump required or not.
 *
 * Trigger SW/HW sequence to recover from timeout during context switch. For
 * safety, just set error notifier to NVGPU_ERR_NOTIFIER_FIFO_ERROR_IDLE_TIMEOUT
 * and print warning if quiesce is not triggered already.
 */
void nvgpu_rc_ctxsw_timeout(struct gk20a *g, u32 eng_bitmask,
				struct nvgpu_tsg *tsg, bool debug_dump);

/**
 * @brief Do recovery processing for PBDMA fault.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param pbdma_id [in]		Pbdma identifier.
 * @param error_notifier [in]	Error notifier type to be set.
 * @param pbdma_status [in]	Pointer to PBDMA status info.
 *
 * Do PBDMA fault recovery. Set error notifier as per \a error_notifier and call
 * \a nvgpu_rc_tsg_and_related_engines to do the recovery.
 */
void nvgpu_rc_pbdma_fault(struct gk20a *g, u32 pbdma_id, u32 error_notifier,
			struct nvgpu_pbdma_status_info *pbdma_status);

/**
 * @brief Do recovery processing for runlist update timeout.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param runlist_id [in]	Runlist identifier.
 *
 * Do runlist update timeout recovery. For safety, just print warning if quiesce
 * is not triggered already.
 */
void nvgpu_rc_runlist_update(struct gk20a *g, u32 runlist_id);

/**
 * @brief Do recovery processing for preemption timeout.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param tsg [in]		Pointer to TSG struct.
 *
 * Do preemption timeout recovery. For safety, just set error notifier to
 * NVGPU_ERR_NOTIFIER_FIFO_ERROR_IDLE_TIMEOUT and call BUG() if quiesce is
 * not triggered already.
 */
void nvgpu_rc_preempt_timeout(struct gk20a *g, struct nvgpu_tsg *tsg);

/**
 * @brief Do recovery processing for GR fault.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param tsg [in]		Pointer to TSG struct.
 * @param ch [in]		Pointer to channel struct.
 *
 * Do GR fault recovery. For safety, just print warning if quiesce is not
 * triggered already.
 */
void nvgpu_rc_gr_fault(struct gk20a *g,
			struct nvgpu_tsg *tsg, struct nvgpu_channel *ch);

/**
 * @brief Do recovery processing for bad TSG sched error.
 *
 * @param g [in]		Pointer to GPU driver struct.
 *
 * Do bad TSG sched error recovery. For safety, just print warning if quiesce is
 * not triggered already.
 */
void nvgpu_rc_sched_error_bad_tsg(struct gk20a *g);

/**
 * @brief Do recovery processing for TSG and related engines.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param tsg [in]		Pointer to TSG struct.
 * @param debug_dump [in]	Whether debug dump required or not.
 * @param rc_type [in]		Recovery type.
 *
 * Do TSG and related engines recovery dependending on the \a rc_type. For
 * safety just print warning if quiesce is not triggered already.
 */
void nvgpu_rc_tsg_and_related_engines(struct gk20a *g, struct nvgpu_tsg *tsg,
			 bool debug_dump, u32 rc_type);

/**
 * @brief Do recovery processing for mmu fault.
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param act_eng_bitmask [in]	Engine bitmask.
 * @param id [in]		Hw identifier.
 * @param id_type [in]		Hw id type.
 * @param rc_type [in]		Recovery type.
 * @param mmufault [in]		Mmu fault info
 *
 * Do mmu fault recovery dependending on the \a rc_type, \a act_eng_bitmask,
 * \a hw_id and \a id_type.
 * For safety,
 * - set error notifier to NVGPU_ERR_NOTIFIER_FIFO_ERROR_MMU_ERR_FLT
 *   when \a id_type is TSG.
 * - Mark the channels of that TSG as unserviceable when \a id_type is TSG
 * - print warning if quiesce is not triggered already.
 */
void nvgpu_rc_mmu_fault(struct gk20a *g, u32 act_eng_bitmask,
			u32 id, unsigned int id_type, unsigned int rc_type,
			 struct mmu_fault_info *mmufault);

/**
 * @brief The core recovery sequence to teardown faulty TSG
 *
 * @param g [in]		Pointer to GPU driver struct.
 * @param eng_bitmask [in]	Engine bitmask.
 * @param hw_id [in]		Hardware identifier.
 * @param id_is_tsg [in]	Whether hw id is TSG or not.
 * @param id_is_known [in]	Whether hw id is known or not.
 * @param debug_dump [in]	Whether debug dump required or not.
 * @param rc_type [in]		Recovery type.
 *
 * Perform the manual recommended channel teardown sequence depending on the
 * \a rc_type, \a eng_bitmask, \a hw_id, \a id_is_tsg and \a id_is_known. For
 * safety, just print warning if quiesce is not triggered already.
 */
void nvgpu_rc_fifo_recover(struct gk20a *g,
			u32 eng_bitmask, /* if zero, will be queried from HW */
			u32 hw_id, /* if ~0, will be queried from HW */
			bool id_is_tsg, /* ignored if hw_id == ~0 */
			bool id_is_known, bool debug_dump, u32 rc_type);


void nvgpu_rc_ce_fault(struct gk20a *g, u32 inst_id);
#endif /* NVGPU_RC_H */
