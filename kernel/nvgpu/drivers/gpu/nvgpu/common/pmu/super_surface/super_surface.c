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

#include <nvgpu/pmu.h>
#include <nvgpu/dma.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/string.h>
#include <nvgpu/pmu/super_surface.h>

#include "super_surface_priv.h"

int nvgpu_pmu_super_surface_buf_alloc(struct gk20a *g, struct nvgpu_pmu *pmu,
		struct nvgpu_pmu_super_surface *ss)
{
	struct vm_gk20a *vm = g->mm.pmu.vm;
	int err = 0;
	u32 tmp = 0;

	nvgpu_log_fn(g, " ");

	if (ss == NULL) {
		return 0;
	}

	if (nvgpu_mem_is_valid(&ss->super_surface_buf)) {
		/* skip alloc/reinit for unrailgate sequence */
		return 0;
	}

	err = nvgpu_dma_alloc_map(vm, sizeof(struct super_surface),
		&ss->super_surface_buf);
	if (err != 0) {
		nvgpu_err(g, "failed to allocate pmu suffer surface\n");
		return err;
	}

	/* store the gpu_va in super-surface header for PMU ucode to access */
	tmp = u64_lo32(ss->super_surface_buf.gpu_va);
	nvgpu_mem_wr_n(g, nvgpu_pmu_super_surface_mem(g,
		pmu, pmu->super_surface),
		(u64)offsetof(struct super_surface, hdr.data.address.lo),
		&tmp, sizeof(u32));

	tmp = u64_hi32(ss->super_surface_buf.gpu_va);
	nvgpu_mem_wr_n(g, nvgpu_pmu_super_surface_mem(g,
		pmu, pmu->super_surface),
		(u64)offsetof(struct super_surface, hdr.data.address.hi),
		&tmp, sizeof(u32));

	return err;
}

struct nvgpu_mem *nvgpu_pmu_super_surface_mem(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_super_surface *ss)
{
	(void)g;
	(void)pmu;
	return &ss->super_surface_buf;
}

/*
 * Lookup table to hold info about super surface member,
 * here member ID from nv_pmu_super_surface_member_descriptor
 * used as a index to store the member info in two different
 * table, i.e one table is for SET ID TYPE & second table for
 * GET_STATUS ID_TYPE.
 */
int nvgpu_pmu_ss_create_ssmd_lookup_table(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_super_surface *ss)
{
	struct super_surface_member_descriptor ssmd;
	u32 ssmd_size = (u32)
		sizeof(struct super_surface_member_descriptor);
	u32 idx = 0U;
	int err = 0;

	(void)pmu;

	nvgpu_log_fn(g, " ");

	if (ss == NULL) {
		nvgpu_err(g, "SS not allocated");
		return -ENOMEM;
	}

	for (idx = 0U; idx < NV_PMU_SUPER_SURFACE_MEMBER_DESCRIPTOR_COUNT;
		idx++) {
		(void) memset(&ssmd, 0x0, ssmd_size);

		nvgpu_mem_rd_n(g, &ss->super_surface_buf, idx * ssmd_size,
			&ssmd, ssmd_size);

		nvgpu_pmu_dbg(g, "ssmd: id-0x%x offset-0x%x size-%x rsvd-0x%x",
			ssmd.id, ssmd.offset, ssmd.size, ssmd.rsvd);

		/* Check member type from ID member & update respective table*/
		if ((ssmd.id &
			NV_RM_PMU_SUPER_SURFACE_MEMBER_ID_TYPE_SET) != 0U) {
			/*
			 * clear member type from member ID as we create
			 * different table for each type & use ID as index
			 * during member info fetch.
			 */
			ssmd.id &= 0xFFFFU;
			if (ssmd.id >= NV_PMU_SUPER_SURFACE_MEMBER_COUNT) {
				nvgpu_err(g, "incorrect ssmd id %d", ssmd.id);
				nvgpu_err(g, "Failed to create SSMD table");
				err = -EINVAL;
				break;
			}
			/*use member ID as index for lookup table too*/
			(void) memcpy(&ss->ssmd_set[ssmd.id], &ssmd,
				ssmd_size);
		} else if ((ssmd.id &
			NV_RM_PMU_SUPER_SURFACE_MEMBER_ID_TYPE_GET_STATUS)
			!= 0U) {
			/*
			 * clear member type from member ID as we create
			 * different table for each type & use ID as index
			 * during member info fetch.
			 */
			ssmd.id &= 0xFFFFU;
			if (ssmd.id >= NV_PMU_SUPER_SURFACE_MEMBER_COUNT) {
				nvgpu_err(g, "incorrect ssmd id %d", ssmd.id);
				nvgpu_err(g, "failed to create SSMD table");
				err = -EINVAL;
				break;
			}
			/*use member ID as index for lookup table too*/
			(void) memcpy(&ss->ssmd_get_status[ssmd.id], &ssmd,
				ssmd_size);
		} else {
			continue;
		}
	}

	return err;
}

