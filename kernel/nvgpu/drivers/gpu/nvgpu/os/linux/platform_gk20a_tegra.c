/*
 * GK20A Tegra Platform Interface
 *
 * Copyright (c) 2014-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/of_platform.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <uapi/linux/nvgpu.h>
#include <linux/dma-buf.h>
#include <linux/reset.h>
#include <nvgpu/vpr.h>

#if defined(CONFIG_TEGRA_DVFS)
#include <linux/tegra_soctherm.h>
#endif
#if defined(CONFIG_NVGPU_TEGRA_FUSE) || NVGPU_VPR_RESIZE_SUPPORTED
#include <linux/platform/tegra/common.h>
#endif

#if defined(CONFIG_NVGPU_NVMAP_NEXT)
#include <linux/nvmap_exports.h>
#endif

#ifdef CONFIG_NV_TEGRA_MC
#include <linux/platform/tegra/mc.h>
#endif /* CONFIG_NV_TEGRA_MC */

#include <linux/clk/tegra.h>

#if defined(CONFIG_COMMON_CLK) && defined(CONFIG_TEGRA_DVFS)
#include <soc/tegra/tegra-dvfs.h>
#endif /* CONFIG_COMMON_CLK && CONFIG_TEGRA_DVFS */

#ifdef CONFIG_TEGRA_BWMGR
#include <linux/platform/tegra/emc_bwmgr.h>
#include <linux/platform/tegra/tegra_emc.h>
#endif

#ifdef CONFIG_NVGPU_TEGRA_FUSE
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif
#endif

#include <nvgpu/kmem.h>
#include <nvgpu/bug.h>
#include <nvgpu/enabled.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/gr/global_ctx.h>
#include <nvgpu/nvhost.h>
#include <nvgpu/pmu/pmu_perfmon.h>
#include <nvgpu/linux/dma.h>
#include <nvgpu/soc.h>
#include <nvgpu/errata.h>

#include "hal/clk/clk_gm20b.h"

#include "scale.h"
#include "platform_gk20a.h"
#include "clk.h"
#include "os_linux.h"

#include <soc/tegra/pmc.h>

#define TEGRA_GK20A_BW_PER_FREQ 32
#define TEGRA_GM20B_BW_PER_FREQ 64
#define TEGRA_DDR3_BW_PER_FREQ 16
#define TEGRA_DDR4_BW_PER_FREQ 16
#define MC_CLIENT_GPU 34
#define PMC_GPU_RG_CNTRL_0		0x2d4

#ifdef CONFIG_COMMON_CLK
#define GPU_RAIL_NAME "vdd-gpu"
#else
#define GPU_RAIL_NAME "vdd_gpu"
#endif

#ifdef CONFIG_NVGPU_VPR
extern struct device tegra_vpr_dev;
#endif

#ifdef CONFIG_TEGRA_BWMGR
struct gk20a_emc_params {
	unsigned long bw_ratio;
	unsigned long freq_last_set;
	struct tegra_bwmgr_client *bwmgr_cl;
};
#else
struct gk20a_emc_params {
	unsigned long bw_ratio;
	unsigned long freq_last_set;
};
#endif

#define MHZ_TO_HZ(x) ((x) * 1000000)
#define HZ_TO_MHZ(x) ((x) / 1000000)

#ifdef CONFIG_NVGPU_VPR
static void gk20a_tegra_secure_page_destroy(struct gk20a *g,
				       struct secure_page_buffer *secure_buffer)
{
#if defined(CONFIG_NVGPU_NVMAP_NEXT)
	nvmap_dma_free_attrs(&tegra_vpr_dev, secure_buffer->size,
			(void *)(uintptr_t)secure_buffer->phys,
			secure_buffer->phys, DMA_ATTR_NO_KERNEL_MAPPING);
#else
	dma_free_attrs(&tegra_vpr_dev, secure_buffer->size,
			(void *)(uintptr_t)secure_buffer->phys,
			secure_buffer->phys, DMA_ATTR_NO_KERNEL_MAPPING);
#endif

	secure_buffer->destroy = NULL;
}

static void gk20a_free_secure_buffer(struct gk20a *g,
				struct nvgpu_mem *mem)
{
	if (!nvgpu_mem_is_valid(mem))
		return;

