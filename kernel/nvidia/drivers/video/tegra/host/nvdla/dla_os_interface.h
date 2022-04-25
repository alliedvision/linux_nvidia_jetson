/*
 * NVDLA OS Interface
 *
 * Copyright (c) 2016-2021, NVIDIA Corporation.  All rights reserved.
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

#ifndef DLA_OS_INTERFACE_H
#define DLA_OS_INTERFACE_H

/**
 * @ingroup Task
 * @name Task descriptor version
 * @brief Jobs to DLA are submitted in form of task and uses @ref dla_task_descriptor
 * @{
 */
#define DLA_DESCRIPTOR_VERSION	2U
/** @} */

/**
 * @ingroup Engine
 * @name Engine ID
 * @brief DLA engine ID used to verify version engine
 * @{
 */
#define DLA_ENGINE_ID		0x44
/** @} */

/**
 * @ingroup Command
 * @name Command mask
 * @brief Command is sent through method registers. bit[15:0] specifies
 *        command IDs mentioned in @ref Command IDs
 * @{
 */
#define DLA_METHOD_ID_CMD_MASK		0xff
/** @} */

/**
 * DLA Response Masks
 *
 * Response of a command is sent using mailbox registers. Below
 * specifies contents in mailbox register for a response
 */
enum dla_response_mask {
	/**
	 * Response Message Mask
	 */
	DLA_RESPONSE_MSG_MASK		= 0xffU,
	/**
	 * Response Command Mask
	 */
	DLA_RESPONSE_CMD_MASK		= 0xffU,
	/**
	 * Response Error Mask
	 */
	DLA_RESPONSE_ERROR_MASK		= 0xffU,
};

/**
 * DLA Response Shifts
 *
 * Response of a command is sent using mailbox registers. Below
 * specifies contents in mailbox register for a response
 */
enum dla_response_shift {
	/**
	 * Response Message Shift
	 */
	DLA_RESPONSE_MSG_SHIFT		= 0U,
	/**
	 * Response Command Shift
	 */
	DLA_RESPONSE_CMD_SHIFT		= 8U,
	/**
	 * Response Error Shift
	 */
	DLA_RESPONSE_ERROR_SHIFT	= 16U,
};

/**
 * DLA Interrupt on Command completion or Error Shift
 */
enum dla_interrupt_on_event_shift {
	/**
	 * Shift value for DLA Interrupt on Command completion
	 */
	DLA_INT_ON_COMPLETE_SHIFT	= 8U,
	/**
	 * Shift value for DLA Interrupt on Error
	 */
	DLA_INT_ON_ERROR_SHIFT		= 9U,
};

/**
 * List of pre-actions and post-actions
 */
enum dla_action {
	/**
	 * control actions
	 */
	ACTION_TERMINATE	= 0x0U,

	/* conditional actions */
	ACTION_SEM_EQ	= 0x90U,
	ACTION_SEM_GE	= 0x92U,
	ACTION_GOS_EQ	= 0xB0U,
	ACTION_GOS_GE	= 0xB2U,
	ACTION_TASK_STATUS_EQ	= 0xC0U,

	/* write actions */
	ACTION_WRITE_SEM	= 0x80U,
	ACTION_INCREMENT_SEM	= 0x82U,
	ACTION_WRITE_TS_SEM	= 0x83U,
	ACTION_WRITE_TIMESTAMP	= 0x87U,
	ACTION_WRITE_GOS	= 0xA0U,
	ACTION_WRITE_TASK_STATUS	= 0xC1U,
};

#define PING_DATA_SIZE		4
#define BUFFER_MULTIPLIER	4
#define MAX_NUM_GRIDS		6

#define ERR(code) (-(int32_t)DLA_ERR_##code)

/**
 * List of DLA commands sent by CCPlex to Firmware
 */
