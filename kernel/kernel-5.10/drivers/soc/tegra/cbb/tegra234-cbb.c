// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * The driver handles Error's from Control Backbone(CBB) version 2.0.
 * generated due to illegal accesses. The driver prints debug information
 * about failed transaction on receiving interrupt from Error Notifier.
 * Error types supported by CBB2.0 are:
 *   UNSUPPORTED_ERR, PWRDOWN_ERR, TIMEOUT_ERR, FIREWALL_ERR, DECODE_ERR,
 *   SLAVE_ERR
 */

#include <asm/cpufeature.h>
#include <linux/acpi.h>
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
#include <soc/tegra/tegra234-cbb.h>
#include <soc/tegra/tegra239-cbb.h>
#include <soc/tegra/tegra-grace-cbb.h>

static LIST_HEAD(cbb_errmon_list);
static DEFINE_SPINLOCK(cbb_errmon_lock);

static void tegra234_cbb_errmon_faulten(struct tegra_cbb *cbb)
{
	struct tegra_cbb_errmon_record *errmon;
	void __iomem *addr;

	errmon = (struct tegra_cbb_errmon_record *)cbb->err_rec;

	addr = errmon->vaddr + errmon->err_notifier_base;
	writel(0x1FF, addr + FABRIC_EN_CFG_INTERRUPT_ENABLE_0_0);
	dsb(sy);
}

static void tegra234_cbb_errmon_errclr(struct tegra_cbb *cbb)
{
	void __iomem *addr;

	addr = ((struct tegra_cbb_errmon_record *)cbb->err_rec)->addr_errmon;

	writel(0x3F, addr + FABRIC_MN_MASTER_ERR_STATUS_0);
	dsb(sy);
}

static u32 tegra234_cbb_errmon_errvld(struct tegra_cbb *cbb)
{
	struct tegra_cbb_errmon_record *errmon;
	void __iomem *addr;
	u32 errvld;

	errmon = (struct tegra_cbb_errmon_record *)cbb->err_rec;

	addr = errmon->vaddr + errmon->err_notifier_base;
	errvld = readl(addr + FABRIC_EN_CFG_STATUS_0_0);
	dsb(sy);

	return errvld;
}

static void tegra234_cbb_mn_mask_serror(struct tegra_cbb *cbb)
{
	struct tegra_cbb_errmon_record *errmon;
	void __iomem *erd_mask;

	errmon = (struct tegra_cbb_errmon_record *)cbb->err_rec;

	erd_mask = errmon->vaddr + errmon->off_mask_erd;
	writel(0x1, erd_mask);
	dsb(sy);
}

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
		if (strstr(errmon->name, "cbb")) {
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
		if (strstr(errmon->name, "cbb")) {
			writel(val, errmon->vaddr + offset);
			flag = true;
			break;
		}
	}
	if (!flag)
		pr_err("%s: cbb fabric not initialized\n", __func__);
}
EXPORT_SYMBOL(tegra234_cbb_writel);

static u32 tegra234_cbb_get_tmo_slv(void __iomem *addr)
{
	u32 timeout;

	timeout = readl(addr);
	return timeout;
}

static void tegra234_cbb_tmo_slv(struct seq_file *file, char *slv_name,
				 void __iomem *addr, u32 tmo_status)
{
	tegra_cbb_print_err(file, "\t  %s : 0x%x\n", slv_name, tmo_status);
}

