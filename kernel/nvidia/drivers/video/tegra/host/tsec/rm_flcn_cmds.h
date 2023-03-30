/*
 * RM FLCN CMDS
 *
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

#ifndef __RM_FLCN_CMDS_H__
#define __RM_FLCN_CMDS_H__

struct RM_FLCN_U64 {
	u32	lo;
	u32	hi;
};

struct RM_FLCN_QUEUE_HDR {
	u8	unitId;
	u8	size;
	u8	ctrlFlags;
	u8	seqNumId;
};

#define RM_FLCN_QUEUE_HDR_SIZE		sizeof(struct RM_FLCN_QUEUE_HDR)

struct RM_UPROC_TEST_CMD_WR_PRIV_PROTECTED_REG {
	u8	cmdType;
	u8	regType;
	u8	pad[2];
	u32	val;
};

struct RM_UPROC_TEST_CMD_RTTIMER_TEST {
	u8	cmdType;
	u8	bCheckTime;
	u8	pad[2];
	u32	count;
};

struct RM_UPROC_TEST_CMD_FAKEIDLE_TEST {
	u8	cmdType;
	u8	op;
};

struct RM_UPROC_TEST_CMD_RD_BLACKLISTED_REG {
	u8	cmdType;
	u8	pad[3];
};

struct RM_UPROC_TEST_CMD_MSCG_ISSUE_FB_ACCESS {
	u8	cmdType;
	u8	op;
	u8	pad[2];
	u32	fbOffsetLo32;
	u32	fbOffsetHi32;
};

struct RM_UPROC_TEST_CMD_COMMON_TEST {
	u8	cmdType;
	u32	subCmdType;
	u8	pad[3];
};

union RM_UPROC_TEST_CMD {
	u8	cmdType;
	struct RM_UPROC_TEST_CMD_WR_PRIV_PROTECTED_REG	wrPrivProtectedReg;
	struct RM_UPROC_TEST_CMD_RTTIMER_TEST		rttimer;
	struct RM_UPROC_TEST_CMD_FAKEIDLE_TEST		fakeidle;
	struct RM_UPROC_TEST_CMD_RD_BLACKLISTED_REG	rdBlacklistedReg;
	struct RM_UPROC_TEST_CMD_MSCG_ISSUE_FB_ACCESS	mscgFbAccess;
	struct RM_UPROC_TEST_CMD_COMMON_TEST		commonTest;
};

struct RM_FLCN_HDCP_CMD_GENERIC {
	u8	cmdType;
};

struct RM_FLCN_HDCP_CMD_INIT {
	u8	cmdType;
	u8	reserved[2];
	u8	sorMask;
	u32	chipId;
	u32	options;
};

struct RM_FLCN_HDCP_CMD_SET_OPTIONS {
	u8	cmdType;
	u8	reserved[3];
	u32	options;
};

struct RM_FLCN_MEM_DESC {
	struct RM_FLCN_U64	address;
	u32	params;
};

struct RM_FLCN_HDCP_CMD_VALIDATE_SRM {
	u8	cmdType;
	u8	reserved[3];
	struct RM_FLCN_MEM_DESC	srm;
	u32	srmListSize;
};

struct RM_FLCN_HDCP_CMD_VALIDATE_KSV {
	u8	cmdType;
	u8	head;
	u16	BInfo;
	u32	sorIndex;
	u32	flags;
	u32	ksvNumEntries;
	struct RM_FLCN_MEM_DESC	ksvList;
	struct RM_FLCN_MEM_DESC	srm;
	u32	srmListSize;
	struct RM_FLCN_MEM_DESC	vPrime;
};

struct RM_FLCN_HDCP_CMD_READ_SPRIME {
	u8	cmdType;
};

union RM_FLCN_HDCP_CMD {
	u8	cmdType;
	struct RM_FLCN_HDCP_CMD_GENERIC		gen;
	struct RM_FLCN_HDCP_CMD_INIT		init;
	struct RM_FLCN_HDCP_CMD_SET_OPTIONS	setOptions;
	struct RM_FLCN_HDCP_CMD_VALIDATE_SRM	valSrm;
	struct RM_FLCN_HDCP_CMD_VALIDATE_KSV	valKsv;
	struct RM_FLCN_HDCP_CMD_READ_SPRIME	readSprime;
};

#define HDCP22_NUM_STREAMS_MAX	4
#define HDCP22_NUM_DP_TYPE_MASK	2

enum {
	RM_FLCN_HDCP22_CMD_ID_ENABLE_HDCP22 = 0,
	RM_FLCN_HDCP22_CMD_ID_MONITOR_OFF,
	RM_FLCN_HDCP22_CMD_ID_VALIDATE_SRM2,
	RM_FLCN_HDCP22_CMD_ID_TEST_SE,
	RM_FLCN_HDCP22_CMD_ID_WRITE_DP_ECF,
	RM_FLCN_HDCP22_CMD_ID_VALIDATE_STREAM,
	RM_FLCN_HDCP22_CMD_ID_FLUSH_TYPE,
};

struct HDCP22_STREAM {
	u8	streamId;
	u8	streamType;
};

struct RM_FLCN_HDCP22_CMD_ENABLE_HDCP22 {
	u8	cmdType;
	u8	sorNum;
	u8	sorProtocol;
	u8	ddcPortPrimary;
	u8	ddcPortSecondary;
	u8	bRxRestartRequest;
	u8	bRxIDMsgPending;
	u8	bHpdFromRM;
	u8	bEnforceType0Hdcp1xDS;
	u8	bCheckAutoDisableState;
	u8	numStreams;
	struct HDCP22_STREAM	streamIdType[HDCP22_NUM_STREAMS_MAX];
	u32	dpTypeMask[HDCP22_NUM_DP_TYPE_MASK];
	u32	srmListSize;
	struct RM_FLCN_MEM_DESC	srm;
};

struct RM_FLCN_HDCP22_CMD_MONITOR_OFF {
	u8	cmdType;
	u8	sorNum;
	u8	dfpSublinkMask;
};

struct RM_FLCN_HDCP22_CMD_VALIDATE_SRM2 {
	u8	cmdType;
	u32	srmListSize;
	struct RM_FLCN_MEM_DESC	srm;
};

struct RM_FLCN_HDCP22_CMD_TEST_SE {
	u8	cmdType;
	u8	reserved[3];
	u32	options;
};

struct RM_FLCN_HDCP22_CMD_WRITE_DP_ECF {
	u8	cmdType;
	u8	sorNum;
	u8	reserved[2];
	u32	ecfTimeslot[2];
	u8	bForceClearEcf;
	u8	bAddStreamBack;
};

struct RM_FLCN_HDCP22_CMD_FLUSH_TYPE {
	u8	cmdType;
	u8	reserved[3];
};

union RM_FLCN_HDCP22_CMD {
	u8	cmdType;
	struct RM_FLCN_HDCP22_CMD_ENABLE_HDCP22	cmdHdcp22Enable;
	struct RM_FLCN_HDCP22_CMD_MONITOR_OFF	cmdHdcp22MonitorOff;
	struct RM_FLCN_HDCP22_CMD_VALIDATE_SRM2	cmdValidateSrm2;
	struct RM_FLCN_HDCP22_CMD_TEST_SE	cmdTestSe;
	struct RM_FLCN_HDCP22_CMD_WRITE_DP_ECF	cmdWriteDpEcf;
	struct RM_FLCN_HDCP22_CMD_FLUSH_TYPE	cmdFlushType;
};

enum {
	RM_GSP_SCHEDULER_CMD_ID_TEST = 0x1,
};

struct RM_GSP_SCHEDULER_CMD_TEST {
	u8	cmdType;
	u8	num;
};

union RM_GSP_SCHEDULER_CMD {
	u8	cmdType;
	struct RM_GSP_SCHEDULER_CMD_TEST	test;
};

struct RM_GSP_SCHEDULER_MSG_TEST {
	u8	msgType;
	u8	pad;
	u16	status;
};

union RM_GSP_SCHEDULER_MSG {
	u8	msgType;
	struct RM_GSP_SCHEDULER_MSG_TEST	test;
};

struct RM_GSP_ACR_BOOTSTRAP_ENGINE_DETAILS1 {
	u32	engineId;
	u32	engineInstance;
};

struct RM_GSP_ACR_BOOTSTRAP_ENGINE_DETAILS2 {
	u32	engineIndexMask;
	u32	boot_flags;
};

struct RM_GSP_ACR_CMD_BOOTSTRAP_ENGINE {
	u8	cmdType;
	struct RM_GSP_ACR_BOOTSTRAP_ENGINE_DETAILS1	engineDetails1;
	struct RM_GSP_ACR_BOOTSTRAP_ENGINE_DETAILS2	engineDetails2;
};

struct RM_GSP_ACR_CMD_LOCK_WPR {
	u8	cmdType;
	struct RM_FLCN_U64	wprAddressFb;
};

struct RM_GSP_ACR_CMD_UNLOCK_WPR {
	u8	cmdType;
	u8	unloadType;
};

union RM_GSP_ACR_CMD {
	u8	cmdType;
	struct RM_GSP_ACR_CMD_BOOTSTRAP_ENGINE	bootstrapEngine;
	struct RM_GSP_ACR_CMD_LOCK_WPR		lockWprDetails;
	struct RM_GSP_ACR_CMD_UNLOCK_WPR	unlockWprDetails;
};

struct RM_GSP_RMPROXY_CMD {
	u8	cmdType;
	u32	addr;
	u32	value;
};

struct RM_GSP_SPDM_CE_KEY_INFO {
	u32	ceIndex;
	u32	keyIndex;
	u32	ivSlotIndex;
};

struct RM_GSP_SPDM_CMD_PROGRAM_CE_KEYS {
	u8	cmdType;
	struct RM_GSP_SPDM_CE_KEY_INFO	ceKeyInfo;
};

union RM_GSP_SPDM_CMD {
	u8	cmdType;
	struct RM_GSP_SPDM_CMD_PROGRAM_CE_KEYS	programCeKeys;
};

struct RM_FLCN_CMD_GSP {
	struct RM_FLCN_QUEUE_HDR	hdr;

	union {
		union RM_UPROC_TEST_CMD		test;
		union RM_FLCN_HDCP_CMD		hdcp;
		union RM_FLCN_HDCP22_CMD	hdcp22wired;
		union RM_GSP_SCHEDULER_CMD	scheduler;
		union RM_GSP_ACR_CMD		acr;
		struct RM_GSP_RMPROXY_CMD	rmProxy;
		union RM_GSP_SPDM_CMD		spdm;
	} cmd;
};

struct RM_FLCN_CMD_GEN {
	struct RM_FLCN_QUEUE_HDR	hdr;
	u32	cmd;
};

struct RM_PMU_RPC_CMD {
	u8	padding1;
	u8	flags;
	u16	padding2;
	u32	rpcDmemPtr;
};

struct RM_FLCN_CMD_PMU {
	struct RM_FLCN_QUEUE_HDR	hdr;
	union {
		struct RM_PMU_RPC_CMD	rpc;
	} cmd;
};

#define RM_DPU_CMDQ_LOG_ID 0

struct RM_DPU_REGCACHE_CMD_CONFIG_SV {
	u8	cmdType;
	u8	dmaBufferIdx;
	struct RM_FLCN_MEM_DESC	dmaDesc;
	u32	wborPresentMask;
};

union RM_DPU_REGCACHE_CMD {
	u8	cmdType;
	struct RM_DPU_REGCACHE_CMD_CONFIG_SV	cmdConfigSv;
};

struct RM_DPU_VRR_CMD_ENABLE {
	u8	cmdType;
	u8	headIdx;
	u8	bEnableVrrForceFrameRelease;
	u32	forceReleaseThresholdUs;
};

union RM_DPU_VRR_CMD {
	u8	cmdType;
	struct RM_DPU_VRR_CMD_ENABLE	cmdEnable;
};

struct RM_DPU_SCANOUTLOGGING_CMD_ENABLE {
	u8	cmdType;
	u8	scanoutFlag;
	u32	rmBufTotalRecordCnt;
	u32	head;
	s32	timerOffsetLo;
	s32	timerOffsetHi;
	struct RM_FLCN_MEM_DESC	dmaDesc;
};

struct RM_DPU_SCANOUTLOGGING_CMD_DISABLE {
	u8	cmdType;
};

union RM_DPU_SCANOUTLOGGING_CMD {
	u8	cmdType;
	struct RM_DPU_SCANOUTLOGGING_CMD_ENABLE		cmdEnable;
	struct RM_DPU_SCANOUTLOGGING_CMD_DISABLE	cmdDisable;
};

struct RM_DPU_MSCGWITHFRL_CMD_ENQUEUE {
	u8	cmdType;
	u8	flag;
	u32	head;
	u32	startTimeNsLo;
	u32	startTimeNsHi;
	u32	frlDelayNsLo;
	u32	frlDelayNsHi;
};

union RM_DPU_MSCGWITHFRL_CMD {
	u8	cmdType;
	struct RM_DPU_MSCGWITHFRL_CMD_ENQUEUE	cmdEnqueue;
};

struct RM_DPU_TIMER_CMD_UPDATE_FREQ {
	u8	cmdType;
	u8	reserved[3];
	u32	freqKhz;
};

union RM_DPU_TIMER_CMD {
	u8	cmdType;
	struct RM_DPU_TIMER_CMD_UPDATE_FREQ	cmdUpdateFreq;
};

struct RM_FLCN_CMD_DPU {
	struct RM_FLCN_QUEUE_HDR	hdr;
	union {
		union RM_DPU_REGCACHE_CMD		regcache;
		union RM_DPU_VRR_CMD			vrr;
		union RM_FLCN_HDCP_CMD			hdcp;
		union RM_FLCN_HDCP22_CMD		hdcp22wired;
		union RM_DPU_SCANOUTLOGGING_CMD		scanoutLogging;
		union RM_DPU_MSCGWITHFRL_CMD		mscgWithFrl;
		union RM_DPU_TIMER_CMD			timer;
		union RM_UPROC_TEST_CMD			test;
	} cmd;
};

struct RM_SEC2_TEST_CMD_WR_PRIV_PROTECTED_REG {
	u8	cmdType;
	u8	regType;
	u8	pad[2];
	u32	val;
};

struct RM_SEC2_TEST_CMD_RTTIMER_TEST {
	u8	cmdType;
	u8	bCheckTime;
	u8	pad[2];
	u32	count;
};

struct RM_SEC2_TEST_CMD_FAKEIDLE_TEST {
	u8	cmdType;
	u8	op;
};

struct RM_SEC2_TEST_CMD_RD_BLACKLISTED_REG {
	u8	cmdType;
	u8	pad[3];
};

struct RM_SEC2_TEST_CMD_MSCG_ISSUE_FB_ACCESS {
	u8	cmdType;
	u8	op;
	u8	pad[2];
	u32	fbOffsetLo32;
	u32	fbOffsetHi32;
};

union RM_SEC2_TEST_CMD {
	u8	cmdType;
	struct RM_SEC2_TEST_CMD_WR_PRIV_PROTECTED_REG	wrPrivProtectedReg;
	struct RM_SEC2_TEST_CMD_RTTIMER_TEST		rttimer;
	struct RM_SEC2_TEST_CMD_FAKEIDLE_TEST		fakeidle;
	struct RM_SEC2_TEST_CMD_RD_BLACKLISTED_REG	rdBlacklistedReg;
	struct RM_SEC2_TEST_CMD_MSCG_ISSUE_FB_ACCESS	mscgFbAccess;
};

struct RM_SEC2_CHNMGMT_CMD_ENGINE_RC_RECOVERY {
	u8	cmdType;
	u8	pad[3];
};

struct RM_SEC2_CHNMGMT_CMD_FINISH_RC_RECOVERY {
	u8	cmdType;
	u8	pad[3];
};

union RM_SEC2_CHNMGMT_CMD {
	u8	cmdType;
	struct RM_SEC2_CHNMGMT_CMD_ENGINE_RC_RECOVERY	engineRcCmd;
	struct RM_SEC2_CHNMGMT_CMD_FINISH_RC_RECOVERY	finishRcCmd;
};

struct RM_SEC2_ACR_CMD_BOOTSTRAP_FALCON {
	u8	cmdType;
	u32	flags;
	u32	falconId;
	u32	falconInstance;
	u32	falconIndexMask;
};

struct RM_SEC2_ACR_CMD_WRITE_CBC_BASE {
	u8	cmdType;
	u32	cbcBase;
};

union RM_SEC2_ACR_CMD {
	u8	cmdType;
	struct RM_SEC2_ACR_CMD_BOOTSTRAP_FALCON	bootstrapFalcon;
	struct RM_SEC2_ACR_CMD_WRITE_CBC_BASE	writeCbcBase;
};

struct RM_SEC2_VPR_CMD_SETUP_VPR {
	u8	cmdType;
	u8	pad[3];
	u32	startAddr;
	u32	size;
};

union RM_SEC2_VPR_CMD {
	u8	cmdType;
	struct RM_SEC2_VPR_CMD_SETUP_VPR	vprCmd;
};

struct RM_SEC2_SPDM_CMD_INIT {
	u8	cmdType;
	u8	pad[3];
};

enum SpdmPayloadType {
	SpdmPayloadTypeNormalMessage	= 0x0,
	SpdmPayloadTypeSecuredMessage	= 0x1,
	SpdmPayloadTypeAppMessage	= 0x2,
};

struct RM_SEC2_SPDM_CMD_REQUEST {
	u8	cmdType;
	u8	pad[3];
	u32	reqPayloadEmemAddr;
	u32	reqPayloadSize;
	enum SpdmPayloadType	reqPayloadType;
};

union RM_SEC2_SPDM_CMD {
	u8	cmdType;
	struct RM_SEC2_SPDM_CMD_INIT	initCmd;
	struct RM_SEC2_SPDM_CMD_REQUEST	reqCmd;
};

struct RM_FLCN_CMD_SEC2 {
	struct RM_FLCN_QUEUE_HDR	hdr;
	union {
		union RM_SEC2_TEST_CMD		sec2Test;
		union RM_SEC2_CHNMGMT_CMD	chnmgmt;
		union RM_FLCN_HDCP22_CMD	hdcp22;
		union RM_SEC2_ACR_CMD		acr;
		union RM_SEC2_VPR_CMD		vpr;
		union RM_FLCN_HDCP_CMD		hdcp1x;
		union RM_SEC2_SPDM_CMD		spdm;
		union RM_UPROC_TEST_CMD		test;
	} cmd;
};

union RM_FLCN_CMD {
	struct RM_FLCN_CMD_GEN		cmdGen;
	struct RM_FLCN_CMD_PMU		cmdPmu;
	struct RM_FLCN_CMD_DPU		cmdDpu;
	struct RM_FLCN_CMD_SEC2		cmdSec2;
	struct RM_FLCN_CMD_GSP		cmdGsp;
};

#define RM_DPU_LOG_QUEUE_NUM	2
#define RM_GSP_UNIT_REWIND	(0x00)
#define RM_GSP_UNIT_INIT	(0x02)
#define RM_GSP_UNIT_HDCP22WIRED	(0x06)
#define RM_GSP_UNIT_END		(0x11)

struct RM_GSP_INIT_MSG_GSP_INIT {
	u8	msgType;
	u8	numQueues;
	u16	osDebugEntryPoint;
	struct {
		u32	queueOffset;
		u16	queueSize;
		u8	queuePhyId;
		u8	queueLogId;
	} qInfo[RM_DPU_LOG_QUEUE_NUM];
	u32	rsvd1;
	u8	rsvd2;
	u8	status;
};

struct RM_GSP_INIT_MSG_UNIT_READY {
	u8	msgType;
	u8	taskId;
	u8	taskStatus;
};

union RM_GSP_INIT_MSG {
	u8	msgType;
	struct RM_GSP_INIT_MSG_GSP_INIT		gspInit;
	struct RM_GSP_INIT_MSG_UNIT_READY	msgUnitState;
};

struct RM_UPROC_TEST_MSG_WR_PRIV_PROTECTED_REG {
	u8	msgType;
	u8	regType;
	u8	status;
	u8	pad[1];
	u32	val;
};

struct RM_UPROC_TEST_MSG_RTTIMER_TEST {
	u8	msgType;
	u8	status;
	u8	pad[2];
	u32	oneShotNs;
	u32	continuousNs;
};

struct RM_UPROC_TEST_MSG_FAKEIDLE_TEST {
	u8	msgType;
	u8	status;
};

struct RM_UPROC_TEST_MSG_RD_BLACKLISTED_REG {
	u8	msgType;
	u8	status;
	u8	pad[2];
	u32	val;
};

struct RM_UPROC_TEST_MSG_MSCG_ISSUE_FB_ACCESS {
	u8	msgType;
	u8	status;
	u8	pad[2];
};

struct RM_UPROC_TEST_MSG_COMMON_TEST {
	u8	msgType;
	u8	status;
	u8	pad[2];
};

union RM_UPROC_TEST_MSG {
	u8	msgType;
	struct RM_UPROC_TEST_MSG_WR_PRIV_PROTECTED_REG	wrPrivProtectedReg;
	struct RM_UPROC_TEST_MSG_RTTIMER_TEST		rttimer;
	struct RM_UPROC_TEST_MSG_FAKEIDLE_TEST		fakeidle;
	struct RM_UPROC_TEST_MSG_RD_BLACKLISTED_REG	rdBlacklistedReg;
	struct RM_UPROC_TEST_MSG_MSCG_ISSUE_FB_ACCESS	mscgFbAccess;
	struct RM_UPROC_TEST_MSG_COMMON_TEST		commonTest;
};

struct RM_FLCN_HDCP_MSG_GENERIC {
	u8	msgType;
	u8	status;
	u8	rsvd[2];
};

struct RM_FLCN_HDCP_MSG_VALIDATE_KSV {
	u8	msgType;
	u8	status;
	u8	attachPoint;
	u8	head;
};

struct RM_FLCN_HDCP_MSG_VALIDATE_LPRIME {
	u8	msgType;
	u8	status;
	u8	rsvd[2];
	u8	l[20];
};

struct RM_FLCN_HDCP_MSG_READ_SPRIME {
	u8	msgType;
	u8	status;
	u8	sprime[9];
	u8	rsvd;
};

union RM_FLCN_HDCP_MSG {
	u8	msgType;
	struct RM_FLCN_HDCP_MSG_GENERIC		gen;
	struct RM_FLCN_HDCP_MSG_VALIDATE_KSV	ksv;
	struct RM_FLCN_HDCP_MSG_VALIDATE_LPRIME	lprimeValidateReply;
	struct RM_FLCN_HDCP_MSG_READ_SPRIME	readSprime;
};

enum RM_FLCN_HDCP22_STATUS {
	RM_FLCN_HDCP22_STATUS_ERROR_NULL = 0,
	RM_FLCN_HDCP22_STATUS_ERROR_ENC_ACTIVE,
	RM_FLCN_HDCP22_STATUS_ERROR_FLCN_BUSY,
	RM_FLCN_HDCP22_STATUS_ERROR_TYPE1_LOCK_ACTIVE,
	RM_FLCN_HDCP22_STATUS_ERROR_INIT_SESSION_FAILED,
	RM_FLCN_HDCP22_STATUS_ERROR_AKE_INIT,
	RM_FLCN_HDCP22_STATUS_ERROR_CERT_RX,
	RM_FLCN_HDCP22_STATUS_TIMEOUT_CERT_RX,
	RM_FLCN_HDCP22_STATUS_ERROR_MASTER_KEY_EXCHANGE,
	RM_FLCN_HDCP22_STATUS_ERROR_H_PRIME,
	RM_FLCN_HDCP22_STATUS_TIMEOUT_H_PRIME,
	RM_FLCN_HDCP22_STATUS_ERROR_PAIRING,
	RM_FLCN_HDCP22_STATUS_TIMEOUT_PAIRING,
	RM_FLCN_HDCP22_STATUS_ERROR_LC_INIT,
	RM_FLCN_HDCP22_STATUS_ERROR_L_PRIME,
	RM_FLCN_HDCP22_STATUS_TIMEOUT_L_PRIME,
	RM_FLCN_HDCP22_STATUS_ERROR_SKE_INIT,
	RM_FLCN_HDCP22_STATUS_ERROR_SET_STREAM_TYPE,
	RM_FLCN_HDCP22_STATUS_ERROR_EN_ENC,
	RM_FLCN_HDCP22_STATUS_ERROR_RPTR_INIT,
	RM_FLCN_HDCP22_STATUS_ERROR_RPTR_STREAM_MNT,
	RM_FLCN_HDCP22_STATUS_TIMEOUT_RXID_LIST,
	RM_FLCN_HDCP22_STATUS_ERROR_RPTR_MPRIME,
	RM_FLCN_HDCP22_STATUS_TIMEOUT_MPRIME,
	RM_FLCN_HDCP22_STATUS_ENC_ENABLED,
	RM_FLCN_HDCP22_STATUS_INIT_SECONDARY_LINK,
	RM_FLCN_HDCP22_STATUS_RPTR_STARTED,
	RM_FLCN_HDCP22_STATUS_RPTR_DONE,
	RM_FLCN_HDCP22_STATUS_REAUTH_REQ,
	RM_FLCN_HDCP22_STATUS_MONITOR_OFF_SUCCESS,
	RM_FLCN_HDCP22_STATUS_VALID_SRM,
	RM_FLCN_HDCP22_STATUS_ERROR_INVALID_SRM,
	RM_FLCN_HDCP22_STATUS_TEST_SE_SUCCESS,
	RM_FLCN_HDCP22_STATUS_TEST_SE_FAILURE,
	RM_FLCN_HDCP22_STATUS_WRITE_DP_ECF_SUCCESS,
	RM_FLCN_HDCP22_STATUS_WRITE_DP_ECF_FAILURE,
	RM_FLCN_HDCP22_STATUS_ERROR_NOT_SUPPORTED,
	RM_FLCN_HDCP22_STATUS_ERROR_HPD,
	RM_FLCN_HDCP22_STATUS_VALIDATE_STREAM_SUCCESS,
	RM_FLCN_HDCP22_STATUS_ERROR_VALIDATE_STREAM_FAILURE,
	RM_FLCN_HDCP22_STATUS_ERROR_STREAM_INVALID,
	RM_FLCN_HDCP22_STATUS_ERROR_ILLEGAL_TIMEREVENT,
	RM_FLCN_HDCP22_STATUS_FLUSH_TYPE_SUCCESS,
	RM_FLCN_HDCP22_STATUS_FLUSH_TYPE_FAILURE,
	RM_FLCN_HDCP22_STATUS_FLUSH_TYPE_LOCK_ACTIVE,
	RM_FLCN_HDCP22_STATUS_FLUSH_TYPE_IN_PROGRESS,
	RM_FLCN_HDCP22_STATUS_ERROR_REGISTER_RW,
	RM_FLCN_HDCP22_STATUS_INVALID_ARGUMENT,
	RM_FLCN_HDCP22_STATUS_ERROR_INTEGRITY_CHECK_FAILURE,
	RM_FLCN_HDCP22_STATUS_ERROR_INTEGRITY_UPDATE_FAILURE,
	RM_FLCN_HDCP22_STATUS_ERROR_DISABLE_WITH_LANECNT0,
	RM_FLCN_HDCP22_STATUS_ERROR_START_TIMER,
	RM_FLCN_HDCP22_STATUS_ERROR_HWDRM_WAR_AUTH_FAILURE,
	RM_FLCN_HDCP22_STATUS_ERROR_START_SESSION,
};

struct RM_FLCN_HDCP22_MSG_GENERIC {
	u8	msgType;
	enum RM_FLCN_HDCP22_STATUS	flcnStatus;
	u8	streamType;
};

union RM_FLCN_HDCP22_MSG {
	u8	msgType;
	struct RM_FLCN_HDCP22_MSG_GENERIC	msgGeneric;
};

struct RM_GSP_ACR_MSG_BOOTSTRAP_ENGINE {
	u8	msgType;
	u32	errorCode;
	struct RM_GSP_ACR_BOOTSTRAP_ENGINE_DETAILS1	engineDetails;
};

struct RM_GSP_ACR_MSG_LOCK_WPR {
	u8	msgType;
	u32	errorCode;
	u32	errorInfo;
};

struct RM_GSP_ACR_MSG_UNLOCK_WPR {
	u8	msgType;
	u32	errorCode;
	u32	errorInfo;
};

union RM_GSP_ACR_MSG {
	u8	msgType;
	struct RM_GSP_ACR_MSG_BOOTSTRAP_ENGINE	msgEngine;
	struct RM_GSP_ACR_MSG_LOCK_WPR		msgLockWpr;
	struct RM_GSP_ACR_MSG_UNLOCK_WPR	msgUnlockWpr;
};

struct RM_GSP_RMPROXY_MSG {
	u8	msgType;
	u8	result;
	u32	value;
};

struct RM_GSP_SPDM_MSG_PROGRAM_CE_KEYS {
	u8	msgType;
	u32	errorCode;
};

union RM_GSP_SPDM_MSG {
	u8	msgType;
	struct RM_GSP_SPDM_MSG_PROGRAM_CE_KEYS	msgProgramCeKeys;
};

struct RM_FLCN_MSG_GSP {
	struct RM_FLCN_QUEUE_HDR	hdr;
	union {
		union RM_GSP_INIT_MSG		init;
		union RM_UPROC_TEST_MSG		test;
		union RM_FLCN_HDCP_MSG		hdcp;
		union RM_FLCN_HDCP22_MSG	hdcp22wired;
		union RM_GSP_SCHEDULER_MSG	scheduler;
		union RM_GSP_ACR_MSG		acr;
		struct RM_GSP_RMPROXY_MSG	rmProxy;
		union RM_GSP_SPDM_MSG		spdm;
	} msg;
};

/*!
 * Convenience macros for determining the size of body for a command or message:
 */
#define RM_FLCN_CMD_BODY_SIZE(u, t)      sizeof(struct RM_FLCN_##u##_CMD_##t)

/*!
 * Convenience macros for determining the size of a command or message:
 */
#define RM_FLCN_CMD_SIZE(u, t)  \
	(RM_FLCN_QUEUE_HDR_SIZE + RM_FLCN_CMD_BODY_SIZE(u, t))

#endif

