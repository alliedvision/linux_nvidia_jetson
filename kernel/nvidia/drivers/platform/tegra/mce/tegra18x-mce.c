/*
 * Copyright (c) 2017-2020, NVIDIA CORPORATION. All rights reserved.
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
#include <linux/t18x_ari.h>
#include <linux/tegra-mce.h>

#include <asm/smp_plat.h>
#include <linux/version.h>
#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif

#define SMC_SIP_INVOKE_MCE	0xC2FFFF00

#define NR_SMC_REGS		6

/* MCE command enums for SMC calls */
enum {
	MCE_SMC_ENTER_CSTATE = 0,
	MCE_SMC_UPDATE_CSTATE_INFO = 1,
	MCE_SMC_UPDATE_XOVER_TIME = 2,
	MCE_SMC_READ_CSTATE_STATS = 3,
	MCE_SMC_WRITE_CSTATE_STATS = 4,
	MCE_SMC_IS_SC7_ALLOWED = 5,
	MCE_SMC_ONLINE_CORE = 6,
	MCE_SMC_CC3_CTRL = 7,
	MCE_SMC_ECHO_DATA = 8,
	MCE_SMC_READ_VERSIONS = 9,
	MCE_SMC_ENUM_FEATURES = 10,
	MCE_SMC_ROC_FLUSH_CACHE = 11,
	MCE_SMC_ENUM_READ_MCA = 12,
	MCE_SMC_ENUM_WRITE_MCA = 13,
	MCE_SMC_ROC_FLUSH_CACHE_ONLY = 14,
	MCE_SMC_ROC_CLEAN_CACHE_ONLY = 15,
	MCE_SMC_ENABLE_LATIC = 16,
	MCE_SMC_UNCORE_PERFMON_REQ = 17,
	MCE_SMC_MISC_CCPLEX = 18,
	MCE_SMC_ENUM_MAX = 0xFF,	/* enums cannot exceed this value */
};

struct tegra_mce_regs {
	u64 args[NR_SMC_REGS];
};

static noinline notrace int __send_smc(u8 func, struct tegra_mce_regs *regs)
{
	u32 ret = SMC_SIP_INVOKE_MCE | (func & MCE_SMC_ENUM_MAX);

	asm volatile (
	"	mov	x0, %0\n"
	"	ldp	x1, x2, [%1, #16 * 0]\n"
	"	ldp	x3, x4, [%1, #16 * 1]\n"
	"	ldp	x5, x6, [%1, #16 * 2]\n"
	"	isb\n"
	"	smc	#0\n"
	"	mov	%0, x0\n"
	"	stp	x0, x1, [%1, #16 * 0]\n"
	"	stp	x2, x3, [%1, #16 * 1]\n"
	: "+r" (ret)
	: "r" (regs)
	: "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8",
	"x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17");

	return ret;
}

#define send_smc(func, regs)						\
({									\
	int __ret = __send_smc(func, regs);				\
									\
	if (__ret)							\
		pr_err("%s: failed (ret=%d)\n", __func__, __ret);	\
	__ret;								\
})

static int tegra18x_mce_enter_cstate(u32 state, u32 wake_time)
{
	struct tegra_mce_regs regs;

	regs.args[0] = state;
	regs.args[1] = wake_time;

	return send_smc(MCE_SMC_ENTER_CSTATE, &regs);
}

static int tegra18x_mce_update_cstate_info(u32 cluster, u32 ccplex, u32 system,
				    u8 force, u32 wake_mask, bool valid)
{
	struct tegra_mce_regs regs;

	regs.args[0] = cluster;
	regs.args[1] = ccplex;
	regs.args[2] = system;
	regs.args[3] = force;
	regs.args[4] = wake_mask;
	regs.args[5] = valid;

	return send_smc(MCE_SMC_UPDATE_CSTATE_INFO, &regs);
}

static int tegra18x_mce_update_crossover_time(u32 type, u32 time)
{
	struct tegra_mce_regs regs;

	regs.args[0] = type;
	regs.args[1] = time;

	return send_smc(MCE_SMC_UPDATE_XOVER_TIME, &regs);
}

static int tegra18x_mce_read_cstate_stats(u32 state, u64 *stats)
{
	struct tegra_mce_regs regs;

	regs.args[0] = state;
	send_smc(MCE_SMC_READ_CSTATE_STATS, &regs);
	*stats = regs.args[2];

	return 0;
}

