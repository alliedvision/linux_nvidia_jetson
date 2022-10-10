/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef NVGPU_FALCON_H
#define NVGPU_FALCON_H

/**
 * @file
 * @page unit-falcon Unit Falcon
 *
 * Overview
 * ========
 *
 * The falcon unit is responsible for managing falcon engines/controllers
 * that provide the base support for GPU functions such as context
 * switch (FECS/GPCCS), power/perf management (PMU), secure load
 * of other falcons (ACR). These GPU functions are executed by
 * uCode which runs on each falcon.
 *
 * The falcon unit provides interfaces to nvgpu driver to access falcon
 * controller through following interfaces:
 *
 *   + Falcon internal registers.
 *     + Intrerrupt registers.
 *     + Mailbox registers.
 *     + Memory control registers etc.
 *   + IMEM (Instruction memory), DMEM (Data memory), EMEM (External memory).
 *
 * Data Structures
 * ===============
 *
 * The data structure exposed to users of the Falcon unit is:
 *
 *   + struct nvgpu_falcon
 *
 *       struct nvgpu_falcon defines a Falcon's software state. It contains the
 *       hardware ID, base address for registers access, memory access locks,
 *       engine specific functions.
 *
 * Static Design
 * =============
 *
 * Falcon Initialization
 * ---------------------
 * Before accessing the falcon's registers and memory for various tasks like
 * loading the firmwares or check the falcon's status, nvgpu driver needs to
 * initialize the falcon software state. This sets up the base address for
 * falcon register access, initializes the memory access locks and links
 * the hardware specific functions.
 *
 * Falcon Teardown
 * ---------------
 * While powering down the device, falcon software state that is setup by
 * #nvgpu_falcon_sw_init is destroyed.
 *
 * External APIs
 * -------------
 *   + nvgpu_falcon_sw_init()
 *   + nvgpu_falcon_sw_free()
 *
 * Dynamic Design
 * ==============
 *
 * General operation
 * -----------------
 *   + During nvgpu driver power on, various falcons are initialized with
 *     #nvgpu_falcon_sw_init (PMU, SEC2, NVDEC, GSPLITE, FECS) and then
 *     ACR is initialized.
 *   + ACR HS ucode is bootstrapped using #nvgpu_falcon_hs_ucode_load_bootstrap.
 *
 * Sequence for loading any uCode on the falcon
 * --------------------------------------------
 *   + Reset the falcon.
 *   + Setup falcon apertures and boot configuration.
 *   + Copy secure/non-secure code to IMEM and data to DMEM.
 *   + Update mailbox registers as required for ACK or reading capabilities.
 *   + Bootstrap falcon.
 *   + Wait for halt.
 *   + Read mailbox registers.
 *
 * External APIs
 * -------------
 *   + nvgpu_falcon_reset()
 *   + nvgpu_falcon_wait_for_halt()
 *   + nvgpu_falcon_wait_idle()
 *   + nvgpu_falcon_mem_scrub_wait()
 *   + nvgpu_falcon_copy_to_dmem()
 *   + nvgpu_falcon_copy_to_imem()
 *   + nvgpu_falcon_mailbox_read()
 *   + nvgpu_falcon_mailbox_write()
 *   + nvgpu_falcon_hs_ucode_load_bootstrap()
 *   + nvgpu_falcon_get_id()
 *   + nvgpu_falcon_set_irq()
 */

#include <nvgpu/types.h>
#include <nvgpu/lock.h>
#include <nvgpu/static_analysis.h>

/** Falcon ID for PMU engine */
#define FALCON_ID_PMU       (0U)
/** Falcon ID for GSPLITE engine */
#define FALCON_ID_GSPLITE   (1U)
/** Falcon ID for FECS engine */
#define FALCON_ID_FECS      (2U)
/** Falcon ID for GPCCS engine */
#define FALCON_ID_GPCCS     (3U)
/** Falcon ID for NVDEC engine */
#define FALCON_ID_NVDEC     (4U)
/** Falcon ID for SEC2 engine */
#define FALCON_ID_SEC2      (7U)
/** Falcon ID for MINION engine */
#define FALCON_ID_MINION    (10U)
#define FALCON_ID_PMU_NEXT_CORE (13U)
#define FALCON_ID_END	    (15U)
#define FALCON_ID_INVALID   0xFFFFFFFFU

