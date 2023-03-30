/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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
 * The driver handles Error's from Control Backbone(CBB) generated due to
 * illegal accesses. When an error is reported from a NOC within CBB,
 * the driver checks ErrVld status of all three Error Logger's of that NOC.
 * It then prints debug information about failed transaction using ErrLog
 * registers of error logger which has ErrVld set. Currently, SLV, DEC,
 * TMO, SEC, UNS are the only codes which are supported by CBB.
 */

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
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
#include <asm/traps.h>
#include <soc/tegra/chip-id.h>
#else
#include <asm/cpufeature.h>
#include <soc/tegra/fuse.h>
#endif
#include <linux/platform/tegra/tegra_cbb.h>
#include <linux/platform/tegra/tegra23x_cbb.h>
#include <linux/platform/tegra/tegra239_cbb.h>
#include <linux/platform/tegra/tegra23x_cbb_reg.h>

#define get_mstr_id(userbits) get_em_el_subfield(errmon->user_bits, 29, 24)

#define DONOT_CLR_TIMEDOUT_SLAVE_BIT
#define MAX_TMO_CLR_RETRY 2
#define FABRIC_NAME_LEN 10

static LIST_HEAD(cbb_errmon_list);
static DEFINE_SPINLOCK(cbb_errmon_lock);

static struct tegra23x_cbb_fabric_sn_map fabric_sn_map[MAX_FAB_ID];


u32 tegra234_cbb_readl(unsigned long offset)
{
	struct tegra_cbb_errmon_record *errmon;
	bool flag = 0;
	u32 val = 0;

	if (offset > 0x3FFFFF) {
		pr_err("%s: wrong offset value\n", __func__);
		return 0;
	}

	list_for_each_entry(errmon, &cbb_errmon_list, node) {
		if (strstr(errmon->name, "CBB")) {
			val = readl(errmon->vaddr + offset);
			flag = true;
			break;
		}
	}
	if (!flag)
		pr_err("%s: cbb fabric not initialized\n", __func__);

	return val;
}
EXPORT_SYMBOL(tegra234_cbb_readl);

void tegra234_cbb_writel(unsigned long offset, u32 val)
{
	struct tegra_cbb_errmon_record *errmon;
	bool flag = 0;

	if (offset > 0x3FFFFF) {
		pr_err("%s: wrong offset value\n", __func__);
		return;
	}

	list_for_each_entry(errmon, &cbb_errmon_list, node) {
		if (strstr(errmon->name, "CBB")) {
			writel(val, errmon->vaddr + offset);
			flag = true;
			break;
		}
	}
	if (!flag)
		pr_err("%s: cbb fabric not initialized\n", __func__);
}
EXPORT_SYMBOL(tegra234_cbb_writel);

static void tegra234_cbb_errmon_faulten(void __iomem *addr)
{
	writel(0x1FF, addr + FABRIC_EN_CFG_INTERRUPT_ENABLE_0_0);
	dsb(sy);
}

static void tegra234_cbb_errmon_errclr(void __iomem *addr)
{
	writel(0x3F, addr + FABRIC_MN_MASTER_ERR_STATUS_0);
	dsb(sy);
}

static unsigned int tegra234_cbb_errmon_errvld(void __iomem *addr)
{
	unsigned int errvld_status = 0;

	errvld_status = readl(addr + FABRIC_EN_CFG_STATUS_0_0);

	dsb(sy);
	return errvld_status;
}

static unsigned int tegra234_cbb_get_tmo_slv(void __iomem *addr)
{
	unsigned int timeout_status;

	timeout_status = readl(addr);
	return timeout_status;
}

#ifndef DONOT_CLR_TIMEDOUT_SLAVE_BIT
static void tegra234_cbb_reset_slv(void __iomem *addr, u32 val)
{
	writel(val, addr);
	dsb(sy);
}
#endif

static void tegra234_cbb_reset_tmo_slv(struct seq_file *file, char *slv_name,
				       void __iomem *addr, u32 tmo_status)
{
#ifdef DONOT_CLR_TIMEDOUT_SLAVE_BIT
	print_cbb_err(file, "\t  %s : 0x%x\n", slv_name, tmo_status);
#else
	int i = 0;

	while (tmo_status && (i < MAX_TMO_CLR_RETRY)) {
		print_cbb_err(file, "\t  %s : 0x%x\n", slv_name, tmo_status);
		print_cbb_err(file, "\t  Resetting timed-out client 0x%x\n",
			      tmo_status);
		tegra234_cbb_reset_slv((void __iomem *)addr, tmo_status);

		tmo_status = tegra234_cbb_get_tmo_slv((void __iomem *)addr);
		print_cbb_err(file, "\t  Readback %s: 0x%x\n", slv_name,
			      tmo_status);
		i++;
	}

	if (tmo_status && (i == MAX_TMO_CLR_RETRY)) {
		print_cbb_err(file, "\t  Timeout flag didn't reset twice.\n");
		BUG();
	}
#endif
}

