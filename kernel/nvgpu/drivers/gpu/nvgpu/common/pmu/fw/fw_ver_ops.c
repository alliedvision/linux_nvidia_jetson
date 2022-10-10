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

#include <nvgpu/pmu.h>
#include <nvgpu/dma.h>
#include <nvgpu/log.h>
#include <nvgpu/bug.h>
#include <nvgpu/pmu/pmuif/nvgpu_cmdif.h>
#include <nvgpu/firmware.h>
#include <nvgpu/enabled.h>
#include <nvgpu/utils.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/boardobjgrp.h>
#include <nvgpu/pmu/volt.h>
#include <nvgpu/pmu/clk/clk.h>
#include <nvgpu/pmu/pmu_perfmon.h>
#include <nvgpu/pmu/allocator.h>
#include <nvgpu/pmu/lsfm.h>
#include <nvgpu/pmu/super_surface.h>
#include <nvgpu/pmu/fw.h>
#include <nvgpu/pmu/seq.h>

/* PMU F/W version */
#define APP_VERSION_NVGPU_NEXT_CORE 0U
#define APP_VERSION_NVGPU_NEXT	29323513U
#define APP_VERSION_TU10X	28084434U
#define APP_VERSION_GV11B	25005711U
#define APP_VERSION_GV10X	25633490U
#define APP_VERSION_GP10X	24076634U
#define APP_VERSION_GP10B	29888552U
#define APP_VERSION_GM20B	20490253U

/* PMU version specific functions */
static u32 pmu_perfmon_cntr_sz_v2(struct nvgpu_pmu *pmu)
{
	(void)pmu;
	return (u32)sizeof(struct pmu_perfmon_counter_v2);
}

static void *pmu_get_perfmon_cntr_ptr_v2(struct nvgpu_pmu *pmu)
{
	return (void *)(&pmu->pmu_perfmon->perfmon_counter_v2);
}

static void pmu_set_perfmon_cntr_ut_v2(struct nvgpu_pmu *pmu, u16 ut)
{
	pmu->pmu_perfmon->perfmon_counter_v2.upper_threshold = ut;
}

static void pmu_set_perfmon_cntr_lt_v2(struct nvgpu_pmu *pmu, u16 lt)
{
	pmu->pmu_perfmon->perfmon_counter_v2.lower_threshold = lt;
}

static void pmu_set_perfmon_cntr_valid_v2(struct nvgpu_pmu *pmu, u8 valid)
{
	pmu->pmu_perfmon->perfmon_counter_v2.valid = valid;
}

static void pmu_set_perfmon_cntr_index_v2(struct nvgpu_pmu *pmu, u8 index)
{
	pmu->pmu_perfmon->perfmon_counter_v2.index = index;
}

static void pmu_set_perfmon_cntr_group_id_v2(struct nvgpu_pmu *pmu, u8 gid)
{
	pmu->pmu_perfmon->perfmon_counter_v2.group_id = gid;
}

static void pmu_set_cmd_line_args_trace_dma_base_v4(struct nvgpu_pmu *pmu)
{
	pmu->fw->args_v4.dma_addr.dma_base =
		((u32)pmu->trace_buf.gpu_va)/0x100U;
	pmu->fw->args_v4.dma_addr.dma_base1 = 0;
	pmu->fw->args_v4.dma_addr.dma_offset = 0;
}

static u32 pmu_cmd_line_size_v4(struct nvgpu_pmu *pmu)
{
	(void)pmu;
	return (u32)sizeof(struct pmu_cmdline_args_v4);
}

static void pmu_set_cmd_line_args_cpu_freq_v4(struct nvgpu_pmu *pmu, u32 freq)
{
	pmu->fw->args_v4.cpu_freq_hz = freq;
}
static void pmu_set_cmd_line_args_secure_mode_v4(struct nvgpu_pmu *pmu, u8 val)
{
	pmu->fw->args_v4.secure_mode = val;
}

static void pmu_set_cmd_line_args_trace_size_v4(
			struct nvgpu_pmu *pmu, u32 size)
{
	pmu->fw->args_v4.falc_trace_size = size;
}
static void pmu_set_cmd_line_args_trace_dma_idx_v4(
			struct nvgpu_pmu *pmu, u32 idx)
{
	pmu->fw->args_v4.falc_trace_dma_idx = idx;
}

static u32 pmu_cmd_line_size_v6(struct nvgpu_pmu *pmu)
{
	(void)pmu;
	return (u32)sizeof(struct pmu_cmdline_args_v6);
}

static u32 pmu_cmd_line_size_v7(struct nvgpu_pmu *pmu)
{
	(void)pmu;
	return (u32)sizeof(struct pmu_cmdline_args_v7);
}

static void pmu_set_cmd_line_args_cpu_freq_v5(struct nvgpu_pmu *pmu, u32 freq)
{
	(void)freq;
	pmu->fw->args_v5.cpu_freq_hz = 204000000;
}
static void pmu_set_cmd_line_args_secure_mode_v5(struct nvgpu_pmu *pmu, u8 val)
{
	pmu->fw->args_v5.secure_mode = val;
}

static void pmu_set_cmd_line_args_trace_size_v5(
			struct nvgpu_pmu *pmu, u32 size)
{
	(void)pmu;
	(void)size;
	/* set by surface describe */
}

static void pmu_set_cmd_line_args_trace_dma_base_v5(struct nvgpu_pmu *pmu)
{
	struct gk20a *g = pmu->g;

	nvgpu_pmu_allocator_surface_describe(g, &pmu->trace_buf,
		&pmu->fw->args_v5.trace_buf);
}

static void config_cmd_line_args_super_surface_v6(struct nvgpu_pmu *pmu)
{
	struct gk20a *g = pmu->g;

	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_PMU_SUPER_SURFACE)) {
		nvgpu_pmu_allocator_surface_describe(g,
			nvgpu_pmu_super_surface_mem(g, pmu, pmu->super_surface),
			&pmu->fw->args_v6.super_surface);
	}
}

static void config_cmd_line_args_super_surface_v7(struct nvgpu_pmu *pmu)
{
	struct gk20a *g = pmu->g;

	if (nvgpu_is_enabled(g, NVGPU_SUPPORT_PMU_SUPER_SURFACE)) {
		nvgpu_pmu_allocator_surface_describe(g,
			nvgpu_pmu_super_surface_mem(g, pmu, pmu->super_surface),
			&pmu->fw->args_v7.super_surface);
	}
}

static void pmu_set_cmd_line_args_trace_dma_idx_v5(
			struct nvgpu_pmu *pmu, u32 idx)
{
	(void)pmu;
	(void)idx;
	/* set by surface describe */
}

static u32 pmu_cmd_line_size_v3(struct nvgpu_pmu *pmu)
{
	(void)pmu;
	return (u32)sizeof(struct pmu_cmdline_args_v3);
}

static void pmu_set_cmd_line_args_cpu_freq_v3(struct nvgpu_pmu *pmu, u32 freq)
{
	pmu->fw->args_v3.cpu_freq_hz = freq;
}
static void pmu_set_cmd_line_args_secure_mode_v3(struct nvgpu_pmu *pmu, u8 val)
{
	pmu->fw->args_v3.secure_mode = val;
}

static void pmu_set_cmd_line_args_trace_size_v3(
			struct nvgpu_pmu *pmu, u32 size)
{
	pmu->fw->args_v3.falc_trace_size = size;
}