static int tegra18x_mce_write_cstate_stats(u32 state, u32 stats)
{
	struct tegra_mce_regs regs;

	regs.args[0] = state;
	regs.args[1] = stats;

	return send_smc(MCE_SMC_WRITE_CSTATE_STATS, &regs);
}

static int tegra18x_mce_is_sc7_allowed(u32 state, u32 wake, u32 *allowed)
{
	struct tegra_mce_regs regs;

	regs.args[0] = state;
	regs.args[1] = wake;
	send_smc(MCE_SMC_IS_SC7_ALLOWED, &regs);
	*allowed = (u32)regs.args[3];

	return 0;
}

static int tegra18x_mce_online_core(int cpu)
{
	struct tegra_mce_regs regs;

	regs.args[0] = cpu_logical_map(cpu);

	return send_smc(MCE_SMC_ONLINE_CORE, &regs);
}

static int tegra18x_mce_cc3_ctrl(u32 ndiv, u32 vindex, u8 enable)
{
	struct tegra_mce_regs regs;

	regs.args[0] = ndiv;
	regs.args[1] = vindex;
	regs.args[2] = enable;

	return send_smc(MCE_SMC_CC3_CTRL, &regs);
}

static int tegra18x_mce_echo_data(u64 data, u64 *matched)
{
	struct tegra_mce_regs regs;

	regs.args[0] = (u32)(data & 0xFFFFFFFF);
	send_smc(MCE_SMC_ECHO_DATA, &regs);
	*matched = regs.args[2];

	return 0;
}

static int tegra18x_mce_read_versions(u32 *major, u32 *minor)
{
	struct tegra_mce_regs regs;

	send_smc(MCE_SMC_READ_VERSIONS, &regs);
	*major = (u32)regs.args[1];
	*minor = (u32)regs.args[2];

	return 0;
}

static int tegra18x_mce_enum_features(u64 *features)
{
	struct tegra_mce_regs regs;

	send_smc(MCE_SMC_ENUM_FEATURES, &regs);
	*features = (u32)regs.args[1];

	return 0;
}

static int tegra18x_mce_read_uncore_mca(mca_cmd_t cmd, u64 *data, u32 *error)
{
	struct tegra_mce_regs regs;

	regs.args[0] = cmd.data;
	regs.args[1] = 0;
	send_smc(MCE_SMC_ENUM_READ_MCA, &regs);
	*data = regs.args[2];
	*error = (u32)regs.args[3];

	return 0;
}

static int tegra18x_mce_write_uncore_mca(mca_cmd_t cmd, u64 data, u32 *error)
{
	struct tegra_mce_regs regs;

	regs.args[0] = cmd.data;
	regs.args[1] = data;
	send_smc(MCE_SMC_ENUM_WRITE_MCA, &regs);
	*error = (u32)regs.args[3];

	return 0;
}

static int tegra18x_mce_read_uncore_perfmon(u32 req, u32 *data)
{
	struct tegra_mce_regs regs;
	u32 status;

	if (data == NULL)
		return -EINVAL;

	regs.args[0] = req;
	status = send_smc(MCE_SMC_UNCORE_PERFMON_REQ, &regs);
	*data = (u32)regs.args[1];

	return status;
}

static int tegra18x_mce_write_uncore_perfmon(u32 req, u32 data)
{
	struct tegra_mce_regs regs;
	u32 status = 0;

	regs.args[0] = req;
	regs.args[1] = data;
	status = send_smc(MCE_SMC_UNCORE_PERFMON_REQ, &regs);

	return status;
}

static int tegra18x_mce_enable_latic(void)
{
	struct tegra_mce_regs regs;

	return send_smc(MCE_SMC_ENABLE_LATIC, &regs);
}

#ifdef CONFIG_DEBUG_FS
static int tegra18x_mce_features_get(void *data, u64 *val)
{
	return tegra_mce_enum_features(val);
}

static int tegra18x_mce_enable_latic_set(void *data, u64 val)
{
	if (tegra_mce_enable_latic())
		return -EINVAL;
	return 0;
}

