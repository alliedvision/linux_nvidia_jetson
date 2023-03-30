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

#ifdef CONFIG_NVGPU_NVLINK

#include <nvgpu/nvlink.h>
#include <nvgpu/io.h>
#include <nvgpu/timers.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/nvlink_link_mode_transitions.h>

#include "link_mode_transitions_gv100.h"

#include <nvgpu/hw/gv100/hw_nvl_gv100.h>
#include <nvgpu/hw/gv100/hw_trim_gv100.h>

/*
 * Init UPHY
 */
static int gv100_nvlink_init_uphy(struct gk20a *g, unsigned long mask,
					bool sync)
{
	int err = 0;
	enum nvgpu_nvlink_minion_dlcmd init_pll_cmd;
	u32 link_id, master_pll, slave_pll;
	u32 master_state, slave_state;
	u32 link_enable;
	unsigned long bit;

	link_enable = g->ops.nvlink.get_link_reset_mask(g);

	if ((g->nvlink.speed) == nvgpu_nvlink_speed_20G) {
		init_pll_cmd = NVGPU_NVLINK_MINION_DLCMD_INITPLL_1;
	} else {
		nvgpu_err(g, "Unsupported UPHY speed");
		return -EINVAL;
	}

	for_each_set_bit(bit, &mask, NVLINK_MAX_LINKS_SW) {
		link_id = (u32)bit;
		master_pll = g->nvlink.links[link_id].pll_master_link_id;
		slave_pll = g->nvlink.links[link_id].pll_slave_link_id;

		master_state = nvl_link_state_state_init_v();
		slave_state = nvl_link_state_state_init_v();

		if ((BIT32(master_pll) & link_enable) != 0U) {
			master_state = nvl_link_state_state_v(g->ops.nvlink.
					link_mode_transitions.get_link_state(
								g, master_pll));
		}

		if ((BIT32(slave_pll) & link_enable) != 0U) {
			slave_state = nvl_link_state_state_v(g->ops.nvlink.
					link_mode_transitions.get_link_state(
								g, slave_pll));
		}

		if ((slave_state != nvl_link_state_state_init_v()) ||
		   (master_state != nvl_link_state_state_init_v())) {
			nvgpu_err(g, "INIT PLL can only be executed when both "
				"master and slave links are in init state");
			return -EINVAL;
		}

		/* Check if INIT PLL is done on link */
		if ((BIT(master_pll) & g->nvlink.init_pll_done) == 0U) {
			err = g->ops.nvlink.minion.send_dlcmd(g, master_pll,
						init_pll_cmd, sync);
			if (err != 0) {
				nvgpu_err(g, " Error sending INITPLL to minion");
				return err;
			}

			g->nvlink.init_pll_done |= BIT32(master_pll);
		}
	}

	err = g->ops.nvlink.link_mode_transitions.setup_pll(g, mask);
	if (err != 0) {
		nvgpu_err(g, "Error setting up PLL");
		return err;
	}

	/* INITPHY commands */
	for_each_set_bit(bit, &mask, NVLINK_MAX_LINKS_SW) {
		link_id = (u32)bit;
		err = g->ops.nvlink.minion.send_dlcmd(g, link_id,
				NVGPU_NVLINK_MINION_DLCMD_INITPHY, sync);
		if (err != 0) {
			nvgpu_err(g, "Error on INITPHY minion DL command %u",
					link_id);
			return err;
		}
	}

	return 0;
}

/*
 * Set Data ready
 */
int gv100_nvlink_data_ready_en(struct gk20a *g,
					unsigned long link_mask, bool sync)
{
	int ret = 0;
	u32 link_id;
	unsigned long bit;

	for_each_set_bit(bit, &link_mask, NVLINK_MAX_LINKS_SW) {
		link_id = (u32)bit;
		ret = g->ops.nvlink.minion.send_dlcmd(g, link_id,
				NVGPU_NVLINK_MINION_DLCMD_INITLANEENABLE, sync);
		if (ret != 0) {
			nvgpu_err(g, "Failed initlaneenable on link %u",
								link_id);
			return ret;
		}
	}

	for_each_set_bit(bit, &link_mask, NVLINK_MAX_LINKS_SW) {
		link_id = (u32)bit;
		ret = g->ops.nvlink.minion.send_dlcmd(g, link_id,
				NVGPU_NVLINK_MINION_DLCMD_INITDLPL, sync);
		if (ret != 0) {
			nvgpu_err(g, "Failed initdlpl on link %u", link_id);
			return ret;
		}
	}
	return ret;
}