static void tegra234_cbb_lookup_apbslv(struct seq_file *file, char *slave_name, u64 addr)
{
	unsigned int blkno_tmo_status, tmo_status;
	unsigned int reset_client, client_id;
	char slv_name[40];
	int block_num = 0;

	tmo_status = tegra234_cbb_get_tmo_slv((void __iomem *)addr);
	if (tmo_status)
		tegra_cbb_print_err(file, "\t  %s_BLOCK_TMO_STATUS : 0x%x\n",
				    slave_name, tmo_status);

	while (tmo_status) {
		if (!(tmo_status & BIT(0)))
			goto next_iter;

		addr =  addr + APB_BLOCK_NUM_TMO_OFFSET + (block_num * 4);
		blkno_tmo_status = tegra234_cbb_get_tmo_slv((void __iomem *)addr);
		reset_client = blkno_tmo_status;

		if (blkno_tmo_status) {
			client_id = 1;
			while (blkno_tmo_status) {
				if (blkno_tmo_status & 0x1) {
					if (reset_client != 0xffffffff)
						reset_client &= client_id;

					sprintf(slv_name, "%s_BLOCK%d_TMO", slave_name, block_num);

					tegra234_cbb_tmo_slv(file, slv_name, (void __iomem *)addr,
							     reset_client);
				}
				blkno_tmo_status >>= 1;
				client_id <<= 1;
			}
		}
next_iter:
		tmo_status >>= 1;
		block_num++;
	}
}

static void tegra234_lookup_slave_timeout(struct seq_file *file,
					  struct tegra_cbb_errmon_record *errmon,
					  u8 slave_id, u8 fab_id)
{
	struct tegra_sn_addr_map *sn_lookup = errmon->sn_addr_map;
	void __iomem *base_addr = errmon->vaddr;
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

	addr = (u64)base_addr + sn_lookup[i].off_slave;

	if (strstr(sn_lookup[i].slave_name, "AXI2APB")) {
		addr = addr + APB_BLOCK_TMO_STATUS_0;
		tegra234_cbb_lookup_apbslv(file, sn_lookup[i].slave_name, addr);
	} else {
		addr = addr + AXI_SLV_TIMEOUT_STATUS_0_0;

		tmo_status = tegra234_cbb_get_tmo_slv((void __iomem *)addr);
		if (tmo_status) {
			sprintf(slv_name, "%s_SLV_TIMEOUT_STATUS", sn_lookup[i].slave_name);
			tegra234_cbb_tmo_slv(file, slv_name, (void __iomem *)addr, tmo_status);
		}
	}
}

static void print_errmon_err(struct seq_file *file, struct tegra_cbb_errmon_record *errmon,
			     unsigned int em_err_status, unsigned int em_overflow_status)
{
	int err_type = 0;

	if (em_err_status & (em_err_status - 1))
		tegra_cbb_print_err(file, "\t  Multiple type of errors reported\n");

	while (em_err_status) {
		if (em_err_status & 0x1)
			tegra_cbb_print_err(file, "\t  Error Code\t\t: %s\n",
					    t234_errmon_errors[err_type].errcode);
		em_err_status >>= 1;
		err_type++;
	}

	err_type = 0;
	while (em_overflow_status) {
		if (em_overflow_status & 0x1)
			tegra_cbb_print_err(file, "\t  Overflow\t\t: Multiple %s\n",
					    t234_errmon_errors[err_type].errcode);
		em_overflow_status >>= 1;
		err_type++;
	}
}