	if (mem->priv.sgt != NULL) {
		sg_free_table(mem->priv.sgt);
	}

	nvgpu_kfree(g, mem->priv.sgt);
	mem->priv.sgt = NULL;

	mem->size = 0;
	mem->aligned_size = 0;
	mem->aperture = APERTURE_INVALID;

}

static int gk20a_tegra_secure_alloc(struct gk20a *g,
				struct nvgpu_mem *desc_mem, size_t size,
				global_ctx_mem_destroy_fn *destroy)
{
	struct device *dev = dev_from_gk20a(g);
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct secure_page_buffer *secure_buffer = &platform->secure_buffer;
	dma_addr_t phys;
	struct sg_table *sgt;
	struct page *page;
	int err = 0;
	size_t aligned_size = PAGE_ALIGN(size);

	if (nvgpu_mem_is_valid(desc_mem))
		return 0;

	/* We ran out of preallocated memory */
	if (secure_buffer->used + aligned_size > secure_buffer->size) {
		nvgpu_err(platform->g, "failed to alloc %zu bytes of VPR, %zu/%zu used",
				size, secure_buffer->used, secure_buffer->size);
		return -ENOMEM;
	}

	phys = secure_buffer->phys + secure_buffer->used;

	sgt = nvgpu_kzalloc(platform->g, sizeof(*sgt));
	if (!sgt) {
		nvgpu_err(platform->g, "failed to allocate memory");
		return -ENOMEM;
	}
	err = sg_alloc_table(sgt, 1, GFP_KERNEL);
	if (err) {
		nvgpu_err(platform->g, "failed to allocate sg_table");
		goto fail_sgt;
	}
	page = phys_to_page(phys);
	sg_set_page(sgt->sgl, page, size, 0);
	/* This bypasses SMMU for VPR during gmmu_map. */
	sg_dma_address(sgt->sgl) = 0;

	*destroy = gk20a_free_secure_buffer;

	desc_mem->priv.sgt = sgt;
	desc_mem->size = size;
	desc_mem->aperture = APERTURE_SYSMEM;

	secure_buffer->used += aligned_size;

	return err;

fail_sgt:
	nvgpu_kfree(platform->g, sgt);
	return err;
}
#endif

#ifdef CONFIG_TEGRA_BWMGR
/*
 * gk20a_tegra_get_emc_rate()
 *
 * This function returns the minimum emc clock based on gpu frequency
 */

static unsigned long gk20a_tegra_get_emc_rate(struct gk20a *g,
				struct gk20a_emc_params *emc_params)
{
	unsigned long gpu_freq, gpu_fmax_at_vmin;
	unsigned long emc_rate, emc_scale;

	gpu_freq = clk_get_rate(g->clk.tegra_clk);
#ifdef CONFIG_TEGRA_DVFS
	gpu_fmax_at_vmin = tegra_dvfs_get_fmax_at_vmin_safe_t(
		clk_get_parent(g->clk.tegra_clk));
#else
	gpu_fmax_at_vmin = 0;
#endif

	/* When scaling emc, account for the gpu load when the
	 * gpu frequency is less than or equal to fmax@vmin. */
	if (gpu_freq <= gpu_fmax_at_vmin)
		emc_scale = min(nvgpu_pmu_perfmon_get_load_avg(g->pmu),
					g->emc3d_ratio);
	else
		emc_scale = g->emc3d_ratio;

	emc_rate =
		(HZ_TO_MHZ(gpu_freq) * emc_params->bw_ratio * emc_scale) / 1000;

	return MHZ_TO_HZ(emc_rate);
}
#endif

/*
 * gk20a_tegra_prescale(profile, freq)
 *
 * This function informs EDP about changed constraints.
 */

static void gk20a_tegra_prescale(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);
	u32 avg = 0;

	nvgpu_pmu_load_norm(g, &avg);
}

/*
 * gk20a_tegra_calibrate_emc()
 *
 */

static void gk20a_tegra_calibrate_emc(struct gk20a_platform *platform,
			       struct gk20a_emc_params *emc_params)
{
	enum tegra_chip_id cid = platform->platform_chip_id;
	long gpu_bw, emc_bw;

