/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef NVGPU_ACR_INTERFACE_H
#define NVGPU_ACR_INTERFACE_H

/**
 * @defgroup NVGPURM_BLOB_CONSTRUCT blob construct
 *
 * Blob construct interfaces:
 * NVGPU creates LS ucode blob in system/FB's non-WPR memory. LS ucodes
 * will be read from filesystem and added to blob for the detected chip.
 * Below are the structs that need to be filled by NvGPU for each LS Falcon
 * ucode supported for the detected chip. After filling structures successfully,
 * NvGPU should copy below structs along with ucode to the non-WPR blob
 * in below mentioned pattern. LS ucodes blob is required by the ACR HS
 * ucode to authenticate & load LS ucode on to respective engine's LS Falcon.
 *
 * + WPR header struct #lsf_wpr_header.
 * + LSB header struct #lsf_lsb_header.
 * + Boot loader struct #flcn_bl_dmem_desc.
 * + ucode image.
 *
 * + BLOB Pattern:
 *    ---------------------------------------------
 *   | LSF WPR HDR  | LSF LSB HDR | BL desc | ucode |
 *    ---------------------------------------------
 */

/**
 * @ingroup NVGPURM_BLOB_CONSTRUCT
 */
/** @{*/

/**
 * Light Secure WPR Content Alignments
 */
/** WPR header should be aligned to 256 bytes */
#define LSF_WPR_HEADER_ALIGNMENT        (256U)
/** SUB WPR header should be aligned to 256 bytes */
#define LSF_SUB_WPR_HEADER_ALIGNMENT    (256U)
/** LSB header should be aligned to 256 bytes */
#define LSF_LSB_HEADER_ALIGNMENT        (256U)
/** BL DATA should be aligned to 256 bytes */
#define LSF_BL_DATA_ALIGNMENT           (256U)
/** BL DATA size should be aligned to 256 bytes */
#define LSF_BL_DATA_SIZE_ALIGNMENT      (256U)
/** BL CODE size should be aligned to 256 bytes */
#define LSF_BL_CODE_SIZE_ALIGNMENT      (256U)
/** LSF DATA size should be aligned to 256 bytes */
#define LSF_DATA_SIZE_ALIGNMENT         (256U)
/** LSF CODE size should be aligned to 256 bytes */
#define LSF_CODE_SIZE_ALIGNMENT         (256U)

/** UCODE surface should be aligned to 4k PAGE_SIZE */
#define LSF_UCODE_DATA_ALIGNMENT 4096U

/**
 * Maximum WPR Header size
 */
#define LSF_WPR_HEADERS_TOTAL_SIZE_MAX	\
	(ALIGN_UP(((u32)sizeof(struct lsf_wpr_header) * FALCON_ID_END), \
		LSF_WPR_HEADER_ALIGNMENT))
#define LSF_LSB_HEADER_TOTAL_SIZE_MAX	(\
	ALIGN_UP(sizeof(struct lsf_lsb_header), LSF_LSB_HEADER_ALIGNMENT))

/** @} */

#ifdef CONFIG_NVGPU_DGPU
/* Maximum SUB WPR header size */
#define LSF_SUB_WPR_HEADERS_TOTAL_SIZE_MAX	(ALIGN_UP( \
	(sizeof(struct lsf_shared_sub_wpr_header) * \
	LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_MAX), \
	LSF_SUB_WPR_HEADER_ALIGNMENT))

/* MMU excepts sub_wpr sizes in units of 4K */
#define SUB_WPR_SIZE_ALIGNMENT	(4096U)

/* Defined for 1MB alignment */
#define SHIFT_4KB	(12U)

/* shared sub_wpr use case IDs */
enum {
	LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_FRTS_VBIOS_TABLES	= 1,
	LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_PLAYREADY_SHARED_DATA = 2
};

#define LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_MAX \
	LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_PLAYREADY_SHARED_DATA

#define LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_INVALID	(0xFFFFFFFFU)

#define MAX_SUPPORTED_SHARED_SUB_WPR_USE_CASES	\
	LSF_SHARED_DATA_SUB_WPR_USE_CASE_ID_MAX