enum dla_commands {
	/**
	 * Used for testing communication between CCPLEX and DLA
	 */
	DLA_CMD_PING			= 1U,
	DLA_CMD_GET_STATUS_UNUSED	= 2U,
	DLA_CMD_RESET_UNUSED		= 3U,
	DLA_CMD_DLA_CONTROL_UNUSED	= 4U,
	DLA_CMD_GET_QUEUE_STATUS_UNUSED	= 5U,
	DLA_CMD_GET_STATISTICS_UNUSED	= 6U,
	/**
	 * Submit task to DLA
	 */
	DLA_CMD_SUBMIT_TASK		= 7U,
	DLA_CMD_SET_SCHEDULER_UNUSED	= 8U,
	DLA_CMD_READ_INFO_UNUSED	= 9U,
	/**
	 * Set various debugging parameters (trace/printf/crashdump)
	 * Only enabled in Debug build.
	 */
	DLA_CMD_SET_DEBUG		= 10U,
	/**
	 * Set the address & size of various regions used for various reasons
	 */
	DLA_CMD_SET_REGIONS		= 11U,
	/**
	 * Suspend processing a queue
	 */
	DLA_CMD_QUEUE_SUSPEND		= 12U,
	/**
	 * Resume processing a queue
	 */
	DLA_CMD_QUEUE_RESUME		= 13U,
	/**
	 * Flushes a queue
	 */
	DLA_CMD_QUEUE_FLUSH		= 14U,
};

/**
 * Error Response sent back to CCPLEX
 */
enum dla_errors {
	DLA_ERR_NONE			= 0,
	DLA_ERR_INVALID_METHOD		= 1,
	DLA_ERR_INVALID_TASK		= 2,
	DLA_ERR_INVALID_INPUT		= 3,
	DLA_ERR_INVALID_FALC_DMA	= 4,
	DLA_ERR_INVALID_QUEUE		= 5,
	DLA_ERR_INVALID_PREACTION	= 6,
	DLA_ERR_INVALID_POSTACTION	= 7,
	DLA_ERR_NO_MEM			= 8,
	DLA_ERR_INVALID_DESC_VER	= 9,
	DLA_ERR_INVALID_ENGINE_ID	= 10,
	DLA_ERR_INVALID_REGION		= 11,
	DLA_ERR_PROCESSOR_BUSY		= 12,
	DLA_ERR_RETRY			= 13,
	DLA_ERR_TASK_STATUS_MISMATCH	= 14,
	DLA_ERR_ENGINE_TIMEOUT		= 15,
	DLA_ERR_DATA_MISMATCH		= 16,
};

/**
 * Message sent back to CCPlex
 */
enum dla_msgs {
	DLA_MSG_CMD_ERROR		= 1U,
	DLA_MSG_CMD_COMPLETE		= 2U,
	DLA_MSG_EXCEPTION		= 3U,
	DLA_MSG_SWBREAKPT		= 4U,
	DLA_MSG_UNHANDLED_INTERRUPT	= 5U,
	DLA_MSG_UNUSED			= 6U,
	DLA_MSG_DEBUG_PRINT		= 7U,
	DLA_MSG_TASK_TIMEOUT		= 8U,
};

/**
 * Magic number expected to be written to mailbox0 after
 * interuppt handling is complete
 */
#define DLA_MSG_INTERRUPT_HANDLING_COMPLETE 0xD1A0CAFE

/**
 * Struct dla_task_descriptor
 *
 * Task descriptor for DLA_CMD_SUBMIT_TASK
 */
struct dla_task_descriptor {
	/**
	 * Common parameters
	 */
	uint64_t next; /**< Pointer to next task descriptor in queue */
	uint8_t version; /**< Descriptor version */
	uint8_t engine_id; /**< DLA engine ID */
	uint16_t size; /**< Task descriptor size including preactions and postactions */
	uint16_t sequence; /**< Not used in DLA */
	uint8_t num_preactions; /**< Number of preactions */
	uint8_t num_postactions; /**< Number of postactions */
	/**
	 * Offset to a list of dla_action_list structures that should be executed
	 * before starting the task.
	 */
	uint16_t preactions;
	/**
	 * Offset to a list of dla_action_list structures that should be executed
	 * after executing the task.
	 */
	uint16_t postactions;

	/**
	 * DLA specific parameters
	 */