/*
 * Request that minion disable the lane
 */
static int gv100_nvlink_lane_disable(struct gk20a *g, u32 link_id,
								bool sync)
{
	int err = 0;

	err = g->ops.nvlink.minion.send_dlcmd(g, link_id,
				NVGPU_NVLINK_MINION_DLCMD_LANEDISABLE, sync);

	if (err != 0) {
		nvgpu_err(g, " failed to disable lane on %d", link_id);
	}

	return err;
}

/*
 * Request that minion shutdown the lane
 */
static int gv100_nvlink_lane_shutdown(struct gk20a *g, u32 link_id,
								bool sync)
{
	int err = 0;

	err = g->ops.nvlink.minion.send_dlcmd(g, link_id,
			NVGPU_NVLINK_MINION_DLCMD_LANESHUTDOWN, sync);

	if (err != 0) {
		nvgpu_err(g, " failed to shutdown lane on %d", link_id);
	}

	return err;
}

#define TRIM_SYS_NVLINK_CTRL(i) (trim_sys_nvlink0_ctrl_r() + 16U*(i))
#define TRIM_SYS_NVLINK_STATUS(i) (trim_sys_nvlink0_status_r() + 16U*(i))

int gv100_nvlink_setup_pll(struct gk20a *g, unsigned long link_mask)
{
	int err = 0;
	u32 reg;
	u32 link_id;
	u32 links_off;
	struct nvgpu_timeout timeout;
	u32 pad_ctrl = 0U;
	u32 swap_ctrl = 0U;
	u8 pll_id;
	unsigned long bit;

	reg = gk20a_readl(g, trim_sys_nvlink_uphy_cfg_r());
	reg = set_field(reg, trim_sys_nvlink_uphy_cfg_phy2clks_use_lockdet_m(),
			trim_sys_nvlink_uphy_cfg_phy2clks_use_lockdet_f(1));
	gk20a_writel(g, trim_sys_nvlink_uphy_cfg_r(), reg);

	if (g->ops.top.get_nvhsclk_ctrl_e_clk_nvl != NULL) {
		pad_ctrl = g->ops.top.get_nvhsclk_ctrl_e_clk_nvl(g);
	}
	if (g->ops.top.get_nvhsclk_ctrl_swap_clk_nvl != NULL) {
		swap_ctrl = g->ops.top.get_nvhsclk_ctrl_swap_clk_nvl(g);
	}

	for_each_set_bit(bit, &link_mask, NVLINK_MAX_LINKS_SW) {
		link_id = (u32)bit;
		/* There are 3 PLLs for 6 links. We have 3 bits for each PLL.
		 * The PLL bit corresponding to a link is /2 of its master link.
                 */
		pll_id = g->nvlink.links[link_id].pll_master_link_id >> 1;
		pad_ctrl  |= BIT32(pll_id);
		swap_ctrl |= BIT32(pll_id);
	}

	if (g->ops.top.set_nvhsclk_ctrl_e_clk_nvl != NULL) {
		g->ops.top.set_nvhsclk_ctrl_e_clk_nvl(g, pad_ctrl);
	}
	if (g->ops.top.set_nvhsclk_ctrl_swap_clk_nvl != NULL) {
		g->ops.top.set_nvhsclk_ctrl_swap_clk_nvl(g, swap_ctrl);
	}

	for_each_set_bit(bit, &link_mask, NVLINK_MAX_LINKS_SW) {
		link_id = (u32)bit;
		reg = gk20a_readl(g, TRIM_SYS_NVLINK_CTRL(link_id));
		reg = set_field(reg,
			trim_sys_nvlink0_ctrl_unit2clks_pll_turn_off_m(),
			trim_sys_nvlink0_ctrl_unit2clks_pll_turn_off_f(0));
		gk20a_writel(g, TRIM_SYS_NVLINK_CTRL(link_id), reg);
	}

	/* Poll for links to go up */
	links_off = (u32) link_mask;

	nvgpu_timeout_init_cpu_timer(g, &timeout, NVLINK_PLL_ON_TIMEOUT_MS);

	do {
		for_each_set_bit(bit, &link_mask, NVLINK_MAX_LINKS_SW) {
			link_id = (u32)bit;
			reg = gk20a_readl(g, TRIM_SYS_NVLINK_STATUS(link_id));
			if (trim_sys_nvlink0_status_pll_off_v(reg) == 0U) {
				links_off &= ~BIT32(link_id);
			}
		}
		nvgpu_udelay(5);

	} while ((nvgpu_timeout_expired_msg(&timeout, "timeout on pll on") == 0)
						&& (links_off != 0U));

	if (nvgpu_timeout_peek_expired(&timeout)) {
		return -ETIMEDOUT;
	}

	return err;
}

