/*
 * Copyright (c) 2015-2023, NVIDIA CORPORATION. All rights reserved.
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

#include <linux/tegra-camera-rtcpu.h>

#include <linux/bitops.h>
#include <linux/completion.h>
#include <linux/delay.h>
#if IS_ENABLED(CONFIG_INTERCONNECT)
#include <linux/interconnect.h>
#include <dt-bindings/interconnect/tegra_icc_id.h>
#endif
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/irqchip/tegra-agic.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#if IS_ENABLED(CONFIG_TEGRA_BWMGR)
#include <linux/platform/tegra/emc_bwmgr.h>
#endif
#include <linux/pm_runtime.h>
#include <linux/sched.h>
#include <linux/seq_buf.h>
#include <linux/slab.h>
#include <linux/tegra-firmwares.h>
#include <linux/tegra-hsp.h>
#include <linux/tegra-ivc-bus.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
#include <linux/tegra_pm_domains.h>
#include <soc/tegra/chip-id.h>
#else
#include <linux/pm_domain.h>
#include <soc/tegra/fuse.h>
#endif
#include <linux/tegra-rtcpu-monitor.h>
#include <linux/tegra-rtcpu-trace.h>
#include <linux/version.h>
#include <linux/wait.h>

#include <dt-bindings/memory/tegra-swgroup.h>

#include "rtcpu/clk-group.h"
#include "rtcpu/device-group.h"
#include "rtcpu/reset-group.h"
#include "rtcpu/hsp-combo.h"

#include "soc/tegra/camrtc-commands.h"
#include <linux/tegra-rtcpu-coverage.h>

#ifndef RTCPU_DRIVER_SM5_VERSION
#define RTCPU_DRIVER_SM5_VERSION U32_C(5)
#endif

#if defined(LINUX_VERSION) && (LINUX_VERSION < 409)
#define DISABLE_APE_RUNTIME_PM 1
#else
#define DISABLE_APE_RUNTIME_PM 0
#endif

enum tegra_cam_rtcpu_id {
	TEGRA_CAM_RTCPU_SCE,
	TEGRA_CAM_RTCPU_APE,
	TEGRA_CAM_RTCPU_RCE,
};

#define CAMRTC_NUM_REGS		2
#define CAMRTC_NUM_RESETS	2
#define CAMRTC_NUM_IRQS		1

struct tegra_cam_rtcpu_pdata {
	const char *name;
	void (*assert_resets)(struct device *);
	int (*deassert_resets)(struct device *);
	int (*wait_for_idle)(struct device *);
	const char * const *reset_names;
	const char * const *reg_names;
	const char * const *irq_names;
	enum tegra_cam_rtcpu_id id;
};

/* Register specifics */
#define TEGRA_APS_FRSC_SC_CTL_0			0x0
#define TEGRA_APS_FRSC_SC_MODEIN_0		0x14
#define TEGRA_PM_R5_CTRL_0			0x40
#define TEGRA_PM_PWR_STATUS_0			0x20

#define TEGRA_R5R_SC_DISABLE			0x5
#define TEGRA_FN_MODEIN				0x29527
#define TEGRA_PM_FWLOADDONE			0x2
#define TEGRA_PM_WFIPIPESTOPPED			0x200000

#define AMISC_ADSP_STATUS			0x14
#define AMISC_ADSP_L2_IDLE			BIT(31)
#define AMISC_ADSP_L2_CLKSTOPPED		BIT(30)

static int tegra_sce_cam_wait_for_idle(struct device *dev);
static void tegra_sce_cam_assert_resets(struct device *dev);
static int tegra_sce_cam_deassert_resets(struct device *dev);

static int tegra_ape_cam_wait_for_idle(struct device *dev);
static irqreturn_t tegra_camrtc_adsp_wfi_handler(int irq, void *data);
static void tegra_ape_cam_assert_resets(struct device *dev);
static int tegra_ape_cam_deassert_resets(struct device *dev);

static int tegra_rce_cam_wait_for_idle(struct device *dev);
static void tegra_rce_cam_assert_resets(struct device *dev);
static int tegra_rce_cam_deassert_resets(struct device *dev);

static const char * const sce_reset_names[] = {
	"nvidia,reset-group-1",
	"nvidia,reset-group-2",
	NULL,
};

static const char * const sce_reg_names[] = {
	"sce-pm",
	"sce-cfg",
	NULL
};

static const struct tegra_cam_rtcpu_pdata sce_pdata = {
	.name = "sce",
	.wait_for_idle = tegra_sce_cam_wait_for_idle,
	.assert_resets = tegra_sce_cam_assert_resets,
	.deassert_resets = tegra_sce_cam_deassert_resets,
	.id = TEGRA_CAM_RTCPU_SCE,
	.reset_names = sce_reset_names,
	.reg_names = sce_reg_names,
};

static const char * const ape_reg_names[] = {
	"ape-amisc",
	NULL,
};

static const char * const ape_reset_names[] = {
	"reset-names",			/* all named resets */
	NULL,
};

static const char * const ape_irq_names[] = {
	"adsp-wfi",
	NULL
};

static const struct tegra_cam_rtcpu_pdata ape_pdata = {
	.name = "ape",
	.assert_resets = tegra_ape_cam_assert_resets,
	.deassert_resets = tegra_ape_cam_deassert_resets,
	.wait_for_idle = tegra_ape_cam_wait_for_idle,
	.id = TEGRA_CAM_RTCPU_APE,
	.reset_names = ape_reset_names,
	.reg_names = ape_reg_names,
	.irq_names = ape_irq_names,
};

