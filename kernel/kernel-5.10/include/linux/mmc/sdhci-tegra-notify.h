/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef LINUX_MMC_SDHCI_TEGRA_NOTIFY_H
#define LINUX_MMC_SDHCI_TEGRA_NOTIFY_H

#include <linux/platform_device.h>
#include <linux/notifier.h>

/*
 * Notification chain for PCIe layer to get notifications from SD layer
 */

/* Get handle for SDMMC instance */
struct device *get_sdhci_device_handle(struct device *dev);

/* Declare notifier register function for PCIe driver */
int register_notifier_from_sd(struct device *dev,
			      struct notifier_block *nb);

/* Declare notifier unregister function for PCIe driver */
int unregister_notifier_from_sd(struct device *dev, struct notifier_block *nb);

/* Define PCIe notification events */
#define CARD_INSERTED		0x1
#define CARD_REMOVED		0x2
#define CARD_IS_SD_EXPRESS	0x3
#define CARD_IS_SD_ONLY		0x4
#define SD_EXP_SUSPEND_COMPLETE 0x5

/*
 * Notification chain for SD layer to get notification from PCIe layer
 */

int notifier_to_sd_call_chain(struct device *dev, int value);

#endif
