/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 */

static char *tegra_grace_master_id[] = {
	"TZ",                           /* 0x0  */
	"CCPLEX",                       /* 0x1  */
	"CCPMU",                        /* 0x2  */
	"BPMP_FW",                      /* 0x3  */
	"PSC_FW_USER",                  /* 0x4  */
	"PSC_FW_SUPERVISOR",            /* 0x5  */
	"PSC_FW_MACHINE",               /* 0x6  */
	"PSC_BOOT",                     /* 0x7  */
	"BPMP_BOOT",                    /* 0x8  */
	"JTAGM_DFT",                    /* 0x9  */
	"CORESIGHT",                    /* 0xA  */
	"GPU",                          /* 0xB  */
	"PEATRANS",                     /* 0xC  */
	"RSVD"                          /* 0x3F */
};

/*
 * Possible causes for Slave and Timeout errors.
 * SLAVE_ERR:
 * Slave being accessed responded with an error. Slave could return
 * an error for various cases :
 *   Unsupported access, clamp setting when power gated, register
 *   level firewall(SCR), address hole within the slave, etc
 *
 * TIMEOUT_ERR:
 * No response returned by slave. Can be due to slave being clock
 * gated, under reset, powered down or slave inability to respond
 * for an internal slave issue
 */

static struct tegra_noc_errors tegra_grace_errmon_errors[] = {
	{.errcode = "SLAVE_ERR",
	 .type = "Slave being accessed responded with an error."
	},
	{.errcode = "DECODE_ERR",
	 .type = "Attempt to access an address hole or Reserved region of memory."
	},
	{.errcode = "FIREWALL_ERR",
	 .type = "Attempt to access a region which is firewalled."
	},
	{.errcode = "TIMEOUT_ERR",
	 .type = "No response returned by slave."
	},
	{.errcode = "PWRDOWN_ERR",
	 .type = "Attempt to access a portion of the fabric that is powered down."
	},
	{.errcode = "UNSUPPORTED_ERR",
	 .type = "Attempt to access a slave through an unsupported access."
	},
	{.errcode = "POISON_ERR",
	 .type = "Slave responds with poison error to indicate error in data."
	},
	{.errcode = "RSVD"},
	{.errcode = "RSVD"},
	{.errcode = "RSVD"},
	{.errcode = "RSVD"},
	{.errcode = "RSVD"},
	{.errcode = "RSVD"},
	{.errcode = "RSVD"},
	{.errcode = "RSVD"},
	{.errcode = "RSVD"},
	{.errcode = "NO_SUCH_ADDRESS_ERR",
	 .type = "The address belongs to the pri_target range but there is no register implemented at the address."
	},
	{.errcode = "TASK_ERR",
	 .type = "Attempt to update a PRI task when the current task has still not completed."
	},
	{.errcode = "EXTERNAL_ERR",
	 .type = "Indicates that an external PRI register access met with an error due to any issue in the unit."
	},
	{.errcode = "INDEX_ERR",
	 .type = "Applicable to PRI index aperture pair, when the programmed index is outside the range defined in the manual."
	},
	{.errcode = "RESET_ERR",
	 .type = "Target in Reset Error: Attempt to access a SubPri or external PRI register but they are in reset."
	},
	{.errcode = "REGISTER_RST_ERR",
	 .type = "Attempt to access a PRI register but the register is partial or completely in reset."
	},
	{.errcode = "POWER_GATED_ERR",
	 .type = "Returned by external PRI client when the external access goes to a power gated domain."
	},
	{.errcode = "SUBPRI_FS_ERR",
	 .type = "Subpri is floorswept: Attempt to access a subpri through the main pri target but subPri logic is floorswept."
	},
	{.errcode = "SUBPRI_CLK_OFF_ERR",
	 .type = "Subpri clock is off: Attempt to access a subpri through the main pri target but subPris clock is gated/off."
	}
};

#define REQ_SOCKET_ID		GENMASK(27, 24)

#define GRACE_BPMP_SN_AXI2APB_1_BASE	0x00000
#define GRACE_BPMP_SN_CBB_T_BASE		0x15000
#define GRACE_BPMP_SN_CPU_T_BASE		0x16000
#define GRACE_BPMP_SN_DBB0_T_BASE	0x17000
#define GRACE_BPMP_SN_DBB1_T_BASE	0x18000

