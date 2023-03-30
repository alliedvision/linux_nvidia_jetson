/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Defining registers address and its bit definitions of NVVRS11
 *
 * Copyright (C) 2022 NVIDIA CORPORATION. All rights reserved.
 */

#ifndef _LINUX_I2C_NVVRS11_H_
#define _LINUX_I2C_NVVRS11_H_

#include <linux/types.h>

/* Vendor ID */
#define NVVRS11_REG_VENDOR_ID		0x00
#define NVVRS11_REG_MODEL_REV		0x01

/* Voltage Output registers */
#define NVVRS11_REG_VOUT_A		0x30
#define NVVRS11_REG_VOUT_B		0x33

/* Current Output registers */
#define NVVRS11_REG_IOUT_A		0x31
#define NVVRS11_REG_IOUT_B		0x34

/* Temperature registers */
#define NVVRS11_REG_TEMP_A		0x32
#define NVVRS11_REG_TEMP_B		0x35

struct nvvrs11_chip {
	struct device *dev;
	struct regmap *rmap;
	struct i2c_client *client;
	const char *loopA_rail_name;
	const char *loopB_rail_name;
};

#endif /* _LINUX_I2C_NVVRS11_H_ */
