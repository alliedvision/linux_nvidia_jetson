/*
 * Copyright (c) 2016-2023, NVIDIA CORPORATION. All rights reserved.
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

#ifndef PVA_INTERFACE_H
#define PVA_INTERFACE_H

#include <pva-bit.h>
#include <pva-version.h>
#include <pva-fw-version.h>
#include <pva-types.h>
#include <pva-errors.h>

/*
 * Register definition for PVA_SHRD_SMP_STA0
 *
 * This is used to communicate various bits of information between the
 * OS and the PVA.
 */

/*
 * Bits set by the OS and examined by the R5
 */
#define PVA_BOOT_INT PVA_BIT(31U) /* OS wants an interrupt */
#define PVA_OS_PRINT PVA_BIT(30U) /* OS will process print */
#define PVA_TEST_WAIT PVA_BIT(29U) /* R5 wait to start tests */
#define PVA_TEST_RUN PVA_BIT(28U) /* Start tests */
#define PVA_WAIT_DEBUG PVA_BIT(24U) /* Spin-wait early in boot */
#define PVA_CG_DISABLE PVA_BIT(20U) /* Disable PVA clock gating */
#define PVA_VMEM_RD_WAR_DISABLE PVA_BIT(19U) /* Disable VMEM RD fail WAR */
#define PVA_VMEM_MBX_WAR_ENABLE PVA_BIT(18U) /* WAR for Bug 2090939 enabled*/

/*
 * Bits set by the R5 and examined by the OS
 */
#define PVA_TESTS_STARTED PVA_BIT(10U) /* PVA Tests started */
#define PVA_TESTS_PASSED PVA_BIT(9U) /* PVA Tests passed */
#define PVA_TESTS_FAILED PVA_BIT(8U) /* PVA Tests failed */
#define PVA_HALTED PVA_BIT(2U) /* PVA uCode halted */
#define PVA_BOOT_DONE PVA_BIT(1U) /* PVA is "ready" */
#define PVA_TEST_MODE PVA_BIT(0U) /* PVA is in "test mode" */

/*
 * Symbolic definitions of the mailbox registers (rather than using 0-7)
 */
#define PVA_MBOX_COMMAND 0U
#define PVA_MBOX_ADDR 1U
#define PVA_MBOX_LENGTH 2U
#define PVA_MBOX_ARG 3U
#define PVA_MBOX_SIDE_CHANNEL_HOST_WR 4U
#define PVA_MBOX_AISR 5U
#define PVA_MBOX_SIDE_CHANNEL_HOST_RD 6U
#define PVA_MBOX_ISR 7U

/*
 * For using the mailboxes as a status interface, we overload them
 */
#define PVA_MBOX_STATUS4 1U
#define PVA_MBOX_STATUS5 2U
#define PVA_MBOX_STATUS6 3U
#define PVA_MBOX_STATUS7 4U

/*
 * Mailbox side channel bit definitions
 */
#define PVA_SIDE_CHANNEL_MBOX_BIT 0U
#define PVA_SIDE_CHANNEL_MBOX_BIT_MASK (~(1U << PVA_SIDE_CHANNEL_MBOX_BIT))

/*
 * Code checking the version of the R5 uCode should check
 * the values returned from the R5_VERSION subcommand of
 * CMD_GET_STATUS to determine if the version currently
 * running on the PVA's R5 is compatible with what the
 * driver was compiled against.
 */
#define PVA_R5_VERSION                                                         \
	PVA_MAKE_VERSION(0, PVA_VERSION_MAJOR, PVA_VERSION_MINOR,              \
			 PVA_VERSION_SUBMINOR)

/*
 * PVA interrupt status register contained in PVA_MBOX_ISR.
 */
#define PVA_INT_PENDING PVA_BIT(31U)
#define PVA_READY PVA_BIT(30U)
#define PVA_BUSY PVA_BIT(29U)
#define PVA_CMD_COMPLETE PVA_BIT(28U)
#define PVA_CMD_ERROR PVA_BIT(27U)
#define PVA_VALID_STATUS7 PVA_BIT(26U)
#define PVA_VALID_STATUS6 PVA_BIT(25U)
#define PVA_VALID_STATUS5 PVA_BIT(24U)
#define PVA_VALID_STATUS4 PVA_BIT(23U)
#define PVA_VALID_STATUS3 PVA_BIT(22U)

