/*
 * Copyright (c) 2017-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_PMU_FW_H
#define NVGPU_PMU_FW_H

#include <nvgpu/types.h>
#include <nvgpu/flcnif_cmn.h>
#include <nvgpu/pmu/pmuif/nvgpu_cmdif.h>

struct nvgpu_pmu;
struct pmu_sequence;
struct nvgpu_pmu_super_surface;
struct pmu_pg_cmd;
struct boardobjgrp;
struct boardobjgrp_pmu_cmd;
struct boardobjgrpmask;

#define PMU_RTOS_UCODE_SIZE_MAX	(256U * 1024U)

#define PMU_RTOS_TRACE_BUFSIZE		0x4000U    /* 4K */

/* Choices for pmu_state */
#define PMU_FW_STATE_OFF			0U /* PMU is off */
#define PMU_FW_STATE_STARTING		1U /* PMU is on, but not booted */
#define PMU_FW_STATE_INIT_RECEIVED	2U /* PMU init message received */
#define PMU_FW_STATE_ELPG_BOOTING	3U /* PMU is booting */
#define PMU_FW_STATE_ELPG_BOOTED	4U /* ELPG is initialized */
#define PMU_FW_STATE_LOADING_PG_BUF	5U /* Loading PG buf */
#define PMU_FW_STATE_LOADING_ZBC	6U /* Loading ZBC buf */
#define PMU_FW_STATE_STARTED		7U /* Fully unitialized */
#define PMU_FW_STATE_EXIT			8U /* Exit PMU state machine */

struct pmu_fw_ver_ops {
	u32 (*get_cmd_line_args_size)(struct nvgpu_pmu *pmu);
	void (*set_cmd_line_args_cpu_freq)(struct nvgpu_pmu *pmu,
		u32 freq);
	void (*set_cmd_line_args_trace_size)(struct nvgpu_pmu *pmu,
		u32 size);
	void (*set_cmd_line_args_trace_dma_base)(
		struct nvgpu_pmu *pmu);
	void (*config_cmd_line_args_super_surface)(
		struct nvgpu_pmu *pmu);
	void (*set_cmd_line_args_trace_dma_idx)(
		struct nvgpu_pmu *pmu, u32 idx);
	void * (*get_cmd_line_args_ptr)(struct nvgpu_pmu *pmu);
	void (*set_cmd_line_args_secure_mode)(struct nvgpu_pmu *pmu,
		u8 val);
	u32 (*get_allocation_struct_size)(struct nvgpu_pmu *pmu);
	void (*set_allocation_ptr)(struct nvgpu_pmu *pmu,
		void **pmu_alloc_ptr, void *assign_ptr);
	void (*allocation_set_dmem_size)(struct nvgpu_pmu *pmu,
		void *pmu_alloc_ptr, u16 size);
	u16 (*allocation_get_dmem_size)(struct nvgpu_pmu *pmu,
		void *pmu_alloc_ptr);
	u32 (*allocation_get_dmem_offset)(struct nvgpu_pmu *pmu,
		void *pmu_alloc_ptr);
	u32 * (*allocation_get_dmem_offset_addr)(
		struct nvgpu_pmu *pmu, void *pmu_alloc_ptr);
	void (*allocation_set_dmem_offset)(struct nvgpu_pmu *pmu,
		void *pmu_alloc_ptr, u32 offset);
	void * (*allocation_get_fb_addr)(struct nvgpu_pmu *pmu,
		void *pmu_alloc_ptr);
	u32 (*allocation_get_fb_size)(struct nvgpu_pmu *pmu,
		void *pmu_alloc_ptr);
	void (*get_init_msg_queue_params)(u32 id, void *init_msg,
		u32 *index, u32 *offset, u32 *size);
	void *(*get_init_msg_ptr)(struct pmu_init_msg *init);
	u16 (*get_init_msg_sw_mngd_area_off)(
		union pmu_init_msg_pmu *init_msg);
	u16 (*get_init_msg_sw_mngd_area_size)(
		union pmu_init_msg_pmu *init_msg);
	u32 (*get_perfmon_cmd_start_size)(void);
	int (*get_perfmon_cmd_start_offset_of_var)(
		enum pmu_perfmon_cmd_start_fields field,
		u32 *offset);
	void (*perfmon_start_set_cmd_type)(struct pmu_perfmon_cmd *pc,
		u8 value);
	void (*perfmon_start_set_group_id)(struct pmu_perfmon_cmd *pc,
		u8 value);
	void (*perfmon_start_set_state_id)(struct pmu_perfmon_cmd *pc,
		u8 value);
	void (*perfmon_start_set_flags)(struct pmu_perfmon_cmd *pc,
		u8 value);
	u8 (*perfmon_start_get_flags)(struct pmu_perfmon_cmd *pc);
	u32 (*get_perfmon_cmd_init_size)(void);
	int (*get_perfmon_cmd_init_offset_of_var)(
		enum pmu_perfmon_cmd_start_fields field,
		u32 *offset);
	void (*perfmon_cmd_init_set_sample_buffer)(
		struct pmu_perfmon_cmd *pc, u16 value);
	void (*perfmon_cmd_init_set_dec_cnt)(
		struct pmu_perfmon_cmd *pc, u8 value);
	void (*perfmon_cmd_init_set_base_cnt_id)(
		struct pmu_perfmon_cmd *pc, u8 value);
	void (*perfmon_cmd_init_set_samp_period_us)(
		struct pmu_perfmon_cmd *pc, u32 value);
	void (*perfmon_cmd_init_set_num_cnt)(struct pmu_perfmon_cmd *pc,
		u8 value);
	void (*perfmon_cmd_init_set_mov_avg)(struct pmu_perfmon_cmd *pc,
		u8 value);
	void *(*get_seq_in_alloc_ptr)(struct pmu_sequence *seq);
	void *(*get_seq_out_alloc_ptr)(struct pmu_sequence *seq);