static void tegra234_cbb_lookup_apbslv
	(struct seq_file *file, char *slave_name, u64 addr) {

	unsigned int blockno_tmo_status, tmo_status;
	unsigned int reset_client, client_id;
	char slv_name[40];
	int block_num = 0;
	int ret = 0;

	tmo_status = tegra234_cbb_get_tmo_slv((void __iomem *)addr);
	if (tmo_status)
		print_cbb_err(file, "\t  %s_BLOCK_TMO_STATUS : 0x%x\n",
			      slave_name, tmo_status);

	while (tmo_status) {
		if (tmo_status & BIT(0)) {
			addr =  addr + APB_BLOCK_NUM_TMO_OFFSET +
							(block_num * 4);
			blockno_tmo_status =
				tegra234_cbb_get_tmo_slv((void __iomem *)addr);
			reset_client = blockno_tmo_status;

			if (blockno_tmo_status) {
				client_id = 1;
				while (blockno_tmo_status) {
					if (blockno_tmo_status & 0x1) {
						if (reset_client != 0xffffffff)
							reset_client &= client_id;
						ret = sprintf(slv_name, "%s_BLOCK%d_TMO",
							      slave_name, block_num);
						if (ret < 0) {
							pr_err("%s: sprintf failed\n", __func__);
							return;
						}
						tegra234_cbb_reset_tmo_slv
						(file, slv_name, (void __iomem *)addr,
						 reset_client);
					}
					blockno_tmo_status >>= 1;
					client_id <<= 1;
				}
			}
			tmo_status >>= 1;
			block_num++;
		}
	}
}

static void tegra234_lookup_slave_timeout(struct seq_file *file, u8 slave_id,
					  u8 fab_id) {
	struct tegra_sn_addr_map *sn_lookup = fabric_sn_map[fab_id].sn_lookup;
	void __iomem *base_addr = fabric_sn_map[fab_id].fab_base_vaddr;
	unsigned int tmo_status;
	char slv_name[40];
	int i = slave_id;
	u64 addr = 0;

	/*
	 * 1) Get slave node name and address mapping using slave_id.
	 * 2) Check if the timed out slave node is APB or AXI.
	 * 3) If AXI, then print timeout register and reset axi slave
	 *    using <FABRIC>_SN_<>_SLV_TIMEOUT_STATUS_0_0 register.
	 * 4) If APB, then perform an additional lookup to find the client
	 *    which timed out.
	 *	a) Get block number from the index of set bit in
	 *	   <FABRIC>_SN_AXI2APB_<>_BLOCK_TMO_STATUS_0 register.
	 *	b) Get address of register repective to block number i.e.
	 *	   <FABRIC>_SN_AXI2APB_<>_BLOCK<index-set-bit>_TMO_0.
	 *	c) Read the register in above step to get client_id which
	 *	   timed out as per the set bits.
	 *      d) Reset the timedout client and print details.
	 *	e) Goto step-a till all bits are set.
	 */

	addr = (__force u64)base_addr + sn_lookup[i].off_slave;

	if (strstr(sn_lookup[i].slave_name, "AXI2APB")) {

		addr = addr + APB_BLOCK_TMO_STATUS_0;
		tegra234_cbb_lookup_apbslv(file, sn_lookup[i].slave_name, addr);
	} else {
		addr = addr + AXI_SLV_TIMEOUT_STATUS_0_0;

		tmo_status = tegra234_cbb_get_tmo_slv((void __iomem *)addr);
		if (tmo_status) {
			sprintf(slv_name, "%s_SLV_TIMEOUT_STATUS",
				sn_lookup[i].slave_name);
			tegra234_cbb_reset_tmo_slv
			(file, slv_name, (void __iomem *)addr, tmo_status);
		}
	}
}