static void print_errlog_err(struct seq_file *file, struct tegra_cbb_errmon_record *errmon)
{
	u8 cache_type = 0, prot_type = 0, burst_length = 0;
	u8 mstr_id = 0, grpsec = 0, vqc = 0, falconsec = 0;
	u8 beat_size = 0, access_type = 0, access_id = 0;
	u8 requester_socket_id = 0, local_socket_id = 0;
	u8 slave_id = 0, fab_id = 0, burst_type = 0;
	char fabric_name[20];
	bool is_numa = 0;

	if (num_possible_nodes() > 1)
		is_numa = true;

	mstr_id   = FIELD_GET(FAB_EM_EL_MSTRID, errmon->mn_user_bits);
	vqc	  = FIELD_GET(FAB_EM_EL_VQC, errmon->mn_user_bits);
	grpsec	  = FIELD_GET(FAB_EM_EL_GRPSEC, errmon->mn_user_bits);
	falconsec = FIELD_GET(FAB_EM_EL_FALCONSEC, errmon->mn_user_bits);

	/*
	 * For SOC with multiple NUMA nodes, print cross socket access
	 * errors only if initiator/master_id is CCPLEX, CPMU or GPU.
	 */
	if (is_numa) {
		local_socket_id = numa_node_id();
		requester_socket_id = FIELD_GET(REQ_SOCKET_ID, errmon->mn_attr2);

		if (requester_socket_id != local_socket_id) {
			if ((mstr_id != 0x1) && (mstr_id != 0x2) && (mstr_id != 0xB))
				return;
		}
	}

	fab_id	   = FIELD_GET(FAB_EM_EL_FABID, errmon->mn_attr2);
	slave_id   = FIELD_GET(FAB_EM_EL_SLAVEID, errmon->mn_attr2);

	access_id  = FIELD_GET(FAB_EM_EL_ACCESSID, errmon->mn_attr1);

	cache_type   = FIELD_GET(FAB_EM_EL_AXCACHE, errmon->mn_attr0);
	prot_type    = FIELD_GET(FAB_EM_EL_AXPROT, errmon->mn_attr0);
	burst_length = FIELD_GET(FAB_EM_EL_BURSTLENGTH, errmon->mn_attr0);
	burst_type   = FIELD_GET(FAB_EM_EL_BURSTTYPE, errmon->mn_attr0);
	beat_size    = FIELD_GET(FAB_EM_EL_BEATSIZE, errmon->mn_attr0);
	access_type  = FIELD_GET(FAB_EM_EL_ACCESSTYPE, errmon->mn_attr0);

	tegra_cbb_print_err(file, "\n");
	tegra_cbb_print_err(file, "\t  Error Code\t\t: %s\n",
			    t234_errmon_errors[errmon->err_type].errcode);

	tegra_cbb_print_err(file, "\t  MASTER_ID\t\t: %s\n", errmon->tegra_cbb_master_id[mstr_id]);
	tegra_cbb_print_err(file, "\t  Address\t\t: 0x%llx\n", (u64)errmon->addr_access);

	tegra_cbb_print_cache(file, cache_type);
	tegra_cbb_print_prot(file, prot_type);

	tegra_cbb_print_err(file, "\t  Access_Type\t\t: %s", (access_type) ? "Write\n" : "Read\n");
	tegra_cbb_print_err(file, "\t  Access_ID\t\t: 0x%x", access_id);

	if (fab_id == PSC_FAB_ID)
		strcpy(fabric_name, "psc-fabric");
	else if (fab_id == FSI_FAB_ID)
		strcpy(fabric_name, "fsi-fabric");
	else
		strcpy(fabric_name, errmon->name);

	if (is_numa) {
		tegra_cbb_print_err(file, "\t  Requester_Socket_Id\t: 0x%x\n",
				    requester_socket_id);
		tegra_cbb_print_err(file, "\t  Local_Socket_Id\t: 0x%x\n",
				    local_socket_id);
		tegra_cbb_print_err(file, "\t  No. of NUMA_NODES\t: 0x%x\n",
				    num_possible_nodes());
	}

	tegra_cbb_print_err(file, "\t  Fabric\t\t: %s\n", fabric_name);
	tegra_cbb_print_err(file, "\t  Slave_Id\t\t: 0x%x\n", slave_id);
	tegra_cbb_print_err(file, "\t  Burst_length\t\t: 0x%x\n", burst_length);
	tegra_cbb_print_err(file, "\t  Burst_type\t\t: 0x%x\n", burst_type);
	tegra_cbb_print_err(file, "\t  Beat_size\t\t: 0x%x\n", beat_size);
	tegra_cbb_print_err(file, "\t  VQC\t\t\t: 0x%x\n", vqc);
	tegra_cbb_print_err(file, "\t  GRPSEC\t\t: 0x%x\n", grpsec);
	tegra_cbb_print_err(file, "\t  FALCONSEC\t\t: 0x%x\n", falconsec);

	if ((fab_id == PSC_FAB_ID) || (fab_id == FSI_FAB_ID))
		return;

	if (!strcmp(errmon->noc_errors[errmon->err_type].errcode, "TIMEOUT_ERR")) {
		tegra234_lookup_slave_timeout(file, errmon, slave_id, fab_id);
		return;
	}
	tegra_cbb_print_err(file, "\t  Slave\t\t\t: %s\n",
			    errmon->sn_addr_map[slave_id].slave_name);
}

