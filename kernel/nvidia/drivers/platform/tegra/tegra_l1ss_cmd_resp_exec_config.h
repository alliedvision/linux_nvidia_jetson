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

#ifndef _CMD_RESP_EXEC_CONFIG_H
#define _CMD_RESP_EXEC_CONFIG_H

#include "tegra_l1ss_cmd_resp_l2_interface.h"

#define CMDRESPEXEC_L1_LAYER_ID 1U
#define CMDRESPEXEC_L2_LAYER_ID 2U
#define CMDRESPEXEC_L3_LAYER_ID 3U

/*CmdResp L0,L1 and L2 CLasses*/
#define CMDRESPL1_CLASS0 0U
#define CMDRESPL1_CLASS1 1U
#define CMDRESPL1_CLASS2 2U

/*CmdResp L3 Classes*/
#define CMDRESPL1_CLASS_TEST 0U
#define CMDRESPL1_CLASS_DIAG 1U
#define CMDRESPL1_CLASS_FUSA 2U
#define CMDRESPL1_CLASS_ERR  3U
#define CMDRESPL1_CLASS_HB   4U
#define CMDRESPL1_CLASS_EC   5U
#define CMDRESPL1_CLASS_USER 5U
#define CMDRESPL1_CLASS_IST  6U

#define CMDRESPL1_N_CLASSES 3U
#define CMDRESPL1_MAX_CMD_IN_CLASS 16U

#define CMDRESPL1_N_CLASSES_L3 5U
#define CMDRESPL1_MAX_CMD_IN_CLASS_L3 12U

#define STD_ON 1U
#define STD_OF 0U

/* IVC E2E Configuration */
#define CMDRESPEXEC_L2_IVC_E2E_ENABLED             (STD_OF)
#define CMDRESPEXEC_L2_IVC_E2E_MAX_DELTA_COUNTER   1U

#define CMDRESPL2_CMD_ID_NOTUSED         0xFFU

/*Diag Class Commands*/
#define CMDRESPL1_RUN_ONDEMAND_TEST    0x01U
#define CMDRESPL1_GET_RESULT_DIAG_TEST 0x03U
#define CMDRESPL1_GET_GROUP_STATUS     0x05U

/*FuSa Class Commands*/
#define CMDRESPL1_GET_TEGRA_SW_VERSION 0x01U
#define CMDRESPL1_REQ_SAFETY_DEREG     0x06U
#define CMDRESPL1_REQ_SAFE_SHUTDOWN    0x08U
#define CMDRESPL1_EEP_COMMUNICATION    0x09U
#define CMDRESPL1_REQ_OVERRIDE_ERROR   0x0AU
#define CMDRESPL1_REQ_RUN_PHASE        0x0BU

/*Error Class Commands*/
#define CMDRESPL1_GET_SAFETY_ERROR_STATUS 0x01U

/*CMDRESPL1_L1 Class0 commands*/
#define CMDRESPL1_GET_DIAG_TEST_RESULT    (uint8_t)5
#define CMDRESPL1_GET_ERROR_STATUS        (uint8_t)6
#define CMDRESPL1_GRP_STATUS              (uint8_t)8

/*CMDRESPL1_L1 Class1 Commands*/
#define CMDRESPL1_REGISTER_SERVICE        (uint8_t)1
#define CMDRESPL1_REPORT_SERVICE_STATUS   (uint8_t)4
#define CMDRESPL1_REQUEST_SERVICE         (uint8_t)5
#define CMDRESPL1_FUSA_STATE_NOTIFICATION (uint8_t)9

/*CMDRESPL1_L1 Class2 Commands*/
#define CMDRESPL1_REGISTER_NOTIFICATION   (uint8_t)4
#define CMDRESPL1_DEREGISTER_SERVICE      (uint8_t)5
#define CMDRESPL1_OVERRIDE                (uint8_t)6
#define CMDRESPL1_TESTCOMPLETION_STATUS   (uint8_t)10
#ifdef PROFILING_ENABLED
#define CMDRESPL2_SEND_STATS              (uint8_t)13
#endif

