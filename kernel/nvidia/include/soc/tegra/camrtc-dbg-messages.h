/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2022, NVIDIA CORPORATION.  All rights reserved.
 */

#ifndef INCLUDE_CAMRTC_DBG_MESSAGES_H
#define INCLUDE_CAMRTC_DBG_MESSAGES_H

#include "camrtc-common.h"

#pragma GCC diagnostic error "-Wpadded"

/*
 * Message identifiers.
 */
#define CAMRTC_REQ_PING                 MK_U32(0x01) /* Ping request. */
#define CAMRTC_REQ_PM_SLEEP             MK_U32(0x02) /* Never implemented */
#define CAMRTC_REQ_MODS_TEST            MK_U32(0x03) /* Run MODS test */
#define CAMRTC_REQ_SET_LOGLEVEL         MK_U32(0x04) /* Set log level */
#define CAMRTC_REQ_LOGLEVEL             CAMRTC_REQ_SET_LOGLEVEL
#define CAMRTC_REQ_RTOS_STATE           MK_U32(0x05) /* Get FreeRTOS state */
#define CAMRTC_REQ_READ_MEMORY_32BIT    MK_U32(0x06) /* Read memory */
#define CAMRTC_REQ_READ_MEMORY          MK_U32(0x07)
#define CAMRTC_REQ_SET_PERF_COUNTERS    MK_U32(0x08) /* ARM Performance counter */
#define CAMRTC_REQ_GET_PERF_COUNTERS    MK_U32(0x09)
#define CAMRTC_REQ_GET_LOGLEVEL         MK_U32(0x0A)
#define CAMRTC_REQ_RUN_TEST             MK_U32(0x0B) /* Run functional test (obsolete) */
#define CAMRTC_REQ_GET_TASK_STAT        MK_U32(0x0C)
#define CAMRTC_REQ_ENABLE_VI_STAT       MK_U32(0x0D)
#define CAMRTC_REQ_GET_VI_STAT          MK_U32(0x0E)
#define CAMRTC_REQ_GET_MEM_USAGE        MK_U32(0x0F)
#define CAMRTC_REQ_RUN_MEM_TEST         MK_U32(0x10) /* Run functional test */
#define CAMRTC_REQ_GET_IRQ_STAT         MK_U32(0x11)
#define CAMRTC_REQ_SET_FALCON_COVERAGE  MK_U32(0x12)
#define CAMRTC_REQ_GET_COVERAGE_SUPPORT MK_U32(0x13)
#define CAMRTC_REQUEST_TYPE_MAX         MK_U32(0x14)

/* MODS test cases */
#define CAMRTC_MODS_TEST_BASIC		MK_U32(0x00) /* Basic MODS tests */
#define CAMRTC_MODS_TEST_DMA		MK_U32(0x01) /* MODS DMA test */

/* Deprecated */
#define CAMRTC_RESP_PONG                CAMRTC_REQ_PING
#define CAMRTC_RESP_PM_SLEEP            CAMRTC_REQ_PM_SLEEP
#define CAMRTC_RESP_MODS_RESULT         CAMRTC_REQ_MODS_TEST
#define CAMRTC_RESP_LOGLEVEL            CAMRTC_REQ_SET_LOGLEVEL
#define CAMRTC_RESP_RTOS_STATE          CAMRTC_REQ_RTOS_STATE
#define CAMRTC_RESP_READ_MEMORY_32BIT   CAMRTC_REQ_READ_MEMORY_32BIT
#define CAMRTC_RESP_READ_MEMORY         CAMRTC_REQ_READ_MEMORY
#define CAMRTC_RESP_SET_PERF_COUNTERS   CAMRTC_REQ_SET_PERF_COUNTERS
#define CAMRTC_RESP_GET_PERF_COUNTERS   CAMRTC_REQ_GET_PERF_COUNTERS

