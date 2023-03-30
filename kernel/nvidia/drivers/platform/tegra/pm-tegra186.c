/*
 * Copyright (c) 2016-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/pm.h>
#include <linux/tegra-pm.h>
#include <linux/version.h>

#ifdef CONFIG_TEGRA_PM_DEBUG
static u32 shutdown_state;
#endif

#define SMC_PM_FUNC	0xC2FFFE00
#define SMC_SET_SHUTDOWN_MODE 0x1
#define SYSTEM_SHUTDOWN_STATE_FULL_POWER_OFF 0
#define SYSTEM_SHUTDOWN_STATE_SC8 8
#define SMC_GET_CLK_COUNT 0x2

/*
 * Helper function for send_smc that actually makes the smc call
 */
static noinline notrace int __send_smc(u32 smc_func, struct pm_regs *regs)
{
	u32 ret = smc_func;

	asm volatile (
	"       mov     x0, %0\n"
	"       ldp     x1, x2, [%1, #16 * 0]\n"
	"       ldp     x3, x4, [%1, #16 * 1]\n"
	"       ldp     x5, x6, [%1, #16 * 2]\n"
	"       isb\n"
	"       smc     #0\n"
	"       mov     %0, x0\n"
	"       stp     x0, x1, [%1, #16 * 0]\n"
	"       stp     x2, x3, [%1, #16 * 1]\n"
	: "+r" (ret)
	: "r" (regs)
	: "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8",
	"x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17");
	return ret;
}

/*
 * Make an SMC call. Takes in the SMC function to be invoked & registers to be
 * passed along as args.
 */
int send_smc(u32 smc_func, struct pm_regs *regs)
{
	int __ret = __send_smc(smc_func, regs);

	if (__ret) {
		pr_err("%s: failed (ret=%d)\n", __func__, __ret);
		return __ret;
	}

	return __ret;
}

/**
 * Specify state for SYSTEM_SHUTDOWN
 *
 * @shutdown_state:	Specific shutdown state to set
 *
 */
int tegra_set_shutdown_mode(u32 shutdown_state)
{
	struct pm_regs regs;
	u32 smc_func = SMC_PM_FUNC | (SMC_SET_SHUTDOWN_MODE & SMC_ENUM_MAX);
	regs.args[0] = shutdown_state;
	return send_smc(smc_func, &regs);
}
EXPORT_SYMBOL(tegra_set_shutdown_mode);

/**
 * read core clk and ref clk counters under EL3
 *
 * @mpidr: MPIDR of target core
 * @midr: MIDR of target core
 * @coreclk: core clk counter
 * @refclk : ref clk counter
 *
 * Returns 0 if success.
 */
int tegra_get_clk_counter(u32 mpidr, u32 midr, u32 *coreclk,
	u32 *refclk)
{
	struct pm_regs regs;
	int ret;
	u32 smc_func = SMC_PM_FUNC | (SMC_GET_CLK_COUNT & SMC_ENUM_MAX);

	regs.args[0] = mpidr;
	regs.args[1] = midr;

	ret = send_smc(smc_func, &regs);

	*coreclk = (u32)regs.args[1];
	*refclk = (u32)regs.args[2];

	return ret;
}
EXPORT_SYMBOL(tegra_get_clk_counter);

static void tegra186_power_off_prepare(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 2, 0)
	disable_nonboot_cpus();
#else
	suspend_disable_secondary_cpus();
#endif
}

static int __init tegra186_pm_init(void)
{
	pm_power_off_prepare = tegra186_power_off_prepare;

	return 0;
}
core_initcall(tegra186_pm_init);

#ifdef CONFIG_TEGRA_PM_DEBUG
static int shutdown_state_get(void *data, u64 *val)
{
	*val = shutdown_state;
	return 0;
}

static int shutdown_state_set(void *data, u64 val)
{
	int ret;
	if (val == SYSTEM_SHUTDOWN_STATE_FULL_POWER_OFF ||
					val == SYSTEM_SHUTDOWN_STATE_SC8) {
		shutdown_state = val;
		ret = tegra_set_shutdown_mode(shutdown_state);
	}
	else {
		printk("Invalid Shutdown state\n");
		ret = -1;
	}

	return ret;
}

DEFINE_SIMPLE_ATTRIBUTE(shutdown_state_fops, shutdown_state_get, shutdown_state_set, "%llu\n");

static int __init tegra18_suspend_debugfs_init(void)
{
	struct dentry *dfs_file, *system_state_debugfs;

	system_state_debugfs = return_system_states_dir();
	if (!system_state_debugfs)
		goto err_out;

	dfs_file = debugfs_create_file("shutdown", 0644,
					system_state_debugfs, NULL, &shutdown_state_fops);
	if (!dfs_file)
		goto err_out;

	return 0;

err_out:
	pr_err("%s: Couldn't create debugfs node for shutdown\n", __func__);
	return -ENOMEM;

}
module_init(tegra18_suspend_debugfs_init);
#endif

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Tegra T18x Suspend Mode debugfs");
