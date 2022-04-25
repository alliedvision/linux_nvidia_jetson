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

#include "tegra_l1ss.h"
#include "tegra_l1ss_cmd_resp_l2_interface.h"
#include "tegra_l1ss_heartbeat.h"

/* 2s / 40ms */
#define TEGRA_L1SS_INIT_DONE_PHASE_HB_COUNT	50
/* 6s / 40ms */
#define TEGRA_L1SS_RUN_PHASE_HB_COUNT		150

static nv_guard_tegraphase_t phase = NVGUARD_TEGRA_PHASE_INITDONE;
int l1ss_cmd_resp_send_frame(const cmdresp_frame_ex_t *pCmdPkt,
			     nv_guard_3lss_layer_t NvGuardLayerId,
			     struct l1ss_data *ldata);

int cmd_resp_l1_callback_not_configured(const cmdresp_frame_ex_t *cmd_resp,
					struct l1ss_data *ldata)
{
	uint8_t cmd;
	uint8_t class;
	bool is_resp;
	cmdresp_header_t *cmdresp_h = (cmdresp_header_t *)&(cmd_resp->header);

	l1ss_get_class_cmd_resp_from_header(cmdresp_h, &class, &cmd, &is_resp);
	PDEBUG("%s: Class(%d), Cmd(%d), is_resp(%d) not implemented\n",
			__func__, class, cmd, is_resp);
	return 0;
}

int cmd_resp_l1_user_rcv_register_notification(
					const cmdresp_frame_ex_t *cmdresp_frame,
					struct l1ss_data *ldata)
{
	nv_guard_grp_list_t lVar1;
	uint8_t *lPtr;

	lPtr = (uint8_t *)&lVar1;
	(void)memcpy(lPtr, &(cmdresp_frame->data[0]),
			sizeof(nv_guard_grp_list_t));
	PDEBUG("%s(%d) num_group=%d %d\n", __func__, __LINE__,
			lVar1.num_grp, lVar1.grp_list[0]);
	atomic_set(&ldata->cmd.notify_registered, 1);
	wake_up(&ldata->cmd.notify_waitq);

	return 0;
}
int cmd_resp_l1_user_rcv_FuSa_state_notification(
					const cmdresp_frame_ex_t *cmdresp_frame,
					struct l1ss_data *ldata)
{
	const nv_guard_FuSa_state_t *l_var1;

	l_var1 = (const nv_guard_FuSa_state_t *)&(cmdresp_frame->data[0]);
	PDEBUG("%s FuSa State = %d\n", __func__, *l_var1);

	return 0;
}

static void tegra_safety_create_l1ss_hb(cmdresp_frame_ex_t *l1ss_hb)
{
	static u16 monotonic_count;

	/* Set phase to NVGUARD_TEGRA_PHASE_INIT for Init stage */
	l1ss_hb->data[TEGRA_3LSS_PHASE_BYTE] = l_set_hb_field(
			l1ss_hb->data[TEGRA_3LSS_PHASE_BYTE],
			TEGRA_3LSS_PHASE_MASK,
			TEGRA_3LSS_PHASE_SHIFT, phase);

	/* Hard coding SW error status to no error for now. */
	l1ss_hb->data[SW_ERROR_STATUS_BYTE] = NVGUARD_NO_ERROR;
	l1ss_hb->data[HW_ERROR_STATUS_BYTE] = NVGUARD_NO_ERROR;
	l1ss_hb->data[DIAG_SERVICE_STATUS_BYTE] = l_set_hb_field(
			l1ss_hb->data[DIAG_SERVICE_STATUS_BYTE],
			DIAG_SERVICE_STATUS_MASK,
			DIAG_SERVICE_STATUS_SHIFT, SDL_E_OK);

	memcpy(&l1ss_hb->data[MONOTONIC_COUNT_LB],
		(uint8_t *)&monotonic_count, sizeof(uint16_t));

	if (monotonic_count < (uint16_t)UINT16_MAX)
		monotonic_count++;
	else
		monotonic_count = 0;

	/* SCE is configured currently for 40ms of HB period */
	l1ss_hb->data[DIAG_PERIOD_BYTE] = 4;
	/* Variable to store common service deadline */
	l1ss_hb->data[SRV_DEADLINE_BYTE] = 2;
	l1ss_hb->data[MAJOR_VER_BYTE] = TEGRA_SAFETY_L1SS_MAJOR_VERSION;
	l1ss_hb->data[MINOR_VER_BYTE] = TEGRA_SAFETY_L1SS_MINOR_VERSION;
	l1ss_hb->data[PATCH_VER_BYTE] = TEGRA_SAFETY_L1SS_PATCH_VERSION;

	cmd_resp_update_header(&l1ss_hb->header, CMDRESPL1_CLASS0,
				CMDRESPL1_CHECK_ALIVENESS, NVGUARD_LAYER_2,
				true);
}