/* Return statuses */
#define CAMRTC_STATUS_OK		MK_U32(0)
#define CAMRTC_STATUS_ERROR		MK_U32(1) /* Generic error */
#define CAMRTC_STATUS_REQ_UNKNOWN	MK_U32(2) /* Unknown req_type */
#define CAMRTC_STATUS_NOT_IMPLEMENTED	MK_U32(3) /* Request not implemented */
#define CAMRTC_STATUS_INVALID_PARAM	MK_U32(4) /* Invalid parameter */

#define CAMRTC_DBG_FRAME_SIZE		MK_U32(448)
#define CAMRTC_DBG_MAX_DATA		MK_U32(440)
#define CAMRTC_DBG_TASK_STAT_MAX	MK_U32(16)

/*
 * This struct is used to query or set the wake timeout for the target.
 * Fields:
 * force_entry:	when set forces the target to sleep for a set time
 */
struct camrtc_pm_data {
	uint32_t force_entry;
};

/* This struct is used to send the loop count to perform the mods test
 * on the target.
 * Fields:
 * mods_loops:	number of times mods test should be run
 */
struct camrtc_mods_data {
	uint32_t mods_case;
	uint32_t mods_loops;
	uint32_t mods_dma_channels;
};

/* This struct is used to extract the firmware version of the RTCPU.
 * Fields:
 * data:	buffer to store the version string. Uses uint8_t
 */
struct camrtc_ping_data {
	uint64_t ts_req;		/* requestor timestamp */
	uint64_t ts_resp;		/* response timestamp */
	uint8_t data[64];		/* data */
};

struct camrtc_log_data {
	uint32_t level;
};

struct camrtc_rtos_state_data {
	uint8_t rtos_state[CAMRTC_DBG_MAX_DATA];	/* string data */
};

/* This structure is used to read 32 bit data from firmware address space.
 * Fields:
 *   addr: address to read from. should be 4 byte aligned.
 *   data: 32 bit value read from memory.
 */
struct camrtc_dbg_read_memory_32bit {
	uint32_t addr;
};

struct camrtc_dbg_read_memory_32bit_result {
	uint32_t data;
};

#define CAMRTC_DBG_READ_MEMORY_COUNT_MAX	MK_U32(256)

/* This structure is used to read memory in firmware address space.
 * Fields:
 *   addr: starting address. no alignment requirement
 *   count: number of bytes to read. limited to CAMRTC_DBG_READ_MEMORY_COUNT_MAX
 *   data: contents read from memory.
 */
struct camrtc_dbg_read_memory {
	uint32_t addr;
	uint32_t count;
};

struct camrtc_dbg_read_memory_result {
	uint8_t data[CAMRTC_DBG_READ_MEMORY_COUNT_MAX];
};

#define CAMRTC_DBG_MAX_PERF_COUNTERS		MK_U32(31)

/* This structure is used to set event type that each performance counter
 * will monitor. This doesn't include fixed performance counter. If there
 * are 4 counters available, only 3 of them are configurable.
 * Fields:
 *   number: Number of performance counters to set.
 *     This excludes a fixed performance counter: cycle counter
 *   do_reset: Whether to reset counters
 *   cycle_counter_div64: Whether to enable cycle counter divider
 *   events: Event type to monitor
 */
struct camrtc_dbg_set_perf_counters {
	uint32_t number;
	uint32_t do_reset;
	uint32_t cycle_counter_div64;
	uint32_t events[CAMRTC_DBG_MAX_PERF_COUNTERS];
};

/* This structure is used to get performance counters.
 * Fields:
 *   number: Number of performance counters.
 *     This includes a fixed performance counter: cycle counter
 *   counters: Descriptors of event counters. First entry is for cycle counter.
 *     event: Event type that the value represents.
 *       For first entry, this field is don't care.
 *     value: Value of performance counter.
 */
struct camrtc_dbg_get_perf_counters_result {
	uint32_t number;
	struct {
		uint32_t event;
		uint32_t value;
	} counters[CAMRTC_DBG_MAX_PERF_COUNTERS];
};


#define CAMRTC_DBG_MAX_TEST_DATA (CAMRTC_DBG_MAX_DATA - sizeof(uint64_t))