/* Static sizes of shared subWPRs */
/* Minimum granularity supported is 4K */
/* 1MB in 4K */
#define LSF_SHARED_DATA_SUB_WPR_FRTS_VBIOS_TABLES_SIZE_IN_4K	(0x100U)
/* 4K */
#define LSF_SHARED_DATA_SUB_WPR_PLAYREADY_SHARED_DATA_SIZE_IN_4K	(0x1U)
#endif

/**
 * @ingroup NVGPURM_BLOB_CONSTRUCT
 */
/** @{*/

/**
 * Image status updated by ACR HS ucode to know the LS
 * Falcon ucode status.
 */
/** IMAGE copied from NON-WPR to WPR BLOB*/
#define LSF_IMAGE_STATUS_COPY                           (1U)
/** LS Falcon ucode verification failed*/
#define LSF_IMAGE_STATUS_VALIDATION_CODE_FAILED         (2U)
/** LS Falcon data verification failed*/
#define LSF_IMAGE_STATUS_VALIDATION_DATA_FAILED         (3U)
/** Both ucode and data validation passed */
#define LSF_IMAGE_STATUS_VALIDATION_DONE                (4U)
/**
 * LS Falcons such as FECS and GPCCS does not have signatures for binaries in
 * debug environment(fmodel).
 */
#define LSF_IMAGE_STATUS_VALIDATION_SKIPPED             (5U)
/** LS Falcon validation passed & ready for bootstrap */
#define LSF_IMAGE_STATUS_BOOTSTRAP_READY                (6U)

/**
 * Light Secure WPR Header
 * Defines state allowing Light Secure Falcon bootstrapping.
 */
struct lsf_wpr_header {
	 /**
	 * LS Falcon ID
	 * FALCON_ID_FECS  - 2
	 * FALCON_ID_GPCCS - 3
	 */
	u32 falcon_id;
	/**
	 * LS Falcon LSB header offset from non-WPR base, below equation used
	 * to get LSB header offset for each managed LS falcon.
	 * Offset = Non-WPR base + #LSF_LSB_HEADER_ALIGNMENT +
	 *          ((#LSF_UCODE_DATA_ALIGNMENT + #LSF_BL_DATA_ALIGNMENT) *
	 *          LS Falcon index)
	 *
	 */
	u32 lsb_offset;
	/**
	 * LS Falcon bootstrap owner, which performs bootstrapping of
	 * supported LS Falcon from ACR HS ucode. Below are the bootstrapping
	 * supporting Falcon owners.
	 *  + Falcon #FALCON_ID_PMU
	 *
	 * On GV11B, bootstrap_owner set to #FALCON_ID_PMU as ACR HS ucode
	 * runs on PMU Engine Falcon.
	 *
	 */
	u32 bootstrap_owner;
	/**
	 * Skip bootstrapping by ACR HS ucode,
	 * 1 - skip LS Falcon bootstrapping by ACR HS ucode.
	 * 0 - LS Falcon bootstrapping is done by ACR HS ucode.
	 *
	 * On GV11B, always set 0.
	 */
	u32 lazy_bootstrap;
	/** LS ucode bin version*/
	u32 bin_version;
	/**
	 * Bootstrapping status updated by ACR HS ucode to know the LS
	 * Falcon ucode status.
	 */
	u32 status;
};

/** @} */

/**
 * @ingroup NVGPURM_BLOB_CONSTRUCT
 */
/** @{*/
/**
 * Size in entries of the ucode descriptor's dependency map.
 */
#define LSF_FALCON_DEPMAP_SIZE  (11U)

/**
 * Code/data signature details of LS falcon
 */
struct lsf_ucode_desc {
	/** ucode's production signature */
	u8  prd_keys[2][16];
	/** ucode's debug signature */
	u8  dbg_keys[2][16];
	/**
	 * production signature present status,
	 * 1 - production signature present
	 * 0 - production signature not present
	 */
	u32 b_prd_present;
	/**
	 * debug signature present
	 * 1 - debug signature present
	 * 0 - debug signature not present
	 */
	u32 b_dbg_present;
	/**
	 * LS Falcon ID
	 * FALCON_ID_FECS  - 2
	 * FALCON_ID_GPCCS - 3
	 */
	u32 falcon_id;
	/**
	 * include version in signature calculation if supported
	 * 1 - supported
	 * 0 - not supported
	 */
	u32 bsupports_versioning;
	/** version to include it in signature calculation if supported */
	u32 version;
	/** valid dependency map data to consider from  dep_map array member */
	u32 dep_map_count;
	/**
	 * packed dependency map used to compute the DM hashes on the code and
	 * data.
	 */
	u8  dep_map[LSF_FALCON_DEPMAP_SIZE * 2 * 4];
	/** Message used to derive key */
	u8  kdf[16];
};