u32 nvgpu_pmu_get_ss_member_set_offset(struct gk20a *g,
	struct nvgpu_pmu *pmu, u32 member_id)
{
	(void)g;
	return pmu->super_surface->ssmd_set[member_id].offset;
}

u32 nvgpu_pmu_get_ss_member_set_size(struct gk20a *g,
	struct nvgpu_pmu *pmu, u32 member_id)
{
	(void)g;
	return pmu->super_surface->ssmd_set[member_id].size;
}

u32 nvgpu_pmu_get_ss_member_get_status_offset(struct gk20a *g,
	struct nvgpu_pmu *pmu, u32 member_id)
{
	(void)g;
	return pmu->super_surface->ssmd_get_status[member_id].offset;
}

u32 nvgpu_pmu_get_ss_member_get_status_size(struct gk20a *g,
	struct nvgpu_pmu *pmu, u32 member_id)
{
	(void)g;
	return pmu->super_surface->ssmd_get_status[member_id].size;
}

u32 nvgpu_pmu_get_ss_cmd_fbq_offset(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_super_surface *ss, u32 id)
{
	(void)g;
	(void)pmu;
	(void)ss;
	return (u32)offsetof(struct super_surface,
			fbq.cmd_queues.queue[id]);
}

u32 nvgpu_pmu_get_ss_msg_fbq_offset(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_super_surface *ss)
{
	(void)g;
	(void)pmu;
	(void)ss;
	return (u32)offsetof(struct super_surface,
			fbq.msg_queue);
}

u32 nvgpu_pmu_get_ss_msg_fbq_element_offset(struct gk20a *g,
	struct nvgpu_pmu *pmu, struct nvgpu_pmu_super_surface *ss, u32 idx)
{
	(void)g;
	(void)pmu;
	(void)ss;
	return (u32)offsetof(struct super_surface,
		fbq.msg_queue.element[idx]);
}

void nvgpu_pmu_ss_fbq_flush(struct gk20a *g, struct nvgpu_pmu *pmu)
{
	nvgpu_memset(g, nvgpu_pmu_super_surface_mem(g,
			pmu, pmu->super_surface),
			(u64)offsetof(struct super_surface, fbq.cmd_queues),
			0x00, sizeof(struct nv_pmu_fbq_cmd_queues));

	nvgpu_memset(g, nvgpu_pmu_super_surface_mem(g,
			pmu, pmu->super_surface),
			(u64)offsetof(struct super_surface, fbq.msg_queue),
			0x00, sizeof(struct nv_pmu_fbq_msg_queue));
}

void nvgpu_pmu_super_surface_deinit(struct gk20a *g, struct nvgpu_pmu *pmu,
	struct nvgpu_pmu_super_surface *ss)
{
	(void)pmu;

	nvgpu_log_fn(g, " ");

	if (ss == NULL) {
		return;
	}

	if (nvgpu_mem_is_valid(&ss->super_surface_buf)) {
		nvgpu_dma_free(g, &ss->super_surface_buf);
	}

	nvgpu_kfree(g, ss);
}

int nvgpu_pmu_super_surface_init(struct gk20a *g, struct nvgpu_pmu *pmu,
	struct nvgpu_pmu_super_surface **super_surface)
{
	(void)pmu;

	if (*super_surface != NULL) {
		/* skip alloc/reinit for unrailgate sequence */
		return 0;
	}

	*super_surface = (struct nvgpu_pmu_super_surface *) nvgpu_kzalloc(g,
		sizeof(struct nvgpu_pmu_super_surface));
	if (*super_surface == NULL) {
		return -ENOMEM;
	}

	return 0;
}