#define GRACE_CBB_SN_CCPLEX_SLAVE_BASE 	0x50000
#define GRACE_CBB_SN_PCIE_C8_BASE	0x51000
#define GRACE_CBB_SN_PCIE_C9_BASE	0x52000
#define GRACE_CBB_SN_PCIE_C4_BASE	0x53000
#define GRACE_CBB_SN_PCIE_C5_BASE	0x54000
#define GRACE_CBB_SN_PCIE_C6_BASE	0x55000
#define GRACE_CBB_SN_PCIE_C7_BASE	0x56000
#define GRACE_CBB_SN_PCIE_C2_BASE	0x57000
#define GRACE_CBB_SN_PCIE_C3_BASE	0x58000
#define GRACE_CBB_SN_PCIE_C0_BASE	0x59000
#define GRACE_CBB_SN_PCIE_C1_BASE	0x5A000
#define GRACE_CBB_SN_AON_SLAVE_BASE	0x5B000
#define GRACE_CBB_SN_BPMP_SLAVE_BASE	0x5C000
#define GRACE_CBB_SN_PSC_SLAVE_BASE	0x5D000
#define GRACE_CBB_SN_STM_BASE		0x5E000
#define GRACE_CBB_SN_AXI2APB_1_BASE	0x70000
#define GRACE_CBB_SN_AXI2APB_10_BASE	0x71000
#define GRACE_CBB_SN_AXI2APB_11_BASE	0x72000
#define GRACE_CBB_SN_AXI2APB_12_BASE	0x73000
#define GRACE_CBB_SN_AXI2APB_13_BASE	0x74000
#define GRACE_CBB_SN_AXI2APB_14_BASE	0x75000
#define GRACE_CBB_SN_AXI2APB_15_BASE	0x76000
#define GRACE_CBB_SN_AXI2APB_16_BASE	0x77000
#define GRACE_CBB_SN_AXI2APB_17_BASE	0x78000
#define GRACE_CBB_SN_AXI2APB_18_BASE	0x79000
#define GRACE_CBB_SN_AXI2APB_19_BASE	0x7A000
#define GRACE_CBB_SN_AXI2APB_2_BASE	0x7B000
#define GRACE_CBB_SN_AXI2APB_20_BASE	0x7C000
#define GRACE_CBB_SN_AXI2APB_21_BASE	0x7D000
#define GRACE_CBB_SN_AXI2APB_22_BASE	0x7E000
#define GRACE_CBB_SN_AXI2APB_23_BASE	0x7F000
#define GRACE_CBB_SN_AXI2APB_24_BASE	0x80000
#define GRACE_CBB_SN_AXI2APB_25_BASE	0x81000
#define GRACE_CBB_SN_AXI2APB_26_BASE	0x82000
#define GRACE_CBB_SN_AXI2APB_27_BASE	0x83000
#define GRACE_CBB_SN_AXI2APB_28_BASE	0x84000
#define GRACE_CBB_SN_AXI2APB_29_BASE	0x85000
#define GRACE_CBB_SN_AXI2APB_30_BASE	0x86000
#define GRACE_CBB_SN_AXI2APB_4_BASE	0x87000
#define GRACE_CBB_SN_AXI2APB_5_BASE	0x88000
#define GRACE_CBB_SN_AXI2APB_6_BASE	0x89000
#define GRACE_CBB_SN_AXI2APB_7_BASE	0x8A000
#define GRACE_CBB_SN_AXI2APB_8_BASE	0x8B000
#define GRACE_CBB_SN_AXI2APB_9_BASE	0x8C000
#define GRACE_CBB_SN_AXI2APB_3_BASE	0x8D000

#define SLAVE_LOOKUP(sn) {#sn, sn}

static struct tegra_sn_addr_map tegra_grace_bpmp_sn_lookup[] = {
	{ "RSVD", 0 },
	{ "RSVD", 0 },
	SLAVE_LOOKUP(GRACE_BPMP_SN_CBB_T_BASE),
	SLAVE_LOOKUP(GRACE_BPMP_SN_CPU_T_BASE),
	SLAVE_LOOKUP(GRACE_BPMP_SN_AXI2APB_1_BASE),
	SLAVE_LOOKUP(GRACE_BPMP_SN_DBB0_T_BASE),
	SLAVE_LOOKUP(GRACE_BPMP_SN_DBB1_T_BASE)
};

static struct tegra_sn_addr_map tegra_grace_cbb_sn_lookup[] = {
	SLAVE_LOOKUP(GRACE_CBB_SN_PCIE_C8_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_PCIE_C9_BASE),
	{ "RSVD", 0 },
	{ "RSVD", 0 },
	{ "RSVD", 0 },
	{ "RSVD", 0 },
	{ "RSVD", 0 },
	{ "RSVD", 0 },
	{ "RSVD", 0 },
	{ "RSVD", 0 },
	SLAVE_LOOKUP(GRACE_CBB_SN_AON_SLAVE_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_BPMP_SLAVE_BASE),
	{ "RSVD", 0 },
	{ "RSVD", 0 },
	SLAVE_LOOKUP(GRACE_CBB_SN_PSC_SLAVE_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_STM_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_1_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_10_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_11_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_12_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_13_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_14_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_15_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_16_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_17_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_18_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_19_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_2_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_20_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_4_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_5_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_6_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_7_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_8_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_9_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_3_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_21_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_22_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_23_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_24_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_25_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_26_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_27_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_28_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_PCIE_C4_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_PCIE_C5_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_PCIE_C6_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_PCIE_C7_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_PCIE_C2_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_PCIE_C3_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_PCIE_C0_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_PCIE_C1_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_CCPLEX_SLAVE_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_29_BASE),
	SLAVE_LOOKUP(GRACE_CBB_SN_AXI2APB_30_BASE)
};