static void print_errmon_err(struct seq_file *file,
		struct tegra_cbb_errmon_record *errmon,
		unsigned int errmon_err_status,
		unsigned int errmon_overflow_status, int max_errs)
{
	int err_type = 0;

	if (errmon_err_status & (errmon_err_status - 1))
		print_cbb_err(file, "\t  Multiple type of errors reported\n");

	while (errmon_err_status && (err_type < max_errs)) {
		if (errmon_err_status & 0x1)
			print_cbb_err(file, "\t  Error Code\t\t: %s\n",
				tegra234_errmon_errors[err_type].errcode);
		errmon_err_status >>= 1;
		err_type++;
	}

	err_type = 0;
	while (errmon_overflow_status && (err_type < max_errs)) {
		if (errmon_overflow_status & 0x1)
			print_cbb_err(file, "\t  Overflow\t\t: Multiple %s\n",
				tegra234_errmon_errors[err_type].errcode);
		errmon_overflow_status >>= 1;
		err_type++;
	}
}

static void print_errlog_err(struct seq_file *file,
		struct tegra_cbb_errmon_record *errmon)
{
	u8 cache_type = 0, prot_type = 0, burst_length = 0;
	u8 beat_size = 0, access_type = 0, access_id = 0;
	u8 mstr_id = 0, grpsec = 0, vqc = 0, falconsec = 0;
	u8 slave_id = 0, fab_id = 0, burst_type = 0;
	char fabric_name[FABRIC_NAME_LEN];

	cache_type = get_em_el_subfield(errmon->attr0, 27, 24);
	prot_type = get_em_el_subfield(errmon->attr0, 22, 20);
	burst_length = get_em_el_subfield(errmon->attr0, 19, 12);
	burst_type = get_em_el_subfield(errmon->attr0, 9, 8);
	beat_size = get_em_el_subfield(errmon->attr0, 6, 4);
	access_type = get_em_el_subfield(errmon->attr0, 0, 0);

	access_id = get_em_el_subfield(errmon->attr1, 7, 0);

	fab_id = get_em_el_subfield(errmon->attr2, 20, 16);
	slave_id = get_em_el_subfield(errmon->attr2, 7, 0);

	mstr_id = get_em_el_subfield(errmon->user_bits, 29, 24);
	vqc = get_em_el_subfield(errmon->user_bits, 17, 16);
	grpsec = get_em_el_subfield(errmon->user_bits, 14, 8);
	falconsec = get_em_el_subfield(errmon->user_bits, 1, 0);

	print_cbb_err(file, "\t  First logged Err Code : %s\n",
			tegra234_errmon_errors[errmon->err_type].errcode);

	print_cbb_err(file, "\t  MASTER_ID\t\t: %s\n",
					errmon->tegra_cbb_master_id[mstr_id]);
	print_cbb_err(file, "\t  Address\t\t: %px\n",
					errmon->addr_access);

	print_cache(file, cache_type);
	print_prot(file, prot_type);

	print_cbb_err(file, "\t  Access_Type\t\t: %s",
			(access_type) ? "Write\n" : "Read");

	if (fab_id == PSC_FAB_ID)
		strcpy(fabric_name, "PSC");
	else if (fab_id == FSI_FAB_ID)
		strcpy(fabric_name, "FSI");
	else
		strncpy(fabric_name, fabric_sn_map[fab_id].fab_name, FABRIC_NAME_LEN - 1);

	print_cbb_err(file, "\t  Fabric\t\t: %s\n", fabric_name);
	print_cbb_err(file, "\t  Slave_Id\t\t: 0x%x\n", slave_id);
	print_cbb_err(file, "\t  Burst_length\t\t: 0x%x\n", burst_length);
	print_cbb_err(file, "\t  Burst_type\t\t: 0x%x\n", burst_type);
	print_cbb_err(file, "\t  Beat_size\t\t: 0x%x\n", beat_size);
	print_cbb_err(file, "\t  VQC\t\t\t: 0x%x\n", vqc);
	print_cbb_err(file, "\t  GRPSEC\t\t: 0x%x\n", grpsec);
	print_cbb_err(file, "\t  FALCONSEC\t\t: 0x%x\n", falconsec);

	if ((fab_id == PSC_FAB_ID) || (fab_id == FSI_FAB_ID))
		return;

	if (!strcmp(tegra234_errmon_errors[errmon->err_type].errcode,
		    "TIMEOUT_ERR")) {
		tegra234_lookup_slave_timeout(file, slave_id, fab_id);
		return;
	}
	print_cbb_err(file, "\t  Slave\t\t\t: %s\n",
		      fabric_sn_map[fab_id].sn_lookup[slave_id].slave_name);
}