	/* store gpu bw based on soc */
	switch (cid) {
	case TEGRA_210:
		gpu_bw = TEGRA_GM20B_BW_PER_FREQ;
		break;
	case TEGRA_124:
	case TEGRA_132:
		gpu_bw = TEGRA_GK20A_BW_PER_FREQ;
		break;
	default:
		gpu_bw = 0;
		break;
	}

	/* TODO detect DDR type.
	 * Okay for now since DDR3 and DDR4 have the same BW ratio */
	emc_bw = TEGRA_DDR3_BW_PER_FREQ;

	/* Calculate the bandwidth ratio of gpu_freq <-> emc_freq
	 *   NOTE the ratio must come out as an integer */
	emc_params->bw_ratio = (gpu_bw / emc_bw);
}

#ifdef CONFIG_TEGRA_BWMGR
#ifdef CONFIG_TEGRA_DVFS
static void gm20b_bwmgr_set_rate(struct gk20a_platform *platform, bool enb)
{
	struct gk20a_scale_profile *profile = platform->g->scale_profile;
	struct gk20a_emc_params *params;
	unsigned long rate;

	if (!profile || !profile->private_data)
		return;

	params = (struct gk20a_emc_params *)profile->private_data;
	rate = (enb) ? params->freq_last_set : 0;
	tegra_bwmgr_set_emc(params->bwmgr_cl, rate, TEGRA_BWMGR_SET_EMC_FLOOR);
}
#endif

static void gm20b_tegra_postscale(struct device *dev, unsigned long freq)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct gk20a_scale_profile *profile = platform->g->scale_profile;
	struct gk20a_emc_params *emc_params;
	unsigned long emc_rate;

	if (!profile || !profile->private_data)
		return;

	emc_params = profile->private_data;
	emc_rate = gk20a_tegra_get_emc_rate(get_gk20a(dev), emc_params);

	if (emc_rate > tegra_bwmgr_get_max_emc_rate())
		emc_rate = tegra_bwmgr_get_max_emc_rate();

	emc_params->freq_last_set = emc_rate;
	if (platform->is_railgated && platform->is_railgated(dev))
		return;

	tegra_bwmgr_set_emc(emc_params->bwmgr_cl, emc_rate,
			TEGRA_BWMGR_SET_EMC_FLOOR);

}

#endif

#if defined(CONFIG_TEGRA_DVFS)
/*
 * gk20a_tegra_is_railgated()
 *
 * Check status of gk20a power rail
 */

static bool gk20a_tegra_is_railgated(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	bool ret = false;

#ifdef CONFIG_TEGRA_DVFS
	if (!nvgpu_is_enabled(g, NVGPU_IS_FMODEL))
		ret = !tegra_dvfs_is_rail_up(platform->gpu_rail);
#endif

	return ret;
}

/*
 * gm20b_tegra_railgate()
 *
 * Gate (disable) gm20b power rail
 */

static int gm20b_tegra_railgate(struct device *dev)
{
	struct gk20a *g = get_gk20a(dev);
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	int ret = 0;

#ifdef CONFIG_NV_TEGRA_MC
#ifdef CONFIG_TEGRA_DVFS
	if (nvgpu_is_enabled(g, NVGPU_IS_FMODEL) ||
	    !tegra_dvfs_is_rail_up(platform->gpu_rail))
		return 0;
#endif

	tegra_mc_flush(MC_CLIENT_GPU);

	udelay(10);

	tegra_pmc_gpu_clamp_enable();

	udelay(10);

	platform->reset_assert(dev);

	udelay(10);

	/*
	 * GPCPLL is already disabled before entering this function; reference
	 * clocks are enabled until now - disable them just before rail gating
	 */
	clk_disable_unprepare(platform->clk_reset);
	clk_disable_unprepare(platform->clk[0]);
	clk_disable_unprepare(platform->clk[1]);
	if (platform->clk[3])
		clk_disable_unprepare(platform->clk[3]);

	udelay(10);

	tegra_soctherm_gpu_tsens_invalidate(1);

#ifdef CONFIG_TEGRA_DVFS
	if (tegra_dvfs_is_rail_up(platform->gpu_rail)) {
		ret = tegra_dvfs_rail_power_down(platform->gpu_rail);
		if (ret)
			goto err_power_off;
	} else
		pr_info("No GPU regulator?\n");
#endif

#ifdef CONFIG_TEGRA_BWMGR
	gm20b_bwmgr_set_rate(platform, false);
#endif

	return 0;

#else
	ret = -ENOTSUP;
#endif /* CONFIG_NV_TEGRA_MC */

err_power_off:
	nvgpu_err(platform->g, "Could not railgate GPU");
	return ret;
}


