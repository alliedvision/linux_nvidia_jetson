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
 */

#define FABRIC_EN_CFG_INTERRUPT_ENABLE_0_0	0x0
#define FABRIC_EN_CFG_STATUS_0_0		0x40
#define FABRIC_EN_CFG_ADDR_INDEX_0_0		0x60
#define FABRIC_EN_CFG_ADDR_LOW_0		0x80
#define FABRIC_EN_CFG_ADDR_HI_0			0x84

#define FABRIC_MN_MASTER_ERR_EN_0		0x200
#define FABRIC_MN_MASTER_ERR_FORCE_0		0x204
#define FABRIC_MN_MASTER_ERR_STATUS_0		0x208
#define FABRIC_MN_MASTER_ERR_OVERFLOW_STATUS_0	0x20c

#define FABRIC_MN_MASTER_LOG_ERR_STATUS_0	0x300
#define FABRIC_MN_MASTER_LOG_ADDR_LOW_0		0x304
#define FABRIC_MN_MASTER_LOG_ADDR_HIGH_0	0x308
#define FABRIC_MN_MASTER_LOG_ATTRIBUTES0_0	0x30c
#define FABRIC_MN_MASTER_LOG_ATTRIBUTES1_0	0x310
#define FABRIC_MN_MASTER_LOG_ATTRIBUTES2_0	0x314
#define FABRIC_MN_MASTER_LOG_USER_BITS0_0	0x318

#define AXI_SLV_TIMEOUT_STATUS_0_0      	0x8
#define APB_BLOCK_TMO_STATUS_0          	0xC00
#define APB_BLOCK_NUM_TMO_OFFSET        	0x20

#define get_em_el_subfield(_x_, _msb_, _lsb_) CBB_EXTRACT(_x_, _msb_, _lsb_)

enum fabric{
	CBB_FAB_ID,
	SCE_FAB_ID,
	RCE_FAB_ID,
	DCE_FAB_ID,
	AON_FAB_ID,
	PSC_FAB_ID,
	BPMP_FAB_ID,
	FSI_FAB_ID,
	APE_FAB_ID,
	MAX_FAB_ID,
};

struct tegra_cbb_errmon_record {
	struct list_head node;
	const char *name;
	int errmon_no;
	u32 err_type;
	phys_addr_t start;
	phys_addr_t err_notifier_base;
	void __iomem *vaddr;
	void __iomem *addr_errmon;
	void __iomem *addr_access;
	u32 attr0;
	u32 attr1;
	u32 attr2;
	u32 user_bits;
	int num_intr;
	int errmon_secure_irq;
	int errmon_nonsecure_irq;
	char **tegra_cbb_master_id;
	bool erd_mask_inband_err;
	bool is_clk_rst;
	int (*is_cluster_probed)(void);
	int (*is_clk_enabled)(void);
	int (*tegra_errmon_en_clk_rpm)(void);
	int (*tegra_errmon_dis_clk_rpm)(void);
	int (*tegra_errmon_en_clk_no_rpm)(void);
	int (*tegra_errmon_dis_clk_no_rpm)(void);
};

struct tegra_sn_addr_map {
	char *slave_name;
	u32 off_slave;
};

struct tegra23x_cbb_fabric_sn_map {
	const char *fab_name;
	void __iomem *fab_base_vaddr;
	struct tegra_sn_addr_map *sn_lookup;
};

static char *t234_master_id[] = {
	"TZ",				/* 0x0  */
	"CCPLEX",			/* 0x1  */
	"CCPMU",			/* 0x2  */
	"BPMP_FW",			/* 0x3  */
	"AON",				/* 0x4  */
	"SCE",				/* 0x5  */
	"GPCDMA_P",			/* 0x6  */
	"TSECA_NONSECURE",		/* 0x7  */
	"TSECA_LIGHTSECURE",		/* 0x8  */
	"TSECA_HEAVYSECURE",		/* 0x9  */
	"CORESIGHT",			/* 0xA  */
	"APE",				/* 0xB  */
	"PEATRANS",			/* 0xC  */
	"JTAGM_DFT",			/* 0xD  */
	"RCE",				/* 0xE  */
	"DCE",				/* 0xF  */
	"PSC_FW_USER",			/* 0x10 */
	"PSC_FW_SUPERVISOR",		/* 0x11 */
	"PSC_FW_MACHINE",		/* 0x12 */
	"PSC_BOOT",			/* 0x13 */
	"BPMP_BOOT",			/* 0x14 */
	"NVDEC_NONSECURE",		/* 0x15 */
	"NVDEC_LIGHTSECURE",		/* 0x16 */
	"NVDEC_HEAVYSECURE",		/* 0x17 */
	"CBB_INTERNAL",			/* 0x18 */
	"RSVD"				/* 0x3F */
};