static int print_errmonX_info(
		struct seq_file *file,
		struct tegra_cbb_errmon_record *errmon)
{
	int max_errs = ARRAY_SIZE(tegra234_errmon_errors);
	unsigned int errmon_err_status = 0;
	unsigned int errlog_err_status = 0;
	unsigned int errmon_overflow_status = 0;
	u64 addr = 0;
	int ret = 0;

	errmon->err_type = 0;

	errmon_err_status = readl(errmon->addr_errmon+
					FABRIC_MN_MASTER_ERR_STATUS_0);
	if (!errmon_err_status) {
		pr_err("Error Notifier received a spurious notification\n");
		BUG();
	}

	/*get overflow flag*/
	errmon_overflow_status = readl(errmon->addr_errmon+
					FABRIC_MN_MASTER_ERR_OVERFLOW_STATUS_0);

	print_errmon_err(file, errmon, errmon_err_status,
			 errmon_overflow_status, max_errs);

	errlog_err_status = readl(errmon->addr_errmon+
					FABRIC_MN_MASTER_LOG_ERR_STATUS_0);
	if (!errlog_err_status) {
		pr_info("Error Monitor doesn't have Error Logger\n");
		return -EINVAL;
	}

	if ((errmon_err_status == 0xFFFFFFFF) ||
	    (errlog_err_status == 0xFFFFFFFF)) {
		pr_err("CBB registers returning all 1's which is invalid\n");
		return -EINVAL;
	}

	while (errlog_err_status && (errmon->err_type < max_errs)) {
		if (errlog_err_status & BIT(0)) {
			addr = readl(errmon->addr_errmon+
					FABRIC_MN_MASTER_LOG_ADDR_HIGH_0);
			addr = (addr<<32) | readl(errmon->addr_errmon+
					FABRIC_MN_MASTER_LOG_ADDR_LOW_0);
			errmon->addr_access = (void __iomem *)addr;

			errmon->attr0 = readl(errmon->addr_errmon+
					FABRIC_MN_MASTER_LOG_ATTRIBUTES0_0);
			errmon->attr1 = readl(errmon->addr_errmon+
					FABRIC_MN_MASTER_LOG_ATTRIBUTES1_0);
			errmon->attr2 = readl(errmon->addr_errmon+
					FABRIC_MN_MASTER_LOG_ATTRIBUTES2_0);
			errmon->user_bits = readl(errmon->addr_errmon+
					FABRIC_MN_MASTER_LOG_USER_BITS0_0);

			print_errlog_err(file, errmon);
		}
		errmon->err_type++;
		errlog_err_status >>= 1;
	}
	return ret;
}

static int print_err_notifier(struct seq_file *file,
		struct tegra_cbb_errmon_record *errmon,
		int err_notifier_status)
{
	u64 errmon_phys_addr = 0;
	u64 errmon_addr_offset = 0;
	int errmon_no = 1;
	int ret = 0;

	pr_crit("**************************************\n");
	pr_crit("* For more Internal Decode Help\n");
	pr_crit("*     http://nv/cbberr\n");
	pr_crit("* NVIDIA userID is required to access\n");
	pr_crit("**************************************\n");
	pr_crit("CPU:%d, Error:%s, Errmon:%d\n", smp_processor_id(),
					errmon->name, err_notifier_status);
	while (err_notifier_status) {
		if (err_notifier_status & BIT(0)) {
			writel(errmon_no, errmon->vaddr+
					errmon->err_notifier_base+
					FABRIC_EN_CFG_ADDR_INDEX_0_0);

			errmon_phys_addr = readl(errmon->vaddr+
						errmon->err_notifier_base+
						FABRIC_EN_CFG_ADDR_HI_0);
			errmon_phys_addr = (errmon_phys_addr<<32) |
						readl(errmon->vaddr+
						errmon->err_notifier_base+
						FABRIC_EN_CFG_ADDR_LOW_0);

			errmon_addr_offset = errmon_phys_addr - errmon->start;
			errmon->addr_errmon = (void __iomem *)(errmon->vaddr+
							errmon_addr_offset);
			errmon->errmon_no = errmon_no;

			ret = print_errmonX_info(file, errmon);
			tegra234_cbb_errmon_errclr(errmon->addr_errmon);
			if (ret)
				return ret;
		}
		err_notifier_status >>= 1;
		errmon_no <<= 1;
	}

	print_cbb_err(file, "\t**************************************\n");
	return ret;
}

#ifdef CONFIG_DEBUG_FS
static DEFINE_MUTEX(cbb_err_mutex);