/*
 * gm20b_tegra_unrailgate()
 *
 * Ungate (enable) gm20b power rail
 */

static int gm20b_tegra_unrailgate(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct gk20a *g = platform->g;
	int ret = 0;
	bool first = false;

	if (nvgpu_is_enabled(g, NVGPU_IS_FMODEL))
		return 0;

	ret = tegra_dvfs_rail_power_up(platform->gpu_rail);
	if (ret)
		return ret;

#ifdef CONFIG_TEGRA_BWMGR
	gm20b_bwmgr_set_rate(platform, true);
#endif

	tegra_soctherm_gpu_tsens_invalidate(0);

	if (!platform->clk_reset) {
		platform->clk_reset = clk_get(dev, "gpu_gate");
		if (IS_ERR(platform->clk_reset)) {
			nvgpu_err(g, "fail to get gpu reset clk");
			goto err_clk_on;
		}
	}

	if (!first) {
		ret = clk_prepare_enable(platform->clk_reset);
		if (ret) {
			nvgpu_err(g, "could not turn on gpu_gate");
			goto err_clk_on;
		}

		ret = clk_prepare_enable(platform->clk[0]);
		if (ret) {
			nvgpu_err(g, "could not turn on gpu pll");
			goto err_clk_on;
		}
		ret = clk_prepare_enable(platform->clk[1]);
		if (ret) {
			nvgpu_err(g, "could not turn on pwr clock");
			goto err_clk_on;
		}

		if (platform->clk[3]) {
			ret = clk_prepare_enable(platform->clk[3]);
			if (ret) {
				nvgpu_err(g, "could not turn on fuse clock");
				goto err_clk_on;
			}
		}
	}

	udelay(10);

	platform->reset_assert(dev);

	udelay(10);

	tegra_pmc_gpu_clamp_disable();

	udelay(10);

	clk_disable(platform->clk_reset);
	platform->reset_deassert(dev);
	clk_enable(platform->clk_reset);

#ifdef CONFIG_NV_TEGRA_MC
	/* Flush MC after boot/railgate/SC7 */
	tegra_mc_flush(MC_CLIENT_GPU);

	udelay(10);

	tegra_mc_flush_done(MC_CLIENT_GPU);
#endif

	udelay(10);

	return 0;

err_clk_on:
#ifdef CONFIG_TEGRA_DVFS
	tegra_dvfs_rail_power_down(platform->gpu_rail);
#endif

	return ret;
}
#endif


static struct {
	char *name;
	unsigned long default_rate;
} tegra_gk20a_clocks[] = {
	{"gpu_ref", UINT_MAX},
	{"pll_p_out5", 204000000},
	{"emc", UINT_MAX},
	{"fuse", UINT_MAX},
};



/*
 * gk20a_tegra_get_clocks()
 *
 * This function finds clocks in tegra platform and populates
 * the clock information to gk20a platform data.
 */

static int gk20a_tegra_get_clocks(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	char devname[16];
	unsigned int i;
	int ret = 0;

	BUG_ON(GK20A_CLKS_MAX < ARRAY_SIZE(tegra_gk20a_clocks));

	(void) snprintf(devname, sizeof(devname), "tegra_%s", dev_name(dev));

	platform->num_clks = 0;
	for (i = 0; i < ARRAY_SIZE(tegra_gk20a_clocks); i++) {
		long rate = tegra_gk20a_clocks[i].default_rate;
		struct clk *c;

		c = clk_get_sys(devname, tegra_gk20a_clocks[i].name);
		if (IS_ERR(c)) {
			ret = PTR_ERR(c);
			goto err_get_clock;
		}
		rate = clk_round_rate(c, rate);
		clk_set_rate(c, rate);
		platform->clk[i] = c;
	}
	platform->num_clks = i;

	return 0;

err_get_clock:

	while (i--)
		clk_put(platform->clk[i]);
	return ret;
}