static void pmu_set_cmd_line_args_trace_dma_base_v3(struct nvgpu_pmu *pmu)
{
	pmu->fw->args_v3.falc_trace_dma_base =
		((u32)pmu->trace_buf.gpu_va)/0x100U;
}

static void pmu_set_cmd_line_args_trace_dma_idx_v3(
			struct nvgpu_pmu *pmu, u32 idx)
{
	pmu->fw->args_v3.falc_trace_dma_idx = idx;
}

static void *pmu_get_cmd_line_args_ptr_v4(struct nvgpu_pmu *pmu)
{
	return (void *)(&pmu->fw->args_v4);
}

static void *pmu_get_cmd_line_args_ptr_v3(struct nvgpu_pmu *pmu)
{
	return (void *)(&pmu->fw->args_v3);
}

static void *pmu_get_cmd_line_args_ptr_v5(struct nvgpu_pmu *pmu)
{
	return (void *)(&pmu->fw->args_v5);
}

static u32 pmu_get_allocation_size_v3(struct nvgpu_pmu *pmu)
{
	(void)pmu;
	return (u32)sizeof(struct pmu_allocation_v3);
}

static u32 pmu_get_allocation_size_v2(struct nvgpu_pmu *pmu)
{
	(void)pmu;
	return (u32)sizeof(struct pmu_allocation_v2);
}

static u32 pmu_get_allocation_size_v1(struct nvgpu_pmu *pmu)
{
	(void)pmu;
	return (u32)sizeof(struct pmu_allocation_v1);
}

static void pmu_set_allocation_ptr_v3(struct nvgpu_pmu *pmu,
	void **pmu_alloc_ptr, void *assign_ptr)
{
	struct pmu_allocation_v3 **pmu_a_ptr =
		(struct pmu_allocation_v3 **)pmu_alloc_ptr;

	(void)pmu;
	*pmu_a_ptr = (struct pmu_allocation_v3 *)assign_ptr;
}

static void pmu_set_allocation_ptr_v2(struct nvgpu_pmu *pmu,
	void **pmu_alloc_ptr, void *assign_ptr)
{
	struct pmu_allocation_v2 **pmu_a_ptr =
		(struct pmu_allocation_v2 **)pmu_alloc_ptr;

	(void)pmu;
	*pmu_a_ptr = (struct pmu_allocation_v2 *)assign_ptr;
}

static void pmu_set_allocation_ptr_v1(struct nvgpu_pmu *pmu,
	void **pmu_alloc_ptr, void *assign_ptr)
{
	struct pmu_allocation_v1 **pmu_a_ptr =
		(struct pmu_allocation_v1 **)pmu_alloc_ptr;

	(void)pmu;
	*pmu_a_ptr = (struct pmu_allocation_v1 *)assign_ptr;
}

static void pmu_allocation_set_dmem_size_v3(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr, u16 size)
{
	struct pmu_allocation_v3 *pmu_a_ptr =
		(struct pmu_allocation_v3 *)pmu_alloc_ptr;

	(void)pmu;
	pmu_a_ptr->alloc.dmem.size = size;
}

static void pmu_allocation_set_dmem_size_v2(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr, u16 size)
{
	struct pmu_allocation_v2 *pmu_a_ptr =
		(struct pmu_allocation_v2 *)pmu_alloc_ptr;

	(void)pmu;
	pmu_a_ptr->alloc.dmem.size = size;
}

static void pmu_allocation_set_dmem_size_v1(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr, u16 size)
{
	struct pmu_allocation_v1 *pmu_a_ptr =
		(struct pmu_allocation_v1 *)pmu_alloc_ptr;

	(void)pmu;
	pmu_a_ptr->alloc.dmem.size = size;
}

static u16 pmu_allocation_get_dmem_size_v3(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v3 *pmu_a_ptr =
		(struct pmu_allocation_v3 *)pmu_alloc_ptr;

	(void)pmu;
	return pmu_a_ptr->alloc.dmem.size;
}

static u16 pmu_allocation_get_dmem_size_v2(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v2 *pmu_a_ptr =
		(struct pmu_allocation_v2 *)pmu_alloc_ptr;

	(void)pmu;
	return pmu_a_ptr->alloc.dmem.size;
}

static u16 pmu_allocation_get_dmem_size_v1(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v1 *pmu_a_ptr =
		(struct pmu_allocation_v1 *)pmu_alloc_ptr;

	(void)pmu;
	return pmu_a_ptr->alloc.dmem.size;
}

static u32 pmu_allocation_get_dmem_offset_v3(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v3 *pmu_a_ptr =
		(struct pmu_allocation_v3 *)pmu_alloc_ptr;

	(void)pmu;
	return pmu_a_ptr->alloc.dmem.offset;
}

static u32 pmu_allocation_get_dmem_offset_v2(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v2 *pmu_a_ptr =
		(struct pmu_allocation_v2 *)pmu_alloc_ptr;

	(void)pmu;
	return pmu_a_ptr->alloc.dmem.offset;
}

static u32 pmu_allocation_get_dmem_offset_v1(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v1 *pmu_a_ptr =
		(struct pmu_allocation_v1 *)pmu_alloc_ptr;

	(void)pmu;
	return pmu_a_ptr->alloc.dmem.offset;
}

static u32 *pmu_allocation_get_dmem_offset_addr_v3(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v3 *pmu_a_ptr =
		(struct pmu_allocation_v3 *)pmu_alloc_ptr;

	(void)pmu;
	return &pmu_a_ptr->alloc.dmem.offset;
}

static void *pmu_allocation_get_fb_addr_v3(
				struct nvgpu_pmu *pmu, void *pmu_alloc_ptr)
{
	struct pmu_allocation_v3 *pmu_a_ptr =
			(struct pmu_allocation_v3 *)pmu_alloc_ptr;

	(void)pmu;
	return (void *)&pmu_a_ptr->alloc.fb;
}

static u32 pmu_allocation_get_fb_size_v3(
				struct nvgpu_pmu *pmu, void *pmu_alloc_ptr)
{
	struct pmu_allocation_v3 *pmu_a_ptr =
			(struct pmu_allocation_v3 *)pmu_alloc_ptr;

	(void)pmu;
	return (u32)sizeof(pmu_a_ptr->alloc.fb);
}

static u32 *pmu_allocation_get_dmem_offset_addr_v2(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v2 *pmu_a_ptr =
		(struct pmu_allocation_v2 *)pmu_alloc_ptr;

	(void)pmu;
	return &pmu_a_ptr->alloc.dmem.offset;
}

static u32 *pmu_allocation_get_dmem_offset_addr_v1(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr)
{
	struct pmu_allocation_v1 *pmu_a_ptr =
		(struct pmu_allocation_v1 *)pmu_alloc_ptr;

	(void)pmu;
	return &pmu_a_ptr->alloc.dmem.offset;
}

static void pmu_allocation_set_dmem_offset_v3(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr, u32 offset)
{
	struct pmu_allocation_v3 *pmu_a_ptr =
		(struct pmu_allocation_v3 *)pmu_alloc_ptr;

	(void)pmu;
	pmu_a_ptr->alloc.dmem.offset = offset;
}

static void pmu_allocation_set_dmem_offset_v2(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr, u32 offset)
{
	struct pmu_allocation_v2 *pmu_a_ptr =
		(struct pmu_allocation_v2 *)pmu_alloc_ptr;

	(void)pmu;
	pmu_a_ptr->alloc.dmem.offset = offset;
}