/* PKC */
/*
 * Currently 2 components in ucode image have signature.
 * One is code section and another is data section.
 */
#define LSF_UCODE_COMPONENT_INDEX_CODE          (0)
#define LSF_UCODE_COMPONENT_INDEX_DATA          (1)
#define LSF_UCODE_COMPONENT_INDEX_MAX           (2)

/*
 * For PKC operation, SE engine needs each input component size to be 512 bytes
 * length. So even though PKC signature is 384 bytes, it needs to be padded with
 * zeros until size is 512.
 * Currently, for version 2, MAX LS signature size is set to be 512 as well.
 */
#define PKC_SIGNATURE_SIZE_BYTE               (384)
#define PKC_SIGNATURE_PADDING_SIZE_BYTE       (128)
#define PKC_SIGNATURE_PADDED_SIZE_BYTE		\
		(PKC_SIGNATURE_SIZE_BYTE + PKC_SIGNATURE_PADDING_SIZE_BYTE)

/* Size in bytes for RSA 3K (RSA_3K struct from bootrom_pkc_parameters.h */
#define PKC_PK_SIZE_BYTE                      (2048)

#define LSF_SIGNATURE_SIZE_PKC_BYTE		\
		(PKC_SIGNATURE_SIZE_BYTE + PKC_SIGNATURE_PADDING_SIZE_BYTE)
#define LSF_SIGNATURE_SIZE_MAX_BYTE              LSF_SIGNATURE_SIZE_PKC_BYTE
#define LSF_PK_SIZE_MAX                         PKC_PK_SIZE_BYTE

/* LS Encryption Defines */
#define LS_ENCRYPTION_AES_CBC_IV_SIZE_BYTE      (16)

/*!
 * WPR generic struct header
 * identifier - To identify type of WPR struct i.e. WPR vs SUBWPR vs LSB vs LSF_UCODE_DESC
 * version    - To specify version of struct, for backward compatibility
 * size       - Size of struct, include header and body
 */
struct wpr_generic_header {
	u16 identifier;
	u16 version;
	u32 size;
};

/*
 * Light Secure Falcon Ucode Version 2 Description Defines
 * This stucture is prelim and may change as the ucode signing flow evolves.
 */
struct lsf_ucode_desc_v2 {
	u32 falcon_id;  // lsenginedid
	u8  b_prd_present;
	u8  b_dbg_present;
	u16 reserved;
	u32 sig_size;
	u8  prod_sig[LSF_UCODE_COMPONENT_INDEX_MAX][LSF_SIGNATURE_SIZE_PKC_BYTE];
	u8  debug_sig[LSF_UCODE_COMPONENT_INDEX_MAX][LSF_SIGNATURE_SIZE_PKC_BYTE];
	u16 sig_algo_ver;
	u16 sig_algo;
	u16 hash_algo_ver;
	u16 hash_algo;
	u32 sig_algo_padding_type;
	u8  dep_map[LSF_FALCON_DEPMAP_SIZE * 8];
	u32 dep_map_count;
	u8  b_supports_versioning;
	u8  pad[3];
	u32 ls_ucode_version; // lsucodeversion
	u32 ls_ucode_id;      // lsucodeid
	u32 b_ucode_ls_encrypted;
	u32 ls_encalgo_type;
	u32 ls_enc_algo_ver;
	u8  ls_enc_iv[LS_ENCRYPTION_AES_CBC_IV_SIZE_BYTE];
	u8  rsvd[36];              // reserved for any future usage
};

/*
 * The wrapper for LSF_UCODE_DESC, start support from version 2.
 */
struct lsf_ucode_desc_wrapper {
	struct wpr_generic_header generic_hdr;

	union {
		struct lsf_ucode_desc_v2 lsf_ucode_desc_v2;
	};
};

/* PKC end*/

/** @} */

/**
 * @ingroup NVGPURM_BLOB_CONSTRUCT
 */
/** @{*/

/**
 * Light Secure Bootstrap Header
 * Defines state allowing Light Secure Falcon bootstrapping.
 */