#define FALCON_MAILBOX_0	0x0U
#define FALCON_MAILBOX_1	0x1U
/** Total Falcon mailbox registers */
#define FALCON_MAILBOX_COUNT	0x02U
/** Falcon IMEM block size in bytes */
#define FALCON_BLOCK_SIZE	0x100U

/** NVRISCV BR completion time check in ms*/
#define NVRISCV_BR_COMPLETION_TIMEOUT_NON_SILICON_MS   10000U
#define NVRISCV_BR_COMPLETION_TIMEOUT_SILICON_MS       100U
#define NVRISCV_BR_COMPLETION_POLLING_TIME_INTERVAL_MS 5U

#define GET_IMEM_TAG(IMEM_ADDR) ((IMEM_ADDR) >> 8U)

#define GET_NEXT_BLOCK(ADDR) \
	(((nvgpu_safe_add_u32((ADDR), (FALCON_BLOCK_SIZE - 1U)) \
		& ~(FALCON_BLOCK_SIZE-1U)) / FALCON_BLOCK_SIZE) << 8U)

/**
 * Falcon ucode header format
 *   OS Code Offset
 *   OS Code Size
 *   OS Data Offset
 *   OS Data Size
 *   NumApps (N)
 *   App   0 Code Offset
 *   App   0 Code Size
 *   .  .  .  .
 *   App   N - 1 Code Offset
 *   App   N - 1 Code Size
 *   App   0 Data Offset
 *   App   0 Data Size
 *   .  .  .  .
 *   App   N - 1 Data Offset
 *   App   N - 1 Data Size
 *   OS Ovl Offset
 *   OS Ovl Size
 */
#define OS_CODE_OFFSET 0x0U
#define OS_CODE_SIZE   0x1U
#define OS_DATA_OFFSET 0x2U
#define OS_DATA_SIZE   0x3U
#define NUM_APPS       0x4U
#define APP_0_CODE_OFFSET 0x5U
#define APP_0_CODE_SIZE   0x6U

/**
 * Falcon/Falcon2 fuse settings bit
 */
#define FCD            (0U)
#define FENEN          (1U)
#define NVRISCV_BRE_EN (2U)
#define NVRISCV_DEVD   (3U)
#define NVRISCV_PLD    (4U)
#define DCS            (5U)
#define NVRISCV_SEN    (6U)
#define NVRISCV_SA     (7U)
#define NVRISCV_SH     (8U)
#define NVRISCV_SI     (9U)
#define SECURE_DBGD    (10U)
#define AES_ALGO_DIS   (11U)
#define PKC_ALGO_DIS   (12U)

struct gk20a;
struct nvgpu_falcon;

/**
 * Falcon memory types.
 */
enum falcon_mem_type {
	/** Falcon data memory */
	MEM_DMEM = 0,
	/** Falcon instruction memory */
	MEM_IMEM
};

#ifdef CONFIG_NVGPU_FALCON_DEBUG
/*
 * Structure tracking information relevant to firmware debug buffer.
 */
struct nvgpu_falcon_dbg_buf {
	/* Offset to debug buffer in NVRISCV DMEM */
	u32 dmem_offset;

	/*
	 * Pointer to local debug buffer copy on system memory
	 * where nvgpu copy the data from NVRISCV DMEM.
	 */
	u8 *local_buf;

	/* Last read offset for the circular debug buffer */
	u32 read_offset;

	/* Read/Write offset register addresses */
	u32 read_offset_address;
	u32 write_offset_address;

	/* Flcn debug buffer size */
	u32 buffer_size;

