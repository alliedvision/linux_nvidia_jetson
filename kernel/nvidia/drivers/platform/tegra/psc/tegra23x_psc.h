/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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
 */

#if defined(CONFIG_DEBUG_FS)
int psc_debugfs_create(struct platform_device *pdev);
void psc_debugfs_remove(struct platform_device *pdev);
#else
static inline int psc_debugfs_create(struct platform_device *pdev) { return 0; }
static inline void psc_debugfs_remove(struct platform_device *pdev) { return; }
#endif
