/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _CMD_RESP_L2_INTERFACE_H
#define _CMD_RESP_L2_INTERFACE_H

#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/slab.h>

#include <linux/tegra-ivc.h>
#include <linux/tegra-safety-ivc.h>
#include <linux/tegra-ivc-instance.h>


#include "tegra_l1ss_cmd_resp_exec_config.h"

int l1ss_cmd_resp_send_frame(const cmdresp_frame_ex_t *pCmdPkt,
			     nv_guard_3lss_layer_t NvGuardLayerId,
			     struct l1ss_data *ldata);

int cmd_resp_l1_callback_not_configured(const cmdresp_frame_ex_t *cmd_resp,
		struct l1ss_data *ldata);

int
cmd_resp_l1_user_rcv_register_notification(
					const cmdresp_frame_ex_t *CmdRespFrame,
					struct l1ss_data *ldata);

int cmd_resp_l1_user_rcv_FuSa_state_notification(
					const cmdresp_frame_ex_t *cmdresp_frame,
					struct l1ss_data *ldata);

int cmd_resp_l1_user_rcv_check_aliveness(
					 const cmdresp_frame_ex_t *CmdRespFrame,
					 struct l1ss_data *ldata);

int user_send_service_status_notification(const nv_guard_srv_status_t *Var1,
					nv_guard_3lss_layer_t Layer_Id,
					struct l1ss_data *ldata);

int user_send_ist_mesg(const nv_guard_user_msg_t *var1,
					  nv_guard_3lss_layer_t layer_id,
					  struct l1ss_data *ldata);

int user_send_phase_notify(struct l1ss_data *ldata, nv_guard_3lss_layer_t layer,
		nv_guard_tegraphase_t phase);

int cmd_resp_l1_user_rcv_phase_notify(const cmdresp_frame_ex_t *CmdRespFrame,
					 struct l1ss_data *ldata);
#endif