static const char * const rce_reset_names[] = {
	"reset-names",			/* all named resets */
	NULL,
};

/* SCE and RCE share the PM regs */
static const char * const rce_reg_names[] = {
	"rce-pm",
	NULL,
};

static const struct tegra_cam_rtcpu_pdata rce_pdata = {
	.name = "rce",
	.wait_for_idle = tegra_rce_cam_wait_for_idle,
	.assert_resets = tegra_rce_cam_assert_resets,
	.deassert_resets = tegra_rce_cam_deassert_resets,
	.id = TEGRA_CAM_RTCPU_RCE,
	.reset_names = rce_reset_names,
	.reg_names = rce_reg_names,
};

#define NV(p) "nvidia," #p

struct tegra_cam_rtcpu {
	const char *name;
	struct tegra_ivc_bus *ivc;
	struct device_dma_parameters dma_parms;
	struct device *hsp_device;
	struct camrtc_hsp *hsp;
	struct tegra_rtcpu_trace *tracer;
	struct tegra_rtcpu_coverage *coverage;
	u32 cmd_timeout;
	u32 fw_version;
	u8 fw_hash[RTCPU_FW_HASH_SIZE];
	struct {
		u64 reset_complete;
		u64 boot_handshake;
	} stats;
	union {
		void __iomem *regs[CAMRTC_NUM_REGS];
		struct {
			void __iomem *pm_base;
			void __iomem *cfg_base;
		};
		struct {
			void __iomem *amisc_base;
		};
	};
	struct camrtc_clk_group *clocks;
	struct camrtc_reset_group *resets[CAMRTC_NUM_RESETS];
	union {
		int irqs[CAMRTC_NUM_IRQS];
		struct {
			int adsp_wfi_irq;
		};
	};
	const struct tegra_cam_rtcpu_pdata *pdata;
	struct camrtc_device_group *camera_devices;
#if IS_ENABLED(CONFIG_INTERCONNECT)
	struct icc_path *icc_path;
	u32 mem_bw;
#endif
#if IS_ENABLED(CONFIG_TEGRA_BWMGR)
	struct tegra_bwmgr_client *bwmgr;
	unsigned long full_bw;
#endif
	struct tegra_camrtc_mon *monitor;
	u32 max_reboot_retry;
	bool powered;
	bool boot_sync_done;
	bool fw_active;
	bool online;
};

static void __iomem *tegra_cam_ioremap(struct device *dev, int index)
{
	struct resource mem;
	int err = of_address_to_resource(dev->of_node, index, &mem);
	if (err)
		return IOMEM_ERR_PTR(err);

	/* NOTE: assumes size is large enough for caller */
	return devm_ioremap_resource(dev, &mem);
}

static void __iomem *tegra_cam_ioremap_byname(struct device *dev,
					const char *name)
{
	int index = of_property_match_string(dev->of_node, "reg-names", name);
	if (index < 0)
		return IOMEM_ERR_PTR(-ENOENT);
	return tegra_cam_ioremap(dev, index);
}

