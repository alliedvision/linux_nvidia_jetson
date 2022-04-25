/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _L1SS_IOCTL_H
#define _L1SS_IOCTL_H

#include <linux/ioctl.h>
#include <linux/tegra_nv_guard_service_id.h>
#include <linux/tegra_nv_guard_group_id.h>
#include <linux/platform/tegra/l1ss_datatypes.h>

#define L1SS_IOCTL_MAGIC				'L'
#define L1SS_CLIENT_REQUEST    _IOR(L1SS_IOCTL_MAGIC, 1, nv_guard_request_t)

#endif
