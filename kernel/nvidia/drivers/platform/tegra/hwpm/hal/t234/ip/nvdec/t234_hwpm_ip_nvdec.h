/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef T234_HWPM_IP_NVDEC_H
#define T234_HWPM_IP_NVDEC_H

#if defined(CONFIG_SOC_HWPM_IP_NVDEC)
#define T234_HWPM_ACTIVE_IP_NVDEC		T234_HWPM_IP_NVDEC,

/* This data should ideally be available in HW headers */
#define T234_HWPM_IP_NVDEC_NUM_INSTANCES		1U
#define T234_HWPM_IP_NVDEC_NUM_CORE_ELEMENT_PER_INST	1U
#define T234_HWPM_IP_NVDEC_NUM_PERFMON_PER_INST		1U
#define T234_HWPM_IP_NVDEC_NUM_PERFMUX_PER_INST		1U
#define T234_HWPM_IP_NVDEC_NUM_BROADCAST_PER_INST	0U

extern struct hwpm_ip t234_hwpm_ip_nvdec;

#else
#define T234_HWPM_ACTIVE_IP_NVDEC
#endif

#endif /* T234_HWPM_IP_NVDEC_H */
