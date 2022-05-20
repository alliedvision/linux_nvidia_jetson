// SPDX-License-Identifier: GPL-2.0+
/*
 * cache-maintenance functions for gem buffer object
 *
 * Copyright (c), 2020 Nvidia Corporation
 */

#include <linux/of_device.h>
#include <drm/tegra_drm.h>
#include <asm/cacheflush.h>
#include <asm/delay.h>
#include <linux/dma-direction.h>
#include <linux/vmalloc.h>
#include <linux/io.h>
#include <linux/module.h>
#include "gem.h"
#include "drm.h"

#define NR_SMC_REGS		6
#define SMC_SIP_INVOKE_MCE	0xC2FFFF00

enum cache_op {
	TEGRA_CACHE_OP_WB = 0,
	TEGRA_CACHE_OP_INV,
	TEGRA_CACHE_OP_WB_INV,
};

struct cache_maint {
	phys_addr_t start;
	phys_addr_t end;
	enum cache_op op;
};

struct cache_maint_ops {
	int (*flush_dcache_all)(void);
	int (*clean_dcache_all)(void);
};

struct cache_maint_soc_data {
	struct cache_maint_ops *ops;
};

const struct cache_maint_soc_data *soc_data = NULL;

struct tegra_mce_regs {
	u64 args[NR_SMC_REGS];
};

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

#define send_smc(func, regs)							\
	({									\
		int __ret = __send_smc(func, regs);				\
										\
		if (__ret)							\
			pr_err("%s: failed (ret=%d)\n", __func__, __ret);	\
		__ret;								\
	})

static int hw_cache_maint(struct cache_maint *c_maint)
{
	if (!soc_data)
		return -ENOTSUPP;

	if ((c_maint->op == TEGRA_CACHE_OP_WB_INV) &&
		(soc_data->ops->flush_dcache_all))
		return soc_data->ops->flush_dcache_all();
	else if ((c_maint->op == TEGRA_CACHE_OP_WB) &&
		(soc_data->ops->clean_dcache_all))
		return soc_data->ops->clean_dcache_all();
	else
		return -ENOTSUPP;

	return 0;
}

static int do_cache_maint(struct cache_maint *c_maint)
{
	return hw_cache_maint(c_maint);
}

int tegra_gem_cache_maint(struct drm_gem_object *gem,
			struct drm_tegra_gem_cache_ops *ca_obj)
{
	struct cache_maint c_op;
	struct vm_area_struct *vma;
	ulong start, end;
	int err;

	if (!ca_obj->addr || ca_obj->op < TEGRA_CACHE_OP_WB ||
		ca_obj->op > TEGRA_CACHE_OP_WB_INV)
		return -EINVAL;

	vma = find_vma(current->active_mm, (unsigned long)ca_obj->addr);
	if (!vma || (ulong)ca_obj->addr < vma->vm_start ||
	    (ulong)ca_obj->addr >= vma->vm_end ||
	    ca_obj->len > vma->vm_end - (ulong)ca_obj->addr) {
		err = -EADDRNOTAVAIL;
		goto err_out;
	}

	start = (unsigned long)ca_obj->addr - vma->vm_start +
		(vma->vm_pgoff << PAGE_SHIFT);
	end = start + ca_obj->len;

	c_op.start = start ? start : 0;
	c_op.end = end ? end : ca_obj->len;
	c_op.op = ca_obj->op;

	err = do_cache_maint(&c_op);
	if (err)
		goto  err_out;

	return 0;
err_out:
	return err;
}
EXPORT_SYMBOL(tegra_gem_cache_maint);

__always_inline int tegra18x_roc_flush_cache_only(void)
{
	struct tegra_mce_regs regs;

	return send_smc(MCE_SMC_ROC_FLUSH_CACHE_ONLY, &regs);
}


__always_inline int tegra18x_roc_clean_cache(void)
{
	struct tegra_mce_regs regs;

	return send_smc(MCE_SMC_ROC_CLEAN_CACHE_ONLY, &regs);
}

int tegra186_flush_dcache_all(void)
{
	return tegra18x_roc_flush_cache_only();
}

int tegra186_clean_dcache_all(void)
{
	return tegra18x_roc_clean_cache();
}

static struct cache_maint_ops ca_maint_ops_tegra186 = {
	.flush_dcache_all = tegra186_flush_dcache_all,
	.clean_dcache_all = tegra186_clean_dcache_all,
};

static struct cache_maint_soc_data ca_maint_tegra186 = {
	.ops = &ca_maint_ops_tegra186,
};

#define MASK GENMASK(15, 12)
int tegra194_flush_dcache_all(void)
{
	u64 id_afr0;
	u64 ret;
	u64 retry = 10;

	asm volatile ("mrs %0, ID_AFR0_EL1" : "=r"(id_afr0));
	/* check if dcache flush through mts is supported */
	if (!likely(id_afr0 & MASK)) {
		pr_warn("SCF dcache flush is not supported in MTS\n");
		return -ENOTSUPP;
	}

	do {
		asm volatile ("mrs %0, s3_0_c15_c3_6" : "=r" (ret));
		udelay(1);
	} while (!ret && retry--);
	asm volatile ("dsb sy");

	if (!ret) {
		WARN_ONCE(!ret, "%s failed\n", __func__);
		pr_err("SCF dcache flush: instruction error\n");
		return -EINVAL;
	}

	return 0;
}

int tegra194_clean_dcache_all(void)
{
	u64 id_afr0;
	u64 ret;
	u64 retry = 10;

	asm volatile ("mrs %0, ID_AFR0_EL1" : "=r"(id_afr0));
	/* check if dcache clean through mts is supported */
	if (!likely(id_afr0 & MASK)) {
		pr_err("SCF dcache clean is not supported in MTS\n");
		return -ENOTSUPP;
	}

	do {
		asm volatile ("mrs %0, s3_0_c15_c3_5" : "=r" (ret));
	} while (!ret && retry--);
	asm volatile ("dsb sy");

	if (!ret) {
		WARN_ONCE(!ret, "%s failed\n", __func__);
		pr_err("SCF dcache clean: instruction error\n");
		return -EINVAL;
	}

	return 0;
}

static struct cache_maint_ops ca_maint_ops_tegra194  = {
	.flush_dcache_all = tegra194_flush_dcache_all,
	.clean_dcache_all = tegra194_clean_dcache_all,
};

static struct cache_maint_soc_data ca_maint_tegra194 = {
	.ops = &ca_maint_ops_tegra194,
};
static const struct of_device_id tegra_cache_maint_of_match[] = {
	{ .compatible = "nvidia,t18x-cache", .data = &ca_maint_tegra186},
	{ .compatible = "nvidia,t19x-cache", .data = &ca_maint_tegra194},
	{ }
};
MODULE_DEVICE_TABLE(of, tegra_cache_maint_of_match);

static int tegra_ca_maint_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;

	match = of_match_device(tegra_cache_maint_of_match, &pdev->dev);
	if (!match)
		return -EINVAL;

	soc_data = match->data;

	return 0;

}
static int tegra_ca_maint_remove(struct platform_device *pdev)
{
	return 0;
}

struct platform_driver tegra_cache_maint_driver = {
	.driver = {
		.name = "tegra_cache_maint",
		.of_match_table = tegra_cache_maint_of_match,
	},
	.probe = tegra_ca_maint_probe,
	.remove = tegra_ca_maint_remove,
};