/* Enable/disable coresight clock gating */
static int tegra18x_mce_coresight_cg_set(void *data, u64 val)
{
	struct tegra_mce_regs regs;

	/* Enable - 1, disable - 0 are the only valid values */
	if (val > 1) {
		pr_err("mce: invalid enable value.\n");
		return -EINVAL;
	}

	regs.args[0] = TEGRA_ARI_MISC_CCPLEX_CORESIGHT_CG_CTRL;
	regs.args[1] = (u32)val;
	send_smc(MCE_SMC_MISC_CCPLEX, &regs);

	return 0;
}

/* Enable external debug on MCA */
static int tegra18x_mce_edbgreq_set(void *data, u64 val)
{
	struct tegra_mce_regs regs;

	regs.args[0] = TEGRA_ARI_MISC_CCPLEX_EDBGREQ;
	send_smc(MCE_SMC_MISC_CCPLEX, &regs);

	return 0;
}

#define CSTAT_ENTRY(stat)[TEGRA_ARI_CSTATE_STATS_##stat] = #stat

static const char * const cstats_table[] = {
	CSTAT_ENTRY(SC7_ENTRIES),
	CSTAT_ENTRY(A57_CC6_ENTRIES),
	CSTAT_ENTRY(A57_CC7_ENTRIES),
	CSTAT_ENTRY(D15_CC6_ENTRIES),
	CSTAT_ENTRY(D15_CC7_ENTRIES),
	CSTAT_ENTRY(D15_0_C6_ENTRIES),
	CSTAT_ENTRY(D15_1_C6_ENTRIES),
	CSTAT_ENTRY(D15_0_C7_ENTRIES),
	CSTAT_ENTRY(D15_1_C7_ENTRIES),
	CSTAT_ENTRY(A57_0_C7_ENTRIES),
	CSTAT_ENTRY(A57_1_C7_ENTRIES),
	CSTAT_ENTRY(A57_2_C7_ENTRIES),
	CSTAT_ENTRY(A57_3_C7_ENTRIES),
	CSTAT_ENTRY(LAST_CSTATE_ENTRY_D15_0),
	CSTAT_ENTRY(LAST_CSTATE_ENTRY_D15_1),
	CSTAT_ENTRY(LAST_CSTATE_ENTRY_A57_0),
	CSTAT_ENTRY(LAST_CSTATE_ENTRY_A57_1),
	CSTAT_ENTRY(LAST_CSTATE_ENTRY_A57_2),
	CSTAT_ENTRY(LAST_CSTATE_ENTRY_A57_3),
};

static int tegra18x_mce_dbg_cstats_show(struct seq_file *s, void *data)
{
	int st;
	u64 val;

	seq_printf(s, "%-30s%-10s\n", "name", "count");
	seq_puts(s, "----------------------------------------\n");
	for (st = 1; st <= TEGRA_ARI_CSTATE_STATS_MAX; st++) {
		if (!cstats_table[st])
			continue;
		if (tegra18x_mce_read_cstate_stats(st, &val))
			pr_err("mce: failed to read cstat: %d\n", st);
		else
			seq_printf(s, "%-30s%-10lld\n", cstats_table[st], val);
	}

	return 0;
}

static struct dentry *mce_debugfs;

static int tegra18x_mce_echo_set(void *data, u64 val)
{
	u64 matched;
	int ret;

	ret = tegra_mce_echo_data(val, &matched);
	if (ret && ret != -ENOTSUPP)
		return -EINVAL;
	return 0;
}

static int tegra18x_mce_versions_get(void *data, u64 *val)
{
	u32 major, minor;
	int ret;

	ret = tegra_mce_read_versions(&major, &minor);
	if (!ret)
		*val = ((u64)major << 32) | minor;
	return ret;
}

static int tegra18x_mce_dbg_cstats_open(struct inode *inode, struct file *file)
{
	int (*f)(struct seq_file *, void *);

	f = tegra18x_mce_dbg_cstats_show;
	return single_open(file, f, inode->i_private);
}

