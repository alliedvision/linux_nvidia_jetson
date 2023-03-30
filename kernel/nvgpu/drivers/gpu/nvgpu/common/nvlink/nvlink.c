/*
 * Copyright (c) 2018-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/gk20a.h>
#include <nvgpu/nvlink.h>
#include <nvgpu/nvlink_probe.h>
#include <nvgpu/enabled.h>
#include <nvgpu/device.h>
#include <nvgpu/nvlink_bios.h>
#include <nvgpu/device.h>
#include <nvgpu/cic_mon.h>
#include <nvgpu/errata.h>

#ifdef CONFIG_NVGPU_NVLINK

static int nvgpu_nvlink_enable_links_pre_top(struct gk20a *g,
					unsigned long links)
{
	u32 link_id;
	int err;
	unsigned long bit;

	nvgpu_log(g, gpu_dbg_nvlink, " enabling 0x%lx links", links);
	for_each_set_bit(bit, &links, NVLINK_MAX_LINKS_SW) {
		link_id = (u32)bit;

		/* Take links out of reset */
		g->ops.nvlink.clear_link_reset(g, link_id);

		/* Before  doing any link initialization, run RXDET to check
		 * if link is connected on  other end.
		 */
		if (g->ops.nvlink.rxdet != NULL) {
			err = g->ops.nvlink.rxdet(g, link_id);
			if (err != 0) {
				return err;
			}
		}

		/* Enable Link DLPL for AN0 */
		g->ops.nvlink.enable_link_an0(g, link_id);

		/* This should be done by the NVLINK API */
		err = g->ops.nvlink.link_mode_transitions.set_sublink_mode(g,
				link_id, false, nvgpu_nvlink_sublink_tx_common);
		if (err != 0) {
			nvgpu_err(g, "Failed to init phy of link: %u", link_id);
			return err;
		}

		err = g->ops.nvlink.link_mode_transitions.set_sublink_mode(g,
			link_id, true, nvgpu_nvlink_sublink_rx_rxcal);
		if (err != 0) {
			nvgpu_err(g, "Failed to RXcal on link: %u", link_id);
			return err;
		}

		err = g->ops.nvlink.link_mode_transitions.set_sublink_mode(g,
			link_id, false, nvgpu_nvlink_sublink_tx_data_ready);
		if (err != 0) {
			nvgpu_err(g, "Failed to set data ready link:%u",
				link_id);
			return err;
		}

		g->nvlink.enabled_links |= BIT32(link_id);
	}

	nvgpu_log(g, gpu_dbg_nvlink, "enabled_links=0x%08x",
		g->nvlink.enabled_links);

	if (g->nvlink.enabled_links != 0U) {
		return 0;
	}

	nvgpu_err(g, "No links were enabled");
	return -EINVAL;
}

static int nvgpu_nvlink_enable_links_post_top(struct gk20a *g,
					unsigned long links)
{
	u32 link_id;
	unsigned long bit;
	unsigned long enabled_links = (links & g->nvlink.enabled_links) &
			~g->nvlink.initialized_links;

	for_each_set_bit(bit, &enabled_links, NVLINK_MAX_LINKS_SW) {
		link_id = (u32)bit;
		if (nvgpu_is_errata_present(g, NVGPU_ERRATA_1888034)) {
			g->ops.nvlink.set_sw_errata(g, link_id);
		}
		g->ops.nvlink.intr.init_link_err_intr(g, link_id);
		g->ops.nvlink.intr.enable_link_err_intr(g, link_id, true);

		g->nvlink.initialized_links |= BIT32(link_id);
	};

	return 0;
}

/*
 * Main Nvlink init function. Calls into the Nvlink core API
 */
int nvgpu_nvlink_init(struct gk20a *g)
{
	int err = 0;

	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_NVLINK)) {
		return -ENODEV;
	}

	err = nvgpu_nvlink_enumerate(g);
	if (err != 0) {
		nvgpu_err(g, "failed to enumerate nvlink");
		goto fail;
	}

	/* Set HSHUB and SG_PHY */
	nvgpu_set_enabled(g, NVGPU_MM_USE_PHYSICAL_SG, true);

	err = g->ops.fb.enable_nvlink(g);
	if (err != 0) {
		nvgpu_err(g, "failed switch to nvlink sysmem");
		goto fail;
	}

	return err;

