// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2020, NVIDIA CORPORATION.  All rights reserved.
 *
 */

#include <linux/of.h>
#include <soc/tegra/fuse.h>

bool is_tegra_hypervisor_mode(void)
{
#ifdef CONFIG_OF
	return of_property_read_bool(of_chosen,
		"nvidia,tegra-hypervisor-mode");
#else
	return false;
#endif
}
EXPORT_SYMBOL(is_tegra_hypervisor_mode);