	/**
	 * Queue identifier. The tasks are divided into independent queues.
	 * The scheduler on DLA goes through each queue and tries to get the
	 * first entry from the queue.
	 */
	uint8_t queue_id;

	/**
	 * IOVA address list for addresses used in surface descriptors
	 *
	 * Index references used in address list are as:
	 *
	 * address_list[0]:  address of a dla_network_desc
	 *
	 * address_list[net.dependency_graph_index] : start address of a list of dla_common_op_desc
	 *
	 * address_list[net.lut_data_index]         : start address of a list of dla_lut_param
	 *
	 * address_list[net.roi_array_index]        : start address of a list of dla_roi_desc, but the
	 *			    first entry has to be dla_roi_array_desc
	 */
	uint64_t address_list;
	uint16_t num_addresses; /**< Number of addresses in address list */
	uint16_t status; /**< Update task status here after completion */
	uint64_t timeout; /**< Timeout value for the task */
#define DLA_DESC_FLAGS_BYPASS_EXEC	(1U << 0)
	uint16_t flags;

	uint64_t reserved1;
	uint64_t reserved2;
} __attribute__ ((packed, aligned(4)));

/**
 * Struct dla_action_list
 *
 * Below are the different types of actions supported on DLA. It will throw
 * error for any other action specified in action list. Each action has an opcode
 * associated with it which is used to identify the type of action and then read
 * action data which is appended immediately next to opcode without any padding.
 *
 * Firmware uses this format to read all actions and execute.
 *
 * DLA firmware reads only the action that is supposed to execute due to memory
 * restrictions. It keeps that action cached in DMEM until it is successful if
 * that is a blocking action.
 *
 * Firmware executes actions in a list until it finds terminate
 * action or size of action list is executed.
 *
 * Host OS must add terminate action at the end of list to terminate it.
 *
 * DLA action list
 */
struct dla_action_list {
	uint16_t offset; /**< Offset to action list from start of task descriptor */
	uint16_t size; /**< Total size of action list */
} __attribute__ ((packed));

/**
 * Struct dla_action_opcode
 *
 * Structure to hold DLA action opcode
 */
struct dla_action_opcode {
	uint8_t value; /**< Opcode value */
} __attribute__ ((packed));

/**
 * Semaphore action structure
 *
 * DLA action semaphore structure includes information about fence type, offset
 * and value.
 *
 * OPCODE = 0x90/0x80/0x92/0x83
 *
 * Action ID from unified task descriptor definition:
 *
 * 0x80: [iova[uint64] p] [uint32 v] Write given value to an address
 *
 * 0x82: [iova[uint64] p] [uint32 v] Incerment value at given address by v
 *
 * 0x90: [iova[uint64] p] [uint32 v] Blocks processing of an action list until pointer p has value v
 *
 * 0x92: [iova[uint64] p] [uint32 v] As 0x90, except replacing the equality predicate with greater-than-equal (Not permitted in completion list)
 *
 * 0x83: [iova[uint64] p] [uint32 v] Write given value to an address. Write 64-bit hardware timer value (timestamp) to pointer p+8
 */
struct dla_action_semaphore {
	uint64_t address; /**< Address to read or write value */
	uint32_t value; /**< Value to compare */
} __attribute__ ((packed));

/**
 * GoS action structure
 *
 * DLA action GoS structure includes information about GoS index, offset and value.
 *
 * OPCODE = 0xA0/0xB0/0xA2
 *
 * Action ID from unified task descriptor definition:
 *
 * 0xA0/0xB0/0xB2: [uint8 gos] [uint16 ofs] [uint32 v]
 *
 * Same as the 0x80/0x90/0x92 actions, except instead of identifying a semaphore
 * with a pointer, it is identified with a grid of semaphores number.
 */
struct dla_action_gos {
	uint8_t index; /**< Index of Grid Of Semaphores */
	uint16_t offset; /**< Offset within grid */
	uint32_t value; /**< Value to compare */
} __attribute__ ((packed));

