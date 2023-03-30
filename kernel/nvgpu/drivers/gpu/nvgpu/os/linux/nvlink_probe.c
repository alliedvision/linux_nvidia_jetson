/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/mutex.h>
#ifdef CONFIG_NVGPU_NVLINK
#include <nvlink/common/tegra-nvlink.h>
#endif

#include <nvgpu/gk20a.h>
#include <nvgpu/nvlink.h>
#include <nvgpu/enabled.h>
#include "module.h"
#include <nvgpu/nvlink_probe.h>
#include <nvgpu/nvlink_device_reginit.h>
#include <nvgpu/nvlink_link_mode_transitions.h>

#ifdef CONFIG_NVGPU_NVLINK
int nvgpu_nvlink_read_dt_props(struct gk20a *g)
{
	struct device_node *np;
	struct nvlink_device *ndev = g->nvlink.priv;
	u32 local_dev_id;
	u32 local_link_id;
	u32 remote_dev_id;
	u32 remote_link_id;
	bool is_master;
	int err = 0;

	/* Parse DT */
	np = nvgpu_get_node(g);
	if (!np) {
		err = -ENODEV;
		goto fail;
	}

	np = of_get_child_by_name(np, "nvidia,nvlink");
	if (!np) {
		err = -ENODEV;
		goto fail;
	}

	np = of_get_child_by_name(np, "endpoint");
	if (!np) {
		err = -ENODEV;
		goto fail;
	}

	/* Parse DT structure to detect endpoint topology */
	err = of_property_read_u32(np, "local_dev_id", &local_dev_id);
	if (err != 0) {
		goto fail;
	}

	err = of_property_read_u32(np, "local_link_id", &local_link_id);
	if (err != 0) {
		goto fail;
	}

	err = of_property_read_u32(np, "remote_dev_id", &remote_dev_id);
	if (err != 0) {
		goto fail;
	}

	err = of_property_read_u32(np, "remote_link_id", &remote_link_id);
	if (err != 0) {
		goto fail;
	}

	is_master = of_property_read_bool(np, "is_master");

	/* Check that we are in dGPU mode */
	if (local_dev_id != NVLINK_ENDPT_GV100) {
		nvgpu_err(g, "Local nvlink device is not dGPU");
		return -EINVAL;
	}

	ndev->is_master = is_master;
	ndev->device_id = local_dev_id;
	ndev->link.link_id = local_link_id;
	ndev->link.remote_dev_info.device_id = remote_dev_id;
	ndev->link.remote_dev_info.link_id = remote_link_id;

	return 0;

fail:
	nvgpu_info(g, "nvlink endpoint not found or invaling in DT");
	return err;
}

static int nvgpu_nvlink_ops_early_init(struct nvlink_device *ndev)
{
	struct gk20a *g = (struct gk20a *) ndev->priv;

	return nvgpu_nvlink_early_init(g);
}

static int nvgpu_nvlink_ops_link_early_init(struct nvlink_device *ndev)
{
	struct gk20a *g = (struct gk20a *) ndev->priv;

	return nvgpu_nvlink_link_early_init(g);
}

static int nvgpu_nvlink_ops_interface_init(struct nvlink_device *ndev)
{
	struct gk20a *g = (struct gk20a *) ndev->priv;

	return nvgpu_nvlink_interface_init(g);
}

static int nvgpu_nvlink_ops_interface_disable(struct nvlink_device *ndev)
{
	struct gk20a *g = (struct gk20a *) ndev->priv;

	return nvgpu_nvlink_interface_disable(g);
}

static int nvgpu_nvlink_ops_dev_shutdown(struct nvlink_device *ndev)
{
	struct gk20a *g = (struct gk20a *) ndev->priv;

	return nvgpu_nvlink_dev_shutdown(g);
}

static int nvgpu_nvlink_ops_reg_init(struct nvlink_device *ndev)
{
	struct gk20a *g = (struct gk20a *) ndev->priv;

	return nvgpu_nvlink_reg_init(g);
}

