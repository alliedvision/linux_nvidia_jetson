/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/**
 * @file NvGuard_InterfaceDataTypes.h
 * @brief <b> NvGuard interface data types header file </b>
 *
 * Application/driver interface with L0SS
 */

#ifndef NVGUARD_INTERFACEDATATYPES_H
#define NVGUARD_INTERFACEDATATYPES_H

/* ==================[inclusions]============================================ */

/**
 * <!-- Doxygen Group - DO NOT REMOVE -->
 * @defgroup nvguard_interface_datatypes NvGuard Interface Data Types
 *
 * Defines data types for NvGuard interface.
 *
 * @ingroup nvguard_api_top
 * @{
 */

/* ==================[macros]================================================ */
/* Error status values of type 'NvGuard_ErrStatus_t' */
/** Defines the default error status when test for fault is not executed.
 */
#define NVGUARD_ERROR_UNDETERMINED                  (0x00U)
/** Defines the error status when fault is confirmed at source, and is not fixed.
 */
#define NVGUARD_ERROR_DETECTED                      (0xAAU)
/** Defines the error status when the test function does not encounter a fault,
 *  or when a pre-existing fault is fixed.
 */
#define NVGUARD_NO_ERROR                            (0x55U)

/*  NvGuard operation identifiers.
 *  Error code for service error callback(ErrCallbackPtr) function
 *  to indicate failed operation
 */
/** Defines a register operation.
 */
#define NVGUARD_OPERATION_REGISTER                  (0U)
/** Defines an enable operation.
 */
#define NVGUARD_OPERATION_ENABLE                    (1U)
/** Defines a disable operation.
 */
#define NVGUARD_OPERATION_DISABLE                   (2U)
/** Defines an error/service status report operation.
 */
#define NVGUARD_OPERATION_REPORT                    (3U)

/*  3LSS error messages of type 'NvGuard_3LSSError_t'.
 *  Argument for 'InternalError_Notification' callback
 *  in 3LSS notification config structure
 */
/** Denotes 3LSS internal error.
 */
#define NVGUARD_3LSSERR_3LSSINTERNAL                (0U)

/*  Tegra phase values of type 'NvGuard_TegraPhase_t'.
 *  Argument for 'Phase_Notification' callback in 3LSS notification config
 *  structure. 3LSS framework maintains and synchronizes execution phases
 *  to orchestrate safe startup and safe shutdown across different layers.
 *  A client will receive notification for all phase transitions if
 *  callback is configured using 'NvGuard_Register3LSSNotification' API
 */
/** Defines Tegra phase during 3LSS initialization.
 */
#define NVGUARD_TEGRA_PHASE_INIT                    (0U)
/** Defines Tegra phase when 3LSS initialization is completed.
 */
#define NVGUARD_TEGRA_PHASE_INITDONE                (2U)
/** Defines Tegra phase in which periodic tests are triggered.
 */
#define NVGUARD_TEGRA_PHASE_RUN                     (4U)
/** Defines Tegra phase when 3LSS de-registers all clients.
 */
#define NVGUARD_TEGRA_PHASE_PRESHUTDOWN             (6U)
/** Defines Tegra phase after Tegra shutdown request to system manager.
 */
#define NVGUARD_TEGRA_PHASE_SHUTDOWN                (8U)

/* Tegra FuSa state values of type 'NvGuard_FuSaState_t'.
 * FuSa(Functional Safety) state indicates health of the SOC with respect to
 * safety critical errors
 */
/** Defines the FuSa state during initialization.
 */
#define NVGUARD_TEGRA_FUSASTATE_INIT                (0U)
/** Defines the FuSa state when no error is reported to 3LSS.
 */
#define NVGUARD_TEGRA_FUSASTATE_NOERROR             (1U)
/** Defines the FuSa state when an error is reported to 3LSS.
 */
#define NVGUARD_TEGRA_FUSASTATE_ERROR               (2U)

/*  Supplementary notification of type 'NvGuard_SupplementaryNotification_t'.
 *  Argument for supplementary notification callback registered by Drive OS user
 */
/** Defines a change in the FuSa state.
 */
#define NVGUARD_SUPPNOTIF_FUSASTATE_CHANGE          (0U)
/** Defines the availability of user data to be read.
 */
#define NVGUARD_SUPPNOTIF_USERMSG_READY             (1U)

/** Defines the maximum length of user message, in bytes.
 */
#define NVGUARD_USERDATA_MAXLEN                     (56U)