/**
 * Status notifier action structure
 *
 * This structure is used for delivering information about an error between the
 * engines in the CV pipeline. If an error is detected while processing a surface,
 * the status is set to 1
 *
 * OPCODE = 0xC0/0xC1
 *
 * Action ID from unified task descriptor definition:
 * 0xC0: [iova[status notifier] p] [uint16 status]
 * Verify status in the given address. The address is an IOVA to a struct dla_task_status.
 */
struct dla_action_task_status {
	uint64_t address; /**< Address to struct dla_task_status */
	uint16_t status;  /**< Status to compare or update */
} __attribute__ ((packed));

/**
 * Timestamp update action structure
 *
 * OPCODE = 0x87
 */
struct dla_action_timestamp {
	uint64_t address; /**< Address to write timestamp value */
} __attribute__ ((packed));

/**
 * Status notifier structure
 */
struct dla_task_status_notifier {
	uint64_t timestamp; /**< 64-bit timestamp representing the time at which the notifier was written */
	uint32_t status_engine; /**< status work captured from HW engine */
	uint16_t subframe; /**< NA */
	uint16_t status_task; /**< status word as configured from an action list */
} __attribute__ ((packed));

/** @cond DISABLED_IN_SAFETY_BUILD */
/**
 * Regions to be configured from host
 */
enum dla_regions_e {
	DLA_REGION_PRINTF = 1,
	DLA_REGION_GOS = 2,
	DLA_REGION_TRACE = 3,
	DLA_REGION_GCOV = 4,
};

/**
 * DLA_PRINTF_REGION
 *
 * Command to configure printf regions from host
 *
 */
struct dla_region_printf {
	uint32_t region; /**< value for DLA_PRINTF_REGION */
	uint64_t address; /**< region address */
	uint32_t size; /**< size of region */
} __attribute__ ((packed, aligned(8)));

/**
 * DLA_REGION_GOS
 *
 * Command to set GoS regions
 *
 */
struct dla_region_gos {
	uint32_t region; /**< value for DLA_REGION_GOS */
	uint16_t num_grids; /**< IOVA/PA address of region */
	uint16_t grid_size; /**< Number of grids */
	uint64_t address[MAX_NUM_GRIDS]; /**< Size of each grid */
} __attribute__ ((packed, aligned(8)));

/**
 * Debug Setting to be configured from host
 */
enum dla_debug_e {
	DLA_SET_TRACE_ENABLE = 1,
	DLA_SET_TRACE_EVENT_MASK = 2,
};

/**
 * DLA_SET_TRACE_EVENTS
 *
 * Command to configure Trace Events from host
 */
struct dla_debug_config {
	uint32_t sub_cmd; /**< subcommand within Set Debug Command */
	uint64_t data; /**< to hold the data e.g trace_enable/event_mask */
	uint64_t reserved; /**< to keep this reserved for future use */
} __attribute__ ((packed));

#define MAX_MESSAGE_SIZE	512U /**< Maxmimum message size in bytes */

/**
 * Struct print_data
 *
 * Holds buffer to capture prints from Firmware
 */
struct print_data {
	char buffer[MAX_MESSAGE_SIZE]; /**< Buffer array */
} __attribute__ ((packed, aligned(256)));

/** @endcond */ /* DISABLED_IN_SAFETY_BUILD */

static inline uint32_t dla_response(uint32_t msg, uint32_t cmd, uint32_t error)
{
	return ((msg & (uint32_t)DLA_RESPONSE_MSG_MASK) |
		(cmd << (uint32_t)DLA_RESPONSE_CMD_SHIFT) |
		(error << (uint32_t)DLA_RESPONSE_ERROR_SHIFT));
}

static inline uint32_t dla_command(uint32_t method_id)
{
	return (method_id & (uint32_t)DLA_METHOD_ID_CMD_MASK);
}

static inline uint32_t is_int_on_complete(uint32_t method_id)
{
	return ((method_id >> DLA_INT_ON_COMPLETE_SHIFT) & 0x1U);
}

static inline uint32_t is_int_on_error(uint32_t method_id)
{
	return ((method_id >> DLA_INT_ON_ERROR_SHIFT) & 0x1U);
}

#endif