#define PVA_VALID_CCQ_ISR PVA_BIT(20U)
#define PVA_VALID_CCQ_AISR PVA_BIT(24U)
#define PVA_CCQ_OVERFLOW PVA_BIT(28U)

/*
 * On T23X we pack the ISR in with the ERR code
 */
#define PVA_STATUS_ISR_MSB 31
#define PVA_STATUS_ISR_LSB 16
#define PVA_STATUS_ERR_MSB 15
#define PVA_STATUS_ERR_LSB 0

/*
 * PVA interrupt status register contained in PVA_MBOX_AISR
 */
#define PVA_AISR_INT_PENDING PVA_BIT(31U)
#define PVA_AISR_TASK_COMPLETE PVA_BIT(30U)
#define PVA_AISR_TASK_ERROR PVA_BIT(29U)
#define PVA_AISR_ABORT PVA_BIT(0U)

#define PVA_STATUS_AISR_TASK_ID_MSB (8U)
#define PVA_STATUS_AISR_TASK_ID_LSB (1U)
#define PVA_STATUS_AISR_VPU_ID_MSB (9U)
#define PVA_STATUS_AISR_VPU_ID_LSB (9U)
#define PVA_STATUS_AISR_QUEUE_MSB (12U)
#define PVA_STATUS_AISR_QUEUE_LSB (10U)
#define PVA_STATUS_AISR_ERR_MSB (28U)
#define PVA_STATUS_AISR_ERR_LSB (13U)

#define PVA_PACK_AISR_STATUS(e, q, v, t) (PVA_INSERT(e, PVA_STATUS_AISR_ERR_MSB,\
				PVA_STATUS_AISR_ERR_LSB) \
				| PVA_INSERT(q, PVA_STATUS_AISR_QUEUE_MSB, \
				PVA_STATUS_AISR_QUEUE_LSB) \
				| PVA_INSERT(v, PVA_STATUS_AISR_VPU_ID_MSB, \
				PVA_STATUS_AISR_VPU_ID_LSB) \
				| PVA_INSERT(t, PVA_STATUS_AISR_TASK_ID_MSB, \
				PVA_STATUS_AISR_TASK_ID_LSB))
#define PVA_GET_QUEUE_ID_FROM_STATUS(_s_) PVA_EXTRACT((_s_), \
					PVA_STATUS_AISR_QUEUE_MSB, \
					PVA_STATUS_AISR_QUEUE_LSB, \
					uint8_t)
#define PVA_GET_ERROR_FROM_STATUS(_s_) PVA_EXTRACT((_s_), \
					PVA_STATUS_AISR_ERR_MSB, \
					PVA_STATUS_AISR_ERR_LSB, \
					uint16_t)
#define PVA_GET_VPU_ID_FROM_STATUS(_s_) PVA_EXTRACT((_s_), \
					PVA_STATUS_AISR_VPU_ID_MSB, \
					PVA_STATUS_AISR_VPU_ID_LSB, \
					uint8_t)
#define PVA_GET_TASK_ID_FROM_STATUS(_s_) PVA_EXTRACT((_s_), \
					PVA_STATUS_AISR_TASK_ID_MSB, \
					PVA_STATUS_AISR_TASK_ID_LSB, \
					uint8_t)

#define PVA_GET_ERROR_CODE(_s_) PVA_EXTRACT((_s_), 15U, 0U, pva_errors_t)

/*
 * Commands that can be sent to the PVA through the PVA_SHRD_MBOX
 * interface.
 */
typedef uint8_t pva_cmds_t;
#define CMD_GET_STATUS		0U
#define CMD_SUBMIT		1U
#define CMD_ABORT_QUEUE		2U
#define CMD_NOOP		3U
#define CMD_SW_BIST		4U
#define CMD_GET_VPU_STATS	5U
#define CMD_SET_LOGGING		6U
#define CMD_NEXT		7U /* Must be last */

/*
 * CMD_GET_STATUS subcommands
 */
typedef uint8_t pva_status_cmds_t;
#define R5_VERSION 0U
#define PVA_UPTIME 1U
#define COMPLETED_TASK 2U
#define GET_STATUS_NEXT 3U /* Deleted RUNNING TASKS as it is not used in FW */

/*
 * CCQ FIFO SUBMIT interface definition
 */