static struct tegra_noc_errors tegra234_errmon_errors[] = {
	{.errcode = "SLAVE_ERR",
	 .type = "Slave being accessed responded with an error"
	},
	{.errcode = "DECODE_ERR",
	 .type = "Attempt to access an address hole"
	},
	{.errcode = "FIREWALL_ERR",
	 .type = "Attempt to access a region which is firewall protected"
	},
	{.errcode = "TIMEOUT_ERR",
	 .type = "No response returned by slave"
	},
	{.errcode = "PWRDOWN_ERR",
	 .type = "Attempt to access a portion of fabric that is powered down"
	},
	{.errcode = "UNSUPPORTED_ERR",
	 .type = "Attempt to access a slave through an unsupported access"
	}
};

#define AON_SN_AXI2APB_1	0x00000
#define AON_SN_AST1_T 		0x14000
#define AON_SN_CBB_T		0x15000
#define AON_SN_CPU_T		0x16000

#define BPMP_SN_AXI2APB_1	0x00000
#define BPMP_SN_AST0_T		0x15000
#define BPMP_SN_AST1_T		0x16000
#define BPMP_SN_CBB_T		0x17000
#define BPMP_SN_CPU_T		0x18000

#define DCE_SN_AXI2APB_1	0x00000
#define DCE_SN_AST0_T		0x15000
#define DCE_SN_AST1_T		0x16000
#define DCE_SN_CPU_T		0x18000

#define RCE_SN_AXI2APB_1	0x00000
#define RCE_SN_AST0_T		0x15000
#define RCE_SN_AST1_T		0x16000
#define RCE_SN_CPU_T		0x18000

#define SCE_SN_AXI2APB_1	0x00000
#define SCE_SN_AST0_T		0x15000
#define SCE_SN_AST1_T		0x16000
#define SCE_SN_CBB_T		0x17000
#define SCE_SN_CPU_T		0x18000

#define CBB_SN_AON_SLAVE	0x40000
#define CBB_SN_BPMP_SLAVE	0x41000
#define CBB_SN_CBB_CENTRAL	0x42000
#define CBB_SN_HOST1X		0x43000
#define CBB_SN_STM		0x44000
#define CBB_SN_FSI_SLAVE	0x45000
#define CBB_SN_PSC_SLAVE	0x46000
#define CBB_SN_PCIE_C1		0x47000
#define CBB_SN_PCIE_C2		0x48000
#define CBB_SN_PCIE_C3		0x49000
#define CBB_SN_PCIE_C0		0x4A000
#define CBB_SN_PCIE_C4		0x4B000
#define CBB_SN_GPU		0x4C000
#define CBB_SN_SMMU0		0x4D000
#define CBB_SN_SMMU1		0x4E000
#define CBB_SN_SMMU2		0x4F000
#define CBB_SN_SMMU3		0x50000
#define CBB_SN_SMMU4		0x51000
#define CBB_SN_PCIE_C10		0x52000
#define CBB_SN_PCIE_C7		0x53000
#define CBB_SN_PCIE_C8		0x54000
#define CBB_SN_PCIE_C9		0x55000
#define CBB_SN_PCIE_C5		0x56000
#define CBB_SN_PCIE_C6		0x57000
#define CBB_SN_DCE_SLAVE	0x58000
#define CBB_SN_RCE_SLAVE	0x59000
#define CBB_SN_SCE_SLAVE	0x5A000
#define CBB_SN_AXI2APB_1	0x70000
#define CBB_SN_AXI2APB_10	0x71000
#define CBB_SN_AXI2APB_11	0x72000
#define CBB_SN_AXI2APB_12	0x73000
#define CBB_SN_AXI2APB_13	0x74000
#define CBB_SN_AXI2APB_14	0x75000
#define CBB_SN_AXI2APB_15	0x76000
#define CBB_SN_AXI2APB_16	0x77000
#define CBB_SN_AXI2APB_17	0x78000
#define CBB_SN_AXI2APB_18	0x79000
#define CBB_SN_AXI2APB_19	0x7A000
#define CBB_SN_AXI2APB_2	0x7B000
#define CBB_SN_AXI2APB_20	0x7C000
#define CBB_SN_AXI2APB_21	0x7D000
#define CBB_SN_AXI2APB_22	0x7E000
#define CBB_SN_AXI2APB_23	0x7F000
#define CBB_SN_AXI2APB_25	0x80000
#define CBB_SN_AXI2APB_26	0x81000
#define CBB_SN_AXI2APB_27	0x82000
#define CBB_SN_AXI2APB_28	0x83000
#define CBB_SN_AXI2APB_29	0x84000
#define CBB_SN_AXI2APB_30	0x85000
#define CBB_SN_AXI2APB_31	0x86000
#define CBB_SN_AXI2APB_32	0x87000
#define CBB_SN_AXI2APB_33	0x88000
#define CBB_SN_AXI2APB_34	0x89000
#define CBB_SN_AXI2APB_35	0x92000
#define CBB_SN_AXI2APB_4	0x8B000
#define CBB_SN_AXI2APB_5	0x8C000
#define CBB_SN_AXI2APB_6	0x8D000
#define CBB_SN_AXI2APB_7	0x8E000
#define CBB_SN_AXI2APB_8	0x8F000
#define CBB_SN_AXI2APB_9	0x90000
#define CBB_SN_AXI2APB_3	0x91000