static void pmu_allocation_set_dmem_offset_v1(struct nvgpu_pmu *pmu,
	void *pmu_alloc_ptr, u32 offset)
{
	struct pmu_allocation_v1 *pmu_a_ptr =
		(struct pmu_allocation_v1 *)pmu_alloc_ptr;

	(void)pmu;
	pmu_a_ptr->alloc.dmem.offset = offset;
}

static void *pmu_get_init_msg_ptr_v5(struct pmu_init_msg *init)
{
	return (void *)(&(init->pmu_init_v5));
}

static void *pmu_get_init_msg_ptr_v4(struct pmu_init_msg *init)
{
	return (void *)(&(init->pmu_init_v4));
}

static u16 pmu_get_init_msg_sw_mngd_area_off_v5(
	union pmu_init_msg_pmu *init_msg)
{
	struct pmu_nvgpu_rpc_struct_cmdmgmt_init *init =
		(struct pmu_nvgpu_rpc_struct_cmdmgmt_init *)(&init_msg->v5);

	return init->sw_managed_area_offset;
}

static u16 pmu_get_init_msg_sw_mngd_area_off_v4(
	union pmu_init_msg_pmu *init_msg)
{
	struct pmu_init_msg_pmu_v4 *init =
		(struct pmu_init_msg_pmu_v4 *)(&init_msg->v4);

	return init->sw_managed_area_offset;
}

static u16 pmu_get_init_msg_sw_mngd_area_size_v5(
	union pmu_init_msg_pmu *init_msg)
{
	struct pmu_nvgpu_rpc_struct_cmdmgmt_init *init =
		(struct pmu_nvgpu_rpc_struct_cmdmgmt_init *)(&init_msg->v5);

	return init->sw_managed_area_size;
}

static u16 pmu_get_init_msg_sw_mngd_area_size_v4(
	union pmu_init_msg_pmu *init_msg)
{
	struct pmu_init_msg_pmu_v4 *init =
		(struct pmu_init_msg_pmu_v4 *)(&init_msg->v4);

	return init->sw_managed_area_size;
}

static void *pmu_get_init_msg_ptr_v1(struct pmu_init_msg *init)
{
	return (void *)(&(init->pmu_init_v1));
}

static u16 pmu_get_init_msg_sw_mngd_area_off_v1(
	union pmu_init_msg_pmu *init_msg)
{
	struct pmu_init_msg_pmu_v1 *init =
		(struct pmu_init_msg_pmu_v1 *)(&init_msg->v1);

	return init->sw_managed_area_offset;
}

static u16 pmu_get_init_msg_sw_mngd_area_size_v1(
	union pmu_init_msg_pmu *init_msg)
{
	struct pmu_init_msg_pmu_v1 *init =
		(struct pmu_init_msg_pmu_v1 *)(&init_msg->v1);

	return init->sw_managed_area_size;
}

static u32 pmu_get_perfmon_cmd_start_size_v3(void)
{
	return (u32)sizeof(struct pmu_perfmon_cmd_start_v3);
}

static u32 pmu_get_perfmon_cmd_start_size_v2(void)
{
	return (u32)sizeof(struct pmu_perfmon_cmd_start_v2);
}

static u32 pmu_get_perfmon_cmd_start_size_v1(void)
{
	return (u32)sizeof(struct pmu_perfmon_cmd_start_v1);
}

static int pmu_get_perfmon_cmd_start_offset_of_var_v3(
	enum pmu_perfmon_cmd_start_fields field, u32 *offset)
{
	int status = 0;

	switch (field) {
	case COUNTER_ALLOC:
		*offset = (u32)offsetof(struct pmu_perfmon_cmd_start_v3,
				counter_alloc);
		break;

	default:
		status = -EINVAL;
		break;
	}

	return status;
}

static int pmu_get_perfmon_cmd_start_offset_of_var_v2(
	enum pmu_perfmon_cmd_start_fields field, u32 *offset)
{
	int status = 0;

	switch (field) {
	case COUNTER_ALLOC:
		*offset = (u32)offsetof(struct pmu_perfmon_cmd_start_v2,
				counter_alloc);
		break;

	default:
		status = -EINVAL;
		break;
	}

	return status;
}

static int pmu_get_perfmon_cmd_start_offset_of_var_v1(
	enum pmu_perfmon_cmd_start_fields field, u32 *offset)
{
	int status = 0;

	switch (field) {
	case COUNTER_ALLOC:
		*offset = (u32)offsetof(struct pmu_perfmon_cmd_start_v1,
				counter_alloc);
		break;

	default:
		status = -EINVAL;
		break;
	}

	return status;
}

static u32 pmu_get_perfmon_cmd_init_size_v3(void)
{
	return (u32)sizeof(struct pmu_perfmon_cmd_init_v3);
}

static u32 pmu_get_perfmon_cmd_init_size_v2(void)
{
	return (u32)sizeof(struct pmu_perfmon_cmd_init_v2);
}

static u32 pmu_get_perfmon_cmd_init_size_v1(void)
{
	return (u32)sizeof(struct pmu_perfmon_cmd_init_v1);
}

static int pmu_get_perfmon_cmd_init_offset_of_var_v3(
	enum pmu_perfmon_cmd_start_fields field, u32 *offset)
{
	int status = 0;

	switch (field) {
	case COUNTER_ALLOC:
		*offset = (u32)offsetof(struct pmu_perfmon_cmd_init_v3,
				counter_alloc);
		break;

	default:
		status = -EINVAL;
		break;
	}

	return status;
}

static int pmu_get_perfmon_cmd_init_offset_of_var_v2(
	enum pmu_perfmon_cmd_start_fields field, u32 *offset)
{
	int status = 0;

	switch (field) {
	case COUNTER_ALLOC:
		*offset = (u32)offsetof(struct pmu_perfmon_cmd_init_v2,
				counter_alloc);
		break;

	default:
		status = -EINVAL;
		break;
	}

	return status;
}

static int pmu_get_perfmon_cmd_init_offset_of_var_v1(
	enum pmu_perfmon_cmd_start_fields field, u32 *offset)
{
	int status = 0;

	switch (field) {
	case COUNTER_ALLOC:
		*offset = (u32)offsetof(struct pmu_perfmon_cmd_init_v1,
				counter_alloc);
		break;

	default:
		status = -EINVAL;
		break;
	}

	return status;
}

static void pmu_perfmon_start_set_cmd_type_v3(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_start_v3 *start = &pc->start_v3;

	start->cmd_type = value;
}

static void pmu_perfmon_start_set_cmd_type_v2(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_start_v2 *start = &pc->start_v2;

	start->cmd_type = value;
}

static void pmu_perfmon_start_set_cmd_type_v1(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_start_v1 *start = &pc->start_v1;

	start->cmd_type = value;
}

static void pmu_perfmon_start_set_group_id_v3(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_start_v3 *start = &pc->start_v3;

	start->group_id = value;
}

static void pmu_perfmon_start_set_group_id_v2(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_start_v2 *start = &pc->start_v2;

	start->group_id = value;
}

static void pmu_perfmon_start_set_group_id_v1(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_start_v1 *start = &pc->start_v1;

	start->group_id = value;
}

static void pmu_perfmon_start_set_state_id_v3(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_start_v3 *start = &pc->start_v3;

	start->state_id = value;
}