static int tegra_camrtc_get_resources(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	const struct tegra_cam_rtcpu_pdata *pdata = rtcpu->pdata;
	struct camrtc_device_group *devgrp;
	int i, err;

	rtcpu->clocks = camrtc_clk_group_get(dev);
	if (IS_ERR(rtcpu->clocks)) {
		err = PTR_ERR(rtcpu->clocks);
		if (err == -EPROBE_DEFER)
			dev_info(dev, "defer %s probe because of %s\n",
				rtcpu->name, "clocks");
		else
			dev_warn(dev, "clocks not available: %d\n", err);
		return err;
	}

	devgrp = camrtc_device_group_get(dev, "nvidia,camera-devices",
		"nvidia,camera-device-names");
	if (!IS_ERR(devgrp)) {
		rtcpu->camera_devices = devgrp;
	} else {
		err = PTR_ERR(devgrp);
		if (err == -EPROBE_DEFER)
			return err;
		if (err != -ENODATA && err != -ENOENT)
			dev_warn(dev, "get %s: failed: %d\n",
				"nvidia,camera-devices", err);
	}

#define GET_RESOURCES(_res_, _get_, _null_, _toerr)	\
	for (i = 0; i < ARRAY_SIZE(rtcpu->_res_##s); i++) { \
		if (!pdata->_res_##_names[i]) \
			break; \
		rtcpu->_res_##s[i] = _get_(dev, pdata->_res_##_names[i]); \
		err = _toerr(rtcpu->_res_##s[i]); \
		if (err == 0) \
			continue; \
		rtcpu->_res_##s[i] = _null_; \
		if (err == -EPROBE_DEFER) { \
			dev_info(dev, "defer %s probe because %s %s\n", \
				rtcpu->name, #_res_, pdata->_res_##_names[i]); \
			return err; \
		} \
		if (err != -ENODATA && err != -ENOENT) \
			dev_warn(dev, "%s %s not available: %d\n", #_res_, \
				pdata->_res_##_names[i], err); \
	}

#define _PTR2ERR(x) (IS_ERR(x) ? PTR_ERR(x) : 0)

	GET_RESOURCES(reset, camrtc_reset_group_get, NULL, _PTR2ERR);
	GET_RESOURCES(reg, tegra_cam_ioremap_byname, NULL, _PTR2ERR);

#undef _PTR2ERR

	if (rtcpu->resets[0] == NULL) {
		struct camrtc_reset_group *resets;

		resets = camrtc_reset_group_get(dev, NULL);

		if (!IS_ERR(resets))
			rtcpu->resets[0] = resets;
		else if (PTR_ERR(resets) == -EPROBE_DEFER) {
			dev_info(dev, "defer %s probe because of %s\n",
				rtcpu->name, "resets");
			return -EPROBE_DEFER;
		}
	}

	return 0;
}

static int tegra_camrtc_get_irqs(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	const struct tegra_cam_rtcpu_pdata *pdata = rtcpu->pdata;
	int i, err;

	/*
	 * AGIC can be touched only after APE is fully powered on.
	 *
	 * This can be called only after runtime resume.
	 */

	if (pdata->irq_names == NULL)
		return 0;

#define _get_irq(_dev, _name) of_irq_get_byname(_dev->of_node, _name)
#define _int2err(x) ((x) < 0 ? (x) : 0)

	GET_RESOURCES(irq, _get_irq, 0, _int2err);

#undef _get_irq
#undef _int2err

	return 0;
}

static int tegra_camrtc_enable_clks(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	return camrtc_clk_group_enable(rtcpu->clocks);
}

static void tegra_camrtc_disable_clks(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	return camrtc_clk_group_disable(rtcpu->clocks);
}

static void tegra_camrtc_assert_resets(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	if (rtcpu->pdata->assert_resets)
		rtcpu->pdata->assert_resets(dev);
}

static int tegra_camrtc_deassert_resets(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	int ret = 0;

	if (rtcpu->pdata->deassert_resets) {
		ret = rtcpu->pdata->deassert_resets(dev);
		rtcpu->stats.reset_complete = ktime_get_ns();
		rtcpu->stats.boot_handshake = 0;
	}

	return ret;
}

#define CAMRTC_MAX_BW (0xFFFFFFFFU)

#if IS_ENABLED(CONFIG_INTERCONNECT)

#define RCE_MAX_BW_MBPS (160)

static void tegra_camrtc_init_icc(struct device *dev, u32 bw)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	if (bw == CAMRTC_MAX_BW)
		rtcpu->mem_bw = MBps_to_icc(RCE_MAX_BW_MBPS);
	else
		rtcpu->mem_bw = bw;

	rtcpu->icc_path = icc_get(dev, TEGRA_ICC_RCE, TEGRA_ICC_PRIMARY);

	if (IS_ERR_OR_NULL(rtcpu->icc_path)) {
		dev_warn(dev, "no interconnect control\n");
		rtcpu->icc_path = NULL;
		return;
	}

	dev_dbg(dev, "using icc rate %u for power-on\n", rtcpu->mem_bw);
}
#endif

#if IS_ENABLED(CONFIG_TEGRA_BWMGR)
static void tegra_camrtc_init_bwmgr(struct device *dev, u32 bw)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	if (bw == CAMRTC_MAX_BW)
		rtcpu->full_bw = tegra_bwmgr_get_max_emc_rate();
	else
		rtcpu->full_bw = tegra_bwmgr_round_rate(bw);

	rtcpu->bwmgr = tegra_bwmgr_register(TEGRA_BWMGR_CLIENT_CAMRTC);

	if (IS_ERR_OR_NULL(rtcpu->bwmgr)) {
		dev_warn(dev, "no memory bw manager\n");
		rtcpu->bwmgr = NULL;
		return;
	}

	dev_dbg(dev, "using emc rate %lu for power-on\n", rtcpu->full_bw);
}
#endif

static void tegra_camrtc_init_membw(struct device *dev)
{
	u32 bw = CAMRTC_MAX_BW;

	if (of_property_read_u32(dev->of_node, "nvidia,memory-bw", &bw) != 0) {
		;
	} else if (tegra_get_chip_id() == TEGRA234) {
#if IS_ENABLED(CONFIG_INTERCONNECT)
		tegra_camrtc_init_icc(dev, bw);
#endif
	} else {
#if IS_ENABLED(CONFIG_TEGRA_BWMGR)
		tegra_camrtc_init_bwmgr(dev, bw);
#endif
	}
}

static void tegra_camrtc_full_mem_bw(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

#if IS_ENABLED(CONFIG_INTERCONNECT)
	if (rtcpu->icc_path != NULL) {
		int ret = icc_set_bw(rtcpu->icc_path, 0, rtcpu->mem_bw);

		if (ret)
			dev_err(dev, "set icc bw [%u] failed: %d\n", rtcpu->mem_bw, ret);
		else
			dev_dbg(dev, "requested icc bw %u\n", rtcpu->mem_bw);
	}
#endif

#if IS_ENABLED(CONFIG_TEGRA_BWMGR)
	if (rtcpu->bwmgr != NULL) {
		int ret = tegra_bwmgr_set_emc(rtcpu->bwmgr, rtcpu->full_bw,
				TEGRA_BWMGR_SET_EMC_FLOOR);
		if (ret < 0)
			dev_info(dev, "emc request rate %lu failed, %d\n",
				rtcpu->full_bw, ret);
		else
			dev_dbg(dev, "requested emc rate %lu\n",
				rtcpu->full_bw);
	}
#endif
}

static void tegra_camrtc_slow_mem_bw(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

#if IS_ENABLED(CONFIG_INTERCONNECT)
	if (rtcpu->icc_path != NULL)
		(void)icc_set_bw(rtcpu->icc_path, 0, 0);
#endif

	if (rtcpu->bwmgr != NULL)
		(void)tegra_bwmgr_set_emc(rtcpu->bwmgr, 0,
				TEGRA_BWMGR_SET_EMC_FLOOR);
}

static void tegra_camrtc_set_fwloaddone(struct device *dev, bool fwloaddone)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	if (rtcpu->pm_base != NULL) {
		u32 val = readl(rtcpu->pm_base + TEGRA_PM_R5_CTRL_0);

		if (fwloaddone)
			val |= TEGRA_PM_FWLOADDONE;
		else
			val &= ~TEGRA_PM_FWLOADDONE;

		writel(val, rtcpu->pm_base + TEGRA_PM_R5_CTRL_0);
	}
}

static int tegra_sce_cam_deassert_resets(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	int err;

	err = camrtc_reset_group_deassert(rtcpu->resets[0]);
	if (err)
		return err;

	/* Configure R5 core */
	if (rtcpu->cfg_base != NULL) {
		u32 val = readl(rtcpu->cfg_base + TEGRA_APS_FRSC_SC_CTL_0);

		if (val != TEGRA_R5R_SC_DISABLE) {
			/* Disable R5R and smartcomp in camera mode */
			writel(TEGRA_R5R_SC_DISABLE,
				rtcpu->cfg_base + TEGRA_APS_FRSC_SC_CTL_0);

			/* Enable JTAG/Coresight */
			writel(TEGRA_FN_MODEIN,
				rtcpu->cfg_base + TEGRA_APS_FRSC_SC_MODEIN_0);
		}
	}

	/* Group 2 */
	err = camrtc_reset_group_deassert(rtcpu->resets[1]);
	if (err)
		return err;

	/* Group 3: nCPUHALT controlled by PM, not by CAR. */
	tegra_camrtc_set_fwloaddone(dev, true);

	return 0;
}

static void tegra_sce_cam_assert_resets(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	tegra_camrtc_set_fwloaddone(dev, false);

	camrtc_reset_group_assert(rtcpu->resets[1]);
	camrtc_reset_group_assert(rtcpu->resets[0]);
}

static int tegra_sce_cam_wait_for_idle(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	long timeout = rtcpu->cmd_timeout;
	long delay_stride = HZ / 50;

	if (rtcpu->pm_base == NULL)
		return 0;

	/* Poll for WFI assert.*/
	for (;;) {
		u32 val = readl(rtcpu->pm_base + TEGRA_PM_PWR_STATUS_0);

		if ((val & TEGRA_PM_WFIPIPESTOPPED) == 0)
			break;

		if (timeout < 0) {
			dev_warn(dev, "timeout waiting for WFI\n");
			return -EBUSY;
		}

		msleep(delay_stride);
		timeout -= delay_stride;
	}

	return 0;
}

static void tegra_ape_cam_assert_resets(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	camrtc_reset_group_assert(rtcpu->resets[0]);
}

static int tegra_ape_cam_deassert_resets(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	return camrtc_reset_group_deassert(rtcpu->resets[0]);
}

static irqreturn_t tegra_camrtc_adsp_wfi_handler(int irq, void *data)
{
	struct completion *entered_wfi = data;

	disable_irq_nosync(irq);

	complete(entered_wfi);

	return IRQ_HANDLED;
}

static int tegra_ape_cam_wait_for_l2_idle(struct device *dev, long *timeout)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	long delay_stride = HZ / 50;

	if (rtcpu->amisc_base == NULL) {
		dev_WARN(dev, "iobase \"ape-amisc\" missing\n");
		return 0;
	}

	/* Poll for L2 idle.*/
	for (;;) {
		u32 val = readl(rtcpu->amisc_base + AMISC_ADSP_STATUS);
		u32 mask = AMISC_ADSP_L2_IDLE;

		if ((val & mask) == mask)
			break;

		if (*timeout <= 0) {
			dev_WARN(dev, "timeout waiting for L2 idle\n");
			return -EBUSY;
		}

		msleep(delay_stride);
		*timeout -= delay_stride;
	}

	return 0;
}

static int tegra_ape_cam_wait_for_wfi(struct device *dev, long *timeout)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	DECLARE_COMPLETION_ONSTACK(entered_wfi);
	unsigned int irq = rtcpu->adsp_wfi_irq;
	int err;

	if (irq <= 0) {
		dev_WARN(dev, "irq \"adsp-wfi\" missing\n");
		return 0;
	}

	err = request_threaded_irq(irq,
			tegra_camrtc_adsp_wfi_handler, NULL,
			IRQF_TRIGGER_HIGH,
			"adsp-wfi", &entered_wfi);
	if (err) {
		dev_WARN(dev, "cannot request for %s interrupt: %d\n",
			"adsp-wfi", err);
		return err;
	}

	*timeout = wait_for_completion_timeout(&entered_wfi, *timeout);

	free_irq(irq, &entered_wfi);

	if (*timeout == 0) {
		dev_WARN(dev, "timeout waiting for WFI\n");
		return -EBUSY;
	}

	return 0;
}