	/* Set once nvgpu get the first message from FLCN */
	bool first_msg_received;

	/* flag to print buffer when PMU error occurs */
	bool is_prints_as_err;
};
#endif

/**
 * This struct holds the falcon ops which are falcon engine specific.
 */
struct nvgpu_falcon_engine_dependency_ops {
	/** reset function specific to engine */
	int (*reset_eng)(struct gk20a *g);
	/** Falcon bootstrap config function specific to engine */
	void (*setup_bootstrap_config)(struct gk20a *g);
	/** copy functions for SEC2 falcon engines on dGPU that supports EMEM */
	int (*copy_from_emem)(struct gk20a *g, u32 src, u8 *dst,
		u32 size, u8 port);
	int (*copy_to_emem)(struct gk20a *g, u32 dst, u8 *src,
		u32 size, u8 port);
};

/**
 * This struct holds the software state of the underlying falcon engine.
 * Falcon interfaces rely on this state. This struct is updated/used
 * through the interfaces provided, by the common.init, common.acr
 * and the common.pmu units.
 */
struct nvgpu_falcon {
	/** The GPU driver struct */
	struct gk20a *g;
	/** Falcon ID for the engine */
	u32 flcn_id;
	/** Base address to access falcon registers */
	u32 flcn_base;
	/** Base address to access nextcore registers */
	u32 flcn2_base;
	/** Indicates if the falcon is supported and initialized for use. */
	bool is_falcon_supported;
	/** Indicates if the falcon2 is enabled or not. */
	bool is_falcon2_enabled;
	/** Indicates if the falcon interrupts are enabled. */
	bool is_interrupt_enabled;
	/** Fuse settings */
	unsigned long fuse_settings;
	/** Lock to access the falcon's IMEM. */
	struct nvgpu_mutex imem_lock;
	/** Lock to access the falcon's DMEM. */
	struct nvgpu_mutex dmem_lock;
	/** Indicates if the falcon supports EMEM. */
	bool emem_supported;
	/** Lock to access the falcon's EMEM. */
	struct nvgpu_mutex emem_lock;
	/** Functions for engine specific reset and memory access. */
	struct nvgpu_falcon_engine_dependency_ops flcn_engine_dep_ops;
#ifdef CONFIG_NVGPU_FALCON_DEBUG
	struct nvgpu_falcon_dbg_buf debug_buffer;
#endif
};

/**
 * @brief Read the falcon register.
 *
 * @param flcn [in] The falcon.
 * @param offset [in] offset of the register.
 *
 * This function is called to read a register with common falcon offset.
 *
 * Steps:
 * - Read and return data from register at \a offset from the base of
 *   \a flcn.
 *
 * @return register data.
 */
u32 nvgpu_falcon_readl(struct nvgpu_falcon *flcn, u32 offset);

/**
 * @brief Write the falcon register.
 *
 * @param flcn [in] The falcon.
 * @param offset [in] Index of the register.
 * @param data [in] Data to be written to the register.
 *
 * This function is called to write to a register with common falcon offset.
 *
 * Steps:
 * - Write \a data to register at \a offet from the base of the \a flcn.
 */
void nvgpu_falcon_writel(struct nvgpu_falcon *flcn,
                                       u32 offset, u32 val);

/**
 * @brief Reset the falcon CPU or Engine.
 *
 * @param flcn [in] The falcon.
 *
 * This function is invoked to reset the falcon CPU before loading ACR uCode
 * on the PMU falcon.
 *
 * Steps:
 * - Validate that the passed in falcon struct is not NULL and is for supported
 *   falcon. If not valid, return -EINVAL.
 * - Do the falcon \a flcn reset through cpuctl register if not being
 *   controlled by an engine
 * - Else do engine dependent reset.
 * - Wait for ther memory scrub completion using function
 *   #nvgpu_falcon_mem_scrub_wait and return the status.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ETIMEDOUT in case the timeout expired waiting for memory scrub.
 */