static u32 nvgpu_nvlink_ops_get_link_mode(struct nvlink_device *ndev)
{
	struct gk20a *g = (struct gk20a *) ndev->priv;
	enum nvgpu_nvlink_link_mode mode;

	mode = nvgpu_nvlink_get_link_mode(g);

	switch (mode) {
	case nvgpu_nvlink_link_off:
		return NVLINK_LINK_OFF;
	case nvgpu_nvlink_link_hs:
		return NVLINK_LINK_HS;
	case nvgpu_nvlink_link_safe:
		return NVLINK_LINK_SAFE;
	case nvgpu_nvlink_link_fault:
		return NVLINK_LINK_FAULT;
	case nvgpu_nvlink_link_rcvy_ac:
		return NVLINK_LINK_RCVY_AC;
	case nvgpu_nvlink_link_rcvy_sw:
		return NVLINK_LINK_RCVY_SW;
	case nvgpu_nvlink_link_rcvy_rx:
		return NVLINK_LINK_RCVY_RX;
	case nvgpu_nvlink_link_detect:
		return NVLINK_LINK_DETECT;
	case nvgpu_nvlink_link_reset:
		return NVLINK_LINK_RESET;
	case nvgpu_nvlink_link_enable_pm:
		return NVLINK_LINK_ENABLE_PM;
	case nvgpu_nvlink_link_disable_pm:
		return NVLINK_LINK_DISABLE_PM;
	case nvgpu_nvlink_link_disable_err_detect:
		return NVLINK_LINK_DISABLE_ERR_DETECT;
	case nvgpu_nvlink_link_lane_disable:
		return NVLINK_LINK_LANE_DISABLE;
	case nvgpu_nvlink_link_lane_shutdown:
		return NVLINK_LINK_LANE_SHUTDOWN;
	default:
		nvgpu_log(g, gpu_dbg_info | gpu_dbg_nvlink,
						"unsupported mode %u", mode);
	}

	return NVLINK_LINK_OFF;
}

static u32 nvgpu_nvlink_ops_get_link_state(struct nvlink_device *ndev)
{
	struct gk20a *g = (struct gk20a *) ndev->priv;

	return nvgpu_nvlink_get_link_state(g);
}

static int nvgpu_nvlink_ops_set_link_mode(struct nvlink_device *ndev, u32 mode)
{
	struct gk20a *g = (struct gk20a *) ndev->priv;
	enum nvgpu_nvlink_link_mode mode_sw;

	switch (mode) {
	case NVLINK_LINK_OFF:
		mode_sw = nvgpu_nvlink_link_off;
		break;
	case NVLINK_LINK_HS:
		mode_sw = nvgpu_nvlink_link_hs;
		break;
	case NVLINK_LINK_SAFE:
		mode_sw = nvgpu_nvlink_link_safe;
		break;
	case NVLINK_LINK_FAULT:
		mode_sw = nvgpu_nvlink_link_fault;
		break;
	case NVLINK_LINK_RCVY_AC:
		mode_sw = nvgpu_nvlink_link_rcvy_ac;
		break;
	case NVLINK_LINK_RCVY_SW:
		mode_sw = nvgpu_nvlink_link_rcvy_sw;
		break;
	case NVLINK_LINK_RCVY_RX:
		mode_sw = nvgpu_nvlink_link_rcvy_rx;
		break;
	case NVLINK_LINK_DETECT:
		mode_sw = nvgpu_nvlink_link_detect;
		break;
	case NVLINK_LINK_RESET:
		mode_sw = nvgpu_nvlink_link_reset;
		break;
	case NVLINK_LINK_ENABLE_PM:
		mode_sw = nvgpu_nvlink_link_enable_pm;
		break;
	case NVLINK_LINK_DISABLE_PM:
		mode_sw = nvgpu_nvlink_link_disable_pm;
		break;
	case NVLINK_LINK_DISABLE_ERR_DETECT:
		mode_sw = nvgpu_nvlink_link_disable_err_detect;
		break;
	case NVLINK_LINK_LANE_DISABLE:
		mode_sw = nvgpu_nvlink_link_lane_disable;
		break;
	case NVLINK_LINK_LANE_SHUTDOWN:
		mode_sw = nvgpu_nvlink_link_lane_shutdown;
		break;
	default:
		mode_sw = nvgpu_nvlink_link_off;
	}

	return nvgpu_nvlink_set_link_mode(g, mode_sw);
}

static void nvgpu_nvlink_ops_get_tx_sublink_state(struct nvlink_device *ndev,
					u32 *tx_sublink_state)
{
	struct gk20a *g = (struct gk20a *) ndev->priv;

	return nvgpu_nvlink_get_tx_sublink_state(g, tx_sublink_state);
}

static void nvgpu_nvlink_ops_get_rx_sublink_state(struct nvlink_device *ndev,
					u32 *rx_sublink_state)
{
	struct gk20a *g = (struct gk20a *) ndev->priv;

	return nvgpu_nvlink_get_rx_sublink_state(g, rx_sublink_state);
}