	u32 (*get_perfmon_cntr_sz)(struct nvgpu_pmu *pmu);
	void* (*get_perfmon_cntr_ptr)(struct nvgpu_pmu *pmu);
	void (*set_perfmon_cntr_ut)(struct nvgpu_pmu *pmu, u16 ut);
	void (*set_perfmon_cntr_lt)(struct nvgpu_pmu *pmu, u16 lt);
	void (*set_perfmon_cntr_valid)(struct nvgpu_pmu *pmu, u8 val);
	void (*set_perfmon_cntr_index)(struct nvgpu_pmu *pmu, u8 val);
	void (*set_perfmon_cntr_group_id)(struct nvgpu_pmu *pmu,
		u8 gid);

	u8 (*pg_cmd_eng_buf_load_size)(struct pmu_pg_cmd *pg);
	void (*pg_cmd_eng_buf_load_set_cmd_type)(struct pmu_pg_cmd *pg,
		u8 value);
	void (*pg_cmd_eng_buf_load_set_engine_id)(struct pmu_pg_cmd *pg,
		u8 value);
	void (*pg_cmd_eng_buf_load_set_buf_idx)(struct pmu_pg_cmd *pg,
		u8 value);
	void (*pg_cmd_eng_buf_load_set_pad)(struct pmu_pg_cmd *pg,
		u8 value);
	void (*pg_cmd_eng_buf_load_set_buf_size)(struct pmu_pg_cmd *pg,
		u16 value);
	void (*pg_cmd_eng_buf_load_set_dma_base)(struct pmu_pg_cmd *pg,
		u32 value);
	void (*pg_cmd_eng_buf_load_set_dma_offset)(struct pmu_pg_cmd *pg,
		u8 value);
	void (*pg_cmd_eng_buf_load_set_dma_idx)(struct pmu_pg_cmd *pg,
		u8 value);
	struct {
		int (*boardobjgrp_pmucmd_construct_impl)
			(struct gk20a *g,
			struct boardobjgrp *pboardobjgrp,
			struct boardobjgrp_pmu_cmd *cmd, u8 id, u8 msgid,
			u16 hdrsize, u16 entrysize, u16 fbsize, u32 ss_offset,
			u8 rpc_func_id);
		int (*boardobjgrp_pmuset_impl)(struct gk20a *g,
			struct boardobjgrp *pboardobjgrp);
		int (*boardobjgrp_pmugetstatus_impl)(struct gk20a *g,
			struct boardobjgrp *pboardobjgrp,
			struct boardobjgrpmask *mask);
		int (*is_boardobjgrp_pmucmd_id_valid)(struct gk20a *g,
			struct boardobjgrp *pboardobjgrp,
			struct boardobjgrp_pmu_cmd *cmd);
	} obj;
	struct {
		int (*clk_set_boot_clk)(struct gk20a *g);
	} clk;
	int (*prepare_ns_ucode_blob)(struct gk20a *g);
};

