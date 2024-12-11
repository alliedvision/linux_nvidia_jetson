/*
 * Tegra GK20A GPU Debugger Driver Register Ops
 *
 * Copyright (c) 2013-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/log.h>
#include <nvgpu/bsearch.h>
#include <nvgpu/bug.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/regops.h>
#include <nvgpu/gr/gr_instances.h>
#include <nvgpu/gr/obj_ctx.h>
#include <nvgpu/gr/gr_utils.h>
#include <nvgpu/grmgr.h>
#include <nvgpu/profiler.h>

/* Access ctx buffer offset functions in gr_gk20a.h */
#include "hal/gr/gr/gr_gk20a.h"

static int regop_bsearch_range_cmp(const void *pkey, const void *pelem)
{
	const u32 key = *(const u32 *)pkey;
	const struct regop_offset_range *prange =
		(const struct regop_offset_range *)pelem;
	if (key < prange->base) {
		return -1;
	} else if (prange->base <= key && key < (U32(prange->base) +
					       (U32(prange->count) * U32(4)))) {
		return 0;
	}
	return 1;
}

static inline bool linear_search(u32 offset, const u32 *list, u64 size)
{
	u64 i;
	for (i = 0; i < size; i++) {
		if (list[i] == offset) {
			return true;
		}
	}
	return false;
}

/*
 * In order to perform a context relative op the context has
 * to be created already... which would imply that the
 * context switch mechanism has already been put in place.
 * So by the time we perform such an opertation it should always
 * be possible to query for the appropriate context offsets, etc.
 *
 * But note: while the dbg_gpu bind requires the a channel fd,
 * it doesn't require an allocated gr/compute obj at that point...
 */
static bool gr_context_info_available(struct gk20a *g)
{
	struct nvgpu_gr_obj_ctx_golden_image *gr_golden_image =
					nvgpu_gr_get_golden_image_ptr(g);

	return nvgpu_gr_obj_ctx_is_golden_image_ready(gr_golden_image);
}

static bool validate_reg_ops(struct gk20a *g,
			    struct nvgpu_profiler_object *prof,
			    u32 *ctx_rd_count, u32 *ctx_wr_count,
			    struct nvgpu_dbg_reg_op *ops,
			    u32 op_count,
			    bool valid_ctx,
			    u32 *flags);