#define PVA_ADDR_LOWER_32BITS_MSB       (63U)
#define PVA_ADDR_LOWER_32BITS_LSB       (32U)
#define PVA_QUEUE_ID_MSB                (28U)
#define PVA_QUEUE_ID_LSB                (24U)
#define PVA_BATCH_SIZE_MSB              (23U)
#define PVA_BATCH_SIZE_LSB              (16U)
#define PVA_ADDR_HIGHER_8BITS_MSB       (15U)
#define PVA_ADDR_HIGHER_8BITS_LSB       (8U)
#define PVA_CMD_ID_MSB                  (7U)
#define PVA_CMD_ID_LSB                  (0U)

/*
 * Macros to indicate LSB and MSB of SUBCOMMAND field in a command
 */
#define PVA_SUB_CMD_ID_MSB                  (15U)
#define PVA_SUB_CMD_ID_LSB                  (8U)
/*
 * Defenitions used in CMD_SET_STATUS_BUFFER
 */
#define PVA_CMD_STATUS_BUFFER_LENGTH_MSB   (27U)
#define PVA_CMD_STATUS_BUFFER_LENGTH_LSB   (16U)

/*
 * Macro used to indicate the most significant
 * bit to extract higher 8 bits of the 40 bit address
 */
#define PVA_EXTRACT_ADDR_HIGHER_8BITS_MSB		39U
/*
 * Macro used to indicate the least significant
 * bit to extract higher 8 bits of the 40 bit address
 */
#define PVA_EXTRACT_ADDR_HIGHER_8BITS_LSB		32U

/**
 * Macro used to specify most significant bit
 * of the VPU stats enable field in CMD_SET_VPU_STATS_BUFFER command
 */
#define PVA_CMD_VPU_STATS_EN_MSB		23U
/**
 * Macro used to specify least significant bit
 * of the VPU stats enable field in CMD_SET_VPU_STATS_BUFFER command
 */
#define PVA_CMD_VPU_STATS_EN_LSB		16U

/*
 * SW Bist subcommands
 */
#define PVA_SDL_SUBMIT 0xF1U
#define PVA_SDL_SET_ERROR_INJECT_SDL 0xF2U
#define PVA_SDL_SET_ERROR_INJECT_PANIC 0xF3U

/*
 * Generic fields in a command sent to the PVA through the PVA_SHRD_MBOX
 * interface.
 */
#define PVA_CMD_INT_ON_ERR PVA_BIT(30U)
#define PVA_CMD_INT_ON_COMPLETE PVA_BIT(29U)
#define PVA_GET_BATCH_SIZE(_c_, _t_) PVA_EXTRACT(_c_, PVA_BATCH_SIZE_MSB, PVA_BATCH_SIZE_LSB, _t_)
#define PVA_SET_BATCH_SIZE(_c_) PVA_INSERT(_c_, PVA_BATCH_SIZE_MSB, PVA_BATCH_SIZE_LSB)
#define PVA_GET_SUBCOMMAND(_c_, _t_) PVA_EXTRACT(_c_, PVA_SUB_CMD_ID_MSB, PVA_SUB_CMD_ID_LSB, _t_)
#define PVA_SET_SUBCOMMAND(_c_) PVA_INSERT(_c_, PVA_SUB_CMD_ID_MSB, PVA_SUB_CMD_ID_LSB)
#define PVA_GET_COMMAND(_c_) PVA_EXTRACT(_c_, PVA_CMD_ID_MSB, PVA_CMD_ID_LSB, pva_cmds_t)
#define PVA_SET_COMMAND(_c_) PVA_INSERT(_c_, PVA_CMD_ID_MSB, PVA_CMD_ID_LSB)

/*
 * Generic fields in a command sent through the command FIFO interface.
 */
#define PVA_FIFO_GET_COMMAND(_c_)                                              \
	PVA_EXTRACT64_RANGE((_c_), PVA_CCQ_CMD, pva_cmds_t)
#define PVA_CMD_MBOX_TO_FIFO_FLAG_SHIFT 29U
#define PVA_FIFO_INT_ON_ERR PVA_BIT64(1U)
#define PVA_FIFO_INT_ON_COMPLETE PVA_BIT64(0U)

/*
 * Reserved bits in mbox3 used and consumed internally by R5
 */
#define PVA_MBOX3_RESERVED_SOURCE_INTERFACE_MSB 31
#define PVA_MBOX3_RESERVED_SOURCE_INTERFACE_LSB 24