/** Load BL at 0th IMEM offset */
#define NV_FLCN_ACR_LSF_FLAG_LOAD_CODE_AT_0_FALSE       0U
#define NV_FLCN_ACR_LSF_FLAG_LOAD_CODE_AT_0_TRUE        BIT32(0)
/** This falcon requires a ctx before issuing DMAs. */
#define NV_FLCN_ACR_LSF_FLAG_DMACTL_REQ_CTX_FALSE       0U
#define NV_FLCN_ACR_LSF_FLAG_DMACTL_REQ_CTX_TRUE        BIT32(2)
/** Use priv loading method instead of bootloader/DMAs */
#define NV_FLCN_ACR_LSF_FLAG_FORCE_PRIV_LOAD_TRUE       BIT32(3)
#define NV_FLCN_ACR_LSF_FLAG_FORCE_PRIV_LOAD_FALSE      (0U)
struct lsf_lsb_header {
	/** Code/data signature details of each LS falcon */
	struct lsf_ucode_desc signature;
	/**
	 * Offset from non-WPR base where UCODE is located,
	 * Offset = Non-WPR base + #LSF_LSB_HEADER_ALIGNMENT +
	 *          #LSF_UCODE_DATA_ALIGNMENT + ( #LSF_BL_DATA_ALIGNMENT *
	 *          LS Falcon index)
	 */
	u32 ucode_off;
	/**
	 * Size of LS Falcon ucode, required to perform signature verification
	 * of LS Falcon ucode by ACR HS.
	 */
	u32 ucode_size;
	/**
	 * Size of LS Falcon ucode data, required to perform signature
	 * verification of LS Falcon ucode data by ACR HS.
	 */
	u32 data_size;
	/**
	 * Size of bootloader that needs to be loaded by bootstrap owner.
	 *
	 * On GV11B, respective LS Falcon BL code size should not exceed
	 * below mentioned size.
	 * FALCON_ID_FECS IMEM size  - 32k
	 * FALCON_ID_GPCCS IMEM size - 16k
	 */
	u32 bl_code_size;
	/** BL starting virtual address. Need for tagging */
	u32 bl_imem_off;
	/**
	 * Offset from non-WPR base holding the BL data
	 * Offset = (Non-WPR base + #LSF_LSB_HEADER_ALIGNMENT +
	 *          #LSF_UCODE_DATA_ALIGNMENT + #LSF_BL_DATA_ALIGNMENT) *
	 *          #LS Falcon index
	 */
	u32 bl_data_off;
	/**
	 * Size of BL data, BL data will be copied to LS Falcon DMEM of
	 * bl data size
	 *
	 * On GV11B, respective LS Falcon BL data size should not exceed
	 * below mentioned size.
	 * FALCON_ID_FECS DMEM size  - 8k
	 * FALCON_ID_GPCCS DMEM size - 5k
	 */
	u32 bl_data_size;
	/**
	 * Offset from non-WPR base address where UCODE Application code is
	 * located.
	 */
	u32 app_code_off;
	/**
	 * Size of UCODE Application code.
	 *
	 * On GV11B, FECS/GPCCS LS Falcon app code size should not exceed
	 * below mentioned size.
	 * FALCON_ID_FECS IMEM size  - 32k
	 * FALCON_ID_GPCCS IMEM size - 16k
	 */
	u32 app_code_size;
	/**
	 * Offset from non-WPR base address where UCODE Application data
	 * is located
	 */
	u32 app_data_off;
	/**
	 * Size of UCODE Application data.
	 *
	 * On GV11B, respective LS Falcon app data size should not exceed
	 * below mentioned size.
	 * FALCON_ID_FECS DMEM size  - 8k
	 * FALCON_ID_GPCCS DMEM size - 5k
	 */
	u32 app_data_size;
	/**
	 * NV_FLCN_ACR_LSF_FLAG_LOAD_CODE_AT_0 - Load BL at 0th IMEM offset
	 * NV_FLCN_ACR_LSF_FLAG_DMACTL_REQ_CTX - This falcon requires a ctx
	 * before issuing DMAs.
	 * NV_FLCN_ACR_LSF_FLAG_FORCE_PRIV_LOAD - Use priv loading method
	 * instead of bootloader/DMAs
	 */
	u32 flags;
};

