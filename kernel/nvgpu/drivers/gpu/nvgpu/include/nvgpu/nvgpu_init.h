/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_INIT_H
#define NVGPU_INIT_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_ref;

/**
 * @file
 * @page unit-init Unit Init
 *
 * Overview
 * ========
 *
 * The Init unit is called by OS (QNX, Linux) to initialize or teardown the
 * driver. The Init unit ensures all the other sub-units are initialized so the
 * driver is able to provide general functionality to the application.
 *
 * Static Design
 * =============
 *
 * HAL Initialization
 * ------------------
 * The HAL must be initialized before the nvgpu_finalize_poweron() is called.
 * This is accomplished by calling nvgpu_detect_chip() which will determine
 * which GPU is in the system and configure the HAL interfaces.
 *
 * Common Initialization
 * ---------------------
 * The main driver initialization occurs by calling nvgpu_finalize_poweron()
 * which will initialize all of the common units in the driver and must be done
 * before the driver is ready to provide full functionality.
 *
 * Common Teardown
 * ---------------
 * If the GPU is unused, the driver can be torn down by calling
 * nvgpu_prepare_poweroff().
 *
 * External APIs
 * -------------
 * + nvgpu_detect_chip() - Called to initialize the HAL.
 * + nvgpu_finalize_poweron() - Called to initialize nvgpu driver.
 * + nvgpu_prepare_poweroff() - Called before powering off GPU HW.
 * + nvgpu_init_gpu_characteristics() - Called during HAL init for enable flag
 * processing.
 *
 * Dynamic Design
 * ==============
 * After initialization, the Init unit provides a number of APIs to track state
 * and usage count.
 *
 * External APIs
 * -------------
 * + nvgpu_get() / nvgpu_put() - Maintains ref count for usage.
 * + nvgpu_can_busy() - Check to make sure driver is ready to go busy.
 * + nvgpu_check_gpu_state() - Restart if the state is invalid.
 */

/**
 * @brief Initial driver initialization
 *
 * @param g [in] The GPU
 *
 * Initializes device and grmgr subunits in the early stage of
 * GPU power on sequence. This separate routine is required to create
 * the GPU dev node in the early stage of GPU power on sequence.
 * Each sub-unit is responsible for HW initialization.
 *
 * nvgpu poweron sequence split into two stages:
 * - nvgpu_early_init() - Initializes the sub units
 *   which are required to be initialized before the grgmr init.
 *   For creating dev node, grmgr init and its dependency unit
 *   needs to move to early stage of GPU power on.
 *   After successful nvgpu_early_init() sequence,
 *   NvGpu can indetify the number of MIG instance required
 *   for each physical GPU.
 * - nvgpu_finalize_poweron() - Initializes the sub units which
 *   can be initialized at the later stage of GPU power on sequence.
 *
 * grmgr init depends on the following HAL sub units,
 * device - To get the device caps.
 * priv_ring - To get the gpc count and other MIG config programming.
 * fifo_reset_hw - In simulation/emulation/GPU standalone platform,
 *                 XBAR, L2 and HUB are enabled during
 *                 g->ops.fifo.reset_enable_hw(). This introduces a
 *                 dependency to get the MIG map conf information.
 *                 (if nvgpu_is_bpmp_running() == false treated as
 *                 simulation/emulation/GPU standalone platform).
 * fb - MIG config programming.
 * ltc - MIG config programming.
 * bios, bus, ecc and clk - dependent module of priv_ring/fb/ltc.
 *
 * @return 0 in case of success, < 0 in case of failure.
 */
int nvgpu_early_poweron(struct gk20a *g);

/**
 * @brief Final driver initialization
 *
 * @param g [in] The GPU
 *
 * Initializes GPU units in the GPU driver. Each sub-unit is responsible for HW
 * initialization.
 *
 * Note: Requires the GPU is already powered on and the HAL is initialized.
 *
 * @return 0 in case of success, < 0 in case of failure.
 */
int nvgpu_finalize_poweron(struct gk20a *g);

/**
 * @brief Prepare driver for poweroff
 *
 * @param g [in] The GPU
 *
 * Prepare the driver subsystems and HW for powering off GPU.
 *
 * @return 0 in case of success, < 0 in case of failure.
 */