static int tegra_ape_cam_wait_for_idle(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	long timeout = rtcpu->cmd_timeout;
	int err;

	err = tegra_ape_cam_wait_for_wfi(dev, &timeout);
	if (err)
		return err;

	return tegra_ape_cam_wait_for_l2_idle(dev, &timeout);
}

static int tegra_rce_cam_wait_for_idle(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	long timeout = rtcpu->cmd_timeout;
	long delay_stride = HZ / 50;

	if (rtcpu->pm_base == NULL)
		return 0;

	/* Poll for WFI assert.*/
	for (;;) {
		u32 val = readl(rtcpu->pm_base + TEGRA_PM_PWR_STATUS_0);

		if ((val & TEGRA_PM_WFIPIPESTOPPED) == 0)
			break;

		if (timeout < 0) {
			dev_info(dev, "timeout waiting for WFI\n");
			return -EBUSY;
		}

		msleep(delay_stride);
		timeout -= delay_stride;
	}

	return 0;
}

static int tegra_rce_cam_deassert_resets(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	int err;

	err = camrtc_reset_group_deassert(rtcpu->resets[0]);
	if (err)
		return err;

	/* nCPUHALT is a reset controlled by PM, not by CAR. */
	tegra_camrtc_set_fwloaddone(dev, true);

	return 0;
}