static int tegra234_cbb_err_show(struct seq_file *file, void *data)
{
	struct tegra_cbb_errmon_record *errmon;
	unsigned int errvld_status = 0;
	int ret = 0;

	mutex_lock(&cbb_err_mutex);

	list_for_each_entry(errmon, &cbb_errmon_list, node) {
		if ((!errmon->is_clk_rst) ||
			(errmon->is_clk_rst && errmon->is_clk_enabled())) {

			errvld_status = tegra_cbb_errvld(errmon->vaddr+
						errmon->err_notifier_base);
			if (errvld_status) {
				ret = print_err_notifier(file, errmon,
						errvld_status);
			}
		}
	}

	mutex_unlock(&cbb_err_mutex);
	return ret;
}
#endif

/*
 * Handler for CBB errors
 */
static irqreturn_t tegra234_cbb_error_isr(int irq, void *dev_id)
{
	struct tegra_cbb_errmon_record *errmon;
	unsigned int errvld_status = 0;
	unsigned long flags;
	bool is_inband_err = 0;
	u8 mstr_id = 0;
	int ret = 0;

	spin_lock_irqsave(&cbb_errmon_lock, flags);

	list_for_each_entry(errmon, &cbb_errmon_list, node) {
		if ((!errmon->is_clk_rst) ||
			(errmon->is_clk_rst && errmon->is_clk_enabled())) {
			errvld_status = tegra_cbb_errvld(errmon->vaddr+
						errmon->err_notifier_base);

			if (errvld_status &&
				((irq == errmon->errmon_secure_irq) ||
				(irq == errmon->errmon_nonsecure_irq))) {
				print_cbb_err(NULL, "CPU:%d, Error:%s@0x%llx,"
				"irq=%d\n", smp_processor_id(), errmon->name,
				errmon->start, irq);

				ret = print_err_notifier(NULL, errmon, errvld_status);
				if (ret)
					goto en_isr_exit;

				mstr_id = get_mstr_id(errmon->user_bits);
				/* If illegal request is from CCPLEX(id:0x1)
				 * master then call BUG() to crash system.
				 */
				if ((mstr_id == 0x1) &&
						(errmon->erd_mask_inband_err))
					is_inband_err = 1;
			}
		}
	}
en_isr_exit:
	spin_unlock_irqrestore(&cbb_errmon_lock, flags);

	WARN_ON(is_inband_err);

	return IRQ_HANDLED;
}

/*
 * Register handler for CBB_NONSECURE & CBB_SECURE interrupts due to
 * CBB errors from masters other than CCPLEX
 */
static int tegra234_cbb_enable_interrupt(struct platform_device *pdev,
				int errmon_secure_irq, int errmon_nonsecure_irq)
{
	int err = 0;

	if (errmon_secure_irq) {
		if (request_irq(errmon_secure_irq, tegra234_cbb_error_isr, 0,
					dev_name(&pdev->dev), pdev)) {
			dev_err(&pdev->dev,
				"%s: Unable to register (%d) interrupt\n",
				__func__, errmon_secure_irq);
			goto isr_err_free_sec_irq;
		}
	}
	if (errmon_nonsecure_irq) {
		if (request_irq(errmon_nonsecure_irq, tegra234_cbb_error_isr, 0,
					dev_name(&pdev->dev), pdev)) {
			dev_err(&pdev->dev,
				"%s: Unable to register (%d) interrupt\n",
				__func__, errmon_nonsecure_irq);
			goto isr_err_free_nonsec_irq;
		}
	}
	return 0;

isr_err_free_nonsec_irq:
	if (errmon_nonsecure_irq)
		free_irq(errmon_nonsecure_irq, pdev);
isr_err_free_sec_irq:
	if (errmon_secure_irq)
		free_irq(errmon_secure_irq, pdev);

	return err;
}


static void tegra234_cbb_error_enable(void __iomem *vaddr)
{
	tegra_cbb_faulten(vaddr);
}


static int tegra234_cbb_remove(struct platform_device *pdev)
{
	return 0;
}

static struct tegra_cbberr_ops tegra234_cbb_errmon_ops = {
	.errvld	 = tegra234_cbb_errmon_errvld,
	.errclr	 = tegra234_cbb_errmon_errclr,
	.faulten = tegra234_cbb_errmon_faulten,
	.cbb_error_enable = tegra234_cbb_error_enable,
	.cbb_enable_interrupt = tegra234_cbb_enable_interrupt,
#ifdef CONFIG_DEBUG_FS
	.cbb_err_debugfs_show = tegra234_cbb_err_show
#endif
};

