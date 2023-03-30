/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _LINUX_TEGRA194_CPUFREQ_H
#define _LINUX_TEGRA194_CPUFREQ_H

enum cluster {
	CLUSTER0,
	CLUSTER1,
	CLUSTER2,
	CLUSTER3,
	MAX_CLUSTERS,
};

struct tegra_cpu_ctr {
	uint32_t cpu;
	uint32_t delay;
	uint32_t coreclk_cnt, last_coreclk_cnt;
	uint32_t refclk_cnt, last_refclk_cnt;
};

uint16_t clamp_ndiv(struct mrq_cpu_ndiv_limits_response *nltbl,
				uint16_t ndiv);
uint16_t map_freq_to_ndiv(struct mrq_cpu_ndiv_limits_response
	*nltbl, uint32_t freq);
struct mrq_cpu_ndiv_limits_response *get_ndiv_limits(enum cluster cl);
void cpufreq_hv_init(struct kobject *kobj);

#endif