#define SLAVE_LOOKUP(sn) #sn, sn

static struct tegra_sn_addr_map tegra23x_aon_sn_lookup[] = {
	{ SLAVE_LOOKUP(AON_SN_AXI2APB_1) },
	{ SLAVE_LOOKUP(AON_SN_AST1_T) },
	{ SLAVE_LOOKUP(AON_SN_CBB_T) },
	{ SLAVE_LOOKUP(AON_SN_CPU_T) }
};

static struct tegra_sn_addr_map tegra23x_bpmp_sn_lookup[] = {
	{ SLAVE_LOOKUP(BPMP_SN_AXI2APB_1) },
	{ SLAVE_LOOKUP(BPMP_SN_AST0_T) },
	{ SLAVE_LOOKUP(BPMP_SN_AST1_T) },
	{ SLAVE_LOOKUP(BPMP_SN_CBB_T) },
	{ SLAVE_LOOKUP(BPMP_SN_CPU_T) },
};

static struct tegra_sn_addr_map tegra23x_sce_sn_lookup[] = {
	{ SLAVE_LOOKUP(SCE_SN_AXI2APB_1) },
	{ SLAVE_LOOKUP(SCE_SN_AST0_T) },
	{ SLAVE_LOOKUP(SCE_SN_AST1_T) },
	{ SLAVE_LOOKUP(SCE_SN_CBB_T) },
	{ SLAVE_LOOKUP(SCE_SN_CPU_T) }
};

static struct tegra_sn_addr_map tegra23x_dce_sn_lookup[] = {
	{ SLAVE_LOOKUP(DCE_SN_AXI2APB_1) },
	{ SLAVE_LOOKUP(DCE_SN_AST0_T) },
	{ SLAVE_LOOKUP(DCE_SN_AST1_T) },
	{ SLAVE_LOOKUP(DCE_SN_CPU_T) }
};

static struct tegra_sn_addr_map tegra23x_rce_sn_lookup[] = {
	{ SLAVE_LOOKUP(RCE_SN_AXI2APB_1) },
	{ SLAVE_LOOKUP(RCE_SN_AST0_T) },
	{ SLAVE_LOOKUP(RCE_SN_AST1_T) },
	{ SLAVE_LOOKUP(RCE_SN_CPU_T) }
};

static struct tegra_sn_addr_map tegra23x_cbb_sn_lookup[] = {
	{ SLAVE_LOOKUP(CBB_SN_AON_SLAVE) },
	{ SLAVE_LOOKUP(CBB_SN_BPMP_SLAVE) },
	{ SLAVE_LOOKUP(CBB_SN_CBB_CENTRAL) },
	{ SLAVE_LOOKUP(CBB_SN_HOST1X) },
	{ SLAVE_LOOKUP(CBB_SN_STM) },
	{ SLAVE_LOOKUP(CBB_SN_FSI_SLAVE) },
	{ SLAVE_LOOKUP(CBB_SN_PSC_SLAVE) },
	{ SLAVE_LOOKUP(CBB_SN_PCIE_C1) },
	{ SLAVE_LOOKUP(CBB_SN_PCIE_C2) },
	{ SLAVE_LOOKUP(CBB_SN_PCIE_C3) },
	{ SLAVE_LOOKUP(CBB_SN_PCIE_C0) },
	{ SLAVE_LOOKUP(CBB_SN_PCIE_C4) },
	{ SLAVE_LOOKUP(CBB_SN_GPU) },
	{ SLAVE_LOOKUP(CBB_SN_SMMU0) },
	{ SLAVE_LOOKUP(CBB_SN_SMMU1) },
	{ SLAVE_LOOKUP(CBB_SN_SMMU2) },
	{ SLAVE_LOOKUP(CBB_SN_SMMU3) },
	{ SLAVE_LOOKUP(CBB_SN_SMMU4) },
	{ SLAVE_LOOKUP(CBB_SN_PCIE_C10) },
	{ SLAVE_LOOKUP(CBB_SN_PCIE_C7) },
	{ SLAVE_LOOKUP(CBB_SN_PCIE_C8) },
	{ SLAVE_LOOKUP(CBB_SN_PCIE_C9) },
	{ SLAVE_LOOKUP(CBB_SN_PCIE_C5) },
	{ SLAVE_LOOKUP(CBB_SN_PCIE_C6) },
	{ SLAVE_LOOKUP(CBB_SN_DCE_SLAVE) },
	{ SLAVE_LOOKUP(CBB_SN_RCE_SLAVE) },
	{ SLAVE_LOOKUP(CBB_SN_SCE_SLAVE) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_1) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_10) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_11) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_12) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_13) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_14) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_15) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_16) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_17) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_18) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_19) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_2) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_20) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_21) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_22) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_23) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_25) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_26) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_27) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_28) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_29) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_30) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_31) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_32) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_33) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_34) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_35) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_4) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_5) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_6) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_7) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_8) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_9) },
	{ SLAVE_LOOKUP(CBB_SN_AXI2APB_3) }
};
