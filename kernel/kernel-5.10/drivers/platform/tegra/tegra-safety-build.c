// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
 *
 */

#include <linux/of.h>
#include <soc/tegra/fuse.h>

bool is_tegra_safety_build(void)
{
#ifdef CONFIG_OF
	return of_property_read_bool(of_chosen,
		"nvidia,tegra-safety-build");
#else
	return false;
#endif
}
EXPORT_SYMBOL(is_tegra_safety_build);