#if defined(CONFIG_RESET_CONTROLLER) && defined(CONFIG_COMMON_CLK)
static int gm20b_tegra_reset_assert(struct device *dev)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);

	if (!platform->reset_control) {
		WARN(1, "Reset control not initialized\n");
		return -ENOSYS;
	}

	return reset_control_assert(platform->reset_control);
}

static int gm20b_tegra_reset_deassert(struct device *dev)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);

	if (!platform->reset_control) {
		WARN(1, "Reset control not initialized\n");
		return -ENOSYS;
	}

	return reset_control_deassert(platform->reset_control);
}
#endif

static void gk20a_tegra_scale_init(struct device *dev)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a_scale_profile *profile = platform->g->scale_profile;
	struct gk20a_emc_params *emc_params;

	if (!profile)
		return;

	if (profile->private_data)
		return;

	emc_params = nvgpu_kzalloc(platform->g, sizeof(*emc_params));
	if (!emc_params)
		return;

	emc_params->freq_last_set = -1;
	gk20a_tegra_calibrate_emc(platform, emc_params);

#ifdef CONFIG_TEGRA_BWMGR
	emc_params->bwmgr_cl = tegra_bwmgr_register(TEGRA_BWMGR_CLIENT_GPU);
	if (!emc_params->bwmgr_cl) {
		nvgpu_log_info(platform->g,
			       "%s Missing GPU BWMGR client\n", __func__);
		return;
	}
#endif

	profile->private_data = emc_params;
}

static void gk20a_tegra_scale_exit(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct gk20a_scale_profile *profile = platform->g->scale_profile;
	struct gk20a_emc_params *emc_params;

	if (!profile)
		return;

	emc_params = profile->private_data;
#ifdef CONFIG_TEGRA_BWMGR
	tegra_bwmgr_unregister(emc_params->bwmgr_cl);
#endif

	nvgpu_kfree(platform->g, profile->private_data);
}

void gk20a_tegra_debug_dump(struct device *dev)
{
#ifdef CONFIG_TEGRA_GK20A_NVHOST
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a *g = platform->g;

	if (g->nvhost)
		nvgpu_nvhost_debug_dump_device(g->nvhost);
#endif
}

int gk20a_tegra_busy(struct device *dev)
{
#ifdef CONFIG_TEGRA_GK20A_NVHOST
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a *g = platform->g;

	if (g->nvhost)
		return nvgpu_nvhost_module_busy_ext(g->nvhost);
#endif
	return 0;
}

void gk20a_tegra_idle(struct device *dev)
{
#ifdef CONFIG_TEGRA_GK20A_NVHOST
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a *g = platform->g;

	if (g->nvhost)
		nvgpu_nvhost_module_idle_ext(g->nvhost);
#endif
}

int gk20a_tegra_init_secure_alloc(struct gk20a_platform *platform)
{
#ifdef CONFIG_NVGPU_VPR
	struct gk20a *g = platform->g;
	struct secure_page_buffer *secure_buffer = &platform->secure_buffer;
	dma_addr_t iova;

	if (nvgpu_platform_is_simulation(g)) {
		/*
		 * On simulation platform, VPR is only supported with
		 * vdk frontdoor boot and gpu frontdoor mode.
		 */
#if NVGPU_VPR_RESIZE_SUPPORTED
		tegra_unregister_idle_unidle(gk20a_do_idle);
#endif
		nvgpu_log_info(g,
			"VPR is not supported on simulation platform");
		return 0;
	}

#if NVGPU_CPU_PAGE_SIZE > 4096
	platform->secure_buffer_size += SZ_64K;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
	tegra_vpr_dev.coherent_dma_mask = DMA_BIT_MASK(32);
#endif
#if defined(CONFIG_NVGPU_NVMAP_NEXT)
	(void)nvmap_dma_alloc_attrs(&tegra_vpr_dev,
				    platform->secure_buffer_size, &iova,
				    GFP_KERNEL, DMA_ATTR_NO_KERNEL_MAPPING);
#else
	(void)dma_alloc_attrs(&tegra_vpr_dev, platform->secure_buffer_size, &iova,
				      GFP_KERNEL, DMA_ATTR_NO_KERNEL_MAPPING);
#endif
	/* Some platforms disable VPR. In that case VPR allocations always
	 * fail. Just disable VPR usage in nvgpu in that case. */
	if (dma_mapping_error(&tegra_vpr_dev, iova)) {
#if NVGPU_VPR_RESIZE_SUPPORTED
		tegra_unregister_idle_unidle(gk20a_do_idle);
#endif
		return 0;
	}

	secure_buffer->size = platform->secure_buffer_size;
	secure_buffer->phys = iova;
	secure_buffer->destroy = gk20a_tegra_secure_page_destroy;

	g->ops.secure_alloc = gk20a_tegra_secure_alloc;
	nvgpu_set_enabled(g, NVGPU_SUPPORT_VPR, true);
#endif
	return 0;
}