static u32 nvgpu_nvlink_ops_get_sublink_mode(struct nvlink_device *ndev,
					bool is_rx_sublink)
{
	struct gk20a *g = (struct gk20a *) ndev->priv;
	enum nvgpu_nvlink_sublink_mode mode;

	mode = nvgpu_nvlink_get_sublink_mode(g, is_rx_sublink);

	switch (mode) {
	case nvgpu_nvlink_sublink_tx_hs:
		return NVLINK_TX_HS;
	case nvgpu_nvlink_sublink_tx_off:
		return NVLINK_TX_OFF;
	case nvgpu_nvlink_sublink_tx_single_lane:
		return NVLINK_TX_SINGLE_LANE;
	case nvgpu_nvlink_sublink_tx_safe:
		return NVLINK_TX_SAFE;
	case nvgpu_nvlink_sublink_tx_enable_pm:
		return NVLINK_TX_ENABLE_PM;
	case nvgpu_nvlink_sublink_tx_disable_pm:
		return NVLINK_TX_DISABLE_PM;
	case nvgpu_nvlink_sublink_tx_common:
		return NVLINK_TX_COMMON;
	case nvgpu_nvlink_sublink_tx_common_disable:
		return NVLINK_TX_COMMON_DISABLE;
	case nvgpu_nvlink_sublink_tx_data_ready:
		return NVLINK_TX_DATA_READY;
	case nvgpu_nvlink_sublink_tx_prbs_en:
		return NVLINK_TX_PRBS_EN;
	case nvgpu_nvlink_sublink_rx_hs:
		return NVLINK_RX_HS;
	case nvgpu_nvlink_sublink_rx_enable_pm:
		return NVLINK_RX_ENABLE_PM;
	case nvgpu_nvlink_sublink_rx_disable_pm:
		return NVLINK_RX_DISABLE_PM;
	case nvgpu_nvlink_sublink_rx_single_lane:
		return NVLINK_RX_SINGLE_LANE;
	case nvgpu_nvlink_sublink_rx_safe:
		return NVLINK_RX_SAFE;
	case nvgpu_nvlink_sublink_rx_off:
		return NVLINK_RX_OFF;
	case nvgpu_nvlink_sublink_rx_rxcal:
		return NVLINK_RX_RXCAL;
	default:
		nvgpu_log(g, gpu_dbg_nvlink, "Unsupported mode: %u", mode);
		break;
	}

	if (is_rx_sublink)
		return NVLINK_RX_OFF;
	return NVLINK_TX_OFF;
}

static int nvgpu_nvlink_ops_set_sublink_mode(struct nvlink_device *ndev,
						bool is_rx_sublink, u32 mode)
{
	struct gk20a *g = (struct gk20a *) ndev->priv;
	enum nvgpu_nvlink_sublink_mode mode_sw;

	if (!is_rx_sublink) {
		switch (mode) {
		case NVLINK_TX_HS:
			mode_sw = nvgpu_nvlink_sublink_tx_hs;
			break;
		case NVLINK_TX_ENABLE_PM:
			mode_sw = nvgpu_nvlink_sublink_tx_enable_pm;
			break;
		case NVLINK_TX_DISABLE_PM:
			mode_sw = nvgpu_nvlink_sublink_tx_disable_pm;
			break;
		case NVLINK_TX_SINGLE_LANE:
			mode_sw = nvgpu_nvlink_sublink_tx_single_lane;
			break;
		case NVLINK_TX_SAFE:
			mode_sw = nvgpu_nvlink_sublink_tx_safe;
			break;
		case NVLINK_TX_OFF:
			mode_sw = nvgpu_nvlink_sublink_tx_off;
			break;
		case NVLINK_TX_COMMON:
			mode_sw = nvgpu_nvlink_sublink_tx_common;
			break;
		case NVLINK_TX_COMMON_DISABLE:
			mode_sw = nvgpu_nvlink_sublink_tx_common_disable;
			break;
		case NVLINK_TX_DATA_READY:
			mode_sw = nvgpu_nvlink_sublink_tx_data_ready;
			break;
		case NVLINK_TX_PRBS_EN:
			mode_sw = nvgpu_nvlink_sublink_tx_prbs_en;
			break;
		default:
			return -EINVAL;
		}
	} else {
		switch (mode) {
		case NVLINK_RX_HS:
			mode_sw = nvgpu_nvlink_sublink_rx_hs;
			break;
		case NVLINK_RX_ENABLE_PM:
			mode_sw = nvgpu_nvlink_sublink_rx_enable_pm;
			break;
		case NVLINK_RX_DISABLE_PM:
			mode_sw = nvgpu_nvlink_sublink_rx_disable_pm;
			break;
		case NVLINK_RX_SINGLE_LANE:
			mode_sw = nvgpu_nvlink_sublink_rx_single_lane;
			break;
		case NVLINK_RX_SAFE:
			mode_sw = nvgpu_nvlink_sublink_rx_safe;
			break;
		case NVLINK_RX_OFF:
			mode_sw = nvgpu_nvlink_sublink_rx_off;
			break;
		case NVLINK_RX_RXCAL:
			mode_sw = nvgpu_nvlink_sublink_rx_rxcal;
			break;
		default:
			return -EINVAL;
		}
	}

	return nvgpu_nvlink_set_sublink_mode(g, is_rx_sublink, mode_sw);
}