static int tegra234_cbb_mn_mask_erd(u64 mask_erd)
{
	writel(0x1, (void __iomem *)mask_erd);
	dsb(sy);
	return 0;
}

static struct tegra_cbb_noc_data tegra239_cbb_en_data = {
	.name   = "CBB-EN",
	.is_clk_rst = false,
	.erd_mask_inband_err = true,
	.off_mask_erd = 0x3d004,
	.tegra_cbb_noc_set_erd = tegra234_cbb_mn_mask_erd
};

static struct tegra_cbb_noc_data tegra239_ape_en_data = {
	.name   = "APE-EN",
	.is_clk_rst = false,
	.erd_mask_inband_err = false
};
static struct tegra_cbb_noc_data tegra234_aon_en_data = {
	.name   = "AON-EN",
	.is_clk_rst = false,
	.erd_mask_inband_err = false
};

static struct tegra_cbb_noc_data tegra234_bpmp_en_data = {
	.name   = "BPMP-EN",
	.is_clk_rst = false,
	.erd_mask_inband_err = false
};

static struct tegra_cbb_noc_data tegra234_cbb_en_data = {
	.name   = "CBB-EN",
	.is_clk_rst = false,
	.erd_mask_inband_err = true,
	.off_mask_erd = 0x3a004,
	.tegra_cbb_noc_set_erd = tegra234_cbb_mn_mask_erd
};

static struct tegra_cbb_noc_data tegra234_dce_en_data = {
	.name   = "DCE-EN",
	.is_clk_rst = false,
	.erd_mask_inband_err = false
};

static struct tegra_cbb_noc_data tegra234_rce_en_data = {
	.name   = "RCE-EN",
	.is_clk_rst = false,
	.erd_mask_inband_err = false
};

static struct tegra_cbb_noc_data tegra234_sce_en_data = {
	.name   = "SCE-EN",
	.is_clk_rst = false,
	.erd_mask_inband_err = false
};

static const struct of_device_id tegra234_cbb_match[] = {
	{.compatible    = "nvidia,tegra234-CBB-EN",
		.data = &tegra234_cbb_en_data},
	{.compatible    = "nvidia,tegra234-AON-EN",
		.data = &tegra234_aon_en_data},
	{.compatible    = "nvidia,tegra234-BPMP-EN",
		.data = &tegra234_bpmp_en_data},
	{.compatible    = "nvidia,tegra234-DCE-EN",
		.data = &tegra234_dce_en_data},
	{.compatible    = "nvidia,tegra234-RCE-EN",
		.data = &tegra234_rce_en_data},
	{.compatible    = "nvidia,tegra234-SCE-EN",
		.data = &tegra234_sce_en_data},
	{.compatible    = "nvidia,tegra239-CBB-EN",
		.data = &tegra239_cbb_en_data},
	{.compatible    = "nvidia,tegra239-APE-EN",
		.data = &tegra239_ape_en_data},
	{},
};
MODULE_DEVICE_TABLE(of, tegra234_cbb_match);

static int tegra234_cbb_errmon_set_data(struct tegra_cbb_errmon_record *errmon)
{
	if (strlen(errmon->name) > 0)
		errmon->tegra_cbb_master_id = t234_master_id;

	if (!strcmp(errmon->name, "CBB-EN")) {
		fabric_sn_map[CBB_FAB_ID].fab_name = "CBB";
		fabric_sn_map[CBB_FAB_ID].fab_base_vaddr = errmon->vaddr;
		if (of_machine_is_compatible("nvidia,tegra239"))
			fabric_sn_map[CBB_FAB_ID].sn_lookup = tegra239_cbb_sn_lookup;
		else
			fabric_sn_map[CBB_FAB_ID].sn_lookup = tegra23x_cbb_sn_lookup;
	} else if (!strcmp(errmon->name, "SCE-EN")) {
		fabric_sn_map[SCE_FAB_ID].fab_name = "SCE";
		fabric_sn_map[SCE_FAB_ID].fab_base_vaddr = errmon->vaddr;
		fabric_sn_map[SCE_FAB_ID].sn_lookup = tegra23x_sce_sn_lookup;
	} else if (!strcmp(errmon->name, "RCE-EN")) {
		fabric_sn_map[RCE_FAB_ID].fab_name = "RCE";
		fabric_sn_map[RCE_FAB_ID].fab_base_vaddr = errmon->vaddr;
		fabric_sn_map[RCE_FAB_ID].sn_lookup = tegra23x_rce_sn_lookup;
	} else if (!strcmp(errmon->name, "DCE-EN")) {
		fabric_sn_map[DCE_FAB_ID].fab_name = "DCE";
		fabric_sn_map[DCE_FAB_ID].fab_base_vaddr = errmon->vaddr;
		fabric_sn_map[DCE_FAB_ID].sn_lookup = tegra23x_dce_sn_lookup;
	} else if (!strcmp(errmon->name, "AON-EN")) {
		fabric_sn_map[AON_FAB_ID].fab_name = "AON";
		fabric_sn_map[AON_FAB_ID].fab_base_vaddr = errmon->vaddr;
		fabric_sn_map[AON_FAB_ID].sn_lookup = tegra23x_aon_sn_lookup;
	} else if (!strcmp(errmon->name, "BPMP-EN")) {
		fabric_sn_map[BPMP_FAB_ID].fab_name = "BPMP";
		fabric_sn_map[BPMP_FAB_ID].fab_base_vaddr = errmon->vaddr;
		fabric_sn_map[BPMP_FAB_ID].sn_lookup = tegra23x_bpmp_sn_lookup;
	} else if (!strcmp(errmon->name, "APE-EN")) {
		fabric_sn_map[APE_FAB_ID].fab_name = "APE";
		fabric_sn_map[APE_FAB_ID].fab_base_vaddr = errmon->vaddr;
		fabric_sn_map[APE_FAB_ID].sn_lookup = tegra239_ape_sn_lookup;
	} else
		return -EINVAL;

	return 0;
}