static void tegra_rce_cam_assert_resets(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	camrtc_reset_group_assert(rtcpu->resets[0]);
}

static int tegra_camrtc_wait_for_idle(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	return rtcpu->pdata->wait_for_idle(dev);
}

static int tegra_camrtc_fw_suspend(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	if (!rtcpu->fw_active || !rtcpu->hsp)
		return 0;

	rtcpu->fw_active = false;

	return camrtc_hsp_suspend(rtcpu->hsp);
}

static int tegra_camrtc_setup_shared_memory(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	int ret;

	/*
	 * Set-up trace
	 */
	ret = tegra_rtcpu_trace_boot_sync(rtcpu->tracer);
	if (ret < 0)
		dev_err(dev, "trace boot sync failed: %d\n", ret);

	/*
	 * Set-up coverage buffer
	 */
	ret = tegra_rtcpu_coverage_boot_sync(rtcpu->coverage);
	if (ret < 0) {
		/*
		 * Not a fatal error, don't stop the sync.
		 * But go ahead and remove the coverage debug FS
		 * entries and release the memory.
		 */
		tegra_rtcpu_coverage_destroy(rtcpu->coverage);
		rtcpu->coverage = NULL;
	}

	/*
	 * Set-up and activate the IVC services in firmware
	 */
	ret = tegra_ivc_bus_boot_sync(rtcpu->ivc);
	if (ret < 0)
		dev_err(dev, "ivc-bus boot sync failed: %d\n", ret);

	return ret;
}

static void tegra_camrtc_set_online(struct device *dev, bool online)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	if (online == rtcpu->online)
		return;

	if (online) {
		if (tegra_camrtc_setup_shared_memory(dev) < 0)
			return;
	}

	/* Postpone the online transition if still probing */
	if (!IS_ERR_OR_NULL(rtcpu->ivc)) {
		rtcpu->online = online;
		tegra_ivc_bus_ready(rtcpu->ivc, online);
	}
}

int tegra_camrtc_ping(struct device *dev, u32 data, long timeout)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	return camrtc_hsp_ping(rtcpu->hsp, data, timeout);
}
EXPORT_SYMBOL(tegra_camrtc_ping);

static void tegra_camrtc_ivc_notify(struct device *dev, u16 group)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	if (rtcpu->ivc)
		tegra_ivc_bus_notify(rtcpu->ivc, group);
}

void tegra_camrtc_ivc_ring(struct device *dev, u16 group)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	camrtc_hsp_group_ring(rtcpu->hsp, group);
}
EXPORT_SYMBOL(tegra_camrtc_ivc_ring);

static int tegra_camrtc_poweron(struct device *dev, bool full_speed)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	int ret;

	if (rtcpu->powered) {
		if (full_speed)
			camrtc_clk_group_adjust_fast(rtcpu->clocks);
		return 0;
	}

	/* APE power domain may misbehave and try to resume while probing */
	if (rtcpu->hsp == NULL) {
		dev_info(dev, "poweron while probing");
		return 0;
	}

	/* Power on and let core run */
	ret = tegra_camrtc_enable_clks(dev);
	if (ret) {
		dev_err(dev, "failed to turn on %s clocks: %d\n",
			rtcpu->name, ret);
		return ret;
	}

	if (full_speed)
		camrtc_clk_group_adjust_fast(rtcpu->clocks);

	ret = tegra_camrtc_deassert_resets(dev);
	if (ret)
		return ret;

	rtcpu->powered = true;

	return 0;
}

static void tegra_camrtc_poweroff(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	if (!rtcpu->powered)
		return;

	rtcpu->powered = false;
	rtcpu->boot_sync_done = false;
	rtcpu->fw_active = false;

	tegra_camrtc_assert_resets(dev);
	tegra_camrtc_disable_clks(dev);
}

static int tegra_camrtc_boot_sync(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	int ret;

	if (!rtcpu->boot_sync_done) {
		ret = camrtc_hsp_sync(rtcpu->hsp);
		if (ret < 0)
			return ret;

		rtcpu->fw_version = ret;
		rtcpu->boot_sync_done = true;
	}

	if (!rtcpu->fw_active) {
		ret = camrtc_hsp_resume(rtcpu->hsp);
		if (ret < 0)
			return ret;

		rtcpu->fw_active = true;
	}

	return 0;
}

/*
 * RTCPU boot sequence
 */
static int tegra_camrtc_boot(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	int retry = 0, max_retries = rtcpu->max_reboot_retry;
	int ret;

	ret = tegra_camrtc_poweron(dev, true);
	if (ret)
		return ret;

	tegra_camrtc_full_mem_bw(dev);

	for (;;) {
		ret = tegra_camrtc_boot_sync(dev);

		tegra_camrtc_set_online(dev, ret == 0);

		if (ret == 0)
			break;
		if (retry++ == max_retries)
			break;
		if (retry > 1) {
			dev_warn(dev, "%s full reset, retry %u/%u\n",
				rtcpu->name, retry, max_retries);
			tegra_camrtc_assert_resets(dev);
			usleep_range(10, 30);
			tegra_camrtc_deassert_resets(dev);
		}
	}

	tegra_camrtc_slow_mem_bw(dev);

	return 0;
}