struct lsf_lsb_header_v2 {
	/** Code/data signature details of each LS falcon */
	struct lsf_ucode_desc_wrapper signature;
	/**
	 * Offset from non-WPR base where UCODE is located,
	 * Offset = Non-WPR base + #LSF_LSB_HEADER_ALIGNMENT +
	 *          #LSF_UCODE_DATA_ALIGNMENT + ( #LSF_BL_DATA_ALIGNMENT *
	 *          LS Falcon index)
	 */
	u32 ucode_off;
	/**
	 * Size of LS Falcon ucode, required to perform signature verification
	 * of LS Falcon ucode by ACR HS.
	 */
	u32 ucode_size;
	/**
	 * Size of LS Falcon ucode data, required to perform signature
	 * verification of LS Falcon ucode data by ACR HS.
	 */
	u32 data_size;
	/**
	 * Size of bootloader that needs to be loaded by bootstrap owner.
	 *
	 * On GV11B, respective LS Falcon BL code size should not exceed
	 * below mentioned size.
	 * FALCON_ID_FECS IMEM size  - 32k
	 * FALCON_ID_GPCCS IMEM size - 16k
	 */
	u32 bl_code_size;
	/** BL starting virtual address. Need for tagging */
	u32 bl_imem_off;
	/**
	 * Offset from non-WPR base holding the BL data
	 * Offset = (Non-WPR base + #LSF_LSB_HEADER_ALIGNMENT +
	 *          #LSF_UCODE_DATA_ALIGNMENT + #LSF_BL_DATA_ALIGNMENT) *
	 *          #LS Falcon index
	 */
	u32 bl_data_off;
	/**
	 * Size of BL data, BL data will be copied to LS Falcon DMEM of
	 * bl data size
	 *
	 * On GV11B, respective LS Falcon BL data size should not exceed
	 * below mentioned size.
	 * FALCON_ID_FECS DMEM size  - 8k
	 * FALCON_ID_GPCCS DMEM size - 5k
	 */
	u32 bl_data_size;
	/**
	 * Offset from non-WPR base address where UCODE Application code is
	 * located.
	 */
	u32 app_code_off;
	/**
	 * Size of UCODE Application code.
	 *
	 * On GV11B, FECS/GPCCS LS Falcon app code size should not exceed
	 * below mentioned size.
	 * FALCON_ID_FECS IMEM size  - 32k
	 * FALCON_ID_GPCCS IMEM size - 16k
	 */
	u32 app_code_size;
	/**
	 * Offset from non-WPR base address where UCODE Application data
	 * is located
	 */
	u32 app_data_off;
	/**
	 * Size of UCODE Application data.
	 *
	 * On GV11B, respective LS Falcon app data size should not exceed
	 * below mentioned size.
	 * FALCON_ID_FECS DMEM size  - 8k
	 * FALCON_ID_GPCCS DMEM size - 5k
	 */
	u32 app_data_size;
	/**
	 * NV_FLCN_ACR_LSF_FLAG_LOAD_CODE_AT_0 - Load BL at 0th IMEM offset
	 * NV_FLCN_ACR_LSF_FLAG_DMACTL_REQ_CTX - This falcon requires a ctx
	 * before issuing DMAs.
	 * NV_FLCN_ACR_LSF_FLAG_FORCE_PRIV_LOAD - Use priv loading method
	 * instead of bootloader/DMAs
	 */
	u32 flags;
};

#define FLCN_SIG_SIZE	(4U)

/** @} */

/**
 * @ingroup NVGPURM_BLOB_CONSTRUCT
 */
/** @{*/
/**
 * Structure used by the boot-loader to load the rest of the LS Falcon code.
 *
 * This has to be filled by the GPU driver and copied into WPR region offset
 * holding the BL data.
 */
