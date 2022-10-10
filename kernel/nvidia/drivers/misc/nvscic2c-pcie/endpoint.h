/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __ENDPOINT_H__
#define __ENDPOINT_H__

#include "common.h"

/* forward declaration. */
struct driver_ctx_t;

/*
 * Entry point for the endpoint(s) char device sub-module/abstraction.
 *
 * On successful return (0), devices would have been created and ready to
 * accept ioctls from user-space application.
 */
int
endpoints_setup(struct driver_ctx_t *drv_ctx, void **endpoints_h);

/* exit point for nvscic2c-pcie endpoints char device sub-module/abstraction.*/
int
endpoints_release(void **endpoints_h);

/*
 * Wait for ack from user space process for PCIe link status change.
 * Deinit edma handle with stream-extension.
 */
int
endpoints_core_deinit(void *endpoints_h);
#endif /*__ENDPOINT_H__ */
