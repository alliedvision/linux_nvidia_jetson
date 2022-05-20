// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020, NVIDIA CORPORATION, All rights reserved.
 */

#include <linux/module.h>
#include <linux/random.h>
#include <linux/string.h>


int __init bad_access(void)
{
	static char source[] = "Twenty characters!!!";
	char dest[10];
	strncpy(dest, source, strlen(source));
        pr_err("%s\n", dest);
	return 0;
}

module_init(bad_access);

MODULE_AUTHOR("Dmitry Pervushin <dpervushin@nvidia.com>");
MODULE_DESCRIPTION("Tegra bad access driver");
MODULE_LICENSE("GPL v2");
