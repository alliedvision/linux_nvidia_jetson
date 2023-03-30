/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _TEGRA_AON_IVC_PLLAON_H_
#define _TEGRA_AON_IVC_PLLAON_H_

#include <linux/types.h>

/* Get state of PLLAON clock from aon controller.
 * 1 - if PLLAON clock is enabled
 * 0 - if PLLAON clock is disabled
 * -EINVAL - if IVC fails
 */
int tegra_aon_get_pllaon_state(void);

/* Set state of PLLAON clock from aon controller.
 * @enable true  - to enable PLLAON clock
 *	   false - to disable PLLAON clock
 * @return
 * 	0 - if IVC with AON controller is successful
 * 	-EINVAL - if IVC fails
 */
int tegra_aon_set_pllaon_state(bool enable);

#endif