#ifdef CONFIG_COMMON_CLK
static struct clk *gk20a_clk_get(struct gk20a *g)
{
	if (!g->clk.tegra_clk) {
		struct clk *clk, *clk_parent;
		char clk_dev_id[32];
		struct device *dev = dev_from_gk20a(g);

		(void) snprintf(clk_dev_id, 32, "tegra_%s", dev_name(dev));

		clk = clk_get_sys(clk_dev_id, "gpu");
		if (IS_ERR(clk)) {
			nvgpu_err(g, "fail to get tegra gpu clk %s/gpu\n",
				  clk_dev_id);
			return NULL;
		}

		clk_parent = clk_get_parent(clk);
		if (IS_ERR_OR_NULL(clk_parent)) {
			nvgpu_err(g, "fail to get tegra gpu clk parent%s/gpu\n",
				  clk_dev_id);
			return NULL;
		}

		g->clk.tegra_clk = clk;
		g->clk.tegra_clk_parent = clk_parent;
	}

	return g->clk.tegra_clk;
}

static struct clk_gk20a *clk_gk20a_from_hw(struct clk_hw *hw)
{
	return (struct clk_gk20a *)
		((uintptr_t)hw - offsetof(struct clk_gk20a, hw));
}

static int gm20b_clk_prepare_ops(struct clk_hw *hw)
{
	struct clk_gk20a *clk = clk_gk20a_from_hw(hw);
	return gm20b_clk_prepare(clk);
}

static void gm20b_clk_unprepare_ops(struct clk_hw *hw)
{
	struct clk_gk20a *clk = clk_gk20a_from_hw(hw);
	gm20b_clk_unprepare(clk);
}

static int gm20b_clk_is_prepared_ops(struct clk_hw *hw)
{
	struct clk_gk20a *clk = clk_gk20a_from_hw(hw);
	return gm20b_clk_is_prepared(clk);
}

static unsigned long gm20b_recalc_rate_ops(struct clk_hw *hw, unsigned long parent_rate)
{
	struct clk_gk20a *clk = clk_gk20a_from_hw(hw);
	return gm20b_recalc_rate(clk, parent_rate);
}

static int gm20b_gpcclk_set_rate_ops(struct clk_hw *hw, unsigned long rate,
				 unsigned long parent_rate)
{
	struct clk_gk20a *clk = clk_gk20a_from_hw(hw);
	return gm20b_gpcclk_set_rate(clk, rate, parent_rate);
}

static long gm20b_round_rate_ops(struct clk_hw *hw, unsigned long rate,
			     unsigned long *parent_rate)
{
	struct clk_gk20a *clk = clk_gk20a_from_hw(hw);
	return gm20b_round_rate(clk, rate, parent_rate);
}

static const struct clk_ops gm20b_clk_ops = {
	.prepare = gm20b_clk_prepare_ops,
	.unprepare = gm20b_clk_unprepare_ops,
	.is_prepared = gm20b_clk_is_prepared_ops,
	.recalc_rate = gm20b_recalc_rate_ops,
	.set_rate = gm20b_gpcclk_set_rate_ops,
	.round_rate = gm20b_round_rate_ops,
};