int tegra_camrtc_iovm_setup(struct device *dev, dma_addr_t iova)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	return camrtc_hsp_ch_setup(rtcpu->hsp, iova);
}
EXPORT_SYMBOL(tegra_camrtc_iovm_setup);

ssize_t tegra_camrtc_print_version(struct device *dev,
					char *buf, size_t size)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	struct seq_buf s;
	int i;

	seq_buf_init(&s, buf, size);
	seq_buf_printf(&s, "version cpu=%s cmd=%u sha1=",
		rtcpu->name, rtcpu->fw_version);

	for (i = 0; i < RTCPU_FW_HASH_SIZE; i++)
		seq_buf_printf(&s, "%02x", rtcpu->fw_hash[i]);

	return seq_buf_used(&s);
}
EXPORT_SYMBOL(tegra_camrtc_print_version);

static void tegra_camrtc_log_fw_version(struct device *dev)
{
	char version[TEGRA_CAMRTC_VERSION_LEN];

	tegra_camrtc_print_version(dev, version, sizeof(version));

	dev_info(dev, "firmware %s\n", version);
}

static void tegra_camrtc_pm_start(struct device *dev, char const *op)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	dev_dbg(dev, "start %s [powered=%d synced=%d active=%d online=%d]\n",
		op, rtcpu->powered, rtcpu->boot_sync_done,
		rtcpu->fw_active, rtcpu->online);
}

static void tegra_camrtc_pm_done(struct device *dev, char const *op, int err)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	dev_dbg(dev, "done %s err=%d [powered=%d synced=%d active=%d online=%d]\n",
		op, err, rtcpu->powered, rtcpu->boot_sync_done,
		rtcpu->fw_active, rtcpu->online);
}

static int tegra_cam_rtcpu_runtime_suspend(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	int err;

	tegra_camrtc_pm_start(dev, "runtime_suspend");

	err = tegra_camrtc_fw_suspend(dev);
	/* Try full reset if an error occurred while suspending core. */
	if (err < 0) {

		dev_info(dev, "RTCPU suspend failed, resetting it");

		/* runtime_resume() powers RTCPU back on */
		tegra_camrtc_poweroff(dev);

		/* We want to boot sync IVC and trace when resuming */
		tegra_camrtc_set_online(dev, false);
	}

	camrtc_clk_group_adjust_slow(rtcpu->clocks);

	tegra_camrtc_pm_done(dev, "runtime_suspend", err);

	return 0;
}

static int tegra_cam_rtcpu_runtime_resume(struct device *dev)
{
	int err;

	tegra_camrtc_pm_start(dev, "runtime_resume");

	err = tegra_camrtc_boot(dev);

	tegra_camrtc_pm_done(dev, "runtime_resume", err);

	return err;
}

static int tegra_cam_rtcpu_runtime_idle(struct device *dev)
{
	pm_runtime_mark_last_busy(dev);

	return 0;
}

static struct device *tegra_camrtc_get_hsp_device(struct device_node *hsp_node)
{
	struct device_node *of_node;
	struct platform_device *pdev;

	of_node = of_parse_phandle(hsp_node, "device", 0);
	if (of_node == NULL)
		return NULL;

	pdev = of_find_device_by_node(of_node);
	of_node_put(of_node);

	if (pdev == NULL)
		return ERR_PTR(-EPROBE_DEFER);

	if (&pdev->dev.driver == NULL) {
		platform_device_put(pdev);
		return ERR_PTR(-EPROBE_DEFER);
	}

	return &pdev->dev;
}

static int tegra_camrtc_hsp_init(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	struct device_node *hsp_node;
	int err;

	if (!IS_ERR_OR_NULL(rtcpu->hsp))
		return 0;

	hsp_node = of_get_child_by_name(dev->of_node, "hsp");
	rtcpu->hsp_device = tegra_camrtc_get_hsp_device(hsp_node);
	if (IS_ERR(rtcpu->hsp_device)) {
		of_node_put(hsp_node);
		return PTR_ERR(rtcpu->hsp_device);
	}

	if (rtcpu->hsp_device != NULL) {
		int ret = pm_runtime_get_sync(rtcpu->hsp_device);
		if (ret < 0) {
			dev_warn(rtcpu->hsp_device,
				"power on failure: %d\n", ret);
			of_node_put(hsp_node);
			put_device(rtcpu->hsp_device);
			rtcpu->hsp_device = NULL;
			return ret;
		}
	}

	rtcpu->hsp = camrtc_hsp_create(dev, tegra_camrtc_ivc_notify,
			rtcpu->cmd_timeout);
	if (IS_ERR(rtcpu->hsp)) {
		err = PTR_ERR(rtcpu->hsp);
		rtcpu->hsp = NULL;
		return err;
	}

	return 0;
}

