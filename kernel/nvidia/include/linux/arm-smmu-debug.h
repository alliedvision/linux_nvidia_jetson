/*
 * Copyright (C) 2020-2021 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ARM_SMMU_DEBUG_H
#define _ARM_SMMU_DEBUG_H

struct arm_smmu_device;

/* Identification registers */
#define ARM_SMMU_GR0_nsCR0		0x400
#define ARM_SMMU_GR0_nsGFAR		0x440
#define ARM_SMMU_GR0_nsGFSR		0x448
#define ARM_SMMU_GR0_nsGFSYNR0		0x450
#define ARM_SMMU_GR0_nsGFSYNR1		0x454
#define ARM_SMMU_GR0_nsGFSYNR2		0x458
#define ARM_SMMU_GR0_PIDR0		0xfe0
#define ARM_SMMU_GR0_PIDR1		0xfe4
#define ARM_SMMU_GR0_PIDR2		0xfe8

/* Perf Monitor registers */
#define ARM_SMMU_GNSR0_PMCNTENSET_0	0xc00
#define ARM_SMMU_GNSR0_PMCNTENCLR_0	0xc20
#define ARM_SMMU_GNSR0_PMINTENSET_0	0xc40
#define ARM_SMMU_GNSR0_PMINTENCLR_0	0xc60
#define ARM_SMMU_GNSR0_PMOVSCLR_0	0xc80
#define ARM_SMMU_GNSR0_PMOVSSET_0	0xcc0
#define ARM_SMMU_GNSR0_PMCFGR_0		0xe00
#define ARM_SMMU_GNSR0_PMCR_0		0xe04
#define ARM_SMMU_GNSR0_PMCEID0_0	0xe20
#define ARM_SMMU_GNSR0_PMAUTHSTATUS_0	0xfb8
#define ARM_SMMU_GNSR0_PMDEVTYPE_0	0xfcc

#define ARM_SMMU_GNSR0_PMEVTYPER(n)	(0x400 + ((n) << 2))
#define ARM_SMMU_GNSR0_PMEVCNTR(n)	(0x0 + ((n) << 2))
#define ARM_SMMU_GNSR0_PMCGCR(n)	(0x800 + ((n) << 2))
#define ARM_SMMU_GNSR0_PMCGSMR(n)	(0xa00 + ((n) << 2))

/* Counter group registers */
#define PMCG_SIZE	32
/* Event Counter registers */
#define PMEV_SIZE	8

/* Global TLB invalidation */
#define ARM_SMMU_GR0_nsTLBGSYNC		0x470
#define ARM_SMMU_GR0_nsTLBGSTATUS	0x474

/* Context bank attribute registers */
#define ARM_SMMU_CB_FAR_LO		0x60
#define ARM_SMMU_CB_FAR_HI		0x64

/* Maximum number of context banks per SMMU */
#define ARM_SMMU_MAX_CBS		128

/* Maximum number of SMMU instances */
#define MAX_SMMUS			5

struct smmu_debugfs_master {
	struct device	*dev;
	s16		*smendx;
	struct dentry	*dent;
	struct list_head node;
	u16 streamid_mask;
};

struct smmu_debugfs_info {
	struct device	*dev;
	DECLARE_BITMAP(context_filter, ARM_SMMU_MAX_CBS);
	void __iomem	*base;
	void __iomem	*bases[MAX_SMMUS];
	int		size;
	u32		num_smmus;
	struct dentry	*debugfs_root;
	struct dentry	*cb_root;
	struct dentry	*masters_root;
	struct list_head masters_list;
	int		num_context_banks;
	unsigned long	pgshift;
	int		max_cbs;
	u16		streamid_mask;
	struct debugfs_regset32 *regset;
	struct debugfs_regset32 *perf_regset;
	struct debugfs_reg32 *reg;
	u8 debug_smmu_id;
};

void arm_smmu_debugfs_setup_bases(struct arm_smmu_device *smmu, u32 num_smmus,
				  void __iomem *bases[]);
void arm_smmu_debugfs_setup_cfg(struct arm_smmu_device *smmu);
void arm_smmu_debugfs_add_master(struct device *dev,
				 struct smmu_debugfs_info *smmu_dfs,
				 u8 *cbndx, u16 smendx[]);
void arm_smmu_debugfs_remove_master(struct device *dev,
				    struct smmu_debugfs_info *smmu_dfs);

#endif /* _ARM_SMMU_DEBUG_H */
