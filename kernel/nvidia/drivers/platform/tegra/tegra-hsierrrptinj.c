/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/**
 * @file tegra-hsierrrptinj.c
 * @brief <b> HSI Error Report Injection driver</b>
 *
 * This file will register as client driver to support triggering 
 * HSI error reporting from CCPLEX to FSI.
 */

/* ==================[Includes]============================================= */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/debugfs.h>
#include <linux/pm.h>
#include "linux/tegra-hsierrrptinj.h"


/*Format of input buffer*/
/* IP ID, Instance ID, Error Code, Reporter ID, Error Attribute*/
/*"0x0000 0x0000 0x0000 0x0000 0x00000000\n"*/
static const size_t hsierrrptinj_err_rpt_len = 39;

/* This directory entry will point to `/sys/kernel/debug/tegra_hsierrrptinj`. */
static struct dentry *hsierrrptinj_debugfs_root = 0;

/* This file will point to `/sys/kernel/debug/tegra_hsierrrptinj/hsierrrpt`. */
const char *hsierrrptinj_debugfs_name = "hsierrrpt";

static ssize_t hsierrrptinj_inject(struct file *file, const char *buf, size_t lbuf, loff_t *ppos)
{
    pr_info("tegra-hsierrrptinj: inject entry\n");
    if(lbuf != hsierrrptinj_err_rpt_len) {
        pr_err("tegra-hsierrrptinj: Invalid input.\n");
        return -EINVAL;
    }

    /** Extract data and trigger*/
    pr_info("tegra-hsierrrptinj: print input\n");
    pr_info("%.38s\n", buf);
    return 0;
}

static const struct file_operations hsierrrptinj_fops = {
        .read = NULL,
        .write = hsierrrptinj_inject,
};


static int __maybe_unused hsierrrptinj_suspend(struct device *dev)
{
    pr_debug("tegra-hsierrrptinj: suspend called\n");
    return 0;
}

static int __maybe_unused hsierrrptinj_resume(struct device *dev)
{
    pr_debug("tegra-hsierrrptinj: resume called\n");
    return 0;
}
static SIMPLE_DEV_PM_OPS(hsierrrptinj_pm, hsierrrptinj_suspend, hsierrrptinj_resume);

static const struct of_device_id hsierrrptinj_dt_match[] = {
    { .compatible = "nvidia,tegra234-epl-client"},
    {}
};

MODULE_DEVICE_TABLE(of, hsierrrptinj_dt_match);

static int hsierrrptinj_probe(struct platform_device *pdev)
{
    struct dentry *dent = 0;
    pr_info("tegra-hsierrrptinj: probe entry\n");
    /* Create a directory 'tegra_hsierrrptinj' under 'sys/kernel/debug'
     * to hold the set of debug files*/
    pr_info("tegra-hsierrrptinj: debugfs_create_dir\n");
    hsierrrptinj_debugfs_root = debugfs_create_dir("tegra_hsierrrptinj", NULL);
    if (IS_ERR_OR_NULL(hsierrrptinj_debugfs_root))
        return -EFAULT;

    /* Create a debug file 'hsierrrpt' under 'sys/kernel/debug/tegra_hsierrrptinj' */
    pr_info("tegra-hsierrrptinj: debugfs_create_file\n");
    dent = debugfs_create_file(hsierrrptinj_debugfs_name, S_IWUSR, hsierrrptinj_debugfs_root, NULL, &hsierrrptinj_fops);
    if (IS_ERR_OR_NULL(dent))
        goto abort;

    pr_info("tegra-hsierrrptinj: probe success");
    return 0;

abort:
    /* We must manually remove the debugfs entries we created. They are not
     * automatically removed upon module removal.
     */
    debugfs_remove_recursive(hsierrrptinj_debugfs_root);
    return -EFAULT;
}

static int hsierrrptinj_remove(struct platform_device *pdev)
{
    /* We must manually remove the debugfs entries we created. They are not
     * automatically removed upon module removal.
     */
    debugfs_remove_recursive(hsierrrptinj_debugfs_root);

    return 0;
}

static struct platform_driver hsierrrptinj = {
    .driver         = {
    .name   = "hsierrrptinj",
        .probe_type = PROBE_FORCE_SYNCHRONOUS,
        .of_match_table = of_match_ptr(hsierrrptinj_dt_match),
        .pm = pm_ptr(&hsierrrptinj_pm),
    },
    .probe          = hsierrrptinj_probe,
    .remove         = hsierrrptinj_remove,
};


module_platform_driver(hsierrrptinj);

MODULE_DESCRIPTION("tegra: HSI Error Report Injection driver");
MODULE_AUTHOR("Prasun Kumar <prasunk@nvidia.com>");
MODULE_LICENSE("GPL v2");