fail:
	nvgpu_set_enabled(g, NVGPU_MM_USE_PHYSICAL_SG, false);
	nvgpu_set_enabled(g, NVGPU_SUPPORT_NVLINK, false);
	return err;
}


/*
 * Query IOCTRL for device discovery
 */
static int nvgpu_nvlink_discover_ioctrl(struct gk20a *g)
{
	u32 i;
	u32 ioctrl_num_entries = 0U;
	struct nvgpu_nvlink_ioctrl_list *ioctrl_table;

	ioctrl_num_entries = nvgpu_device_count(g, NVGPU_DEVTYPE_IOCTRL);
	nvgpu_log_info(g, "ioctrl_num_entries: %d", ioctrl_num_entries);

	if (ioctrl_num_entries == 0U) {
		nvgpu_err(g, "No NVLINK IOCTRL entry found in dev_info table");
		return -EINVAL;
	}

	ioctrl_table = nvgpu_kzalloc(g, ioctrl_num_entries *
				sizeof(struct nvgpu_nvlink_ioctrl_list));
	if (ioctrl_table == NULL) {
		nvgpu_err(g, "Failed to allocate memory for nvlink io table");
		return -ENOMEM;
	}

	for (i = 0U; i < ioctrl_num_entries; i++) {
		const struct nvgpu_device *dev;

		dev = nvgpu_device_get(g, NVGPU_DEVTYPE_IOCTRL, i);
		if (dev == NULL) {
			nvgpu_err(g, "Failed to parse dev_info table IOCTRL dev (%d)",
				  NVGPU_DEVTYPE_IOCTRL);
			nvgpu_kfree(g, ioctrl_table);
			return -EINVAL;
		}

		ioctrl_table[i].valid = true;
		ioctrl_table[i].intr_enum = dev->intr_id;
		ioctrl_table[i].reset_enum = dev->reset_id;
		ioctrl_table[i].pri_base_addr = dev->pri_base;
		nvgpu_log(g, gpu_dbg_nvlink,
			"Dev %d: Pri_Base = 0x%0x Intr = %d Reset = %d",
			i, ioctrl_table[i].pri_base_addr,
			ioctrl_table[i].intr_enum,
			ioctrl_table[i].reset_enum);
	}
	g->nvlink.ioctrl_table = ioctrl_table;
	g->nvlink.io_num_entries = ioctrl_num_entries;

	return 0;
}


/*
 * Performs nvlink device level initialization by discovering the topology
 * taking device out of reset, boot minion, set clocks up and common interrupts
 */
int nvgpu_nvlink_early_init(struct gk20a *g)
{
	int err = 0;

	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_NVLINK)) {
		return -EINVAL;
	}

	err = nvgpu_bios_get_nvlink_config_data(g);
	if (err != 0) {
		nvgpu_err(g, "failed to read nvlink vbios data");
		goto exit;
	}

	err = nvgpu_nvlink_discover_ioctrl(g);
	if (err != 0) {
		goto exit;
	}

	/* Enable NVLINK in MC */
	nvgpu_log(g, gpu_dbg_nvlink, "mc_reset_nvlink_mask: 0x%x",
			BIT32(g->nvlink.ioctrl_table[0].reset_enum));
	err = nvgpu_mc_reset_units(g, NVGPU_UNIT_NVLINK);
	if (err != 0) {
		nvgpu_err(g, "Failed to reset NVLINK unit");
	}

	nvgpu_cic_mon_intr_stall_unit_config(g, NVGPU_CIC_INTR_UNIT_NVLINK,
					NVGPU_CIC_INTR_ENABLE);

	err = g->ops.nvlink.discover_link(g);
	if ((err != 0) || (g->nvlink.discovered_links == 0U)) {
		nvgpu_err(g, "No links available");
		goto exit;
	}

	err = nvgpu_falcon_sw_init(g, FALCON_ID_MINION);
	if (err != 0) {
		nvgpu_err(g, "failed to sw init FALCON_ID_MINION");
		goto exit;
	}

	g->nvlink.discovered_links &= ~g->nvlink.link_disable_mask;
	nvgpu_log(g, gpu_dbg_nvlink, "link_disable_mask = 0x%08x (from VBIOS)",
		g->nvlink.link_disable_mask);

	/* Links in reset should be removed from initialized link sw state */
	g->nvlink.initialized_links &= g->ops.nvlink.get_link_reset_mask(g);

	/* VBIOS link_disable_mask should be sufficient to find the connected
	 * links. As VBIOS is not updated with correct mask, we parse the DT
	 * node where we hardcode the link_id. DT method is not scalable as same
	 * DT node is used for different dGPUs connected over PCIE.
	 * Remove the DT parsing of link id and use HAL to get link_mask based
	 * on the GPU. This is temporary fix while we get the VBIOS updated with
	 * correct mask.
	 */
	if (nvgpu_is_errata_present(g, NVGPU_ERRATA_VBIOS_NVLINK_MASK)) {
		g->ops.nvlink.get_connected_link_mask(
			&(g->nvlink.connected_links));
	}

	nvgpu_log(g, gpu_dbg_nvlink, "connected_links = 0x%08x",
						g->nvlink.connected_links);

	/* Track only connected links */
	g->nvlink.discovered_links &= g->nvlink.connected_links;

	nvgpu_log(g, gpu_dbg_nvlink, "discovered_links = 0x%08x (combination)",
		g->nvlink.discovered_links);

	if (hweight32(g->nvlink.discovered_links) > 1U) {
		nvgpu_err(g, "more than one link enabled");
		err = -EINVAL;
		goto nvlink_init_exit;
	}

	g->nvlink.speed = nvgpu_nvlink_speed_20G;
	err = nvgpu_nvlink_minion_load(g);
	if (err != 0) {
		nvgpu_err(g, "Failed Nvlink state load");
		goto nvlink_init_exit;
	}
	err = g->ops.nvlink.configure_ac_coupling(g,
					g->nvlink.ac_coupling_mask, true);
	if (err != 0) {
		nvgpu_err(g, "Failed AC coupling configuration");
		goto nvlink_init_exit;
	}

	/* Program clocks */
	g->ops.nvlink.prog_alt_clk(g);