int exec_regops_gk20a(struct gk20a *g,
		      struct nvgpu_tsg *tsg,
		      struct nvgpu_dbg_reg_op *ops,
		      u32 num_ops,
		      u32 ctx_wr_count,
		      u32 ctx_rd_count,
		      u32 *flags)
{
	int err = 0;
	unsigned int i;
	u32 data32_lo = 0, data32_hi = 0;
	bool skip_read_lo, skip_read_hi;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, " ");

	/* be sure that ctx info is in place if there are ctx ops */
	if ((ctx_wr_count | ctx_rd_count) != 0U) {
		if (!gr_context_info_available(g)) {
			nvgpu_err(g, "gr context data not available");
			return -ENODEV;
		}
	}

	for (i = 0; i < num_ops; i++) {
		/* if it isn't global then it is done in the ctx ops... */
		if (ops[i].type != REGOP(TYPE_GLOBAL)) {
			continue;
		}

		/*
		 * Move to next op if current op is invalid.
		 * Execution will reach here only if CONTINUE_ON_ERROR
		 * mode is requested.
		 */
		if (ops[i].status != REGOP(STATUS_SUCCESS)) {
			continue;
		}

		switch (ops[i].op) {

		case REGOP(READ_32):
			ops[i].value_hi = 0;
			ops[i].value_lo = gk20a_readl(g, ops[i].offset);
			nvgpu_log(g, gpu_dbg_gpu_dbg, "read_32 0x%08x from 0x%08x",
				   ops[i].value_lo, ops[i].offset);

			break;

		case REGOP(READ_64):
			ops[i].value_lo = gk20a_readl(g, ops[i].offset);
			ops[i].value_hi =
				gk20a_readl(g, ops[i].offset + 4U);

			nvgpu_log(g, gpu_dbg_gpu_dbg, "read_64 0x%08x:%08x from 0x%08x",
				   ops[i].value_hi, ops[i].value_lo,
				   ops[i].offset);
		break;

		case REGOP(WRITE_32):
		case REGOP(WRITE_64):
			/* some of this appears wonky/unnecessary but
			   we've kept it for compat with existing
			   debugger code.  just in case... */
			skip_read_lo = skip_read_hi = false;
			if (ops[i].and_n_mask_lo == ~(u32)0) {
				data32_lo = ops[i].value_lo;
				skip_read_lo = true;
			}

			if ((ops[i].op == REGOP(WRITE_64)) &&
			    (ops[i].and_n_mask_hi == ~(u32)0)) {
				data32_hi = ops[i].value_hi;
				skip_read_hi = true;
			}

			/* read first 32bits */
			if (skip_read_lo == false) {
				data32_lo = gk20a_readl(g, ops[i].offset);
				data32_lo &= ~ops[i].and_n_mask_lo;
				data32_lo |= ops[i].value_lo;
			}

			/* if desired, read second 32bits */
			if ((ops[i].op == REGOP(WRITE_64)) &&
			    !skip_read_hi) {
				data32_hi = gk20a_readl(g, ops[i].offset + 4U);
				data32_hi &= ~ops[i].and_n_mask_hi;
				data32_hi |= ops[i].value_hi;
			}

			/* now update first 32bits */
			gk20a_writel(g, ops[i].offset, data32_lo);
			nvgpu_log(g, gpu_dbg_gpu_dbg, "Wrote 0x%08x to 0x%08x ",
				   data32_lo, ops[i].offset);
			/* if desired, update second 32bits */
			if (ops[i].op == REGOP(WRITE_64)) {
				gk20a_writel(g, ops[i].offset + 4U, data32_hi);
				nvgpu_log(g, gpu_dbg_gpu_dbg, "Wrote 0x%08x to 0x%08x ",
					   data32_hi, ops[i].offset + 4U);

			}


			break;

		/* shouldn't happen as we've already screened */
		default:
			BUG();
			err = -EINVAL;
			goto clean_up;
			break;
		}
	}

	if ((ctx_wr_count | ctx_rd_count) != 0U) {
		err = gr_gk20a_exec_ctx_ops(tsg, ops, num_ops,
					    ctx_wr_count, ctx_rd_count,
					    flags);
		if (err != 0) {
			nvgpu_warn(g, "failed to perform ctx ops");
			goto clean_up;
		}
	}

 clean_up:
	nvgpu_log(g, gpu_dbg_gpu_dbg, "ret=%d", err);
	return err;

}

#ifdef CONFIG_NVGPU_MIG
static int calculate_new_offsets_for_perf_fbp_chiplets(struct gk20a *g,
	struct nvgpu_dbg_reg_op *op, u32 reg_chiplet_base, u32 chiplet_offset)
{
	int ret = 0;
	u32 fbp_local_index, fbp_logical_index;
	u32 gr_instance_id;
	u32 new_offset;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, " ");

	gr_instance_id = nvgpu_gr_get_cur_instance_id(g);

	if (chiplet_offset == 0U) {
		nvgpu_err(g, "Invalid chiplet offsets");
		return -EINVAL;
	}

	fbp_local_index = nvgpu_safe_sub_u32(op->offset, reg_chiplet_base)/chiplet_offset;
	/*
	 * Validate fbp_local_index for current MIG Instance.
	 * Substitute local indexes with logical indexes.
	 * Obtain new offset.
	 */

	if (fbp_local_index >= nvgpu_grmgr_get_gr_num_fbps(g, gr_instance_id)) {
		ret = -EINVAL;
		nvgpu_err(g, "Invalid FBP Index");
		return ret;
	}

	/* At present, FBP indexes doesn't need conversion */
	if (nvgpu_grmgr_get_memory_partition_support_status(g, gr_instance_id)) {
		fbp_logical_index = nvgpu_grmgr_get_fbp_logical_id(g,
			gr_instance_id, fbp_local_index);

		new_offset = nvgpu_safe_sub_u32(op->offset,
			nvgpu_safe_mult_u32(fbp_local_index, chiplet_offset));

		new_offset = nvgpu_safe_add_u32(new_offset,
			nvgpu_safe_mult_u32(fbp_logical_index, chiplet_offset));

		nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
			"old offset: 0x%08x, new offset = 0x%08x, Local index = %u, logical index = %u",
			op->offset, new_offset, fbp_local_index, fbp_logical_index);

		op->offset = new_offset;
	}

	return 0;
}