static int print_errmonX_info(struct seq_file *file, struct tegra_cbb_errmon_record *errmon)
{
	unsigned int em_overflow_status = 0;
	unsigned int em_err_status = 0;
	unsigned int el_err_status = 0;
	u64 addr = 0;

	errmon->err_type = 0;

	em_err_status = readl(errmon->addr_errmon +
					FABRIC_MN_MASTER_ERR_STATUS_0);
	if (!em_err_status) {
		pr_err("Error Notifier received a spurious notification\n");
		BUG();
	}

	if (em_err_status == 0xFFFFFFFF) {
		pr_err("CBB registers returning all 1's which is invalid\n");
		return -EINVAL;
	}

	/*get overflow flag*/
	em_overflow_status = readl(errmon->addr_errmon +
				   FABRIC_MN_MASTER_ERR_OVERFLOW_STATUS_0);

	print_errmon_err(file, errmon, em_err_status, em_overflow_status);

	el_err_status = readl(errmon->addr_errmon + FABRIC_MN_MASTER_LOG_ERR_STATUS_0);
	if (!el_err_status) {
		pr_info("Error Monitor doesn't have Error Logger\n");
		return -EINVAL;
	}

	while (el_err_status) {
		if (el_err_status & BIT(0)) {
			addr = readl(errmon->addr_errmon +
				     FABRIC_MN_MASTER_LOG_ADDR_HIGH_0);
			addr = (addr << 32) |
				readl(errmon->addr_errmon +
				      FABRIC_MN_MASTER_LOG_ADDR_LOW_0);
			errmon->addr_access = (void __iomem *)addr;

			errmon->mn_attr0 = readl(errmon->addr_errmon +
						 FABRIC_MN_MASTER_LOG_ATTRIBUTES0_0);

			errmon->mn_attr1 = readl(errmon->addr_errmon +
						 FABRIC_MN_MASTER_LOG_ATTRIBUTES1_0);

			errmon->mn_attr2 = readl(errmon->addr_errmon +
						 FABRIC_MN_MASTER_LOG_ATTRIBUTES2_0);

			errmon->mn_user_bits = readl(errmon->addr_errmon +
						     FABRIC_MN_MASTER_LOG_USER_BITS0_0);

			print_errlog_err(file, errmon);
		}
		errmon->err_type++;
		el_err_status >>= 1;
	}
	return 0;
}

static int print_err_notifier(struct seq_file *file, struct tegra_cbb *cbb,
			      struct tegra_cbb_errmon_record *errmon, int err_notifier_sts)
{
	u64 em_addr_offset = 0;
	u64 em_phys_addr = 0;
	int errmon_no = 1;
	int ret = 0;

	pr_crit("**************************************\n");
	pr_crit("CPU:%d, Error:%s, Errmon:%d\n", smp_processor_id(),
		errmon->name, err_notifier_sts);

