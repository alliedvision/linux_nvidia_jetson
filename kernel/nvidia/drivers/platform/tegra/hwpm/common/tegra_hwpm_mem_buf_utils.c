/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/of_address.h>
#include <linux/dma-buf.h>
#include <soc/tegra/fuse.h>
#include <uapi/linux/tegra-soc-hwpm-uapi.h>

#include <tegra_hwpm_log.h>
#include <tegra_hwpm.h>
#include <tegra_hwpm_common.h>
#include <tegra_hwpm_static_analysis.h>

static int tegra_hwpm_dma_map_stream_buffer(struct tegra_soc_hwpm *hwpm,
	struct tegra_soc_hwpm_alloc_pma_stream *alloc_pma_stream)
{
	tegra_hwpm_fn(hwpm, " ");

	hwpm->stream_dma_buf = dma_buf_get(tegra_hwpm_safe_cast_u64_to_s32(
		alloc_pma_stream->stream_buf_fd));
	if (IS_ERR(hwpm->stream_dma_buf)) {
		tegra_hwpm_err(hwpm, "Unable to get stream dma_buf");
		return PTR_ERR(hwpm->stream_dma_buf);
	}
	hwpm->stream_attach = dma_buf_attach(hwpm->stream_dma_buf, hwpm->dev);
	if (IS_ERR(hwpm->stream_attach)) {
		tegra_hwpm_err(hwpm, "Unable to attach stream dma_buf");
		return PTR_ERR(hwpm->stream_attach);
	}
	hwpm->stream_sgt = dma_buf_map_attachment(hwpm->stream_attach,
						  DMA_FROM_DEVICE);
	if (IS_ERR(hwpm->stream_sgt)) {
		tegra_hwpm_err(hwpm, "Unable to map stream attachment");
		return PTR_ERR(hwpm->stream_sgt);
	}

	return 0;
}

static int tegra_hwpm_dma_map_mem_bytes_buffer(struct tegra_soc_hwpm *hwpm,
	struct tegra_soc_hwpm_alloc_pma_stream *alloc_pma_stream)
{
	tegra_hwpm_fn(hwpm, " ");

	hwpm->mem_bytes_dma_buf = dma_buf_get(tegra_hwpm_safe_cast_u64_to_s32(
		alloc_pma_stream->mem_bytes_buf_fd));
	if (IS_ERR(hwpm->mem_bytes_dma_buf)) {
		tegra_hwpm_err(hwpm, "Unable to get mem bytes dma_buf");
		return PTR_ERR(hwpm->mem_bytes_dma_buf);
	}

	hwpm->mem_bytes_attach = dma_buf_attach(hwpm->mem_bytes_dma_buf,
						hwpm->dev);
	if (IS_ERR(hwpm->mem_bytes_attach)) {
		tegra_hwpm_err(hwpm, "Unable to attach mem bytes dma_buf");
		return PTR_ERR(hwpm->mem_bytes_attach);
	}

	hwpm->mem_bytes_sgt = dma_buf_map_attachment(hwpm->mem_bytes_attach,
						     DMA_FROM_DEVICE);
	if (IS_ERR(hwpm->mem_bytes_sgt)) {
		tegra_hwpm_err(hwpm, "Unable to map mem bytes attachment");
		return PTR_ERR(hwpm->mem_bytes_sgt);
	}

	hwpm->mem_bytes_kernel = dma_buf_vmap(hwpm->mem_bytes_dma_buf);
	if (!hwpm->mem_bytes_kernel) {
		tegra_hwpm_err(hwpm,
			"Unable to map mem_bytes buffer into kernel VA space");
		return -ENOMEM;
	}
	memset(hwpm->mem_bytes_kernel, 0, 32);

	return 0;
}

static int tegra_hwpm_reset_stream_buf(struct tegra_soc_hwpm *hwpm)
{
	tegra_hwpm_fn(hwpm, " ");

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

	return 0;
}

int tegra_hwpm_map_stream_buffer(struct tegra_soc_hwpm *hwpm,
	struct tegra_soc_hwpm_alloc_pma_stream *alloc_pma_stream)
{
	int ret = 0, err = 0;

	tegra_hwpm_fn(hwpm, " ");

	/* Memory map stream buffer */
	ret = tegra_hwpm_dma_map_stream_buffer(hwpm, alloc_pma_stream);
	if (ret != 0) {
		tegra_hwpm_err(hwpm, "Failed to map stream buffer");
		goto fail;
	}

	alloc_pma_stream->stream_buf_pma_va =
					sg_dma_address(hwpm->stream_sgt->sgl);
	if (alloc_pma_stream->stream_buf_pma_va == 0) {
		tegra_hwpm_err(hwpm, "Invalid stream buffer SMMU IOVA");
		ret = -ENXIO;
		goto fail;
	}
	tegra_hwpm_dbg(hwpm, hwpm_dbg_alloc_pma_stream,
		"stream_buf_pma_va = 0x%llx",
		alloc_pma_stream->stream_buf_pma_va);