/*CMDRESPL1_L2_CLASS0 Commands*/
#define CMDRESPL1_CHECK_ALIVENESS             0x01U
#define CMDRESPL1_UPDATE_MISSION_PARAM        0x02U
#define CMDRESPL1_INJECT_ERROR                0x03U
#define CMDRESPL1_READ_ADDR                   0x04U
#define CMDRESPL1_ENABLE_ERR_INJECTION        0X0DU
/*CMDRESPL1_L2_CLASS1 Commands*/
#define CMDRESPL1_ENABLE_SERVICE              0x02U
#define CMDRESPL1_DISABLE_SERVICE             0x03U
#define CMDRESPL1_SEND_USERMESG               0x07U
#define CMDRESPL1_PHASE_NOTIFICATION          0x08U
#define CMDRESPL1_SERVICE_STATUS_NOTIFICATION 0x0AU
#define CMDRESPL1_NOTIFY_GRP_STATE            0x0BU
#define CMDRESPL1_SERVICE_HANDLER_EXECUTION   0x0CU
#define CMDRESPL1_SERVICE_HANDLER_REQ_STATUS  0X0DU

/*CMDRESPL1_L2_CLASS2 Commands*/
#define CMDRESPL1_REQUEST_PHASE_CHANGE        0x07U
#define CMDRESPL1_SET_SEC_THRESHOLD           0x08U
#define CMDRESPL1_SEND_ISTMESG                0x0CU

/*CMDRESPL2_HB_CLASS Command*/
#define CMDRESPL1_SEND_HEARTBEAT              0x01U

/*CMDRESP Class 5 Command*/
#define CMDRESPL1_USER_COMMAND                0x01U

/*CMDRESP Class 6 Command*/
#define CMDRESPL1_IST_COMMAND                 0x01U
/*PlceHolder for IST Command*/
#define CMDRESPL1_RCV_IST                     0x02U

#define CMDRESPL1_CMD_ID_NOTUSED              0xFFU

#define CMDRESPL1_L2_CLASS0 \
{\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CHECK_ALIVENESS, cmd_resp_l1_user_rcv_check_aliveness,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_ENABLE_ERR_INJECTION, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
}

#define CMDRESPL1_L2_CLASS1 \
{\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_REGISTER_SERVICE, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_ENABLE_SERVICE, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_DISABLE_SERVICE, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_REPORT_SERVICE_STATUS, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_PHASE_NOTIFICATION, cmd_resp_l1_user_rcv_phase_notify,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_FUSA_STATE_NOTIFICATION,\
		cmd_resp_l1_user_rcv_FuSa_state_notification,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
}

#define CMDRESPL1_L2_CLASS2 \
{\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_REGISTER_NOTIFICATION,\
		cmd_resp_l1_user_rcv_register_notification,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
	{CMDRESPL1_CMD_ID_NOTUSED, cmd_resp_l1_callback_not_configured,\
	cmd_resp_l1_callback_not_configured, 0U},\
}

/*
 * MASK and SHIFT macro to extract Data from CmdResp Frames
 */
#define CMDRESPL2_DEST_CLASS_ID_SHIFT (4U)
#define CMDRESPL2_DEST_CLASS_ID_MASK  ((uint16_t)0x00F0)
#define CMDRESPL2_CMD_ID_MASK         ((uint16_t)0x7800)
#define CMDRESPL2_CMD_ID_SHIFT        (11U)
#define CMDRESPL2_RESPFLAG_MASK       ((uint16_t)0x8000)
#define CMDRESPL2_SRC_CLASS_ID_MASK   ((uint16_t)0x000F)
#define CMDRESPL2_SHUTDOWN_TIMERID    (21U)
#define CMDRESPL2_CMD_ID_RESET        (0x87FFU)
#define CMDRESPL2_CMD_ID_SHIFT        (11U)
#define CMDRESPL2_RESPFLAG_SHIFT      (0x7FFFU)
#define UINT8_MAX    (0xFFU)
#define UINT16_MAX    (0xFFFFU)

#endif