struct pmu_rtos_fw {
	struct pmu_fw_ver_ops ops;

	struct nvgpu_firmware *fw_desc;
	struct nvgpu_firmware *fw_image;
	struct nvgpu_firmware *fw_sig;

	struct nvgpu_mem ucode;
	struct nvgpu_mem ucode_boot_args;
	struct nvgpu_mem ucode_core_dump;

	u32 state;
	bool ready;

	union {
		struct pmu_cmdline_args_v3 args_v3;
		struct pmu_cmdline_args_v4 args_v4;
		struct pmu_cmdline_args_v5 args_v5;
		struct pmu_cmdline_args_v6 args_v6;
		struct pmu_cmdline_args_v7 args_v7;
	};
};

void nvgpu_pmu_fw_get_cmd_line_args_offset(struct gk20a *g,
	u32 *args_offset);
void nvgpu_pmu_fw_state_change(struct gk20a *g, struct nvgpu_pmu *pmu,
	u32 pmu_state, bool post_change_event);
u32 nvgpu_pmu_get_fw_state(struct gk20a *g, struct nvgpu_pmu *pmu);
bool nvgpu_pmu_get_fw_ready(struct gk20a *g, struct nvgpu_pmu *pmu);
void nvgpu_pmu_set_fw_ready(struct gk20a *g, struct nvgpu_pmu *pmu,
	bool status);
int nvgpu_pmu_wait_fw_ready(struct gk20a *g, struct nvgpu_pmu *pmu);
int nvgpu_pmu_wait_fw_ack_status(struct gk20a *g, struct nvgpu_pmu *pmu,
	u32 timeout_ms, void *var, u8 val);
int nvgpu_pmu_init_fw_ver_ops(struct gk20a *g,
	struct nvgpu_pmu *pmu, u32 app_version);
struct nvgpu_firmware *nvgpu_pmu_fw_sig_desc(struct gk20a *g,
	struct nvgpu_pmu *pmu);
struct nvgpu_firmware *nvgpu_pmu_fw_desc_desc(struct gk20a *g,
	struct nvgpu_pmu *pmu);
struct nvgpu_firmware *nvgpu_pmu_fw_image_desc(struct gk20a *g,
	struct nvgpu_pmu *pmu);
void nvgpu_pmu_fw_deinit(struct gk20a *g, struct nvgpu_pmu *pmu,
	struct pmu_rtos_fw *rtos_fw);
int nvgpu_pmu_init_pmu_fw(struct gk20a *g, struct nvgpu_pmu *pmu,
	struct pmu_rtos_fw **rtos_fw_p);
int nvgpu_pmu_ns_fw_bootstrap(struct gk20a *g, struct nvgpu_pmu *pmu);

#endif /* NVGPU_PMU_FW_H */