int nvgpu_falcon_reset(struct nvgpu_falcon *flcn);

/**
 * @brief Wait for the falcon CPU to be halted.
 *
 * @param flcn [in] The falcon.
 * @param timeout [in] Duration to wait for halt.
 *
 * This function is invoked after bootstrapping PMU falcon with ACR uCode to
 * ascertain the successful boot of the ACR uCode.
 *
 * Steps:
 * - Validate that the passed in falcon struct is not NULL and is for supported
 *   falcon. If not valid, return -EINVAL.
 * - Initialize the timer using function #nvgpu_timeout_init with passed in
 *   duration \a timeout. Verify the timeout initialization and return error
 *   if failed.
 * - While the timeout is not expired, check the falcon halt status from
 *   cpuctl register every 10us.
 * - Return value based on timeout expiry.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ETIMEDOUT in case the timeout expired waiting for halt.
 */
int nvgpu_falcon_wait_for_halt(struct nvgpu_falcon *flcn, unsigned int timeout);

/**
 * @brief Wait for the falcon to be idle.
 *
 * @param flcn [in] The falcon.
 *
 * This function is invoked during PMU engine reset flow after enabling PMU.
 *
 * Steps:
 * - Validate that the passed in falcon struct is not NULL and is for supported
 *   falcon. If not valid, return -EINVAL.
 * - Initialize the timer using function #nvgpu_timeout_init with the duration
 *   of minimum of 200ms in the form of #NVGPU_TIMER_RETRY_TIMER. Verify the
 *   timeout initialization and return error if failed.
 * - While the timeout is not expired, check the falcon units' idle status
 *   from idlestate register every 100-200us.
 * - Return value based on timeout expiry.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ETIMEDOUT in case the timeout expired waiting for idle.
 */
int nvgpu_falcon_wait_idle(struct nvgpu_falcon *flcn);

/**
 * @brief Wait for the falcon memory scrub.
 *
 * @param flcn [in] The falcon.
 *
 * This function is invoked after resetting the falcon or PMU engine.
 *
 * Steps:
 * - Validate that the passed in falcon struct is not NULL and is for supported
 *   falcon. If not valid, return -EINVAL.
 * - Initialize the timer using function #nvgpu_timeout_init with the duration
 *   of minimum of 1ms in the form of #NVGPU_TIMER_RETRY_TIMER. Verify the
 *   timeout initialization and return error if failed.
 * - While the timeout is not expired, check the falcon memory scrubbing status
 *   from dmactrl register every 10us.
 * - Return value based on timeout expiry.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ETIMEDOUT in case the timeout expired waiting for scrub completion.
 */
int nvgpu_falcon_mem_scrub_wait(struct nvgpu_falcon *flcn);

/**
 * @brief Copy data to falcon's DMEM.
 *
 * @param flcn [in] The falcon.
 * @param dst  [in] Address in the DMEM (Block and offset). Should be aligned
 *		    at 4-bytes.
 *		    - Min: 0
 *		    - Max: size of DMEM from hwcfg register - 1
 * @param src  [in] Source data to be copied to DMEM.
 * @param size [in] Size in bytes of the source data.
 *		    - Min: 1
 *		    - Max: size of DMEM from hwcfg register
 * @param port [in] DMEM port to be used for copy.
 *		    - Min: 0
 *		    - Max: maximum number of DMEM ports from hwcfg register - 1
 *
 * This function is used to copy uCode data to falcon's DMEM while bootstrapping
 * the uCode on the falcon.
 *
 * Steps:
 * - Validate that the passed in falcon struct is not NULL and is for supported
 *   falcon. If not valid, return -EINVAL.
 * - Validate the parameters \a dst, \a size, \a port for DMEM alignment and
 *   size restrictions. If not valid, return -EINVAL.
 * - Acquire DMEM copy lock.
 * - Copy data \a src of \a size though \a port at offset \a dst of DMEM.
 *   - Set \a dst offset and AINCW (auto increment on write) bit in DMEMC
 *     (control) register corresponding to the port \a port.
 *   - Write the data words from \a src to DMEMD (data) register word-by-word.
 *   - Write the remaining bytes to DMEMD register zeroing non-data bytes.
 *   - Read the DMEMC register and verify the count of bytes written.
 * - Release DMEM copy lock.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -EINVAL if #nvgpu_falcon is invalid.
 * @retval -EIO if data write fails to DMEM.
 */
