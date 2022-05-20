//SPDX-License-Identifier: GPL-2.0

/*
 *
 * Copyright (c) 2020, NVIDIA Corporation. All rights reserved.
 *
 * Tegra 23x cache driver.
 */

#include <linux/mod_devicetable.h>

#ifdef CONFIG_OF
const struct of_device_id cache_of_match[] = {
	{ .compatible = "nvidia,t19x-cache", .data = NULL, },
	{ .compatible = "nvidia,t23x-cache", .data = NULL, },
	{ },
};
#endif