/** Defines the Service Id registration length for the asynchronous call.
 */
#define NVGUARD_SRV_REG_LIST_LEN_ASYNC              (29U)

/* =================NvGuard_LibDataTypes.h Macros =========================== */

/** Service Id registration status length for synchronous call
 */
#define NVGUARD_SRV_REG_LIST_LEN_SYNC               (31U)

/** Length of service list
 */
#define NVGUARD_SRV_LIST_LEN                        (20U)
/** Number of reserved bytes in service status structure
 */
#define RESERVED_BYTES                              (10U)
/** Maximum lenght of error information, in bytes, in service status structure
 */
#define NVGUARD_ERRINFO_LEN                         (180U)
/** Length of group list
 */
#define NVGUARD_GRP_LIST_LEN                        (12U)
/** Number of groups in state packet structure. MUST BE GREATER THAN 0
 */
#define NVGUARD_GROUPSTATEPKT_DATACOUNT             (24U)


/** NvGuard service classes
 */
/** Denotes HSM errors
 */
#define NVGUARD_SERVICECLASS_HSM_ERROR              (0U)
/** Denotes software errors
 */
#define NVGUARD_SERVICECLASS_SW_ERROR               (1U)
/** Denotes diagnostic tests
 */
#define NVGUARD_SERVICECLASS_DIAG_TEST              (2U)


/* Masks and shift values to extract relevant information from a service identifier
 */
/** Bit mask to extract group index from service identifier
 */
#define NVGUARD_SRVID_GROUPINDEX_MASK               (0xFFC00U)
/** Bit position of group index in service identifier
 */
#define NVGUARD_SRVID_GROUPINDEX_SHIFT              (10U)
/** Bit mask to extract error index from service identifier
 */
#define NVGUARD_SRVID_INDEX_MASK                    (0x1FFU)
/** Bit position of error index in service identifier
 */
#define NVGUARD_SRVID_INDEX_SHIFT                   (0U)
/** Bit mask to extract error collator access bit from service identifier
 */
#define NVGUARD_SRVID_ECACCESS_MASK                 (0x200U)
/** Bit position of error collator access bit in service identifier
 */
#define NVGUARD_SRVID_ECACCESS_SHIFT                (9U)
/** Bit mask to extract error class from service identifier
 */
#define NVGUARD_SRVID_CLASS_MASK                    (0xF00000U)
/** Bit position of error class in service identifier
 */
#define NVGUARD_SRVID_CLASS_SHIFT                   (20U)
/** Bit mask to extract layer from service identifier
 */
#define NVGUARD_SRVID_LAYER_MASK                    (0x0F000000U)
/** Bit position of layer in service identifier
 */
#define NVGUARD_SRVID_LAYER_SHIFT                   (24U)


/* Masks and shift values to extract relevant information from a 32 bit client message
 */
/** Bit mask to extract service identifier from client message
 */
#define NVGUARD_CLIENTMSG_SRVID_MASK                (0xFFFFFFFU)
/** Bit position of service identifier in client message
 */
#define NVGUARD_CLIENTMSG_SRVID_SHIFT               (0U)
/** Bit mask to extract service command from client message
 */
#define NVGUARD_CLIENTMSG_SRVCMD_MASK               (0xF0000000U)
/** Bit position of service command in client message
 */
#define NVGUARD_CLIENTMSG_SRVCMD_SHIFT              (28U)
/** Bit mask to extract notification parameter from error ID
 */
#define NVGUARD_CLIENTMSG_NOTIFICATION_MASK          (0xFFU)
/** Bit position of notification parameter in error  ID
 */
#define NVGUARD_CLIENTMSG_NOTIFICATION_SHIFT         (0U)


/* Masks and shift values to extract relevant information from a group identifier
 */
/** Bit mask to extract group index from group identifier
 */
#define NVGUARD_GRPID_INDEX_MASK                    (0x3FFU)
/** Bit position of group index in group identifier
 */
#define NVGUARD_GRPID_INDEX_SHIFT                   (0U)
/** Bit mask to extract layer from group identifier
 */
#define NVGUARD_GRPID_LAYER_MASK                    (0x3C00U)
/** Bit position of layer in group identifier
 */
#define NVGUARD_GRPID_LAYER_SHIFT                   (10U)

/** Client operation values
 */
