/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/module.h>
#include <linux/tegra-cache.h>
#include <linux/version.h>
#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif

/* MCE defines for SMC calls */
#define NR_SMC_REGS			6
#define SMC_SIP_INVOKE_MCE		0xC2FFFF00
#define MCE_SMC_ROC_FLUSH_CACHE		11
#define	MCE_SMC_ROC_FLUSH_CACHE_ONLY 	14
#define MCE_SMC_ROC_CLEAN_CACHE_ONLY	15
#define MCE_SMC_ENUM_MAX 0xFF	/* command cannot exceed this value */

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

static int tegra18x_roc_flush_cache(void)
{
	struct tegra_mce_regs regs;

	return send_smc(MCE_SMC_ROC_FLUSH_CACHE, &regs);
}

static int tegra18x_roc_flush_cache_only(void *__maybe_unused unused)
{
	struct tegra_mce_regs regs;

	return send_smc(MCE_SMC_ROC_FLUSH_CACHE_ONLY, &regs);
}

static int tegra18x_roc_clean_cache(void *__maybe_unused unused)
{
	struct tegra_mce_regs regs;

	return send_smc(MCE_SMC_ROC_CLEAN_CACHE_ONLY, &regs);
}

static struct tegra_cache_ops t18x_cache_ops = {
	.flush_cache_all = tegra18x_roc_flush_cache,
	.flush_dcache_all = tegra18x_roc_flush_cache_only,
	.clean_dcache_all = tegra18x_roc_clean_cache,
};

static int __init tegra18x_cache_early_init(void)
{
	if (tegra_get_chip_id() == TEGRA186) {
		tegra_cache_set_ops(&t18x_cache_ops);
	}

	return 0;
}
pure_initcall(tegra18x_cache_early_init);

MODULE_DESCRIPTION("T18x Cache operations registration");
MODULE_AUTHOR("Sandipan Patra <spatra@nvidia.com>");
MODULE_LICENSE("GPL v2");