nvlink_init_exit:
	nvgpu_falcon_sw_free(g, FALCON_ID_MINION);
exit:
	return err;
}

int nvgpu_nvlink_link_early_init(struct gk20a *g)
{
	u32 discovered_links;
	u32 link_id;
	int ret = 0;
	/*
	 * First check the topology and setup connectivity
	 * HACK: we are only enabling one link for now!!!
	 */
	discovered_links = nvgpu_ffs(g->nvlink.discovered_links);
	if (discovered_links == 0) {
		nvgpu_err(g, "discovered links is 0");
		return -EINVAL;
	}

	link_id = (u32)(discovered_links - 1UL);
	g->nvlink.links[link_id].remote_info.is_connected = true;
	g->nvlink.links[link_id].remote_info.device_type =
							nvgpu_nvlink_endp_tegra;

	ret = nvgpu_nvlink_enable_links_pre_top(g, BIT32(link_id));
	if (ret != 0) {
		nvgpu_err(g, "Pre topology failed for link");
		return ret;
	}
	ret = nvgpu_nvlink_enable_links_post_top(g, BIT32(link_id));
	if (ret != 0) {
		nvgpu_err(g, "Post topology failed for link");
		return ret;
	}
	return ret;
}

int nvgpu_nvlink_interface_init(struct gk20a *g)
{
	int err;

	err = g->ops.fb.init_nvlink(g);
	if (err != 0) {
		nvgpu_err(g, "failed to setup nvlinks for sysmem");
		return err;
	}

	return 0;
}

int nvgpu_nvlink_interface_disable(struct gk20a *g)
{
	return 0;
}

int nvgpu_nvlink_dev_shutdown(struct gk20a *g)
{
	nvgpu_falcon_sw_free(g, FALCON_ID_MINION);
	return 0;
}
#endif

int nvgpu_nvlink_remove(struct gk20a *g)
{
#ifdef CONFIG_NVGPU_NVLINK
	int err;

	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_NVLINK)) {
		return -ENODEV;
	}

	nvgpu_set_enabled(g, NVGPU_SUPPORT_NVLINK, false);

	err = nvgpu_nvlink_unregister_link(g);
	if (err != 0) {
		nvgpu_err(g, "failed on nvlink link unregistration");
		return err;
	}

	err = nvgpu_nvlink_unregister_device(g);
	if (err != 0) {
		nvgpu_err(g, "failed on nvlink device unregistration");
		return err;
	}

	nvgpu_kfree(g, g->nvlink.priv);

	return 0;
#else
	return -ENODEV;
#endif
}