static void tegra234_cbb_errmon_set_clk_en_ops(
		struct tegra_cbb_errmon_record *errmon,
		const struct tegra_cbb_noc_data *bdata)
{
	if (bdata->is_clk_rst) {
		errmon->is_clk_rst = bdata->is_clk_rst;
		errmon->is_cluster_probed = bdata->is_cluster_probed;
		errmon->is_clk_enabled = bdata->is_clk_enabled;
		errmon->tegra_errmon_en_clk_rpm = bdata->tegra_noc_en_clk_rpm;
		errmon->tegra_errmon_dis_clk_rpm = bdata->tegra_noc_dis_clk_rpm;
		errmon->tegra_errmon_en_clk_no_rpm =
			bdata->tegra_noc_en_clk_no_rpm;
		errmon->tegra_errmon_dis_clk_no_rpm =
			bdata->tegra_noc_dis_clk_no_rpm;
	}
}

static int tegra234_cbb_errmon_init(struct platform_device *pdev,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
		struct serr_hook *callback,
#endif
		const struct tegra_cbb_noc_data *bdata,
		struct tegra_cbb_init_data *cbb_init_data)
{
	struct tegra_cbb_errmon_record *errmon;
	unsigned long flags;
	struct resource *res_base = cbb_init_data->res_base;
	struct device_node *np;
	int err = 0;

	errmon = devm_kzalloc(&pdev->dev, sizeof(*errmon), GFP_KERNEL);
	if (!errmon)
		return -ENOMEM;

	errmon->start = res_base->start;
	errmon->vaddr = devm_ioremap_resource(&pdev->dev, res_base);
	if (IS_ERR(errmon->vaddr))
		return -EPERM;

	errmon->name      = bdata->name;
	errmon->tegra_cbb_master_id = bdata->tegra_cbb_master_id;
	errmon->erd_mask_inband_err = bdata->erd_mask_inband_err;

	np = of_node_get(pdev->dev.of_node);
	err = of_property_read_u64(np, "err-notifier-base",
						&errmon->err_notifier_base);
	if (err) {
		dev_err(&pdev->dev, "Can't parse err-notifier-base\n");
		return -ENOENT;
	}

	tegra_cbberr_set_ops(&tegra234_cbb_errmon_ops);
	tegra234_cbb_errmon_set_clk_en_ops(errmon, bdata);
	err = tegra234_cbb_errmon_set_data(errmon);
	if (err) {
		dev_err(&pdev->dev, "Err logger name mismatch\n");
		return -EINVAL;
	}

	err = tegra_cbb_err_getirq(pdev,
				&errmon->errmon_nonsecure_irq,
				&errmon->errmon_secure_irq, &errmon->num_intr);
	if (err)
		return -EINVAL;

	cbb_init_data->secure_irq = errmon->errmon_secure_irq;
	cbb_init_data->nonsecure_irq = errmon->errmon_nonsecure_irq;
	cbb_init_data->vaddr = errmon->vaddr + (__force u64)(errmon->err_notifier_base);
	cbb_init_data->addr_mask_erd = (__force u64)(errmon->vaddr)
						+ bdata->off_mask_erd;

