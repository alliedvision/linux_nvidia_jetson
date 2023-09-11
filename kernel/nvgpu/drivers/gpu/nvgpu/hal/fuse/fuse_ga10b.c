/*
 * GA10B FUSE
 *
 * Copyright (c) 2020-2023, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/types.h>
#include <nvgpu/fuse.h>
#include <nvgpu/enabled.h>
#include <nvgpu/io.h>
#include <nvgpu/soc.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/soc.h>

#include "fuse_ga10b.h"

#include <nvgpu/hw/ga10b/hw_fuse_ga10b.h>

#define AES_ALGO BIT(0)
#define PKC_ALGO BIT(1)

int ga10b_fuse_read_gcplex_config_fuse(struct gk20a *g, u32 *val)
{
	u32 reg_val = 0U;
	u32 fuse_val = 0U;

	/*
	 * SOC FUSE_GCPLEX_CONFIG_FUSE_0 bit(2) mapped to
	 * fuse_opt_wpr_enabled igpu fuse register
	 */
	reg_val = nvgpu_readl(g, fuse_opt_wpr_enabled_r());
	fuse_val |= (fuse_opt_wpr_enabled_data_v(reg_val) << 2U);

	/*
	 * SOC FUSE_GCPLEX_CONFIG_FUSE_0 bit(1) mapped to
	 * fuse_opt_vpr_enabled igpu fuse register
	 */
	reg_val = nvgpu_readl(g, fuse_opt_vpr_enabled_r());
	fuse_val |= (fuse_opt_vpr_enabled_data_v(reg_val) << 1U);

	/*
	 * SOC FUSE_GCPLEX_CONFIG_FUSE_0 bit(0) mapped to
	 * fuse_opt_vpr_auto_fetch_disable
	 */
	reg_val = nvgpu_readl(g, fuse_opt_vpr_auto_fetch_disable_r());
	fuse_val |= fuse_opt_vpr_auto_fetch_disable_data_v(reg_val);

	*val = fuse_val;

	return 0;
}

bool ga10b_fuse_is_opt_ecc_enable(struct gk20a *g)
{
	bool ecc_enable = nvgpu_readl(g, fuse_opt_ecc_en_r()) != 0U;

	if (nvgpu_platform_is_silicon(g) && !ecc_enable) {
#ifdef CONFIG_NVGPU_NON_FUSA
		nvgpu_log_info(g, "OPT_ECC_EN fuse not set");
#else
		nvgpu_err(g, "OPT_ECC_EN fuse not set");
#endif
	}

	return ecc_enable;
}

bool ga10b_fuse_is_opt_feature_override_disable(struct gk20a *g)
{
	return nvgpu_readl(g,
			fuse_opt_feature_fuses_override_disable_r()) != 0U;
}

u32 ga10b_fuse_status_opt_gpc(struct gk20a *g)
{
	return nvgpu_readl(g, fuse_status_opt_gpc_r());
}

u32 ga10b_fuse_status_opt_fbio(struct gk20a *g)
{
	return nvgpu_readl(g, fuse_status_opt_fbio_r());
}

u32 ga10b_fuse_status_opt_fbp(struct gk20a *g)
{
	return nvgpu_readl(g, fuse_status_opt_fbp_r());
}

u32 ga10b_fuse_status_opt_l2_fbp(struct gk20a *g, u32 fbp)
{
	return nvgpu_readl(g, fuse_ctrl_opt_ltc_fbp_r(fbp));
}

u32 ga10b_fuse_status_opt_tpc_gpc(struct gk20a *g, u32 gpc)
{
	return nvgpu_readl(g, fuse_status_opt_tpc_gpc_r(gpc));
}

void ga10b_fuse_ctrl_opt_tpc_gpc(struct gk20a *g, u32 gpc, u32 val)
{
	nvgpu_writel(g, fuse_ctrl_opt_tpc_gpc_r(gpc), val);
}

