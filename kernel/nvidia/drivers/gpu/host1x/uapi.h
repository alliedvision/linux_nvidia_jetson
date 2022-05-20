/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, NVIDIA Corporation.
 */

#ifndef HOST1X_UAPI_H
#define HOST1X_UAPI_H

#include <linux/cdev.h>

struct host1x_uapi {
	struct class *class;

	struct cdev cdev;
	struct device *dev;
	dev_t dev_num;
};

int host1x_uapi_init(struct host1x_uapi *uapi, struct host1x *host1x);
void host1x_uapi_deinit(struct host1x_uapi *uapi);

#endif
