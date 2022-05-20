/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_MC_H
#define NVGPU_MC_H

/**
 * @file
 * @page unit-mc Unit Master Control (MC)
 *
 * Overview
 * ========
 *
 * The Master Control (MC) unit is responsible for configuring HW units/engines
 * in the GPU.
 *
 * It provides interfaces to nvgpu driver to access the GPU chip details and
 * program HW units/engines through following registers:
 *
 *   + Boot registers: Setup by BIOS and read by nvgpu driver.
 *     - Has the information about architecture, implementation and revision.
 *
 *   + Interrupt registers: These allow to control the interrupts for the local
 *     devices. Interrupts are set by an event and are cleared by software.
 *
 *     Various interrupts sources are: Graphics, Copy*, NVENC*, NVDEC, SEC,
 *     PFIFO, HUB, PFB, THERMAL, HDACODEC, PTIMER, PMGR, NVLINK, DFD, PMU,
 *     LTC, PDISP, PBUS, XVE, PRIV_RING, SOFTWARE.
 *
 *     + There are two interrupt status registers:
 *       + mc_intr_r(0) is for stalling interrupts routed to CPU.
 *       + mc_intr_r(1) is for non-stalling interrupts routed to CPU.
 *     + There are two interrupt enable registers, which can be updated
 *	 through interrupt set/clear (mc_intr_set_r/mc_intr_clear_r)
 *       registers.
 *       + mc_intr_en_r(0) is for stalling interrupts routed to CPU.
 *       + mc_intr_en_r(1) is for non-stalling interrupts routed to CPU.
 *     + Register mc_intr_ltc_r indicates which of the FB partitions
 *       are reporting an LTC interrupt.
 *
 *   + Configuration registers: These are used to configure each of the HW
 *     units/engines after reset.
 *     - Master Control Enable Register (mc_enable_r()) is used to enable/
 *       disable engines.
 *
 * Data Structures
 * ===============
 *
 * + struct nvgpu_mc
 *   This struct holds the variables needed to manage the configuration and
 *   interrupt handling of the units/engines.
 *
 *
 * Static Design
 * =============
 *
 * nvgpu initialization
 * --------------------
 * Before initializing nvgpu driver, the MC unit interface to get the chip
 * version details is invoked. Interrupts are enabled at MC level in
 * #nvgpu_finalize_poweron and the engines are reset.
 *
 * nvgpu teardown
 * --------------
 * During #nvgpu_prepare_poweroff, all interrupts are disabled at MC level
 * by calling the interface from the MC unit.
 *
 * External APIs
 * -------------
 * Most of the static interfaces are HAL functions. They are documented
 * here.
 *   + include/nvgpu/gops/mc.h
 *
 * Dynamic Design
 * ==============
 *
 * At runtime, the stalling and non-stalling interrupts are inquired through
 * MC unit interface. Then corresponding handlers that are exported by the
 * MC unit are invoked. While in ISRs, interrupts are disabled and they
 * are re-enabled after ISRs through interfaces provided by the MC unit.
 *
 * For quiesce state handling, interrupts will have to be disabled that is
 * again supported through MC unit interface.
 *
 * External APIs
 * -------------
 * Some of the dynamic interfaces are HAL functions. They are documented
 * here.
 *   + include/nvgpu/gops/mc.h
 */


#include <nvgpu/types.h>
#include <nvgpu/cond.h>
#include <nvgpu/atomic.h>
#include <nvgpu/lock.h>
#include <nvgpu/bitops.h>
#include <nvgpu/cic_mon.h>

struct gk20a;
struct nvgpu_device;

#define MC_ENABLE_DELAY_US	20U
#define MC_RESET_DELAY_US	20U
#define MC_RESET_CE_DELAY_US	500U

/**
 * @defgroup NVGPU_MC_UNIT_DEFINES
 *
 * Enumeration of all units intended to be used by enabling/disabling HAL
 * that requires unit as parameter. Units can be added to the enumeration as
 * needed.
 */

/**
 * @ingroup NVGPU_MC_UNIT_DEFINES
 */

/** FIFO Engine */
#define NVGPU_UNIT_FIFO		BIT32(0)
/** Performance Monitoring unit */
#define NVGPU_UNIT_PERFMON	BIT32(1)
/** Graphics Engine */
#define NVGPU_UNIT_GRAPH	BIT32(2)
/** BLPG and BLCG controllers within Graphics Engine */
#define NVGPU_UNIT_BLG		BIT32(3)
#ifdef CONFIG_NVGPU_HAL_NON_FUSA
#define NVGPU_UNIT_PWR		BIT32(4)
#endif
#ifdef CONFIG_NVGPU_DGPU
#define NVGPU_UNIT_NVDEC	BIT32(5)
#endif
/** CE2 unit */
#define NVGPU_UNIT_CE2		BIT32(6)
/** NVLINK unit */
#define NVGPU_UNIT_NVLINK	BIT32(7)