static int calculate_new_offsets_for_perf_gpc_chiplets(struct gk20a *g,
	struct nvgpu_dbg_reg_op *op, u32 reg_chiplet_base, u32 chiplet_offset)
{
	int ret = 0;
	u32 gr_instance_id;
	u32 gpc_local_index;
	u32 gpc_logical_index, new_offset;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, " ");

	gr_instance_id = nvgpu_gr_get_cur_instance_id(g);

	if (chiplet_offset == 0U) {
		nvgpu_err(g, "Invalid chiplet offsets");
		return -EINVAL;
	}

	gpc_local_index = nvgpu_safe_sub_u32(op->offset, reg_chiplet_base)/chiplet_offset;
	/*
	 * Validate gpc_local_index for current MIG Instance.
	 * Substitute local indexes with logical indexes.
	 * Obtain new offset.
	 */

	/* Validate whether gpc_local_index value is within the partition limits */
	if (gpc_local_index >= nvgpu_grmgr_get_gr_num_gpcs(g, gr_instance_id)) {
		ret = -EINVAL;
		nvgpu_err(g, "Invalid GPC Index");
		return ret;
	}

	gpc_logical_index = nvgpu_grmgr_get_gr_gpc_logical_id(g,
		gr_instance_id, gpc_local_index);

	new_offset = nvgpu_safe_sub_u32(op->offset,
		nvgpu_safe_mult_u32(gpc_local_index, chiplet_offset));

	new_offset = nvgpu_safe_add_u32(new_offset,
		nvgpu_safe_mult_u32(gpc_logical_index, chiplet_offset));

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg,
		"old Offset: 0x%08x, new Offset = 0x%08x, local index = %u, logical index = %u, physical index = %u",
		op->offset, new_offset, gpc_local_index, gpc_logical_index,
		nvgpu_grmgr_get_gr_gpc_phys_id(g, gr_instance_id, gpc_local_index));

	op->offset = new_offset;

	return 0;
}

static int translate_regops_for_profiler(struct gk20a *g,
	struct nvgpu_profiler_object *prof,
	struct nvgpu_dbg_reg_op *op,
	struct nvgpu_pm_resource_register_range_map *entry)
{
	int ret = 0;
	u32 chiplet_offset;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, " ");

	if ((entry->type != NVGPU_HWPM_REGISTER_TYPE_HWPM_PERFMON) &&
			(entry->type != NVGPU_HWPM_REGISTER_TYPE_HWPM_ROUTER)) {
		return 0;
	}

	if (entry->start == g->ops.perf.get_hwpm_gpc_perfmon_regs_base(g)) {
		chiplet_offset = g->ops.perf.get_pmmgpc_per_chiplet_offset();
		ret = calculate_new_offsets_for_perf_gpc_chiplets(g,
			op, entry->start, chiplet_offset);
	} else if (entry->start == g->ops.perf.get_hwpm_gpcrouter_perfmon_regs_base(g)) {
		chiplet_offset = g->ops.perf.get_pmmgpcrouter_per_chiplet_offset();
		ret = calculate_new_offsets_for_perf_gpc_chiplets(g,
			op, entry->start, chiplet_offset);
	} else if (entry->start == g->ops.perf.get_hwpm_fbp_perfmon_regs_base(g)) {
		chiplet_offset = g->ops.perf.get_pmmfbp_per_chiplet_offset();
		ret = calculate_new_offsets_for_perf_fbp_chiplets(g, op,
			entry->start, chiplet_offset);
	} else if (entry->start == g->ops.perf.get_hwpm_fbprouter_perfmon_regs_base(g)) {
		chiplet_offset = g->ops.perf.get_pmmfbprouter_per_chiplet_offset();
		ret = calculate_new_offsets_for_perf_fbp_chiplets(g, op,
			entry->start, chiplet_offset);
	}

	return ret;
}

static int translate_regops_without_profiler(struct gk20a *g,
	struct nvgpu_dbg_reg_op *op)
{
	struct nvgpu_pm_resource_register_range_map entry = {0};

