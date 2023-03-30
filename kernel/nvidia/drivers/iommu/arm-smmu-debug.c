/*
 * Copyright (C) 2020-2022 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
#include "arm/arm-smmu/arm-smmu.h"
#else
#include "arm-smmu.h"
#endif
#include <linux/arm-smmu-debug.h>

#define defreg(_name)				\
	{					\
		.name = __stringify(_name),	\
		.offset = ARM_SMMU_ ## _name,	\
	}
#define defreg_gr0(_name) defreg(GR0_ ## _name)

static const struct debugfs_reg32 arm_smmu_gr0_regs[] = {
	defreg_gr0(sCR0),
	defreg_gr0(ID0),
	defreg_gr0(ID1),
	defreg_gr0(ID2),
	defreg_gr0(sGFSR),
	defreg_gr0(sGFSYNR0),
	defreg_gr0(sGFSYNR1),
	defreg_gr0(sTLBGSTATUS),
	defreg_gr0(nsCR0),
	defreg_gr0(nsGFSR),
	defreg_gr0(nsGFSYNR0),
	defreg_gr0(nsGFSYNR1),
	defreg_gr0(nsTLBGSTATUS),
	defreg_gr0(PIDR2),
};

#define defreg_gnsr0(_name) defreg(GNSR0_ ## _name)

static const struct debugfs_reg32 arm_smmu_gnsr0_regs[] = {
	defreg_gnsr0(PMCNTENSET_0),
	defreg_gnsr0(PMCNTENCLR_0),
	defreg_gnsr0(PMINTENSET_0),
	defreg_gnsr0(PMINTENCLR_0),
	defreg_gnsr0(PMOVSCLR_0),
	defreg_gnsr0(PMOVSSET_0),
	defreg_gnsr0(PMCFGR_0),
	defreg_gnsr0(PMCR_0),
	defreg_gnsr0(PMCEID0_0),
	defreg_gnsr0(PMAUTHSTATUS_0),
	defreg_gnsr0(PMDEVTYPE_0)
};

#define defreg_cb(_name)			\
	{					\
		.name = __stringify(_name),	\
		.offset = ARM_SMMU_CB_ ## _name,\
	}

static const struct debugfs_reg32 arm_smmu_cb_regs[] = {
	defreg_cb(SCTLR),
	defreg_cb(S1_MAIR0),
	defreg_cb(FSR),
	defreg_cb(FAR_LO),
	defreg_cb(FAR_HI),
	defreg_cb(FSYNR0),
};

static ssize_t smmu_context_filter_write(struct file *file,
					 const char __user *user_buf,
					 size_t count, loff_t *ppos)
{
	s16 cbndx;
	char *pbuf, *temp, *val;
	bool first_times = 1;
	ssize_t ret = count;
	struct seq_file *seqf = file->private_data;
	struct smmu_debugfs_info *smmu_dfs = seqf->private;
	unsigned long *bitmap = smmu_dfs->context_filter;

	/* Clear bitmap in case of user buf empty */
	if (count == 1 && *user_buf == '\n') {
		bitmap_zero(bitmap, smmu_dfs->num_context_banks);
		return ret;
	}

	pbuf = vmalloc(count + 1);
	if (!pbuf)
		return -ENOMEM;

	if (copy_from_user(pbuf, user_buf, count)) {
		ret = -EFAULT;
		goto end;
	}

	if (pbuf[count - 1] == '\n')
		pbuf[count - 1] = '\0';
	else
		pbuf[count] = '\0';

	temp = pbuf;

	do {
		val = strsep(&temp, ",");
		if (*val) {
			if (kstrtos16(val, 10, &cbndx))
				continue;

			/* Reset bitmap in case of negative index */
			if (cbndx < 0) {
				bitmap_fill(bitmap,
					    smmu_dfs->num_context_banks);
				goto end;
			}

			if (cbndx >= smmu_dfs->num_context_banks) {
				dev_err(smmu_dfs->dev,
					"context filter index out of range\n");
				ret = -EINVAL;
				goto end;
			}

			if (first_times) {
				bitmap_zero(bitmap,
					    smmu_dfs->num_context_banks);
				first_times = 0;
			}

			set_bit(cbndx, bitmap);
		}
	} while (temp);

