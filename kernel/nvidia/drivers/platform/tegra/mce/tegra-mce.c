/*
 * Copyright (c) 2014-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/debugfs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/tegra-mce.h>

#include <asm/smp_plat.h>

static struct tegra_mce_ops *mce_ops;

void tegra_mce_set_ops(struct tegra_mce_ops *tegra_mce_plat_ops)
{
	mce_ops = tegra_mce_plat_ops;
}

/**
 * Specify power state and wake time for entering upon STANDBYWFI
 *
 * @state:		requested core power state
 * @wake_time:	wake time in TSC ticks
 *
 * Returns 0 if success.
 */
int tegra_mce_enter_cstate(u32 state, u32 wake_time)
{
	if (mce_ops && mce_ops->enter_cstate)
		return mce_ops->enter_cstate(state, wake_time);
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_enter_cstate);

/**
 * Specify deepest cluster/ccplex/system states allowed.
 *
 * @cluster:	deepest cluster-wide state
 * @ccplex:		deepest ccplex-wide state
 * @system:		deepest system-wide state
 * @force:		forced system state
 * @wake_mask:	wake mask to be updated
 * @valid:		is wake_mask applicable?
 *
 * Returns 0 if success.
 */
int tegra_mce_update_cstate_info(u32 cluster, u32 ccplex, u32 system,
				 u8 force, u32 wake_mask, bool valid)
{
	if (mce_ops && mce_ops->update_cstate_info)
		return mce_ops->update_cstate_info(cluster, ccplex, system,
					     force, wake_mask, valid);
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_update_cstate_info);

/**
 * Update threshold for one specific c-state crossover
 *
 * @type: type of state crossover.
 * @time: idle time threshold.
 *
 * Returns 0 if success.
 */
int tegra_mce_update_crossover_time(u32 type, u32 time)
{
	if (mce_ops && mce_ops->update_crossover_time)
		return mce_ops->update_crossover_time(type, time);
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_update_crossover_time);

/**
 * Query the runtime stats of a specific cstate
 *
 * @state: c-state of the stats.
 * @stats: output integer to hold the stats.
 *
 * Returns 0 if success.
 */
int tegra_mce_read_cstate_stats(u32 state, u64 *stats)
{
	if (mce_ops && mce_ops->read_cstate_stats)
		return mce_ops->read_cstate_stats(state, stats);
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_read_cstate_stats);

/**
 * Overwrite the runtime stats of a specific c-state
 *
 * @state: c-state of the stats.
 * @stats: integer represents the new stats.
 *
 * Returns 0 if success.
 */
int tegra_mce_write_cstate_stats(u32 state, u32 stats)
{
	if (mce_ops && mce_ops->write_cstate_stats)
		return mce_ops->write_cstate_stats(state, stats);
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_write_cstate_stats);

/**
 * Query MCE to determine if SC7 is allowed
 * given a target core's C-state and wake time
 *
 * @state: c-state of the stats.
 * @stats: integer represents the new stats.
 * @allowed: pointer to result
 *
 * Returns 0 if success.
 */
int tegra_mce_is_sc7_allowed(u32 state, u32 wake, u32 *allowed)
{
	if (mce_ops && mce_ops->is_sc7_allowed)
		return mce_ops->is_sc7_allowed(state, wake, allowed);
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_is_sc7_allowed);

/**
 * Bring another offlined core back online to C0 state.
 *
 * @cpu: logical cpuid from smp_processor_id()
 *
 * Returns 0 if success.
 */
int tegra_mce_online_core(int cpu)
{
	if (mce_ops && mce_ops->online_core)
		return mce_ops->online_core(cpu);
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_online_core);

/**
 * Program Auto-CC3 feature.
 *
 * @ndiv:		ndiv of IDLE voltage/freq register
 * @vindex:		vindex of IDLE voltage/freq register
 *			(Not used on tegra19x)
 * @enable:		enable bit for Auto-CC3
 *
 * Returns 0 if success.
 */
int tegra_mce_cc3_ctrl(u32 ndiv, u32 vindex, u8 enable)
{
	if (mce_ops && mce_ops->cc3_ctrl)
		return mce_ops->cc3_ctrl(ndiv, vindex, enable);
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_cc3_ctrl);

/**
 * Send data to MCE which echoes it back.
 *
 * @data: data to be sent to MCE.
 * @out: output data to hold the response.
 * @matched: pointer to matching result
 *
 * Returns 0 if success.
 */
int tegra_mce_echo_data(u64 data, u64 *matched)
{
	if (mce_ops && mce_ops->echo_data)
		return mce_ops->echo_data(data, matched);
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_echo_data);

/**
 * Read out MCE API major/minor versions
 *
 * @major: output for major number.
 * @minor: output for minor number.
 *
 * Returns 0 if success.
 */
int tegra_mce_read_versions(u32 *major, u32 *minor)
{
	if (mce_ops && mce_ops->read_versions)
		return mce_ops->read_versions(major, minor);
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_read_versions);

/**
 * Read out RT Safe Mask
 *
 * @rt_safe_mask: output for rt safe mask.
 *
 * Returns 0 if success.
 */
int tegra_mce_read_rt_safe_mask(u64 *rt_safe_mask)
{
	if (mce_ops && mce_ops->read_rt_safe_mask)
		return mce_ops->read_rt_safe_mask(rt_safe_mask);
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_read_rt_safe_mask);

/**
 * Write RT Safe Mask
 *
 * @rt_safe_mask: rt safe mask value to be written
 *
 * Returns 0 if success.
 */
int tegra_mce_write_rt_safe_mask(u64 rt_safe_mask)
{
	if (mce_ops && mce_ops->write_rt_safe_mask)
		return mce_ops->write_rt_safe_mask(rt_safe_mask);
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_write_rt_safe_mask);

