/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _TEGRA_VIRT_STORAGE_SPEC_H_
#define _TEGRA_VIRT_STORAGE_SPEC_H_

#include <linux/types.h> /* size_t */

#define VS_REQ_OP_F_NONE      0

enum vs_req_type {
	VS_DATA_REQ = 1,
	VS_CONFIGINFO_REQ = 2,
	VS_UNKNOWN_CMD = 0xffffffff,
};

enum vs_dev_type {
	VS_BLK_DEV = 1,
	VS_MTD_DEV = 2,
	VS_UNKNOWN_DEV = 0xffffffff,
};

enum mtd_cmd_op {
	VS_MTD_READ = 1,
	VS_MTD_WRITE = 2,
	VS_MTD_ERASE = 3,
	VS_MTD_IOCTL = 4,
	VS_MTD_INVAL_REQ = 32,
	VS_UNKNOWN_MTD_CMD = 0xffffffff,
};

/* MTD device request Operation type features supported */
#define VS_MTD_READ_OP_F          (1 << VS_MTD_READ)
#define VS_MTD_WRITE_OP_F         (1 << VS_MTD_WRITE)
#define VS_MTD_ERASE_OP_F         (1 << VS_MTD_ERASE)
#define VS_MTD_IOCTL_OP_F         (1 << VS_MTD_IOCTL)
#define VS_MTD_READ_ONLY_MASK     ~(VS_MTD_READ_OP_F)

enum blk_cmd_op {
	VS_BLK_READ = 1,
	VS_BLK_WRITE = 2,
	VS_BLK_FLUSH = 3,
	VS_BLK_DISCARD = 4,
	VS_BLK_SECURE_ERASE = 5,
	VS_BLK_IOCTL = 6,
	VS_BLK_INVAL_REQ = 32,
	VS_UNKNOWN_BLK_CMD = 0xffffffff,
};

/* Blk device request Operation type features supported */
#define VS_BLK_READ_OP_F          (1 << VS_BLK_READ)
#define VS_BLK_WRITE_OP_F         (1 << VS_BLK_WRITE)
#define VS_BLK_FLUSH_OP_F         (1 << VS_BLK_FLUSH)
#define VS_BLK_DISCARD_OP_F       (1 << VS_BLK_DISCARD)
#define VS_BLK_SECURE_ERASE_OP_F  (1 << VS_BLK_SECURE_ERASE)
#define VS_BLK_IOCTL_OP_F         (1 << VS_BLK_IOCTL)
#define VS_BLK_READ_ONLY_MASK     ~(VS_BLK_READ_OP_F)

#pragma pack(push)
#pragma pack(1)

struct vs_blk_request {
	uint64_t blk_offset;		/* Offset into storage device in terms
						of blocks for block device */
	uint32_t num_blks;		/* Total Block number to transfer */
	uint32_t data_offset;		/* Offset into mempool for data region
						*/
	/* IOVA address of the buffer. In case of read request, VSC will get
	 * the response to this address. In case of write request, VSC will
	 * get the data from this address.
	 */
	uint64_t iova_addr;
};

struct vs_mtd_request {
	uint64_t offset;		/* Offset into storage device in terms
						of bytes in case of mtd device */
	uint32_t size;			/* Total number of bytes to transfer
						  to be used for MTD device */
	uint32_t data_offset;		/* Offset into mempool for data region
						*/
};

struct vs_ioctl_request {
	uint32_t ioctl_id;		/* Id of the ioctl */
	uint32_t ioctl_len;		/* Length of the mempool area associated
						with ioctl */
	uint32_t data_offset;		/* Offset into mempool for data region
						*/
};

struct vs_blkdev_request {
	enum blk_cmd_op req_op;
	union {
		struct vs_blk_request blk_req;
		struct vs_ioctl_request ioctl_req;
	};
};

struct vs_mtddev_request {
	enum mtd_cmd_op req_op;
	union {
		struct vs_mtd_request mtd_req;
		struct vs_ioctl_request ioctl_req;
	};
};

struct vs_blk_response {
	int32_t status;			/* 0 for success, < 0 for error */
	uint32_t num_blks;
};