end:
	vfree(pbuf);
	return ret;
}

static int smmu_context_filter_show(struct seq_file *s, void *unused)
{
	struct smmu_debugfs_info *smmu_dfs = s->private;
	unsigned long *bitmap = smmu_dfs->context_filter;
	int idx = 0;

	while (1) {
		idx = find_next_bit(bitmap, smmu_dfs->max_cbs, idx);
		if (idx >= smmu_dfs->num_context_banks)
			break;
		seq_printf(s, "%d,", idx);
		idx++;
	}
	seq_putc(s, '\n');
	return 0;
}

static int smmu_context_filter_open(struct inode *inode, struct file *file)
{
	return single_open(file, smmu_context_filter_show, inode->i_private);
}

static const struct file_operations smmu_context_filter_fops = {
	.open		= smmu_context_filter_open,
	.read		= seq_read,
	.write		= smmu_context_filter_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void debugfs_create_smmu_cb(struct smmu_debugfs_info *smmu_dfs, u8 cbndx)
{
	struct dentry *dent;
	char name[] = "cb000";
	struct debugfs_regset32 *cb;
	int err;

	err = sprintf(name, "cb%03d", cbndx);
	if (err < 0) {
		pr_err("%s: %d: sprintf failed to write\n", __func__, __LINE__);
		return;
	}
	dent = debugfs_create_dir(name, smmu_dfs->debugfs_root);
	if (!dent)
		return;

	cb = smmu_dfs->regset + 1 + cbndx;
	cb->regs = arm_smmu_cb_regs;
	cb->nregs = ARRAY_SIZE(arm_smmu_cb_regs);
	cb->base = smmu_dfs->bases[0] + (smmu_dfs->size >> 1) +
		cbndx * (1 << smmu_dfs->pgshift);
	debugfs_create_regset32("regdump", S_IRUGO, dent, cb);
}

static int smmu_reg32_debugfs_set(void *data, u64 val)
{
	struct smmu_debugfs_info *smmu_dfs = (struct smmu_debugfs_info *)data;

	if (smmu_dfs == NULL) {
		pr_err("SMMU debugfs setup not complete\n");
		return -EINVAL;
	}
	writel(val, (smmu_dfs->bases[smmu_dfs->debug_smmu_id] +
			smmu_dfs->reg->offset));
	return 0;
}

static int smmu_reg32_debugfs_get(void *data, u64 *val)
{
	struct smmu_debugfs_info *smmu_dfs = (struct smmu_debugfs_info *)data;

	if (smmu_dfs == NULL) {
		pr_err("SMMU debugfs setup not complete\n");
		return -EINVAL;
	}
	*val = readl(smmu_dfs->bases[smmu_dfs->debug_smmu_id] +
			smmu_dfs->reg->offset);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(smmu_reg32_debugfs_fops,
			smmu_reg32_debugfs_get,
			smmu_reg32_debugfs_set, "%08llx\n");

static int smmu_perf_regset_debugfs_set(void *data, u64 val)
{
	struct smmu_debugfs_info *smmu_dfs = (struct smmu_debugfs_info *)data;

	if (smmu_dfs == NULL) {
		pr_err("SMMU debugfs setup not complete\n");
		return -EINVAL;
	}
	writel(val, (smmu_dfs->perf_regset->base + smmu_dfs->reg->offset));
	return 0;
}

static int smmu_perf_regset_debugfs_get(void *data, u64 *val)
{
	struct smmu_debugfs_info *smmu_dfs = (struct smmu_debugfs_info *)data;

	if (smmu_dfs == NULL) {
		pr_err("SMMU debugfs setup not complete\n");
		return -EINVAL;
	}
	*val = readl(smmu_dfs->perf_regset->base + smmu_dfs->reg->offset);
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(smmu_perf_regset_debugfs_fops,
			smmu_perf_regset_debugfs_get,
			smmu_perf_regset_debugfs_set, "%08llx\n");

static void arm_smmu_regs_debugfs_delete(struct smmu_debugfs_info *smmu_dfs)
{
	int i;

	if (smmu_dfs->regset) {
		const struct debugfs_reg32 *regs = smmu_dfs->regset->regs;

		regs += ARRAY_SIZE(arm_smmu_gr0_regs);
		for (i = 0; i < 4 * smmu_dfs->num_context_banks; i++)
			kfree(regs[i].name);

		kfree(smmu_dfs->regset);
		smmu_dfs->regset = NULL;
	}

	if (smmu_dfs->perf_regset) {
		const struct debugfs_reg32 *regs = smmu_dfs->perf_regset->regs;

		i = ARRAY_SIZE(arm_smmu_gnsr0_regs);
		for (; i < smmu_dfs->perf_regset->nregs ; i++)
			kfree(regs[i].name);

		kfree(smmu_dfs->perf_regset);
		smmu_dfs->perf_regset = NULL;
	}

	debugfs_remove_recursive(smmu_dfs->debugfs_root);
}

static int debug_smmu_id_debugfs_set(void *data, u64 val)
{
	struct smmu_debugfs_info *smmu_dfs = (struct smmu_debugfs_info *)data;

	if (val >= smmu_dfs->num_smmus)
		return -EINVAL;

	smmu_dfs->debug_smmu_id = (u8)val;
	smmu_dfs->regset->base = smmu_dfs->bases[smmu_dfs->debug_smmu_id];
	smmu_dfs->perf_regset->base = smmu_dfs->regset->base +
					3 * (1 << smmu_dfs->pgshift);
	return 0;
}

static int debug_smmu_id_debugfs_get(void *data, u64 *val)
{
	struct smmu_debugfs_info *smmu_dfs = (struct smmu_debugfs_info *)data;

	*val = smmu_dfs->debug_smmu_id;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_smmu_id_debugfs_fops,
			debug_smmu_id_debugfs_get,
			debug_smmu_id_debugfs_set, "%08llx\n");

static int num_smmus_debugfs_set(void *data, u64 val)
{
	return 0;
}

static int num_smmus_debugfs_get(void *data, u64 *val)
{
	struct smmu_debugfs_info *smmu_dfs = (struct smmu_debugfs_info *)data;
	*val = smmu_dfs->num_smmus;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(num_smmus_debugfs_fops,
			num_smmus_debugfs_get,
			num_smmus_debugfs_set, "%08llx\n");

static int arm_smmu_regs_debugfs_create(struct smmu_debugfs_info *smmu_dfs)
{
	int i;
	struct debugfs_reg32 *regs;
	size_t bytes;
	struct dentry *dent_gr, *dent_gnsr;

	if (!smmu_dfs->debugfs_root)
		return -1;

	debugfs_create_file("debug_smmu_id", S_IRUGO | S_IWUSR,
			smmu_dfs->debugfs_root, smmu_dfs,
			&debug_smmu_id_debugfs_fops);

	debugfs_create_file("num_smmus", S_IRUSR, smmu_dfs->debugfs_root,
					 smmu_dfs, &num_smmus_debugfs_fops);

	dent_gr = debugfs_create_dir("gr", smmu_dfs->debugfs_root);
	if (!dent_gr)
		goto err_out;

	dent_gnsr = debugfs_create_dir("gnsr", smmu_dfs->debugfs_root);
	if (!dent_gnsr)
		goto err_out;

	smmu_dfs->masters_root = debugfs_create_dir("masters",
						    smmu_dfs->debugfs_root);
	if (!smmu_dfs->masters_root)
		goto err_out;

	smmu_dfs->cb_root = debugfs_create_dir("context_banks",
						smmu_dfs->debugfs_root);
	if (!smmu_dfs->cb_root)
		goto err_out;

	bytes = (smmu_dfs->num_context_banks + 1) * sizeof(*smmu_dfs->regset);
	bytes += ARRAY_SIZE(arm_smmu_gr0_regs) * sizeof(*regs);
	bytes += 4 * smmu_dfs->num_context_banks * sizeof(*regs);
	smmu_dfs->regset = kzalloc(bytes, GFP_KERNEL);
	if (!smmu_dfs->regset)
		goto err_out;

	smmu_dfs->regset->base = smmu_dfs->bases[0];
	smmu_dfs->regset->nregs = ARRAY_SIZE(arm_smmu_gr0_regs) +
		4 * smmu_dfs->num_context_banks;
	smmu_dfs->regset->regs = (struct debugfs_reg32 *)(smmu_dfs->regset +
					smmu_dfs->num_context_banks + 1);
	regs = (struct debugfs_reg32 *)smmu_dfs->regset->regs;
	for (i = 0; i < ARRAY_SIZE(arm_smmu_gr0_regs); i++) {
		regs->name = arm_smmu_gr0_regs[i].name;
		regs->offset = arm_smmu_gr0_regs[i].offset;
		regs++;
	}

	for (i = 0; i < smmu_dfs->num_context_banks; i++) {
		regs->name = kasprintf(GFP_KERNEL, "GR0_SMR%03d", i);
		if (!regs->name)
			goto err_out;
		regs->offset = ARM_SMMU_GR0_SMR(i);
		regs++;

		regs->name = kasprintf(GFP_KERNEL, "GR0_S2CR%03d", i);
		if (!regs->name)
			goto err_out;
		regs->offset = ARM_SMMU_GR0_S2CR(i);
		regs++;

		regs->name = kasprintf(GFP_KERNEL, "GR1_CBAR%03d", i);
		if (!regs->name)
			goto err_out;
		regs->offset = (1 << smmu_dfs->pgshift) + ARM_SMMU_GR1_CBAR(i);
		regs++;

		regs->name = kasprintf(GFP_KERNEL, "GR1_CBA2R%03d", i);
		if (!regs->name)
			goto err_out;
		regs->offset = (1 << smmu_dfs->pgshift) + ARM_SMMU_GR1_CBA2R(i);
		regs++;
	}

	regs = (struct debugfs_reg32 *)smmu_dfs->regset->regs;
	for (i = 0; i < smmu_dfs->regset->nregs; i++) {
		smmu_dfs->reg = regs;
		debugfs_create_file(regs->name, S_IRUGO | S_IWUSR,
				dent_gr, smmu_dfs, &smmu_reg32_debugfs_fops);
		regs++;
	}

	debugfs_create_regset32("regdump", S_IRUGO, smmu_dfs->debugfs_root,
				smmu_dfs->regset);

	bytes = sizeof(*smmu_dfs->perf_regset);
	bytes += ARRAY_SIZE(arm_smmu_gnsr0_regs) * sizeof(*regs);
	/*
	 * Account the number of bytes for two sets of
	 * counter group registers
	 */
	bytes += 2 * PMCG_SIZE * sizeof(*regs);
	/*
	 * Account the number of bytes for two sets of
	 * event counter registers
	 */
	bytes += 2 * PMEV_SIZE * sizeof(*regs);

	/* Allocate memory for Perf Monitor registers */
	smmu_dfs->perf_regset =  kzalloc(bytes, GFP_KERNEL);
	if (!smmu_dfs->perf_regset)
		goto err_out;

	/*
	 * perf_regset base address is placed at offset (3 * smmu_pagesize)
	 * from smmu->base address
	 */
	smmu_dfs->perf_regset->base = smmu_dfs->bases[0] +
					3 * (1 << smmu_dfs->pgshift);
	smmu_dfs->perf_regset->nregs = ARRAY_SIZE(arm_smmu_gnsr0_regs) +
		2 * PMCG_SIZE + 2 * PMEV_SIZE;
	smmu_dfs->perf_regset->regs =
		(struct debugfs_reg32 *)(smmu_dfs->perf_regset + 1);

	regs = (struct debugfs_reg32 *)smmu_dfs->perf_regset->regs;

	for (i = 0; i < ARRAY_SIZE(arm_smmu_gnsr0_regs); i++) {
		regs->name = arm_smmu_gnsr0_regs[i].name;
		regs->offset = arm_smmu_gnsr0_regs[i].offset;
		regs++;
	}

	for (i = 0; i < PMEV_SIZE; i++) {
		regs->name = kasprintf(GFP_KERNEL, "GNSR0_PMEVTYPER%d_0", i);
		if (!regs->name)
			goto err_out;
		regs->offset = ARM_SMMU_GNSR0_PMEVTYPER(i);
		regs++;

		regs->name = kasprintf(GFP_KERNEL, "GNSR0_PMEVCNTR%d_0", i);
		if (!regs->name)
			goto err_out;
		regs->offset = ARM_SMMU_GNSR0_PMEVCNTR(i);
		regs++;
	}

	for (i = 0; i < PMCG_SIZE; i++) {
		regs->name = kasprintf(GFP_KERNEL, "GNSR0_PMCGCR%d_0", i);
		if (!regs->name)
			goto err_out;
		regs->offset = ARM_SMMU_GNSR0_PMCGCR(i);
		regs++;

		regs->name = kasprintf(GFP_KERNEL, "GNSR0_PMCGSMR%d_0", i);
		if (!regs->name)
			goto err_out;
		regs->offset = ARM_SMMU_GNSR0_PMCGSMR(i);
		regs++;
	}

	regs = (struct debugfs_reg32 *)smmu_dfs->perf_regset->regs;
	for (i = 0; i < smmu_dfs->perf_regset->nregs; i++) {
		smmu_dfs->reg = regs;
		debugfs_create_file(regs->name, S_IRUGO | S_IWUSR,
			dent_gnsr, smmu_dfs, &smmu_perf_regset_debugfs_fops);
		regs++;
	}

	for (i = 0; i < smmu_dfs->num_context_banks; i++)
		debugfs_create_smmu_cb(smmu_dfs, i);

	INIT_LIST_HEAD(&smmu_dfs->masters_list);

	return 0;

err_out:
	arm_smmu_regs_debugfs_delete(smmu_dfs);
	return -1;
}

static void arm_smmu_debugfs_create(struct smmu_debugfs_info *smmu_dfs)
{
	struct dentry *d;

	d = debugfs_create_dir(dev_name(smmu_dfs->dev), NULL);
	if (!d)
		return;

	smmu_dfs->debugfs_root = d;

	d = debugfs_create_file("context_filter", S_IRUGO | S_IWUSR,
			    d, smmu_dfs,
			    &smmu_context_filter_fops);
	if (!d) {
		pr_warn("Making context filter failed\n");
		return;
	}

	arm_smmu_regs_debugfs_create(smmu_dfs);
}

void arm_smmu_debugfs_setup_bases(struct arm_smmu_device *smmu, u32 num_smmus,
			    void __iomem *bases[])
{
	struct smmu_debugfs_info *smmu_dfs;
	int i;

	smmu_dfs = devm_kmalloc(smmu->dev, sizeof(struct smmu_debugfs_info),
			    GFP_KERNEL);
	if (smmu_dfs == NULL) {
		dev_err(smmu->dev, "Out of memoryn\n");
		return;
	}
	for (i = 1; i < num_smmus; i++)
		smmu_dfs->bases[i] = bases[i];
	smmu_dfs->num_smmus = num_smmus;

	smmu->debug_info = smmu_dfs;
}

void arm_smmu_debugfs_setup_cfg(struct arm_smmu_device *smmu)
{
	struct smmu_debugfs_info *smmu_dfs = smmu->debug_info;

	if (smmu_dfs == NULL) {
		smmu_dfs = devm_kmalloc(smmu->dev,
				sizeof(struct smmu_debugfs_info), GFP_KERNEL);
		if (smmu_dfs == NULL) {
			dev_err(smmu->dev, "Out of memoryn\n");
			return;
		}
		smmu_dfs->num_smmus = 1;
		smmu->debug_info = smmu_dfs;
	}
	smmu_dfs->base = smmu->base;
	smmu_dfs->bases[0] = smmu->base;
	smmu_dfs->dev = smmu->dev;
	smmu_dfs->size = smmu->numpage;
	smmu_dfs->num_context_banks = smmu->num_context_banks;
	smmu_dfs->pgshift = smmu->pgshift;
	smmu_dfs->max_cbs = ARM_SMMU_MAX_CBS;
	smmu_dfs->streamid_mask = smmu->streamid_mask;
	smmu_dfs->debug_smmu_id = 0;

	arm_smmu_debugfs_create(smmu_dfs);
}

static int smmu_master_show(struct seq_file *s, void *unused)
{
	int i;
	struct smmu_debugfs_master *master = s->private;
	struct iommu_fwspec *fwspec = dev_iommu_fwspec_get(master->dev);

	if (!fwspec)
		return -ENODEV;

	for (i = 0; i < fwspec->num_ids; i++) {
		seq_printf(s, "streamids: % 3d ",
				fwspec->ids[i] & master->streamid_mask);
	}
	seq_printf(s, "\n");
	for (i = 0; i < fwspec->num_ids; i++) {
		seq_printf(s, "smrs: % 3d ", master->smendx[i]);
	}
	seq_printf(s, "\n");

	return 0;
}

static int smmu_master_open(struct inode *inode, struct file *file)
{
	return single_open(file, smmu_master_show, inode->i_private);
}

static const struct file_operations smmu_master_fops = {
	.open           = smmu_master_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

void arm_smmu_debugfs_add_master(struct device *dev,
				 struct smmu_debugfs_info *smmu_dfs,
				 u8 *cbndx, u16 smendx[])
{
	struct smmu_debugfs_master *master;
	struct dentry *dent = NULL;
	char name[] = "cb000";
	char target[] = "../../cb000";
	int err;

	if (smmu_dfs == NULL) {
		pr_warn("Debugfs setup not complete\n");
		return;
	}

	dent = debugfs_lookup(dev_name(dev), smmu_dfs->masters_root);
	if (dent)
		return;

	dent = debugfs_create_dir(dev_name(dev), smmu_dfs->masters_root);
	if (!dent) {
		return;
	}

	master = kmalloc(sizeof(*master), GFP_KERNEL);
	if (master == NULL) {
		dev_err(dev, "Failed to allocate memory for master\n");
		return;
	}
	master->dev = dev;
	master->smendx = smendx;
	master->dent = dent;
	master->streamid_mask = smmu_dfs->streamid_mask;

	// TODO create the streamids file
	debugfs_create_file("streamids", 0444, dent, master,
							&smmu_master_fops);
	debugfs_create_u8("cbndx", 0444, dent, cbndx);
	err = sprintf(name, "cb%03d", *cbndx);
	if (err < 0) {
		pr_err("%s: %d: sprintf failed to write\n", __func__, __LINE__);
		debugfs_remove_recursive(master->dent);
		kfree(master);
		return;
	}
	err = sprintf(target, "../../cb%03d", *cbndx);
	if (err < 0) {
		pr_err("%s: %d: sprintf failed to write\n", __func__, __LINE__);
		debugfs_remove_recursive(master->dent);
		kfree(master);
		return;
	}
	debugfs_create_symlink(name, dent, target);

	list_add_tail(&master->node, &smmu_dfs->masters_list);
}

void arm_smmu_debugfs_remove_master(struct device *dev,
				    struct smmu_debugfs_info *smmu_dfs)
{
	struct smmu_debugfs_master *master = NULL;

	if (smmu_dfs == NULL) {
		pr_warn("%s: Debugfs setup not complete\n", __func__);
		return;
	}

	list_for_each_entry(master, &smmu_dfs->masters_list, node) {
		if (master->dev == dev)
			break;
	}

	if (master != NULL) {
		debugfs_remove_recursive(master->dent);
		list_del(&master->node);
		kfree(master);
	}
}
