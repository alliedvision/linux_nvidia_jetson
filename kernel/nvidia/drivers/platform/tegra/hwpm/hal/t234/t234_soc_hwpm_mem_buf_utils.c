/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <soc/tegra/fuse.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/of_address.h>
#include <linux/dma-buf.h>

#include <tegra-soc-hwpm-log.h>
#include <tegra-soc-hwpm-io.h>
#include <hal/tegra-soc-hwpm-structures.h>
#include <uapi/linux/tegra-soc-hwpm-uapi.h>
#include <hal/t234/t234_soc_hwpm_perfmon_dt.h>
#include <hal/t234/t234_soc_hwpm_init.h>
#include <hal/t234/hw/t234_addr_map_soc_hwpm.h>

int t234_soc_hwpm_update_mem_bytes(struct tegra_soc_hwpm *hwpm,
		struct tegra_soc_hwpm_update_get_put *update_get_put)
{
	u32 *mem_bytes_kernel_u32 = NULL;
	u32 reg_val = 0U;
	u32 field_val = 0U;
	int ret;


	/* Update SW get pointer */
	hwpm_writel(hwpm, T234_SOC_HWPM_PMA_DT,
		pmasys_channel_mem_bump_r(0) - addr_map_pma_base_r(),
		update_get_put->mem_bump);

	/* Stream MEM_BYTES value to MEM_BYTES buffer */
	if (update_get_put->b_stream_mem_bytes) {
		mem_bytes_kernel_u32 = (u32 *)(hwpm->mem_bytes_kernel);
		*mem_bytes_kernel_u32 = TEGRA_SOC_HWPM_MEM_BYTES_INVALID;
		ret = reg_rmw(hwpm, NULL, T234_SOC_HWPM_PMA_DT,
			pmasys_channel_control_user_r(0) - addr_map_pma_base_r(),
			pmasys_channel_control_user_update_bytes_m(),
			pmasys_channel_control_user_update_bytes_doit_f(),
			false, false);
		if (ret < 0) {
			tegra_soc_hwpm_err("Failed to stream mem_bytes to buffer");
			return -EIO;
		}
	}

	/* Read HW put pointer */
	if (update_get_put->b_read_mem_head) {
		update_get_put->mem_head = hwpm_readl(hwpm,
			T234_SOC_HWPM_PMA_DT,
			pmasys_channel_mem_head_r(0) - addr_map_pma_base_r());
		tegra_soc_hwpm_dbg("MEM_HEAD = 0x%llx",
				   update_get_put->mem_head);
	}

	/* Check overflow error status */
	if (update_get_put->b_check_overflow) {
		reg_val = hwpm_readl(hwpm, T234_SOC_HWPM_PMA_DT,
			pmasys_channel_status_secure_r(0) -
			addr_map_pma_base_r());
		field_val = pmasys_channel_status_secure_membuf_status_v(
			reg_val);
		update_get_put->b_overflowed = (field_val ==
			pmasys_channel_status_secure_membuf_status_overflowed_v());
		tegra_soc_hwpm_dbg("OVERFLOWED = %u",
				   update_get_put->b_overflowed);
	}

	return 0;
}

