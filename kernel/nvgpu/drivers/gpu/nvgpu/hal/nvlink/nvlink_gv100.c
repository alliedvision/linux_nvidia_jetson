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

#ifdef CONFIG_NVGPU_NVLINK

#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/timers.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/nvlink_minion.h>
#include <nvgpu/mc.h>

#include "nvlink_gv100.h"

#include <nvgpu/hw/gv100/hw_nvlinkip_discovery_gv100.h>
#include <nvgpu/hw/gv100/hw_ioctrl_gv100.h>
#include <nvgpu/hw/gv100/hw_nvl_gv100.h>
#include <nvgpu/hw/gv100/hw_trim_gv100.h>

#define NVL_DEVICE(str) nvlinkip_discovery_common_device_##str##_v()

u32 gv100_nvlink_get_link_reset_mask(struct gk20a *g)
{
	u32 reg_data;

	reg_data = IOCTRL_REG_RD32(g, ioctrl_reset_r());

	return ioctrl_reset_linkreset_v(reg_data);
}

static const char *gv100_device_type_to_str(u32 type)
{
	if (type == NVL_DEVICE(ioctrl)) {
		return "IOCTRL";
	}
	if (type == NVL_DEVICE(dlpl)) {
		return "DL/PL";
	}
	if (type == NVL_DEVICE(nvltlc)) {
		return "NVLTLC";
	}
	if (type == NVL_DEVICE(ioctrlmif)) {
		return "IOCTRLMIF";
	}
	if (type == NVL_DEVICE(nvlipt)) {
		return "NVLIPT";
	}
	if (type == NVL_DEVICE(minion)) {
		return "MINION";
	}
	if (type == NVL_DEVICE(dlpl_multicast)) {
		return "DL/PL MULTICAST";
	}
	if (type == NVL_DEVICE(nvltlc_multicast)) {
		return "NVLTLC MULTICAST";
	}
	if (type == NVL_DEVICE(ioctrlmif_multicast)) {
		return "IOCTRLMIF MULTICAST";
	}
	return "UNKNOWN";
}

/*
 * Query internal device topology and discover devices in nvlink local
 * infrastructure. Initialize register base and offsets
 */