	while (err_notifier_sts) {
		if (err_notifier_sts & BIT(0)) {
			writel(errmon_no, errmon->vaddr + errmon->err_notifier_base +
			       FABRIC_EN_CFG_ADDR_INDEX_0_0);

			em_phys_addr = readl(errmon->vaddr + errmon->err_notifier_base +
					     FABRIC_EN_CFG_ADDR_HI_0);
			em_phys_addr = (em_phys_addr << 32) |
					    readl(errmon->vaddr + errmon->err_notifier_base +
						  FABRIC_EN_CFG_ADDR_LOW_0);

			em_addr_offset = em_phys_addr - errmon->start;
			errmon->addr_errmon = (void __iomem *)(errmon->vaddr + em_addr_offset);

			errmon->errmon_no = errmon_no;

			ret = print_errmonX_info(file, errmon);
			tegra234_cbb_errmon_errclr(cbb);
			if (ret)
				return ret;
		}
		err_notifier_sts >>= 1;
		errmon_no <<= 1;
	}

	tegra_cbb_print_err(file, "\t**************************************\n");
	return 0;
}

#ifdef CONFIG_DEBUG_FS
static DEFINE_MUTEX(cbb_err_mutex);

static int tegra234_cbb_err_show(struct tegra_cbb *cbb, struct seq_file *file, void *data)
{
	struct tegra_cbb_errmon_record *errmon;
	unsigned int errvld = 0;
	int ret = 0;

	mutex_lock(&cbb_err_mutex);

	list_for_each_entry(errmon, &cbb_errmon_list, node) {
		cbb = errmon->cbb;
		errvld = tegra_cbb_errvld(cbb);
		if (errvld) {
			ret = print_err_notifier(file, cbb, errmon, errvld);
			if (ret)
				goto en_show_exit;
		}
	}

en_show_exit:
	mutex_unlock(&cbb_err_mutex);
	return ret;
}
#endif

/*
 * Handler for CBB errors
 */
static irqreturn_t tegra234_cbb_err_isr(int irq, void *data)
{
	struct tegra_cbb_errmon_record *errmon;
	struct tegra_cbb *cbb = data;
	bool is_inband_err = 0;
	unsigned long flags;
	u32 errvld = 0;
	u8 mstr_id = 0;
	int ret = 0;

	spin_lock_irqsave(&cbb_errmon_lock, flags);

	list_for_each_entry(errmon, &cbb_errmon_list, node) {
		errvld = tegra_cbb_errvld(cbb);

		if (errvld && (irq == errmon->sec_irq)) {
			tegra_cbb_print_err(NULL, "CPU:%d, Error:%s@0x%llx, irq=%d\n",
					    smp_processor_id(), errmon->name,
					    errmon->start, irq);

			ret = print_err_notifier(NULL, cbb, errmon, errvld);
			if (ret)
				goto en_isr_exit;

			mstr_id =  FIELD_GET(USRBITS_MSTR_ID, errmon->mn_user_bits);

			/* If illegal request is from CCPLEX(id:0x1)
			 * master then call BUG() to crash system.
			 */
			if ((mstr_id == 0x1) && errmon->erd_mask_inband_err)
				is_inband_err = 1;
		}
	}
en_isr_exit:
	spin_unlock_irqrestore(&cbb_errmon_lock, flags);

	WARN_ON(is_inband_err);

	return IRQ_HANDLED;
}

/*
 * Register handler for CBB_SECURE interrupt for reporting errors
 */
static int tegra234_cbb_intr_en(struct tegra_cbb *cbb)
{
	struct platform_device *pdev = cbb->pdev;
	int sec_irq;
	int err = 0;

	sec_irq = ((struct tegra_cbb_errmon_record *)cbb->err_rec)->sec_irq;

	if (sec_irq) {
		err = devm_request_irq(&pdev->dev, sec_irq, tegra234_cbb_err_isr,
				       0, dev_name(&pdev->dev), cbb);
		if (err) {
			dev_err(&pdev->dev, "%s: Unable to register (%d) interrupt\n",
				__func__, sec_irq);
			goto isr_err_free_sec_irq;
		}
	}
	return 0;

isr_err_free_sec_irq:
	free_irq(sec_irq, pdev);

	return err;
}

static void tegra234_cbb_err_en(struct tegra_cbb *cbb)
{
	tegra_cbb_faulten(cbb);
}