static void pmu_perfmon_start_set_state_id_v2(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_start_v2 *start = &pc->start_v2;

	start->state_id = value;
}

static void pmu_perfmon_start_set_state_id_v1(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_start_v1 *start = &pc->start_v1;

	start->state_id = value;
}

static void pmu_perfmon_start_set_flags_v3(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_start_v3 *start = &pc->start_v3;

	start->flags = value;
}

static void pmu_perfmon_start_set_flags_v2(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_start_v2 *start = &pc->start_v2;

	start->flags = value;
}

static void pmu_perfmon_start_set_flags_v1(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_start_v1 *start = &pc->start_v1;

	start->flags = value;
}

static u8 pmu_perfmon_start_get_flags_v3(struct pmu_perfmon_cmd *pc)
{
	struct pmu_perfmon_cmd_start_v3 *start = &pc->start_v3;

	return start->flags;
}

static u8 pmu_perfmon_start_get_flags_v2(struct pmu_perfmon_cmd *pc)
{
	struct pmu_perfmon_cmd_start_v2 *start = &pc->start_v2;

	return start->flags;
}

static u8 pmu_perfmon_start_get_flags_v1(struct pmu_perfmon_cmd *pc)
{
	struct pmu_perfmon_cmd_start_v1 *start = &pc->start_v1;

	return start->flags;
}

static void pmu_perfmon_cmd_init_set_sample_buffer_v3(
	struct pmu_perfmon_cmd *pc, u16 value)
{
	struct pmu_perfmon_cmd_init_v3 *init = &pc->init_v3;

	init->sample_buffer = value;
}

static void pmu_perfmon_cmd_init_set_sample_buffer_v2(
	struct pmu_perfmon_cmd *pc, u16 value)
{
	struct pmu_perfmon_cmd_init_v2 *init = &pc->init_v2;

	init->sample_buffer = value;
}


static void pmu_perfmon_cmd_init_set_sample_buffer_v1(
	struct pmu_perfmon_cmd *pc, u16 value)
{
	struct pmu_perfmon_cmd_init_v1 *init = &pc->init_v1;

	init->sample_buffer = value;
}

static void pmu_perfmon_cmd_init_set_dec_cnt_v3(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v3 *init = &pc->init_v3;

	init->to_decrease_count = value;
}

static void pmu_perfmon_cmd_init_set_dec_cnt_v2(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v2 *init = &pc->init_v2;

	init->to_decrease_count = value;
}

static void pmu_perfmon_cmd_init_set_dec_cnt_v1(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v1 *init = &pc->init_v1;

	init->to_decrease_count = value;
}

static void pmu_perfmon_cmd_init_set_base_cnt_id_v3(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v3 *init = &pc->init_v3;

	init->base_counter_id = value;
}

static void pmu_perfmon_cmd_init_set_base_cnt_id_v2(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v2 *init = &pc->init_v2;

	init->base_counter_id = value;
}

static void pmu_perfmon_cmd_init_set_base_cnt_id_v1(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v1 *init = &pc->init_v1;

	init->base_counter_id = value;
}

static void pmu_perfmon_cmd_init_set_samp_period_us_v3(
	struct pmu_perfmon_cmd *pc, u32 value)
{
	struct pmu_perfmon_cmd_init_v3 *init = &pc->init_v3;

	init->sample_period_us = value;
}

static void pmu_perfmon_cmd_init_set_samp_period_us_v2(
	struct pmu_perfmon_cmd *pc, u32 value)
{
	struct pmu_perfmon_cmd_init_v2 *init = &pc->init_v2;

	init->sample_period_us = value;
}

static void pmu_perfmon_cmd_init_set_samp_period_us_v1(
	struct pmu_perfmon_cmd *pc, u32 value)
{
	struct pmu_perfmon_cmd_init_v1 *init = &pc->init_v1;

	init->sample_period_us = value;
}

static void pmu_perfmon_cmd_init_set_num_cnt_v3(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v3 *init = &pc->init_v3;

	init->num_counters = value;
}

static void pmu_perfmon_cmd_init_set_num_cnt_v2(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v2 *init = &pc->init_v2;

	init->num_counters = value;
}

static void pmu_perfmon_cmd_init_set_num_cnt_v1(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v1 *init = &pc->init_v1;

	init->num_counters = value;
}

static void pmu_perfmon_cmd_init_set_mov_avg_v3(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v3 *init = &pc->init_v3;

	init->samples_in_moving_avg = value;
}

static void pmu_perfmon_cmd_init_set_mov_avg_v2(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v2 *init = &pc->init_v2;

	init->samples_in_moving_avg = value;
}

static void pmu_perfmon_cmd_init_set_mov_avg_v1(struct pmu_perfmon_cmd *pc,
	u8 value)
{
	struct pmu_perfmon_cmd_init_v1 *init = &pc->init_v1;

	init->samples_in_moving_avg = value;
}

static void pmu_get_init_msg_queue_params_v1(
	u32 id, void *init_msg, u32 *index, u32 *offset, u32 *size)
{
	struct pmu_init_msg_pmu_v1 *init =
		(struct pmu_init_msg_pmu_v1 *)init_msg;

	*index = init->queue_info[id].index;
	*offset = init->queue_info[id].offset;
	*size = init->queue_info[id].size;
}

static void pmu_get_init_msg_queue_params_v4(
	u32 id, void *init_msg, u32 *index, u32 *offset, u32 *size)
{
	struct pmu_init_msg_pmu_v4 *init = init_msg;
	u32 current_ptr = 0;
	u32 i;

	if (id == PMU_COMMAND_QUEUE_HPQ) {
		id = PMU_QUEUE_HPQ_IDX_FOR_V3;
	} else if (id == PMU_COMMAND_QUEUE_LPQ) {
		id = PMU_QUEUE_LPQ_IDX_FOR_V3;
	} else if (id == PMU_MESSAGE_QUEUE) {
		id = PMU_QUEUE_MSG_IDX_FOR_V3;
	} else {
		return;
	}

	*index = init->queue_index[id];
	*size = init->queue_size[id];
	if (id != 0U) {
		for (i = 0 ; i < id; i++) {
			current_ptr += init->queue_size[i];
		}
	}
	*offset = init->queue_offset + current_ptr;
}

static void *pmu_get_sequence_in_alloc_ptr_v3(struct pmu_sequence *seq)
{
	return (void *)(&seq->in_v3);
}

static void *pmu_get_sequence_in_alloc_ptr_v1(struct pmu_sequence *seq)
{
	return (void *)(&seq->in_v1);
}

static void *pmu_get_sequence_out_alloc_ptr_v3(struct pmu_sequence *seq)
{
	return (void *)(&seq->out_v3);
}

static void *pmu_get_sequence_out_alloc_ptr_v1(struct pmu_sequence *seq)
{
	return (void *)(&seq->out_v1);
}

static u8 pmu_pg_cmd_eng_buf_load_size_v0(struct pmu_pg_cmd *pg)
{
	size_t tmp_size = sizeof(pg->eng_buf_load_v0);

	nvgpu_assert(tmp_size <= (size_t)U8_MAX);
	return U8(tmp_size);
}

static u8 pmu_pg_cmd_eng_buf_load_size_v1(struct pmu_pg_cmd *pg)
{
	size_t tmp_size = sizeof(pg->eng_buf_load_v1);

	nvgpu_assert(tmp_size <= (size_t)U8_MAX);
	return U8(tmp_size);
}