struct flcn_bl_dmem_desc {
	/** Should be always first element */
	u32 reserved[FLCN_SIG_SIZE];
	/**
	 * Signature should follow reserved 16B signature for secure code.
	 * 0s if no secure code
	 */
	u32 signature[FLCN_SIG_SIZE];
	/**
	 * Type of memory-aperture DMA index used by the bootloader
	 * while loading code/data.
	 */
	u32 ctx_dma;
	/**
	 * 256B aligned physical sysmem(iGPU)/FB(dGPU) address where code
	 * is located.
	 */
	struct falc_u64 code_dma_base;
	/**
	 * Offset from code_dma_base where the nonSecure code is located.
	 * The offset must be multiple of 256 to help performance.
	 */
	u32 non_sec_code_off;
	/**
	 * The size of the non-secure code part.
	 *
	 * On GV11B, FECS/GPCCS LS Falcon non-secure + secure code size
	 * should not exceed below mentioned size.
	 * FALCON_ID_FECS IMEM size  - 32k
	 * FALCON_ID_GPCCS IMEM size - 16k
	 */
	u32 non_sec_code_size;
	/**
	 * Offset from code_dma_base where the secure code is located.
	 * The offset must be multiple of 256 to help performance.
	 */
	u32 sec_code_off;
	/**
	 * The size of the secure code part.
	 *
	 * On GV11B, FECS/GPCCS LS Falcon non-secure + secure code size
	 * should not exceed below mentioned size.
	 * FALCON_ID_FECS IMEM size  - 32k
	 * FALCON_ID_GPCCS IMEM size - 16k
	 */
	u32 sec_code_size;
	/**
	 * Code entry point which will be invoked by BL after code is
	 * loaded.
	 */
	u32 code_entry_point;
	/**
	 * 256B aligned Physical sysmem(iGPU)/FB(dGPU) Address where data
	 * is located.
	 */
	struct falc_u64 data_dma_base;
	/**
	 * Size of data block. Should be multiple of 256B.
	 *
	 * On GV11B, respective LS Falcon data size should not exceed
	 * below mentioned size.
	 * FALCON_ID_FECS DMEM size  - 8k
	 * FALCON_ID_GPCCS DMEM size - 5k
	 */
	u32 data_size;
	/** Arguments to be passed to the target firmware being loaded. */
	u32 argc;
	/**
	 * Number of arguments to be passed to the target firmware
	 * being loaded.
	 */
	u32 argv;
};

/** @} */

/**
 * @defgroup NVGPURM_ACR_HS_LOAD_BOOTSTRAP ACR HS ucode load & bootstrap
 *
 * ACR HS ucode load & bootstrap interfaces:
 * ACR HS ucode is read from the filesystem based on the chip-id by the ACR
 * unit. Read ACR HS ucode will be update with below structs by patching at
 * offset present in struct #struct acr_fw_header member hdr_offset. Read
 * ACR HS ucode is loaded onto PMU/SEC2/GSP engines Falcon to bootstrap
 * ACR HS ucode. ACR HS ucode does self-authentication using H/W based
 * HS authentication methodology. Once authenticated the ACR HS ucode
 * starts executing on the falcon.
 */

/**
 * @ingroup NVGPURM_ACR_HS_LOAD_BOOTSTRAP
 */
/** @{*/

/**
 * Supporting maximum of 2 regions.
 * This is needed to pre-allocate space in DMEM
 */
#define NVGPU_FLCN_ACR_MAX_REGIONS                (2U)
/** Reserve 512 bytes for bootstrap owner LS ucode data */
#define LSF_BOOTSTRAP_OWNER_RESERVED_DMEM_SIZE    (0x200U)

/**
 * The descriptor used by ACR HS ucode to figure out properties of individual
 * WPR regions.
 *
 * On GV11B, this struct members are set to 0x0 by default, reason
 * to fetch WPR1 details from H/W.
 */
struct flcn_acr_region_prop {
	/** Starting address of WPR region */
	u32 start_addr;
	/** Ending address of WPR region */
	u32 end_addr;
	/** The ID of the WPR region. 0 for WPR1 and 1 for WPR2  */
	u32 region_id;
	/** Read mask associated with this region */
	u32 read_mask;
	/** Write mask associated with this region */
	u32 write_mask;
	/** Bit map of all clients currently using this region */
	u32 client_mask;
	/**
	 * sysmem(iGPU)/FB(dGPU) location from where contents need to
	 * be copied to startAddress
	 */
	u32 shadowmMem_startaddress;
};

/**
 * The descriptor used by ACR HS ucode to figure out supporting regions &
 * its properties.
 */
struct flcn_acr_regions {
	/**
	 * Number of regions used by NVGPU from the total number of ACR
	 * regions supported in chip.
	 *
	 * On GV11B, 1 ACR region supported and should always be greater
	 * than 0.
	 */
	u32 no_regions;
	/** Region properties */
	struct flcn_acr_region_prop region_props[NVGPU_FLCN_ACR_MAX_REGIONS];
};