static int gv100_nvlink_prbs_gen_en(struct gk20a *g, unsigned long mask)
{
	u32 reg;
	u32 link_id;
	unsigned long bit;

	for_each_set_bit(bit, &mask, NVLINK_MAX_LINKS_SW) {
		link_id = (u32)bit;
		/* Write is required as part of HW sequence */
		DLPL_REG_WR32(g, link_id, nvl_sl1_rxslsm_timeout_2_r(), 0);

		reg = DLPL_REG_RD32(g, link_id, nvl_txiobist_config_r());
		reg = set_field(reg, nvl_txiobist_config_dpg_prbsseedld_m(),
			nvl_txiobist_config_dpg_prbsseedld_f(0x1));
		DLPL_REG_WR32(g, link_id, nvl_txiobist_config_r(), reg);

		reg = DLPL_REG_RD32(g, link_id, nvl_txiobist_config_r());
		reg = set_field(reg, nvl_txiobist_config_dpg_prbsseedld_m(),
			nvl_txiobist_config_dpg_prbsseedld_f(0x0));
		DLPL_REG_WR32(g, link_id, nvl_txiobist_config_r(), reg);
	}

	return 0;
}

static int gv100_nvlink_rxcal_en(struct gk20a *g, unsigned long mask)
{
	u32 link_id;
	struct nvgpu_timeout timeout;
	u32 reg;
	unsigned long bit;
	int ret = 0;

	for_each_set_bit(bit, &mask, NVLINK_MAX_LINKS_SW) {
		link_id = (u32)bit;
		/* Timeout from HW specs */
		nvgpu_timeout_init_cpu_timer(g, &timeout,
					     8*NVLINK_SUBLINK_TIMEOUT_MS);
		reg = DLPL_REG_RD32(g, link_id, nvl_br0_cfg_cal_r());
		reg = set_field(reg, nvl_br0_cfg_cal_rxcal_m(),
			nvl_br0_cfg_cal_rxcal_on_f());
		DLPL_REG_WR32(g, link_id, nvl_br0_cfg_cal_r(), reg);

		do {
			reg = DLPL_REG_RD32(g, link_id,
						nvl_br0_cfg_status_cal_r());

			if (nvl_br0_cfg_status_cal_rxcal_done_v(reg) == 1U) {
				break;
			}
			nvgpu_udelay(5);
		} while (nvgpu_timeout_expired_msg(&timeout,
						"timeout on rxcal") == 0);

		if (nvgpu_timeout_peek_expired(&timeout)) {
			return -ETIMEDOUT;
		}
	}

	return ret;
}

/*
 * Get link state
 */
u32 gv100_nvlink_get_link_state(struct gk20a *g, u32 link_id)
{
	return DLPL_REG_RD32(g, link_id, nvl_link_state_r()) &
			nvl_link_state_state_m();
}

