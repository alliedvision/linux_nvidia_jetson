/*
 * Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES.  All rights reserved.
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

/**
 * @file  tegra-hsierrrptinj.h
 * @brief <b> HSI Error Report Injection driver header file</b>
 *
 * This file will expose API prototypes for HSI Error Report Injection kernel
 * space APIs.
 */

#ifndef TEGRA_HSIERRRPTINJ_H
#define TEGRA_HSIERRRPTINJ_H

/* ==================[Includes]============================================= */

#include "tegra-epl.h"

/* ==================[Type Definitions]===================================== */
/**
 * @brief IP Ids
 */
typedef enum {
IP_EQOS  = 0x0000,
IP_GPU   = 0x0001,
IP_I2C   = 0x0002,
IP_MGBE  = 0x0003,
IP_PCIE  = 0x0004,
IP_PSC   = 0x0005,
IP_QSPI  = 0x0006,
IP_TSEC  = 0x0007,
IP_SDMMC = 0x0008,
IP_OTHER = 0x0009
} hsierrrpt_ipid_t;

/**
 * @brief Callback signature for initiating HSI error reports to FSI
 *
 * @param[in]   instance_id             Instance of the supported IP.
 * @param[in]   err_rpt_frame           Error frame to be reported.
 *
 * API signature for the common callback function that will be
 * implemented by the set of Tegra onchip IP drivers that report HSI
 * errors to the FSI.
 *
 * @returns
 *  0           (success)
 *  -EINVAL     (On invalid arguments)
 *  -ENODEV     (On device driver not loaded)
 *  -EFAULT     (On IP driver failure to report error)
 *  -ETIME      (On timeout in IP driver)
 */
int (*hsierrrpt_inj)(uint32_t instance_id, struct epl_error_report_frame err_rpt_frame);

/**
 * @brief HSI error report injection callback registration
 *
 * @param[in]   ip_id                   Supported IP Id.
 * @param[in]   cb_func                 Pointer to callback function.
 *
 * API to register the HSI error report trigger callback function
 * with the utility. Tegra onchip IP drivers supporting HSI error
 * reporting to FSI shall call this API once, at launch time.
 *
 * @returns
 *  0           (success)
 *  -EINVAL     (On invalid arguments)
 *  -ENODEV     (On device driver not loaded)
 *  -EFAULT     (On IP driver failure to register callback)
 *  -ETIME      (On timeout in IP driver)
 */
int hsierrrpt_reg_cb(hsierrrpt_ipid_t ip_id, hsierrrpt_inj cb_func);


#endif /* TEGRA_EPL_H */
