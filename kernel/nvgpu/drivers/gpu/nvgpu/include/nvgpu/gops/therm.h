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

#ifndef NVGPU_GOPS_THERM_H
#define NVGPU_GOPS_THERM_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * Thermal (Therm) HAL interface.
 */
struct gk20a;

/**
 * Therm HAL operations.
 *
 * @see gpu_ops.
 */
struct gops_therm {

	/**
	 * @brief Initialize therm unit.
	 *
	 * @param g [in]	The GPU driver struct.
	 *
	 * This HAL performs initialization of therm unit which includes HW
	 * initialization and unit interface initialization.
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 */
	int (*init_therm_support)(struct gk20a *g);

	/**
	 * @brief Initialize therm hardware.
	 *
	 * @param g [in]	The GPU driver struct.
	 *
	 * This HAL performs initialization of therm HW by writing required
	 * values to various therm registers.
	 * - enables trigger for EXT_THERM_0/1/2 events
	 * - sets slowdown factor for EXT_THERM_0/1/2 events
	 * - sets up the gradual stepping tables 0 and 1 for jumping
	 *   from full speed gpu clk to requested slow down factor
	 * - enables gradual slowdown for gpu clk
	 * - configures gradual slowdown settings
	 * - disables idle clock slowdown
	 *
	 * @return 0 in case of success, < 0 in case of failure.
	 */
	int (*init_therm_setup_hw)(struct gk20a *g);

	/**
	 * @brief Control ELCG mode of an engine.
	 *
	 * @param g [in]	The GPU driver struct.
	 * @param mode [in]	ELCG mode.
	 *                      Valid ELCG modes are: ELCG_RUN, ELCG_STOP
	 *			and ELCG_AUTO.
	 * @param engine [in]	Engine index for control reg.
	 *			Valid engine range: 0 to NV_HOST_NUM_ENGINES - 1
	 *
	 * This HAL controls engine level clock gating (ELCG) of an engine with
	 * following steps:
	 * - Skip ELCG if NVGPU_GPU_CAN_ELCG is not enabled
	 * - Update NV_THERM_GATE_CTRL register with one of the following modes:
	 *   . RUN:  clk always runs
	 *   . AUTO: clk runs when non-idle
	 *   . STOP: clk is stopped
	 */
	void (*init_elcg_mode)(struct gk20a *g, u32 mode, u32 engine);

	/**
	 * @brief Init ELCG idle filters of GPU engines, FECS and HUBMMU.
	 *
	 * @param g [in]	The GPU driver struct.
	 *
	 * Idle filter specifies the amount of time that the engine must be
	 * idle before sending a turnoff request to host in preparation to
	 * gate the engine. This HAL skips idle filter initialization for
	 * simulation  platform. Otherwise sets up idle filters with prod
	 * settings for:
	 * - Active engines
	 * - FECS
	 * - HUBMMU
	 *
	 * @return 0.
	 */
	int (*elcg_init_idle_filters)(struct gk20a *g);

	/** @cond DOXYGEN_SHOULD_SKIP_THIS */
	/**
	 * @brief Control BLCG mode of an engine.
	 *
	 * @param g [in]	The GPU driver struct.
	 * @param mode [in]	BLCG mode.
	 * @param engine [in]	Engine index for control reg.
	 *
	 * This HAL controls Block level clock gating (BLCG) of an engine with
	 * following steps:
	 * - Skip BLCG if NVGPU_GPU_CAN_BLCG is not enabled
	 * - Update NV_THERM_GATE_CTRL register with either "RUN:clk always
	 *   runs" or "AUTO:clk runs when non-idle" mode
	 */
	void (*init_blcg_mode)(struct gk20a *g, u32 mode, u32 engine);

	void (*get_internal_sensor_limits)(s32 *max_24_8,
							s32 *min_24_8);
	void (*throttle_enable)(struct gk20a *g, u32 val);
	u32 (*throttle_disable)(struct gk20a *g);
	void (*idle_slowdown_enable)(struct gk20a *g, u32 val);
	u32 (*idle_slowdown_disable)(struct gk20a *g);
#ifdef CONFIG_NVGPU_IOCTL_NON_FUSA
	int (*configure_therm_alert)(struct gk20a *g, s32 curr_warn_temp);
#endif
#ifdef CONFIG_DEBUG_FS
	void (*therm_debugfs_init)(struct gk20a *g);
#endif
	u32 (*therm_max_fpdiv_factor)(void);
	u32 (*therm_grad_stepping_pdiv_duration)(void);
	void (*get_internal_sensor_curr_temp)(struct gk20a *g, u32 *temp_f24_8);
	/** @endcond DOXYGEN_SHOULD_SKIP_THIS */
};

#endif /* NVGPU_GOPS_THERM_H */