u32 ga10b_fuse_status_opt_pes_gpc(struct gk20a *g, u32 gpc)
{
	return nvgpu_readl(g, fuse_status_opt_pes_gpc_r(gpc));
}

u32 ga10b_fuse_status_opt_rop_gpc(struct gk20a *g, u32 gpc)
{
	return nvgpu_readl(g, fuse_status_opt_rop_gpc_r(gpc));
}

u32 ga10b_fuse_opt_priv_sec_en(struct gk20a *g)
{
	return nvgpu_readl(g, fuse_opt_priv_sec_en_r());
}

u32 ga10b_fuse_opt_sm_ttu_en(struct gk20a *g)
{
	return nvgpu_readl(g, fuse_opt_sm_ttu_en_r());
}

void ga10b_fuse_write_feature_override_ecc(struct gk20a *g, u32 val)
{
	nvgpu_writel(g, fuse_feature_override_ecc_r(), val);
}

void ga10b_fuse_write_feature_override_ecc_1(struct gk20a *g, u32 val)
{
	nvgpu_writel(g, fuse_feature_override_ecc_1_r(), val);
}

static void ga10b_fuse_read_feature_override_ecc_1(struct gk20a *g,
			struct nvgpu_fuse_feature_override_ecc *ecc_feature)
{
	u32 ecc_1 = nvgpu_readl(g, fuse_feature_override_ecc_1_r());

	ecc_feature->sm_l0_icache_enable =
		fuse_feature_override_ecc_1_sm_l0_icache_v(ecc_1) ==
			fuse_feature_override_ecc_1_sm_l0_icache_enabled_v();
	ecc_feature->sm_l0_icache_override =
		fuse_feature_override_ecc_1_sm_l0_icache_override_v(ecc_1) ==
		    fuse_feature_override_ecc_1_sm_l0_icache_override_true_v();

	ecc_feature->sm_l1_icache_enable =
		fuse_feature_override_ecc_1_sm_l1_icache_v(ecc_1) ==
			fuse_feature_override_ecc_1_sm_l1_icache_enabled_v();
	ecc_feature->sm_l1_icache_override =
		fuse_feature_override_ecc_1_sm_l1_icache_override_v(ecc_1) ==
		    fuse_feature_override_ecc_1_sm_l1_icache_override_true_v();
}

void ga10b_fuse_read_feature_override_ecc(struct gk20a *g,
			struct nvgpu_fuse_feature_override_ecc *ecc_feature)
{
	u32 ecc = nvgpu_readl(g, fuse_feature_override_ecc_r());

	ecc_feature->sm_lrf_enable =
		fuse_feature_override_ecc_sm_lrf_v(ecc) ==
			fuse_feature_override_ecc_sm_lrf_enabled_v();
	ecc_feature->sm_lrf_override =
		fuse_feature_override_ecc_sm_lrf_override_v(ecc) ==
			fuse_feature_override_ecc_sm_lrf_override_true_v();

	ecc_feature->sm_l1_data_enable =
		fuse_feature_override_ecc_sm_l1_data_v(ecc) ==
			fuse_feature_override_ecc_sm_l1_data_enabled_v();
	ecc_feature->sm_l1_data_override =
		fuse_feature_override_ecc_sm_l1_data_override_v(ecc) ==
			fuse_feature_override_ecc_sm_l1_data_override_true_v();

	ecc_feature->sm_l1_tag_enable =
		fuse_feature_override_ecc_sm_l1_tag_v(ecc) ==
			fuse_feature_override_ecc_sm_l1_tag_enabled_v();
	ecc_feature->sm_l1_tag_override =
		fuse_feature_override_ecc_sm_l1_tag_override_v(ecc) ==
			fuse_feature_override_ecc_sm_l1_tag_override_true_v();

	ecc_feature->ltc_enable =
		fuse_feature_override_ecc_ltc_v(ecc) ==
			fuse_feature_override_ecc_ltc_enabled_v();
	ecc_feature->ltc_override =
		fuse_feature_override_ecc_ltc_override_v(ecc) ==
			fuse_feature_override_ecc_ltc_override_true_v();