#define DMEM_WORD_SIZE		4U
#define DUMMY_SPACE_SIZE	4U
/**
 * The descriptor used by ACR HS ucode to figure out the
 * WPR & non-WPR blob details.
 */
struct flcn_acr_desc {
	/*
	 * The bootstrap owner needs to switch into LS mode when bootstrapping
	 * other LS Falcons is completed. It needs to have its own actual
	 * DMEM image copied into DMEM as part of LS setup. If ACR desc is
	 * at location 0, it will definitely get overwritten causing data
	 * corruption. Hence need to reserve 0x200 bytes to give room for
	 * any loading data.
	 * NOTE: This has to be the first member always.
	 */
	union {
		u32 reserved_dmem[(LSF_BOOTSTRAP_OWNER_RESERVED_DMEM_SIZE/DMEM_WORD_SIZE)];
	} ucode_reserved_space;
	/** Signature of ACR ucode. */
	u32 signatures[FLCN_SIG_SIZE];
	/**
	 * WPR Region ID holding the WPR header and its details
	 *
	 * on GV11B, wpr_region_id set to 0x0 by default to indicate
	 * to ACR HS ucode to fetch WPR region details from H/W &
	 * updating WPR start_addr, end_addr, read_mask & write_mask
	 * of struct #flcn_acr_region_prop.
	 */
	u32 wpr_region_id;
	/** Offset from the non-WPR base holding the wpr header */
	u32 wpr_offset;
	/** usable memory ranges, on GV11B it is not set */
	u32 mmu_mem_range;
	/**
	 * WPR Region descriptors to provide info about WPR.
	 * on GV11B, no_regions set to 1 & region properties value to 0x0
	 * to indicate to ACR HS ucode to fetch WPR region details from H/W.
	 */
	struct flcn_acr_regions regions;
	/**
	 * stores the size of the ucode blob.
	 *
	 * On GV11B, size is calculated at runtime & aligned to 256 bytes.
	 * Size varies based on number of LS falcon supports.
	 */
	u32 nonwpr_ucode_blob_size;
	/**
	 * stores sysmem(iGPU)/FB's(dGPU) non-WPR start address where
	 * kernel stores ucode blob
	 */
	u64 nonwpr_ucode_blob_start;
	/** dummy space, not used by iGPU */
	u32 dummy[DUMMY_SPACE_SIZE];
};

/* MIG mode selection*/
#define MIG_MODE                      BIT(8U)

/* Let ACR know when in simulation*/
#define ACR_SIMULATION_MODE           BIT(16U)

struct flcn2_acr_desc {
	/**
	 * WPR Region ID holding the WPR header and its details
	 *
	 * on GPUID_NEXT, wpr_region_id set to 0x0 by default to indicate
	 * to ACR HS ucode to fetch WPR region details from H/W &
	 * updating WPR start_addr, end_addr, read_mask & write_mask
	 * of struct #flcn_acr_region_prop.
	 */
	u32 wpr_region_id;
	/** Offset from the non-WPR base holding the wpr header */
	u32 wpr_offset;
	/**
	 * WPR Region descriptors to provide info about WPR.
	 * on GPUID_NEXT, no_regions set to 1 & region properties value to 0x0
	 * to indicate to ACR HS ucode to fetch WPR region details from H/W.
	 */
	struct flcn_acr_regions regions;
	/**
	 * stores the size of the ucode blob.
	 *
	 * On GPUID_NEXT, size is calculated at runtime & aligned to 256 bytes.
	 * Size varies based on number of LS falcon supports.
	 */
	u32 nonwpr_ucode_blob_size;
	/**
	 * stores sysmem(iGPU)/FB's(dGPU) non-WPR start address where
	 * kernel stores ucode blob
	 */
	u64 nonwpr_ucode_blob_start;

	u64 ls_pmu_desc;

	/**
	 * stores flag value to enable:
	 * emulate_mode       7:0  bit
	 * MIG mode          15:8  bit
	 * Simulation mode   23:16 bit
	 */
	u32 gpu_mode;
};

/** @} */

#endif /* NVGPU_ACR_INTERFACE_H */