/**
 * Read out RT Window US
 *
 * @rt_window_us: output for rt window us
 *
 * Returns 0 if success.
 */
int tegra_mce_read_rt_window_us(u64 *rt_window_us)
{
	if (mce_ops && mce_ops->read_rt_window_us)
		return mce_ops->read_rt_window_us(rt_window_us);
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_read_rt_window_us);


/**
 * Write RT Window US
 *
 * @rt_window_us: rt window us value to be written
 *
 * Returns 0 if success.
 */
int tegra_mce_write_rt_window_us(u64 rt_window_us)
{
	if (mce_ops && mce_ops->write_rt_window_us)
		return mce_ops->write_rt_window_us(rt_window_us);
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_write_rt_window_us);


/**
 * Read out RT Fwd Progress US
 *
 * @rt_fwd_progress_us: output for rt fwd progress us
 *
 * Returns 0 if success.
 */
int tegra_mce_read_rt_fwd_progress_us(u64 *rt_fwd_progress_us)
{
	if (mce_ops && mce_ops->read_rt_fwd_progress_us)
		return mce_ops->read_rt_fwd_progress_us(rt_fwd_progress_us);
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_read_rt_fwd_progress_us);


/**
 * Write RT Fwd Progress US
 *
 * @rt_fwd_progress_us: rt fwd progress us value to be written
 *
 * Returns 0 if success.
 */
int tegra_mce_write_rt_fwd_progress_us(u64 rt_fwd_progress_us)
{
	if (mce_ops && mce_ops->write_rt_fwd_progress_us)
		return mce_ops->write_rt_fwd_progress_us(rt_fwd_progress_us);
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_write_rt_fwd_progress_us);


/**
 * Enumerate MCE API features
 *
 * @features: output feature vector (4bits each)
 *
 * Returns 0 if success.
 */
int tegra_mce_enum_features(u64 *features)
{
	if (mce_ops && mce_ops->enum_features)
		return mce_ops->enum_features(features);
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_enum_features);

/**
 * Read uncore MCA errors.
 *
 * @cmd: MCA command
 * @data: output data for the command
 * @error: error from MCA
 *
 * Returns 0 if success.
 */
int tegra_mce_read_uncore_mca(mca_cmd_t cmd, u64 *data, u32 *error)
{
	if (mce_ops && mce_ops->read_uncore_mca)
		return mce_ops->read_uncore_mca(cmd, data, error);
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_read_uncore_mca);

/**
 * Write uncore MCA errors.
 *
 * @cmd: MCA command
 * @data: input data for the command
 * @error: error from MCA
 *
 * Returns 0 if success.
 */
int tegra_mce_write_uncore_mca(mca_cmd_t cmd, u64 data, u32 *error)
{
	if (mce_ops && mce_ops->write_uncore_mca)
		return mce_ops->write_uncore_mca(cmd, data, error);
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_write_uncore_mca);

/**
 * Query PMU for uncore perfmon counter
 *
 * @req input command and counter index
 * @data output counter value
 *
 * Returns status of read request.
 */
int tegra_mce_read_uncore_perfmon(u32 req, u32 *data)
{
	if (mce_ops && mce_ops->read_uncore_perfmon)
		return mce_ops->read_uncore_perfmon(req, data);
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_read_uncore_perfmon);

/**
 * Write PMU reg for uncore perfmon counter
 *
 * @req input command and counter index
 * @data data to be written
 *
 * Returns status of write request.
 */
int tegra_mce_write_uncore_perfmon(u32 req, u32 data)
{
	if (mce_ops && mce_ops->write_uncore_perfmon)
		return mce_ops->write_uncore_perfmon(req, data);
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_write_uncore_perfmon);

int tegra_mce_enable_latic(void)
{
	if (mce_ops && mce_ops->enable_latic)
		return mce_ops->enable_latic();
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_enable_latic);

/**
 * Write to NVG DDA registers
 *
 * @index:   NVG communication channel id
 * @value:   Register value to be written
 *
 * Returns 0 on success
 */
int tegra_mce_write_dda_ctrl(u32 index, u64 value)
{
	if(mce_ops && mce_ops->write_dda_ctrl)
		return mce_ops->write_dda_ctrl(index, value);
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_write_dda_ctrl);

/**
 * Read NVG DDA registers
 *
 * @index:   NVG communication channel id
 * @value:   Associated register value read
 *
 * Returns 0 on success
 */
int tegra_mce_read_dda_ctrl(u32 index, u64 *value)
{
	if(mce_ops && mce_ops->read_dda_ctrl)
		return mce_ops->read_dda_ctrl(index, value);
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_read_dda_ctrl);

/**
 * Read NVG L3 cache control register
 *
 * @value:   Fill L3 cache ways
 *
 * Returns 0 on success
 */
int tegra_mce_read_l3_cache_ways(u64 *value)
{
	if (mce_ops && mce_ops->read_l3_cache_ways)
		return mce_ops->read_l3_cache_ways(value);
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_read_l3_cache_ways);

/**
 * Write L3 cache ways and read back the l3 cache ways written
 *
 * @data:   L3 cache ways to be writtein
 * @value:  L3 cache ways returrned back
 *
 * Returns 0 on success
 */
int tegra_mce_write_l3_cache_ways(u64 data, u64 *value)
{
	if (mce_ops && mce_ops->write_l3_cache_ways)
		return mce_ops->write_l3_cache_ways(data, value);
	else
		return -ENOTSUPP;
}
EXPORT_SYMBOL_GPL(tegra_mce_write_l3_cache_ways);

