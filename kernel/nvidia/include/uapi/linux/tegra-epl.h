/* SPDX-License-Identifier: (GPL-2.0 WITH Linux-syscall-note)
 *
 * Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 */

#ifndef _UAPI_TEGRA_EPL_H_
#define _UAPI_TEGRA_EPL_H_

#include <linux/ioctl.h>

/* ioctl call macros */
#define EPL_REPORT_ERROR_CMD   _IOWR('q', 1, uint8_t *)

#endif /* _UAPI_TEGRA_EPL_H_ */