static struct tegra_cbb_err_ops tegra234_cbb_errmon_ops = {
	.errvld	 = tegra234_cbb_errmon_errvld,
	.errclr	 = tegra234_cbb_errmon_errclr,
	.faulten = tegra234_cbb_errmon_faulten,
	.cbb_err_enable = tegra234_cbb_err_en,
	.cbb_intr_enable = tegra234_cbb_intr_en,
#ifdef CONFIG_DEBUG_FS
	.cbb_err_debugfs_show = tegra234_cbb_err_show
#endif
};

static struct tegra_cbb_fabric_data tegra234_aon_fab_data = {
	.name   = "aon-fabric",
	.tegra_cbb_master_id = t234_master_id,
	.sn_addr_map = t234_aon_sn_lookup,
	.noc_errors = t234_errmon_errors,
	.err_notifier_base = 0x17000
};

static struct tegra_cbb_fabric_data tegra234_bpmp_fab_data = {
	.name   = "bpmp-fabric",
	.tegra_cbb_master_id = t234_master_id,
	.sn_addr_map = t234_bpmp_sn_lookup,
	.noc_errors = t234_errmon_errors,
	.err_notifier_base = 0x19000
};

static struct tegra_cbb_fabric_data tegra234_cbb_fab_data = {
	.name   = "cbb-fabric",
	.tegra_cbb_master_id = t234_master_id,
	.sn_addr_map = t234_cbb_sn_lookup,
	.noc_errors = t234_errmon_errors,
	.err_notifier_base = 0x60000,
	.off_mask_erd = 0x3a004
};

static struct tegra_cbb_fabric_data tegra234_dce_fab_data = {
	.name   = "dce-fabric",
	.tegra_cbb_master_id = t234_master_id,
	.sn_addr_map = t234_dce_sn_lookup,
	.noc_errors = t234_errmon_errors,
	.err_notifier_base = 0x19000
};

static struct tegra_cbb_fabric_data tegra234_rce_fab_data = {
	.name   = "rce-fabric",
	.tegra_cbb_master_id = t234_master_id,
	.sn_addr_map = t234_rce_sn_lookup,
	.noc_errors = t234_errmon_errors,
	.err_notifier_base = 0x19000
};

static struct tegra_cbb_fabric_data tegra234_sce_fab_data = {
	.name   = "sce-fabric",
	.tegra_cbb_master_id = t234_master_id,
	.sn_addr_map = t234_sce_sn_lookup,
	.noc_errors = t234_errmon_errors,
	.err_notifier_base = 0x19000
};

static struct tegra_cbb_fabric_data tegra_grace_cbb_fab_data = {
	.name   = "cbb-fabric",
	.tegra_cbb_master_id = tegra_grace_master_id,
	.sn_addr_map = tegra_grace_cbb_sn_lookup,
	.noc_errors = tegra_grace_errmon_errors,
	.err_notifier_base = 0x60000,
	.off_mask_erd = 0x40004
};

static struct tegra_cbb_fabric_data tegra_grace_bpmp_fab_data = {
	.name   = "bpmp-fabric",
	.tegra_cbb_master_id = tegra_grace_master_id,
	.sn_addr_map = tegra_grace_bpmp_sn_lookup,
	.noc_errors = tegra_grace_errmon_errors,
	.err_notifier_base = 0x19000
};

static struct tegra_cbb_fabric_data tegra239_aon_fab_data = {
	.name   = "aon-fabric",
	.tegra_cbb_master_id = t234_master_id,
	.sn_addr_map = t234_aon_sn_lookup,
	.noc_errors = t234_errmon_errors,
	.err_notifier_base = 0x17000
};

static struct tegra_cbb_fabric_data tegra239_bpmp_fab_data = {
	.name   = "bpmp-fabric",
	.tegra_cbb_master_id = t234_master_id,
	.sn_addr_map = t234_bpmp_sn_lookup,
	.noc_errors = t234_errmon_errors,
	.err_notifier_base = 0x19000
};

