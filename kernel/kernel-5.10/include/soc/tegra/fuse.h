/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2022, NVIDIA CORPORATION.  All rights reserved.
 */

#ifndef __SOC_TEGRA_FUSE_H__
#define __SOC_TEGRA_FUSE_H__

/* supported tegra chip id list */
#define TEGRA20		0x20
#define TEGRA30		0x30
#define TEGRA114	0x35
#define TEGRA148	0x14
#define TEGRA124	0x40
#define TEGRA132	0x13
#define TEGRA210	0x21
#define TEGRA186	0x18
#define TEGRA194	0x19
#define TEGRA234	0x23

/* production mode */
#define TEGRA_FUSE_PRODUCTION_MODE      0x0

/* control read/write calls for below offsets */
#define FUSE_FUSEBYPASS_0		0x24
#define FUSE_WRITE_ACCESS_SW_0		0x30

#define TEGRA_FUSE_SKU_CALIB_0	0xf0
#define TEGRA30_FUSE_SATA_CALIB	0x124

/* read/write calls for below offsets */
#define FUSE_GCPLEX_CONFIG_FUSE_0	0x1c8
#define FUSE_RESERVED_CALIB0_0		0x204
#define FUSE_OPT_GPU_TPC0_DISABLE_0	0x20c
#define FUSE_OPT_GPU_TPC1_DISABLE_0	0x23c

#define TEGRA_FUSE_USB_CALIB_EXT_0 0x250
#define FUSE_TDIODE_CALIB	0x274

/* T186+ */
#define FUSE_PDI0			0x300
#define FUSE_PDI1			0x304

#define FUSE_IP_DISABLE_0			0x4b0
#define FUSE_IP_DISABLE_0_NVLINK_MASK		0x10

#define FUSE_UCODE_MINION_REV_0			0x4d4
#define FUSE_UCODE_MINION_REV_0_MASK		0x7

#define FUSE_SECURE_MINION_DEBUG_DIS_0		0x4d8
#define FUSE_SECURE_MINION_DEBUG_DIS_0_MASK	0x1

#define TEGRA_FUSE_ODMID_0			0x308
#define TEGRA_FUSE_ODMID_1			0x30c
#define TEGRA_FUSE_ODM_INFO			0x19c

/* opt fuse offsets */
#if IS_ENABLED(CONFIG_ARCH_TEGRA_23x_SOC)
#define TEGRA_FUSE_OPT_CCPLEX_CLUSTER_DISABLE	0x214
#define TEGRA_FUSE_OPT_DLA_DISABLE		0x3f0
#define TEGRA_FUSE_OPT_EMC_DISABLE		0x8c0
#define TEGRA_FUSE_OPT_FBP_DISABLE		0xa70
#define TEGRA_FUSE_OPT_FSI_DISABLE		0x8c8
#define TEGRA_FUSE_OPT_GPC_DISABLE		0x188
#define TEGRA_FUSE_OPT_NVENC_DISABLE		0x3e0
#define TEGRA_FUSE_OPT_NVDEC_DISABLE		0x4f0
#define TEGRA_FUSE_OPT_PVA_DISABLE		0x3e8
#define TEGRA_FUSE_OPT_TPC_DISABLE		0x20c
#endif

#ifndef __ASSEMBLY__

#include <linux/of.h>

extern u32 tegra_read_chipid(void);
extern u8 tegra_get_chip_id(void);
u8 tegra_get_major_rev(void);
u8 tegra_get_minor_rev(void);
int tegra_miscreg_set_erd(u64 err_config);
u8 tegra_get_platform(void);
bool tegra_is_silicon(void);
int tegra194_miscreg_mask_serror(void);

enum tegra_revision {
	TEGRA_REVISION_UNKNOWN = 0,
	TEGRA_REVISION_A01,
	TEGRA_REVISION_A01q,
	TEGRA_REVISION_A02,
	TEGRA_REVISION_A02p,
	TEGRA_REVISION_A03,
	TEGRA_REVISION_A03p,
	TEGRA_REVISION_A04,
	TEGRA_REVISION_A04p,
	TEGRA210_REVISION_A01,
	TEGRA210_REVISION_A01q,
	TEGRA210_REVISION_A02,
	TEGRA210_REVISION_A02p,
	TEGRA210_REVISION_A03,
	TEGRA210_REVISION_A03p,
	TEGRA210_REVISION_A04,
	TEGRA210_REVISION_A04p,
	TEGRA210_REVISION_B01,
	TEGRA210B01_REVISION_A01,
	TEGRA186_REVISION_A01,
	TEGRA186_REVISION_A01q,
	TEGRA186_REVISION_A02,
	TEGRA186_REVISION_A02p,
	TEGRA186_REVISION_A03,
	TEGRA186_REVISION_A03p,
	TEGRA186_REVISION_A04,
	TEGRA186_REVISION_A04p,
	TEGRA194_REVISION_A01,
	TEGRA194_REVISION_A02,
	TEGRA194_REVISION_A02p,
	TEGRA_REVISION_QT,
	TEGRA_REVISION_SIM,
	TEGRA_REVISION_MAX,
};

enum tegra_ucm {
	TEGRA_UCM1 = 0,
	TEGRA_UCM2,
};

struct tegra_sku_info {
	int sku_id;
	int cpu_process_id;
	int cpu_speedo_id;
	int cpu_speedo_value;
	int cpu_iddq_value;
	int core_process_id;
	int soc_process_id;
	int soc_speedo_id;
	int soc_speedo_value;
	int soc_iddq_value;
	int gpu_process_id;
	int gpu_speedo_id;
	int gpu_iddq_value;
	int gpu_speedo_value;
	enum tegra_revision revision;
	enum tegra_ucm ucm;
	int speedo_rev;
};

u32 tegra_read_straps(void);
u32 tegra_read_ram_code(void);

int tegra_fuse_control_read(unsigned long offset, u32 *value);
void tegra_fuse_control_write(u32 value, unsigned long offset);

int tegra_fuse_readl(unsigned long offset, u32 *value);
void tegra_fuse_writel(u32 val, unsigned long offset);

extern struct tegra_sku_info tegra_sku_info;

struct device *tegra_soc_device_register(void);

/*
 * begin block - downstream declarations
 */

#define FUSE_OPT_FT_REV_0              0x28

#define FUSE_CP_REV			0x90
#define TEGRA_FUSE_CP_REV_0_3		(3)

#define TEGRA_FUSE_HAS_PLATFORM_APIS

extern int tegra_set_erd(u64 err_config);

extern struct tegra_sku_info tegra_sku_info;

extern enum tegra_revision tegra_revision;

extern u32 tegra_read_emu_revid(void);
extern enum tegra_revision tegra_chip_get_revision(void);
extern bool is_t210b01_sku(void);

/* check if in hypervisor mode */
bool is_tegra_hypervisor_mode(void);

/* check if safety build */
bool is_tegra_safety_build(void);

/* tegra-platform.c declarations */
extern bool tegra_cpu_is_asim(void);

extern bool tegra_platform_is_silicon(void);
extern bool tegra_platform_is_qt(void);
extern bool tegra_platform_is_fpga(void);
extern bool tegra_platform_is_vdk(void);
extern bool tegra_platform_is_sim(void);
extern bool tegra_platform_is_vsp(void);
/*
 * end block - downstream declarations
 */

#endif /* !__ASSEMBLY__ */
#endif /* __SOC_TEGRA_FUSE_H__ */