int t234_soc_hwpm_clear_pipeline(struct tegra_soc_hwpm *hwpm)
{
	int err = 0;
	int ret = 0;
	bool timeout = false;
	u32 *mem_bytes_kernel_u32 = NULL;

	/* Stream MEM_BYTES to clear pipeline */
	if (hwpm->mem_bytes_kernel) {
		mem_bytes_kernel_u32 = (u32 *)(hwpm->mem_bytes_kernel);
		*mem_bytes_kernel_u32 = TEGRA_SOC_HWPM_MEM_BYTES_INVALID;
		err = reg_rmw(hwpm, NULL, T234_SOC_HWPM_PMA_DT,
			pmasys_channel_control_user_r(0) - addr_map_pma_base_r(),
			pmasys_channel_control_user_update_bytes_m(),
			pmasys_channel_control_user_update_bytes_doit_f(),
			false, false);
		RELEASE_FAIL("Unable to stream MEM_BYTES");
		timeout = HWPM_TIMEOUT(*mem_bytes_kernel_u32 !=
				       TEGRA_SOC_HWPM_MEM_BYTES_INVALID,
			     "MEM_BYTES streaming");
		if (timeout && ret == 0)
			ret = -EIO;
	}

	/* Disable PMA streaming */
	err = reg_rmw(hwpm, NULL, T234_SOC_HWPM_PMA_DT,
		pmasys_trigger_config_user_r(0) - addr_map_pma_base_r(),
		pmasys_trigger_config_user_record_stream_m(),
		pmasys_trigger_config_user_record_stream_disable_f(),
		false, false);
	RELEASE_FAIL("Unable to disable PMA streaming");

	err = reg_rmw(hwpm, NULL, T234_SOC_HWPM_PMA_DT,
		pmasys_channel_control_user_r(0) - addr_map_pma_base_r(),
		pmasys_channel_control_user_stream_m(),
		pmasys_channel_control_user_stream_disable_f(),
		false, false);
	RELEASE_FAIL("Unable to disable PMA streaming");

	/* Memory Management */
	hwpm_writel(hwpm, T234_SOC_HWPM_PMA_DT,
		pmasys_channel_outbase_r(0) - addr_map_pma_base_r(), 0);
	hwpm_writel(hwpm, T234_SOC_HWPM_PMA_DT,
		pmasys_channel_outbaseupper_r(0) - addr_map_pma_base_r(), 0);
	hwpm_writel(hwpm, T234_SOC_HWPM_PMA_DT,
		pmasys_channel_outsize_r(0) - addr_map_pma_base_r(), 0);
	hwpm_writel(hwpm, T234_SOC_HWPM_PMA_DT,
		pmasys_channel_mem_bytes_addr_r(0) - addr_map_pma_base_r(), 0);

	if (hwpm->stream_sgt && (!IS_ERR(hwpm->stream_sgt))) {
		dma_buf_unmap_attachment(hwpm->stream_attach,
					 hwpm->stream_sgt,
					 DMA_FROM_DEVICE);
	}
	hwpm->stream_sgt = NULL;

	if (hwpm->stream_attach && (!IS_ERR(hwpm->stream_attach))) {
		dma_buf_detach(hwpm->stream_dma_buf, hwpm->stream_attach);
	}
	hwpm->stream_attach = NULL;

	if (hwpm->stream_dma_buf && (!IS_ERR(hwpm->stream_dma_buf))) {
		dma_buf_put(hwpm->stream_dma_buf);
	}
	hwpm->stream_dma_buf = NULL;

	if (hwpm->mem_bytes_kernel) {
		dma_buf_vunmap(hwpm->mem_bytes_dma_buf,
			       hwpm->mem_bytes_kernel);
		hwpm->mem_bytes_kernel = NULL;
	}

	if (hwpm->mem_bytes_sgt && (!IS_ERR(hwpm->mem_bytes_sgt))) {
		dma_buf_unmap_attachment(hwpm->mem_bytes_attach,
					 hwpm->mem_bytes_sgt,
					 DMA_FROM_DEVICE);
	}
	hwpm->mem_bytes_sgt = NULL;

	if (hwpm->mem_bytes_attach && (!IS_ERR(hwpm->mem_bytes_attach))) {
		dma_buf_detach(hwpm->mem_bytes_dma_buf, hwpm->mem_bytes_attach);
	}
	hwpm->mem_bytes_attach = NULL;

	if (hwpm->mem_bytes_dma_buf && (!IS_ERR(hwpm->mem_bytes_dma_buf))) {
		dma_buf_put(hwpm->mem_bytes_dma_buf);
	}
	hwpm->mem_bytes_dma_buf = NULL;

	return ret;
}

int t234_soc_hwpm_stream_buf_map(struct tegra_soc_hwpm *hwpm,
	struct tegra_soc_hwpm_alloc_pma_stream *alloc_pma_stream)
{
	int ret = 0;
	u32 reg_val = 0;
	u32 outbase_lo = 0;
	u32 outbase_hi = 0;
	u32 outsize = 0;
	u32 mem_bytes_addr = 0;