static struct tegra_cbb_fabric_data tegra239_cbb_fab_data = {
	.name   = "cbb-fabric",
	.tegra_cbb_master_id = t234_master_id,
	.sn_addr_map = t239_cbb_sn_lookup,
	.noc_errors = t234_errmon_errors,
	.err_notifier_base = 0x60000,
	.off_mask_erd = 0x3d004
};

static struct tegra_cbb_fabric_data tegra239_ape_fab_data = {
	.name   = "ape-fabric",
	.tegra_cbb_master_id = t234_master_id,
	.sn_addr_map = t239_ape_sn_lookup,
	.noc_errors = t234_errmon_errors,
	.err_notifier_base = 0x1E000
};

static const struct of_device_id tegra234_cbb_dt_ids[] = {
	{.compatible    = "nvidia,tegra234-cbb-fabric",
		.data = &tegra234_cbb_fab_data},
	{.compatible    = "nvidia,tegra234-aon-fabric",
		.data = &tegra234_aon_fab_data},
	{.compatible    = "nvidia,tegra234-bpmp-fabric",
		.data = &tegra234_bpmp_fab_data},
	{.compatible    = "nvidia,tegra234-dce-fabric",
		.data = &tegra234_dce_fab_data},
	{.compatible    = "nvidia,tegra234-rce-fabric",
		.data = &tegra234_rce_fab_data},
	{.compatible    = "nvidia,tegra234-sce-fabric",
		.data = &tegra234_sce_fab_data},
	{.compatible    = "nvidia,tegra239-cbb-fabric",
		.data = &tegra239_cbb_fab_data},
	{.compatible    = "nvidia,tegra239-aon-fabric",
		.data = &tegra239_aon_fab_data},
	{.compatible    = "nvidia,tegra239-bpmp-fabric",
		.data = &tegra239_bpmp_fab_data},
	{.compatible    = "nvidia,tegra239-ape-fabric",
		.data = &tegra239_ape_fab_data},
	{},
};
MODULE_DEVICE_TABLE(of, tegra234_cbb_dt_ids);

struct cbb_acpi_uid_noc {
	const char *hid;
	const char *uid;
	const struct tegra_cbb_fabric_data *fab;
};

static const struct cbb_acpi_uid_noc cbb_acpi_uids[] = {
	{ "NVDA1070", "1", &tegra_grace_cbb_fab_data },
	{ "NVDA1070", "2", &tegra_grace_bpmp_fab_data },
	{ },
};

static const struct acpi_device_id tegra_grace_cbb_acpi_ids[] = {
	{ "NVDA1070" },
	{ },
};
MODULE_DEVICE_TABLE(acpi, tegra_grace_cbb_acpi_ids);

static const struct
tegra_cbb_fabric_data *cbb_acpi_get_fab_data(struct acpi_device *adev)
{
	const struct cbb_acpi_uid_noc *u;

	for (u = cbb_acpi_uids; u->hid; u++) {
		if (acpi_dev_hid_uid_match(adev, u->hid, u->uid))
			return u->fab;
	}
	return NULL;
}

static int
tegra234_cbb_errmon_init(const struct tegra_cbb_fabric_data *pdata,
			 struct tegra_cbb *cbb, struct resource *res_base)
{
	struct platform_device *pdev = cbb->pdev;
	struct tegra_cbb_errmon_record *errmon;
	unsigned long flags = 0;
	int err = 0;

	errmon = (struct tegra_cbb_errmon_record *)cbb->err_rec;
	errmon->vaddr = devm_ioremap_resource(&pdev->dev, res_base);
	if (IS_ERR(errmon->vaddr))
		return -EINVAL;