int gv100_nvlink_discover_link(struct gk20a *g)
{
	u32 i;
	u32 link_id;
	u32 ioctrl_entry_addr;
	u32 ioctrl_device_type;
	u32 table_entry;
	u32 ioctrl_info_entry_type;
	u32 ioctrl_discovery_size;
	bool is_chain = false;
	u8 nvlink_num_devices = 0U;
	unsigned long available_links = 0UL;
	struct nvgpu_nvlink_device_list *device_table;
	int err = 0;
	unsigned long bit;

	/*
	 * Process Entry 0 & 1 of IOCTRL table to find table size
	 */
	if ((g->nvlink.ioctrl_table != NULL) &&
			(g->nvlink.ioctrl_table[0].pri_base_addr != 0U)) {
		ioctrl_entry_addr = g->nvlink.ioctrl_table[0].pri_base_addr;
		table_entry = gk20a_readl(g, ioctrl_entry_addr);
		ioctrl_info_entry_type = nvlinkip_discovery_common_device_v(table_entry);
	} else {
		nvgpu_err(g, " Bad IOCTRL PRI Base addr");
		return -EINVAL;
	}

	if (ioctrl_info_entry_type == NVL_DEVICE(ioctrl)) {
		ioctrl_entry_addr = g->nvlink.ioctrl_table[0].pri_base_addr + 4U;
		table_entry = gk20a_readl(g, ioctrl_entry_addr);
		ioctrl_discovery_size = nvlinkip_discovery_common_ioctrl_length_v(table_entry);
		nvgpu_log(g, gpu_dbg_nvlink, "IOCTRL size: %d", ioctrl_discovery_size);
	} else {
		nvgpu_err(g, " First entry of IOCTRL_DISCOVERY invalid");
		return -EINVAL;
	}

	device_table = nvgpu_kzalloc(g, ioctrl_discovery_size *
			sizeof(struct nvgpu_nvlink_device_list));
	if (device_table == NULL) {
		nvgpu_err(g, " Unable to allocate nvlink device table");
		return -ENOMEM;
	}

	for (i = 0U; i < ioctrl_discovery_size; i++) {
		ioctrl_entry_addr =
			g->nvlink.ioctrl_table[0].pri_base_addr + 4U*i;
		table_entry = gk20a_readl(g, ioctrl_entry_addr);

		nvgpu_log(g, gpu_dbg_nvlink, "parsing ioctrl %d: 0x%08x", i, table_entry);

		ioctrl_info_entry_type = nvlinkip_discovery_common_entry_v(table_entry);

		if (ioctrl_info_entry_type ==
				nvlinkip_discovery_common_entry_invalid_v()) {
			continue;
		}

		if (ioctrl_info_entry_type ==
				nvlinkip_discovery_common_entry_enum_v()) {

			nvgpu_log(g, gpu_dbg_nvlink, "IOCTRL entry %d is ENUM", i);

			ioctrl_device_type =
				nvlinkip_discovery_common_device_v(table_entry);

			if (nvlinkip_discovery_common_chain_v(table_entry) !=
				nvlinkip_discovery_common_chain_enable_v()) {

				nvgpu_log(g, gpu_dbg_nvlink,
					"IOCTRL entry %d is ENUM but no chain",
					i);
				err = -EINVAL;
				break;
			}

			is_chain = true;
			device_table[nvlink_num_devices].valid = true;
			device_table[nvlink_num_devices].device_type =
				(u8)ioctrl_device_type;
			device_table[nvlink_num_devices].device_id =
				(u8)nvlinkip_discovery_common_id_v(table_entry);
			device_table[nvlink_num_devices].device_version =
				nvlinkip_discovery_common_version_v(
								table_entry);
			continue;
		}

		if (ioctrl_info_entry_type ==
				nvlinkip_discovery_common_entry_data1_v()) {
			nvgpu_log(g, gpu_dbg_nvlink, "IOCTRL entry %d is DATA1", i);

			if (is_chain) {
				device_table[nvlink_num_devices].pri_base_addr =
					nvlinkip_discovery_common_pri_base_v(
						table_entry) << 12;

				device_table[nvlink_num_devices].intr_enum =
					(u8)nvlinkip_discovery_common_intr_v(
						table_entry);

				device_table[nvlink_num_devices].reset_enum =
					(u8)nvlinkip_discovery_common_reset_v(
						table_entry);

				nvgpu_log(g, gpu_dbg_nvlink, "IOCTRL entry %d type = %d base: 0x%08x intr: %d reset: %d",
					i,
					device_table[nvlink_num_devices].device_type,
					device_table[nvlink_num_devices].pri_base_addr,
					device_table[nvlink_num_devices].intr_enum,
					device_table[nvlink_num_devices].reset_enum);

				if (device_table[nvlink_num_devices].device_type ==
					NVL_DEVICE(dlpl)) {
					device_table[nvlink_num_devices].num_tx =
						(u8)nvlinkip_discovery_common_dlpl_num_tx_v(table_entry);
					device_table[nvlink_num_devices].num_rx =
						(u8)nvlinkip_discovery_common_dlpl_num_rx_v(table_entry);

					nvgpu_log(g, gpu_dbg_nvlink, "DLPL tx: %d rx: %d",
						device_table[nvlink_num_devices].num_tx,
						device_table[nvlink_num_devices].num_rx);
				}

				if (nvlinkip_discovery_common_chain_v(table_entry) !=
					nvlinkip_discovery_common_chain_enable_v()) {

					is_chain = false;
					nvlink_num_devices++;
				}
			}
			continue;
		}

		if (ioctrl_info_entry_type ==
				nvlinkip_discovery_common_entry_data2_v()) {

			nvgpu_log(g, gpu_dbg_nvlink, "IOCTRL entry %d is DATA2", i);

			if (is_chain) {
				if (nvlinkip_discovery_common_dlpl_data2_type_v(table_entry) != 0U) {
					device_table[nvlink_num_devices].pll_master =
						(u8)nvlinkip_discovery_common_dlpl_data2_master_v(table_entry);
					device_table[nvlink_num_devices].pll_master_id =
						(u8)nvlinkip_discovery_common_dlpl_data2_masterid_v(table_entry);
					nvgpu_log(g, gpu_dbg_nvlink, "PLL info: Master: %d, Master ID: %d",
						device_table[nvlink_num_devices].pll_master,
						device_table[nvlink_num_devices].pll_master_id);
				}

				if (nvlinkip_discovery_common_chain_v(table_entry) !=
					nvlinkip_discovery_common_chain_enable_v()) {

					is_chain = false;
					nvlink_num_devices++;
				}
			}
			continue;
		}
	}

	g->nvlink.device_table = device_table;
	g->nvlink.num_devices = nvlink_num_devices;

	/*
	 * Print table
	 */
	for (i = 0; i < nvlink_num_devices; i++) {
		if (device_table[i].valid) {
			nvgpu_log(g, gpu_dbg_nvlink, "Device %d - %s", i,
				gv100_device_type_to_str(
						device_table[i].device_type));
			nvgpu_log(g, gpu_dbg_nvlink, "+Link/Device Id: %d", device_table[i].device_id);
			nvgpu_log(g, gpu_dbg_nvlink, "+Version: %d", device_table[i].device_version);
			nvgpu_log(g, gpu_dbg_nvlink, "+Base Addr: 0x%08x", device_table[i].pri_base_addr);
			nvgpu_log(g, gpu_dbg_nvlink, "+Intr Enum: %d", device_table[i].intr_enum);
			nvgpu_log(g, gpu_dbg_nvlink, "+Reset Enum: %d", device_table[i].reset_enum);
			if ((device_table[i].device_type == NVL_DEVICE(dlpl)) ||
			    (device_table[i].device_type == NVL_DEVICE(nvlink))) {
				nvgpu_log(g, gpu_dbg_nvlink, "+TX: %d", device_table[i].num_tx);
				nvgpu_log(g, gpu_dbg_nvlink, "+RX: %d", device_table[i].num_rx);
				nvgpu_log(g, gpu_dbg_nvlink, "+PLL Master: %d", device_table[i].pll_master);
				nvgpu_log(g, gpu_dbg_nvlink, "+PLL Master ID: %d", device_table[i].pll_master_id);
			}
		}
	}

	for (i = 0; i < nvlink_num_devices; i++) {
		if (device_table[i].valid) {

			if (device_table[i].device_type == NVL_DEVICE(ioctrl)) {

				g->nvlink.ioctrl_type =
					device_table[i].device_type;
				g->nvlink.ioctrl_base =
					device_table[i].pri_base_addr;
				continue;
			}

			if (device_table[i].device_type == NVL_DEVICE(dlpl)) {

				g->nvlink.dlpl_type =
					device_table[i].device_type;
				g->nvlink.dlpl_base[device_table[i].device_id] =
					device_table[i].pri_base_addr;
				g->nvlink.links[device_table[i].device_id].valid = true;
				g->nvlink.links[device_table[i].device_id].g = g;
				g->nvlink.links[device_table[i].device_id].dlpl_version =
					device_table[i].device_version;
				g->nvlink.links[device_table[i].device_id].dlpl_base =
					device_table[i].pri_base_addr;
				g->nvlink.links[device_table[i].device_id].intr_enum =
					device_table[i].intr_enum;
				g->nvlink.links[device_table[i].device_id].reset_enum =
					device_table[i].reset_enum;
				g->nvlink.links[device_table[i].device_id].link_id =
					device_table[i].device_id;

				/* initiate the PLL master and slave link id to max */
				g->nvlink.links[device_table[i].device_id].pll_master_link_id =
					NVLINK_MAX_LINKS_SW;
				g->nvlink.links[device_table[i].device_id].pll_slave_link_id =
					NVLINK_MAX_LINKS_SW;

				/* Update Pll master */
				if (device_table[i].pll_master != 0U) {
					g->nvlink.links[device_table[i].device_id].pll_master_link_id =
						g->nvlink.links[device_table[i].device_id].link_id;
				} else {
					g->nvlink.links[device_table[i].device_id].pll_master_link_id =
						device_table[i].pll_master_id;
					g->nvlink.links[device_table[i].device_id].pll_slave_link_id =
						g->nvlink.links[device_table[i].device_id].link_id;
					g->nvlink.links[device_table[i].pll_master_id].pll_slave_link_id =
						g->nvlink.links[device_table[i].device_id].link_id;
				}

				available_links |= BIT64(
						device_table[i].device_id);
				continue;
			}

			if (device_table[i].device_type == NVL_DEVICE(nvltlc)) {

				g->nvlink.tl_type = device_table[i].device_type;
				g->nvlink.tl_base[device_table[i].device_id] =
					device_table[i].pri_base_addr;
				g->nvlink.links[device_table[i].device_id].tl_base =
					device_table[i].pri_base_addr;
				g->nvlink.links[device_table[i].device_id].tl_version =
					device_table[i].device_version;
				continue;
			}

			if (device_table[i].device_type == NVL_DEVICE(nvltlc)) {

				g->nvlink.tl_type = device_table[i].device_type;
				g->nvlink.tl_base[device_table[i].device_id] =
					device_table[i].pri_base_addr;
				g->nvlink.links[device_table[i].device_id].tl_base =
					device_table[i].pri_base_addr;
				g->nvlink.links[device_table[i].device_id].tl_version =
					device_table[i].device_version;
				continue;
			}

			if (device_table[i].device_type == NVL_DEVICE(ioctrlmif)) {

				g->nvlink.mif_type = device_table[i].device_type;
				g->nvlink.mif_base[device_table[i].device_id] =
					device_table[i].pri_base_addr;
				g->nvlink.links[device_table[i].device_id].mif_base =
					device_table[i].pri_base_addr;
				g->nvlink.links[device_table[i].device_id].mif_version =
					device_table[i].device_version;
				continue;
			}

			if (device_table[i].device_type == NVL_DEVICE(nvlipt)) {

				g->nvlink.ipt_type =
					device_table[i].device_type;
				g->nvlink.ipt_base =
					device_table[i].pri_base_addr;
				g->nvlink.ipt_version =
					device_table[i].device_version;
				continue;
			}

			if (device_table[i].device_type == NVL_DEVICE(minion)) {

				g->nvlink.minion_type =
					device_table[i].device_type;
				g->nvlink.minion_base =
					device_table[i].pri_base_addr;
				g->nvlink.minion_version =
					device_table[i].device_version;
				continue;
			}

			if (device_table[i].device_type == NVL_DEVICE(dlpl_multicast)) {

				g->nvlink.dlpl_multicast_type =
					device_table[i].device_type;
				g->nvlink.dlpl_multicast_base =
					device_table[i].pri_base_addr;
				g->nvlink.dlpl_multicast_version =
					device_table[i].device_version;
				continue;
			}
			if (device_table[i].device_type == NVL_DEVICE(nvltlc_multicast)) {

				g->nvlink.tl_multicast_type =
					device_table[i].device_type;
				g->nvlink.tl_multicast_base =
					device_table[i].pri_base_addr;
				g->nvlink.tl_multicast_version =
					device_table[i].device_version;
				continue;
			}

			if (device_table[i].device_type == NVL_DEVICE(ioctrlmif_multicast)) {

				g->nvlink.mif_multicast_type =
					device_table[i].device_type;
				g->nvlink.mif_multicast_base =
					device_table[i].pri_base_addr;
				g->nvlink.mif_multicast_version =
					device_table[i].device_version;
				continue;
			}

		}
	}

	g->nvlink.discovered_links = (u32) available_links;

	nvgpu_log(g, gpu_dbg_nvlink, "Nvlink Tree:");
	nvgpu_log(g, gpu_dbg_nvlink, "+ Available Links: 0x%08lx", available_links);
	nvgpu_log(g, gpu_dbg_nvlink, "+ Per-Link Devices:");

	for_each_set_bit(bit, &available_links, NVLINK_MAX_LINKS_SW) {
		link_id = (u32)bit;
		nvgpu_log(g, gpu_dbg_nvlink, "-- Link %d Dl/Pl Base: 0x%08x TLC Base: 0x%08x MIF Base: 0x%08x",
			link_id, g->nvlink.dlpl_base[link_id], g->nvlink.tl_base[link_id], g->nvlink.mif_base[link_id]);
	}

	nvgpu_log(g, gpu_dbg_nvlink, "+ IOCTRL Base: 0x%08x", g->nvlink.ioctrl_base);
	nvgpu_log(g, gpu_dbg_nvlink, "+ NVLIPT Base: 0x%08x", g->nvlink.ipt_base);
	nvgpu_log(g, gpu_dbg_nvlink, "+ MINION Base: 0x%08x", g->nvlink.minion_base);
	nvgpu_log(g, gpu_dbg_nvlink, "+ DLPL MCAST Base: 0x%08x", g->nvlink.dlpl_multicast_base);
	nvgpu_log(g, gpu_dbg_nvlink, "+ TLC MCAST Base: 0x%08x", g->nvlink.tl_multicast_base);
	nvgpu_log(g, gpu_dbg_nvlink, "+ MIF MCAST Base: 0x%08x", g->nvlink.mif_multicast_base);

	if (g->nvlink.minion_version == 0U) {
		nvgpu_err(g, "Unsupported MINION version");

		nvgpu_kfree(g, device_table);
		g->nvlink.device_table = NULL;
		g->nvlink.num_devices = 0;
		return -EINVAL;
	}

	return err;
}