/* This structure is used pass textual input data to functional test
 * case and get back the test output, including verdict.
 *
 * Fields:
 *   timeout: maximum time test may run in nanoseconds
 *      data: textual data (e.g., test name, verdict)
 */
struct camrtc_dbg_run_test_data {
	uint64_t timeout;	/* Time in nanoseconds */
	char data[CAMRTC_DBG_MAX_TEST_DATA];
};

/* Number of memory areas */
#define CAMRTC_DBG_NUM_MEM_TEST_MEM MK_U32(8)

#define CAMRTC_DBG_MAX_MEM_TEST_DATA (\
	CAMRTC_DBG_MAX_DATA \
	- sizeof(uint64_t) - sizeof(struct camrtc_dbg_streamids) \
	- (sizeof(struct camrtc_dbg_test_mem) * CAMRTC_DBG_NUM_MEM_TEST_MEM))

struct camrtc_dbg_test_mem {
	uint32_t size;
	uint32_t page_size;
	uint64_t phys_addr;
	uint64_t rtcpu_iova;
	uint64_t vi_iova;
	uint64_t vi2_iova;
	uint64_t isp_iova;
};

struct camrtc_dbg_streamids {
	uint8_t rtcpu;
	uint8_t vi;
	uint8_t vi2;
	uint8_t isp;
};

/* This structure is used pass memory areas and textual input data to
 * functional test case and get back the test output, including
 * verdict.
 *
 * Fields:
 *   timeout: maximum time test may run in nanoseconds
 *     mem[]: address and size of memory areas passed to the test
 *      data: textual data (e.g., test name, verdict)
 */
struct camrtc_dbg_run_mem_test_data {
	uint64_t timeout;	/* Time in nanoseconds */
	struct camrtc_dbg_test_mem mem[CAMRTC_DBG_NUM_MEM_TEST_MEM];
	struct camrtc_dbg_streamids streamids;
	char data[CAMRTC_DBG_MAX_MEM_TEST_DATA];
};

/* This structure is used get information on system tasks.
 * Fields:
 *   n_task: number of reported tasks
 *   total_count: total runtime
 *   task: array of reported tasks
 *     id: task name
 *     count: runtime allocated to task
 *     number: unique task number
 *     priority: priority of task when this structure was populated
 */
struct camrtc_dbg_task_stat {
	uint32_t n_task;
	uint32_t total_count;
	struct {
		uint32_t id[2];
		uint32_t count;
		uint32_t number;
		uint32_t priority;
	} task[CAMRTC_DBG_TASK_STAT_MAX];
};

/* Limit for default CAMRTC_DBG_FRAME_SIZE */
#define CAMRTC_DBG_NUM_IRQ_STAT			MK_U32(11)

/*
 * This structure is used get information on interrupts.
 *
 * Fields:
 *   n_active: number of active interrupts
 *   total_called: total number of interrupts handled
 *   total_runtime: total runtime
 *   n_irq: number of reported interrupts
 *   irqs: array of reported tasks
 *     irq_num: irq number
 *     num_called: times this interrupt has been handled
 *     runtime: runtime for this interrupt
 *     name: name of the interrupt (may not be NUL-terminated)
 */
struct camrtc_dbg_irq_stat {
	uint32_t n_active;
	uint32_t n_irq;
	uint64_t total_called;
	uint64_t total_runtime;
	struct {
		uint32_t irq_num;
		char name[12];
		uint64_t runtime;
		uint32_t max_runtime;
		uint32_t num_called;
	} irqs[CAMRTC_DBG_NUM_IRQ_STAT];
};

/* These structure is used to get VI message statistics.
 * Fields:
 *   enable: enable/disable collecting vi message statistics
 */
struct camrtc_dbg_enable_vi_stat {
	uint32_t enable;
};

/* These structure is used to get VI message statistics.
 * Fields:
 *   avg: running average of VI message latency.
 *   max: maximum VI message latency observed so far.
 */
struct camrtc_dbg_vi_stat {
	uint32_t avg;
	uint32_t max;
};

/* These structure is used to get memory usage.
 * Fields:
 *   text: code memory usage
 *   bss: global/static memory usage.
 *   data: global/static memory usage.
 *   heap: heap memory usage.
 *   stack: cpu stack memory usage.
 *   free: remaining free memory.
 */