int nvgpu_prepare_poweroff(struct gk20a *g);

/**
 * @brief Enter SW Quiesce state
 *
 * @param g [in] The GPU
 *
 * Enters SW quiesce state:
 * - set sw_quiesce_pending: When this flag is set, interrupt
 *   handlers exit after masking interrupts. This should help mitigate
 *   an interrupt storm.
 * - wake up thread to complete quiescing.
 *
 * The thread performs the following:
 * - set NVGPU_DRIVER_IS_DYING to prevent allocation of new resources
 * - disable interrupts
 * - disable fifo scheduling
 * - preempt all runlists
 * - set error notifier for all active channels
 *
 * @note: For channels with usermode submit enabled, userspace can
 * still ring doorbell, but this will not trigger any work on
 * engines since fifo scheduling is disabled.
 */
void nvgpu_sw_quiesce(struct gk20a *g);

/**
 * @brief Cleanup SW Quiesce state
 *
 * @param g [in] The GPU
 *
 * If SW Quiesce was previously initialized:
 * - Stop the quiesce thread.
 * - Destroy cond object.
 * - Mark Quiesce as uninitialized.
 */
void nvgpu_sw_quiesce_remove_support(struct gk20a *g);

/**
 * @brief Start GPU idle
 *
 * @param g [in] The GPU
 *
 * Set #NVGPU_DRIVER_IS_DYING to prevent allocation of new resources.
 * User API call will fail once this flag is set, as gk20a_busy will fail.
 */
void nvgpu_start_gpu_idle(struct gk20a *g);

/**
 * @brief Enable interrupt handlers
 * This API also creates stall, non-stall and priority threads to process
 * different kind of interrupts.
 *
 * @param g [in] The GPU
 *
 * - Steps to enable interrupt handlers :
 *   - Set flag nonstall_work to 0.
 *   - Initialize sw_irq_stall condition variable. If return code is non zero
 *     return error.
 *   - Initialize sw_irq_nonstall condition variable. If return code is non
 *     zero return error.
 *   - Initialize nonstall_cond condition variable. If return code is non zero
 *     return error.
 *   - Create nvgpu_nonstall_worker thread. It keeps on waiting for channel
 *     semaphore. If enable to create a thread return error.
 *   - Create a nvgpu_intr_stall thread with the NVGPU_INTR_PRIORITY
 *     priority. This is a blocking thread which keeps on waiting for stall
 *     intrerrupts and process the work if any stall intrerrupt gets generate.
 *     irqs. If return code is non zero return error.
 *   - If disable_nonstall_intr is not set then create nonstall_irq_thread
 *     with the NVGPU_INTR_PRIORITY priority. This is a blocking thread which
 *     keeps on waiting for non_stall intrerrupts and process the work if any
 *     non_stall intrerrupt gets generate.
 *   - set r->irq_requested to true once all the irqs are enabled successfully.
 *   - In case of failure start cleanup in the reverse order of init.
 *
 *  @return 0 in case of success, <0 in case of failure.
 *  @retval ENOMEM is returned if (1) Insufficient memory to initialize the
 *  condition variable attribute object attr, (2) All kernel synchronization
 *  objects are in use, or there wasn't enough memory to initialize the condvar.
 *  @retval EBUSY is returned if the cond argument pointer to a previously
 *  initialized condition variable that hasn't been destroyed.
 *  @retval EINVAL is returned if invalid value attr passed.
 *  @retval EFAULT is returned if (1) A fault occurred when the kernel tried to
 *  access cond or attr, (2) An error occurred trying to access the buffers or
 *  the start_routine provided.
 *  @retval EAGAIN is returned if insufficient system resources to create thread.
 *  @retval Other error codes if pthread_attr_destroy() fails.
 */
int nvgpu_enable_irqs(struct gk20a *g);

/**
 * @brief Disable interrupt handlers
 *
 * @param g [in] The GPU
 *
 * - Steps to disable irqs.
 *   - Stop stall irq thread.
 *   - If disable_nonstall_intr not enabled stop nonstall irq thread.
 *   - Stop irq worker thread.
 *   - Destroy condition sw_irq_stall_last_handled_cond and
 *     sw_irq_nonstall_last_handled_cond variables.
 *   - Set irq_requested to false.
 *
 * @return none.
 */
