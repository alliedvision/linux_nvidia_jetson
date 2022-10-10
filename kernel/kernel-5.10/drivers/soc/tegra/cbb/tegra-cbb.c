// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 */

#include <asm/cpufeature.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/version.h>
#include <soc/tegra/fuse.h>
#include <soc/tegra/tegra-cbb.h>

void tegra_cbb_print_err(struct seq_file *file, const char *fmt, ...)
{
	va_list args;
	struct va_format vaf;

	va_start(args, fmt);

	if (file) {
		seq_vprintf(file, fmt, args);
	} else {
		vaf.fmt = fmt;
		vaf.va = &args;
		pr_crit("%pV", &vaf);
	}

	va_end(args);
}

void tegra_cbb_print_cache(struct seq_file *file, u32 cache)
{
	char *buff_str;
	char *mod_str;
	char *rd_str;
	char *wr_str;

	buff_str = (cache & BIT(0)) ? "Bufferable " : "";
	mod_str = (cache & BIT(1)) ? "Modifiable " : "";
	rd_str = (cache & BIT(2)) ? "Read-Allocate " : "";
	wr_str = (cache & BIT(3)) ? "Write-Allocate" : "";

	if (cache == 0x0)
		buff_str = "Device Non-Bufferable";

	tegra_cbb_print_err(file, "\t  Cache\t\t\t: 0x%x -- %s%s%s%s\n",
			    cache, buff_str, mod_str, rd_str, wr_str);
}

void tegra_cbb_print_prot(struct seq_file *file, u32 prot)
{
	char *data_str;
	char *secure_str;
	char *priv_str;

	data_str = (prot & 0x4) ? "Instruction" : "Data";
	secure_str = (prot & 0x2) ? "Non-Secure" : "Secure";
	priv_str = (prot & 0x1) ? "Privileged" : "Unprivileged";

	tegra_cbb_print_err(file, "\t  Protection\t\t: 0x%x -- %s, %s, %s Access\n",
			    prot, priv_str, secure_str, data_str);
}

#ifdef CONFIG_DEBUG_FS
static int created_root;

static int cbb_err_show(struct seq_file *file, void *data)
{
	struct tegra_cbb *cbb = file->private;

	return cbb->ops->cbb_err_debugfs_show(cbb, file, data);
}

static int cbb_err_open(struct inode *inode, struct file *file)
{
	return single_open(file, cbb_err_show, inode->i_private);
}

static const struct file_operations cbb_err_fops = {
	.open = cbb_err_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static int tegra_cbb_err_dbgfs_init(struct tegra_cbb *cbb)
{
	struct dentry *d;

	if (!created_root) {
		d = debugfs_create_file("tegra_cbb_err", 0444, NULL, cbb,
					&cbb_err_fops);
		if (IS_ERR_OR_NULL(d)) {
			pr_err
			("%s: could not create 'tegra_cbb_err' node\n",
			 __func__);
			return PTR_ERR(d);
		}
		created_root = true;
	}
	return 0;
}

#else
static int tegra_cbb_err_dbgfs_init(struct tegra_cbb *cbb) { return 0; }
#endif

void tegra_cbb_stallen(struct tegra_cbb *cbb)
{
	if (cbb->ops->stallen)
		cbb->ops->stallen(cbb);
}

void tegra_cbb_faulten(struct tegra_cbb *cbb)
{
	if (cbb->ops->faulten)
		cbb->ops->faulten(cbb);
}

void tegra_cbb_errclr(struct tegra_cbb *cbb)
{
	if (cbb->ops->errclr)
		cbb->ops->errclr(cbb);
}

u32 tegra_cbb_errvld(struct tegra_cbb *cbb)
{
	if (cbb->ops->errvld)
		return cbb->ops->errvld(cbb);
	else
		return 0;
}

int tegra_cbb_err_getirq(struct platform_device *pdev, int *nonsec_irq, int *sec_irq)
{
	int num_intr = 0;
	int intr_indx = 0;

	num_intr = platform_irq_count(pdev);
	if (!num_intr)
		return -EINVAL;

	if (num_intr == 2) {
		*nonsec_irq = platform_get_irq(pdev, intr_indx);
		if (*nonsec_irq <= 0) {
			dev_err(&pdev->dev, "can't get irq (%d)\n", *nonsec_irq);
			return -ENOENT;
		}
		intr_indx++;
	}

	*sec_irq = platform_get_irq(pdev, intr_indx);
	if (*sec_irq <= 0) {
		dev_err(&pdev->dev, "can't get irq (%d)\n", *sec_irq);
		return -ENOENT;
	}

	if (num_intr == 1)
		dev_info(&pdev->dev, "secure_irq = %d\n", *sec_irq);
	if (num_intr == 2)
		dev_info(&pdev->dev, "secure_irq = %d, nonsecure_irq = %d>\n",
			 *sec_irq, *nonsec_irq);
	return 0;
}

int tegra_cbb_register_isr_enaberr(struct tegra_cbb *cbb)
{
	struct platform_device *pdev = cbb->pdev;
	int ret = 0;

	ret = tegra_cbb_err_dbgfs_init(cbb);
	if (ret) {
		dev_err(&pdev->dev, "failed to create debugfs\n");
		return ret;
	}

	/* register interrupt handler for errors due to different initiators */
	ret = cbb->ops->cbb_intr_enable(cbb);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register CBB Interrupt ISR");
		return ret;
	}

	cbb->ops->cbb_err_enable(cbb);
	dsb(sy);

	return 0;
}