/*
 * On T23X we map 4x32bit pushes to the CCQ to our mailbox command structure
 * CCQ is delivered in 64bit chunks. This defines the mapping into each of the
 * 64bit chunks.
 */
/* First 64bit write */
#define PVA_CCQ_FIRST_PUSH_MBOX_0_MSB 31
#define PVA_CCQ_FIRST_PUSH_MBOX_0_LSB 0

#define PVA_CCQ_FIRST_PUSH_MBOX_1_MSB 63
#define PVA_CCQ_FIRST_PUSH_MBOX_1_LSB 32
/* Second 64bit write */
#define PVA_CCQ_SECOND_PUSH_MBOX_2_MSB 31
#define PVA_CCQ_SECOND_PUSH_MBOX_2_LSB 0

#define PVA_CCQ_SECOND_PUSH_MBOX_3_MSB 63
#define PVA_CCQ_SECOND_PUSH_MBOX_3_LSB 32

/*
 * Structure for managing commands through PVA_SHRD_MBOX*
 */
struct pva_cmd_s {
	uint32_t cmd_field[4];
};

struct pva_vpu_stats_s {
	/**
	 * @brief The accumulated VPU utilization time in the current window.
	 */
	uint64_t total_utilization_time[2];
	/**
	 * @brief The timestamp which signifies start of the current window.
	 */
	uint64_t window_start_time;
	/**
	 * @brief The timestamp of end of the current window.
	 */
	uint64_t window_end_time;
} __packed;

/*
 * CMD_NOOP command
 */
#define PVA_CMD_FL_NOOP_ECHO PVA_BIT(28U)
#define PVA_CMD_FL_NOOP_ERROR PVA_BIT(27U)

static inline uint32_t pva_cmd_noop(struct pva_cmd_s *const cmd,
				    const uint32_t echo_data,
				    const uint32_t status_reg,
				    const uint32_t flags)
{
	cmd->cmd_field[0] = flags | PVA_SET_SUBCOMMAND(status_reg) |
		       PVA_SET_COMMAND(CMD_NOOP);
	cmd->cmd_field[1] = echo_data;

	return 2U;
}

/*
 * CMD_GET_STATUS
 * Not used directly.
 */
static inline uint32_t pva_cmd_get_status(const pva_status_cmds_t subcommand,
					  const uint32_t flags)
{
	return flags | PVA_SET_SUBCOMMAND(subcommand) |
	       PVA_SET_COMMAND(CMD_GET_STATUS);
}

/*
 * R5_VERSION get status command
 */
struct pva_status_R5_version_s {
	uint32_t cur_version;
	uint32_t oldest_version;
	uint32_t change_id;
	uint32_t build_date;
};

static inline uint32_t pva_cmd_R5_version(struct pva_cmd_s *const cmd,
					  const uint32_t flags)
{
	cmd->cmd_field[0] = pva_cmd_get_status(R5_VERSION, flags);
	return 1U;
}

/*
 * PVA_UPTIME get status command
 */
struct pva_status_pva_uptime_s {
	uint32_t uptime_lo;
	uint32_t uptime_hi;
};

static inline uint32_t pva_cmd_pva_uptime(struct pva_cmd_s *const cmd,
					  const pva_vpu_id_t vpu,
					  const uint32_t flags)
{
	(void) vpu; /*For Future use*/
	cmd->cmd_field[0] = pva_cmd_get_status(PVA_UPTIME, flags);
	return 1U;
}

/*
 * COMPLETED_TASK get status command
 */
struct pva_status_completed_task_s {
	uint32_t task_addr_lo;
	uint32_t task_addr_hi;
	uint32_t task_error;
	uint32_t task_queue_vpu;
};

static inline uint32_t pva_cmd_completed_task(struct pva_cmd_s *const cmd,
					      const uint32_t flags)
{
	cmd->cmd_field[0] = pva_cmd_get_status(COMPLETED_TASK, flags);
	return 1U;
}

/*
 * CMD_SET_LOGGING
 */

