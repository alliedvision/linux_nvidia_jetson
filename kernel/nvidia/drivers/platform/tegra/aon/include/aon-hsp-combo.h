/*
 * Copyright (c) 2020, NVIDIA CORPORATION.	All rights reserved.
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

#ifndef INCLUDE_AON_HSP_COMBO_H
#define INCLUDE_AON_HSP_COMBO_H

#include <linux/types.h>

struct tegra_aon;

int tegra_aon_hsp_sm_tx_write(struct tegra_aon *aon, u32 value);
int tegra_aon_hsp_sm_pair_request(struct tegra_aon *aon,
				void (*full_notify)(void *data, u32 value),
				void *pdata);
void tegra_aon_hsp_sm_pair_free(struct tegra_aon *aon);
bool tegra_aon_hsp_sm_tx_is_empty(struct tegra_aon *aon);

#endif