/* Get link mode */
enum nvgpu_nvlink_link_mode gv100_nvlink_get_link_mode(struct gk20a *g,
								u32 link_id)
{
	u32 state;
	if ((BIT(link_id) & g->nvlink.discovered_links) == 0U) {
		return nvgpu_nvlink_link__last;
	}

	state = nvl_link_state_state_v(g->ops.nvlink.link_mode_transitions.
						get_link_state(g, link_id));

	if (state == nvl_link_state_state_init_v()) {
		return nvgpu_nvlink_link_off;
	}
	if (state == nvl_link_state_state_hwcfg_v()) {
		return nvgpu_nvlink_link_detect;
	}
	if (state == nvl_link_state_state_swcfg_v()) {
		return nvgpu_nvlink_link_safe;
	}
	if (state == nvl_link_state_state_active_v()) {
		return nvgpu_nvlink_link_hs;
	}
	if (state == nvl_link_state_state_fault_v()) {
		return nvgpu_nvlink_link_fault;
	}
	if (state == nvl_link_state_state_rcvy_ac_v()) {
		return nvgpu_nvlink_link_rcvy_ac;
	}
	if (state == nvl_link_state_state_rcvy_sw_v()) {
		return nvgpu_nvlink_link_rcvy_sw;
	}
	if (state == nvl_link_state_state_rcvy_rx_v()) {
		return nvgpu_nvlink_link_rcvy_rx;
	}

	return nvgpu_nvlink_link_off;
}

/* Set Link mode */
int gv100_nvlink_set_link_mode(struct gk20a *g, u32 link_id,
					enum nvgpu_nvlink_link_mode mode)
{
	u32 state;
	u32 reg;
	int err = 0;

	nvgpu_log(g, gpu_dbg_nvlink, "link :%d, mode:%u", link_id, mode);

	if ((BIT(link_id) & g->nvlink.enabled_links) == 0U) {
		return -EINVAL;
	}

	state = nvl_link_state_state_v(g->ops.nvlink.link_mode_transitions.
						get_link_state(g, link_id));

	switch (mode) {
	case nvgpu_nvlink_link_safe:
		if (state == nvl_link_state_state_swcfg_v()) {
			nvgpu_warn(g, "link is already in safe mode");
			break;
		} else if (state == nvl_link_state_state_hwcfg_v()) {
			nvgpu_warn(g, "link is transitioning to safe mode");
			break;
		} else if (state == nvl_link_state_state_init_v()) {
			/* Off to Safe transition */
			reg = DLPL_REG_RD32(g, link_id, nvl_link_change_r());
			reg = set_field(reg, nvl_link_change_newstate_m(),
				nvl_link_change_newstate_hwcfg_f());
			reg = set_field(reg, nvl_link_change_oldstate_mask_m(),
				nvl_link_change_oldstate_mask_dontcare_f());
			reg = set_field(reg, nvl_link_change_action_m(),
				nvl_link_change_action_ltssm_change_f());
			DLPL_REG_WR32(g, link_id, nvl_link_change_r(), reg);
		} else if (state == nvl_link_state_state_active_v()) {
			/* TODO:
			 * Disable PM first since we are moving out active
			 * state
			 */
			reg = DLPL_REG_RD32(g, link_id, nvl_link_change_r());
			reg = set_field(reg, nvl_link_change_newstate_m(),
				nvl_link_change_newstate_swcfg_f());
			reg = set_field(reg, nvl_link_change_oldstate_mask_m(),
				nvl_link_change_oldstate_mask_dontcare_f());
			reg = set_field(reg, nvl_link_change_action_m(),
				nvl_link_change_action_ltssm_change_f());
			DLPL_REG_WR32(g, link_id, nvl_link_change_r(), reg);
		} else {
			nvgpu_err(g,
			"Link state transition to Safe mode not permitted");
			return -EPERM;
		}
		break;

	case nvgpu_nvlink_link_hs:
		if (state == nvl_link_state_state_active_v()) {
			nvgpu_err(g, "link is already in active mode");
			break;
		}
		if (state == nvl_link_state_state_init_v()) {
			nvgpu_err(g, "link cannot be taken from init state");
			return -EPERM;
		}

		reg = DLPL_REG_RD32(g, link_id, nvl_link_change_r());
		reg = set_field(reg, nvl_link_change_newstate_m(),
				nvl_link_change_newstate_active_f());
		reg = set_field(reg, nvl_link_change_oldstate_mask_m(),
			nvl_link_change_oldstate_mask_dontcare_f());
		reg = set_field(reg, nvl_link_change_action_m(),
			nvl_link_change_action_ltssm_change_f());
		DLPL_REG_WR32(g, link_id, nvl_link_change_r(), reg);
		break;

	case nvgpu_nvlink_link_off:
		if (state == nvl_link_state_state_active_v()) {
			nvgpu_err(g, "link cannot be taken from active to init");
			return -EPERM;
		}
		if (state == nvl_link_state_state_init_v()) {
			nvgpu_err(g, "link already in init state");
		}

		/* GV100 UPHY is handled by MINION */
		break;
		/* 1/8 th mode not supported */
	case nvgpu_nvlink_link_enable_pm:
	case nvgpu_nvlink_link_disable_pm:
		err = -EPERM;
		break;
	case nvgpu_nvlink_link_disable_err_detect:
		/* Disable Link interrupts */
		g->ops.nvlink.intr.enable_link_err_intr(g, link_id, false);
		break;
	case nvgpu_nvlink_link_lane_disable:
		err = gv100_nvlink_lane_disable(g, link_id, true);
		break;
	case nvgpu_nvlink_link_lane_shutdown:
		err = gv100_nvlink_lane_shutdown(g, link_id, true);
		break;
	default:
		nvgpu_err(g, "Unhandled mode %x", mode);
		break;
	}

