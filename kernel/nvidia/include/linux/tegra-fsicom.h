/*
 * Copyright (c) 2021, NVIDIA CORPORATION, All rights reserved.
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

/**
 * @file tegra-fsicom.h
 * @brief <b> fsicom driver header file</b>
 *
 * This file will expose the data types and macros for making ioctl call
 * from user sapce FSICOM daemon.
 */


#ifndef FSICOM_CLIENT_IOCTL_H
#define FSICOM_CLIENT_IOCTL_H

/* ==================[Includes]============================================= */

#include <linux/ioctl.h>

/* =================[Data types]============================================ */

struct rw_data {
	uint32_t handle;
	uint64_t pa;
	uint64_t iova;
};

/* ==================[MACROS]=============================================== */

/*signal value*/
#define SIG_FSI_DAEMON 44

/*ioctl call macros*/
#define NVMAP_SMMU_MAP    _IOWR('q', 1, struct rw_data *)
#define NVMAP_SMMU_UNMAP  _IOWR('q', 2, struct rw_data *)
#define TEGRA_HSP_WRITE   _IOWR('q', 3, struct rw_data *)
#define TEGRA_SIGNAL_REG  _IOWR('q', 4, struct rw_data *)

#endif