struct camrtc_dbg_mem_usage {
	uint32_t text;
	uint32_t bss;
	uint32_t data;
	uint32_t heap;
	uint32_t stack;
	uint32_t free_mem;
};

#define CAMRTC_DBG_FALCON_ID_VI       MK_U32(0x00)
#define CAMRTC_DBG_FALCON_ID_ISP      MK_U32(0x80)

/* This structure is used to set falcon code coverage configuration data.
 * Fields:
 *   falcon_id: Which falcon to set up the coverage for.
 *   flush: Flush coverage data action bit.
 *   reset: Reset coverage data action bit. If flush is also set, it runs first.
 *   size: Size of the coverage data buffer.
 *   iova: Address of the coverage data buffer in falcon IOVA space.
 *
 * NOTE: Setting iova and/or size to 0 will disable coverage.
 */
struct camrtc_dbg_coverage_data {
	uint8_t falcon_id;
	uint8_t flush;
	uint8_t reset;
	uint8_t pad__;
	uint32_t size;
	uint64_t iova;
};

/* This structure is used to reply code coverage status.
 * Fields:
 *   falcon_id: Which falcon the status is for
 *   enabled: Coverage output is configured properly and enabled
 *   full: Coverage output buffer is full
 *   bytes_written: Bytes written to buffer so far.
 */
struct camrtc_dbg_coverage_stat {
	uint8_t falcon_id;
	uint8_t enabled;
	uint8_t full;
	uint8_t pad__;
	uint32_t bytes_written;
};

/* This struct encapsulates the type of the request and the respective
 * data associated with that request.
 * Fields:
 * req_type:	indicates the type of the request be it pm related,
 *		mods or ping.
 * data:	Union of structs of all the request types.
 */
struct camrtc_dbg_request {
	uint32_t req_type;
	uint32_t reserved;
	union {
		struct camrtc_pm_data	pm_data;
		struct camrtc_mods_data mods_data;
		struct camrtc_ping_data ping_data;
		struct camrtc_log_data	log_data;
		struct camrtc_dbg_read_memory_32bit rm_32bit_data;
		struct camrtc_dbg_read_memory rm_data;
		struct camrtc_dbg_set_perf_counters set_perf_data;
		struct camrtc_dbg_run_test_data run_test_data;
		struct camrtc_dbg_run_mem_test_data run_mem_test_data;
		struct camrtc_dbg_enable_vi_stat enable_vi_stat;
		struct camrtc_dbg_coverage_data coverage_data;
	} data;
};

/* This struct encapsulates the type of the response and the respective
 * data associated with that response.
 * Fields:
 * resp_type:	indicates the type of the response be it pm related,
 *		mods or ping.
 * status:	response in regard to the request i.e success/failure.
 *		In case of mods, this field is the result.
 * data:	Union of structs of all the request/response types.
 */
struct camrtc_dbg_response {
	uint32_t resp_type;
	uint32_t status;
	union {
		struct camrtc_pm_data pm_data;
		struct camrtc_ping_data ping_data;
		struct camrtc_log_data log_data;
		struct camrtc_rtos_state_data rtos_state_data;
		struct camrtc_dbg_read_memory_32bit_result rm_32bit_data;
		struct camrtc_dbg_read_memory_result rm_data;
		struct camrtc_dbg_get_perf_counters_result get_perf_data;
		struct camrtc_dbg_run_test_data run_test_data;
		struct camrtc_dbg_run_mem_test_data run_mem_test_data;
		struct camrtc_dbg_task_stat task_stat_data;
		struct camrtc_dbg_vi_stat vi_stat;
		struct camrtc_dbg_mem_usage mem_usage;
		struct camrtc_dbg_irq_stat irq_stat;
		struct camrtc_dbg_coverage_stat coverage_stat;
	} data;
};

#pragma GCC diagnostic ignored "-Wpadded"

#endif /* INCLUDE_CAMRTC_DBG_MESSAGES_H */