	/* Memory map stream buffer */
	hwpm->stream_dma_buf = dma_buf_get(alloc_pma_stream->stream_buf_fd);
	if (IS_ERR(hwpm->stream_dma_buf)) {
		tegra_soc_hwpm_err("Unable to get stream dma_buf");
		ret = PTR_ERR(hwpm->stream_dma_buf);
		goto fail;
	}
	hwpm->stream_attach = dma_buf_attach(hwpm->stream_dma_buf, hwpm->dev);
	if (IS_ERR(hwpm->stream_attach)) {
		tegra_soc_hwpm_err("Unable to attach stream dma_buf");
		ret = PTR_ERR(hwpm->stream_attach);
		goto fail;
	}
	hwpm->stream_sgt = dma_buf_map_attachment(hwpm->stream_attach,
						  DMA_FROM_DEVICE);
	if (IS_ERR(hwpm->stream_sgt)) {
		tegra_soc_hwpm_err("Unable to map stream attachment");
		ret = PTR_ERR(hwpm->stream_sgt);
		goto fail;
	}
	alloc_pma_stream->stream_buf_pma_va =
					sg_dma_address(hwpm->stream_sgt->sgl);
	if (alloc_pma_stream->stream_buf_pma_va == 0) {
		tegra_soc_hwpm_err("Invalid stream buffer SMMU IOVA");
		ret = -ENXIO;
		goto fail;
	}
	tegra_soc_hwpm_dbg("stream_buf_pma_va = 0x%llx",
			   alloc_pma_stream->stream_buf_pma_va);

	/* Memory map mem bytes buffer */
	hwpm->mem_bytes_dma_buf =
				dma_buf_get(alloc_pma_stream->mem_bytes_buf_fd);
	if (IS_ERR(hwpm->mem_bytes_dma_buf)) {
		tegra_soc_hwpm_err("Unable to get mem bytes dma_buf");
		ret = PTR_ERR(hwpm->mem_bytes_dma_buf);
		goto fail;
	}
	hwpm->mem_bytes_attach = dma_buf_attach(hwpm->mem_bytes_dma_buf,
						hwpm->dev);
	if (IS_ERR(hwpm->mem_bytes_attach)) {
		tegra_soc_hwpm_err("Unable to attach mem bytes dma_buf");
		ret = PTR_ERR(hwpm->mem_bytes_attach);
		goto fail;
	}
	hwpm->mem_bytes_sgt = dma_buf_map_attachment(hwpm->mem_bytes_attach,
						     DMA_FROM_DEVICE);
	if (IS_ERR(hwpm->mem_bytes_sgt)) {
		tegra_soc_hwpm_err("Unable to map mem bytes attachment");
		ret = PTR_ERR(hwpm->mem_bytes_sgt);
		goto fail;
	}
	hwpm->mem_bytes_kernel = dma_buf_vmap(hwpm->mem_bytes_dma_buf);
	if (!hwpm->mem_bytes_kernel) {
		tegra_soc_hwpm_err(
			"Unable to map mem_bytes buffer into kernel VA space");
		ret = -ENOMEM;
		goto fail;
	}
	memset(hwpm->mem_bytes_kernel, 0, 32);

	outbase_lo = alloc_pma_stream->stream_buf_pma_va &
			pmasys_channel_outbase_ptr_m();
	hwpm_writel(hwpm, T234_SOC_HWPM_PMA_DT,
		pmasys_channel_outbase_r(0) - addr_map_pma_base_r(),
		outbase_lo);
	tegra_soc_hwpm_dbg("OUTBASE = 0x%x", reg_val);

	outbase_hi = (alloc_pma_stream->stream_buf_pma_va >> 32) &
			pmasys_channel_outbaseupper_ptr_m();
	hwpm_writel(hwpm, T234_SOC_HWPM_PMA_DT,
		pmasys_channel_outbaseupper_r(0) - addr_map_pma_base_r(),
		outbase_hi);
	tegra_soc_hwpm_dbg("OUTBASEUPPER = 0x%x", reg_val);