	errmon->name = pdata->name;
	errmon->start = res_base->start;
	errmon->tegra_cbb_master_id = pdata->tegra_cbb_master_id;
	errmon->err_notifier_base = pdata->err_notifier_base;
	errmon->off_mask_erd = pdata->off_mask_erd;
	errmon->sn_addr_map = pdata->sn_addr_map;
	errmon->noc_errors = pdata->noc_errors;
	errmon->cbb = cbb;

	if (errmon->off_mask_erd)
		errmon->erd_mask_inband_err = 1;

	err = tegra_cbb_err_getirq(pdev, NULL, &errmon->sec_irq);
	if (err)
		return err;

	cbb->ops = &tegra234_cbb_errmon_ops;

	spin_lock_irqsave(&cbb_errmon_lock, flags);
	list_add(&errmon->node, &cbb_errmon_list);
	spin_unlock_irqrestore(&cbb_errmon_lock, flags);

	return 0;
};

static int tegra234_cbb_probe(struct platform_device *pdev)
{
	struct tegra_cbb_errmon_record *errmon = NULL;
	const struct tegra_cbb_fabric_data *pdata = NULL;
	struct resource *res_base = NULL;
	struct device *dev = &pdev->dev;
	struct acpi_device *device;
	struct tegra_cbb *cbb;
	int err = 0;

	if (of_machine_is_compatible("nvidia,tegra23x") ||
	    of_machine_is_compatible("nvidia,tegra234") ||
	    of_machine_is_compatible("nvidia,tegra239")) {
		pdata = of_device_get_match_data(&pdev->dev);
	} else {
		device = ACPI_COMPANION(dev);
		if (!device)
			return -ENODEV;
		pdata = cbb_acpi_get_fab_data(device);
	}
	if (!pdata) {
		dev_err(&pdev->dev, "No device match found\n");
		return -EINVAL;
	}

	res_base = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_base) {
		dev_err(&pdev->dev, "Could not find base address");
		return -ENOENT;
	}

	cbb = devm_kzalloc(&pdev->dev, sizeof(*cbb), GFP_KERNEL);
	if (!cbb)
		return -ENOMEM;

	errmon = devm_kzalloc(&pdev->dev, sizeof(*errmon), GFP_KERNEL);
	if (!errmon)
		return -ENOMEM;

	cbb->err_rec = errmon;
	cbb->pdev = pdev;
	err = tegra234_cbb_errmon_init(pdata, cbb, res_base);
	if (err) {
		dev_err(&pdev->dev, "cbberr init for soc failing\n");
		return err;
	}

	/* set ERD bit to mask SError and generate interrupt to report error */
	if (errmon->erd_mask_inband_err)
		tegra234_cbb_mn_mask_serror(cbb);

	platform_set_drvdata(pdev, cbb);

	return tegra_cbb_register_isr_enaberr(cbb);
}

static int tegra234_cbb_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tegra234_cbb_resume_noirq(struct device *dev)
{
	struct tegra_cbb *cbb = dev_get_drvdata(dev);
	struct tegra_cbb_errmon_record *errmon;

	errmon = (struct tegra_cbb_errmon_record *)cbb->err_rec;

	if (!errmon)
		return -EINVAL;

	tegra234_cbb_err_en(cbb);

	dev_info(dev, "%s resumed\n", errmon->name);
	return 0;
}

static const struct dev_pm_ops tegra234_cbb_pm = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(NULL, tegra234_cbb_resume_noirq)
};
#endif

static struct platform_driver tegra234_cbb_driver = {
	.probe          = tegra234_cbb_probe,
	.remove         = tegra234_cbb_remove,
	.driver = {
		.owner  = THIS_MODULE,
		.name   = "tegra234-cbb",
		.of_match_table = of_match_ptr(tegra234_cbb_dt_ids),
		.acpi_match_table = ACPI_PTR(tegra_grace_cbb_acpi_ids),
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
MODULE_DESCRIPTION("Control Backbone 2.0 error handling driver for Tegra234");