/*
 * Configure AC coupling
 */
int gv100_nvlink_configure_ac_coupling(struct gk20a *g,
	unsigned long mask, bool sync)
{
	int err = 0;
	u32 link_id;
	u32 temp;
	unsigned long bit;

	for_each_set_bit(bit, &mask, NVLINK_MAX_LINKS_SW) {
		link_id = (u32)bit;
		temp = DLPL_REG_RD32(g, link_id, nvl_link_config_r());
		temp &= ~nvl_link_config_ac_safe_en_m();
		temp |= nvl_link_config_ac_safe_en_on_f();

		DLPL_REG_WR32(g, link_id, nvl_link_config_r(), temp);

		err = g->ops.nvlink.minion.send_dlcmd(g, link_id,
				NVGPU_NVLINK_MINION_DLCMD_SETACMODE, sync);

		if (err != 0) {
			return err;
		}
	}

	return err;
}

void gv100_nvlink_prog_alt_clk(struct gk20a *g)
{
	u32 tmp;

	/* RMW registers need to be separate */
	tmp = gk20a_readl(g, trim_sys_nvl_common_clk_alt_switch_r());
	tmp &= ~trim_sys_nvl_common_clk_alt_switch_slowclk_m();
	tmp |= trim_sys_nvl_common_clk_alt_switch_slowclk_xtal4x_f();
	gk20a_writel(g, trim_sys_nvl_common_clk_alt_switch_r(), tmp);
}