#define NVGUARD_CONNECTION_INIT                     (0x10U)
#define NVGUARD_CONNECTION_DEINIT                   (0x20U)
#define NVGUARD_REGSITER_SERVICE                    NVGUARD_OPERATION_REGISTER
#define NVGUARD_DEREGSITER_SERVICE                  (0x30U)
#define NVGUARD_ENABLE_SERVICE                      NVGUARD_OPERATION_ENABLE
#define NVGUARD_DISABLE_SERVICE                     NVGUARD_OPERATION_DISABLE
#define NVGUARD_REPORT_STATUS                       NVGUARD_OPERATION_REPORT
#define NVGUARD_REGISTER_NOTIFICATION               (0x40U)
#define NVGUARD_DEREGISTER_NOTIFICATION             (0x50U)
#define NVGUARD_SERVICE_OVERRIDE                    (0x60U)
#define NVGUARD_REQUEST_SERVICE                     (0x05U)
#define NVGUARD_REQUEST_PHASECHANGE                 (0x70U)
#define NVGAURD_SEC_CONFIG                          (0x80U)
#ifdef NVGUARD_ENABLE_ERR_INJ
#define NVGUARD_ERROR_INJECTION                     (0x06U)
#endif /* NVGUARD_ENABLE_ERR_INJ endif*/
#define NVGUARD_USER_MESG                           (0x07U)
#define NVGUARD_PHASE_NOTIFICATION                  (0x08U)
#define NVGUARD_FUSA_NOTIFICATION                   (0x09U)
#define NVGUARD_SERVICESTATUS_NOTIFICATION          (0x0AU)
#define NVGUARD_GROUPSTATE_NOTIFICATION             (0x0BU)
#define NVGUARD_EXECUTE_SERVICEHANDLER              (0x0CU)
#define NVGUARD_READ_SERVICEHANDLERSTATUS           (0x0DU)
#define NVGUARD_ASYNC_SRVSTATUS                     (0x0EU)
#define NVGUARD_READ_SERVICEINFO                    (0x90U)
#define NVGUARD_READ_TESTSTATUS                     (0xA0U)
#define NVGUARD_READ_DIAGPERIOD                     (0xB0U)
#define NVGUARD_SYNC_SRVSTATUS                      (0xC0U)
#define NVGUARD_REPORT_INTERNALERROR                (0XFFU)
#define NVGUARD_READ_USERMSG                        (0x71U)
#define NVGUARD_SEND_ISTMSG                         (0x72U)
#define NVGUARD_NOTIFY_ISTMSG                       (0x73U)
#define NVGUARD_READ_ISTMSG                         (0x74U)
#define NVGUARD_UPDATE_MISSIONPARAM                 (0x75U)
#define NVGUARD_REGISTER_IST                        (0x76U)
#define NVGUARD_READ_SRVSTATUS                      (0xE1U)
#define NVGUARD_INTERNAL_ERROR                      (0xFFU)

/* ================ NvGuard_Global.h Macros ================================= */
#define NVGUARD_LAYER_0_RM_SERVER     (11U)
#define NVGUARD_LAYER_0_VSC           (10U)
#define NVGUARD_LAYER_0_OTA           (9U)
#define NVGUARD_LAYER_0_SECURITY      (8U)
#define NVGUARD_LAYER_0_COMMS         (7U)
#define NVGUARD_LAYER_0_IX_SERVCICE   (6U)
#define NVGUARD_LAYER_0_GOS           (5U)
#define NVGUARD_LAYER_0_SAFETY_SRV    (4U)
#define NVGUARD_LAYER_3               (3U)
#define NVGUARD_LAYER_2               (2U)
#define NVGUARD_LAYER_1               (1U)
#define NVGUARD_LAYER_INVALID         (0U)

#define GUESTOS_LAYERID_BASE  (4U)

#define NVGUARD_MAX_LAYERID   (11U)

/** Enable/disable test application
 */
#define NVGUARD_TESTAPP_EN       0x00


/* =================[type definitions]======================================= */
/*
 * Attributes associated with a service identifier
 */

typedef struct {
	nv_guard_service_id_t SrvId;
	uint8_t isEnabled;
	uint8_t isDriveOSApp;
	uint8_t Reserved[2];
} nv_guard_srv_attributes_t;

/*
 * List of service identifiers
 */
typedef struct {
	uint8_t num_srv;
	uint8_t reserved[3];
	nv_guard_srv_attributes_t srv_list[NVGUARD_SRV_LIST_LEN];
} nv_guard_srv_list_t;