static int gm20b_register_gpcclk(struct gk20a *g)
{
	const char *parent_name = "pllg_ref";
	struct clk_gk20a *clk = &g->clk;
	struct clk_init_data init;
	struct clk *c;
	int err = 0;

	/* make sure the clock is available */
	if (!gk20a_clk_get(g))
		return -ENOSYS;

	err = gm20b_init_clk_setup_sw(g);
	if (err)
		return err;

	init.name = "gpcclk";
	init.ops = &gm20b_clk_ops;
	init.parent_names = &parent_name;
	init.num_parents = 1;
	init.flags = 0;

	/* Data in .init is copied by clk_register(), so stack variable OK */
	clk->hw.init = &init;
	c = clk_register(dev_from_gk20a(g), &clk->hw);
	if (IS_ERR(c)) {
		nvgpu_err(g, "Failed to register GPCPLL clock");
		return -EINVAL;
	}

	clk->g = g;
	clk_register_clkdev(c, "gpcclk", "gpcclk");

	return err;
}
#endif /* CONFIG_COMMON_CLK */

static int gk20a_tegra_probe(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	struct device_node *np = dev->of_node;
	struct device_node *of_chosen;
	bool joint_xpu_rail = false;
	int ret;
	struct gk20a *g = platform->g;

#if defined(CONFIG_COMMON_CLK) && defined(CONFIG_TEGRA_DVFS)
	/* DVFS is not guaranteed to be initialized at the time of probe on
	 * kernels with Common Clock Framework enabled.
	 */
	if (!platform->gpu_rail) {
		platform->gpu_rail = tegra_dvfs_get_rail_by_name(GPU_RAIL_NAME);
		if (!platform->gpu_rail) {
			nvgpu_log_info(g, "deferring probe no gpu_rail");
			return -EPROBE_DEFER;
		}
	}

	if (!tegra_dvfs_is_rail_ready(platform->gpu_rail)) {
		nvgpu_log_info(g, "deferring probe gpu_rail not ready");
		return -EPROBE_DEFER;
	}
#endif

#ifdef CONFIG_TEGRA_GK20A_NVHOST
	ret = nvgpu_get_nvhost_dev(platform->g);
	if (ret)
		return ret;
#endif

#ifdef CONFIG_OF
	of_chosen = of_find_node_by_path("/chosen");
	if (!of_chosen)
		return -ENODEV;

	joint_xpu_rail = of_property_read_bool(of_chosen,
				"nvidia,tegra-joint_xpu_rail");
#endif

	if (joint_xpu_rail) {
		nvgpu_log_info(g, "XPU rails are joint\n");
		nvgpu_set_enabled(g, NVGPU_CAN_RAILGATE, false);
		platform->can_railgate_init = false;
	}

	platform->g->clk.gpc_pll.id = GK20A_GPC_PLL;
	if (nvgpu_is_errata_present(g, NVGPU_ERRATA_1547668)) {
		/* Disable railgating and scaling
		   irrespective of platform data if the rework was not made. */
		np = of_find_node_by_path("/gpu-dvfs-rework");
		if (!(np && of_device_is_available(np))) {
			platform->devfreq_governor = "";
			dev_warn(dev, "board does not support scaling");
		}
		platform->g->clk.gpc_pll.id = GM20B_GPC_PLL_B1;
#ifdef CONFIG_NVGPU_TEGRA_FUSE
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
		if (tegra_chip_get_revision() > TEGRA210_REVISION_A04p)
#else
		if (tegra_get_chip_id() == TEGRA210 &&
		    tegra_chip_get_revision() > TEGRA_REVISION_A04p)
#endif
			platform->g->clk.gpc_pll.id = GM20B_GPC_PLL_C1;
#endif
	}

	if (platform->platform_chip_id == TEGRA_132)
		platform->soc_name = "tegra13x";

	gk20a_tegra_get_clocks(dev);
	nvgpu_linux_init_clk_support(platform->g);

	if (platform->clk_register) {
		ret = platform->clk_register(platform->g);
		if (ret)
			return ret;
	}

	return 0;
}

static int gk20a_tegra_late_probe(struct device *dev)
{
	struct gk20a_platform *platform = dev_get_drvdata(dev);
	int ret;

	ret = gk20a_tegra_init_secure_alloc(platform);
	if (ret)
		return ret;

	return 0;
}