	ecc_feature->dram_enable =
		fuse_feature_override_ecc_dram_v(ecc) ==
			fuse_feature_override_ecc_dram_enabled_v();
	ecc_feature->dram_override =
		fuse_feature_override_ecc_dram_override_v(ecc) ==
			fuse_feature_override_ecc_dram_override_true_v();

	ecc_feature->sm_cbu_enable =
		fuse_feature_override_ecc_sm_cbu_v(ecc) ==
			fuse_feature_override_ecc_sm_cbu_enabled_v();
	ecc_feature->sm_cbu_override =
		fuse_feature_override_ecc_sm_cbu_override_v(ecc) ==
			fuse_feature_override_ecc_sm_cbu_override_true_v();

	ga10b_fuse_read_feature_override_ecc_1(g, ecc_feature);
}

int ga10b_fuse_read_per_device_identifier(struct gk20a *g, u64 *pdi)
{
	u32 lo = 0U;
	u32 hi = 0U;
	u32 pdi_loaded = 0U;
	u32 retries = GA10B_FUSE_READ_DEVICE_IDENTIFIER_RETRIES;

	if (nvgpu_platform_is_silicon(g)) {
		do {
			pdi_loaded = fuse_p2prx_pdi_loaded_v(
				nvgpu_readl(g, fuse_p2prx_pdi_r()));
			retries = nvgpu_safe_sub_u32(retries, 1U);
		} while ((pdi_loaded != fuse_p2prx_pdi_loaded_true_v()) &&
			retries > 0U);

		if (retries == 0U) {
			nvgpu_err(g, "Device identifier load failed");
			return -EAGAIN;
		}
	}

	lo = nvgpu_readl(g, fuse_opt_pdi_0_r());
	hi = nvgpu_readl(g, fuse_opt_pdi_1_r());

	*pdi = ((u64)lo) | (((u64)hi) << 32);

	return 0;
}

u32 ga10b_fuse_status_opt_emc(struct gk20a *g)
{
	u32 fuse_val = 0;
#ifdef __KERNEL__
	/*
	 * Read emc mask from fuse
	 * Note that 0:enable and 1:disable in value read from fuse so we've to
	 * flip the bits.
	 * Also set unused bits to zero
	 * Mapping of floorsweeping for MC/EMC based on channels,
	 * bit[i] floorsweeps channels 4i to 4i+3, the full mapping is
	 * opt_emc_disable[0]: channels 0-3, PD_emcba
	 * opt_emc_disable[1]: channels 4-7, PD_emcbb
	 * opt_emc_disable[2]: channels 8-11, PD_emcaa
	 * opt_emc_disable[3]: channels 12-15, PD_emcab
	 * The floorsweeping definition is a bitmap.
	 */
	nvgpu_tegra_fuse_read_opt_emc_disable(g, &fuse_val);
	fuse_val = ~fuse_val;
	fuse_val = fuse_val & nvgpu_safe_sub_u32(BIT32(4), 1U);
#else
	(void)g;
#endif
	return fuse_val;
}

u32 ga10b_fuse_opt_sec_debug_en(struct gk20a *g)
{
	return nvgpu_readl(g, fuse_opt_sec_debug_en_r());
}

u32 ga10b_fuse_opt_secure_source_isolation_en(struct gk20a *g)
{
	return nvgpu_readl(g, fuse_opt_secure_source_isolation_en_r());
}

/*
 * This function is same as gp10b_fuse_check_priv_security.
 * The only addition is check for secure_source_isolation_en fuse.
 */