	return err;
}

static int gv100_nvlink_link_sublink_check_change(struct gk20a *g, u32 link_id)
{
	struct nvgpu_timeout timeout;
	u32 reg;

	nvgpu_timeout_init_cpu_timer(g, &timeout, NVLINK_SUBLINK_TIMEOUT_MS);

	/* Poll for sublink status */
	do {
		reg = DLPL_REG_RD32(g, link_id, nvl_sublink_change_r());

		if (nvl_sublink_change_status_v(reg) ==
				nvl_sublink_change_status_done_v()) {
			break;
		}
		if (nvl_sublink_change_status_v(reg) ==
				nvl_sublink_change_status_fault_v()) {
			nvgpu_err(g, "Fault detected in sublink change");
			return -EFAULT;
		}
		nvgpu_udelay(5);
	} while (nvgpu_timeout_expired_msg(&timeout,
					"timeout on sublink rdy") == 0);

	if (nvgpu_timeout_peek_expired(&timeout)) {
		return -ETIMEDOUT;
	}
	return 0;
}

int gv100_nvlink_link_set_sublink_mode(struct gk20a *g, u32 link_id,
					bool is_rx_sublink,
					enum nvgpu_nvlink_sublink_mode mode)
{
	int err = 0;
	u32 rx_sublink_state = U32_MAX;
	u32 tx_sublink_state = U32_MAX;
	u32 reg;

	if ((BIT(link_id) & g->nvlink.discovered_links) == 0U) {
		return -EINVAL;
	}

	err = gv100_nvlink_link_sublink_check_change(g, link_id);
	if (err != 0) {
		return err;
	}

	if (is_rx_sublink) {
		rx_sublink_state = g->ops.nvlink.link_mode_transitions.
					get_rx_sublink_state(g, link_id);
	} else {
		tx_sublink_state = g->ops.nvlink.link_mode_transitions.
					get_tx_sublink_state(g, link_id);
	}