	u32 gpc_reg_begin = g->ops.perf.get_hwpm_gpc_perfmon_regs_base(g);
	u32 gpc_reg_end = nvgpu_safe_add_u32(gpc_reg_begin,
		nvgpu_safe_mult_u32(nvgpu_get_litter_value(g, GPU_LIT_NUM_GPCS),
			g->ops.perf.get_pmmgpc_per_chiplet_offset()));
	u32 gpcrouter_reg_begin = g->ops.perf.get_hwpm_gpcrouter_perfmon_regs_base(g);
	u32 gpcrouter_reg_end = nvgpu_safe_add_u32(gpcrouter_reg_begin,
		nvgpu_safe_mult_u32(nvgpu_get_litter_value(g, GPU_LIT_NUM_GPCS),
			g->ops.perf.get_pmmgpcrouter_per_chiplet_offset()));
	u32 fbp_reg_begin = g->ops.perf.get_hwpm_fbp_perfmon_regs_base(g);
	u32 fbp_reg_end = nvgpu_safe_add_u32(fbp_reg_begin,
		nvgpu_safe_mult_u32(nvgpu_get_litter_value(g, GPU_LIT_NUM_FBPS),
			g->ops.perf.get_pmmfbp_per_chiplet_offset()));
	u32 fbprouter_reg_begin = g->ops.perf.get_hwpm_fbprouter_perfmon_regs_base(g);
	u32 fbprouter_reg_end = nvgpu_safe_add_u32(fbprouter_reg_begin,
		nvgpu_safe_mult_u32(nvgpu_get_litter_value(g, GPU_LIT_NUM_FBPS),
			g->ops.perf.get_pmmfbprouter_per_chiplet_offset()));

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, " ");

	if (op->offset >= gpc_reg_begin && op->offset < gpc_reg_end) {
		entry.start = gpc_reg_begin;
		entry.end = gpc_reg_end;
		entry.type = NVGPU_HWPM_REGISTER_TYPE_HWPM_PERFMON;
	} else if (op->offset >= gpcrouter_reg_begin && op->offset < gpcrouter_reg_end) {
		entry.start = gpcrouter_reg_begin;
		entry.end = gpcrouter_reg_end;
		entry.type = NVGPU_HWPM_REGISTER_TYPE_HWPM_ROUTER;
	} else if (op->offset >= fbp_reg_begin && op->offset < fbp_reg_end) {
		entry.start = fbp_reg_begin;
		entry.end = fbp_reg_end;
		entry.type = NVGPU_HWPM_REGISTER_TYPE_HWPM_PERFMON;
	} else if (op->offset >= fbprouter_reg_begin && op->offset < fbprouter_reg_end) {
		entry.start = fbprouter_reg_begin;
		entry.end = fbprouter_reg_end;
		entry.type = NVGPU_HWPM_REGISTER_TYPE_HWPM_ROUTER;
	} else {
		return 0;
	}

	return translate_regops_for_profiler(g, NULL, op, &entry);
}

#endif /* CONFIG_NVGPU_MIG */

int nvgpu_regops_exec(struct gk20a *g,
		struct nvgpu_tsg *tsg,
		struct nvgpu_profiler_object *prof,
		struct nvgpu_dbg_reg_op *ops,
		u32 num_ops,
		u32 *flags)
{
	u32 ctx_rd_count = 0, ctx_wr_count = 0;
	int err = 0;
	bool ok;

	nvgpu_log(g, gpu_dbg_fn | gpu_dbg_gpu_dbg, " ");

	ok = validate_reg_ops(g, prof, &ctx_rd_count, &ctx_wr_count,
		ops, num_ops, tsg != NULL, flags);
	if (!ok) {
		nvgpu_err(g, "invalid op(s)");
		return -EINVAL;
	}

	err = g->ops.regops.exec_regops(g, tsg, ops, num_ops, ctx_wr_count,
					ctx_rd_count, flags);
	if (err != 0) {
		nvgpu_warn(g, "failed to perform regops, err=%d", err);
	}

	return err;
}

static int validate_reg_op_info(struct nvgpu_dbg_reg_op *op)
{
	int err = 0;

	switch (op->op) {
	case REGOP(READ_32):
	case REGOP(READ_64):
	case REGOP(WRITE_32):
	case REGOP(WRITE_64):
		break;
	default:
		op->status |= REGOP(STATUS_UNSUPPORTED_OP);
		err = -EINVAL;
		break;
	}

	switch (op->type) {
	case REGOP(TYPE_GLOBAL):
	case REGOP(TYPE_GR_CTX):
	case REGOP(TYPE_GR_CTX_TPC):
	case REGOP(TYPE_GR_CTX_SM):
	case REGOP(TYPE_GR_CTX_CROP):
	case REGOP(TYPE_GR_CTX_ZROP):
	case REGOP(TYPE_GR_CTX_QUAD):
		break;
	/*
	case NVGPU_DBG_GPU_REG_OP_TYPE_FB:
	*/
	default:
		op->status |= REGOP(STATUS_INVALID_TYPE);
		err = -EINVAL;
		break;
	}

	return err;
}