static const struct file_operations tegra18x_mce_cstats_fops = {
	.open = tegra18x_mce_dbg_cstats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

DEFINE_SIMPLE_ATTRIBUTE(tegra18x_mce_echo_fops, NULL,
			tegra18x_mce_echo_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(tegra18x_mce_versions_fops, tegra18x_mce_versions_get,
			NULL, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(tegra18x_mce_features_fops, tegra18x_mce_features_get,
			NULL, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(tegra18x_mce_enable_latic_fops, NULL,
			tegra18x_mce_enable_latic_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(tegra18x_mce_coresight_cg_fops, NULL,
			tegra18x_mce_coresight_cg_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(tegra18x_mce_edbgreq_fops, NULL,
			tegra18x_mce_edbgreq_set, "%llu\n");

struct debugfs_entry {
	const char *name;
	const struct file_operations *fops;
	mode_t mode;
};

/* Make sure to put an NULL entry at the end of each group */
static struct debugfs_entry tegra18x_mce_attrs[] = {
	{ "echo", &tegra18x_mce_echo_fops, 0200 },
	{ "versions", &tegra18x_mce_versions_fops, 0444 },
	{ "features", &tegra18x_mce_features_fops, 0444 },
	{ "cstats", &tegra18x_mce_cstats_fops, 0444 },
	{ "enable-latic", &tegra18x_mce_enable_latic_fops, 0200 },
	{ "coresight_cg_enable", &tegra18x_mce_coresight_cg_fops, 0200 },
	{ "edbgreq", &tegra18x_mce_edbgreq_fops, 0200 },
	{ NULL, NULL, 0 },
};

static struct debugfs_entry *tegra_mce_attrs = tegra18x_mce_attrs;

static __init int tegra18x_mce_init(void)
{
	struct debugfs_entry *fent;
	struct dentry *dent;
	int ret;

	if (tegra_get_chip_id() != TEGRA186)
		return 0;

	mce_debugfs = debugfs_create_dir("tegra_mce", NULL);
	if (!mce_debugfs)
		return -ENOMEM;

	for (fent = tegra_mce_attrs; fent->name; fent++) {
		dent = debugfs_create_file(fent->name, fent->mode,
					   mce_debugfs, NULL, fent->fops);
		if (IS_ERR_OR_NULL(dent)) {
			ret = dent ? PTR_ERR(dent) : -EINVAL;
			pr_err("%s: failed to create debugfs (%s): %d\n",
			       __func__, fent->name, ret);
			goto err;
		}
	}

	pr_debug("%s: init finished\n", __func__);

	return 0;

err:
	debugfs_remove_recursive(mce_debugfs);

	return ret;
}

static void __exit tegra18x_mce_exit(void)
{
	if (tegra_get_chip_id() == TEGRA186)
		debugfs_remove_recursive(mce_debugfs);
}
module_init(tegra18x_mce_init);
module_exit(tegra18x_mce_exit);
#endif /* CONFIG_DEBUG_FS */

static struct tegra_mce_ops t18x_mce_ops = {
	.enter_cstate = tegra18x_mce_enter_cstate,
	.update_cstate_info = tegra18x_mce_update_cstate_info,
	.update_crossover_time = tegra18x_mce_update_crossover_time,
	.read_cstate_stats = tegra18x_mce_read_cstate_stats,
	.write_cstate_stats = tegra18x_mce_write_cstate_stats,
	.is_sc7_allowed = tegra18x_mce_is_sc7_allowed,
	.online_core = tegra18x_mce_online_core,
	.cc3_ctrl = tegra18x_mce_cc3_ctrl,
	.echo_data = tegra18x_mce_echo_data,
	.read_versions = tegra18x_mce_read_versions,
	.enum_features = tegra18x_mce_enum_features,
	.read_uncore_mca = tegra18x_mce_read_uncore_mca,
	.write_uncore_mca = tegra18x_mce_write_uncore_mca,
	.read_uncore_perfmon = tegra18x_mce_read_uncore_perfmon,
	.write_uncore_perfmon = tegra18x_mce_write_uncore_perfmon,
	.enable_latic = tegra18x_mce_enable_latic,
};

static int __init tegra18x_mce_early_init(void)
{
	if (tegra_get_chip_id() == TEGRA186)
		tegra_mce_set_ops(&t18x_mce_ops);

	return 0;
}
early_initcall(tegra18x_mce_early_init);

MODULE_DESCRIPTION("NVIDIA Tegra18X MCE driver");
MODULE_AUTHOR("NVIDIA Corporation");
MODULE_LICENSE("GPL v2");