int ga10b_fuse_check_priv_security(struct gk20a *g)
{
	u32 gcplex_config;
	bool is_wpr_enabled = false;
	bool is_auto_fetch_disable = false;

	if (g->ops.fuse.read_gcplex_config_fuse(g, &gcplex_config) != 0) {
		nvgpu_err(g, "err reading gcplex config fuse, check fuse clk");
		return -EINVAL;
	}

	if (g->ops.fuse.fuse_opt_priv_sec_en(g) != 0U) {
		nvgpu_log_info(g, "priv_sec_en = 1");
		if (g->ops.fuse.opt_sec_source_isolation_en != NULL) {
			if (g->ops.fuse.opt_sec_source_isolation_en(g) == 0U) {
				nvgpu_err(g, "priv_sec_en is set but "
					"secure_source_isolation_en is 0");
				return -EINVAL;
			}
			nvgpu_log_info(g, "secure_source_isolation_en = 1");
		}
		/*
		 * all falcons have to boot in LS mode and this needs
		 * wpr_enabled set to 1 and vpr_auto_fetch_disable
		 * set to 0. In this case gmmu tries to pull wpr
		 * and vpr settings from tegra mc
		 */
		nvgpu_set_enabled(g, NVGPU_SEC_PRIVSECURITY, true);
		nvgpu_set_enabled(g, NVGPU_SEC_SECUREGPCCS, true);
#ifdef CONFIG_NVGPU_SIM
		if (nvgpu_is_enabled(g, NVGPU_IS_FMODEL)) {
			/*
			 * Do not check other fuses as they are not yet modeled
                         * on FMODEL.
			 */
			return 0;
		}
#endif
		is_wpr_enabled = (gcplex_config &
			GCPLEX_CONFIG_WPR_ENABLED_MASK) != 0U;
		is_auto_fetch_disable = (gcplex_config &
			GCPLEX_CONFIG_VPR_AUTO_FETCH_DISABLE_MASK) != 0U;
		if (is_wpr_enabled && !is_auto_fetch_disable) {
			if (g->ops.fuse.fuse_opt_sec_debug_en(g) != 0U) {
				nvgpu_log(g, gpu_dbg_info,
						"gcplex_config = 0x%08x, "
						"secure mode: ACR debug",
						gcplex_config);
			} else {
				nvgpu_log(g, gpu_dbg_info,
						"gcplex_config = 0x%08x, "
						"secure mode: ACR non debug",
						gcplex_config);
			}

		} else {
			nvgpu_err(g, "gcplex_config = 0x%08x "
				"invalid wpr_enabled/vpr_auto_fetch_disable "
				"with priv_sec_en", gcplex_config);
			/* do not try to boot GPU */
			return -EINVAL;
		}
	} else {
		nvgpu_log_info(g, "secure mode: priv_sec_en = 0");
		nvgpu_set_enabled(g, NVGPU_SEC_PRIVSECURITY, false);
		nvgpu_set_enabled(g, NVGPU_SEC_SECUREGPCCS, false);
		nvgpu_log(g, gpu_dbg_info,
				"gcplex_config = 0x%08x, non secure mode",
				gcplex_config);
	}

	return 0;
}

static void check_and_update_fuse_settings(struct gk20a *g, u32 fuse,
	u32 falcon_feature, unsigned long *fuse_settings)
{
	nvgpu_readl(g, fuse) ?
		nvgpu_set_bit(falcon_feature, fuse_settings) :
		nvgpu_clear_bit(falcon_feature, fuse_settings);
}