static bool check_whitelists(struct gk20a *g,
			     struct nvgpu_dbg_reg_op *op,
			     u32 offset,
			     bool valid_ctx)
{
	bool valid = false;

	if (op->type == REGOP(TYPE_GLOBAL)) {
		/* search global list */
		valid = (g->ops.regops.get_global_whitelist_ranges != NULL) &&
		        (nvgpu_bsearch(&offset,
			        g->ops.regops.get_global_whitelist_ranges(),
			        g->ops.regops.get_global_whitelist_ranges_count(),
			        sizeof(*g->ops.regops.get_global_whitelist_ranges()),
			        regop_bsearch_range_cmp) != NULL);

		/* if debug session, search context list */
		if ((!valid) && (valid_ctx)) {
			/* binary search context list */
			valid = (g->ops.regops.get_context_whitelist_ranges != NULL) &&
			        (nvgpu_bsearch(&offset,
				        g->ops.regops.get_context_whitelist_ranges(),
				        g->ops.regops.get_context_whitelist_ranges_count(),
				        sizeof(*g->ops.regops.get_context_whitelist_ranges()),
				        regop_bsearch_range_cmp) != NULL);
		}

		/* if debug session, search runcontrol list */
		if ((!valid) && (valid_ctx)) {
			valid = (g->ops.regops.get_runcontrol_whitelist != NULL) &&
				linear_search(offset,
					     g->ops.regops.get_runcontrol_whitelist(),
					     g->ops.regops.get_runcontrol_whitelist_count());
		}
	} else if (op->type == REGOP(TYPE_GR_CTX)) {
		/* binary search context list */
		valid = (g->ops.regops.get_context_whitelist_ranges != NULL) &&
		        (nvgpu_bsearch(&offset,
			        g->ops.regops.get_context_whitelist_ranges(),
			        g->ops.regops.get_context_whitelist_ranges_count(),
			        sizeof(*g->ops.regops.get_context_whitelist_ranges()),
			        regop_bsearch_range_cmp) != NULL);

		/* if debug session, search runcontrol list */
		if ((!valid) && (valid_ctx)) {
			valid = (g->ops.regops.get_runcontrol_whitelist != NULL) &&
				linear_search(offset,
					     g->ops.regops.get_runcontrol_whitelist(),
					     g->ops.regops.get_runcontrol_whitelist_count());
		}
	}

	return valid;
}

static int profiler_obj_validate_reg_op_offset(struct nvgpu_profiler_object *prof,
		struct nvgpu_dbg_reg_op *op)
{
	struct gk20a *g = prof->g;
	bool valid = false;
	u32 offset;
	int ret = 0;
	struct nvgpu_pm_resource_register_range_map entry, entry64 = {0};

	offset = op->offset;

	/* support only 24-bit 4-byte aligned offsets */
	if ((offset & 0xFF000003U) != 0U) {
		nvgpu_err(g, "invalid regop offset: 0x%x", offset);
		op->status |= REGOP(STATUS_INVALID_OFFSET);
		return -EINVAL;
	}

	valid = nvgpu_profiler_allowlist_range_search(g, prof->map,
		prof->map_count, offset, &entry);
	if ((op->op == REGOP(READ_64) || op->op == REGOP(WRITE_64)) && valid) {
		valid = nvgpu_profiler_allowlist_range_search(g, prof->map,
					prof->map_count, offset + 4U, &entry64);
	}

	if (!valid) {
		goto error;
	}

	if (op->op == REGOP(READ_64) || op->op == REGOP(WRITE_64)) {
		nvgpu_assert(entry.type == entry64.type);
	}

#ifdef CONFIG_NVGPU_MIG
	/* Validate input register offset first and then translate */
	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		ret = translate_regops_for_profiler(g, prof, op, &entry);
		if (ret != 0) {
			goto error;
		}
	}
#endif

	op->type = (u8)prof->reg_op_type[entry.type];

	return 0;
error:
	op->status |= REGOP(STATUS_INVALID_OFFSET);
	(void)ret;
	return -EINVAL;
}