	switch (mode) {
	case nvgpu_nvlink_sublink_tx_hs:
		if (tx_sublink_state ==
			nvl_sl0_slsm_status_tx_primary_state_hs_v()) {
			nvgpu_err(g, " TX already in HS");
			break;
		} else if (tx_sublink_state ==
				nvl_sl0_slsm_status_tx_primary_state_off_v()) {
			nvgpu_err(g, "TX cannot be do from OFF to HS");
			return -EPERM;
		} else {
			reg = DLPL_REG_RD32(g, link_id, nvl_sublink_change_r());
			reg = set_field(reg, nvl_sublink_change_newstate_m(),
				nvl_sublink_change_newstate_hs_f());
			reg = set_field(reg, nvl_sublink_change_sublink_m(),
					nvl_sublink_change_sublink_tx_f());
			reg = set_field(reg, nvl_sublink_change_action_m(),
				nvl_sublink_change_action_slsm_change_f());
			DLPL_REG_WR32(g, link_id, nvl_sublink_change_r(), reg);

			err = gv100_nvlink_link_sublink_check_change(g, link_id);
			if (err != 0) {
				nvgpu_err(g, "Error in TX to HS");
				return err;
			}
		}
		break;
	case nvgpu_nvlink_sublink_tx_common:
		err = gv100_nvlink_init_uphy(g, BIT64(link_id), true);
		break;
	case nvgpu_nvlink_sublink_tx_common_disable:
		/* NOP */
		break;
	case nvgpu_nvlink_sublink_tx_data_ready:
		err = g->ops.nvlink.link_mode_transitions.data_ready_en(g,
							BIT64(link_id), true);
		break;
	case nvgpu_nvlink_sublink_tx_prbs_en:
		err = gv100_nvlink_prbs_gen_en(g, BIT64(link_id));
		break;
	case nvgpu_nvlink_sublink_tx_safe:
		if (tx_sublink_state ==
				nvl_sl0_slsm_status_tx_primary_state_safe_v()) {
			nvgpu_err(g, "TX already SAFE: %d", link_id);
			break;
		}

		reg = DLPL_REG_RD32(g, link_id, nvl_sublink_change_r());
		reg = set_field(reg, nvl_sublink_change_newstate_m(),
			nvl_sublink_change_newstate_safe_f());
		reg = set_field(reg, nvl_sublink_change_sublink_m(),
			nvl_sublink_change_sublink_tx_f());
		reg = set_field(reg, nvl_sublink_change_action_m(),
			nvl_sublink_change_action_slsm_change_f());
		DLPL_REG_WR32(g, link_id, nvl_sublink_change_r(), reg);

		err = gv100_nvlink_link_sublink_check_change(g, link_id);
		if (err != 0) {
			nvgpu_err(g, "Error in TX to SAFE");
			return err;
		}
		break;
	case nvgpu_nvlink_sublink_tx_off:
		if (tx_sublink_state ==
				nvl_sl0_slsm_status_tx_primary_state_off_v()) {
			nvgpu_err(g, "TX already OFF: %d", link_id);
			break;
		} else if (tx_sublink_state ==
			nvl_sl0_slsm_status_tx_primary_state_hs_v()) {
			nvgpu_err(g, " TX cannot go off from HS %d", link_id);
			return -EPERM;
		} else {
			reg = DLPL_REG_RD32(g, link_id, nvl_sublink_change_r());
			reg = set_field(reg, nvl_sublink_change_newstate_m(),
				nvl_sublink_change_newstate_off_f());
			reg = set_field(reg, nvl_sublink_change_sublink_m(),
				nvl_sublink_change_sublink_tx_f());
			reg = set_field(reg, nvl_sublink_change_action_m(),
				nvl_sublink_change_action_slsm_change_f());
			DLPL_REG_WR32(g, link_id, nvl_sublink_change_r(), reg);

			err = gv100_nvlink_link_sublink_check_change(g, link_id);
			if (err != 0) {
				nvgpu_err(g, "Error in TX to OFF");
				return err;
			}
		}
		break;

	/* RX modes */
	case nvgpu_nvlink_sublink_rx_hs:
	case nvgpu_nvlink_sublink_rx_safe:
		break;
	case nvgpu_nvlink_sublink_rx_off:
		if (rx_sublink_state ==
				nvl_sl1_slsm_status_rx_primary_state_off_v()) {
			nvgpu_err(g, "RX already OFF: %d", link_id);
			break;
		} else if (rx_sublink_state ==
			nvl_sl1_slsm_status_rx_primary_state_hs_v()) {
			nvgpu_err(g, " RX cannot go off from HS %d", link_id);
			return -EPERM;
		} else {
			reg = DLPL_REG_RD32(g, link_id, nvl_sublink_change_r());
			reg = set_field(reg, nvl_sublink_change_newstate_m(),
				nvl_sublink_change_newstate_off_f());
			reg = set_field(reg, nvl_sublink_change_sublink_m(),
				nvl_sublink_change_sublink_rx_f());
			reg = set_field(reg, nvl_sublink_change_action_m(),
				nvl_sublink_change_action_slsm_change_f());
			DLPL_REG_WR32(g, link_id, nvl_sublink_change_r(), reg);

			err = gv100_nvlink_link_sublink_check_change(g, link_id);
			if (err != 0) {
				nvgpu_err(g, "Error in RX to OFF");
				return err;
			}
		}
		break;
	case nvgpu_nvlink_sublink_rx_rxcal:
		err = gv100_nvlink_rxcal_en(g, BIT64(link_id));
		break;

	default:
		if ((is_rx_sublink) && ((mode < nvgpu_nvlink_sublink_rx_hs) ||
				(mode >= nvgpu_nvlink_sublink_rx__last))) {
			nvgpu_err(g, "Unsupported RX mode %u", mode);
			return -EINVAL;
		}
		if (mode >= nvgpu_nvlink_sublink_tx__last) {
			nvgpu_err(g, "Unsupported TX mode %u", mode);
			return -EINVAL;
		}
		nvgpu_err(g, "MODE %u", mode);
		err = -EPERM;
		break;
	}

