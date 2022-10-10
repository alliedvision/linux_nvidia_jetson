/*
 * Copyright (c) 2017-2022, NVIDIA Corporation.  All rights reserved.
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

/**
 * @file include/media/fusa-capture/capture-vi-channel.h
 *
 * @brief VI channel character device driver header for the T186/T194 Camera
 * RTCPU platform.
 */

#ifndef __FUSA_CAPTURE_VI_CHANNEL_H__
#define __FUSA_CAPTURE_VI_CHANNEL_H__

#include <linux/of_platform.h>

struct vi_channel_drv;

/**
 * @brief VI fops for Host1x syncpt/gos allocations
 *
 * This fops is a HAL for chip/IP generations, see the respective VI platform
 * drivers for the implementations.
 */
struct vi_channel_drv_ops {
	/**
	 * Request a syncpt allocation from Host1x.
	 *
	 * @param[in]	pdev		VI platform_device
	 * @param[in]	name		syncpt name
	 * @param[out]	syncpt_id	assigned syncpt id
	 *
	 * @returns	0 (success), neg. errno (failure)
	 */
	int (*alloc_syncpt)(
		struct platform_device *pdev,
		const char *name,
		uint32_t *syncpt_id);

	/**
	 * Release a syncpt to Host1x.
	 *
	 * @param[in]	pdev	VI platform_device
	 * @param[in]	id	syncpt id to free
	 */
	void (*release_syncpt)(
		struct platform_device *pdev,
		uint32_t id);

	/**
	 * Retrieve the GoS table allocated in the VI-THI carveout.
	 *
	 * @param[in]	pdev	VI platform_device
	 * @param[out]	count	No. of carveout devices
	 * @param[out]	table	GoS table pointer
	 */
	void (*get_gos_table)(
		struct platform_device *pdev,
		int *count,
		const dma_addr_t **table);

	/**
	 * Get a syncpt's GoS backing in the VI-THI carveout.
	 *
	 * @param[in]	pdev		VI platform_device
	 * @param[in]	id		syncpt id
	 * @param[out]	gos_index	GoS id
	 * @param[out]	gos_offset	Offset of syncpt within GoS [dword]
	 *
	 * @returns	0 (success), neg. errno (failure)
	 */
	int (*get_syncpt_gos_backing)(
		struct platform_device *pdev,
		uint32_t id,
		dma_addr_t *syncpt_addr,
		uint32_t *gos_index,
		uint32_t *gos_offset);
};

/**
 * @brief VI channel character device driver context.
 */
struct vi_channel_drv {
	struct platform_device *vi_capture_pdev;
		/**< Capture VI driver platform device */
	bool use_legacy_path;
		/**< Flag to maintain backward-compatibility for T186 */
	struct device *dev; /**< VI kernel @em device */
	struct platform_device *ndev; /**< VI kernel @em platform_device */
	struct mutex lock; /**< VI channel driver context lock */
	u8 num_channels; /**< No. of VI channel character devices */
	const struct vi_channel_drv_ops *ops;
		/**< VI fops for Host1x syncpt/gos allocations */
	struct tegra_vi_channel __rcu *channels[];
		/**< Allocated VI channel contexts */
};

/**
 * @brief VI channel context (character device)
 */
struct tegra_vi_channel {
	struct device *dev; /**< VI device */
	struct platform_device *ndev; /**< VI nvhost platform_device */
	struct platform_device *vi_capture_pdev;
		/**< Capture VI driver platform device */
	struct vi_channel_drv *drv; /**< VI channel driver context */
	struct rcu_head rcu; /**< VI channel rcu */
	struct vi_capture *capture_data; /**< VI channel capture context */
	const struct vi_channel_drv_ops *ops; /**< VI syncpt/gos fops */
	struct device *rtcpu_dev; /**< rtcpu device */
	bool is_stream_opened; /**< Whether the NVCSI stream is opened */
};

/**
 * @brief Create the VI channels driver contexts, and instantiate
 * as many channel character device nodes as specified in the device tree.
 *
 * VI channel nodes appear in the filesystem as:
 * /dev/capture-vi-channel{0..max_vi_channels-1}
 *
 * @param[in]	ndev	VI platform_device context
 * @param[in]	max_vi_channels	Maximum number of VI channels
 * @returns	0 (success), neg. errno (failure)
 */
int vi_channel_drv_register(
	struct platform_device *ndev, unsigned int max_vi_channels);

/**
 * @brief Destroy the VI channels driver and all character device nodes.
 *
 * The VI channels driver and associated channel contexts in memory are freed,
 * rendering the VI platform driver unusable until re-initialized.
 *
 * @param[in]	dev	VI device context
 */
void vi_channel_drv_unregister(
	struct device *dev);

/**
 * @brief Register the chip specific syncpt/gos related function table
 *
 * @param[in]	ops	vi_channel_drv_ops fops
 * @returns	0 (success), neg. errno (failure)
 */
int vi_channel_drv_fops_register(
	const struct vi_channel_drv_ops *ops);

/**
 * @brief Unpin and free the list of pinned capture_mapping's associated with a
 *	  VI capture request.
 *
 * @param[in]	chan		VI channel context
 * @param[in]	buffer_index	Capture descriptor queue index
 */
void vi_capture_request_unpin(
	struct tegra_vi_channel *chan,
	uint32_t buffer_index);

/*
 * Internal APIs for V4L2 driver (aka. VI mode)
 */

/**
 * @brief Open a VI channel character device node, power on the camera subsystem
 * and initialize the channel driver context.
 *
 * The act of opening a VI channel character device node does not entail the
 * reservation of a VI channel, VI_CAPTURE_SETUP must be called afterwards to
 * request an allocation by RCE.
 *
 * @param[in]	channel		VI channel enumerated node iminor no.
 * @param[in]	is_mem_pinned	Whether capture request memory will be pinned
 *
 * @returns	tegra_vi_channel pointer (success), ERR_PTR (failure)
 */
struct tegra_vi_channel *vi_channel_open_ex(
	unsigned int channel,
	bool is_mem_pinned);

/**
 * @brief Release a VI channel character device node, power off the camera
 * subsystem and free the VI channel driver context.
 *
 * Under normal operation, the NVCSI stream and TPG source should be closed, and
 * VI_CAPTURE_RESET followed by VI_CAPTURE_RELEASE should be called before
 * releasing the file handle on the device node.
 *
 * If the user-mode client crashes, the operating system will call this
 * @em release handler to perform all of those actions as part of the @em Abort
 * functionality.
 *
 * @param[in]	channel	VI channel enumerated node iminor no.
 * @param[in]	chan	VI channel context
 *
 * @returns	0
 */
int vi_channel_close_ex(
	unsigned int channel,
	struct tegra_vi_channel *chan);

#endif /* __FUSA_CAPTURE_VI_CHANNEL_H__ */