static int tegra_cam_rtcpu_remove(struct platform_device *pdev)
{
	struct tegra_cam_rtcpu *rtcpu = platform_get_drvdata(pdev);
	bool online = rtcpu->online;
	bool pm_is_active = pm_runtime_active(&pdev->dev);

	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);

	tegra_camrtc_set_online(&pdev->dev, false);

	if (rtcpu->hsp) {
		if (pm_is_active)
			tegra_cam_rtcpu_runtime_suspend(&pdev->dev);
		if (online)
			camrtc_hsp_bye(rtcpu->hsp);
		camrtc_hsp_free(rtcpu->hsp);
		rtcpu->hsp = NULL;
	}

	if (!IS_ERR_OR_NULL(rtcpu->hsp_device)) {
		pm_runtime_put(rtcpu->hsp_device);
		put_device(rtcpu->hsp_device);
	}

	tegra_rtcpu_trace_destroy(rtcpu->tracer);
	rtcpu->tracer = NULL;
	tegra_rtcpu_coverage_destroy(rtcpu->coverage);
	rtcpu->coverage = NULL;

	tegra_camrtc_poweroff(&pdev->dev);
#if IS_ENABLED(CONFIG_TEGRA_BWMGR)
	if (rtcpu->bwmgr != NULL)
		tegra_bwmgr_unregister(rtcpu->bwmgr);
	rtcpu->bwmgr = NULL;
#endif
#if IS_ENABLED(CONFIG_INTERCONNECT)
	icc_put(rtcpu->icc_path);
	rtcpu->icc_path = NULL;
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0)
	tegra_pd_remove_device(&pdev->dev);
#else
	pm_genpd_remove_device(&pdev->dev);
#endif
	tegra_cam_rtcpu_mon_destroy(rtcpu->monitor);
	tegra_ivc_bus_destroy(rtcpu->ivc);

	pdev->dev.dma_parms = NULL;

	return 0;
}

static struct device *s_dev;

static int tegra_cam_rtcpu_probe(struct platform_device *pdev)
{
	struct tegra_cam_rtcpu *rtcpu;
	const struct tegra_cam_rtcpu_pdata *pdata;
	struct device *dev = &pdev->dev;
	int ret;
	const char *name;
	uint32_t timeout;

	pdata = of_device_get_match_data(dev);
	if (pdata == NULL) {
		dev_err(dev, "no device match\n");
		return -ENODEV;
	}

	name = pdata->name;
	of_property_read_string(dev->of_node, NV(cpu-name), &name);

	dev_dbg(dev, "probing RTCPU on %s\n", name);

	rtcpu = devm_kzalloc(dev, sizeof(*rtcpu), GFP_KERNEL);
	if (rtcpu == NULL)
		return -ENOMEM;

	rtcpu->pdata = pdata;
	rtcpu->name = name;
	platform_set_drvdata(pdev, rtcpu);

	(void) dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));

	/* Enable runtime power management */
	pm_runtime_enable(dev);

	ret = tegra_camrtc_get_resources(dev);
	if (ret)
		goto fail;

	rtcpu->max_reboot_retry = 3;
	(void)of_property_read_u32(dev->of_node, NV(max-reboot),
			&rtcpu->max_reboot_retry);
	timeout = 2000;
	if (tegra_platform_is_vdk())
		timeout = 5000;

	(void)of_property_read_u32(dev->of_node, "nvidia,cmd-timeout", &timeout);

	rtcpu->cmd_timeout = msecs_to_jiffies(timeout);

	timeout = 60000;
	ret = of_property_read_u32(dev->of_node, NV(autosuspend-delay-ms), &timeout);
	if (ret == 0) {
		pm_runtime_use_autosuspend(dev);
		pm_runtime_set_autosuspend_delay(&pdev->dev, timeout);
	}

	tegra_camrtc_init_membw(dev);

	dev->dma_parms = &rtcpu->dma_parms;
	dma_set_max_seg_size(dev, UINT_MAX);

	rtcpu->tracer = tegra_rtcpu_trace_create(dev, rtcpu->camera_devices);

	rtcpu->coverage = tegra_rtcpu_coverage_create(dev);

	ret = tegra_camrtc_hsp_init(dev);
	if (ret)
		goto fail;

	/* Power on device */
	ret = pm_runtime_get_sync(dev);
	if (ret < 0)
		goto fail;

	/* Clocks are on, resets are deasserted, we can touch the hardware */

	/* Tegra-agic driver routes IRQs when probing, do it when powered */
	ret = tegra_camrtc_get_irqs(dev);
	if (ret)
		goto put_and_fail;

	rtcpu->ivc = tegra_ivc_bus_create(dev);
	if (IS_ERR(rtcpu->ivc)) {
		ret = PTR_ERR(rtcpu->ivc);
		rtcpu->ivc = NULL;
		goto put_and_fail;
	}

	rtcpu->monitor = tegra_camrtc_mon_create(dev);
	if (IS_ERR(rtcpu->monitor)) {
		ret = PTR_ERR(rtcpu->monitor);
		goto put_and_fail;
	}

	if (of_property_read_bool(dev->of_node, NV(disable-runtime-pm)) ||
		/* APE power domain powergates APE block when suspending */
		/* This won't do */
		(DISABLE_APE_RUNTIME_PM && pdata->id == TEGRA_CAM_RTCPU_APE)) {
		pm_runtime_get(dev);
	}

	ret = camrtc_hsp_get_fw_hash(rtcpu->hsp,
			rtcpu->fw_hash, sizeof(rtcpu->fw_hash));
	if (ret == 0)
		devm_tegrafw_register(dev,
			name != pdata->name ? name :  "camrtc",
			TFW_NORMAL, tegra_camrtc_print_version, NULL);

	tegra_camrtc_set_online(dev, true);

	pm_runtime_put(dev);

	/* Print firmware version */
	tegra_camrtc_log_fw_version(dev);

	s_dev = dev;

	dev_dbg(dev, "successfully probed RTCPU on %s\n", name);

	return 0;