	outsize = alloc_pma_stream->stream_buf_size &
			pmasys_channel_outsize_numbytes_m();
	hwpm_writel(hwpm, T234_SOC_HWPM_PMA_DT,
		pmasys_channel_outsize_r(0) - addr_map_pma_base_r(),
		outsize);
	tegra_soc_hwpm_dbg("OUTSIZE = 0x%x", reg_val);

	mem_bytes_addr = sg_dma_address(hwpm->mem_bytes_sgt->sgl) &
			pmasys_channel_mem_bytes_addr_ptr_m();
	hwpm_writel(hwpm, T234_SOC_HWPM_PMA_DT,
		pmasys_channel_mem_bytes_addr_r(0) - addr_map_pma_base_r(),
		mem_bytes_addr);
	tegra_soc_hwpm_dbg("MEM_BYTES_ADDR = 0x%x", reg_val);

	hwpm_writel(hwpm, T234_SOC_HWPM_PMA_DT,
		pmasys_channel_mem_block_r(0) - addr_map_pma_base_r(),
		pmasys_channel_mem_block_valid_f(
			pmasys_channel_mem_block_valid_true_v()));

	return 0;

fail:
	hwpm_writel(hwpm, T234_SOC_HWPM_PMA_DT,
		pmasys_channel_mem_block_r(0) - addr_map_pma_base_r(),
		pmasys_channel_mem_block_valid_f(
			pmasys_channel_mem_block_valid_false_v()));
	hwpm_writel(hwpm, T234_SOC_HWPM_PMA_DT,
		pmasys_channel_outbase_r(0) - addr_map_pma_base_r(), 0);
	hwpm_writel(hwpm, T234_SOC_HWPM_PMA_DT,
		pmasys_channel_outbaseupper_r(0) - addr_map_pma_base_r(), 0);
	hwpm_writel(hwpm, T234_SOC_HWPM_PMA_DT,
		pmasys_channel_outsize_r(0) - addr_map_pma_base_r(), 0);
	hwpm_writel(hwpm, T234_SOC_HWPM_PMA_DT,
		pmasys_channel_mem_bytes_addr_r(0) - addr_map_pma_base_r(), 0);

	alloc_pma_stream->stream_buf_pma_va = 0;

	if (hwpm->stream_sgt && (!IS_ERR(hwpm->stream_sgt))) {
		dma_buf_unmap_attachment(hwpm->stream_attach,
					 hwpm->stream_sgt,
					 DMA_FROM_DEVICE);
	}
	hwpm->stream_sgt = NULL;

	if (hwpm->stream_attach && (!IS_ERR(hwpm->stream_attach))) {
		dma_buf_detach(hwpm->stream_dma_buf, hwpm->stream_attach);
	}
	hwpm->stream_attach = NULL;

	if (hwpm->stream_dma_buf && (!IS_ERR(hwpm->stream_dma_buf))) {
		dma_buf_put(hwpm->stream_dma_buf);
	}
	hwpm->stream_dma_buf = NULL;

	if (hwpm->mem_bytes_kernel) {
		dma_buf_vunmap(hwpm->mem_bytes_dma_buf,
			       hwpm->mem_bytes_kernel);
		hwpm->mem_bytes_kernel = NULL;
	}
	if (hwpm->mem_bytes_sgt && (!IS_ERR(hwpm->mem_bytes_sgt))) {
		dma_buf_unmap_attachment(hwpm->mem_bytes_attach,
					 hwpm->mem_bytes_sgt,
					 DMA_FROM_DEVICE);
	}
	hwpm->mem_bytes_sgt = NULL;

	if (hwpm->mem_bytes_attach && (!IS_ERR(hwpm->mem_bytes_attach))) {
		dma_buf_detach(hwpm->mem_bytes_dma_buf, hwpm->mem_bytes_attach);
	}
	hwpm->mem_bytes_attach = NULL;

	if (hwpm->mem_bytes_dma_buf && (!IS_ERR(hwpm->mem_bytes_dma_buf))) {
		dma_buf_put(hwpm->mem_bytes_dma_buf);
	}
	hwpm->mem_bytes_dma_buf = NULL;

	return ret;
}
