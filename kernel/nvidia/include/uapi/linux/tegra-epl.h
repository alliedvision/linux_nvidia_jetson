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
 * @file tegra-epl.h
 * @brief <b> epl driver header file</b>
 *
 * This file will expose the data types and macros for making ioctl call
 * from user space EPL library.
 */


#ifndef EPL_CLIENT_IOCTL_H
#define EPL_CLIENT_IOCTL_H

/* ==================[Includes]============================================= */

#include <linux/ioctl.h>

/* ==================[MACROS]=============================================== */

/*ioctl call macros*/
#define EPL_REPORT_ERROR_CMD   _IOWR('q', 1, uint8_t *)

#endif /* EPL_CLIENT_IOCTL_H */