int ga10b_fetch_falcon_fuse_settings(struct gk20a *g, u32 falcon_id,
		unsigned long *fuse_settings)
{
	int err = 0;

	switch (falcon_id) {
	case FALCON_ID_PMU:
	case FALCON_ID_PMU_NEXT_CORE:
		check_and_update_fuse_settings(g, fuse_pmu_fcd_r(),
			FCD, fuse_settings);

		check_and_update_fuse_settings(g, fuse_pmu_enen_r(),
			FENEN, fuse_settings);

		check_and_update_fuse_settings(g,
			fuse_pmu_nvriscv_bre_en_r(),
			NVRISCV_BRE_EN, fuse_settings);

		check_and_update_fuse_settings(g, fuse_pmu_nvriscv_devd_r(),
			NVRISCV_DEVD, fuse_settings);

		check_and_update_fuse_settings(g, fuse_pmu_nvriscv_pld_r(),
			NVRISCV_PLD, fuse_settings);

		check_and_update_fuse_settings(g, fuse_pmu_dcs_r(),
			DCS, fuse_settings);

		check_and_update_fuse_settings(g, fuse_pmu_nvriscv_sen_r(),
			NVRISCV_SEN, fuse_settings);

		check_and_update_fuse_settings(g, fuse_pmu_nvriscv_sa_r(),
			NVRISCV_SA, fuse_settings);

		check_and_update_fuse_settings(g, fuse_pmu_nvriscv_sh_r(),
			NVRISCV_SH, fuse_settings);

		check_and_update_fuse_settings(g, fuse_pmu_nvriscv_si_r(),
			NVRISCV_SI, fuse_settings);

		check_and_update_fuse_settings(g, fuse_secure_pmu_dbgd_r(),
			SECURE_DBGD, fuse_settings);

		/*
		 * Bit[0] for AES; Bit[1] for PKC. When this fuse is not blown,
		 * both AES and PKC are enabled
		 */
		nvgpu_readl(g, fuse_pkc_pmu_algo_dis_r()) & AES_ALGO ?
			nvgpu_set_bit(AES_ALGO_DIS, fuse_settings) :
			nvgpu_clear_bit(AES_ALGO_DIS, fuse_settings);

		nvgpu_readl(g, fuse_pkc_pmu_algo_dis_r()) & PKC_ALGO ?
			nvgpu_set_bit(PKC_ALGO_DIS, fuse_settings) :
			nvgpu_clear_bit(PKC_ALGO_DIS, fuse_settings);

		break;
	case FALCON_ID_GSPLITE:
		check_and_update_fuse_settings(g, fuse_gsp_fcd_r(),
			FCD, fuse_settings);

		check_and_update_fuse_settings(g, fuse_gsp_enen_r(),
			FENEN, fuse_settings);

		check_and_update_fuse_settings(g,
			fuse_gsp_nvriscv_bre_en_r(),
			NVRISCV_BRE_EN, fuse_settings);

		check_and_update_fuse_settings(g, fuse_gsp_nvriscv_devd_r(),
			NVRISCV_DEVD, fuse_settings);

		check_and_update_fuse_settings(g, fuse_gsp_nvriscv_pld_r(),
			NVRISCV_PLD, fuse_settings);

		check_and_update_fuse_settings(g, fuse_gsp_dcs_r(),
			DCS, fuse_settings);

		check_and_update_fuse_settings(g, fuse_gsp_nvriscv_sen_r(),
			NVRISCV_SEN, fuse_settings);

		check_and_update_fuse_settings(g, fuse_gsp_nvriscv_sa_r(),
			NVRISCV_SA, fuse_settings);

		check_and_update_fuse_settings(g, fuse_gsp_nvriscv_sh_r(),
			NVRISCV_SH, fuse_settings);

		check_and_update_fuse_settings(g, fuse_gsp_nvriscv_si_r(),
			NVRISCV_SI, fuse_settings);

		check_and_update_fuse_settings(g, fuse_secure_gsp_dbgd_r(),
			SECURE_DBGD, fuse_settings);

		/*
		 * Bit[0] for AES; Bit[1] for PKC. When this fuse is not blown,
		 * both AES and PKC are enabled
		 */
		nvgpu_readl(g, fuse_pkc_gsp_algo_dis_r()) & AES_ALGO ?
			nvgpu_set_bit(AES_ALGO_DIS, fuse_settings) :
			nvgpu_clear_bit(AES_ALGO_DIS, fuse_settings);

		nvgpu_readl(g, fuse_pkc_gsp_algo_dis_r()) & PKC_ALGO ?
			nvgpu_set_bit(PKC_ALGO_DIS, fuse_settings) :
			nvgpu_clear_bit(PKC_ALGO_DIS, fuse_settings);
		break;
	default:
		err = -EINVAL;
		nvgpu_err(g, "Invalid/Unsupported falcon ID %x", falcon_id);
		break;
	}

	return err;
}