int nvgpu_nvlink_setup_ndev(struct gk20a *g)
{
	struct nvlink_device *ndev;

	/* Allocating structures */
	ndev = nvgpu_kzalloc(g, sizeof(struct nvlink_device));
	if (!ndev) {
		nvgpu_err(g, "OOM while allocating nvlink device struct");
		return -ENOMEM;
	}
	ndev->priv = (void *) g;
	g->nvlink.priv = (void *) ndev;

	return 0;
}

int nvgpu_nvlink_init_ops(struct gk20a *g)
{
	struct nvlink_device *ndev = (struct nvlink_device *) g->nvlink.priv;

	if (!ndev)
		return -EINVAL;

	/* Fill in device struct */
	ndev->dev_ops.dev_early_init = nvgpu_nvlink_ops_early_init;
	ndev->dev_ops.dev_interface_init = nvgpu_nvlink_ops_interface_init;
	ndev->dev_ops.dev_reg_init = nvgpu_nvlink_ops_reg_init;
	ndev->dev_ops.dev_interface_disable =
					nvgpu_nvlink_ops_interface_disable;
	ndev->dev_ops.dev_shutdown = nvgpu_nvlink_ops_dev_shutdown;

	/* Fill in the link struct */
	ndev->link.device_id = ndev->device_id;
	ndev->link.mode = NVLINK_LINK_OFF;
	ndev->link.is_sl_supported = false;
	ndev->link.link_ops.get_link_mode = nvgpu_nvlink_ops_get_link_mode;
	ndev->link.link_ops.set_link_mode = nvgpu_nvlink_ops_set_link_mode;
	ndev->link.link_ops.get_sublink_mode =
					nvgpu_nvlink_ops_get_sublink_mode;
	ndev->link.link_ops.set_sublink_mode =
					nvgpu_nvlink_ops_set_sublink_mode;
	ndev->link.link_ops.get_link_state = nvgpu_nvlink_ops_get_link_state;
	ndev->link.link_ops.get_tx_sublink_state =
					nvgpu_nvlink_ops_get_tx_sublink_state;
	ndev->link.link_ops.get_rx_sublink_state =
					nvgpu_nvlink_ops_get_rx_sublink_state;
	ndev->link.link_ops.link_early_init =
					nvgpu_nvlink_ops_link_early_init;

	return 0;
}

int nvgpu_nvlink_register_device(struct gk20a *g)
{
	struct nvlink_device *ndev = (struct nvlink_device *) g->nvlink.priv;

	if (!ndev)
		return -ENODEV;

	return nvlink_register_device(ndev);
}

int nvgpu_nvlink_unregister_device(struct gk20a *g)
{
	struct nvlink_device *ndev = (struct nvlink_device *) g->nvlink.priv;

	if (!ndev)
		return -ENODEV;

	return nvlink_unregister_device(ndev);
}

int nvgpu_nvlink_register_link(struct gk20a *g)
{
	struct nvlink_device *ndev = (struct nvlink_device *) g->nvlink.priv;

	if (!ndev)
		return -ENODEV;

	return nvlink_register_link(&ndev->link);
}

int nvgpu_nvlink_unregister_link(struct gk20a *g)
{
	struct nvlink_device *ndev = (struct nvlink_device *) g->nvlink.priv;

	if (!ndev)
		return -ENODEV;

	return nvlink_unregister_link(&ndev->link);
}

#endif /* CONFIG_NVGPU_NVLINK */