/* note: the op here has already been through validate_reg_op_info */
static int validate_reg_op_offset(struct gk20a *g,
				  struct nvgpu_dbg_reg_op *op,
				  bool valid_ctx)
{
	int ret = 0;
	u32 offset;
	bool valid = false;

	offset = op->offset;

	/* support only 24-bit 4-byte aligned offsets */
	if ((offset & 0xFF000003U) != 0U) {
		nvgpu_err(g, "invalid regop offset: 0x%x", offset);
		op->status |= REGOP(STATUS_INVALID_OFFSET);
		return -EINVAL;
	}

	valid = check_whitelists(g, op, offset, valid_ctx);
	if ((op->op == REGOP(READ_64) || op->op == REGOP(WRITE_64)) && valid) {
		valid = check_whitelists(g, op, offset + 4U, valid_ctx);
	}

	if (!valid) {
		nvgpu_err(g, "invalid regop offset: 0x%x", offset);
		op->status |= REGOP(STATUS_INVALID_OFFSET);
		return -EINVAL;
	}

#ifdef CONFIG_NVGPU_MIG
	/* Validate input register offset first and then translate */
	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_MIG)) {
		ret = translate_regops_without_profiler(g, op);
		if (ret != 0) {
			return ret;
		}
	}
#endif
	(void)ret;
	return 0;
}

static bool validate_reg_ops(struct gk20a *g,
			    struct nvgpu_profiler_object *prof,
			    u32 *ctx_rd_count, u32 *ctx_wr_count,
			    struct nvgpu_dbg_reg_op *ops,
			    u32 op_count,
			    bool valid_ctx,
			    u32 *flags)
{
	bool all_or_none = (*flags) & NVGPU_REG_OP_FLAG_MODE_ALL_OR_NONE;
	bool gr_ctx_ops = false;
	bool op_failed = false;
	u32 i;

	/* keep going until the end so every op can get
	 * a separate error code if needed */
	for (i = 0; i < op_count; i++) {
		ops[i].status = 0U;

		/* if "allow_all" flag enabled, dont validate offset */
		if (!g->allow_all) {
			if (prof != NULL) {
				if (profiler_obj_validate_reg_op_offset(prof, &ops[i]) != 0) {
					op_failed = true;
					if (all_or_none) {
						break;
					}
				}
			} else {
				if (validate_reg_op_offset(g, &ops[i], valid_ctx) != 0) {
					op_failed = true;
					if (all_or_none) {
						break;
					}
				}
			}
		}

		if (validate_reg_op_info(&ops[i]) != 0) {
			op_failed = true;
			if (all_or_none) {
				break;
			}
		}

		if (reg_op_is_gr_ctx(ops[i].type)) {
			if (reg_op_is_read(ops[i].op)) {
				(*ctx_rd_count)++;
			} else {
				(*ctx_wr_count)++;
			}

			gr_ctx_ops = true;
		}

		/* context operations need valid context */
		if (gr_ctx_ops && !valid_ctx) {
			op_failed = true;
			if (all_or_none) {
				break;
			}
		}

		if (ops[i].status == 0U) {
			ops[i].status = REGOP(STATUS_SUCCESS);
		}
	}

	nvgpu_log(g, gpu_dbg_gpu_dbg, "ctx_wrs:%d ctx_rds:%d",
		   *ctx_wr_count, *ctx_rd_count);

	if (all_or_none) {
		if (op_failed) {
			return false;
		} else {
			return true;
		}
	}

	/* Continue on error */
	if (!op_failed) {
		*flags |= NVGPU_REG_OP_FLAG_ALL_PASSED;
	}

	return true;
}

/* exported for tools like cyclestats, etc */
bool is_bar0_global_offset_whitelisted_gk20a(struct gk20a *g, u32 offset)
{
	bool valid = nvgpu_bsearch(&offset,
			g->ops.regops.get_global_whitelist_ranges(),
			g->ops.regops.get_global_whitelist_ranges_count(),
			sizeof(*g->ops.regops.get_global_whitelist_ranges()),
			regop_bsearch_range_cmp) != NULL;
	return valid;
}

bool reg_op_is_gr_ctx(u8 type)
{
	return  type == REGOP(TYPE_GR_CTX) ||
		type == REGOP(TYPE_GR_CTX_TPC) ||
		type == REGOP(TYPE_GR_CTX_SM) ||
		type == REGOP(TYPE_GR_CTX_CROP) ||
		type == REGOP(TYPE_GR_CTX_ZROP) ||
		type == REGOP(TYPE_GR_CTX_QUAD);
}

bool reg_op_is_read(u8 op)
{
	return  op == REGOP(READ_32) ||
		op == REGOP(READ_64);
}