int nvgpu_falcon_copy_to_dmem(struct nvgpu_falcon *flcn,
	u32 dst, u8 *src, u32 size, u8 port);

/**
 * @brief Copy data to falcon's IMEM.
 *
 * @param flcn [in] The falcon.
 * @param dst  [in] Address in the IMEM (Block and offset). Should be aligned
 *		    at 4-bytes.
 *		    - Min: 0
 *		    - Max: size of IMEM from hwcfg register - 1
 * @param src  [in] Source data to be copied to IMEM.
 * @param size [in] Size in bytes of the source data.
 *		    - Min: 1
 *		    - Max: size of IMEM from hwcfg register
 * @param port [in] IMEM port to be used for copy.
 *		    - Min: 0
 *		    - Max: maximum number of IMEM ports from hwcfg register - 1
 * @param sec  [in] Indicates if blocks are to be marked as secure.
 * @param tag  [in] Tag to be set for the blocks.
 *
 * This function is used to copy uCode instructions to falcon's IMEM while
 * bootstrapping the uCode on the falcon.
 *
 * Steps:
 * - Validate that the passed in falcon struct is not NULL and is for supported
 *   falcon. If not valid, return -EINVAL.
 * - Validate the parameters \a dst, \a size, \a port for IMEM alignment and
 *   size restrictions. If not valid, return -EINVAL.
 * - Acquire IMEM copy lock.
 * - Copy data \a src of \a size though \a port at offset \a dst of IMEM.
 *   - Set \a dst offset and AINCW (auto increment on write) bit in IMEMC
 *     (control) register corresponding to the port \a port. Set the secure
 *     bit based on \a sec.
 *   - Write the data words from \a src to IMEMD (data) register word-by-word.
 *   - Write \a tag every 256B (64 words). Increment the tag.
 *   - Zero the remaining bytes in the last 256B block (if total size is
 *     not multiple of 256B block) by writing zero to IMEMD register.
 * - Release IMEM copy lock.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -EINVAL if #nvgpu_falcon is invalid.
 */
int nvgpu_falcon_copy_to_imem(struct nvgpu_falcon *flcn,
	u32 dst, u8 *src, u32 size, u8 port, bool sec, u32 tag);

/**
 * @brief Read the falcon mailbox register.
 *
 * @param flcn [in] The falcon.
 * @param mailbox_index [in] Index of the mailbox register.
 *			     - Min: 0
 *			     - Max: #FALCON_MAILBOX_COUNT - 1
 *
 * This function is called to know the ACR bootstrap status.
 *
 * Steps:
 * - Validate that the passed in falcon struct is not NULL and is for supported
 *   falcon. If not valid, return 0.
 * - Validate that the passed in \a mailbox_index < #FALCON_MAILBOX_COUNT.
 *   If not, return 0.
 * - Read and return data from mailbox register \a mailbox_index of the falcon
 *   \a flcn.
 *
 * @return register data in case of success, 0 in case of failure.
 * @retval 0 if #nvgpu_falcon is invalid.
 * @retval 0 if mailbox index is wrong.
 */
u32 nvgpu_falcon_mailbox_read(struct nvgpu_falcon *flcn, u32 mailbox_index);