void nvgpu_disable_irqs(struct gk20a *g);

/** Signify current state of nvgpu driver */
#define NVGPU_STATE_POWERED_OFF		0U
#define NVGPU_STATE_POWERING_ON		1U
#define NVGPU_STATE_POWERED_ON		2U

/**
 * @brief Set the nvgpu power state
 *
 * @param g [in] The GPU
 * @param state Power state
 *
 * Set the nvgpu power state.
 */
void nvgpu_set_power_state(struct gk20a *g, u32 state);

/**
 * @brief Get the nvgpu power state
 *
 * @param g [in] The GPU
 *
 * Returns the nvgpu power state.
 */
const char *nvgpu_get_power_state(struct gk20a *g);

/**
 * @brief Get whether power state is
 *  NVGPU_STATE_POWERING_ON or NVGPU_STATE_POWERED_ON
 *
 * @param g [in] The GPU
 */
bool nvgpu_poweron_started(struct gk20a *g);

/**
 * @brief Return the nvgpu power-on state
 *
 * @param g [in] The GPU
 *
 * Return true if nvgpu is in powered on state, else false.
 */
bool nvgpu_is_powered_on(struct gk20a *g);

/**
 * @brief Return the nvgpu power-off state
 *
 * @param g [in] The GPU
 *
 * Return true if nvgpu is in powered off state, else false.
 */
bool nvgpu_is_powered_off(struct gk20a *g);

/**
 * @brief Check if the device can go busy
 *
 * @param g [in] The GPU
 *
 * @return 1 if it ok to go busy, 0 if it is not ok to go busy.
 */
int nvgpu_can_busy(struct gk20a *g);

/**
 * @brief Increment ref count on driver.
 *
 * @param g [in] The GPU
 *
 * This will fail if the driver is in the process of being released.
 *
 * @return pointer to g if successful, otherwise 0.
 */
struct gk20a * nvgpu_get(struct gk20a *g);

/**
 * @brief Decrement ref count on driver.
 *
 * @param g [in] The GPU
 *
 * Will free underlying driver memory if driver is no longer in use.
 */
void nvgpu_put(struct gk20a *g);

/**
 * @brief Check driver state and enter quiesce if the state is invalid
 *
 * @param g [in] The GPU
 *
 * If driver state is invalid, makes call to enter quiesce state.
 */
void nvgpu_check_gpu_state(struct gk20a *g);

/**
 * @brief Configure initial GPU "enable" state and setup SM arch.
 *
 * @param g [in] The GPU
 *
 * This is called during HAL initialization.
 *
 * @return 0 in case of success, < 0 in case of failure.
 */
int nvgpu_init_gpu_characteristics(struct gk20a *g);

/**
 * @brief Takes a reference for keeping gpu busy but not try to initialize it.
 * Does nothing for safety.
 *
 * @param g [in] The GPU
 */
void gk20a_busy_noresume(struct gk20a *g);

/**
 * @brief Drops a reference for gpu. Does nothing for safety.
 *
 * @param g [in] The GPU
 */
void gk20a_idle_nosuspend(struct gk20a *g);

/**
 * @brief Takes a reference for keeping gpu busy and initialize it if this is
 * first reference. Also, takes a power ref if power saving is supported. On
 * safety it just checks if GPU is in usable state.
 *
 * @param g [in] The GPU
 *
 * @return 0 in case of success, -ENODEV in case of failure.
 *
 * This is called mostly by the devctl path to check if proceeding further is
 * allowed or not.
 */
int gk20a_busy(struct gk20a *g);

/**
 * @brief Drops a reference for gpu, put on idle if power saving is supported
 * and power ref goes to 0. Does nothing for safety.
 *
 * @param g [in] The GPU
 */
void gk20a_idle(struct gk20a *g);

/**
 * @brief Check if the GPU HW is in a valid state by making sure the boot_0
 * register returns a valid value.
 *
 * @param g [in] The GPU
 *
 * @return True if the HW state is determined to be valid. False, otherwise.
 */
bool is_nvgpu_gpu_state_valid(struct gk20a *g);

#endif /* NVGPU_INIT_H */