struct vs_mtd_response {
	int32_t status;			/* 0 for success, < 0 for error */
	uint32_t size;			/* Number of bytes processed in case of
						of mtd device*/
};

struct vs_ioctl_response {
	int32_t status;			/* 0 for success, < 0 for error */
};

struct vs_blkdev_response {
	union {
		struct vs_blk_response blk_resp;
		struct vs_ioctl_response ioctl_resp;
	};
};

struct vs_mtddev_response {
	union {
		struct vs_mtd_response mtd_resp;
		struct vs_ioctl_response ioctl_resp;
	};
};

struct vs_blk_dev_config {
	uint32_t hardblk_size;		/* Block Size */
	uint32_t max_read_blks_per_io;	/* Limit number of Blocks
						per I/O*/
	uint32_t max_write_blks_per_io; /* Limit number of Blocks
					   per I/O*/
	uint32_t max_erase_blks_per_io; /* Limit number of Blocks per I/O */
	uint32_t req_ops_supported;	/* Allowed operations by requests */
	uint64_t num_blks;		/* Total number of blks */

	/*
	 * If true, then VM need to provide local IOVA address for read and
	 * write requests. For IOCTL requests, mempool will be used
	 * irrespective of this flag.
	 */
	uint32_t use_vm_address;
};

struct vs_mtd_dev_config {
	uint32_t max_read_bytes_per_io;	/* Limit number of bytes
						per I/O */
	uint32_t max_write_bytes_per_io; /* Limit number of bytes
					   per I/O */
	uint32_t erase_size;		/* Erase size for mtd
					   device*/
	uint32_t req_ops_supported;	/* Allowed operations by requests */
	uint64_t size;			/* Total number of bytes */
};

/* Physical device types */
#define VSC_DEV_EMMC	1U
#define VSC_DEV_UFS	2U
#define VSC_DEV_QSPI	3U

/* Storage Types */
#define VSC_STORAGE_RPMB	1U
#define VSC_STORAGE_BOOT	2U
#define VSC_STORAGE_LUN0	3U
#define VSC_STORAGE_LUN1	4U
#define VSC_STORAGE_LUN2	5U
#define VSC_STORAGE_LUN3	6U
#define VSC_STORAGE_LUN4	7U
#define VSC_STORAGE_LUN5	8U
#define VSC_STORAGE_LUN6	9U
#define VSC_STORAGE_LUN7	10U

#define SPEED_MODE_MAX_LEN	32

struct vs_config_info {
	uint32_t virtual_storage_ver;		/* Version of virtual storage */
	enum vs_dev_type type;			/* Type of underlying device */
	union {
		struct vs_blk_dev_config blk_config;
		struct vs_mtd_dev_config mtd_config;
	};
	uint32_t phys_dev;
	uint32_t phys_base;
	uint32_t storage_type;
	uint8_t speed_mode[SPEED_MODE_MAX_LEN];
};

struct vs_request {
	uint32_t req_id;
	enum vs_req_type type;
	union {
		struct vs_blkdev_request blkdev_req;
		struct vs_mtddev_request mtddev_req;
	};
	int32_t status;
	union {
		struct vs_blkdev_response blkdev_resp;
		struct vs_mtddev_response mtddev_resp;
		struct vs_config_info config_info;
	};
};

/**
 * @addtogroup MMC_RESP MMC Responses
 *
 * @brief Defines Command Responses of EMMC
 */
typedef enum {
	/** @brief No Response */
	RESP_TYPE_NO_RESP = 0U,
	/** @brief Response Type 1 */
	RESP_TYPE_R1 = 1U,
	/** @brief Response Type 2 */
	RESP_TYPE_R2 = 2U,
	/** @brief Response Type 3 */
	RESP_TYPE_R3 = 3U,
	/** @brief Response Type 4 */
	RESP_TYPE_R4 = 4U,
	/** @brief Response Type 5 */
	RESP_TYPE_R5 = 5U,
	/** @brief Response Type 6 */
	RESP_TYPE_R6 = 6U,
	/** @brief Response Type 7 */
	RESP_TYPE_R7 = 7U,
	/** @brief Response Type 1B */
	RESP_TYPE_R1B = 8U,
	/** @brief Number of Response Type */
	RESP_TYPE_NUM = 9U
	/* @} */
} sdmmc_resp_type;