void gv100_nvlink_clear_link_reset(struct gk20a *g, u32 link_id)
{
	u32 reg;
	u32 tmp;
	u32 delay = ioctrl_reset_sw_post_reset_delay_microseconds_v();

	reg = IOCTRL_REG_RD32(g, ioctrl_reset_r());
	tmp = (BIT32(link_id) |
		BIT32(g->nvlink.links[link_id].pll_master_link_id));
	reg = set_field(reg, ioctrl_reset_linkreset_m(),
		ioctrl_reset_linkreset_f(ioctrl_reset_linkreset_v(reg) |
		tmp));
	IOCTRL_REG_WR32(g, ioctrl_reset_r(), reg);

	nvgpu_udelay(delay);

	/* Clear warm reset persistent state */
	reg = IOCTRL_REG_RD32(g, ioctrl_debug_reset_r());

	reg &= ~(ioctrl_debug_reset_link_f(1U) |
			ioctrl_debug_reset_common_f(1U));
	IOCTRL_REG_WR32(g, ioctrl_debug_reset_r(), reg);
	nvgpu_udelay(delay);

	reg |= (ioctrl_debug_reset_link_f(1U) |
			ioctrl_debug_reset_common_f(1U));
	IOCTRL_REG_WR32(g, ioctrl_debug_reset_r(), reg);
	nvgpu_udelay(delay);
}