int cmd_resp_l1_user_rcv_check_aliveness(const
					cmdresp_frame_ex_t *cmdresp_frame,
					struct l1ss_data *ldata)
{
	static cmdresp_frame_ex_t l1ss_hb = {0};
	static bool first_hb = true;
	int ret;

	tegra_safety_create_l1ss_hb(&l1ss_hb);
	ret = l1ss_cmd_resp_send_frame(&l1ss_hb, CMDRESPEXEC_L2_LAYER_ID,
			ldata);
	if (ret < 0) {
		pr_err("%s: Failed to send HB(ret - %d)\n", __func__, ret);
		return ret;
	}

	if (first_hb) {
		first_hb = false;
		pr_info("Sending first HB\n");
	}

	return 0;
}

int user_send_service_status_notification(const nv_guard_srv_status_t *Var1,
					  nv_guard_3lss_layer_t Layer_Id,
					  struct l1ss_data *ldata) {
	cmdresp_frame_ex_t lCmdRespData = {0};
	//NvGuard_ReturnType_t lRet = NVGUARD_E_NOK;
	uint8_t lBytePos = 0U;
	const uint8_t *lPtr = (const uint8_t *)(Var1);
	unsigned long timeout;

	PDEBUG("SrvId = %d Status=%d ErrorInfoSize=%d ErrorInfo=%s\n",
			Var1->srv_id, Var1->status,
			Var1->error_info_size, Var1->error_info);
	(void)memcpy(&lCmdRespData.data[lBytePos], lPtr,
			sizeof(nv_guard_srv_status_t));
	lBytePos += (uint8_t)sizeof(nv_guard_srv_status_t);

	/* ToDo: Calclulate CRC
	 * lCmdRespData.e2ecf1_crc = 0xEFEF;
	 * lCmdRespData.e2ecf2 = 0xAA;
	 */
	cmd_resp_update_header(&(lCmdRespData.header),
			CMDRESPL1_CLASS1,
			CMDRESPL1_SERVICE_STATUS_NOTIFICATION,
			Layer_Id,
			false);

	PDEBUG("%s(%d) wait for register notify from SCE\n",
			__func__, __LINE__);
	timeout =
		wait_event_interruptible_timeout(ldata->cmd.notify_waitq,
						 (atomic_read(
						 &ldata->cmd.notify_registered)
						  == 1),
						 10 * HZ);
	if (timeout <= 0) {
		PDEBUG("Done ..but timeout ... wait for register notify\n");
		return -1;
	}
	PDEBUG("%s(%d) Done ..wait for register notify\n", __func__, __LINE__);

	l1ss_cmd_resp_send_frame(&lCmdRespData, Layer_Id, ldata);

	return 0;
}

int user_send_ist_mesg(const nv_guard_user_msg_t *var1,
					  nv_guard_3lss_layer_t layer_id,
					  struct l1ss_data *ldata) {
	cmdresp_frame_ex_t lCmdRespData = {0};
	//NvGuard_ReturnType_t lRet = NVGUARD_E_NOK;
	uint8_t lBytePos = 0U;
	const uint8_t *lPtr = (const uint8_t *)(var1);

	(void)memcpy(&lCmdRespData.data[lBytePos], lPtr,
			sizeof(nv_guard_user_msg_t));
	lBytePos += (uint8_t)sizeof(nv_guard_user_msg_t);

	cmd_resp_update_header(&(lCmdRespData.header),
			CMDRESPL1_CLASS2,
			CMDRESPL1_SEND_ISTMESG,
			layer_id,
			false);
	l1ss_cmd_resp_send_frame(&lCmdRespData, layer_id, ldata);

	return 0;
}

int user_send_phase_notify(struct l1ss_data *ldata, nv_guard_3lss_layer_t layer,
		nv_guard_tegraphase_t phase)
{
	static cmdresp_frame_ex_t send_phase = {0};
	int timeout =
		wait_event_interruptible_timeout(ldata->cmd.notify_waitq,
						 (atomic_read(
						 &ldata->cmd.notify_registered)
						  == 1),
						 10 * HZ);
	if (timeout <= 0) {
		PDEBUG("Done ..but timeout ... wait for register notify\n");
		return -1;
	}

	cmd_resp_update_header(&send_phase.header, CMDRESPL1_CLASS1,
			CMDRESPL1_PHASE_NOTIFICATION, layer, false);
	(void)memcpy(&send_phase.data[0], &phase, sizeof(uint8_t));

	pr_info("%s: Sending phase %d to L2SS\n", __func__, phase);
	return l1ss_cmd_resp_send_frame(&send_phase, NVGUARD_LAYER_2, ldata);
}

int cmd_resp_l1_user_rcv_phase_notify(const cmdresp_frame_ex_t *cmdresp_frame,
		struct l1ss_data *ldata)
{
	(void)memcpy(&phase, &(cmdresp_frame->data[0]),
			sizeof(nv_guard_tegraphase_t));
	return 0;
}
