/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 */

#define  ERRLOGGER_0_ID_COREID_0            0x00000000
#define  ERRLOGGER_0_ID_REVISIONID_0        0x00000004
#define  ERRLOGGER_0_FAULTEN_0              0x00000008
#define  ERRLOGGER_0_ERRVLD_0               0x0000000c
#define  ERRLOGGER_0_ERRCLR_0               0x00000010
#define  ERRLOGGER_0_ERRLOG0_0              0x00000014
#define  ERRLOGGER_0_ERRLOG1_0              0x00000018
#define  ERRLOGGER_0_RSVD_00_0              0x0000001c
#define  ERRLOGGER_0_ERRLOG3_0              0x00000020
#define  ERRLOGGER_0_ERRLOG4_0              0x00000024
#define  ERRLOGGER_0_ERRLOG5_0              0x00000028
#define  ERRLOGGER_0_STALLEN_0              0x00000038

#define  ERRLOGGER_1_ID_COREID_0            0x00000080
#define  ERRLOGGER_1_ID_REVISIONID_0        0x00000084
#define  ERRLOGGER_1_FAULTEN_0              0x00000088
#define  ERRLOGGER_1_ERRVLD_0               0x0000008c
#define  ERRLOGGER_1_ERRCLR_0               0x00000090
#define  ERRLOGGER_1_ERRLOG0_0              0x00000094
#define  ERRLOGGER_1_ERRLOG1_0              0x00000098
#define  ERRLOGGER_1_RSVD_00_0              0x0000009c
#define  ERRLOGGER_1_ERRLOG3_0              0x000000A0
#define  ERRLOGGER_1_ERRLOG4_0              0x000000A4
#define  ERRLOGGER_1_ERRLOG5_0              0x000000A8
#define  ERRLOGGER_1_STALLEN_0              0x000000b8

#define  ERRLOGGER_2_ID_COREID_0            0x00000100
#define  ERRLOGGER_2_ID_REVISIONID_0        0x00000104
#define  ERRLOGGER_2_FAULTEN_0              0x00000108
#define  ERRLOGGER_2_ERRVLD_0               0x0000010c
#define  ERRLOGGER_2_ERRCLR_0               0x00000110
#define  ERRLOGGER_2_ERRLOG0_0              0x00000114
#define  ERRLOGGER_2_ERRLOG1_0              0x00000118
#define  ERRLOGGER_2_RSVD_00_0              0x0000011c
#define  ERRLOGGER_2_ERRLOG3_0              0x00000120
#define  ERRLOGGER_2_ERRLOG4_0              0x00000124
#define  ERRLOGGER_2_ERRLOG5_0              0x00000128
#define  ERRLOGGER_2_STALLEN_0              0x00000138


#define CBB_NOC_INITFLOW GENMASK(23, 20)
#define CBB_NOC_TARGFLOW GENMASK(19, 16)
#define CBB_NOC_TARG_SUBRANGE GENMASK(15, 9)
#define CBB_NOC_SEQID GENMASK(8, 0)

#define BPMP_NOC_INITFLOW GENMASK(20, 18)
#define BPMP_NOC_TARGFLOW GENMASK(17, 13)
#define BPMP_NOC_TARG_SUBRANGE GENMASK(12, 9)
#define BPMP_NOC_SEQID GENMASK(8, 0)

#define AON_NOC_INITFLOW GENMASK(22, 21)
#define AON_NOC_TARGFLOW GENMASK(20, 15)
#define AON_NOC_TARG_SUBRANGE GENMASK(14, 9)
#define AON_NOC_SEQID GENMASK(8, 0)

#define SCE_NOC_INITFLOW GENMASK(21, 19)
#define SCE_NOC_TARGFLOW GENMASK(18, 14)
#define SCE_NOC_TARG_SUBRANGE GENMASK(13, 9)
#define SCE_NOC_SEQID GENMASK(8, 0)

#define CBB_NOC_AXCACHE GENMASK(3, 0)
#define CBB_NOC_NON_MOD GENMASK(4, 4)
#define CBB_NOC_AXPROT GENMASK(7, 5)
#define CBB_NOC_FALCONSEC GENMASK(9, 8)
#define CBB_NOC_GRPSEC GENMASK(16, 10)
#define CBB_NOC_VQC GENMASK(18, 17)
#define CBB_NOC_MSTR_ID GENMASK(22, 19)
#define CBB_NOC_AXI_ID GENMASK(30, 23)

#define CLUSTER_NOC_AXCACHE GENMASK(3, 0)
#define CLUSTER_NOC_AXPROT GENMASK(6, 4)
#define CLUSTER_NOC_FALCONSEC GENMASK(8, 7)
#define CLUSTER_NOC_GRPSEC GENMASK(15, 9)
#define CLUSTER_NOC_VQC GENMASK(17, 16)
#define CLUSTER_NOC_MSTR_ID GENMASK(21, 18)

#define USRBITS_MSTR_ID GENMASK(21, 18)

#define CBB_ERR_OPC GENMASK(4, 1)
#define CBB_ERR_ERRCODE GENMASK(10, 8)
#define CBB_ERR_LEN1 GENMASK(27, 16)

#define DMAAPB_X_RAW_INTERRUPT_STATUS   0x2ec

struct tegra194_cbb_packet_header {
	bool lock;   // [0]
	u8   opc;    // [4:1]
	u8   errcode;// [10:8]= RD, RDW, RDL, RDX, WR, WRW, WRC, PRE, URG
	u16  len1;   // [27:16]
	bool format; // [31]  = 1 -> FlexNoC versions 2.7 & above
};

struct tegra194_cbb_aperture {
	u8  initflow;
	u8  targflow;
	u8  targ_subrange;
	u8  init_mapping;
	u32 init_localaddress;
	u8  targ_mapping;
	u32 targ_localaddress;
	u16 seqid;
};

struct tegra194_cbb_userbits {
	u8  axcache;
	u8  non_mod;
	u8  axprot;
	u8  falconsec;
	u8  grpsec;
	u8  vqc;
	u8  mstr_id;
	u8  axi_id;
};

struct tegra_cbb_noc_data {
	char *name;
	bool erd_mask_inband_err;
	const char **cbb_master_id;
	int  max_aperture;
	const struct tegra194_cbb_aperture *noc_aperture;
	char **noc_routeid_initflow;
	char **noc_routeid_targflow;
	void (*noc_parse_routeid)(struct tegra194_cbb_aperture *info, u64 routeid);
	void (*noc_parse_userbits)(struct tegra194_cbb_userbits *usrbits, u32 elog_5);
};

struct tegra_cbb_errlog_record {
	struct list_head node;
	char *name;
	phys_addr_t start;
	void __iomem *vaddr;
	int num_intr;
	int sec_irq;
	int nonsec_irq;
	u32 errlog0;
	u32 errlog1;
	u32 errlog2;
	u32 errlog3;
	u32 errlog4;
	u32 errlog5;
	int  apb_bridge_cnt;
	void __iomem **axi2abp_bases;
	bool is_ax2apb_bridge_connected;
	bool erd_mask_inband_err;
	const char **cbb_master_id;
	struct tegra_cbb *cbb;
	int  max_aperture;
	const struct tegra194_cbb_aperture *noc_aperture;
	char **noc_routeid_initflow;
	char **noc_routeid_targflow;
	void (*noc_parse_routeid)(struct tegra194_cbb_aperture *info, u64 routeid);
	void (*noc_parse_userbits)(struct tegra194_cbb_userbits *usrbits, u32 elog_5);
};