/**
 * @brief Write the falcon mailbox register.
 *
 * @param flcn [in] The falcon.
 * @param mailbox_index [in] Index of the mailbox register.
 *			     - Min: 0
 *			     - Max: #FALCON_MAILBOX_COUNT - 1
 * @param data [in] Data to be written to mailbox register.
 *
 * This function is called to update the mailbox register that will get
 * written with the ACR bootstrap status by ACR uCode.
 *
 * Steps:
 * - Validate that the passed in falcon struct is not NULL and is for supported
 *   falcon. If not valid, return.
 * - Validate that the passed in \a mailbox_index < #FALCON_MAILBOX_COUNT.
 *   If not, return.
 * - Write \a data to mailbox register \a mailbox_index of the falcon \a flcn.
 */
void nvgpu_falcon_mailbox_write(struct nvgpu_falcon *flcn, u32 mailbox_index,
	u32 data);

/**
 * @brief Bootstrap the falcon with HS ucode.
 *
 * @param flcn  [in] The falcon.
 * @param ucode [in] ucode to be copied.
 * @param ucode_header [in] ucode header.
 *
 * This function is called during nvgpu power on to bootstrap ACR uCode by
 * setting up IMEM and DMEM with required uCode data.
 *
 * Steps:
 * - Validate that the passed in falcon struct is not NULL and is for supported
 *   falcon. If not valid, return.
 * - Reset the falcon with #nvgpu_falcon_reset. If failed, return the error.
 * - Setup the virtual and physical apertures, context interface attributes &
 *   instance block address.
 * - Copy non-secure OS code and HS ucode source to IMEM and descriptor to DMEM.
 *   If any errors, return the errors.
 * - Write non-zero value to falcon mailbox register 0. This register will be
 *   read for polling the bootstrap completion status set in this register by
 *   ucode.
 * - Bootstrap the falcon. If failed, return the error.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -EINVAL if #nvgpu_falcon is invalid.
 * @retval -ETIMEDOUT if engine reset times out.
 */
int nvgpu_falcon_hs_ucode_load_bootstrap(struct nvgpu_falcon *flcn, u32 *ucode,
	u32 *ucode_header);

/**
 * @brief Get the falcon ID.
 *
 * @param flcn [in] The falcon.
 *
 * This function is called during ACR bootstrap to get the ID of the falcon
 * for debug purpose.
 *
 * Steps:
 * - Return the falcon ID of the falcon \a flcn.
 *
 * @return the falcon ID of \a flcn.
 * @retval Falcon ID of a flcn.
 */
u32 nvgpu_falcon_get_id(struct nvgpu_falcon *flcn);

/**
 * @brief Get the reference to falcon struct in GPU driver struct.
 *
 * @param g [in] The GPU driver struct.
 * @param flcn_id [id] falcon ID.
 *		       - Supported IDs are:
 *		         - #FALCON_ID_PMU
 *		         - #FALCON_ID_GSPLITE
 *		         - #FALCON_ID_FECS
 *		         - #FALCON_ID_GPCCS
 *		         - #FALCON_ID_NVDEC
 *		         - #FALCON_ID_SEC2
 *		         - #FALCON_ID_MINION
 *
 * This function is called to get the falcon struct for validation during
 * init/free.
 *
 * Steps:
 * - For valid falcon ID \a flcn_id, return the address of falcon struct in the
 *   struct \a g.
 * - For invalid falcon ID, return NULL.
 *
 * @return the falcon struct of \a g corresponding to \a flcn_id.
 * @retval falcon struct of \a g corresponding to \a flcn_id.
 */
struct nvgpu_falcon *nvgpu_falcon_get_instance(struct gk20a *g, u32 flcn_id);

/**
 * @brief Initialize the falcon software state.
 *
 * @param g [in] The GPU driver struct.
 * @param flcn_id [id] falcon ID. See #nvgpu_falcon_get_instance for supported
 *		       values.
 *
 * This function is called during nvgpu power on to initialize various
 * falcons.
 *
 * Steps:
 * - Validate the passed in \a flcn_id and return -ENODEV if not valid.
 *   If valid, get the reference to the falcon struct in \a g.
 * - Initialize the falcon struct fields with \a flcn_id and \a g.
 * - Set falcon specific state values such as falcon base, support status,
 *   interrupt status and engine dependent ops.
 * - Initializes locks for memory (IMEM, DMEM, EMEM) copy operations as
 *   applicable.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ENODEV if falcon ID is invalid.
 * @retval -EINVAL if GPU ID is invalid.
 */