	/* Memory map mem bytes buffer */
	ret = tegra_hwpm_dma_map_mem_bytes_buffer(hwpm, alloc_pma_stream);
	if (ret != 0) {
		tegra_hwpm_err(hwpm, "Failed to map mem bytes buffer");
		goto fail;
	}

	/* Configure memory management */
	ret = hwpm->active_chip->enable_mem_mgmt(hwpm, alloc_pma_stream);
	if (ret != 0) {
		tegra_hwpm_err(hwpm, "Failed to configure stream memory");
		goto fail;
	}

	return 0;

fail:
	/* Invalidate memory config */
	err = hwpm->active_chip->invalidate_mem_config(hwpm);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "Failed to invalidate memory config");
	}

	/* Disable memory management */
	err = hwpm->active_chip->disable_mem_mgmt(hwpm);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "Failed to disable memory management");
	}

	alloc_pma_stream->stream_buf_pma_va = 0;

	/* Reset stream buffer */
	err = tegra_hwpm_reset_stream_buf(hwpm);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "Failed to reset stream buffer");
	}

	return ret;
}

int tegra_hwpm_clear_mem_pipeline(struct tegra_soc_hwpm *hwpm)
{
	int ret = 0;

	tegra_hwpm_fn(hwpm, " ");

	/* Stream MEM_BYTES to clear pipeline */
	if (hwpm->mem_bytes_kernel) {
		s32 timeout_msecs = 1000;
		u32 sleep_msecs = 100;
		u32 *mem_bytes_kernel_u32 = (u32 *)(hwpm->mem_bytes_kernel);

		do {
			ret = hwpm->active_chip->stream_mem_bytes(hwpm);
			if (ret != 0) {
				tegra_hwpm_err(hwpm,
					"Trigger mem_bytes streaming failed");
				goto fail;
			}
			msleep(sleep_msecs);
			timeout_msecs -= sleep_msecs;
		} while ((*mem_bytes_kernel_u32 ==
			TEGRA_SOC_HWPM_MEM_BYTES_INVALID) &&
			(timeout_msecs > 0));

		if (timeout_msecs <= 0) {
			tegra_hwpm_err(hwpm,
				"Timeout expired for MEM_BYTES streaming");
			return -ETIMEDOUT;
		}
	}

	ret = hwpm->active_chip->disable_pma_streaming(hwpm);
	if (ret != 0) {
		tegra_hwpm_err(hwpm, "Failed to disable pma streaming");
		goto fail;
	}

	/* Disable memory management */
	ret = hwpm->active_chip->disable_mem_mgmt(hwpm);
	if (ret != 0) {
		tegra_hwpm_err(hwpm, "Failed to disable memory management");
		goto fail;
	}

	/* Reset stream buffer */
	ret = tegra_hwpm_reset_stream_buf(hwpm);
	if (ret != 0) {
		tegra_hwpm_err(hwpm, "Failed to reset stream buffer");
		goto fail;
	}
fail:
	return ret;
}

int tegra_hwpm_update_mem_bytes(struct tegra_soc_hwpm *hwpm,
	struct tegra_soc_hwpm_update_get_put *update_get_put)
{
	int ret;

	tegra_hwpm_fn(hwpm, " ");

	/* Update SW get pointer */
	ret = hwpm->active_chip->update_mem_bytes_get_ptr(hwpm,
		update_get_put->mem_bump);
	if (ret != 0) {
		tegra_hwpm_err(hwpm, "Failed to update mem_bytes get ptr");
		return -EINVAL;
	}

	/* Stream MEM_BYTES value to MEM_BYTES buffer */
	if (update_get_put->b_stream_mem_bytes) {
		ret = hwpm->active_chip->stream_mem_bytes(hwpm);
		if (ret != 0) {
			tegra_hwpm_err(hwpm,
				"Failed to trigger mem_bytes streaming");
		}
	}

	/* Read HW put pointer */
	if (update_get_put->b_read_mem_head) {
		update_get_put->mem_head =
			hwpm->active_chip->get_mem_bytes_put_ptr(hwpm);
		tegra_hwpm_dbg(hwpm, hwpm_dbg_update_get_put,
			"MEM_HEAD = 0x%llx", update_get_put->mem_head);
	}

	/* Check overflow error status */
	if (update_get_put->b_check_overflow) {
		update_get_put->b_overflowed =
			(u8) hwpm->active_chip->membuf_overflow_status(hwpm);
		tegra_hwpm_dbg(hwpm, hwpm_dbg_update_get_put, "OVERFLOWED = %u",
			update_get_put->b_overflowed);
	}

	return 0;
}
