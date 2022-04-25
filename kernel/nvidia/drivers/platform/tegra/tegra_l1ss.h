/*
 * Copyright (c) 2021, NVIDIA CORPORATION, All rights reserved.
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

#ifndef _LINUX_TEGRA_SAFETY_TEGRA_L1SS_H
#define _LINUX_TEGRA_SAFETY_TEGRA_L1SS_H

#include <linux/cdev.h>
#include <linux/tegra-safety-ivc.h>
#include <linux/tegra_l1ss_ioctl.h>
#include <linux/tegra_nv_guard_group_id.h>
#include <linux/tegra_nv_guard_service_id.h>
#include <linux/platform/tegra/l1ss_datatypes.h>

#include "tegra_l1ss_cmd_resp_exec_config.h"

#define PDEBUG(fmt, args...) printk(KERN_DEBUG "L1SS : " fmt, ## args)
#define TEGRA_SAFETY_L1SS_MAJOR_VERSION 4
#define TEGRA_SAFETY_L1SS_MINOR_VERSION 0
#define TEGRA_SAFETY_L1SS_PATCH_VERSION 0

struct l1ss_req_node {
	struct l1ss_req_node *next;
	nv_guard_request_t *req;
};

typedef int (*cmd_resp_ex_callout)(const cmdresp_frame_ex_t *cmd_resp,
	     struct l1ss_data *ldata);

typedef struct {
	/* CmdID corresponding to CallOut Function */
	uint8_t cmd;
	/* Cmd Callout Function Pointer */
	cmd_resp_ex_callout cmd_call_back;
	/* Response Callout Function Pointer */
	cmd_resp_ex_callout resp_call_back;
	/* Timeout for Response Monitoring */
	uint32_t timeout;
} cmd_resp_look_up_ex;

struct l1ss_data {
	struct workqueue_struct *wq;
	struct work_struct work;
	spinlock_t slock;
	struct l1ss_req_node *head;
	struct cdev cdev;
	int dev_major;
	struct class *l1ss_class;
	dev_t dev;
	struct tegra_safety_ivc *safety_ivc;
	struct {
		wait_queue_head_t notify_waitq;
		atomic_t notify_registered;
	} cmd;
	cmd_resp_look_up_ex (*cmd_resp_lookup_table)
		[CMDRESPL1_MAX_CMD_IN_CLASS];
};

int l1ss_init(struct tegra_safety_ivc *safety_ivc);
int l1ss_exit(struct tegra_safety_ivc *safety_ivc);
int tegra_safety_handle_cmd(cmdresp_frame_ex_t *cmd_resp,
			    struct l1ss_data *ldata);

void
cmd_resp_update_header(
		cmdresp_header_t *pHeader,
		uint8_t Class,
		uint8_t Cmd,
		uint32_t DestId,
		bool IsResp);

/*
 * Helper functions to update/fetch data from CmdResp Header
 * Deviated MISRA Rule 5.9 (Advisory)
 * Declaring an internal linkage function with identifier "l_get_dest_class_id".
 *
 * Reason:
 * False Positive
 */
inline void  l_get_dest_class_id(const cmdresp_header_t *pHeader, uint8_t *ClassId);

/*
 * Deviated MISRA Rule 5.9 (Advisory)
 * Declaring an internal linkage function with identifier "l_set_dest_class_id".
 *
 * Reason:
 * False Positive
 */
inline void l_set_dest_class_id(cmdresp_header_t *const pHeader, uint8_t ClassId);

/*
 * Deviated MISRA Rule 5.9 (Advisory)
 * Declaring an internal linkage function with identifier "l_get_cmd_id".
 *
 * Reason:
 * False Positive
 */
inline void l_get_cmd_id(const cmdresp_header_t *pHeader, uint8_t *CmdId);

/* Deviated MISRA Rule 5.9 (Advisory)
 * Declaring an internal linkage function with identifier "l_set_cmd_id".
 *
 * Reason:
 * False Positive
 */
inline void l_set_cmd_id(cmdresp_header_t *const pHeader, uint8_t CmdId);

/*
 *  Deviated MISRA Rule 5.9 (Advisory)
 *  Declaring an internal linkage function with identifier "l_get_dest_id".
 *
 * Reason:
 * False Positive
 */
inline void l_get_dest_id(const cmdresp_header_t *pHeader, uint8_t *CmdDestId);

/*
 * Deviated MISRA Rule 5.9 (Advisory)
 * Declaring an internal linkage function with identifier "l_set_dest_id".
 *
 * Reason:
 * False Positive
 */
inline void l_set_dest_id(cmdresp_header_t *const pHeader, uint8_t DestId);

/*
 * Deviated MISRA Rule 5.9 (Advisory)
 * Declaring an internal linkage function with identifier "l_get_src_id".
 *
 * Reason:
 * False Positive
 */
inline void l_get_src_id(const cmdresp_header_t *pHeader, uint8_t *CmdSrcId);

/*
 * Deviated MISRA Rule 5.9 (Advisory)
 * Declaring an internal linkage function with identifier "l_set_src_id".
 *
 * Reason:
 * False Positive
 */
inline void l_set_src_id(cmdresp_header_t *const pHeader, uint8_t CmdSrcId);

/*
 * Deviated MISRA Rule 5.9 (Advisory)
 * Declaring an internal linkage function with identifier "l_is_resp_flag_set".
 *
 * Reason:
 * False Positive
 */
inline bool l_is_resp_flag_set(const cmdresp_header_t Header);

/*
 * Deviated MISRA Rule 5.9 (Advisory)
 * Declaring an internal linkage function with identifier "l_set_resp_flag".
 *
 * Reason:
 * False Positive
 */
inline void l_set_resp_flag(cmdresp_header_t *const pHeader);

/* Deviated MISRA Rule 5.9 (Advisory)
 * Declaring an internal linkage function with identifier "l_set_cmd_msg_id".
 *
 * Reason:
 * False Positive
 */
inline void l_set_cmd_msg_id(cmdresp_header_t *const pHeader, uint16_t CmdMsgId);

/*
 * Deviated MISRA Rule 5.9 (Advisory)
 * Declaring an internal linkage function with identifier "l_set_src_class_id".
 *
 * Reason:
 * False Positive
 */
inline void l_set_src_class_id(cmdresp_header_t *const pHeader, uint8_t ClassId);

nv_guard_3lss_layer_t cmd_resp_get_current_layer_id(void);

void l1ss_get_class_cmd_resp_from_header(cmdresp_header_t *cmdresp_h,
		uint8_t *class, uint8_t *cmd, bool *is_resp);

#endif