	platform_set_drvdata(pdev, errmon);

	spin_lock_irqsave(&cbb_errmon_lock, flags);
	list_add(&errmon->node, &cbb_errmon_list);
	spin_unlock_irqrestore(&cbb_errmon_lock, flags);

	return err;
};

static int tegra234_cbb_probe(struct platform_device *pdev)
{
	const struct tegra_cbb_noc_data *bdata;
	struct resource *res_base;
	struct tegra_cbb_init_data cbb_init_data;
	int err = 0;

	if (!of_machine_is_compatible("nvidia,tegra23x") &&
	    !of_machine_is_compatible("nvidia,tegra234") &&
	    !of_machine_is_compatible("nvidia,tegra239")) {
		dev_err(&pdev->dev, "Wrong SOC\n");
		return -EINVAL;
	}

	bdata = of_device_get_match_data(&pdev->dev);
	if (!bdata) {
		dev_err(&pdev->dev, "No device match found\n");
		return -EINVAL;
	}

	if (bdata->is_clk_rst) {
		if (bdata->is_cluster_probed() && !bdata->is_clk_enabled()) {
			bdata->tegra_noc_en_clk_rpm();
		} else {
			dev_info(&pdev->dev, "defer probe as %s not probed yet",
					bdata->name);
			return -EPROBE_DEFER;
		}
	}

	res_base = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_base) {
		dev_err(&pdev->dev, "Could not find base address");
		return -ENOENT;
	}

	memset(&cbb_init_data, 0, sizeof(cbb_init_data));
	cbb_init_data.res_base = res_base;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
	err = tegra234_cbb_errmon_init(pdev, NULL, bdata, &cbb_init_data);
	if (err) {
		dev_err(&pdev->dev, "cbberr init for soc failing\n");
		return -EINVAL;
	}

	err = tegra_cbberr_register_hook_en(pdev, bdata, NULL,
							cbb_init_data);
	if (err)
		return err;
#else
	err = tegra234_cbb_errmon_init(pdev, bdata, &cbb_init_data);
	if (err) {
		dev_err(&pdev->dev, "cbberr init for soc failing\n");
		return -EINVAL;
	}

	err = tegra_cbberr_register_hook_en(pdev, bdata, cbb_init_data);
	if (err)
		return err;
#endif
	if ((bdata->is_clk_rst) && (bdata->is_cluster_probed())
			&& bdata->is_clk_enabled())
		bdata->tegra_noc_dis_clk_rpm();

	return err;
}

#ifdef CONFIG_PM_SLEEP
static int tegra234_cbb_resume_noirq(struct device *dev)
{
	struct tegra_cbb_errmon_record *errmon = dev_get_drvdata(dev);
	int ret = 0;

	if (errmon->is_clk_rst) {
		if (errmon->is_cluster_probed() && !errmon->is_clk_enabled())
			errmon->tegra_errmon_en_clk_no_rpm();
		else {
			dev_info(dev, "%s not resumed", errmon->name);
			return -EINVAL;
		}
	}

	tegra234_cbb_error_enable(errmon->vaddr+errmon->err_notifier_base);

	if ((errmon->is_clk_rst) && (errmon->is_cluster_probed())
			&& errmon->is_clk_enabled())
		errmon->tegra_errmon_dis_clk_no_rpm();

	dev_info(dev, "%s resumed\n", errmon->name);
	return ret;
}

static int tegra234_cbb_suspend_noirq(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops tegra234_cbb_pm = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(tegra234_cbb_suspend_noirq,
			tegra234_cbb_resume_noirq)
};
#endif

static struct platform_driver tegra234_cbb_driver = {
	.probe          = tegra234_cbb_probe,
	.remove         = tegra234_cbb_remove,
	.driver = {
		.owner  = THIS_MODULE,
		.name   = "tegra23x-cbb",
		.of_match_table = of_match_ptr(tegra234_cbb_match),
#ifdef CONFIG_PM_SLEEP
		.pm     = &tegra234_cbb_pm,
#endif
	},
};

static int __init tegra234_cbb_init(void)
{
	return platform_driver_register(&tegra234_cbb_driver);
}

static void __exit tegra234_cbb_exit(void)
{
	platform_driver_unregister(&tegra234_cbb_driver);
}

pure_initcall(tegra234_cbb_init);
module_exit(tegra234_cbb_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Control Backbone error handling driver for Tegra234");