	if (err != 0) {
		nvgpu_err(g, " failed on set_sublink_mode");
	}
	return err;
}

enum nvgpu_nvlink_sublink_mode gv100_nvlink_link_get_sublink_mode(
						struct gk20a *g, u32 link_id,
						bool is_rx_sublink)
{
	u32 state;

	if ((BIT(link_id) & g->nvlink.discovered_links) == 0U) {
		if (!is_rx_sublink) {
			return nvgpu_nvlink_sublink_tx__last;
		}
		return nvgpu_nvlink_sublink_rx__last;
	}

	if (!is_rx_sublink) {
		state = g->ops.nvlink.link_mode_transitions.
					get_tx_sublink_state(g, link_id);
		if (state == nvl_sl0_slsm_status_tx_primary_state_hs_v()) {
			return nvgpu_nvlink_sublink_tx_hs;
		}
		if (state == nvl_sl0_slsm_status_tx_primary_state_eighth_v()) {
			return nvgpu_nvlink_sublink_tx_single_lane;
		}
		if (state == nvl_sl0_slsm_status_tx_primary_state_safe_v()) {
			return nvgpu_nvlink_sublink_tx_safe;
		}
		if (state == nvl_sl0_slsm_status_tx_primary_state_off_v()) {
			return nvgpu_nvlink_sublink_tx_off;
		}
		return nvgpu_nvlink_sublink_tx__last;
	}

	state = g->ops.nvlink.link_mode_transitions.
					get_rx_sublink_state(g, link_id);
	if (state == nvl_sl1_slsm_status_rx_primary_state_hs_v()) {
		return nvgpu_nvlink_sublink_rx_hs;
	}
	if (state == nvl_sl1_slsm_status_rx_primary_state_eighth_v()) {
		return nvgpu_nvlink_sublink_rx_single_lane;
	}
	if (state == nvl_sl1_slsm_status_rx_primary_state_safe_v()) {
		return nvgpu_nvlink_sublink_rx_safe;
	}
	if (state == nvl_sl1_slsm_status_rx_primary_state_off_v()) {
		return nvgpu_nvlink_sublink_rx_off;
	}
	return nvgpu_nvlink_sublink_rx__last;
}

/*
 * Get TX sublink state
 */
u32 gv100_nvlink_link_get_tx_sublink_state(struct gk20a *g, u32 link_id)
{
	u32 reg = DLPL_REG_RD32(g, link_id, nvl_sl0_slsm_status_tx_r());

	return nvl_sl0_slsm_status_tx_primary_state_v(reg);
}

/*
 * Get RX sublink state
 */
u32 gv100_nvlink_link_get_rx_sublink_state(struct gk20a *g, u32 link_id)
{
	u32 reg = DLPL_REG_RD32(g, link_id, nvl_sl1_slsm_status_rx_r());

	return nvl_sl1_slsm_status_rx_primary_state_v(reg);
}

#endif /* CONFIG_NVGPU_NVLINK */