#pragma pack(push, 1)
/*
 * Status associated with a service identifier
 */
typedef struct {
	nv_guard_service_id_t srv_id;
	uint8_t status;
	uint64_t timestamp;
	uint8_t reserved[RESERVED_BYTES];
	uint8_t error_info_size;
	uint8_t error_info[NVGUARD_ERRINFO_LEN];
} nv_guard_srv_status_t;
#pragma pack(pop)

/*
 * List of group identifiers
 */
typedef struct {
	uint8_t num_grp;
	uint8_t reserved[3];
	nv_guard_group_id_t grp_list[NVGUARD_GRP_LIST_LEN];
} nv_guard_grp_list_t;

/*
 * Identifies a client
 */
typedef uint32_t nv_guard_client_id_t;

/*
 * Identifies a client operation
 */
typedef uint32_t nv_guard_cmd_t;

/*
 * Error class, extracted from service identifier.
 *  Distinguishes HSM error, SW error and diagnostic test
 */
typedef uint8_t NvGuard_ErrClass_t;

/*
 * 3LSS layer where a service/group belong to. Extracted from
 *  service identifier/group identifier
 */
typedef uint32_t nv_guard_3lss_layer_t;

/*
 * Group index extracted from group identifier
 */
typedef uint32_t NvGuard_GroupIndex_t;

/*
 * Data structure to communicate error collator configuration requests
 */
typedef struct {
	nv_guard_service_id_t srv_list[NVGUARD_SRV_LIST_LEN];
	uint8_t num_services;
	uint8_t value;
	uint8_t reserved[2];
} nv_guard_err_collator_cfg_t;

/*
 * Defines the group state based on errors reported to NvGuard.
 *  The state is derived from the status of diagnostic
 *  tests and errors within the group.
 */
typedef enum {
  /*
   * Specifies the state when at least one service in the group is in
   *  'NVGUARD_ERROR_UNDETERMINED' state,
   *  and no other error/test failure is reported from the same group.
   */
	NVGUARD_GROUPSTATE_UNDETERMINED = 0,
  /*
   * Specifies the group state when each service within the group is in
   * 'NVGUARD_NO_ERROR' state. The following are true:
   * -# All HSM error lines associated with the group are enabled,
   * and none are asserted.
   * -# Each diagnostic test in the group has executed at least once, and is
   * currently in the 'NVGUARD_NO_ERROR' state.
   * -# The service owners have confirmed each SW error in the group is in the
   * 'NVGUARD_NO_ERROR' state.
   */
	NVGUARD_GROUPSTATE_NOERROR,
  /*
   * Specifies the state when at least one error/test failure
   * is reported from a group.
   */
	NVGUARD_GROUPSTATE_ERRORDETECTED
} nv_guard_group_state_t;

/*
 * Data structure to fetch a group state
 */
typedef struct {
	nv_guard_group_id_t grp_id;
	nv_guard_group_state_t state;
} nv_guard_query_grp_state_t;

/* Holds Tegra FuSa state.
 *
 * @valuerange
 * - NVGUARD_TEGRA_FUSASTATE_INIT
 * - NVGUARD_TEGRA_FUSASTATE_NOERROR
 * - NVGUARD_TEGRA_FUSASTATE_ERROR
 */
typedef uint8_t nv_guard_FuSa_state_t;

/* Supplementary notification of type 'nv_guard_supplementary_notification_t'.
 * Argument for supplementary notification callback registered by clients
 */
typedef uint8_t nv_guard_supplementary_notification_t;

typedef uint8_t nv_guard_tegraphase_t;

/*
 * Defines the user application message transferred to 3LSS. User messages
 *  are transmitted between Application SW at CCPLEX to MCU.
 */
typedef struct {
  uint8_t data[NVGUARD_USERDATA_MAXLEN];
} nv_guard_user_msg_t;

typedef struct {
	nv_guard_cmd_t srv_id_cmd;
	union {
		nv_guard_service_id_t srv_id;
		nv_guard_srv_status_t srv_status;
		nv_guard_user_msg_t user_msg;
		nv_guard_tegraphase_t phase;
	};
} nv_guard_request_t;

typedef enum {
	L1SS_NOT_READY,
	L1SS_READY,
	L1SS_ALIVE_CHECK,
} l1ss_cli_callback_param;

#endif  /* ifndef NVGUARD_INTERFACEDATATYPES_H */

/* ==================[end of file]=========================================== */