#define PVA_CMD_FL_LOG_PVA_ENABLE PVA_BIT(28U)
#define PVA_CMD_FL_LOG_R5_ENABLE PVA_BIT(27U)
#define PVA_CMD_FL_LOG_VPU_ENABLE PVA_BIT(26U)
#define PVA_CMD_FL_LOG_NO_OVERFLOW PVA_BIT(25U)
#define PVA_CMD_FL_LOG_OVERFLOW_INT PVA_BIT(24U)
#define PVA_CMD_FL_PRT_PVA_ENABLE PVA_BIT(23U)
#define PVA_CMD_FL_PRT_R5_ENABLE PVA_BIT(22U)
#define PVA_CMD_FL_PRT_VPU_ENABLE PVA_BIT(21U)
#define PVA_CMD_FL_PRT_NO_OVERFLOW PVA_BIT(20U)
#define PVA_CMD_FL_PRT_OVERFLOW_INT PVA_BIT(19U)

static inline uint32_t pva_cmd_set_logging_level(struct pva_cmd_s *const cmd,
						 const uint32_t pva_log_level,
						 const uint32_t flags)
{
	cmd->cmd_field[0] = flags | PVA_SET_COMMAND(CMD_SET_LOGGING);
	cmd->cmd_field[1] = PVA_INSERT(pva_log_level, 31U, 0U);
	return 2U;
}

/*
 * CMD_SUBMIT (batch mode)
 */
static inline uint32_t pva_cmd_submit_batch(struct pva_cmd_s *const cmd,
					    const uint8_t queue_id,
					    const uint64_t addr,
					    const uint8_t batch_size,
					    const uint32_t flags)
{
	cmd->cmd_field[0] =
		flags | PVA_SET_COMMAND(CMD_SUBMIT) |
		PVA_INSERT(batch_size, PVA_BATCH_SIZE_MSB, PVA_BATCH_SIZE_LSB) |
		PVA_INSERT(PVA_EXTRACT64(addr, PVA_EXTRACT_ADDR_HIGHER_8BITS_MSB,
					PVA_EXTRACT_ADDR_HIGHER_8BITS_LSB, uint32_t),
					PVA_ADDR_HIGHER_8BITS_MSB, PVA_ADDR_HIGHER_8BITS_LSB) |
		PVA_INSERT(queue_id, PVA_QUEUE_ID_MSB, PVA_QUEUE_ID_LSB);
	cmd->cmd_field[1] = PVA_LOW32(addr);
	return 2U;
}

/*
 * CMD_SUBMIT (single task)
 */
static inline uint32_t pva_cmd_submit(struct pva_cmd_s *const cmd,
				      const uint8_t queue_id,
				      const uint64_t addr, const uint32_t flags)
{
	return pva_cmd_submit_batch(cmd, queue_id, addr, 0U, flags);
}

/*
 * CMD_SW_BIST
 */
static inline uint32_t pva_cmd_sw_bist(struct pva_cmd_s *const cmd,
				       const uint32_t bist_cmd,
				       const uint32_t inject_error,
				       const uint32_t flags)
{
	cmd->cmd_field[0] = flags | PVA_SET_COMMAND(CMD_SW_BIST) |
		       PVA_SET_SUBCOMMAND(bist_cmd);
	cmd->cmd_field[1] = (inject_error == 1) ? 0xAAAAAAAA : 0xBBBBBBBB;
	return 2U;
}

/*
 * CMD_ABORT_QUEUE
 */
static inline uint32_t pva_cmd_abort_task(struct pva_cmd_s *const cmd,
					  const uint8_t queue_id,
					  const uint32_t flags)
{
	cmd->cmd_field[0] = flags | PVA_SET_COMMAND(CMD_ABORT_QUEUE) |
		       PVA_SET_SUBCOMMAND(queue_id);
	return 1U;
}

/*
 * CMD_SET_VPU_STATS
 */
static inline uint32_t
pva_cmd_get_vpu_stats(struct pva_cmd_s * const cmd,
		      const uint64_t addr,
		      const uint32_t flags,
			  const uint8_t value)
{
	cmd->cmd_field[0] = flags
		       | PVA_SET_COMMAND(CMD_GET_VPU_STATS)
		       | PVA_INSERT(PVA_EXTRACT64(addr, PVA_EXTRACT_ADDR_HIGHER_8BITS_MSB,
				    PVA_EXTRACT_ADDR_HIGHER_8BITS_LSB, uint32_t),
				    PVA_ADDR_HIGHER_8BITS_MSB, PVA_ADDR_HIGHER_8BITS_LSB)
			   | PVA_INSERT(value, PVA_CMD_VPU_STATS_EN_MSB, PVA_CMD_VPU_STATS_EN_LSB);
	cmd->cmd_field[1] = PVA_LOW32(addr);

	return 2U;
}
#endif