#define VBLK_MMC_MULTI_IOC_ID 0x1000
struct combo_cmd_t {
	uint32_t cmd;
	uint32_t arg;
	uint32_t write_flag;
	uint32_t response[4];
	uint32_t buf_offset;
	uint32_t data_len;
	sdmmc_resp_type flags;
};

struct combo_info_t {
	uint32_t count;
	int32_t  result;
};

/* SCSI bio layer needs to handle SCSI and UFS IOCTL separately
 * This flag will be ORed with IO_IOCTL to find out difference
 * between SCSI and UFS IOCTL
 */
#define SCSI_IOCTL_FLAG	0x10000000
#define UFS_IOCTL_FLAG	0x20000000
/* Mask for SCSI and UFS ioctl flags, 4 MSB (bits) reserved for it Two LSB
 * bits are used for SCSI and UFS, 2 MSB bits reserved for future use.
 */
#define SCSI_UFS_IOCTL_FLAG_MASK 0xF0000000

#define VBLK_SG_IO_ID	(0x1001 | SCSI_IOCTL_FLAG)
#define VBLK_UFS_IO_ID	(0x1002 | UFS_IOCTL_FLAG)
#define VBLK_UFS_COMBO_IO_ID	(0x1003 | UFS_IOCTL_FLAG)

#define VBLK_SG_MAX_CMD_LEN 16

enum scsi_data_direction {
        SCSI_BIDIRECTIONAL = 0,
        SCSI_TO_DEVICE = 1,
        SCSI_FROM_DEVICE = 2,
        SCSI_DATA_NONE = 3,
	UNKNOWN_DIRECTION = 0xffffffff,
};

struct vblk_sg_io_hdr
{
    int32_t data_direction;     /* [i] data transfer direction  */
    uint8_t cmd_len;            /* [i] SCSI command length */
    uint8_t mx_sb_len;          /* [i] max length to write to sbp */
    uint32_t dxfer_len;         /* [i] byte count of data transfer */
    uint32_t xfer_arg_offset;  /* [i], [*io] offset to data transfer memory */
    uint32_t cmdp_arg_offset;   /* [i], [*i] offset to command to perform */
    uint32_t sbp_arg_offset;    /* [i], [*o] offset to sense_buffer memory */
    uint32_t status;            /* [o] scsi status */
    uint8_t sb_len_wr;          /* [o] byte count actually written to sbp */
    uint32_t dxfer_buf_len;     /* [i] Length of data transfer buffer */
};

struct vblk_ufs_ioc_query_req {
	/* Query opcode to specify the type of Query operation */
	uint8_t opcode;
	/* idn to provide more info on specific operation. */
	uint8_t idn;
	/* index - optional in some cases */
	uint8_t index;
	/* index - optional in some cases */
	uint8_t selector;
	/* buf_size - buffer size in bytes pointed by buffer.
	 * Note:
	 * For Read/Write Attribute this should be of 4 bytes
	 * For Read Flag this should be of 1 byte
	 * For Descriptor Read/Write size depends on the type of the descriptor
	 */
	uint16_t buf_size;
	/*
	 * User buffer offset for query data. The offset should be within the
	 * bounds of the mempool memory region.
	 */
	uint32_t buffer_offset;
	/* Delay after each query command completion in micro seconds. */
	uint32_t delay;
	/* error status for the query operation */
	int32_t error_status;

};

/** @brief Meta data of UFS Native ioctl Combo Command */
typedef struct vblk_ufs_combo_info {
	/** Count of commands in combo command */
	uint32_t count;
	/** Status of combo command */
	int32_t result;
	/** Flag to specify whether to empty the command queue before
	  * processing the combo request.
	  * If user wants to ensure that there are no requests in the UFS device
	  * command queue before executing a query command, this flag has to be
	  * set to 1.
	  * For Example, in case of refresh for Samsung UFS Device, the
	  * command queue should be emptied before setting the attribute for
	  * refresh.
	  */
	uint8_t need_cq_empty;
}vblk_ufs_combo_info_t;

#pragma pack(pop)

#endif
