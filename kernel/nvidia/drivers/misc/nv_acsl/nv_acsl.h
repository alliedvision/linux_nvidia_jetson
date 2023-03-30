/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __NV_ACSL_H__
#define __NV_ACSL_H__

#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <uapi/misc/nv_acsl_ioctl.h>

#include "nv_acsl_ipc.h"

#define UINT8_MAX 255U

//#define BUF_PRINTS

struct acsl_nvmap_entry {
	/* Memory Management */
	enum dma_data_direction dma_dir;
	struct dma_buf_attachment *attach;
	struct list_head list;
	struct sg_table *sgt;
	dma_addr_t dma_addr;
	struct dma_buf *dmabuf;
	uint32_t refcnt;
};

struct acsl_drv {
	dev_t dev_t;
	struct cdev cdev;
	struct class *class;
	struct device *dev;
	struct acsl_nvmap_args_t *map_args;
	struct mutex map_lock;
	struct list_head map_list;

	nvadsp_app_handle_t csm_app_handle;
	nvadsp_app_info_t *csm_app_info;
	struct csm_sm_state_t *csm_sm;
	struct nvadsp_mbox csm_mbox_send;
	struct nvadsp_mbox csm_mbox_recv;
	struct nvadsp_mbox csm_mbox_buf_in_send;
	struct nvadsp_mbox csm_mbox_buf_out_send;
	struct nvadsp_mbox csm_mbox_buf_in_recv;
	struct nvadsp_mbox csm_mbox_buf_out_recv;

	uint32_t m_acq_buf_index[MAX_PORTS][MAX_COMP];
	uint32_t m_rel_buf_index[MAX_PORTS][MAX_COMP];
	struct completion buff_complete[MAX_PORTS][MAX_COMP];
	struct mutex port_lock[MAX_PORTS][MAX_COMP];

	spinlock_t lock;
	uint64_t flag;
	uint32_t major;
	bool append_init_input_buff[MAX_COMP];
};

status_t csm_app_init(struct acsl_drv *drv);
void csm_app_deinit(struct acsl_drv *drv);

status_t acsl_csm_cmd_send(struct acsl_drv *drv, uint32_t data,
			uint32_t flags, bool block, bool ack);
status_t acsl_comp_open(struct acsl_drv *drv, struct acsl_csm_args_t *csm_args);
status_t acsl_comp_close(struct acsl_drv *drv, struct acsl_csm_args_t *csm_args);

status_t acsl_intf_open(struct acsl_drv *drv, struct acsl_csm_args_t *csm_args);
status_t acsl_intf_close(struct acsl_drv *drv, struct acsl_csm_args_t *csm_args);

status_t acsl_open(struct acsl_drv *drv,  struct acsl_csm_args_t *csm_args);
status_t acsl_close(struct acsl_drv *drv);

uint8_t acsl_acq_buf(struct acsl_drv *drv, struct acsl_buf_args_t *buf_args, uint8_t PORT);
uint8_t acsl_rel_buf(struct acsl_drv *drv, struct acsl_buf_args_t *buf_args, uint8_t PORT);

status_t acsl_map_iova_addr(struct acsl_drv *drv);
status_t acsl_unmap_iova_addr(struct acsl_drv *drv);

/** Enumeration to define the type of CSM command supported by ADSP FW.
 */
enum csm_mbx_cmd {
	CSM_INIT_CMD,
	CSM_DEINIT_CMD,
	CSM_INTF_OPEN_CMD,
	CSM_INTF_CLOSE_CMD,
	CSM_DECODE_CMD,
	CSM_IN_BUF_CMD,
	CSM_OUT_BUF_CMD,
	CSM_COMP_OPEN_CMD,
	CSM_COMP_CLOSE_CMD,
	CSM_BUF_CMD,
};

/** Enumeration to define the type of acsl reply supported by ADSP FW.
 */
enum csm_acsl_reply {
	NONE,
	ACK,
	NACK,
};

#endif /* __NV_ACSL_H__ */