static u8 pmu_pg_cmd_eng_buf_load_size_v2(struct pmu_pg_cmd *pg)
{
	size_t tmp_size = sizeof(pg->eng_buf_load_v2);

	nvgpu_assert(tmp_size <= (size_t)U8_MAX);
	return U8(tmp_size);
}

static void pmu_pg_cmd_eng_buf_load_set_cmd_type_v0(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v0.cmd_type = value;
}

static void pmu_pg_cmd_eng_buf_load_set_cmd_type_v1(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v1.cmd_type = value;
}

static void pmu_pg_cmd_eng_buf_load_set_cmd_type_v2(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v2.cmd_type = value;
}

static void pmu_pg_cmd_eng_buf_load_set_engine_id_v0(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v0.engine_id = value;
}
static void pmu_pg_cmd_eng_buf_load_set_engine_id_v1(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v1.engine_id = value;
}
static void pmu_pg_cmd_eng_buf_load_set_engine_id_v2(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v2.engine_id = value;
}
static void pmu_pg_cmd_eng_buf_load_set_buf_idx_v0(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v0.buf_idx = value;
}
static void pmu_pg_cmd_eng_buf_load_set_buf_idx_v1(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v1.buf_idx = value;
}
static void pmu_pg_cmd_eng_buf_load_set_buf_idx_v2(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v2.buf_idx = value;
}

static void pmu_pg_cmd_eng_buf_load_set_pad_v0(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v0.pad = value;
}
static void pmu_pg_cmd_eng_buf_load_set_pad_v1(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v1.pad = value;
}
static void pmu_pg_cmd_eng_buf_load_set_pad_v2(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v2.pad = value;
}

static void pmu_pg_cmd_eng_buf_load_set_buf_size_v0(struct pmu_pg_cmd *pg,
	u16 value)
{
	pg->eng_buf_load_v0.buf_size = value;
}
static void pmu_pg_cmd_eng_buf_load_set_buf_size_v1(struct pmu_pg_cmd *pg,
	u16 value)
{
	pg->eng_buf_load_v1.dma_desc.dma_size = value;
}
static void pmu_pg_cmd_eng_buf_load_set_buf_size_v2(struct pmu_pg_cmd *pg,
	u16 value)
{
	pg->eng_buf_load_v2.dma_desc.params = value;
}

static void pmu_pg_cmd_eng_buf_load_set_dma_base_v0(struct pmu_pg_cmd *pg,
	u32 value)
{
	pg->eng_buf_load_v0.dma_base = (value >> 8);
}
static void pmu_pg_cmd_eng_buf_load_set_dma_base_v1(struct pmu_pg_cmd *pg,
	u32 value)
{
	pg->eng_buf_load_v1.dma_desc.dma_addr.lo |= u64_lo32(value);
	pg->eng_buf_load_v1.dma_desc.dma_addr.hi |= u64_hi32(value);
}
static void pmu_pg_cmd_eng_buf_load_set_dma_base_v2(struct pmu_pg_cmd *pg,
	u32 value)
{
	pg->eng_buf_load_v2.dma_desc.address.lo = u64_lo32(value);
	pg->eng_buf_load_v2.dma_desc.address.hi = u64_lo32(value);
}

static void pmu_pg_cmd_eng_buf_load_set_dma_offset_v0(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v0.dma_offset = value;
}
static void pmu_pg_cmd_eng_buf_load_set_dma_offset_v1(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v1.dma_desc.dma_addr.lo |= value;
}
static void pmu_pg_cmd_eng_buf_load_set_dma_offset_v2(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v2.dma_desc.address.lo |= u64_lo32(value);
	pg->eng_buf_load_v2.dma_desc.address.hi |= u64_lo32(value);
}

static void pmu_pg_cmd_eng_buf_load_set_dma_idx_v0(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v0.dma_idx = value;
}

static void pmu_pg_cmd_eng_buf_load_set_dma_idx_v1(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v1.dma_desc.dma_idx = value;
}

static void pmu_pg_cmd_eng_buf_load_set_dma_idx_v2(struct pmu_pg_cmd *pg,
	u8 value)
{
	pg->eng_buf_load_v2.dma_desc.params |= (U32(value) << U32(24));
}

static int pmu_prepare_ns_ucode_blob(struct gk20a *g)
{
	struct nvgpu_pmu *pmu = g->pmu;
	struct mm_gk20a *mm = &g->mm;
	struct vm_gk20a *vm = mm->pmu.vm;
	struct pmu_ucode_desc *desc;
	struct pmu_rtos_fw *rtos_fw = pmu->fw;
	u32 *ucode_image = NULL;
	int err = 0;

	nvgpu_log_fn(g, " ");

	desc = (struct pmu_ucode_desc *)(void *)rtos_fw->fw_desc->data;
	ucode_image = (u32 *)(void *)rtos_fw->fw_image->data;

	if (!nvgpu_mem_is_valid(&rtos_fw->ucode)) {
		err = nvgpu_dma_alloc_map_sys(vm, PMU_RTOS_UCODE_SIZE_MAX,
				&rtos_fw->ucode);
		if (err != 0) {
			goto exit;
		}
	}

	nvgpu_mem_wr_n(g, &pmu->fw->ucode, 0, ucode_image,
		(desc->app_start_offset + desc->app_size));

exit:
	return err;
}

static int pmu_prepare_ns_ucode_blob_v1(struct gk20a *g)
{
	struct nvgpu_pmu *pmu = g->pmu;
	struct mm_gk20a *mm = &g->mm;
	struct vm_gk20a *vm = mm->pmu.vm;
	struct pmu_ucode_desc_v1 *desc;
	struct pmu_rtos_fw *rtos_fw = pmu->fw;
	u32 *ucode_image = NULL;
	int err = 0;

	nvgpu_log_fn(g, " ");

	ucode_image = (u32 *)(void *)rtos_fw->fw_image->data;

	if (nvgpu_is_enabled(g, NVGPU_PMU_NEXT_CORE_ENABLED)) {
		if (!nvgpu_mem_is_valid(&rtos_fw->ucode)) {
			err = nvgpu_dma_alloc_flags_sys(g,
					NVGPU_DMA_PHYSICALLY_ADDRESSED,
					PMU_RTOS_UCODE_SIZE_MAX,
					&rtos_fw->ucode);
			if (err != 0) {
				goto exit;
			}
		}

		nvgpu_mem_wr_n(g, &pmu->fw->ucode, 0, ucode_image,
				rtos_fw->fw_image->size);

#if defined(CONFIG_NVGPU_NON_FUSA)
		/* alloc boot args */
		err = nvgpu_pmu_next_core_rtos_args_allocate(g, pmu);
		if (err != 0) {
			goto exit;
		}
#endif
	} else {
		desc = (struct pmu_ucode_desc_v1 *)(void *)
					rtos_fw->fw_desc->data;

		if (!nvgpu_mem_is_valid(&rtos_fw->ucode)) {
			err = nvgpu_dma_alloc_map_sys(vm,
					PMU_RTOS_UCODE_SIZE_MAX,
					&rtos_fw->ucode);
			if (err != 0) {
				goto exit;
			}
		}

		nvgpu_mem_wr_n(g, &pmu->fw->ucode, 0, ucode_image,
				(desc->app_start_offset + desc->app_size));
	}

exit:
	return err;
}