int nvgpu_falcon_sw_init(struct gk20a *g, u32 flcn_id);

/**
 * @brief Free the falcon software state.
 *
 * @param g [in] The GPU driver struct.
 * @param flcn_id [in] falcon ID. See #nvgpu_falcon_get_instance for supported
 *		       values.
 *
 * This function is called during nvgpu power off to deinitialize falcons
 * initialized during power on.
 *
 * Steps:
 * - Validate the passed in \a flcn_id and return if not valid.
 *   If valid, get the reference to the falcon struct in \a g.
 * - Mark the falcon as not supported.
 * - Destroy the locks created for memory copy operations.
 */
void nvgpu_falcon_sw_free(struct gk20a *g, u32 flcn_id);

/**
 * @brief Set the falcon interrupt mask and routing registers.
 *
 * @param flcn [in] The falcon.
 * @param enable [in] Indicates if interrupt mask is to be set or cleared and
 *		      if interrupt routing register is to be set.
 * @param intr_mask [in] Interrupt mask to be set when interrupts are to be
 *			 enabled. Not applicable when interrupts are to be
 *			 disabled.
 * @param intr_dest [in] Interrupt routing to be set when interrupts are to
 *			 be enabled. Not applicable when interrupts are to be
 *			 disabled.
 *
 * This function is called during nvgpu power on to enable the falcon ECC
 * interrupt and called during nvgpu power off to disable the falcon interrupts.
 *
 * Steps:
 * - Validate the passed in \a flcn and return if not valid.
 * - Check if interrupts are supported on the falcon and return if not
 *   supported.
 * - If enabling the interrupts:
 *   - Set falcon_falcon_irqmset_r() register with the value \a intr_mask.
 *   - Set falcon_falcon_irqdest_r() register with the value \a intr_dest.
 * - Else if disabling the interrupts:
 *   - Set falcon_falcon_irqmclr_r() register with the value 0xffffffffU;
 */
void nvgpu_falcon_set_irq(struct nvgpu_falcon *flcn, bool enable,
	u32 intr_mask, u32 intr_dest);

/**
 * @brief Get the size of falcon's memory.
 *
 * @param flcn [in] The falcon.
 * @param type [in] Falcon memory type (IMEM, DMEM).
 *		    - Supported types: MEM_DMEM (0), MEM_IMEM (1)
 * @param size [out] Size of the falcon memory type.
 *
 * This function is called to get the size of falcon's memory for validation
 * while copying to IMEM/DMEM.
 *
 * Steps:
 * - Validate that the passed in falcon struct is not NULL and is for supported
 *   falcon. If not valid, return -EINVAL.
 * - Read the size of the falcon memory of \a type in bytes from the HW config
 *   register in output parameter \a size.
 *
 * @return 0 in case of success, < 0 in case of failure.
 */
int nvgpu_falcon_get_mem_size(struct nvgpu_falcon *flcn,
			      enum falcon_mem_type type, u32 *size);

bool nvgpu_falcon_is_falcon2_enabled(struct nvgpu_falcon *flcn);
bool nvgpu_falcon_is_feature_supported(struct nvgpu_falcon *flcn,
		u32 feature);

int nvgpu_falcon_wait_for_nvriscv_brom_completion(struct nvgpu_falcon *flcn);

#ifdef CONFIG_NVGPU_DGPU
int nvgpu_falcon_copy_from_emem(struct nvgpu_falcon *flcn,
	u32 src, u8 *dst, u32 size, u8 port);
int nvgpu_falcon_copy_to_emem(struct nvgpu_falcon *flcn,
	u32 dst, u8 *src, u32 size, u8 port);
#endif

#ifdef CONFIG_NVGPU_FALCON_DEBUG
void nvgpu_falcon_dump_stats(struct nvgpu_falcon *flcn);
#endif