static int gk20a_tegra_remove(struct device *dev)
{
	/* deinitialise tegra specific scaling quirks */
	gk20a_tegra_scale_exit(dev);

#ifdef CONFIG_TEGRA_GK20A_NVHOST
	nvgpu_free_nvhost_dev(get_gk20a(dev));
#endif

	return 0;
}

static int gk20a_tegra_suspend(struct device *dev)
{
	return 0;
}

#if defined(CONFIG_COMMON_CLK)
static long gk20a_round_clk_rate(struct device *dev, unsigned long rate)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a *g = platform->g;

	/* make sure the clock is available */
	if (!gk20a_clk_get(g))
		return rate;

	return clk_round_rate(clk_get_parent(g->clk.tegra_clk), rate);
}

static int gk20a_clk_get_freqs(struct device *dev,
				unsigned long **freqs, int *num_freqs)
{
	struct gk20a_platform *platform = gk20a_get_platform(dev);
	struct gk20a *g = platform->g;

	/* make sure the clock is available */
	if (!gk20a_clk_get(g))
		return -ENOSYS;

#ifdef CONFIG_TEGRA_DVFS
	return tegra_dvfs_get_freqs(clk_get_parent(g->clk.tegra_clk),
				freqs, num_freqs);
#else
	return -EINVAL;
#endif
}
#endif

struct gk20a_platform gm20b_tegra_platform = {
#ifdef CONFIG_TEGRA_GK20A_NVHOST
	.has_syncpoints = true,
#endif
	.aggressive_sync_destroy_thresh = 64,

	/* power management configuration */
	.railgate_delay_init	= 500,
	.can_railgate_init	= true,
	.can_elpg_init          = true,
	.enable_slcg            = true,
	.enable_blcg            = true,
	.enable_elcg            = true,
	.can_slcg               = true,
	.can_blcg               = true,
	.can_elcg               = true,
	.enable_elpg            = true,
	.enable_elpg_ms		= false,
	.enable_aelpg           = true,
	.enable_perfmon         = true,
	.ptimer_src_freq	= 19200000,

	.ch_wdt_init_limit_ms = 7000,

	.probe = gk20a_tegra_probe,
	.late_probe = gk20a_tegra_late_probe,
	.remove = gk20a_tegra_remove,
	/* power management callbacks */
	.suspend = gk20a_tegra_suspend,

#if defined(CONFIG_TEGRA_DVFS)
	.railgate = gm20b_tegra_railgate,
	.unrailgate = gm20b_tegra_unrailgate,
	.is_railgated = gk20a_tegra_is_railgated,
#endif

	.busy = gk20a_tegra_busy,
	.idle = gk20a_tegra_idle,

#if defined(CONFIG_RESET_CONTROLLER) && defined(CONFIG_COMMON_CLK)
	.reset_assert = gm20b_tegra_reset_assert,
	.reset_deassert = gm20b_tegra_reset_deassert,
#else
	.reset_assert = gk20a_tegra_reset_assert,
	.reset_deassert = gk20a_tegra_reset_deassert,
#endif

#if defined(CONFIG_COMMON_CLK)
	.clk_round_rate = gk20a_round_clk_rate,
	.get_clk_freqs = gk20a_clk_get_freqs,
#endif

#ifdef CONFIG_COMMON_CLK
	.clk_register = gm20b_register_gpcclk,
#endif

	/* frequency scaling configuration */
	.initscale = gk20a_tegra_scale_init,
	.prescale = gk20a_tegra_prescale,
#ifdef CONFIG_TEGRA_BWMGR
	.postscale = gm20b_tegra_postscale,
#endif
	.devfreq_governor = "nvhost_podgov",
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
	.qos_min_notify = gk20a_scale_qos_min_notify,
	.qos_max_notify = gk20a_scale_qos_max_notify,
#else
	.qos_notify = gk20a_scale_qos_notify,
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0) */

	.dump_platform_dependencies = gk20a_tegra_debug_dump,

#ifdef CONFIG_NVGPU_SUPPORT_CDE
	.has_cde = true,
#endif

	.platform_chip_id = TEGRA_210,
	.soc_name = "tegra21x",

	.unified_memory = true,
	.dma_mask = DMA_BIT_MASK(34),
	.force_128K_pmu_vm = true,

	.secure_buffer_size = 335872,
};