put_and_fail:
	pm_runtime_dont_use_autosuspend(dev);
	pm_runtime_put_sync_suspend(dev);
fail:
	tegra_cam_rtcpu_remove(pdev);
	return ret;
}

int tegra_camrtc_reboot(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	if (pm_runtime_suspended(dev)) {
		dev_info(dev, "cannot reboot while suspended\n");
		return -EIO;
	}

	if (!rtcpu->powered)
		return -EIO;

	rtcpu->boot_sync_done = false;
	rtcpu->fw_active = false;

	pm_runtime_mark_last_busy(dev);

	tegra_camrtc_set_online(dev, false);

	tegra_camrtc_assert_resets(dev);

	rtcpu->powered = false;

	return tegra_camrtc_boot(dev);
}
EXPORT_SYMBOL(tegra_camrtc_reboot);

int tegra_camrtc_restore(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	if (rtcpu->monitor)
		return tegra_camrtc_mon_restore_rtcpu(rtcpu->monitor);
	else
		return tegra_camrtc_reboot(dev);
}
EXPORT_SYMBOL(tegra_camrtc_restore);

bool tegra_camrtc_is_rtcpu_alive(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	return rtcpu->online;
}
EXPORT_SYMBOL(tegra_camrtc_is_rtcpu_alive);

bool tegra_camrtc_is_rtcpu_powered(void)
{
	struct tegra_cam_rtcpu *rtcpu;

	if (s_dev) {
		rtcpu = dev_get_drvdata(s_dev);
		return rtcpu->powered;
	}

	return false;
}
EXPORT_SYMBOL(tegra_camrtc_is_rtcpu_powered);

void tegra_camrtc_flush_trace(struct device *dev)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);

	tegra_rtcpu_trace_flush(rtcpu->tracer);
}
EXPORT_SYMBOL(tegra_camrtc_flush_trace);

static int tegra_camrtc_halt(struct device *dev, char const *op)
{
	struct tegra_cam_rtcpu *rtcpu = dev_get_drvdata(dev);
	bool online = rtcpu->online;
	int err = 0;

	tegra_camrtc_pm_start(dev, op);

	tegra_camrtc_set_online(dev, false);

	if (!rtcpu->powered) {
		tegra_camrtc_pm_done(dev, op, 0);
		return 0;
	}

	if (!pm_runtime_suspended(dev))
		/* Tell CAMRTC that it should power down camera devices */
		err = tegra_camrtc_fw_suspend(dev);

	if (online && rtcpu->hsp && err == 0)
		/* Tell CAMRTC that shared memory is going away */
		err = camrtc_hsp_bye(rtcpu->hsp);

	if (err == 0)
		/* Don't bother to check for WFI if core is unresponsive */
		tegra_camrtc_wait_for_idle(dev);

	tegra_camrtc_poweroff(dev);

	tegra_camrtc_pm_done(dev, op, err); /* note this is not returned */

	return 0;
}

static int tegra_camrtc_suspend(struct device *dev)
{
	return tegra_camrtc_halt(dev, "suspend");
}

static int tegra_camrtc_resume(struct device *dev)
{
	int err;

	tegra_camrtc_pm_start(dev, "resume");

	pm_runtime_mark_last_busy(dev);

	/* Call tegra_cam_rtcpu_runtime_resume() - unless PM thinks dev is ACTIVE */
	err = pm_runtime_resume(dev);
	if (err == 1)
		/* Already marked ACTIVE, boot explicitly */
		err = tegra_camrtc_boot(dev);

	tegra_camrtc_pm_done(dev, "resume", err);

	return err;
}

static void tegra_cam_rtcpu_shutdown(struct platform_device *pdev)
{
	tegra_camrtc_halt(&pdev->dev, "shutdown");
}

static const struct of_device_id tegra_cam_rtcpu_of_match[] = {
	{
		.compatible = NV(tegra186-sce-ivc), .data = &sce_pdata
	},
	{
		.compatible = NV(tegra186-ape-ivc), .data = &ape_pdata
	},
	{
		.compatible = NV(tegra194-rce), .data = &rce_pdata
	},
	{ },
};
MODULE_DEVICE_TABLE(of, tegra_cam_rtcpu_of_match);

static const struct dev_pm_ops tegra_cam_rtcpu_pm_ops = {
	.suspend = tegra_camrtc_suspend,
	.resume = tegra_camrtc_resume,
	.runtime_suspend = tegra_cam_rtcpu_runtime_suspend,
	.runtime_resume = tegra_cam_rtcpu_runtime_resume,
	.runtime_idle = tegra_cam_rtcpu_runtime_idle,
};

static struct platform_driver tegra_cam_rtcpu_driver = {
	.driver = {
		.name	= "tegra186-cam-rtcpu",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(tegra_cam_rtcpu_of_match),
#ifdef CONFIG_PM
		.pm = &tegra_cam_rtcpu_pm_ops,
#endif
	},
	.probe = tegra_cam_rtcpu_probe,
	.remove = tegra_cam_rtcpu_remove,
	.shutdown = tegra_cam_rtcpu_shutdown,
};
module_platform_driver(tegra_cam_rtcpu_driver);

MODULE_DESCRIPTION("CAMERA RTCPU driver");
MODULE_AUTHOR("NVIDIA");
MODULE_LICENSE("GPL v2");