/** Bit offset of the Architecture field in the HW version register */
#define NVGPU_GPU_ARCHITECTURE_SHIFT 4U

struct nvgpu_intr_unit_info {
	/**
	 * top bit 0 -> subtree 0 -> leaf0, leaf1 -> leaf 0, 1
	 * top bit 1 -> subtree 1 -> leaf0, leaf1 -> leaf 2, 3
	 * top bit 2 -> subtree 2 -> leaf0, leaf1 -> leaf 4, 5
	 * top bit 3 -> subtree 3 -> leaf0, leaf1 -> leaf 6, 7
	 */
	/**
	 * h/w defined vectorids for the s/w defined intr unit.
	 * Upto 32 vectorids (32 bits of a leaf register) are supported for
	 * the intr units that support multiple vector ids.
	 */
	u32 vectorid[NVGPU_CIC_INTR_VECTORID_SIZE_MAX];
	/** number of vectorid supported by the intr unit */
	u32 vectorid_size;
	u32 subtree;	/** subtree number corresponding to vectorid */
	u64 subtree_mask; /** leaf1_leaf0 value for the intr unit */
	/**
	 * This flag will be set to true after all the fields
	 * of nvgpu_intr_unit_info are configured.
	 */
	bool valid;
};

/**
 * This struct holds the variables needed to manage the configuration and
 * interrupt handling of the units/engines.
 */
struct nvgpu_mc {
	/** Lock to access the MC interrupt registers. */
	struct nvgpu_spinlock intr_lock;

	/** Lock to access the MC unit registers. */
	struct nvgpu_spinlock enable_lock;

	/**
	 * Bitmask of the stalling/non-stalling enabled interrupts.
	 * This is used to enable/disable the interrupts at runtime.
	 * intr_mask_restore[2] & intr_mask_restore[3] are applicable
	 * when GSP exists.
	 */
	u32 intr_mask_restore[4];

	/** @cond DOXYGEN_SHOULD_SKIP_THIS */

	/**
	 * intr info array indexed by s/w defined intr unit name
	 */
	struct nvgpu_intr_unit_info intr_unit_info[NVGPU_CIC_INTR_UNIT_MAX];
	/**
	 * Leaf mask per subtree. Subtree is a pair of leaf registers.
	 * Each subtree corresponds to a bit in intr_top register.
	 */
	u64 subtree_mask_restore[HOST2SOC_NUM_SUBTREE];
	/** @endcond DOXYGEN_SHOULD_SKIP_THIS */
};

/**
 * @brief Reset given HW unit(s).
 *
 * @param g [in]	The GPU driver struct.
 * @param units [in]	Value designating the GPU HW unit(s)
 *                      controlled by MC. Supported values are:
 *			  - #NVGPU_UNIT_FIFO
 *			  - #NVGPU_UNIT_PERFMON
 *			  - #NVGPU_UNIT_GRAPH
 *			  - #NVGPU_UNIT_BLG
 *			The logical OR of the reset mask of each given units.
 *
 * This function is called to reset one or multiple units.
 *
 * Steps:
 * - Compute bitmask of given unit or units.
 * - Disable and enable given unit or units.
 *
 * @return -EINVAL if register write fails, else 0.
 */
int nvgpu_mc_reset_units(struct gk20a *g, u32 units);

/**
 * @brief Reset given HW engine.
 *
 * @param g [in]	The GPU driver struct.
 * @param dev [in]	Nvgpu_device struct that
 *                      contains info of engine to be reset.
 *
 * This function is called to reset a single engine.
 * Note: Currently, this API is used to reset non-GR engines only.
 *
 * Steps:
 * - Compute bitmask of given engine from reset_id.
 * - Disable and enable given engine.
 *
 * @return -EINVAL if register write fails, else 0.
 */
int nvgpu_mc_reset_dev(struct gk20a *g, const struct nvgpu_device *dev);

/**
 * @brief Reset all engines of given devtype.
 *
 * @param g [in]	The GPU driver struct.
 * @param devtype [in]	Type of device.
 *                      Supported values are:
 *			   - NVGPU_DEVTYPE_GRAPHICS
 *			   - NVGPU_DEVTYPE_LCE
 *
 * This function is called to reset engines of given devtype.
 * Note: Currently, this API is used to reset non-GR engines only.
 *
 * Steps:
 * - Compute bitmask of all engines of given devtype.
 * - Disable and enable given engines.
 *
 * @return -EINVAL if register write fails, else 0.
 */
int nvgpu_mc_reset_devtype(struct gk20a *g, u32 devtype);

#endif