#if defined(CONFIG_NVGPU_FALCON_DEBUG) || defined(CONFIG_NVGPU_FALCON_NON_FUSA)
int nvgpu_falcon_copy_from_dmem(struct nvgpu_falcon *flcn,
	u32 src, u8 *dst, u32 size, u8 port);
#endif

#ifdef CONFIG_NVGPU_FALCON_NON_FUSA
/**
 * @brief Bootstrap the falcon.
 *
 * @param flcn [in] The falcon.
 * @param boot_vector [in] Address to start the falcon execution.
 *
 * This function is called after setting up IMEM and DMEM with uCode
 * instructions and data to start the execution.
 *
 * Steps:
 * - Validate that the passed in falcon struct is not NULL and is for supported
 *   falcon. If not valid, return -EINVAL.
 * - Set the boot vector address, DMA control and start the falcon CPU
 *   execution.
 *
 * @return 0 in case of success, < 0 in case of failure.
 */
int nvgpu_falcon_bootstrap(struct nvgpu_falcon *flcn, u32 boot_vector);

int nvgpu_falcon_clear_halt_intr_status(struct nvgpu_falcon *flcn,
		unsigned int timeout);
int nvgpu_falcon_copy_from_imem(struct nvgpu_falcon *flcn,
	u32 src, u8 *dst, u32 size, u8 port);
void nvgpu_falcon_print_dmem(struct nvgpu_falcon *flcn, u32 src, u32 size);
void nvgpu_falcon_print_imem(struct nvgpu_falcon *flcn, u32 src, u32 size);
void nvgpu_falcon_get_ctls(struct nvgpu_falcon *flcn, u32 *sctl, u32 *cpuctl);
#endif

#ifdef CONFIG_NVGPU_FALCON_DEBUG
#define NV_RISCV_DEBUG_BUFFER_QUEUE   7U
#define NV_RISCV_DMESG_BUFFER_SIZE    0x1000U

/**
 * @brief falcon debug buffer initialization.
 *
 * @param flcn [in] The falcon.
 *
 * Allocates and maps buffer in system memory for sharing flcn firmware
 * debug prints with client nvgpu.
 *
 * @return '0' if initialization is successful, error otherwise.
 */
int nvgpu_falcon_dbg_buf_init(struct nvgpu_falcon *flcn,
	u32 debug_buffer_max_size, u32 write_reg_addr, u32 read_reg_addr);

/*
 * @brief falcon debug buffer deinitialization.
 *
 * @param flcn [in] The falcon.
 *
 * Frees falcon debug buffer from memory.
 *
 */
void nvgpu_falcon_dbg_buf_destroy(struct nvgpu_falcon *flcn);

/**
 * @brief Display falcon firmware logs
 *
 * @param flcn [in] The falcon.
 *
 * This function reads the contents of flcn debug buffer filled by firmware.
 * Logs are displayed line-by-line with label '<FLCN> Async' signifying that
 * these logs might be delayed and should be assumed as out-of-order when read
 * alongside other client nvgpu logs.
 *
 * @return '0' if contents logged successfully, error otherwise.
 */
int nvgpu_falcon_dbg_buf_display(struct nvgpu_falcon *flcn);

/**
 * @brief Enable/Disable falcon error print support
 *
 * @param flcn [in] The falcon.
 * @param enable [in] true/false value to enable/disable error print
 * support.
 *
 * This function sets the flag with true/false which respectively
 * enables or disables the falcon error print support. This is used to
 * print pc trace values when error is hit.
 *
 */
void nvgpu_falcon_dbg_error_print_enable(struct nvgpu_falcon *flcn, bool enable);

#endif

/**
 *  The falcon unit debugging macro
 */
#define nvgpu_falcon_dbg(g, fmt, args...) \
	nvgpu_log(g, gpu_dbg_falcon, fmt, ##args)

#endif /* NVGPU_FALCON_H */