int nvgpu_pmu_init_fw_ver_ops(struct gk20a *g,
	struct nvgpu_pmu *pmu, u32 app_version)
{
	struct pmu_fw_ver_ops *fw_ops = &pmu->fw->ops;
	int err = 0;

	nvgpu_log_fn(g, " ");

	switch (app_version) {
	case APP_VERSION_GP10B:
		fw_ops->pg_cmd_eng_buf_load_size =
			pmu_pg_cmd_eng_buf_load_size_v1;
		fw_ops->pg_cmd_eng_buf_load_set_cmd_type =
			pmu_pg_cmd_eng_buf_load_set_cmd_type_v1;
		fw_ops->pg_cmd_eng_buf_load_set_engine_id =
			pmu_pg_cmd_eng_buf_load_set_engine_id_v1;
		fw_ops->pg_cmd_eng_buf_load_set_buf_idx =
			pmu_pg_cmd_eng_buf_load_set_buf_idx_v1;
		fw_ops->pg_cmd_eng_buf_load_set_pad =
			pmu_pg_cmd_eng_buf_load_set_pad_v1;
		fw_ops->pg_cmd_eng_buf_load_set_buf_size =
			pmu_pg_cmd_eng_buf_load_set_buf_size_v1;
		fw_ops->pg_cmd_eng_buf_load_set_dma_base =
			pmu_pg_cmd_eng_buf_load_set_dma_base_v1;
		fw_ops->pg_cmd_eng_buf_load_set_dma_offset =
			pmu_pg_cmd_eng_buf_load_set_dma_offset_v1;
		fw_ops->pg_cmd_eng_buf_load_set_dma_idx =
			pmu_pg_cmd_eng_buf_load_set_dma_idx_v1;
		fw_ops->get_perfmon_cntr_ptr = pmu_get_perfmon_cntr_ptr_v2;
		fw_ops->set_perfmon_cntr_ut = pmu_set_perfmon_cntr_ut_v2;
		fw_ops->set_perfmon_cntr_lt = pmu_set_perfmon_cntr_lt_v2;
		fw_ops->set_perfmon_cntr_valid =
			pmu_set_perfmon_cntr_valid_v2;
		fw_ops->set_perfmon_cntr_index =
			pmu_set_perfmon_cntr_index_v2;
		fw_ops->set_perfmon_cntr_group_id =
			pmu_set_perfmon_cntr_group_id_v2;
		fw_ops->get_perfmon_cntr_sz = pmu_perfmon_cntr_sz_v2;
		g->pmu_ver_cmd_id_zbc_table_update = 16;
		nvgpu_set_enabled(g, NVGPU_PMU_ZBC_SAVE, false);
		fw_ops->get_cmd_line_args_size =
			pmu_cmd_line_size_v4;
		fw_ops->set_cmd_line_args_cpu_freq =
			pmu_set_cmd_line_args_cpu_freq_v4;
		fw_ops->set_cmd_line_args_secure_mode =
			pmu_set_cmd_line_args_secure_mode_v4;
		fw_ops->set_cmd_line_args_trace_size =
			pmu_set_cmd_line_args_trace_size_v4;
		fw_ops->set_cmd_line_args_trace_dma_base =
			pmu_set_cmd_line_args_trace_dma_base_v4;
		fw_ops->set_cmd_line_args_trace_dma_idx =
			pmu_set_cmd_line_args_trace_dma_idx_v4;
		fw_ops->get_cmd_line_args_ptr =
			pmu_get_cmd_line_args_ptr_v4;
		fw_ops->get_allocation_struct_size =
			pmu_get_allocation_size_v2;
		fw_ops->set_allocation_ptr =
			pmu_set_allocation_ptr_v2;
		fw_ops->allocation_set_dmem_size =
			pmu_allocation_set_dmem_size_v2;
		fw_ops->allocation_get_dmem_size =
			pmu_allocation_get_dmem_size_v2;
		fw_ops->allocation_get_dmem_offset =
			pmu_allocation_get_dmem_offset_v2;
		fw_ops->allocation_get_dmem_offset_addr =
			pmu_allocation_get_dmem_offset_addr_v2;
		fw_ops->allocation_set_dmem_offset =
			pmu_allocation_set_dmem_offset_v2;
		fw_ops->get_init_msg_queue_params =
			pmu_get_init_msg_queue_params_v1;
		fw_ops->get_init_msg_ptr =
			pmu_get_init_msg_ptr_v1;
		fw_ops->get_init_msg_sw_mngd_area_off =
			pmu_get_init_msg_sw_mngd_area_off_v1;
		fw_ops->get_init_msg_sw_mngd_area_size =
			pmu_get_init_msg_sw_mngd_area_size_v1;
		fw_ops->get_perfmon_cmd_start_size =
			pmu_get_perfmon_cmd_start_size_v2;
		fw_ops->get_perfmon_cmd_start_offset_of_var =
			pmu_get_perfmon_cmd_start_offset_of_var_v2;
		fw_ops->perfmon_start_set_cmd_type =
			pmu_perfmon_start_set_cmd_type_v2;
		fw_ops->perfmon_start_set_group_id =
			pmu_perfmon_start_set_group_id_v2;
		fw_ops->perfmon_start_set_state_id =
			pmu_perfmon_start_set_state_id_v2;
		fw_ops->perfmon_start_set_flags =
			pmu_perfmon_start_set_flags_v2;
		fw_ops->perfmon_start_get_flags =
			pmu_perfmon_start_get_flags_v2;
		fw_ops->get_perfmon_cmd_init_size =
			pmu_get_perfmon_cmd_init_size_v2;
		fw_ops->get_perfmon_cmd_init_offset_of_var =
			pmu_get_perfmon_cmd_init_offset_of_var_v2;
		fw_ops->perfmon_cmd_init_set_sample_buffer =
			pmu_perfmon_cmd_init_set_sample_buffer_v2;
		fw_ops->perfmon_cmd_init_set_dec_cnt =
			pmu_perfmon_cmd_init_set_dec_cnt_v2;
		fw_ops->perfmon_cmd_init_set_base_cnt_id =
			pmu_perfmon_cmd_init_set_base_cnt_id_v2;
		fw_ops->perfmon_cmd_init_set_samp_period_us =
			pmu_perfmon_cmd_init_set_samp_period_us_v2;
		fw_ops->perfmon_cmd_init_set_num_cnt =
			pmu_perfmon_cmd_init_set_num_cnt_v2;
		fw_ops->perfmon_cmd_init_set_mov_avg =
			pmu_perfmon_cmd_init_set_mov_avg_v2;
		fw_ops->get_seq_in_alloc_ptr =
			pmu_get_sequence_in_alloc_ptr_v1;
		fw_ops->get_seq_out_alloc_ptr =
			pmu_get_sequence_out_alloc_ptr_v1;
		fw_ops->prepare_ns_ucode_blob =
			pmu_prepare_ns_ucode_blob;
		break;
	case APP_VERSION_GV11B:
	case APP_VERSION_GV10X:
	case APP_VERSION_TU10X:
	case APP_VERSION_NVGPU_NEXT:
	case APP_VERSION_NVGPU_NEXT_CORE:
		fw_ops->pg_cmd_eng_buf_load_size =
			pmu_pg_cmd_eng_buf_load_size_v2;
		fw_ops->pg_cmd_eng_buf_load_set_cmd_type =
			pmu_pg_cmd_eng_buf_load_set_cmd_type_v2;
		fw_ops->pg_cmd_eng_buf_load_set_engine_id =
			pmu_pg_cmd_eng_buf_load_set_engine_id_v2;
		fw_ops->pg_cmd_eng_buf_load_set_buf_idx =
			pmu_pg_cmd_eng_buf_load_set_buf_idx_v2;
		fw_ops->pg_cmd_eng_buf_load_set_pad =
			pmu_pg_cmd_eng_buf_load_set_pad_v2;
		fw_ops->pg_cmd_eng_buf_load_set_buf_size =
			pmu_pg_cmd_eng_buf_load_set_buf_size_v2;
		fw_ops->pg_cmd_eng_buf_load_set_dma_base =
			pmu_pg_cmd_eng_buf_load_set_dma_base_v2;
		fw_ops->pg_cmd_eng_buf_load_set_dma_offset =
			pmu_pg_cmd_eng_buf_load_set_dma_offset_v2;
		fw_ops->pg_cmd_eng_buf_load_set_dma_idx =
			pmu_pg_cmd_eng_buf_load_set_dma_idx_v2;
		fw_ops->get_perfmon_cntr_ptr = pmu_get_perfmon_cntr_ptr_v2;
		fw_ops->set_perfmon_cntr_ut = pmu_set_perfmon_cntr_ut_v2;
		fw_ops->set_perfmon_cntr_lt = pmu_set_perfmon_cntr_lt_v2;
		fw_ops->set_perfmon_cntr_valid =
			pmu_set_perfmon_cntr_valid_v2;
		fw_ops->set_perfmon_cntr_index =
			pmu_set_perfmon_cntr_index_v2;
		fw_ops->set_perfmon_cntr_group_id =
			pmu_set_perfmon_cntr_group_id_v2;
		fw_ops->get_perfmon_cntr_sz = pmu_perfmon_cntr_sz_v2;
		g->pmu_ver_cmd_id_zbc_table_update = 16;
		nvgpu_set_enabled(g, NVGPU_PMU_ZBC_SAVE, false);
		fw_ops->get_cmd_line_args_size =
			pmu_cmd_line_size_v6;
		fw_ops->set_cmd_line_args_cpu_freq =
			pmu_set_cmd_line_args_cpu_freq_v5;
		fw_ops->set_cmd_line_args_secure_mode =
			pmu_set_cmd_line_args_secure_mode_v5;
		fw_ops->set_cmd_line_args_trace_size =
			pmu_set_cmd_line_args_trace_size_v5;
		fw_ops->set_cmd_line_args_trace_dma_base =
			pmu_set_cmd_line_args_trace_dma_base_v5;
		fw_ops->set_cmd_line_args_trace_dma_idx =
			pmu_set_cmd_line_args_trace_dma_idx_v5;
		fw_ops->config_cmd_line_args_super_surface =
			config_cmd_line_args_super_surface_v6;
		fw_ops->get_cmd_line_args_ptr =
			pmu_get_cmd_line_args_ptr_v5;
		fw_ops->get_allocation_struct_size =
			pmu_get_allocation_size_v3;
		fw_ops->set_allocation_ptr =
			pmu_set_allocation_ptr_v3;
		fw_ops->allocation_set_dmem_size =
			pmu_allocation_set_dmem_size_v3;
		fw_ops->allocation_get_dmem_size =
			pmu_allocation_get_dmem_size_v3;
		fw_ops->allocation_get_dmem_offset =
			pmu_allocation_get_dmem_offset_v3;
		fw_ops->allocation_get_dmem_offset_addr =
			pmu_allocation_get_dmem_offset_addr_v3;
		fw_ops->allocation_set_dmem_offset =
			pmu_allocation_set_dmem_offset_v3;
		fw_ops->allocation_get_fb_addr =
			pmu_allocation_get_fb_addr_v3;
		fw_ops->allocation_get_fb_size =
			pmu_allocation_get_fb_size_v3;
		if (app_version == APP_VERSION_GV10X ||
			app_version == APP_VERSION_TU10X ||
			app_version == APP_VERSION_NVGPU_NEXT ||
			app_version == APP_VERSION_NVGPU_NEXT_CORE) {
			fw_ops->get_init_msg_ptr =
				pmu_get_init_msg_ptr_v5;
			fw_ops->get_init_msg_sw_mngd_area_off =
				pmu_get_init_msg_sw_mngd_area_off_v5;
			fw_ops->get_init_msg_sw_mngd_area_size =
				pmu_get_init_msg_sw_mngd_area_size_v5;
			fw_ops->clk.clk_set_boot_clk = NULL;
		} else {
			fw_ops->get_init_msg_queue_params =
				pmu_get_init_msg_queue_params_v4;
			fw_ops->get_init_msg_ptr =
				pmu_get_init_msg_ptr_v4;
			fw_ops->get_init_msg_sw_mngd_area_off =
				pmu_get_init_msg_sw_mngd_area_off_v4;
			fw_ops->get_init_msg_sw_mngd_area_size =
				pmu_get_init_msg_sw_mngd_area_size_v4;
		}
		fw_ops->get_perfmon_cmd_start_size =
			pmu_get_perfmon_cmd_start_size_v3;
		fw_ops->get_perfmon_cmd_start_offset_of_var =
			pmu_get_perfmon_cmd_start_offset_of_var_v3;
		fw_ops->perfmon_start_set_cmd_type =
			pmu_perfmon_start_set_cmd_type_v3;
		fw_ops->perfmon_start_set_group_id =
			pmu_perfmon_start_set_group_id_v3;
		fw_ops->perfmon_start_set_state_id =
			pmu_perfmon_start_set_state_id_v3;
		fw_ops->perfmon_start_set_flags =
			pmu_perfmon_start_set_flags_v3;
		fw_ops->perfmon_start_get_flags =
			pmu_perfmon_start_get_flags_v3;
		fw_ops->get_perfmon_cmd_init_size =
			pmu_get_perfmon_cmd_init_size_v3;
		fw_ops->get_perfmon_cmd_init_offset_of_var =
			pmu_get_perfmon_cmd_init_offset_of_var_v3;
		fw_ops->perfmon_cmd_init_set_sample_buffer =
			pmu_perfmon_cmd_init_set_sample_buffer_v3;
		fw_ops->perfmon_cmd_init_set_dec_cnt =
			pmu_perfmon_cmd_init_set_dec_cnt_v3;
		fw_ops->perfmon_cmd_init_set_base_cnt_id =
			pmu_perfmon_cmd_init_set_base_cnt_id_v3;
		fw_ops->perfmon_cmd_init_set_samp_period_us =
			pmu_perfmon_cmd_init_set_samp_period_us_v3;
		fw_ops->perfmon_cmd_init_set_num_cnt =
			pmu_perfmon_cmd_init_set_num_cnt_v3;
		fw_ops->perfmon_cmd_init_set_mov_avg =
			pmu_perfmon_cmd_init_set_mov_avg_v3;
		fw_ops->get_seq_in_alloc_ptr =
			pmu_get_sequence_in_alloc_ptr_v3;
		fw_ops->get_seq_out_alloc_ptr =
			pmu_get_sequence_out_alloc_ptr_v3;
		if (app_version == APP_VERSION_NVGPU_NEXT ||
			app_version == APP_VERSION_NVGPU_NEXT_CORE) {
			fw_ops->prepare_ns_ucode_blob =
				pmu_prepare_ns_ucode_blob_v1;
			fw_ops->get_cmd_line_args_size =
				pmu_cmd_line_size_v7;
			fw_ops->config_cmd_line_args_super_surface =
				config_cmd_line_args_super_surface_v7;
		} else {
			fw_ops->prepare_ns_ucode_blob =
				pmu_prepare_ns_ucode_blob;
		}
		break;
	case APP_VERSION_GM20B:
		fw_ops->pg_cmd_eng_buf_load_size =
			pmu_pg_cmd_eng_buf_load_size_v0;
		fw_ops->pg_cmd_eng_buf_load_set_cmd_type =
			pmu_pg_cmd_eng_buf_load_set_cmd_type_v0;
		fw_ops->pg_cmd_eng_buf_load_set_engine_id =
			pmu_pg_cmd_eng_buf_load_set_engine_id_v0;
		fw_ops->pg_cmd_eng_buf_load_set_buf_idx =
			pmu_pg_cmd_eng_buf_load_set_buf_idx_v0;
		fw_ops->pg_cmd_eng_buf_load_set_pad =
			pmu_pg_cmd_eng_buf_load_set_pad_v0;
		fw_ops->pg_cmd_eng_buf_load_set_buf_size =
			pmu_pg_cmd_eng_buf_load_set_buf_size_v0;
		fw_ops->pg_cmd_eng_buf_load_set_dma_base =
			pmu_pg_cmd_eng_buf_load_set_dma_base_v0;
		fw_ops->pg_cmd_eng_buf_load_set_dma_offset =
			pmu_pg_cmd_eng_buf_load_set_dma_offset_v0;
		fw_ops->pg_cmd_eng_buf_load_set_dma_idx =
			pmu_pg_cmd_eng_buf_load_set_dma_idx_v0;
		fw_ops->get_perfmon_cntr_ptr = pmu_get_perfmon_cntr_ptr_v2;
		fw_ops->set_perfmon_cntr_ut = pmu_set_perfmon_cntr_ut_v2;
		fw_ops->set_perfmon_cntr_lt = pmu_set_perfmon_cntr_lt_v2;
		fw_ops->set_perfmon_cntr_valid =
			pmu_set_perfmon_cntr_valid_v2;
		fw_ops->set_perfmon_cntr_index =
			pmu_set_perfmon_cntr_index_v2;
		fw_ops->set_perfmon_cntr_group_id =
			pmu_set_perfmon_cntr_group_id_v2;
		fw_ops->get_perfmon_cntr_sz = pmu_perfmon_cntr_sz_v2;
		g->pmu_ver_cmd_id_zbc_table_update = 16;
		nvgpu_set_enabled(g, NVGPU_PMU_ZBC_SAVE, true);
		fw_ops->get_cmd_line_args_size =
			pmu_cmd_line_size_v3;
		fw_ops->set_cmd_line_args_cpu_freq =
			pmu_set_cmd_line_args_cpu_freq_v3;
		fw_ops->set_cmd_line_args_secure_mode =
			pmu_set_cmd_line_args_secure_mode_v3;
		fw_ops->set_cmd_line_args_trace_size =
			pmu_set_cmd_line_args_trace_size_v3;
		fw_ops->set_cmd_line_args_trace_dma_base =
			pmu_set_cmd_line_args_trace_dma_base_v3;
		fw_ops->set_cmd_line_args_trace_dma_idx =
			pmu_set_cmd_line_args_trace_dma_idx_v3;
		fw_ops->get_cmd_line_args_ptr =
			pmu_get_cmd_line_args_ptr_v3;
		fw_ops->get_allocation_struct_size =
			pmu_get_allocation_size_v1;
		fw_ops->set_allocation_ptr =
			pmu_set_allocation_ptr_v1;
		fw_ops->allocation_set_dmem_size =
			pmu_allocation_set_dmem_size_v1;
		fw_ops->allocation_get_dmem_size =
			pmu_allocation_get_dmem_size_v1;
		fw_ops->allocation_get_dmem_offset =
			pmu_allocation_get_dmem_offset_v1;
		fw_ops->allocation_get_dmem_offset_addr =
			pmu_allocation_get_dmem_offset_addr_v1;
		fw_ops->allocation_set_dmem_offset =
			pmu_allocation_set_dmem_offset_v1;
		fw_ops->get_init_msg_queue_params =
			pmu_get_init_msg_queue_params_v1;
		fw_ops->get_init_msg_ptr =
			pmu_get_init_msg_ptr_v1;
		fw_ops->get_init_msg_sw_mngd_area_off =
			pmu_get_init_msg_sw_mngd_area_off_v1;
		fw_ops->get_init_msg_sw_mngd_area_size =
			pmu_get_init_msg_sw_mngd_area_size_v1;
		fw_ops->get_perfmon_cmd_start_size =
			pmu_get_perfmon_cmd_start_size_v1;
		fw_ops->get_perfmon_cmd_start_offset_of_var =
			pmu_get_perfmon_cmd_start_offset_of_var_v1;
		fw_ops->perfmon_start_set_cmd_type =
			pmu_perfmon_start_set_cmd_type_v1;
		fw_ops->perfmon_start_set_group_id =
			pmu_perfmon_start_set_group_id_v1;
		fw_ops->perfmon_start_set_state_id =
			pmu_perfmon_start_set_state_id_v1;
		fw_ops->perfmon_start_set_flags =
			pmu_perfmon_start_set_flags_v1;
		fw_ops->perfmon_start_get_flags =
			pmu_perfmon_start_get_flags_v1;
		fw_ops->get_perfmon_cmd_init_size =
			pmu_get_perfmon_cmd_init_size_v1;
		fw_ops->get_perfmon_cmd_init_offset_of_var =
			pmu_get_perfmon_cmd_init_offset_of_var_v1;
		fw_ops->perfmon_cmd_init_set_sample_buffer =
			pmu_perfmon_cmd_init_set_sample_buffer_v1;
		fw_ops->perfmon_cmd_init_set_dec_cnt =
			pmu_perfmon_cmd_init_set_dec_cnt_v1;
		fw_ops->perfmon_cmd_init_set_base_cnt_id =
			pmu_perfmon_cmd_init_set_base_cnt_id_v1;
		fw_ops->perfmon_cmd_init_set_samp_period_us =
			pmu_perfmon_cmd_init_set_samp_period_us_v1;
		fw_ops->perfmon_cmd_init_set_num_cnt =
			pmu_perfmon_cmd_init_set_num_cnt_v1;
		fw_ops->perfmon_cmd_init_set_mov_avg =
			pmu_perfmon_cmd_init_set_mov_avg_v1;
		fw_ops->get_seq_in_alloc_ptr =
			pmu_get_sequence_in_alloc_ptr_v1;
		fw_ops->get_seq_out_alloc_ptr =
			pmu_get_sequence_out_alloc_ptr_v1;
		fw_ops->prepare_ns_ucode_blob =
			pmu_prepare_ns_ucode_blob;
		break;
	default:
		nvgpu_err(g, "PMU code version not supported version: %d\n",
			app_version);
		err = -EINVAL;
		break;
	}

	fw_ops->set_perfmon_cntr_index(pmu, 3); /* GR & CE2 */
	fw_ops->set_perfmon_cntr_group_id(pmu, PMU_DOMAIN_GROUP_PSTATE);

	return err;
}