void gv100_nvlink_enable_link_an0(struct gk20a *g, u32 link_id)
{
	u32 reg;

	reg = DLPL_REG_RD32(g, link_id, nvl_link_config_r());
	reg = set_field(reg, nvl_link_config_link_en_m(),
		nvl_link_config_link_en_f(1));
	DLPL_REG_WR32(g, link_id, nvl_link_config_r(), reg);
}


void gv100_nvlink_set_sw_errata(struct gk20a *g, u32 link_id)
{
	u32 reg;

	reg = DLPL_REG_RD32(g, link_id, nvl_sl0_safe_ctrl2_tx_r());
	reg = set_field(reg, nvl_sl0_safe_ctrl2_tx_ctr_init_m(),
		nvl_sl0_safe_ctrl2_tx_ctr_init_init_f());
	reg = set_field(reg, nvl_sl0_safe_ctrl2_tx_ctr_initscl_m(),
		nvl_sl0_safe_ctrl2_tx_ctr_initscl_init_f());
	DLPL_REG_WR32(g, link_id, nvl_sl0_safe_ctrl2_tx_r(), reg);
}

/* Hardcode the link_mask while we wait for VBIOS link_disable_mask field
 * to be updated.
 */
void gv100_nvlink_get_connected_link_mask(u32 *link_mask)
{
	*link_mask = GV100_CONNECTED_LINK_MASK;
}

#endif /* CONFIG_NVGPU_NVLINK */
