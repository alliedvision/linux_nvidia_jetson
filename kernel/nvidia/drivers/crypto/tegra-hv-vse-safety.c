/*
 * Cryptographic API.
 * drivers/crypto/tegra-hv-vse-safety.c
 *
 * Support for Tegra Virtual Security Engine hardware crypto algorithms.
 *
 * Copyright (c) 2019-2022, NVIDIA Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <crypto/scatterwalk.h>
#include <crypto/algapi.h>
#include <crypto/skcipher.h>
#include <crypto/internal/rng.h>
#include <crypto/internal/hash.h>
#include <crypto/internal/aead.h>
#include <crypto/internal/skcipher.h>
#include <crypto/sha.h>
#include <crypto/sha3.h>
#include <linux/delay.h>
#include <linux/tegra-ivc.h>
#include <linux/iommu.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/version.h>

#define TEGRA_HV_VSE_SHA_MAX_LL_NUM_1				1
#define TEGRA_HV_VSE_AES_CMAC_MAX_LL_NUM			1
#define TEGRA_HV_VSE_MAX_TASKS_PER_SUBMIT			1
#define TEGRA_HV_VSE_TIMEOUT			(msecs_to_jiffies(10000))
#define TEGRA_HV_VSE_SHA_MAX_BLOCK_SIZE				128
#define TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE				16
#define TEGRA_VIRTUAL_SE_AES_GCM_TAG_SIZE			16
#define TEGRA_VIRTUAL_SE_AES_MIN_KEY_SIZE			16
#define TEGRA_VIRTUAL_SE_AES_MAX_KEY_SIZE			32
#define TEGRA_VIRTUAL_SE_AES_IV_SIZE				16
#define TEGRA_VIRTUAL_SE_AES_GCM_IV_SIZE			12
#define TEGRA_VIRTUAL_SE_AES_MAX_IV_SIZE			TEGRA_VIRTUAL_SE_AES_IV_SIZE

#define TEGRA_VIRTUAL_SE_CMD_AES_SET_KEY			0xF1
#define TEGRA_VIRTUAL_SE_CMD_AES_ALLOC_KEY			0xF0
#define TEGRA_VIRTUAL_SE_CMD_AES_ENCRYPT_INIT		0x20
#define TEGRA_VIRTUAL_SE_CMD_AES_ENCRYPT			0x21
#define TEGRA_VIRTUAL_SE_CMD_AES_DECRYPT			0x22
#define TEGRA_VIRTUAL_SE_CMD_AES_CMAC				0x23
#define TEGRA_VIRTUAL_SE_CMD_AES_CMAC_GEN_SUBKEY	0x24
#define TEGRA_VIRTUAL_SE_CMD_AES_RNG_DBRG			0x25
#define TEGRA_VIRTUAL_SE_CMD_AES_GCM_CMD_ENCRYPT	0x27
#define TEGRA_VIRTUAL_SE_CMD_AES_GCM_CMD_DECRYPT	0x28
#define TEGRA_VIRTUAL_SE_CMD_AES_CMAC_SIGN			0x32
#define TEGRA_VIRTUAL_SE_CMD_AES_CMAC_VERIFY		0x33

#define TEGRA_VIRTUAL_SE_CMD_AES_GMAC_CMD_INIT			(0x29U)
#define TEGRA_VIRTUAL_SE_CMD_AES_GMAC_CMD_SIGN			(0x30U)
#define TEGRA_VIRTUAL_SE_CMD_AES_GMAC_CMD_VERIFY		(0x31U)

#define TEGRA_VIRTUAL_SE_AES_GMAC_SV_CFG_FIRST_REQ_SHIFT	(0x00U)
#define TEGRA_VIRTUAL_SE_AES_GMAC_SV_CFG_LAST_REQ_SHIFT		(0x01U)

#define TEGRA_VIRTUAL_SE_CMD_SHA_HASH				16
#define TEGRA_VIRTUAL_SE_SHA_HASH_BLOCK_SIZE_512BIT		(512 / 8)
#define TEGRA_VIRTUAL_SE_SHA_HASH_BLOCK_SIZE_576BIT		(576 / 8)
#define TEGRA_VIRTUAL_SE_SHA_HASH_BLOCK_SIZE_832BIT		(832 / 8)
#define TEGRA_VIRTUAL_SE_SHA_HASH_BLOCK_SIZE_1024BIT	(1024 / 8)
#define TEGRA_VIRTUAL_SE_SHA_HASH_BLOCK_SIZE_1088BIT	(1088 / 8)
#define TEGRA_VIRTUAL_SE_SHA_HASH_BLOCK_SIZE_1344BIT	(1344 / 8)

#define SHA3_STATE_SIZE	200

#define TEGRA_VIRTUAL_SE_TIMEOUT_1S				1000000

#define TEGRA_VIRTUAL_SE_AES_CMAC_DIGEST_SIZE			16

#define TEGRA_VIRTUAL_SE_AES_CMAC_STATE_SIZE			16

#define TEGRA_VIRTUAL_SE_MAX_BUFFER_SIZE			0x1000000

#define TEGRA_VIRTUAL_SE_AES_KEYTBL_TYPE_KEY			1
#define TEGRA_VIRTUAL_SE_AES_KEYTBL_TYPE_OIV			2
#define TEGRA_VIRTUAL_SE_AES_KEYTBL_TYPE_UIV			4

#define TEGRA_VIRTUAL_SE_AES_KEYSLOT_LABEL_SIZE		16
#define TEGRA_VIRTUAL_SE_AES_KEYSLOT_LABEL			"NVSEAES"

#define TEGRA_VIRTUAL_SE_AES_LCTR_SIZE				16
#define TEGRA_VIRTUAL_SE_AES_LCTR_CNTN				1

#define TEGRA_VIRTUAL_SE_AES_CMAC_CONFIG_NONLASTBLK		0x00
#define TEGRA_VIRTUAL_SE_AES_CMAC_CONFIG_LASTBLK		0x01
#define TEGRA_VIRTUAL_SE_AES_CMAC_CONFIG_FINAL			0x02

#define TEGRA_VIRTUAL_SE_AES_CMAC_SV_CONFIG_FIRSTREQ	0x01
#define TEGRA_VIRTUAL_SE_AES_CMAC_SV_CONFIG_LASTREQ		0x02

#define TEGRA_VIRTUAL_SE_RNG_IV_SIZE	16
#define TEGRA_VIRTUAL_SE_RNG_DT_SIZE	16
#define TEGRA_VIRTUAL_SE_RNG_KEY_SIZE	16
#define TEGRA_VIRTUAL_SE_RNG_SEED_SIZE (TEGRA_VIRTUAL_SE_RNG_IV_SIZE + \
					TEGRA_VIRTUAL_SE_RNG_KEY_SIZE + \
					TEGRA_VIRTUAL_SE_RNG_DT_SIZE)

#define TEGRA_VIRTUAL_SE_MAX_SUPPORTED_BUFLEN		((1U << 24) - 1U)
#define TEGRA_VIRTUAL_SE_MAX_GCMDEC_BUFLEN			(0x500000U)		/* 5 MB	*/

#define TEGRA_VIRTUAL_SE_ERR_MAC_INVALID	11

static struct task_struct *tegra_vse_task;
static bool vse_thread_start;

/* Security Engine Linked List */
struct tegra_virtual_se_ll {
	dma_addr_t addr; /* DMA buffer address */
	u32 data_len; /* Data length in DMA buffer */
};

struct tegra_vse_tag {
	unsigned int *priv_data;
};

/* Tegra Virtual Security Engine commands */
enum tegra_virtual_se_command {
	VIRTUAL_SE_AES_CRYPTO,
	VIRTUAL_SE_KEY_SLOT,
	VIRTUAL_SE_PROCESS,
	VIRTUAL_CMAC_PROCESS,
	VIRTUAL_SE_AES_GCM_ENC_PROCESS,
};

/* CMAC response */
struct tegra_vse_cmac_data {
	u8 status;
	u8 data[TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE];
};

enum cmac_request_type {
	CMAC_SIGN = 0U,
	CMAC_VERIFY
};

/* CMAC request data */
struct tegra_vse_cmac_req_data {
	enum cmac_request_type request_type;
	/* For CMAC_VERIFY tag comparison result */
	uint8_t result;
};

enum gmac_request_type {
	GMAC_INIT = 0U,
	GMAC_SIGN,
	GMAC_VERIFY
};

/* GMAC request data */
struct tegra_vse_gmac_req_data {
	enum gmac_request_type request_type;
	/* Return IV after GMAC_INIT and pass IV during GMAC_VERIFY*/
	unsigned char *iv;
	bool is_first;
	/* For GMAC_VERIFY tag comparison result */
	uint8_t result;
};

struct tegra_vse_soc_info {
	bool cmac_hw_padding_supported;
	bool gcm_decrypt_supported;
};

struct tegra_vse_priv_data {
	struct skcipher_request *req;
	struct tegra_virtual_se_dev *se_dev;
	struct completion alg_complete;
	int cmd;
	int slot_num;
	struct scatterlist sg;
	void *buf;
	dma_addr_t buf_addr;
	u32 rx_status;
	u8 iv[TEGRA_VIRTUAL_SE_AES_MAX_IV_SIZE];
	struct tegra_vse_cmac_data cmac;
};

struct tegra_virtual_se_dev {
	struct device *dev;
	struct mutex mtx;
	/* Engine id */
	unsigned int engine_id;
	/* Engine suspend state */
	atomic_t se_suspended;
	/* Mutex lock for SE server */
	struct mutex server_lock;
	struct tegra_vse_soc_info *chipdata;
	atomic_t mempoolbuf_in_use;
};

struct tegra_virtual_se_addr {
	u32 lo;
	u32 hi;
};

union tegra_virtual_se_aes_args {
	struct keyiv {
		u32 slot;
		u32 length;
		u32 type;
		u8 data[32];
		u8 oiv[TEGRA_VIRTUAL_SE_AES_IV_SIZE];
		u8 uiv[TEGRA_VIRTUAL_SE_AES_IV_SIZE];
	} key;
	struct aes_encdec {
		u32 keyslot;
		u32 mode;
		u32 ivsel;
		u8 lctr[TEGRA_VIRTUAL_SE_AES_LCTR_SIZE];
		u32 ctr_cntn;
		struct tegra_virtual_se_addr src_addr;
		struct tegra_virtual_se_addr dst_addr;
		u32 key_length;
	} op;
	struct aes_cmac_subkey_s {
		u32 keyslot;
		u32 key_length;
	} op_cmac_subkey_s;
	struct aes_gcm {

		/**
		 * keyslot handle returned by TOS as part of load key operation.
		 * It must be the first variable in the structure.
		 */
		uint32_t keyslot;

		uint32_t dst_addr_lo;
		uint32_t dst_addr_hi;
		uint32_t src_addr_lo;
		uint32_t src_addr_hi;

		uint32_t aad_addr_lo;
		uint32_t aad_addr_hi;

		uint32_t tag_addr_lo;
		uint32_t tag_addr_hi;

		/* TODO: ESLC-6207: use lctr instead*/
		uint8_t iv[12];
		/**
		 * Key length in bytes.
		 *
		 * Supported key length is 16 bytes
		 */
		uint32_t key_length;
		/* Config for AES-GMAC request */
		uint32_t config;
		u8 expected_tag[TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE];
	} op_gcm;
	struct aes_cmac_s {
		u32 keyslot;
		u32 ivsel;
		u32 config;
		u32 lastblock_len;
		u8 lastblock[TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE];
		u8 cmac_reg[TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE];
		u64 dst;
		struct tegra_virtual_se_addr src_addr;
		u32 key_length;
	} op_cmac_s;
	struct aes_cmac_sv {
		u32 keyslot;
		u32 config;
		u32 lastblock_len;
		u8 lastblock[TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE];
		struct tegra_virtual_se_addr src_addr;
		u32 key_length;
		u8 cmac_result[TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE];
	} op_cmac_sv;
	struct aes_rng {
		struct tegra_virtual_se_addr dst_addr;
	} op_rng;
} __attribute__((__packed__));

union tegra_virtual_se_sha_args {
	struct hash {
		u32 msg_total_length[4];
		u32 msg_left_length[4];
		u32 hash[50];
		u64 dst;
		struct tegra_virtual_se_addr src_addr;
		u32 mode;
		u32 hash_length;
	} op_hash;
} __attribute__((__packed__));

struct tegra_virtual_se_ivc_resp_msg_t {
	u32 tag;
	u32 cmd;
	u32 status;
	union {
		/** The init vector of AES-CBC encryption */
		unsigned char iv[TEGRA_VIRTUAL_SE_AES_IV_SIZE];
		/** Hash result for AES CMAC */
		unsigned char cmac_result[TEGRA_VIRTUAL_SE_AES_CMAC_DIGEST_SIZE];
		/** Keyslot for non */
		unsigned char keyslot;
	};
};

struct tegra_virtual_se_ivc_tx_msg_t {
	u32 tag;
	u32 cmd;
	union {
		union tegra_virtual_se_aes_args aes;
		union tegra_virtual_se_sha_args sha;
	};
};

struct tegra_virtual_se_ivc_hdr_t {
	u8 header_magic[4];
	u32 num_reqs;
	u32 engine;
	u8 tag[0x10];
	u32 status;
};

struct tegra_virtual_se_ivc_msg_t {
	struct tegra_virtual_se_ivc_hdr_t ivc_hdr;
	union {
		struct tegra_virtual_se_ivc_tx_msg_t tx[TEGRA_HV_VSE_MAX_TASKS_PER_SUBMIT];
		struct tegra_virtual_se_ivc_resp_msg_t rx[TEGRA_HV_VSE_MAX_TASKS_PER_SUBMIT];
	};
};

/* Security Engine SHA context */
struct tegra_virtual_se_sha_context {
	/* Security Engine device */
	struct tegra_virtual_se_dev *se_dev;
	/* SHA operation mode */
	u32 op_mode;
	unsigned int digest_size;
	u8 mode;
};

struct sha_zero_length_vector {
	unsigned int size;
	char *digest;
};

/* Tegra Virtual Security Engine operation modes */
enum tegra_virtual_se_op_mode {
	/* Secure Hash Algorithm-1 (SHA1) mode */
	VIRTUAL_SE_OP_MODE_SHA1,
	/* Secure Hash Algorithm-224  (SHA224) mode */
	VIRTUAL_SE_OP_MODE_SHA224 = 4,
	/* Secure Hash Algorithm-256  (SHA256) mode */
	VIRTUAL_SE_OP_MODE_SHA256,
	/* Secure Hash Algorithm-384  (SHA384) mode */
	VIRTUAL_SE_OP_MODE_SHA384,
	/* Secure Hash Algorithm-512  (SHA512) mode */
	VIRTUAL_SE_OP_MODE_SHA512,
	/* Secure Hash Algorithm-3  (SHA3-256) mode */
	VIRTUAL_SE_OP_MODE_SHA3_256 = 10,
	/* Secure Hash Algorithm-3  (SHA3-384) mode */
	VIRTUAL_SE_OP_MODE_SHA3_384,
	/* Secure Hash Algorithm-3  (SHA3-512) mode */
	VIRTUAL_SE_OP_MODE_SHA3_512,
	/* Secure Hash Algorithm-3  (SHAKE128) mode */
	VIRTUAL_SE_OP_MODE_SHAKE128,
	/* Secure Hash Algorithm-3  (SHAKE256) mode */
	VIRTUAL_SE_OP_MODE_SHAKE256,
};

/* Security Engine AES context */
struct tegra_virtual_se_aes_context {
	/* Security Engine device */
	struct tegra_virtual_se_dev *se_dev;
	struct skcipher_request *req;
	/* Security Engine key slot */
	u32 aes_keyslot;
	/* key length in bytes */
	u32 keylen;
	/* AES operation mode */
	u32 op_mode;
	/* Is key slot */
	bool is_key_slot_allocated;
	/* size of GCM tag*/
	u32 authsize;
};

enum tegra_virtual_se_aes_op_mode {
	AES_CBC,
	AES_ECB,
	AES_CTR,
};

/* Security Engine request context */
struct tegra_virtual_se_aes_req_context {
	/* Security Engine device */
	struct tegra_virtual_se_dev *se_dev;
	/* Security Engine operation mode */
	enum tegra_virtual_se_aes_op_mode op_mode;
	/* Operation type */
	bool encrypt;
	/* Engine id */
	u8 engine_id;
};

/* Security Engine request context */
struct tegra_virtual_se_req_context {
	/* Security Engine device */
	struct tegra_virtual_se_dev *se_dev;
	unsigned int digest_size;
	unsigned int intermediate_digest_size;
	u8 mode;			/* SHA operation mode */
	u8 *sha_buf;			/* Buffer to store residual data */
	dma_addr_t sha_buf_addr;	/* DMA address to residual data */
	u8 *hash_result;		/* Intermediate hash result */
	dma_addr_t hash_result_addr;	/* Intermediate hash result dma addr */
	u64 total_count;		/* Total bytes in all the requests */
	u32 residual_bytes;		/* Residual byte count */
	u32 blk_size;			/* SHA block size */
	bool is_first;			/* Represents first block */
	bool req_context_initialized;	/* Mark initialization status */
	bool force_align;		/* Enforce buffer alignment */
};

/* Security Engine random number generator context */
struct tegra_virtual_se_rng_context {
	/* Security Engine device */
	struct tegra_virtual_se_dev *se_dev;
	/* RNG buffer pointer */
	u32 *rng_buf;
	/* RNG buffer dma address */
	dma_addr_t rng_buf_adr;
};

/* Security Engine AES CMAC context */
struct tegra_virtual_se_aes_cmac_context {
	unsigned int digest_size;
	u8 *hash_result;		/* Intermediate hash result */
	dma_addr_t hash_result_addr;	/* Intermediate hash result dma addr */
	bool is_first;			/* Represents first block */
	bool req_context_initialized;	/* Mark initialization status */
	u32 aes_keyslot;
	/* key length in bits */
	u32 keylen;
	bool is_key_slot_allocated;
};

/* Security Engine AES GMAC context */
struct tegra_virtual_se_aes_gmac_context {
	/* size of GCM tag*/
	u32 authsize;
	/* Mark initialization status */
	bool req_context_initialized;
	u32 aes_keyslot;
	/* key length in bits */
	u32 keylen;
	bool is_key_slot_allocated;
};

enum se_engine_id {
	VIRTUAL_SE_AES0,
	VIRTUAL_SE_AES1,
	VIRTUAL_SE_SHA=3,
	VIRTUAL_MAX_SE_ENGINE_NUM=5
};

enum tegra_virtual_se_aes_iv_type {
	AES_ORIGINAL_IV,
	AES_UPDATED_IV,
	AES_IV_REG
};

/* Lock for IVC channel */
static DEFINE_MUTEX(se_ivc_lock);

static struct tegra_hv_ivc_cookie *g_ivck;
static struct tegra_hv_ivm_cookie *g_ivmk;
static void *mempool_buf;
static struct tegra_virtual_se_dev *g_virtual_se_dev[VIRTUAL_MAX_SE_ENGINE_NUM];
static struct completion tegra_vse_complete;

static int tegra_hv_vse_safety_send_ivc(
	struct tegra_virtual_se_dev *se_dev,
	struct tegra_hv_ivc_cookie *pivck,
	void *pbuf,
	int length)
{
	u32 timeout;
	int err = 0;

	timeout = TEGRA_VIRTUAL_SE_TIMEOUT_1S;
	mutex_lock(&se_ivc_lock);
	while (tegra_hv_ivc_channel_notified(pivck) != 0) {
		if (!timeout) {
			mutex_unlock(&se_ivc_lock);
			dev_err(se_dev->dev, "ivc reset timeout\n");
			return -EINVAL;
		}
		udelay(1);
		timeout--;
	}

	timeout = TEGRA_VIRTUAL_SE_TIMEOUT_1S;
	while (tegra_hv_ivc_can_write(pivck) == 0) {
		if (!timeout) {
			mutex_unlock(&se_ivc_lock);
			dev_err(se_dev->dev, "ivc send message timeout\n");
			return -EINVAL;
		}
		udelay(1);
		timeout--;
	}

	if (length > sizeof(struct tegra_virtual_se_ivc_msg_t)) {
		mutex_unlock(&se_ivc_lock);
		dev_err(se_dev->dev,
				"Wrong write msg len %d\n", length);
		return -E2BIG;
	}

	err = tegra_hv_ivc_write(pivck, pbuf, length);
	if (err < 0) {
		mutex_unlock(&se_ivc_lock);
		dev_err(se_dev->dev, "ivc write error!!! error=%d\n", err);
		return err;
	}
	mutex_unlock(&se_ivc_lock);
	return 0;
}

static int tegra_hv_vse_safety_prepare_ivc_linked_list(
	struct tegra_virtual_se_dev *se_dev, struct scatterlist *sg,
	u32 total_len, int max_ll_len, int block_size,
	struct tegra_virtual_se_addr *src_addr,
	int *num_lists, enum dma_data_direction dir,
	unsigned int *num_mapped_sgs)
{
	struct scatterlist *src_sg;
	int err = 0;
	int sg_count = 0;
	int i = 0;
	int len, process_len;
	u32 addr, addr_offset;

	src_sg = sg;
	while (src_sg && total_len) {
		err = dma_map_sg(se_dev->dev, src_sg, 1, dir);
		if (!err) {
			dev_err(se_dev->dev, "dma_map_sg() error\n");
			err = -EINVAL;
			goto exit;
		}
		sg_count++;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
		len = min(src_sg->length, (size_t)total_len);
#else
		len = min(src_sg->length, total_len);
#endif
		addr = sg_dma_address(src_sg);
		addr_offset = 0;
		while (len >= TEGRA_VIRTUAL_SE_MAX_BUFFER_SIZE) {
			process_len = TEGRA_VIRTUAL_SE_MAX_BUFFER_SIZE -
				block_size;
			if (i > max_ll_len) {
				dev_err(se_dev->dev,
					"Unsupported no. of list %d\n", i);
				err = -EINVAL;
				goto exit;
			}
			src_addr[i].lo = addr + addr_offset;
			src_addr[i].hi = process_len;
			i++;
			addr_offset += process_len;
			total_len -= process_len;
			len -= process_len;
		}
		if (len) {
			if (i > max_ll_len) {
				dev_err(se_dev->dev,
					"Unsupported no. of list %d\n", i);
				err = -EINVAL;
				goto exit;
			}
			src_addr[i].lo = addr + addr_offset;
			src_addr[i].hi = len;
			i++;
		}
		total_len -= len;
		src_sg = sg_next(src_sg);
	}
	*num_lists = (i + *num_lists);
	*num_mapped_sgs = sg_count;

	return 0;
exit:
	src_sg = sg;
	while (src_sg && sg_count--) {
		dma_unmap_sg(se_dev->dev, src_sg, 1, dir);
		src_sg = sg_next(src_sg);
	}
	*num_mapped_sgs = 0;

	return err;
}

static int tegra_hv_vse_safety_count_sgs(struct scatterlist *sl, u32 nbytes)
{
	struct scatterlist *sg = sl;
	int sg_nents = 0;

	while (sg) {
		sg = sg_next(sg);
		sg_nents++;
	}

	return sg_nents;
}

static int tegra_hv_vse_safety_send_sha_data(struct tegra_virtual_se_dev *se_dev,
				struct ahash_request *req,
				struct tegra_virtual_se_ivc_msg_t *ivc_req_msg,
				u32 count, bool islast)
{
	struct tegra_virtual_se_ivc_tx_msg_t *ivc_tx = NULL;
	struct tegra_virtual_se_ivc_hdr_t *ivc_hdr = NULL;
	struct tegra_hv_ivc_cookie *pivck = g_ivck;
	struct tegra_vse_priv_data *priv = NULL;
	struct tegra_virtual_se_req_context *req_ctx;
	struct tegra_vse_tag *priv_data_ptr;
	union tegra_virtual_se_sha_args *psha = NULL;
	int time_left;
	int err = 0;
	u64 total_count = 0, msg_len = 0;

	if (!req) {
		dev_err(se_dev->dev, "SHA request not valid\n");
		return -EINVAL;
	}

	if (!ivc_req_msg) {
		dev_err(se_dev->dev,
			"%s Invalid ivc_req_msg\n", __func__);
		return -EINVAL;
	}

	priv = devm_kzalloc(se_dev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	req_ctx = ahash_request_ctx(req);
	total_count = req_ctx->total_count;

	ivc_tx = &ivc_req_msg->tx[0];
	ivc_hdr = &ivc_req_msg->ivc_hdr;
	ivc_hdr->engine = VIRTUAL_SE_SHA;
	ivc_tx->cmd = TEGRA_VIRTUAL_SE_CMD_SHA_HASH;

	psha = &(ivc_tx->sha);
	psha->op_hash.mode = req_ctx->mode;
	psha->op_hash.msg_total_length[0] = count;
	psha->op_hash.msg_total_length[1] = 0;
	psha->op_hash.msg_total_length[2] = 0;
	psha->op_hash.msg_total_length[3] = 0;
	psha->op_hash.msg_left_length[0] = count;
	psha->op_hash.msg_left_length[1] = 0;
	psha->op_hash.msg_left_length[2] = 0;
	psha->op_hash.msg_left_length[3] = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	psha->op_hash.hash_length = req->dst_size;
#endif
	if (islast) {
		psha->op_hash.msg_total_length[0] = total_count & 0xFFFFFFFF;
		psha->op_hash.msg_total_length[1] = total_count >> 32;
	} else {
		msg_len = count + 8;
		psha->op_hash.msg_left_length[0] = msg_len & 0xFFFFFFFF;
		psha->op_hash.msg_left_length[1] = msg_len >> 32;

		if (req_ctx->is_first) {
			psha->op_hash.msg_total_length[0] = msg_len & 0xFFFFFFFF;
			psha->op_hash.msg_total_length[1] = msg_len >> 32;
			req_ctx->is_first = false;
		} else {
			msg_len += 8;
			psha->op_hash.msg_total_length[0] = msg_len & 0xFFFFFFFF;
			psha->op_hash.msg_total_length[1] = msg_len >> 32;
		}
	}

	ivc_hdr->header_magic[0] = 'N';
	ivc_hdr->header_magic[1] = 'V';
	ivc_hdr->header_magic[2] = 'D';
	ivc_hdr->header_magic[3] = 'A';
	ivc_hdr->num_reqs = 1;
	priv_data_ptr = (struct tegra_vse_tag *)ivc_req_msg->ivc_hdr.tag;
	priv_data_ptr->priv_data = (unsigned int *)priv;
	priv->cmd = VIRTUAL_SE_PROCESS;
	priv->se_dev = se_dev;

	vse_thread_start = true;
	init_completion(&priv->alg_complete);

	mutex_lock(&se_dev->server_lock);
	/* Return error if engine is in suspended state */
	if (atomic_read(&se_dev->se_suspended)) {
		err = -ENODEV;
		goto exit;
	}

	err = tegra_hv_vse_safety_send_ivc(se_dev, pivck, ivc_req_msg,
			sizeof(struct tegra_virtual_se_ivc_msg_t));
	if (err)
		goto exit;

	time_left = wait_for_completion_timeout(&priv->alg_complete,
			TEGRA_HV_VSE_TIMEOUT);
	if (time_left == 0) {
		dev_err(se_dev->dev, "%s timeout\n", __func__);
		err = -ETIMEDOUT;
	}
exit:
	mutex_unlock(&se_dev->server_lock);
	devm_kfree(se_dev->dev, priv);

	return err;
}

static int tegra_hv_vse_safety_sha_send_one(struct ahash_request *req,
				u32 nbytes, bool islast)
{
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_SHA];
	struct tegra_virtual_se_ivc_msg_t *ivc_req_msg;
	struct tegra_virtual_se_ivc_tx_msg_t *ivc_tx = NULL;
	struct tegra_virtual_se_req_context *req_ctx = ahash_request_ctx(req);
	int err = 0;

	ivc_req_msg = devm_kzalloc(se_dev->dev, sizeof(*ivc_req_msg),
			GFP_KERNEL);
	if (!ivc_req_msg)
		return -ENOMEM;

	if (islast == true &&
			(req_ctx->mode == VIRTUAL_SE_OP_MODE_SHAKE128 ||
			req_ctx->mode == VIRTUAL_SE_OP_MODE_SHAKE256)) {
		req_ctx->sha_buf[nbytes] = 0xff;
		nbytes++;
		req_ctx->total_count++;
	}
	ivc_tx = &ivc_req_msg->tx[0];

	ivc_tx->sha.op_hash.src_addr.lo = req_ctx->sha_buf_addr;
	ivc_tx->sha.op_hash.src_addr.hi = nbytes;

	ivc_tx->sha.op_hash.dst = (u64)req_ctx->hash_result_addr;
	memcpy(ivc_tx->sha.op_hash.hash, req_ctx->hash_result,
		req_ctx->intermediate_digest_size);
	err = tegra_hv_vse_safety_send_sha_data(se_dev, req, ivc_req_msg,
				nbytes, islast);
	if (err) {
		dev_err(se_dev->dev, "%s error %d\n", __func__, err);
		goto exit;
	}
exit:
	devm_kfree(se_dev->dev, ivc_req_msg);
	return err;
}

static int tegra_hv_vse_safety_sha_fast_path(struct ahash_request *req,
					bool is_last, bool process_cur_req)
{
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_SHA];
	u32 bytes_process_in_req = 0, num_blks;
	struct tegra_virtual_se_ivc_msg_t *ivc_req_msg;
	struct tegra_virtual_se_ivc_tx_msg_t *ivc_tx = NULL;
	struct tegra_virtual_se_addr *src_addr = NULL;
	struct tegra_virtual_se_req_context *req_ctx = ahash_request_ctx(req);
	u32 num_mapped_sgs = 0;
	u32 num_lists = 0;
	struct scatterlist *sg;
	int err = 0;
	u32 nbytes_in_req = req->nbytes;

	/* process_cur_req  is_last :
	 *     false         false  : update()                   -> hash
	 *     true          true   : finup(), digest()          -> hash
	 *                   true   : finup(), digest(), final() -> result
	 */
	if ((process_cur_req == false && is_last == false) ||
		(process_cur_req == true && is_last == true)) {
		/* When calling update(), if req->nbytes is aligned with
		 * req_ctx->blk_size, reduce req->nbytes with req_ctx->blk_size
		 * to avoid hashing zero length input at the end.
		 */
		if (req_ctx->residual_bytes == req_ctx->blk_size) {
			err = tegra_hv_vse_safety_sha_send_one(req,
					req_ctx->residual_bytes, false);
			if (err) {
				dev_err(se_dev->dev,
					"%s: failed to send residual data %u\n",
					__func__, req_ctx->residual_bytes);
				return err;
			}
			req_ctx->residual_bytes = 0;
		}

		num_blks = nbytes_in_req / req_ctx->blk_size;
		req_ctx->residual_bytes =
			nbytes_in_req - (num_blks * req_ctx->blk_size);

		if (num_blks > 0 && req_ctx->residual_bytes == 0) {
			/* blk_size aligned. reduce size with one blk and
			 * handle it in the next call.
			 */
			req_ctx->residual_bytes = req_ctx->blk_size;
			req_ctx->total_count += req_ctx->residual_bytes;
			num_blks--;
			sg_pcopy_to_buffer(req->src, (u32)sg_nents(req->src),
				req_ctx->sha_buf, req_ctx->residual_bytes,
				num_blks * req_ctx->blk_size);
		} else {
			/* not aligned at all */
			req_ctx->total_count += req_ctx->residual_bytes;
			sg_pcopy_to_buffer(req->src, (u32)sg_nents(req->src),
				req_ctx->sha_buf, req_ctx->residual_bytes,
				num_blks * req_ctx->blk_size);
		}
		nbytes_in_req -= req_ctx->residual_bytes;

		dev_dbg(se_dev->dev, "%s: req_ctx->residual_bytes %u\n",
			__func__, req_ctx->residual_bytes);

		if (num_blks > 0) {
			ivc_req_msg = devm_kzalloc(se_dev->dev,
				sizeof(*ivc_req_msg), GFP_KERNEL);
			if (!ivc_req_msg)
				return -ENOMEM;

			ivc_tx = &ivc_req_msg->tx[0];
			src_addr = &ivc_tx->sha.op_hash.src_addr;

			bytes_process_in_req = num_blks * req_ctx->blk_size;
			dev_dbg(se_dev->dev, "%s: bytes_process_in_req %u\n",
				__func__, bytes_process_in_req);

			err = tegra_hv_vse_safety_prepare_ivc_linked_list(se_dev,
					req->src, bytes_process_in_req,
					(TEGRA_HV_VSE_SHA_MAX_LL_NUM_1 -
						num_lists),
					req_ctx->blk_size,
					src_addr,
					&num_lists,
					DMA_TO_DEVICE, &num_mapped_sgs);
			if (err) {
				dev_err(se_dev->dev, "%s: ll error %d\n",
					__func__, err);
				goto unmap;
			}

			dev_dbg(se_dev->dev, "%s: num_lists %u\n",
				__func__, num_lists);

			ivc_tx->sha.op_hash.dst
				= (u64)req_ctx->hash_result_addr;
			memcpy(ivc_tx->sha.op_hash.hash,
				req_ctx->hash_result,
				req_ctx->intermediate_digest_size);

			req_ctx->total_count += bytes_process_in_req;

			err = tegra_hv_vse_safety_send_sha_data(se_dev, req,
				ivc_req_msg, bytes_process_in_req, false);
			if (err) {
				dev_err(se_dev->dev, "%s error %d\n",
					__func__, err);
				goto unmap;
			}
unmap:
			sg = req->src;
			while (sg && num_mapped_sgs--) {
				dma_unmap_sg(se_dev->dev, sg, 1, DMA_TO_DEVICE);
				sg = sg_next(sg);
			}
			devm_kfree(se_dev->dev, ivc_req_msg);
		}

		if (req_ctx->residual_bytes > 0 &&
			req_ctx->residual_bytes < req_ctx->blk_size) {
			/* At this point, the buffer is not aligned with
			 * blk_size. Thus, buffer alignment need to be done via
			 * slow path.
			 */
			req_ctx->force_align = true;
		}
	}

	if (is_last) {
		/* handle the last data in finup() , digest() */
		if (req_ctx->residual_bytes > 0) {
			err = tegra_hv_vse_safety_sha_send_one(req,
					req_ctx->residual_bytes, true);
			if (err) {
				dev_err(se_dev->dev,
					"%s: failed to send last data %u\n",
					__func__, req_ctx->residual_bytes);
				return err;
			}
			req_ctx->residual_bytes = 0;
		}

		if (req->result) {
			memcpy(req->result, req_ctx->hash_result,
				req_ctx->digest_size);
		} else {
			dev_err(se_dev->dev, "Invalid clinet result buffer\n");
		}
	}

	return err;
}

static int tegra_hv_vse_safety_sha_slow_path(struct ahash_request *req,
					bool is_last, bool process_cur_req)
{
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_SHA];
	struct tegra_virtual_se_req_context *req_ctx = ahash_request_ctx(req);
	u32 nblk_bytes = 0, num_blks, buflen = SZ_4M;
	u32 length = 0, skip = 0, offset = 0;
	u64 total_bytes = 0, left_bytes = 0;
	int err = 0;
	bool data_processed;

	if ((process_cur_req == false && is_last == false) ||
		(process_cur_req == true && is_last == true)) {

		total_bytes = req_ctx->residual_bytes + req->nbytes;
		num_blks = total_bytes / req_ctx->blk_size;
		nblk_bytes = num_blks * req_ctx->blk_size;
		offset = req_ctx->residual_bytes;

		/* if blk_size aligned, reduce 1 blk_size for the last hash */
		if ((total_bytes - nblk_bytes) == 0)
			total_bytes -= req_ctx->blk_size;

		left_bytes = req->nbytes;

		data_processed = false;
		while (total_bytes >= req_ctx->blk_size) {
			/* Copy to linear buffer */
			num_blks = total_bytes / req_ctx->blk_size;
			nblk_bytes = num_blks * req_ctx->blk_size;
			length = min(buflen, nblk_bytes) - offset;

			sg_pcopy_to_buffer(req->src, (u32)sg_nents(req->src),
				req_ctx->sha_buf + offset, length, skip);
			skip += length;
			req_ctx->total_count += length;

			/* Hash */
			err = tegra_hv_vse_safety_sha_send_one(req,
						length + offset, false);
			if (err) {
				dev_err(se_dev->dev,
					"%s: failed to send one %u\n",
					__func__, length + offset);
				return err;
			}
			total_bytes -= (length + offset);
			left_bytes -= length;
			offset = 0;
			data_processed = true;
		}

		if (data_processed == true) {
			/* Processed in while() loop */
			sg_pcopy_to_buffer(req->src, (u32)sg_nents(req->src),
					req_ctx->sha_buf, left_bytes, skip);
			req_ctx->total_count += left_bytes;
			req_ctx->residual_bytes = left_bytes;
		} else {
			/* Accumulate the request */
			sg_pcopy_to_buffer(req->src, (u32)sg_nents(req->src),
				req_ctx->sha_buf + req_ctx->residual_bytes,
				req->nbytes, skip);
			req_ctx->total_count += req->nbytes;
			req_ctx->residual_bytes += req->nbytes;
		}

		if (req_ctx->force_align == true &&
			req_ctx->residual_bytes == req_ctx->blk_size) {
			/* At this point, the buffer is aligned with blk_size.
			 * Thus, the next call can use fast path.
			 */
			req_ctx->force_align = false;
		}
	}

	if (is_last) {
		/* handle the last data in finup() , digest() */
		if (req_ctx->residual_bytes > 0) {
			err = tegra_hv_vse_safety_sha_send_one(req,
					req_ctx->residual_bytes, true);
			if (err) {
				dev_err(se_dev->dev,
					"%s: failed to send last data%u\n",
					__func__, req_ctx->residual_bytes);
				return err;
			}
			req_ctx->residual_bytes = 0;
		}

		if (req->result) {
			memcpy(req->result, req_ctx->hash_result,
				req_ctx->digest_size);
		} else {
			dev_err(se_dev->dev, "Invalid clinet result buffer\n");
		}
	}

	return err;
}

static int tegra_hv_vse_safety_sha_op(struct ahash_request *req, bool is_last,
			   bool process_cur_req)
{
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_SHA];
	struct tegra_virtual_se_req_context *req_ctx = ahash_request_ctx(req);
	u32 mode;
	u32 num_blks;
	int ret;
	struct sha_zero_length_vector zero_vec[] = {
		{
			.size = SHA1_DIGEST_SIZE,
			.digest = "\xda\x39\xa3\xee\x5e\x6b\x4b\x0d"
				  "\x32\x55\xbf\xef\x95\x60\x18\x90"
				  "\xaf\xd8\x07\x09",
		}, {
			.size = SHA224_DIGEST_SIZE,
			.digest = "\xd1\x4a\x02\x8c\x2a\x3a\x2b\xc9"
				  "\x47\x61\x02\xbb\x28\x82\x34\xc4"
				  "\x15\xa2\xb0\x1f\x82\x8e\xa6\x2a"
				  "\xc5\xb3\xe4\x2f",
		}, {
			.size = SHA256_DIGEST_SIZE,
			.digest = "\xe3\xb0\xc4\x42\x98\xfc\x1c\x14"
				  "\x9a\xfb\xf4\xc8\x99\x6f\xb9\x24"
				  "\x27\xae\x41\xe4\x64\x9b\x93\x4c"
				  "\xa4\x95\x99\x1b\x78\x52\xb8\x55",
		}, {
			.size = SHA384_DIGEST_SIZE,
			.digest = "\x38\xb0\x60\xa7\x51\xac\x96\x38"
				  "\x4c\xd9\x32\x7e\xb1\xb1\xe3\x6a"
				  "\x21\xfd\xb7\x11\x14\xbe\x07\x43"
				  "\x4c\x0c\xc7\xbf\x63\xf6\xe1\xda"
				  "\x27\x4e\xde\xbf\xe7\x6f\x65\xfb"
				  "\xd5\x1a\xd2\xf1\x48\x98\xb9\x5b",
		}, {
			.size = SHA512_DIGEST_SIZE,
			.digest = "\xcf\x83\xe1\x35\x7e\xef\xb8\xbd"
				  "\xf1\x54\x28\x50\xd6\x6d\x80\x07"
				  "\xd6\x20\xe4\x05\x0b\x57\x15\xdc"
				  "\x83\xf4\xa9\x21\xd3\x6c\xe9\xce"
				  "\x47\xd0\xd1\x3c\x5d\x85\xf2\xb0"
				  "\xff\x83\x18\xd2\x87\x7e\xec\x2f"
				  "\x63\xb9\x31\xbd\x47\x41\x7a\x81"
				  "\xa5\x38\x32\x7a\xf9\x27\xda\x3e",
		}, {
			.size = SHA3_256_DIGEST_SIZE,
			.digest = "\xa7\xff\xc6\xf8\xbf\x1e\xd7\x66"
				  "\x51\xc1\x47\x56\xa0\x61\xd6\x62"
				  "\xf5\x80\xff\x4d\xe4\x3b\x49\xfa"
				  "\x82\xd8\x0a\x4b\x80\xf8\x43\x4a",
		}, {
			.size = SHA3_384_DIGEST_SIZE,
			.digest = "\x0c\x63\xa7\x5b\x84\x5e\x4f\x7d"
				  "\x01\x10\x7d\x85\x2e\x4c\x24\x85"
				  "\xc5\x1a\x50\xaa\xaa\x94\xfc\x61"
				  "\x99\x5e\x71\xbb\xee\x98\x3a\x2a"
				  "\xc3\x71\x38\x31\x26\x4a\xdb\x47"
				  "\xfb\x6b\xd1\xe0\x58\xd5\xf0\x04",
		}, {
			.size = SHA3_512_DIGEST_SIZE,
			.digest = "\xa6\x9f\x73\xcc\xa2\x3a\x9a\xc5"
				   "\xc8\xb5\x67\xdc\x18\x5a\x75\x6e"
				   "\x97\xc9\x82\x16\x4f\xe2\x58\x59"
				  "\xe0\xd1\xdc\xc1\x47\x5c\x80\xa6"
				  "\x15\xb2\x12\x3a\xf1\xf5\xf9\x4c"
				  "\x11\xe3\xe9\x40\x2c\x3a\xc5\x58"
				  "\xf5\x00\x19\x9d\x95\xb6\xd3\xe3"
				  "\x01\x75\x85\x86\x28\x1d\xcd\x26",
		}
	};

	if (req_ctx->mode == VIRTUAL_SE_OP_MODE_SHAKE128 ||
			req_ctx->mode == VIRTUAL_SE_OP_MODE_SHAKE256) {
		if (req_ctx->digest_size == 0) {
			dev_info(se_dev->dev, "digest size is 0\n");
			return 0;
		}
	}

	if (req->nbytes == 0) {
		if (req_ctx->total_count > 0) {
			if (is_last == false) {
				dev_info(se_dev->dev, "empty packet\n");
				return 0;
			}

			if (req_ctx->residual_bytes > 0) { /*final() */
				ret = tegra_hv_vse_safety_sha_send_one(req,
					req_ctx->residual_bytes, is_last);
				if (ret) {
					dev_err(se_dev->dev,
					"%s: failed to send last data %u\n",
					__func__, req_ctx->residual_bytes);
					return ret;
				}
				req_ctx->residual_bytes = 0;
			}

			if (is_last) {
				if (req->result) {
					memcpy(req->result,
						req_ctx->hash_result,
						req_ctx->digest_size);
				} else {
					dev_err(se_dev->dev,
						"Invalid clinet result buffer\n");
				}
			}

			return 0;
		}

		if (req_ctx->mode == VIRTUAL_SE_OP_MODE_SHAKE128 ||
				req_ctx->mode == VIRTUAL_SE_OP_MODE_SHAKE256) {
			ret = tegra_hv_vse_safety_sha_send_one(req,
					0, is_last);
			if (ret) {
				dev_err(se_dev->dev, "%s: failed to send last data\n",
						__func__);
				return ret;
			}

			if (is_last) {
				if (req->result) {
					memcpy(req->result,
						req_ctx->hash_result,
						req_ctx->digest_size);
				} else {
					dev_err(se_dev->dev,
						"Invalid clinet result buffer\n");
				}
			}

			return 0;
		}
		/* If the request length is zero, SW WAR for zero length SHA
		 * operation since SE HW can't accept zero length SHA operation
		 */
		if (req_ctx->mode == VIRTUAL_SE_OP_MODE_SHA1)
			mode = VIRTUAL_SE_OP_MODE_SHA1;
		else if (req_ctx->mode < VIRTUAL_SE_OP_MODE_SHA3_256)
			mode = req_ctx->mode - VIRTUAL_SE_OP_MODE_SHA224 + 1;
		else
			mode = req_ctx->mode - VIRTUAL_SE_OP_MODE_SHA224 - 1;

		if (is_last) {
			if (req->result) {
				memcpy(req->result,
					zero_vec[mode].digest,
					zero_vec[mode].size);
			} else {
				dev_err(se_dev->dev,
					"Invalid clinet result buffer\n");
			}
		}

		return 0;
	}

	num_blks = req->nbytes / req_ctx->blk_size;

	if (sg_nents(req->src) > 1)
		req_ctx->force_align = true;

	if (req_ctx->force_align == false && num_blks > 0)
		ret = tegra_hv_vse_safety_sha_fast_path(req, is_last, process_cur_req);
	else
		ret = tegra_hv_vse_safety_sha_slow_path(req, is_last, process_cur_req);

	return ret;
}

static int tegra_hv_vse_safety_sha_init(struct ahash_request *req)
{
	struct crypto_ahash *tfm;
	struct tegra_virtual_se_req_context *req_ctx;
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_SHA];
	u32 dst_len;

	if (!req) {
		dev_err(se_dev->dev, "SHA request not valid\n");
		return -EINVAL;
	}

	/* Return error if engine is in suspended state */
	if (atomic_read(&se_dev->se_suspended))
		return -ENODEV;

	req_ctx = ahash_request_ctx(req);
	if (!req_ctx) {
		dev_err(se_dev->dev, "SHA req_ctx not valid\n");
		return -EINVAL;
	}

	tfm = crypto_ahash_reqtfm(req);
	if (!tfm) {
		dev_err(se_dev->dev, "SHA transform not valid\n");
		return -EINVAL;
	}

	req_ctx->digest_size = crypto_ahash_digestsize(tfm);
	if (strcmp(crypto_ahash_alg_name(tfm), "sha256") == 0) {
		req_ctx->mode = VIRTUAL_SE_OP_MODE_SHA256;
		req_ctx->blk_size = TEGRA_VIRTUAL_SE_SHA_HASH_BLOCK_SIZE_512BIT;
		req_ctx->intermediate_digest_size = SHA256_DIGEST_SIZE;
	} else if (strcmp(crypto_ahash_alg_name(tfm), "sha384") == 0) {
		req_ctx->mode = VIRTUAL_SE_OP_MODE_SHA384;
		req_ctx->blk_size =
			TEGRA_VIRTUAL_SE_SHA_HASH_BLOCK_SIZE_1024BIT;
		/*
		 * The intermediate digest size of SHA384 is same as SHA512
		 */
		req_ctx->intermediate_digest_size = SHA512_DIGEST_SIZE;
	} else if (strcmp(crypto_ahash_alg_name(tfm), "sha512") == 0) {
		req_ctx->mode = VIRTUAL_SE_OP_MODE_SHA512;
		req_ctx->blk_size =
			TEGRA_VIRTUAL_SE_SHA_HASH_BLOCK_SIZE_1024BIT;
		req_ctx->intermediate_digest_size = SHA512_DIGEST_SIZE;
	} else if (strcmp(crypto_ahash_alg_name(tfm), "sha3-256") == 0) {
		req_ctx->mode = VIRTUAL_SE_OP_MODE_SHA3_256;
		req_ctx->blk_size =
			TEGRA_VIRTUAL_SE_SHA_HASH_BLOCK_SIZE_1088BIT;
		req_ctx->intermediate_digest_size = SHA3_STATE_SIZE;
	} else if (strcmp(crypto_ahash_alg_name(tfm), "sha3-384") == 0) {
		req_ctx->mode = VIRTUAL_SE_OP_MODE_SHA3_384;
		req_ctx->blk_size =
			TEGRA_VIRTUAL_SE_SHA_HASH_BLOCK_SIZE_832BIT;
		req_ctx->intermediate_digest_size = SHA3_STATE_SIZE;
	} else if (strcmp(crypto_ahash_alg_name(tfm), "sha3-512") == 0) {
		req_ctx->mode = VIRTUAL_SE_OP_MODE_SHA3_512;
		req_ctx->blk_size =
			TEGRA_VIRTUAL_SE_SHA_HASH_BLOCK_SIZE_576BIT;
		req_ctx->intermediate_digest_size = SHA3_STATE_SIZE;
	} else if (strcmp(crypto_ahash_alg_name(tfm), "shake128") == 0) {
		req_ctx->mode = VIRTUAL_SE_OP_MODE_SHAKE128;
		req_ctx->blk_size =
			TEGRA_VIRTUAL_SE_SHA_HASH_BLOCK_SIZE_1344BIT;
		req_ctx->intermediate_digest_size = SHA3_STATE_SIZE;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		req_ctx->digest_size = req->dst_size;
#endif
	} else if (strcmp(crypto_ahash_alg_name(tfm), "shake256") == 0) {
		req_ctx->mode = VIRTUAL_SE_OP_MODE_SHAKE256;
		req_ctx->blk_size =
			TEGRA_VIRTUAL_SE_SHA_HASH_BLOCK_SIZE_1088BIT;
		req_ctx->intermediate_digest_size = SHA3_STATE_SIZE;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		req_ctx->digest_size = req->dst_size;
#endif
	} else {
		dev_err(se_dev->dev, "Invalid SHA Mode\n");
		return -EINVAL;
	}

	req_ctx->sha_buf = dma_alloc_coherent(se_dev->dev, SZ_4M,
					&req_ctx->sha_buf_addr, GFP_KERNEL);
	if (!req_ctx->sha_buf) {
		dev_err(se_dev->dev, "Cannot allocate memory to sha_buf\n");
		return -ENOMEM;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	if ((req_ctx->mode == VIRTUAL_SE_OP_MODE_SHAKE128) ||
			(req_ctx->mode == VIRTUAL_SE_OP_MODE_SHAKE256)) {
		dst_len = req->dst_size;
	} else {
		dst_len = req_ctx->intermediate_digest_size;
	}
#else
	dst_len = req_ctx->intermediate_digest_size;
#endif

	req_ctx->hash_result = dma_alloc_coherent(
			se_dev->dev, dst_len,
			&req_ctx->hash_result_addr, GFP_KERNEL);
	if (!req_ctx->hash_result) {
		dma_free_coherent(se_dev->dev, SZ_4M,
				req_ctx->sha_buf, req_ctx->sha_buf_addr);
		req_ctx->sha_buf = NULL;
		dev_err(se_dev->dev, "Cannot allocate memory to hash_result\n");
		return -ENOMEM;
	}
	req_ctx->total_count = 0;
	req_ctx->is_first = true;
	req_ctx->residual_bytes = 0;
	req_ctx->req_context_initialized = true;
	req_ctx->force_align = false;

	return 0;
}

static void tegra_hv_vse_safety_sha_req_deinit(struct ahash_request *req)
{
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_SHA];
	struct tegra_virtual_se_req_context *req_ctx = ahash_request_ctx(req);

	/* dma_free_coherent does not panic if addr is NULL */
	dma_free_coherent(se_dev->dev, SZ_4M,
			req_ctx->sha_buf, req_ctx->sha_buf_addr);
	req_ctx->sha_buf = NULL;

	dma_free_coherent(
		se_dev->dev, (TEGRA_HV_VSE_SHA_MAX_BLOCK_SIZE * 2),
		req_ctx->hash_result, req_ctx->hash_result_addr);
	req_ctx->hash_result = NULL;
	req_ctx->req_context_initialized = false;
}

static int tegra_hv_vse_safety_sha_update(struct ahash_request *req)
{
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_SHA];
	struct tegra_virtual_se_req_context *req_ctx = ahash_request_ctx(req);
	int ret = 0;

	if (!req) {
		dev_err(se_dev->dev, "SHA request not valid\n");
		return -EINVAL;
	}

	/* Return error if engine is in suspended state */
	if (atomic_read(&se_dev->se_suspended))
		return -ENODEV;

	req_ctx = ahash_request_ctx(req);
	if (!req_ctx->req_context_initialized) {
		dev_err(se_dev->dev,
			"%s Request ctx not initialized\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&se_dev->mtx);

	ret = tegra_hv_vse_safety_sha_op(req, false, false);
	if (ret)
		dev_err(se_dev->dev, "tegra_se_sha_update failed - %d\n", ret);

	mutex_unlock(&se_dev->mtx);

	return ret;
}

static int tegra_hv_vse_safety_sha_finup(struct ahash_request *req)
{
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_SHA];
	struct tegra_virtual_se_req_context *req_ctx = ahash_request_ctx(req);
	int ret = 0;

	if (!req) {
		dev_err(se_dev->dev, "SHA request not valid\n");
		return -EINVAL;
	}

	/* Return error if engine is in suspended state */
	if (atomic_read(&se_dev->se_suspended))
		return -ENODEV;

	req_ctx = ahash_request_ctx(req);
	if (!req_ctx->req_context_initialized) {
		dev_err(se_dev->dev,
			"%s Request ctx not initialized\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&se_dev->mtx);

	ret = tegra_hv_vse_safety_sha_op(req, true, true);
	if (ret)
		dev_err(se_dev->dev, "tegra_se_sha_finup failed - %d\n", ret);

	mutex_unlock(&se_dev->mtx);

	tegra_hv_vse_safety_sha_req_deinit(req);

	return ret;
}

static int tegra_hv_vse_safety_sha_final(struct ahash_request *req)
{
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_SHA];
	struct tegra_virtual_se_req_context *req_ctx = ahash_request_ctx(req);
	int ret = 0;

	if (!req) {
		dev_err(se_dev->dev, "SHA request not valid\n");
		return -EINVAL;
	}

	/* Return error if engine is in suspended state */
	if (atomic_read(&se_dev->se_suspended))
		return -ENODEV;

	req_ctx = ahash_request_ctx(req);
	if (!req_ctx->req_context_initialized) {
		dev_err(se_dev->dev,
			"%s Request ctx not initialized\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&se_dev->mtx);
	/* Do not process data in given request */
	ret = tegra_hv_vse_safety_sha_op(req, true, false);
	if (ret)
		dev_err(se_dev->dev, "tegra_se_sha_final failed - %d\n", ret);

	mutex_unlock(&se_dev->mtx);
	tegra_hv_vse_safety_sha_req_deinit(req);

	return ret;
}

static int tegra_hv_vse_safety_sha_digest(struct ahash_request *req)
{
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_SHA];
	int ret = 0;

	if (!req) {
		dev_err(se_dev->dev, "SHA request not valid\n");
		return -EINVAL;
	}

	/* Return error if engine is in suspended state */
	if (atomic_read(&se_dev->se_suspended))
		return -ENODEV;

	ret = tegra_hv_vse_safety_sha_init(req);
	if (ret) {
		dev_err(se_dev->dev, "%s init failed - %d\n", __func__, ret);
		return ret;
	}

	mutex_lock(&se_dev->mtx);
	ret = tegra_hv_vse_safety_sha_op(req, true, true);
	if (ret)
		dev_err(se_dev->dev, "tegra_se_sha_digest failed - %d\n", ret);
	mutex_unlock(&se_dev->mtx);

	tegra_hv_vse_safety_sha_req_deinit(req);

	return ret;
}

static int tegra_hv_vse_safety_sha_export(struct ahash_request *req, void *out)
{
	struct tegra_virtual_se_req_context *req_ctx = ahash_request_ctx(req);

	memcpy(out, req_ctx, sizeof(*req_ctx));
	return 0;
}

static int tegra_hv_vse_safety_sha_import(struct ahash_request *req, const void *in)
{
	struct tegra_virtual_se_req_context *req_ctx = ahash_request_ctx(req);

	memcpy(req_ctx, in, sizeof(*req_ctx));
	return 0;
}

static int tegra_hv_vse_safety_sha_cra_init(struct crypto_tfm *tfm)
{
	crypto_ahash_set_reqsize(__crypto_ahash_cast(tfm),
				 sizeof(struct tegra_virtual_se_req_context));

	return 0;
}

static void tegra_hv_vse_safety_sha_cra_exit(struct crypto_tfm *tfm)
{
}

void tegra_hv_vse_safety_prepare_cmd(struct tegra_virtual_se_dev *se_dev,
	struct tegra_virtual_se_ivc_tx_msg_t *ivc_tx,
	struct tegra_virtual_se_aes_req_context *req_ctx,
	struct tegra_virtual_se_aes_context *aes_ctx,
	struct skcipher_request *req)
{
	union tegra_virtual_se_aes_args *aes;

	aes = &ivc_tx->aes;
	if (req_ctx->encrypt == true)
		ivc_tx->cmd = TEGRA_VIRTUAL_SE_CMD_AES_ENCRYPT;
	else
		ivc_tx->cmd = TEGRA_VIRTUAL_SE_CMD_AES_DECRYPT;

	aes->op.keyslot = aes_ctx->aes_keyslot;
	aes->op.key_length = aes_ctx->keylen;
	aes->op.mode = req_ctx->op_mode;
	aes->op.ivsel = AES_ORIGINAL_IV;
	if (req->iv) {
		memcpy(aes->op.lctr, req->iv,
				TEGRA_VIRTUAL_SE_AES_LCTR_SIZE);
		if (req_ctx->op_mode == AES_CTR)
			aes->op.ctr_cntn = TEGRA_VIRTUAL_SE_AES_LCTR_CNTN;
		else if (req_ctx->op_mode == AES_CBC)
			aes->op.ivsel = AES_IV_REG;
		else
			aes->op.ivsel = AES_ORIGINAL_IV;
	}
}

static int status_to_errno(u32 err)
{
	switch (err) {
	case 1:		/* VSE_MSG_ERR_INVALID_CMD */
	case 3:		/* VSE_MSG_ERR_INVALID_ARGS */
	case 11:	/* VSE_MSG_ERR_MAC_INVALID */
		return -EINVAL;
	case 4:		/* VSE_MSG_ERR_INVALID_KEY */
	case 5:		/* VSE_MSG_ERR_CTR_OVERFLOW */
	case 6:		/* VSE_MSG_ERR_INVALID_SUBKEY */
	case 7:		/* VSE_MSG_ERR_CTR_NONCE_INVALID */
	case 8:		/* VSE_MSG_ERR_GCM_IV_INVALID */
	case 9:		/* VSE_MSG_ERR_GCM_NONCE_INVALID */
	case 10:	/* VSE_MSG_ERR_GMAC_INVALID_PARAMS */
		return -EPERM;
	}
	return err;
}

static int tegra_hv_vse_safety_aes_gen_random_iv(
		struct tegra_virtual_se_dev *se_dev,
		struct skcipher_request *req,
		struct tegra_vse_priv_data *priv,
		struct tegra_virtual_se_ivc_msg_t *ivc_req_msg)
{
	struct tegra_virtual_se_ivc_tx_msg_t *ivc_tx = &ivc_req_msg->tx[0];
	struct tegra_hv_ivc_cookie *pivck = g_ivck;
	union tegra_virtual_se_aes_args *aes = &ivc_tx->aes;
	struct tegra_virtual_se_aes_context *aes_ctx;
	int err = 0;
	int time_left;

	ivc_tx->cmd = TEGRA_VIRTUAL_SE_CMD_AES_ENCRYPT_INIT;
	priv->cmd = VIRTUAL_SE_PROCESS;
	aes_ctx = crypto_skcipher_ctx(crypto_skcipher_reqtfm(req));
	aes->op.keyslot = aes_ctx->aes_keyslot;
	aes->op.key_length = aes_ctx->keylen;

	init_completion(&priv->alg_complete);
	mutex_lock(&se_dev->server_lock);
	err = tegra_hv_vse_safety_send_ivc(se_dev, pivck, ivc_req_msg,
			sizeof(struct tegra_virtual_se_ivc_msg_t));
	if (err) {
		dev_err(se_dev->dev,
				"\n %s send ivc failed %d\n", __func__, err);
		mutex_unlock(&se_dev->server_lock);
		return err;
	}
	time_left = wait_for_completion_timeout(&priv->alg_complete,
			TEGRA_HV_VSE_TIMEOUT);
	if (time_left == 0) {
		dev_err(se_dev->dev, "%s timeout\n", __func__);
		err = -ETIMEDOUT;
		mutex_unlock(&se_dev->server_lock);
		return err;
	}
	mutex_unlock(&se_dev->server_lock);

	err = status_to_errno(priv->rx_status);

	if (err) {
		dev_err(se_dev->dev,
			"\n %s IV generation failed %d\n", __func__, err);
	}

	return err;
}

static int tegra_hv_vse_safety_process_aes_req(struct tegra_virtual_se_dev *se_dev,
		struct skcipher_request *req)
{
	struct tegra_virtual_se_aes_req_context *req_ctx;
	struct tegra_virtual_se_aes_context *aes_ctx;
	struct tegra_virtual_se_ivc_tx_msg_t *ivc_tx = NULL;
	struct tegra_virtual_se_ivc_hdr_t *ivc_hdr = NULL;
	struct tegra_hv_ivc_cookie *pivck = g_ivck;
	int err = 0;
	struct tegra_virtual_se_ivc_msg_t *ivc_req_msg = NULL;
	struct tegra_vse_priv_data *priv = NULL;
	struct tegra_vse_tag *priv_data_ptr;
	union tegra_virtual_se_aes_args *aes;
	int time_left;
	int num_sgs;
	int dma_ents = 0;

	priv = devm_kzalloc(se_dev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		err = -ENOMEM;
		goto exit;
	}
	priv->req = req;

	ivc_req_msg =
		devm_kzalloc(se_dev->dev, sizeof(*ivc_req_msg), GFP_KERNEL);
	if (!ivc_req_msg) {
		err = -ENOMEM;
		goto exit;
	}

	priv->buf = kmalloc(req->cryptlen, GFP_KERNEL);
	if (!priv->buf) {
		err = -ENOMEM;
		goto exit;
	}

	num_sgs = tegra_hv_vse_safety_count_sgs(req->src, req->cryptlen);
	if (num_sgs == 1)
		memcpy(priv->buf, sg_virt(req->src), req->cryptlen);
	else
		sg_copy_to_buffer(req->src, num_sgs, priv->buf, req->cryptlen);

	sg_init_one(&priv->sg, priv->buf, req->cryptlen);
	//dma_map_sg returns 0 on error
	dma_ents = dma_map_sg(se_dev->dev, &priv->sg, 1, DMA_BIDIRECTIONAL);
	if (!dma_ents) {
		err = -EINVAL;
		dev_err(se_dev->dev, "dma_map_sg failed\n");
		goto exit;
	}

	priv->buf_addr = sg_dma_address(&priv->sg);

	ivc_tx = &ivc_req_msg->tx[0];
	aes = &ivc_tx->aes;
	req_ctx = skcipher_request_ctx(req);
	aes_ctx = crypto_skcipher_ctx(crypto_skcipher_reqtfm(req));
	if (unlikely(!aes_ctx->is_key_slot_allocated)) {
		dev_err(se_dev->dev, "AES Key slot not allocated\n");
		goto exit;
	}

	ivc_hdr = &ivc_req_msg->ivc_hdr;
	//Currently we support only one request per IVC message
	ivc_hdr->num_reqs = 1U;
	ivc_hdr->header_magic[0] = 'N';
	ivc_hdr->header_magic[1] = 'V';
	ivc_hdr->header_magic[2] = 'D';
	ivc_hdr->header_magic[3] = 'A';
	ivc_hdr->engine = req_ctx->engine_id;

	priv_data_ptr = (struct tegra_vse_tag *)ivc_hdr->tag;
	priv_data_ptr->priv_data = (unsigned int *)priv;
	priv->se_dev = se_dev;
	vse_thread_start = true;

	/*
	 * If first byte of iv is 1 and the request is for AES CBC/CTR encryption,
	 * it means that generation of random IV is required.
	 */
	if (req_ctx->encrypt &&
			(req_ctx->op_mode == AES_CBC ||
			req_ctx->op_mode == AES_CTR) &&
			req->iv[0] == 1) {
		//Random IV generation is required
		err = tegra_hv_vse_safety_aes_gen_random_iv(se_dev, req,
				priv, ivc_req_msg);
		if (err)
			goto exit;
	}
	priv->cmd = VIRTUAL_SE_AES_CRYPTO;

	tegra_hv_vse_safety_prepare_cmd(se_dev, ivc_tx, req_ctx, aes_ctx, req);
	aes->op.src_addr.lo = priv->buf_addr;
	aes->op.src_addr.hi = req->cryptlen;
	aes->op.dst_addr.lo = priv->buf_addr;
	aes->op.dst_addr.hi = req->cryptlen;

	init_completion(&priv->alg_complete);
	mutex_lock(&se_dev->server_lock);
	err = tegra_hv_vse_safety_send_ivc(se_dev, pivck, ivc_req_msg,
			sizeof(struct tegra_virtual_se_ivc_msg_t));
	if (err) {
		dev_err(se_dev->dev,
			"\n %s send ivc failed %d\n", __func__, err);
		mutex_unlock(&se_dev->server_lock);
		goto exit;
	}
	time_left = wait_for_completion_timeout(&priv->alg_complete,
			TEGRA_HV_VSE_TIMEOUT);
	if (time_left == 0) {
		dev_err(se_dev->dev, "%s timeout\n", __func__);
		err = -ETIMEDOUT;
		mutex_unlock(&se_dev->server_lock);
		goto exit;
	}
	mutex_unlock(&se_dev->server_lock);

	if (priv->rx_status == 0U) {
		dma_sync_single_for_cpu(priv->se_dev->dev, priv->buf_addr,
			req->cryptlen, DMA_BIDIRECTIONAL);

		num_sgs = tegra_hv_vse_safety_count_sgs(req->dst, req->cryptlen);
		if (num_sgs == 1)
			memcpy(sg_virt(req->dst), priv->buf, req->cryptlen);
		else
			sg_copy_from_buffer(req->dst, num_sgs,
					priv->buf, req->cryptlen);

		if (((req_ctx->op_mode == AES_CBC)
				|| (req_ctx->op_mode == AES_CTR))
				&& req_ctx->encrypt == true)
			memcpy(req->iv, priv->iv, TEGRA_VIRTUAL_SE_AES_IV_SIZE);
	} else {
		dev_err(se_dev->dev,
				"%s: SE server returned error %u\n",
				__func__, priv->rx_status);
	}

	err = status_to_errno(priv->rx_status);

exit:
	if (dma_ents > 0)
		dma_unmap_sg(se_dev->dev, &priv->sg, 1, DMA_BIDIRECTIONAL);

	if (priv) {
		//kfree won't fail even if priv->buf == NULL
		kfree(priv->buf);
		devm_kfree(se_dev->dev, priv);
	}

	if (ivc_req_msg)
		devm_kfree(se_dev->dev, ivc_req_msg);

	return err;
}

static int tegra_hv_vse_safety_aes_cra_init(struct crypto_skcipher *tfm)
{
	tfm->reqsize =
		sizeof(struct tegra_virtual_se_aes_req_context);

	return 0;
}

static void tegra_hv_vse_safety_aes_cra_exit(struct crypto_skcipher *tfm)
{
	/* nothing to do as user releases the keyslot through tzvault TA */
}

static int tegra_hv_vse_safety_aes_cbc_encrypt(struct skcipher_request *req)
{
	int err = 0;
	struct tegra_virtual_se_aes_req_context *req_ctx = NULL;

	if (!req) {
		pr_err("NULL req received by %s", __func__);
		return -EINVAL;
	}
	if (unlikely(!req->iv)) {
		/* If IV is not set we cannot determine whether
		 * random IV generation is required.
		 */
		pr_err("%s: Unable to determine if random IV generation is needed\n", __func__);
		return -EINVAL;
	}
	req_ctx = skcipher_request_ctx(req);

	req_ctx->encrypt = true;
	req_ctx->op_mode = AES_CBC;
	req_ctx->engine_id = VIRTUAL_SE_AES1;
	req_ctx->se_dev = g_virtual_se_dev[VIRTUAL_SE_AES1];
	mutex_lock(&req_ctx->se_dev->mtx);
	err = tegra_hv_vse_safety_process_aes_req(req_ctx->se_dev, req);
	if (err)
		dev_err(req_ctx->se_dev->dev,
				"%s failed with error %d\n", __func__, err);
	mutex_unlock(&req_ctx->se_dev->mtx);
	return err;
}

static int tegra_hv_vse_safety_aes_cbc_decrypt(struct skcipher_request *req)
{
	int err = 0;
	struct tegra_virtual_se_aes_req_context *req_ctx = NULL;

	if (!req) {
		pr_err("NULL req received by %s", __func__);
		return -EINVAL;
	}
	req_ctx = skcipher_request_ctx(req);

	req_ctx->encrypt = false;
	req_ctx->op_mode = AES_CBC;
	req_ctx->engine_id = VIRTUAL_SE_AES1;
	req_ctx->se_dev = g_virtual_se_dev[VIRTUAL_SE_AES1];
	mutex_lock(&req_ctx->se_dev->mtx);
	err = tegra_hv_vse_safety_process_aes_req(req_ctx->se_dev, req);
	if (err)
		dev_err(req_ctx->se_dev->dev,
				"%s failed with error %d\n", __func__, err);
	mutex_unlock(&req_ctx->se_dev->mtx);
	return err;
}

static int tegra_hv_vse_safety_aes_ecb_encrypt(struct skcipher_request *req)
{
	int err = 0;
	struct tegra_virtual_se_aes_req_context *req_ctx = NULL;

	if (!req) {
		pr_err("NULL req received by %s", __func__);
		return -EINVAL;
	}
	req_ctx = skcipher_request_ctx(req);

	req_ctx->encrypt = true;
	req_ctx->op_mode = AES_ECB;
	req_ctx->engine_id = VIRTUAL_SE_AES1;
	req_ctx->se_dev = g_virtual_se_dev[VIRTUAL_SE_AES1];
	mutex_lock(&req_ctx->se_dev->mtx);
	err = tegra_hv_vse_safety_process_aes_req(req_ctx->se_dev, req);
	if (err)
		dev_err(req_ctx->se_dev->dev,
				"%s failed with error %d\n", __func__, err);
	mutex_unlock(&req_ctx->se_dev->mtx);
	return err;
}

static int tegra_hv_vse_safety_aes_ecb_decrypt(struct skcipher_request *req)
{
	int err = 0;
	struct tegra_virtual_se_aes_req_context *req_ctx = NULL;

	if (!req) {
		pr_err("NULL req received by %s", __func__);
		return -EINVAL;
	}
	req_ctx = skcipher_request_ctx(req);

	req_ctx->encrypt = false;
	req_ctx->op_mode = AES_ECB;
	req_ctx->engine_id = VIRTUAL_SE_AES1;
	req_ctx->se_dev = g_virtual_se_dev[VIRTUAL_SE_AES1];
	mutex_lock(&req_ctx->se_dev->mtx);
	err = tegra_hv_vse_safety_process_aes_req(req_ctx->se_dev, req);
	if (err)
		dev_err(req_ctx->se_dev->dev,
				"%s failed with error %d\n", __func__, err);
	mutex_unlock(&req_ctx->se_dev->mtx);
	return err;
}

static int tegra_hv_vse_safety_aes_ctr_encrypt(struct skcipher_request *req)
{
	int err = 0;
	struct tegra_virtual_se_aes_req_context *req_ctx = NULL;

	if (!req) {
		pr_err("NULL req received by %s", __func__);
		return -EINVAL;
	}
	if (unlikely(!req->iv)) {
		/* If IV is not set we cannot determine whether
		 * random IV generation is required.
		 */
		pr_err("%s: Unable to determine if random IV generation is needed\n", __func__);
		return -EINVAL;
	}
	req_ctx = skcipher_request_ctx(req);

	req_ctx->encrypt = true;
	req_ctx->op_mode = AES_CTR;
	req_ctx->engine_id = VIRTUAL_SE_AES1;
	req_ctx->se_dev = g_virtual_se_dev[VIRTUAL_SE_AES1];
	mutex_lock(&req_ctx->se_dev->mtx);
	err = tegra_hv_vse_safety_process_aes_req(req_ctx->se_dev, req);
	if (err)
		dev_err(req_ctx->se_dev->dev,
				"%s failed with error %d\n", __func__, err);
	mutex_unlock(&req_ctx->se_dev->mtx);
	return err;
}

static int tegra_hv_vse_safety_aes_ctr_decrypt(struct skcipher_request *req)
{
	int err = 0;
	struct tegra_virtual_se_aes_req_context *req_ctx = NULL;

	if (!req) {
		pr_err("NULL req received by %s", __func__);
		return -EINVAL;
	}
	req_ctx = skcipher_request_ctx(req);

	req_ctx->encrypt = false;
	req_ctx->op_mode = AES_CTR;
	req_ctx->engine_id = VIRTUAL_SE_AES1;
	req_ctx->se_dev = g_virtual_se_dev[VIRTUAL_SE_AES1];
	mutex_lock(&req_ctx->se_dev->mtx);
	err = tegra_hv_vse_safety_process_aes_req(req_ctx->se_dev, req);
	if (err)
		dev_err(req_ctx->se_dev->dev,
				"%s failed with error %d\n", __func__, err);
	mutex_unlock(&req_ctx->se_dev->mtx);
	return err;
}

static int tegra_hv_vse_safety_cmac_op(struct ahash_request *req, bool is_last)
{
	struct tegra_virtual_se_aes_cmac_context *cmac_ctx =
			crypto_ahash_ctx(crypto_ahash_reqtfm(req));
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_AES0];
	struct tegra_virtual_se_ivc_hdr_t *ivc_hdr;
	struct tegra_virtual_se_ivc_tx_msg_t *ivc_tx;
	struct tegra_virtual_se_ivc_msg_t *ivc_req_msg;
	struct tegra_hv_ivc_cookie *pivck = g_ivck;
	struct scatterlist *src_sg;
	struct sg_mapping_iter miter;
	u32 num_sgs, blocks_to_process, last_block_bytes = 0, bytes_to_copy = 0;
	u32 temp_len = 0;
	unsigned int total_len;
	unsigned long flags;
	unsigned int sg_flags = SG_MITER_ATOMIC;
	u8 *temp_buffer = NULL;
	int err = 0;
	int num_lists = 0;
	int time_left;
	struct tegra_vse_priv_data *priv = NULL;
	struct tegra_vse_tag *priv_data_ptr;
	unsigned int num_mapped_sgs = 0;

	blocks_to_process = req->nbytes / TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE;
	/* num of bytes less than block size */

	if (is_last == true) {
		if ((req->nbytes % TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE) ||
			!blocks_to_process) {
			last_block_bytes =
				req->nbytes % TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE;
		} else {
			/* decrement num of blocks */
			blocks_to_process--;
			last_block_bytes = TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE;
		}
	} else {
		last_block_bytes = 0;
	}

	ivc_req_msg = devm_kzalloc(se_dev->dev,
		sizeof(*ivc_req_msg), GFP_KERNEL);
	if (!ivc_req_msg)
		return -ENOMEM;

	priv = devm_kzalloc(se_dev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		devm_kfree(se_dev->dev, ivc_req_msg);
		return -ENOMEM;
	}

	ivc_tx = &ivc_req_msg->tx[0];
	ivc_hdr = &ivc_req_msg->ivc_hdr;
	ivc_hdr->num_reqs = 1;
	ivc_hdr->header_magic[0] = 'N';
	ivc_hdr->header_magic[1] = 'V';
	ivc_hdr->header_magic[2] = 'D';
	ivc_hdr->header_magic[3] = 'A';

	src_sg = req->src;
	num_sgs = tegra_hv_vse_safety_count_sgs(src_sg, req->nbytes);
	if (num_sgs > TEGRA_HV_VSE_AES_CMAC_MAX_LL_NUM) {
		dev_err(se_dev->dev,
			"\n Unsupported number of linked list %d\n", num_sgs);
		err = -ENOMEM;
		goto free_mem;
	}
	vse_thread_start = true;

	/* first process all blocks except last block */
	if (blocks_to_process) {
		total_len = blocks_to_process * TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE;

		err = tegra_hv_vse_safety_prepare_ivc_linked_list(se_dev, req->src,
			total_len, TEGRA_HV_VSE_AES_CMAC_MAX_LL_NUM,
			TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE,
			&ivc_tx->aes.op_cmac_s.src_addr,
			&num_lists,
			DMA_TO_DEVICE, &num_mapped_sgs);
		if (err)
			goto free_mem;

	}

	/* get the last block bytes from the sg_dma buffer using miter */
	if (is_last) {
		num_sgs = tegra_hv_vse_safety_count_sgs(req->src, req->nbytes);
		sg_flags |= SG_MITER_FROM_SG;
		sg_miter_start(&miter, req->src, num_sgs, sg_flags);
		local_irq_save(flags);
		total_len = 0;

		temp_len = last_block_bytes;
		temp_buffer = ivc_tx->aes.op_cmac_s.lastblock;
		while (sg_miter_next(&miter) && total_len < req->nbytes) {
			unsigned int len;

			len = min(miter.length, (size_t)(req->nbytes - total_len));
			if ((req->nbytes - (total_len + len)) <= temp_len) {
				bytes_to_copy =
					temp_len -
					(req->nbytes - (total_len + len));
				memcpy(temp_buffer, miter.addr + (len - bytes_to_copy),
					bytes_to_copy);
				temp_len -= bytes_to_copy;
				temp_buffer += bytes_to_copy;
			}
			total_len += len;
		}
		sg_miter_stop(&miter);
		local_irq_restore(flags);
	}


	ivc_hdr->engine = VIRTUAL_SE_AES0;
	ivc_tx->cmd = TEGRA_VIRTUAL_SE_CMD_AES_CMAC;

	ivc_tx->aes.op_cmac_s.keyslot = cmac_ctx->aes_keyslot;
	ivc_tx->aes.op_cmac_s.key_length = cmac_ctx->keylen;
	ivc_tx->aes.op_cmac_s.src_addr.hi = blocks_to_process * TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE;
	if (is_last == true)
		ivc_tx->aes.op_cmac_s.config = TEGRA_VIRTUAL_SE_AES_CMAC_CONFIG_LASTBLK;
	else
		ivc_tx->aes.op_cmac_s.config = TEGRA_VIRTUAL_SE_AES_CMAC_CONFIG_NONLASTBLK;

	ivc_tx->aes.op_cmac_s.lastblock_len = last_block_bytes;

	if (cmac_ctx->is_first) {
		ivc_tx->aes.op_cmac_s.ivsel = AES_ORIGINAL_IV;
		cmac_ctx->is_first = false;
	} else {
		ivc_tx->aes.op_cmac_s.ivsel = AES_IV_REG;
	}

	ivc_tx->aes.op_cmac_s.dst = (u64)cmac_ctx->hash_result_addr;
	memcpy(ivc_tx->aes.op_cmac_s.cmac_reg,
		cmac_ctx->hash_result, cmac_ctx->digest_size);

	priv_data_ptr = (struct tegra_vse_tag *)ivc_req_msg->ivc_hdr.tag;
	priv_data_ptr->priv_data = (unsigned int *)priv;
	if (is_last == true)
		priv->cmd = VIRTUAL_CMAC_PROCESS;
	else
		priv->cmd = VIRTUAL_SE_PROCESS;
	priv->se_dev = se_dev;
	init_completion(&priv->alg_complete);
	mutex_lock(&se_dev->server_lock);
	/* Return error if engine is in suspended state */
	if (atomic_read(&se_dev->se_suspended)) {
		mutex_unlock(&se_dev->server_lock);
		err = -ENODEV;
		goto unmap_exit;
	}
	err = tegra_hv_vse_safety_send_ivc(se_dev, pivck, ivc_req_msg,
			sizeof(struct tegra_virtual_se_ivc_msg_t));
	if (err) {
		mutex_unlock(&se_dev->server_lock);
		goto unmap_exit;
	}

	time_left = wait_for_completion_timeout(&priv->alg_complete,
			TEGRA_HV_VSE_TIMEOUT);
	mutex_unlock(&se_dev->server_lock);
	if (time_left == 0) {
		dev_err(se_dev->dev, "cmac_op timeout\n");
		err = -ETIMEDOUT;
	}

	if (is_last)
		memcpy(req->result, priv->cmac.data, TEGRA_VIRTUAL_SE_AES_CMAC_DIGEST_SIZE);

unmap_exit:
	src_sg = req->src;
	while (src_sg && num_mapped_sgs--) {
		dma_unmap_sg(se_dev->dev, src_sg, 1, DMA_TO_DEVICE);
		src_sg = sg_next(src_sg);
	}
free_mem:
	devm_kfree(se_dev->dev, priv);
	devm_kfree(se_dev->dev, ivc_req_msg);

	return err;

}

static int tegra_hv_vse_safety_cmac_sv_op(struct ahash_request *req, bool is_last)
{
	struct tegra_virtual_se_aes_cmac_context *cmac_ctx =
			crypto_ahash_ctx(crypto_ahash_reqtfm(req));
	struct tegra_vse_cmac_req_data *cmac_req_data;
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_AES0];
	struct tegra_virtual_se_ivc_hdr_t *ivc_hdr;
	struct tegra_virtual_se_ivc_tx_msg_t *ivc_tx;
	struct tegra_virtual_se_ivc_msg_t *ivc_req_msg;
	struct tegra_hv_ivc_cookie *pivck = g_ivck;
	struct scatterlist *src_sg;
	u32 blocks_to_process, last_block_bytes = 0;
	int num_sgs;
	unsigned int total_len;
	int err = 0;
	int num_lists = 0;
	int time_left;
	struct tegra_vse_priv_data *priv = NULL;
	struct tegra_vse_tag *priv_data_ptr;
	unsigned int num_mapped_sgs = 0;

	if ((req->nbytes == 0) || (req->nbytes > TEGRA_VIRTUAL_SE_MAX_SUPPORTED_BUFLEN)) {
		dev_err(se_dev->dev, "%s: input buffer size is invalid\n", __func__);
		return -EINVAL;
	}

	blocks_to_process = req->nbytes / TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE;
	/* num of bytes less than block size */

	if ((req->nbytes % TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE) ||
		blocks_to_process == 0) {
		last_block_bytes =
			req->nbytes % TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE;
	} else {
		/* decrement num of blocks */
		blocks_to_process--;
		last_block_bytes = TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE;
	}

	ivc_req_msg = devm_kzalloc(se_dev->dev,
		sizeof(*ivc_req_msg), GFP_KERNEL);
	if (!ivc_req_msg)
		return -ENOMEM;

	priv = devm_kzalloc(se_dev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		devm_kfree(se_dev->dev, ivc_req_msg);
		return -ENOMEM;
	}

	cmac_req_data = (struct tegra_vse_cmac_req_data *) req->priv;

	ivc_tx = &ivc_req_msg->tx[0];
	ivc_hdr = &ivc_req_msg->ivc_hdr;
	ivc_hdr->num_reqs = 1;
	ivc_hdr->header_magic[0] = 'N';
	ivc_hdr->header_magic[1] = 'V';
	ivc_hdr->header_magic[2] = 'D';
	ivc_hdr->header_magic[3] = 'A';

	src_sg = req->src;
	num_sgs = tegra_hv_vse_safety_count_sgs(src_sg, req->nbytes);
	if ((num_sgs > TEGRA_HV_VSE_AES_CMAC_MAX_LL_NUM) ||
			(num_sgs < 0)) {
		dev_err(se_dev->dev,
			"\n Unsupported number of linked list %d\n", num_sgs);
		err = -ENOMEM;
		goto free_mem;
	}
	vse_thread_start = true;

	/* first process all blocks except last block */
	if (blocks_to_process) {
		total_len = blocks_to_process * TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE;

		err = tegra_hv_vse_safety_prepare_ivc_linked_list(se_dev, req->src,
			total_len, TEGRA_HV_VSE_AES_CMAC_MAX_LL_NUM,
			TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE,
			&ivc_tx->aes.op_cmac_sv.src_addr,
			&num_lists,
			DMA_TO_DEVICE, &num_mapped_sgs);
		if (err)
			goto free_mem;

	}
	ivc_tx->aes.op_cmac_sv.lastblock_len = last_block_bytes;

	sg_pcopy_to_buffer(req->src,
			(u32)num_sgs,
			ivc_tx->aes.op_cmac_sv.lastblock,
			last_block_bytes,
			blocks_to_process * TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE);

	ivc_hdr->engine = VIRTUAL_SE_AES0;
	if (cmac_req_data->request_type == CMAC_SIGN)
		ivc_tx->cmd = TEGRA_VIRTUAL_SE_CMD_AES_CMAC_SIGN;
	else
		ivc_tx->cmd = TEGRA_VIRTUAL_SE_CMD_AES_CMAC_VERIFY;

	ivc_tx->aes.op_cmac_sv.keyslot = cmac_ctx->aes_keyslot;
	ivc_tx->aes.op_cmac_sv.key_length = cmac_ctx->keylen;
	ivc_tx->aes.op_cmac_sv.src_addr.hi = blocks_to_process * TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE;
	ivc_tx->aes.op_cmac_sv.config = 0;
	if (is_last == true)
		ivc_tx->aes.op_cmac_sv.config |= TEGRA_VIRTUAL_SE_AES_CMAC_SV_CONFIG_LASTREQ;

	if (cmac_ctx->is_first) {
		ivc_tx->aes.op_cmac_sv.config |= TEGRA_VIRTUAL_SE_AES_CMAC_SV_CONFIG_FIRSTREQ;
		if (cmac_req_data->request_type == CMAC_VERIFY) {
			memcpy(ivc_tx->aes.op_cmac_sv.cmac_result,
					req->result,
					TEGRA_VIRTUAL_SE_AES_CMAC_DIGEST_SIZE);
		}
		cmac_ctx->is_first = false;
	}

	priv_data_ptr = (struct tegra_vse_tag *)ivc_req_msg->ivc_hdr.tag;
	priv_data_ptr->priv_data = (unsigned int *)priv;
	if (is_last == true)
		priv->cmd = VIRTUAL_CMAC_PROCESS;
	else
		priv->cmd = VIRTUAL_SE_PROCESS;

	priv->se_dev = se_dev;
	init_completion(&priv->alg_complete);
	mutex_lock(&se_dev->server_lock);
	/* Return error if engine is in suspended state */
	if (atomic_read(&se_dev->se_suspended)) {
		mutex_unlock(&se_dev->server_lock);
		err = -ENODEV;
		goto unmap_exit;
	}

	err = tegra_hv_vse_safety_send_ivc(se_dev, pivck, ivc_req_msg,
			sizeof(struct tegra_virtual_se_ivc_msg_t));
	if (err) {
		mutex_unlock(&se_dev->server_lock);
		goto unmap_exit;
	}

	time_left = wait_for_completion_timeout(&priv->alg_complete,
			TEGRA_HV_VSE_TIMEOUT);
	mutex_unlock(&se_dev->server_lock);
	if (time_left == 0) {
		dev_err(se_dev->dev, "cmac_op timeout\n");
		err = -ETIMEDOUT;
	}

	if (is_last) {
		if (cmac_req_data->request_type == CMAC_SIGN) {
			if (priv->rx_status == 0) {
				memcpy(req->result,
						priv->cmac.data,
						TEGRA_VIRTUAL_SE_AES_CMAC_DIGEST_SIZE);
			}
		} else {
			if (priv->rx_status == 0)
				cmac_req_data->result = 0;
			else
				cmac_req_data->result = 1;
		}
	}

	if ((priv->rx_status != 0) &&
			(priv->rx_status != TEGRA_VIRTUAL_SE_ERR_MAC_INVALID)) {
		err = status_to_errno(priv->rx_status);
		dev_err(se_dev->dev, "%s: SE server returned error %u\n",
				__func__, priv->rx_status);
	}

unmap_exit:
	src_sg = req->src;
	while (src_sg && num_mapped_sgs--) {
		dma_unmap_sg(se_dev->dev, src_sg, 1, DMA_TO_DEVICE);
		src_sg = sg_next(src_sg);
	}
free_mem:
	devm_kfree(se_dev->dev, priv);
	devm_kfree(se_dev->dev, ivc_req_msg);

	return err;

}

static int tegra_hv_vse_safety_cmac_init(struct ahash_request *req)
{
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_AES0];
	struct crypto_ahash *tfm;
	struct tegra_virtual_se_aes_cmac_context *cmac_ctx;

	/* Return error if engine is in suspended state */
	if (atomic_read(&se_dev->se_suspended))
		return -ENODEV;

	if (!req) {
		dev_err(se_dev->dev, "AES-CMAC request not valid\n");
		return -EINVAL;
	}

	tfm = crypto_ahash_reqtfm(req);
	if (!tfm) {
		dev_err(se_dev->dev, "AES-CMAC transform not valid\n");
		return -EINVAL;
	}

	cmac_ctx = crypto_ahash_ctx(tfm);
	if (!cmac_ctx) {
		dev_err(se_dev->dev, "AES-CMAC req_ctx not valid\n");
		return -EINVAL;
	}

	cmac_ctx->digest_size = crypto_ahash_digestsize(tfm);
	cmac_ctx->hash_result = dma_alloc_coherent(
			se_dev->dev, TEGRA_VIRTUAL_SE_AES_CMAC_DIGEST_SIZE,
			&cmac_ctx->hash_result_addr, GFP_KERNEL);
	if (!cmac_ctx->hash_result) {
		dev_err(se_dev->dev, "Cannot allocate memory for cmac result\n");
		return -ENOMEM;
	}
	cmac_ctx->is_first = true;
	cmac_ctx->req_context_initialized = true;

	return 0;
}

static void tegra_hv_vse_safety_cmac_req_deinit(struct ahash_request *req)
{
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_AES0];
	struct tegra_virtual_se_aes_cmac_context *cmac_ctx;

	cmac_ctx = crypto_ahash_ctx(crypto_ahash_reqtfm(req));
	if (!cmac_ctx) {
		dev_err(se_dev->dev, "AES-CMAC req_ctx not valid\n");
		return;
	}

	dma_free_coherent(
		se_dev->dev, (TEGRA_HV_VSE_SHA_MAX_BLOCK_SIZE * 2),
		cmac_ctx->hash_result, cmac_ctx->hash_result_addr);
	cmac_ctx->hash_result = NULL;
	cmac_ctx->req_context_initialized = false;
}

static int tegra_hv_vse_safety_cmac_update(struct ahash_request *req)
{

	struct tegra_virtual_se_aes_cmac_context *cmac_ctx = NULL;
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_AES0];
	int ret = 0;

	if (!req) {
		dev_err(se_dev->dev, "AES-CMAC request not valid\n");
		return -EINVAL;
	}

	/* Return error if engine is in suspended state */
	if (atomic_read(&se_dev->se_suspended))
		return -ENODEV;

	cmac_ctx = crypto_ahash_ctx(crypto_ahash_reqtfm(req));
	if (!cmac_ctx) {
		dev_err(se_dev->dev, "AES-CMAC req_ctx not valid\n");
		return -EINVAL;
	}

	if (!cmac_ctx->req_context_initialized) {
		dev_err(se_dev->dev,
			"%s Request ctx not initialized\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&se_dev->mtx);

	/* Do not process data in given request */
	if (se_dev->chipdata->cmac_hw_padding_supported)
		ret = tegra_hv_vse_safety_cmac_sv_op(req, false);
	else
		ret = tegra_hv_vse_safety_cmac_op(req, false);
	if (ret)
		dev_err(se_dev->dev, "tegra_se_cmac_update failed - %d\n", ret);

	mutex_unlock(&se_dev->mtx);

	return ret;
}

static int tegra_hv_vse_safety_cmac_final(struct ahash_request *req)
{
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_AES0];

	/* Return error if engine is in suspended state */
	if (atomic_read(&se_dev->se_suspended))
		return -ENODEV;

	return 0;
}

static int tegra_hv_vse_safety_cmac_finup(struct ahash_request *req)
{
	struct tegra_virtual_se_aes_cmac_context *cmac_ctx = NULL;
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_AES0];
	int ret = 0;

	if (!req) {
		dev_err(se_dev->dev, "AES-CMAC request not valid\n");
		return -EINVAL;
	}

	/* Return error if engine is in suspended state */
	if (atomic_read(&se_dev->se_suspended))
		return -ENODEV;

	cmac_ctx = crypto_ahash_ctx(crypto_ahash_reqtfm(req));
	if (!cmac_ctx) {
		dev_err(se_dev->dev, "AES-CMAC req_ctx not valid\n");
		return -EINVAL;
	}

	if (!cmac_ctx->req_context_initialized) {
		dev_err(se_dev->dev,
			"%s Request ctx not initialized\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&se_dev->mtx);
	/* Do not process data in given request */
	if (se_dev->chipdata->cmac_hw_padding_supported)
		ret = tegra_hv_vse_safety_cmac_sv_op(req, true);
	else
		ret = tegra_hv_vse_safety_cmac_op(req, true);
	if (ret)
		dev_err(se_dev->dev, "tegra_se_cmac_finup failed - %d\n", ret);

	mutex_unlock(&se_dev->mtx);
	tegra_hv_vse_safety_cmac_req_deinit(req);

	return ret;
}


static int tegra_hv_vse_safety_cmac_digest(struct ahash_request *req)
{
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_AES0];

	/* Return error if engine is in suspended state */
	if (atomic_read(&se_dev->se_suspended))
		return -ENODEV;

	return tegra_hv_vse_safety_cmac_init(req) ?: tegra_hv_vse_safety_cmac_final(req);
}

static int tegra_hv_vse_safety_cmac_setkey(struct crypto_ahash *tfm, const u8 *key,
		unsigned int keylen)
{
	struct tegra_virtual_se_aes_cmac_context *ctx =
			crypto_tfm_ctx(crypto_ahash_tfm(tfm));
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_AES1];
	struct tegra_virtual_se_ivc_hdr_t *ivc_hdr;
	struct tegra_virtual_se_ivc_tx_msg_t *ivc_tx;
	struct tegra_hv_ivc_cookie *pivck = g_ivck;
	struct tegra_virtual_se_ivc_msg_t *ivc_req_msg;
	struct tegra_vse_priv_data *priv = NULL;
	struct tegra_vse_tag *priv_data_ptr;
	int err = 0;
	unsigned long time_left;
	s8 label[TEGRA_VIRTUAL_SE_AES_MAX_KEY_SIZE];
	u32 slot;
	bool is_keyslot_label;

	if (!ctx)
		return -EINVAL;

	if (keylen != 16) {
		dev_err(se_dev->dev, "%s: Unsupported key length: %d", __func__, keylen);
		return -EINVAL;
	}

	/* format: 'NVSEAES 1234567\0' */
	is_keyslot_label = strnlen(key, keylen) <= keylen &&
		sscanf(key, "%s %x", label, &slot) == 2 &&
		!strcmp(label, TEGRA_VIRTUAL_SE_AES_KEYSLOT_LABEL);

	if (is_keyslot_label) {
		ctx->keylen = keylen;
		ctx->aes_keyslot = (u32)slot;
		ctx->is_key_slot_allocated = true;
	} else {
		dev_err(se_dev->dev, "%s: Invalid keyslot label %s\n", __func__, key);
		return -EINVAL;
	}

	if (se_dev->chipdata->cmac_hw_padding_supported == false) {
		ivc_req_msg = devm_kzalloc(se_dev->dev, sizeof(*ivc_req_msg),
						GFP_KERNEL);
		if (!ivc_req_msg)
			return -ENOMEM;

		priv = devm_kzalloc(se_dev->dev, sizeof(*priv), GFP_KERNEL);
		if (!priv) {
			devm_kfree(se_dev->dev, ivc_req_msg);
			dev_err(se_dev->dev, "Priv Data allocation failed\n");
			return -ENOMEM;
		}

		ivc_hdr = &ivc_req_msg->ivc_hdr;
		ivc_tx = &ivc_req_msg->tx[0];
		ivc_hdr->num_reqs = 1;
		ivc_hdr->header_magic[0] = 'N';
		ivc_hdr->header_magic[1] = 'V';
		ivc_hdr->header_magic[2] = 'D';
		ivc_hdr->header_magic[3] = 'A';

		vse_thread_start = true;

		ivc_hdr->engine = VIRTUAL_SE_AES0;
		ivc_tx->cmd = TEGRA_VIRTUAL_SE_CMD_AES_CMAC_GEN_SUBKEY;
		ivc_tx->aes.op_cmac_subkey_s.keyslot = ctx->aes_keyslot;
		ivc_tx->aes.op_cmac_subkey_s.key_length = ctx->keylen;
		priv_data_ptr =
			(struct tegra_vse_tag *)ivc_req_msg->ivc_hdr.tag;
		priv_data_ptr->priv_data = (unsigned int *)priv;
		priv->cmd = VIRTUAL_SE_PROCESS;
		priv->se_dev = se_dev;
		init_completion(&priv->alg_complete);

		mutex_lock(&se_dev->server_lock);
		/* Return error if engine is in suspended state */
		if (atomic_read(&se_dev->se_suspended)) {
			mutex_unlock(&se_dev->server_lock);
			err = -ENODEV;
			goto free_exit;
		}
		err = tegra_hv_vse_safety_send_ivc(se_dev, pivck, ivc_req_msg,
				sizeof(struct tegra_virtual_se_ivc_msg_t));
		if (err) {
			mutex_unlock(&se_dev->server_lock);
			goto free_exit;
		}

		time_left = wait_for_completion_timeout(
				&priv->alg_complete,
				TEGRA_HV_VSE_TIMEOUT);
		mutex_unlock(&se_dev->server_lock);
		if (time_left == 0) {
			dev_err(se_dev->dev, "%s timeout\n",
				__func__);
			err = -ETIMEDOUT;
			goto free_exit;
		}

free_exit:
		devm_kfree(se_dev->dev, priv);
		devm_kfree(se_dev->dev, ivc_req_msg);
	}

	return err;
}

static int tegra_hv_vse_safety_cmac_cra_init(struct crypto_tfm *tfm)
{
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_AES0];

	/* Return error if engine is in suspended state */
	if (atomic_read(&se_dev->se_suspended))
		return -ENODEV;

	crypto_ahash_set_reqsize(__crypto_ahash_cast(tfm),
			 sizeof(struct tegra_virtual_se_aes_cmac_context));

	return 0;
}

static void tegra_hv_vse_safety_cmac_cra_exit(struct crypto_tfm *tfm)
{
	/* nothing to do as user releases the keyslot through tzvault TA */
}

static int tegra_hv_vse_safety_aes_setkey(struct crypto_skcipher *tfm,
	const u8 *key, u32 keylen)
{
	struct tegra_virtual_se_aes_context *ctx = crypto_skcipher_ctx(tfm);
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_AES1];
	s8 label[TEGRA_VIRTUAL_SE_AES_MAX_KEY_SIZE];
	u32 slot;
	int err = 0;
	bool is_keyslot_label;

	if (!ctx)
		return -EINVAL;

	if ((keylen != 16) && (keylen != 32)) {
		dev_err(se_dev->dev, "%s: Unsupported key length: %d", __func__, keylen);
		return -EINVAL;
	}

	/* format: 'NVSEAES 1234567\0' */
	is_keyslot_label = strnlen(key, keylen) <= keylen &&
		sscanf(key, "%s %x", label, &slot) == 2 &&
		!strcmp(label, TEGRA_VIRTUAL_SE_AES_KEYSLOT_LABEL);

	if (is_keyslot_label) {
		ctx->keylen = keylen;
		ctx->aes_keyslot = (u32)slot;
		ctx->is_key_slot_allocated = true;
	} else {
		dev_err(se_dev->dev, "%s: Invalid keyslot label %s", __func__, key);
		err = -EINVAL;
	}

	return err;
}

static int tegra_hv_vse_safety_rng_drbg_init(struct crypto_tfm *tfm)
{
	struct tegra_virtual_se_rng_context *rng_ctx = crypto_tfm_ctx(tfm);
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_AES0];

	/* Return error if engine is in suspended state */
	if (atomic_read(&se_dev->se_suspended))
		return -ENODEV;

	rng_ctx->se_dev = se_dev;
	rng_ctx->rng_buf =
		dma_alloc_coherent(rng_ctx->se_dev->dev,
				TEGRA_VIRTUAL_SE_RNG_DT_SIZE,
		&rng_ctx->rng_buf_adr, GFP_KERNEL);
	if (!rng_ctx->rng_buf) {
		dev_err(se_dev->dev, "can not allocate rng dma buffer");
		return -ENOMEM;
	}

	return 0;
}

static void tegra_hv_vse_safety_rng_drbg_exit(struct crypto_tfm *tfm)
{
	struct tegra_virtual_se_rng_context *rng_ctx = crypto_tfm_ctx(tfm);

	if (rng_ctx->rng_buf) {
		dma_free_coherent(rng_ctx->se_dev->dev,
			TEGRA_VIRTUAL_SE_RNG_DT_SIZE, rng_ctx->rng_buf,
			rng_ctx->rng_buf_adr);
	}
	rng_ctx->se_dev = NULL;
}

static int tegra_hv_vse_safety_rng_drbg_get_random(struct crypto_rng *tfm,
	const u8 *src, unsigned int slen, u8 *rdata, unsigned int dlen)
{
	struct tegra_virtual_se_rng_context *rng_ctx = crypto_rng_ctx(tfm);
	struct tegra_virtual_se_dev *se_dev = rng_ctx->se_dev;
	u8 *rdata_addr;
	int err = 0, j, num_blocks, data_len = 0;
	struct tegra_virtual_se_ivc_tx_msg_t *ivc_tx;
	struct tegra_hv_ivc_cookie *pivck = g_ivck;
	struct tegra_virtual_se_ivc_msg_t *ivc_req_msg;
	struct tegra_virtual_se_ivc_hdr_t *ivc_hdr = NULL;
	struct tegra_vse_priv_data *priv = NULL;
	struct tegra_vse_tag *priv_data_ptr;
	int time_left;

	if (dlen == 0) {
		return -EINVAL;
	}

	num_blocks = (dlen / TEGRA_VIRTUAL_SE_RNG_DT_SIZE);
	data_len = (dlen % TEGRA_VIRTUAL_SE_RNG_DT_SIZE);
	if (data_len == 0)
		num_blocks = num_blocks - 1;

	ivc_req_msg = devm_kzalloc(se_dev->dev,
				sizeof(*ivc_req_msg),
				GFP_KERNEL);
	if (!ivc_req_msg)
		return -ENOMEM;

	priv = devm_kzalloc(se_dev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(se_dev->dev, "Priv Data allocation failed\n");
		devm_kfree(se_dev->dev, ivc_req_msg);
		return 0;
	}

	ivc_tx = &ivc_req_msg->tx[0];
	ivc_hdr = &ivc_req_msg->ivc_hdr;
	ivc_hdr->num_reqs = 1;
	ivc_hdr->header_magic[0] = 'N';
	ivc_hdr->header_magic[1] = 'V';
	ivc_hdr->header_magic[2] = 'D';
	ivc_hdr->header_magic[3] = 'A';
	ivc_hdr->engine = VIRTUAL_SE_AES0;
	priv_data_ptr = (struct tegra_vse_tag *)ivc_hdr->tag;
	priv_data_ptr->priv_data = (unsigned int *)priv;
	priv->cmd = VIRTUAL_SE_PROCESS;
	priv->se_dev = se_dev;

	ivc_tx->cmd = TEGRA_VIRTUAL_SE_CMD_AES_RNG_DBRG;

	for (j = 0; j <= num_blocks; j++) {
		ivc_tx->aes.op_rng.dst_addr.lo = rng_ctx->rng_buf_adr & 0xFFFFFFFF;
		ivc_tx->aes.op_rng.dst_addr.hi = (rng_ctx->rng_buf_adr >> 32)
				| TEGRA_VIRTUAL_SE_RNG_DT_SIZE;
		init_completion(&priv->alg_complete);
		vse_thread_start = true;

		mutex_lock(&se_dev->server_lock);
		/* Return error if engine is in suspended state */
		if (atomic_read(&se_dev->se_suspended)) {
			mutex_unlock(&se_dev->server_lock);
			dlen = 0;
			goto exit;
		}
		err = tegra_hv_vse_safety_send_ivc(se_dev, pivck, ivc_req_msg,
				sizeof(struct tegra_virtual_se_ivc_msg_t));
		if (err) {
			mutex_unlock(&se_dev->server_lock);
			dlen = 0;
			goto exit;
		}

		time_left = wait_for_completion_timeout(&priv->alg_complete,
				TEGRA_HV_VSE_TIMEOUT);
		mutex_unlock(&se_dev->server_lock);
		if (time_left == 0) {
			dev_err(se_dev->dev, "%s timeout\n", __func__);
			dlen = 0;
			goto exit;
		}

		rdata_addr =
			(rdata + (j * TEGRA_VIRTUAL_SE_RNG_DT_SIZE));
		if (data_len && num_blocks == j) {
			memcpy(rdata_addr, rng_ctx->rng_buf, data_len);
		} else {
			memcpy(rdata_addr,
				rng_ctx->rng_buf,
				TEGRA_VIRTUAL_SE_RNG_DT_SIZE);
		}
	}
exit:
	devm_kfree(se_dev->dev, priv);
	devm_kfree(se_dev->dev, ivc_req_msg);
	return dlen;
}

static int tegra_hv_vse_safety_rng_drbg_reset(struct crypto_rng *tfm,
	const u8 *seed, unsigned int slen)
{
	return 0;
}

static int tegra_vse_aes_gcm_setkey(struct crypto_aead *tfm, const u8 *key,
	u32 keylen)
{
	/* copied from normal aes keyset, will remove if no modification needed*/
	struct tegra_virtual_se_aes_context *ctx = crypto_aead_ctx(tfm);
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_AES1];
	s8 label[TEGRA_VIRTUAL_SE_AES_MAX_KEY_SIZE];
	u32 slot;
	int err = 0;
	bool is_keyslot_label;

	if (!ctx)
		return -EINVAL;

	if ((keylen != 16) && (keylen != 32)) {
		dev_err(se_dev->dev, "%s: Unsupported key length: %d", __func__, keylen);
		return -EINVAL;
	}

	/* format: 'NVSEAES 1234567\0' */
	is_keyslot_label = strnlen(key, keylen) <= keylen &&
		sscanf(key, "%s %x", label, &slot) == 2 &&
		!strcmp(label, TEGRA_VIRTUAL_SE_AES_KEYSLOT_LABEL);

	if (is_keyslot_label) {
		ctx->keylen = keylen;
		ctx->aes_keyslot = (u32)slot;
		ctx->is_key_slot_allocated = true;
	} else {
		dev_err(se_dev->dev, "%s: Invalid keyslot label %s\n", __func__, key);
		err = -EINVAL;
	}

	return err;
}

static int tegra_vse_aes_gcm_setauthsize(struct crypto_aead *tfm,
	unsigned int authsize)
{
	struct tegra_virtual_se_aes_context *ctx = crypto_aead_ctx(tfm);

	switch (authsize) {
	case 16:
		ctx->authsize = authsize;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int tegra_vse_aes_gcm_init(struct crypto_aead *tfm)
{
	return 0;
}

static void tegra_vse_aes_gcm_exit(struct crypto_aead *tfm)
{
	/* nothing to do as user unloads the key manually with tzvault*/
}

static int tegra_vse_aes_gcm_check_params(struct aead_request *req,
							bool encrypt)
{
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct tegra_virtual_se_aes_context *aes_ctx = crypto_aead_ctx(tfm);
	uint32_t cryptlen;
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_AES1];

	if (aes_ctx->authsize != TEGRA_VIRTUAL_SE_AES_GCM_TAG_SIZE) {
		dev_err(se_dev->dev,
			"Wrong GCM authsize, expected: 0x%x received: 0x%x\n",
				TEGRA_VIRTUAL_SE_AES_GCM_TAG_SIZE,
				aes_ctx->authsize);
		return -EINVAL;
	}


	if (!encrypt) {
		if (req->cryptlen < aes_ctx->authsize) {
			dev_err(se_dev->dev, "%s: gcm_dec cryptlen is invalid\n", __func__);
			return -EINVAL;
		}

		cryptlen = req->cryptlen - aes_ctx->authsize;
		if (cryptlen > TEGRA_VIRTUAL_SE_MAX_GCMDEC_BUFLEN) {
			dev_err(se_dev->dev, "%s: dec srcbuf len is invalid %d\n", __func__,
											cryptlen);
			return -EINVAL;
		}

	} else {
		cryptlen = req->cryptlen;
		if (cryptlen > TEGRA_VIRTUAL_SE_MAX_SUPPORTED_BUFLEN) {
			dev_err(se_dev->dev, "%s: enc srcbuf len is invalid %d\n", __func__,
											cryptlen);
			return -EINVAL;
		}
	}

	if (req->assoclen > TEGRA_VIRTUAL_SE_MAX_SUPPORTED_BUFLEN) {
		dev_err(se_dev->dev, "%s: assoclen is invalid\n", __func__);
		return -EINVAL;
	}

	if (unlikely(!aes_ctx->is_key_slot_allocated)) {
		dev_err(se_dev->dev, "%s: AES Key slot not allocated\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int tegra_vse_aes_gcm_enc_dec(struct aead_request *req, bool encrypt)
{
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct tegra_virtual_se_aes_context *aes_ctx = crypto_aead_ctx(tfm);
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_AES1];
	struct tegra_virtual_se_ivc_msg_t *ivc_req_msg = NULL;
	struct tegra_virtual_se_ivc_hdr_t *ivc_hdr;
	struct tegra_virtual_se_ivc_tx_msg_t *ivc_tx;
	struct tegra_hv_ivc_cookie *pivck = g_ivck;
	struct tegra_vse_priv_data *priv = NULL;
	struct tegra_vse_tag *priv_data_ptr;
	int err = 0;
	int time_left;
	uint32_t cryptlen = 0;

	void *aad_buf = NULL;
	void *src_buf = NULL;
	void *tag_buf = NULL;

	dma_addr_t aad_buf_addr;
	dma_addr_t src_buf_addr;
	dma_addr_t tag_buf_addr;

	mutex_lock(&se_dev->server_lock);
	/* Return error if mempool is being used for another operation */
	if (atomic_read(&se_dev->mempoolbuf_in_use)) {
		mutex_unlock(&se_dev->server_lock);
		dev_err(se_dev->dev, "%s: mempool is in use\n", __func__);
		err = -EPERM;
		goto exit;
	}
	atomic_set(&se_dev->mempoolbuf_in_use, true);
	mutex_unlock(&se_dev->server_lock);

	err = tegra_vse_aes_gcm_check_params(req, encrypt);
	if (err != 0)
		goto free_exit;

	if (encrypt)
		cryptlen = req->cryptlen;
	else
		cryptlen = req->cryptlen - aes_ctx->authsize;

	if (req->assoclen > 0) {
		aad_buf = dma_alloc_coherent(se_dev->dev, req->assoclen,
					&aad_buf_addr, GFP_KERNEL);
		if (!aad_buf) {
			err = -ENOMEM;
			goto free_exit;
		}
		/* copy aad from sgs to buffer*/
		sg_pcopy_to_buffer(req->src, (u32)sg_nents(req->src),
				aad_buf, req->assoclen,
				0);
	}

	if (cryptlen > 0) {
		if (encrypt) {
			src_buf = dma_alloc_coherent(se_dev->dev, cryptlen,
						&src_buf_addr, GFP_KERNEL);
			if (!src_buf) {
				err = -ENOMEM;
				goto free_exit;
			}
		} else {
			src_buf = mempool_buf;
			/* For GCM decrypt buffer address represents offset */
			src_buf_addr = (dma_addr_t)0UL;
		}
		/* copy src from sgs to buffer*/
		sg_pcopy_to_buffer(req->src, (u32)sg_nents(req->src),
				src_buf, cryptlen,
				req->assoclen);
	}

	if (encrypt) {
		tag_buf = dma_alloc_coherent(se_dev->dev, aes_ctx->authsize,
					&tag_buf_addr, GFP_KERNEL);
		if (!tag_buf) {
			err = -ENOMEM;
			goto free_exit;
		}
	}

	priv = devm_kzalloc(se_dev->dev, sizeof(*priv), GFP_KERNEL);

	if (!priv) {
		err = -ENOMEM;
		goto free_exit;
	}

	ivc_req_msg = devm_kzalloc(se_dev->dev, sizeof(*ivc_req_msg),
						GFP_KERNEL);
	if (!ivc_req_msg) {
		err = -ENOMEM;
		goto free_exit;
	}

	ivc_tx = &ivc_req_msg->tx[0];
	ivc_hdr = &ivc_req_msg->ivc_hdr;
	ivc_hdr->num_reqs = 1;
	ivc_hdr->header_magic[0] = 'N';
	ivc_hdr->header_magic[1] = 'V';
	ivc_hdr->header_magic[2] = 'D';
	ivc_hdr->header_magic[3] = 'A';
	ivc_hdr->engine = VIRTUAL_SE_AES1;
	priv_data_ptr = (struct tegra_vse_tag *)ivc_hdr->tag;
	priv_data_ptr->priv_data = (unsigned int *)priv;

	priv->se_dev = se_dev;

	vse_thread_start = true;

	ivc_tx->aes.op_gcm.keyslot = aes_ctx->aes_keyslot;
	ivc_tx->aes.op_gcm.key_length = aes_ctx->keylen;

	if (encrypt) {
		/*
		 * If first byte of iv is 1 and the request is for AES CBC/CTR encryption,
		 * it means that generation of random IV is required.
		 */
		if (req->iv[0] == 1) {
			//Random IV generation is required
			ivc_tx->cmd = TEGRA_VIRTUAL_SE_CMD_AES_ENCRYPT_INIT;
			priv->cmd = VIRTUAL_SE_PROCESS;
			init_completion(&priv->alg_complete);
			mutex_lock(&se_dev->server_lock);
			err = tegra_hv_vse_safety_send_ivc(se_dev, pivck, ivc_req_msg,
					sizeof(struct tegra_virtual_se_ivc_msg_t));
			if (err) {
				dev_err(se_dev->dev,
						"\n %s send ivc failed %d\n", __func__, err);
				mutex_unlock(&se_dev->server_lock);
				goto free_exit;
			}
			time_left = wait_for_completion_timeout(&priv->alg_complete,
					TEGRA_HV_VSE_TIMEOUT);
			if (time_left == 0) {
				dev_err(se_dev->dev, "%s timeout\n", __func__);
				err = -ETIMEDOUT;
				mutex_unlock(&se_dev->server_lock);
				goto free_exit;
			}
			mutex_unlock(&se_dev->server_lock);
			err = status_to_errno(priv->rx_status);
			if (err) {
				dev_err(se_dev->dev,
					"\n %s IV generation failed %d\n", __func__, err);
				goto free_exit;
			}
		}
		priv->cmd = VIRTUAL_SE_AES_GCM_ENC_PROCESS;
		ivc_tx->cmd = TEGRA_VIRTUAL_SE_CMD_AES_GCM_CMD_ENCRYPT;
	} else {
		priv->cmd = VIRTUAL_SE_PROCESS;
		ivc_tx->cmd = TEGRA_VIRTUAL_SE_CMD_AES_GCM_CMD_DECRYPT;
	}

	if (!encrypt) {
		/* copy iv for decryption*/
		memcpy(ivc_tx->aes.op_gcm.iv, req->iv, crypto_aead_ivsize(tfm));

		/* copy expected tag */
		sg_pcopy_to_buffer(req->src, (u32)sg_nents(req->src),
			ivc_tx->aes.op_gcm.expected_tag, TEGRA_VIRTUAL_SE_AES_GCM_TAG_SIZE,
			req->assoclen + cryptlen);
	}

	ivc_tx->aes.op_gcm.src_addr_hi = cryptlen;
	ivc_tx->aes.op_gcm.dst_addr_hi = cryptlen;
	if (cryptlen > 0) {
		ivc_tx->aes.op_gcm.src_addr_lo = (uint32_t)src_buf_addr;
		ivc_tx->aes.op_gcm.src_addr_hi |= (uint32_t)((src_buf_addr >> 8)
						& ~((1U << 24) - 1U));

		/* same source buffer can be used for destination buffer */
		ivc_tx->aes.op_gcm.dst_addr_lo = ivc_tx->aes.op_gcm.src_addr_lo;
		ivc_tx->aes.op_gcm.dst_addr_hi = ivc_tx->aes.op_gcm.src_addr_hi;
	}

	ivc_tx->aes.op_gcm.aad_addr_hi = req->assoclen;
	if (req->assoclen > 0)
		ivc_tx->aes.op_gcm.aad_addr_lo = aad_buf_addr;

	if (encrypt) {
		ivc_tx->aes.op_gcm.tag_addr_hi = aes_ctx->authsize;
		ivc_tx->aes.op_gcm.tag_addr_lo = tag_buf_addr;
	}

	init_completion(&priv->alg_complete);
	mutex_lock(&se_dev->server_lock);
	/* Return error if engine is in suspended state */
	if (atomic_read(&se_dev->se_suspended)) {
		mutex_unlock(&se_dev->server_lock);
		err = -ENODEV;
		goto free_exit;
	}
	err = tegra_hv_vse_safety_send_ivc(se_dev, pivck, ivc_req_msg,
			sizeof(struct tegra_virtual_se_ivc_msg_t));
	if (err) {
		mutex_unlock(&se_dev->server_lock);
		goto free_exit;
	}

	time_left = wait_for_completion_timeout(&priv->alg_complete,
			TEGRA_HV_VSE_TIMEOUT);
	mutex_unlock(&se_dev->server_lock);
	if (time_left == 0) {
		dev_err(se_dev->dev, "%s: completion timeout\n", __func__);
		err = -ETIMEDOUT;
		goto free_exit;
	}

	if (priv->rx_status != 0) {
		dev_err(se_dev->dev, "%s: SE Server returned error %u\n", __func__,
									priv->rx_status);
		err = status_to_errno(priv->rx_status);
		goto free_exit;
	}

	if (encrypt) {
		/* copy iv to req for encryption*/
		memcpy(req->iv, priv->iv, crypto_aead_ivsize(tfm));

		/* copy tag to req for encryption */
		sg_pcopy_from_buffer(req->dst, sg_nents(req->dst),
			tag_buf, aes_ctx->authsize,
			req->assoclen + cryptlen);
	}

	sg_pcopy_from_buffer(req->dst, sg_nents(req->dst),
		src_buf, cryptlen, req->assoclen);

free_exit:
	atomic_set(&se_dev->mempoolbuf_in_use, false);

	if (ivc_req_msg)
		devm_kfree(se_dev->dev, ivc_req_msg);

	if (priv)
		devm_kfree(se_dev->dev, priv);

	if (tag_buf)
		dma_free_coherent(se_dev->dev, aes_ctx->authsize, tag_buf,
				tag_buf_addr);

	if (src_buf && encrypt)
		dma_free_coherent(se_dev->dev, cryptlen, src_buf,
				src_buf_addr);

	if (aad_buf)
		dma_free_coherent(se_dev->dev, req->assoclen, aad_buf,
				aad_buf_addr);

exit:
	return err;
}

static int tegra_vse_aes_gcm_encrypt(struct aead_request *req)
{
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_AES1];
	int err = 0;

	if (!req) {
		dev_err(se_dev->dev, "%s: req is invalid\n", __func__);
		return -EINVAL;
	}
	if (unlikely(!req->iv)) {
		/* If IV is not set we cannot determine whether
		 * random IV generation is required.
		 */
		pr_err("%s: Unable to determine if random IV generation is needed\n", __func__);
		return -EINVAL;
	}

	err = tegra_vse_aes_gcm_enc_dec(req, true);
	if (err)
		dev_err(se_dev->dev, "%s failed %d\n", __func__, err);

	return err;
}

static int tegra_vse_aes_gcm_decrypt(struct aead_request *req)
{
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_AES1];
	int err = 0;

	if (!req) {
		dev_err(se_dev->dev, "%s: req is invalid\n", __func__);
		return -EINVAL;
	}

	err = tegra_vse_aes_gcm_enc_dec(req, false);
	if (err)
		dev_err(se_dev->dev, "%s failed %d\n", __func__, err);

	return err;
}

static int tegra_hv_vse_safety_gmac_cra_init(struct crypto_tfm *tfm)
{
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_AES0];

	/* Return error if engine is in suspended state */
	if (atomic_read(&se_dev->se_suspended))
		return -ENODEV;

	crypto_ahash_set_reqsize(__crypto_ahash_cast(tfm),
			 sizeof(struct tegra_virtual_se_aes_gmac_context));

	return 0;
}

static void tegra_hv_vse_safety_gmac_cra_exit(struct crypto_tfm *tfm)
{
	/* nothing to do as user releases the keyslot through tzvault TA */
}

static int tegra_hv_vse_aes_gmac_setkey(struct crypto_ahash *tfm, const u8 *key,
		unsigned int keylen)
{
	struct tegra_virtual_se_aes_gmac_context *ctx = crypto_ahash_ctx(tfm);
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_AES0];
	s8 label[TEGRA_VIRTUAL_SE_AES_KEYSLOT_LABEL_SIZE];
	u32 slot;
	int err = 0;
	bool is_keyslot_label;

	if (!ctx) {
		dev_err(se_dev->dev, "%s: gmac ctx invalid", __func__);
		err = -EINVAL;
		goto exit;
	}

	if ((keylen != 16) && (keylen != 32)) {
		dev_err(se_dev->dev, "%s: Unsupported key length: %d", __func__, keylen);
		err = -EINVAL;
		goto exit;
	}

	/* format: 'NVSEAES 1234567\0' */
	is_keyslot_label =
		(strnlen(key, TEGRA_VIRTUAL_SE_AES_KEYSLOT_LABEL_SIZE)
			<= TEGRA_VIRTUAL_SE_AES_KEYSLOT_LABEL_SIZE) &&
		(sscanf(key, "%s %x", label, &slot) == 2) &&
		(!strcmp(label, TEGRA_VIRTUAL_SE_AES_KEYSLOT_LABEL));

	if (is_keyslot_label) {
		ctx->keylen = keylen;
		ctx->aes_keyslot = (u32)slot;
		ctx->is_key_slot_allocated = true;
	} else {
		dev_err(se_dev->dev,
			"\n %s: Invalid keyslot label: %s\n", __func__, key);
		err = -EINVAL;
	}

exit:
	return err;
}

static int tegra_hv_vse_aes_gmac_sv_init(struct ahash_request *req)
{
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_AES0];
	struct crypto_ahash *tfm = NULL;
	struct tegra_virtual_se_aes_gmac_context *gmac_ctx = NULL;
	struct tegra_vse_gmac_req_data *gmac_req_data = NULL;
	struct tegra_virtual_se_ivc_msg_t *ivc_req_msg = NULL;
	struct tegra_virtual_se_ivc_hdr_t *ivc_hdr = NULL;
	struct tegra_virtual_se_ivc_tx_msg_t *ivc_tx = NULL;
	struct tegra_hv_ivc_cookie *pivck = g_ivck;
	struct tegra_vse_tag *priv_data_ptr = NULL;
	struct tegra_vse_priv_data *priv = NULL;
	int err = 0;
	u64 time_left;

	/* Return error if engine is in suspended state */
	if (atomic_read(&se_dev->se_suspended)) {
		dev_err(se_dev->dev, "%s: engine is in suspended state", __func__);
		err = -ENODEV;
		goto exit;
	}

	if (!req) {
		dev_err(se_dev->dev, "%s: request invalid\n", __func__);
		err = -EINVAL;
		goto exit;
	}

	tfm = crypto_ahash_reqtfm(req);
	if (!tfm) {
		dev_err(se_dev->dev, "%s: transform not valid\n", __func__);
		err = -EINVAL;
		goto exit;
	}

	gmac_ctx = crypto_ahash_ctx(crypto_ahash_reqtfm(req));
	if (!gmac_ctx) {
		dev_err(se_dev->dev, "%s: req ctx invalid\n", __func__);
		err = -EINVAL;
		goto exit;
	}

	if (gmac_ctx->is_key_slot_allocated == false) {
		dev_err(se_dev->dev, "%s: keyslot is not allocated\n", __func__);
		err = -EPERM;
		goto exit;
	}

	gmac_req_data = (struct tegra_vse_gmac_req_data *) req->priv;

	if ((gmac_req_data->request_type == GMAC_VERIFY)
			|| (gmac_req_data->request_type == GMAC_SIGN)) {
		/* Initialize GMAC ctx */
		gmac_ctx->authsize = crypto_ahash_digestsize(tfm);
		gmac_ctx->req_context_initialized = true;
		/* Exit as GMAC_INIT request need not be sent to SE Server for SIGN/VERIFY */
		err = 0;
		goto exit;
	}

	priv = devm_kzalloc(se_dev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		err = -ENOMEM;
		goto exit;
	}

	ivc_req_msg = devm_kzalloc(se_dev->dev, sizeof(*ivc_req_msg),
						GFP_KERNEL);
	if (!ivc_req_msg) {
		err = -ENOMEM;
		goto free_exit;
	}

	ivc_tx = &ivc_req_msg->tx[0];
	ivc_hdr = &ivc_req_msg->ivc_hdr;
	ivc_hdr->num_reqs = 1;
	ivc_hdr->header_magic[0] = 'N';
	ivc_hdr->header_magic[1] = 'V';
	ivc_hdr->header_magic[2] = 'D';
	ivc_hdr->header_magic[3] = 'A';
	ivc_hdr->engine = VIRTUAL_SE_AES0;
	priv_data_ptr = (struct tegra_vse_tag *)ivc_hdr->tag;
	priv_data_ptr->priv_data = (unsigned int *)priv;
	priv->cmd = VIRTUAL_SE_AES_GCM_ENC_PROCESS;
	priv->se_dev = se_dev;

	ivc_tx->cmd = TEGRA_VIRTUAL_SE_CMD_AES_GMAC_CMD_INIT;
	ivc_tx->aes.op_gcm.keyslot = gmac_ctx->aes_keyslot;
	ivc_tx->aes.op_gcm.key_length = gmac_ctx->keylen;

	vse_thread_start = true;
	init_completion(&priv->alg_complete);
	mutex_lock(&se_dev->server_lock);
	/* Return error if engine is in suspended state */
	if (atomic_read(&se_dev->se_suspended)) {
		mutex_unlock(&se_dev->server_lock);
		dev_err(se_dev->dev, "%s: engine is in suspended state", __func__);
		err = -ENODEV;
		goto free_exit;
	}
	err = tegra_hv_vse_safety_send_ivc(se_dev, pivck, ivc_req_msg,
			sizeof(struct tegra_virtual_se_ivc_msg_t));
	if (err) {
		dev_err(se_dev->dev, "%s: send_ivc failed %d\n", __func__, err);
		mutex_unlock(&se_dev->server_lock);
		goto free_exit;
	}

	time_left = wait_for_completion_timeout(&priv->alg_complete,
			TEGRA_HV_VSE_TIMEOUT);
	mutex_unlock(&se_dev->server_lock);
	if (time_left == 0UL) {
		dev_err(se_dev->dev, "%s: completion timeout\n", __func__);
		err = -ETIMEDOUT;
		goto free_exit;
	}

	if (priv->rx_status != 0) {
		dev_err(se_dev->dev, "%s: SE server returned error %u\n", __func__,
									priv->rx_status);
		err = status_to_errno(priv->rx_status);
		goto free_exit;
	}

	memcpy(gmac_req_data->iv, priv->iv, TEGRA_VIRTUAL_SE_AES_GCM_IV_SIZE);

free_exit:
	if (ivc_req_msg)
		devm_kfree(se_dev->dev, ivc_req_msg);

	if (priv)
		devm_kfree(se_dev->dev, priv);

exit:
	return err;
}

static void tegra_hv_vse_aes_gmac_deinit(struct ahash_request *req)
{
	struct tegra_virtual_se_aes_gmac_context *gmac_ctx =
					crypto_ahash_ctx(crypto_ahash_reqtfm(req));

	gmac_ctx->is_key_slot_allocated = false;
	gmac_ctx->req_context_initialized = false;
}

int tegra_vse_aes_gmac_sv_check_params(struct ahash_request *req)
{
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_AES0];
	int err = 0;

	/* Validate aad buf len */
	if (req->nbytes > TEGRA_VIRTUAL_SE_MAX_SUPPORTED_BUFLEN) {
		dev_err(se_dev->dev, "%s: aad buf length exceeds max supported size\n", __func__);
		err = -EINVAL;
	}

	return err;
}

static int tegra_hv_vse_aes_gmac_sv_op(struct ahash_request *req, bool is_last)
{
	struct tegra_virtual_se_aes_gmac_context *gmac_ctx = NULL;
	struct tegra_vse_gmac_req_data *gmac_req_data;
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_AES0];
	struct tegra_virtual_se_ivc_msg_t *ivc_req_msg = NULL;
	struct tegra_virtual_se_ivc_hdr_t *ivc_hdr;
	struct tegra_virtual_se_ivc_tx_msg_t *ivc_tx;
	struct tegra_hv_ivc_cookie *pivck = g_ivck;
	struct tegra_vse_priv_data *priv = NULL;
	struct tegra_vse_tag *priv_data_ptr;
	void *aad_buf = NULL;
	void *tag_buf = NULL;
	dma_addr_t aad_buf_addr;
	dma_addr_t tag_buf_addr;
	int err = 0;
	u64 time_left;

	gmac_ctx = crypto_ahash_ctx(crypto_ahash_reqtfm(req));
	if (!gmac_ctx) {
		dev_err(se_dev->dev, "%s: transform not valid\n", __func__);
		err = -EINVAL;
		goto exit;
	}

	gmac_req_data = (struct tegra_vse_gmac_req_data *) req->priv;

	err = tegra_vse_aes_gmac_sv_check_params(req);
	if (err != 0)
		goto exit;

	aad_buf = dma_alloc_coherent(se_dev->dev, req->nbytes,
				&aad_buf_addr, GFP_KERNEL);
	if (!aad_buf) {
		err = -ENOMEM;
		goto exit;
	}
	/* copy aad from sgs to buffer*/
	sg_pcopy_to_buffer(req->src, (u32)sg_nents(req->src),
			aad_buf, req->nbytes, 0);

	if (gmac_req_data->request_type == GMAC_SIGN) {
		tag_buf = dma_alloc_coherent(se_dev->dev, gmac_ctx->authsize,
					&tag_buf_addr, GFP_KERNEL);
		if (!tag_buf) {
			err = -ENOMEM;
			goto free_exit;
		}
	}

	priv = devm_kzalloc(se_dev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		err = -ENOMEM;
		goto free_exit;
	}

	ivc_req_msg = devm_kzalloc(se_dev->dev, sizeof(*ivc_req_msg),
						GFP_KERNEL);
	if (!ivc_req_msg) {
		err = -ENOMEM;
		goto free_exit;
	}

	ivc_tx = &ivc_req_msg->tx[0];
	ivc_hdr = &ivc_req_msg->ivc_hdr;
	ivc_hdr->num_reqs = 1;
	ivc_hdr->header_magic[0] = 'N';
	ivc_hdr->header_magic[1] = 'V';
	ivc_hdr->header_magic[2] = 'D';
	ivc_hdr->header_magic[3] = 'A';
	ivc_hdr->engine = VIRTUAL_SE_AES0;

	priv_data_ptr = (struct tegra_vse_tag *)ivc_hdr->tag;
	priv_data_ptr->priv_data = (unsigned int *)priv;
	priv->cmd = VIRTUAL_SE_AES_GCM_ENC_PROCESS;
	priv->se_dev = se_dev;

	if (gmac_req_data->request_type == GMAC_SIGN)
		ivc_tx->cmd = TEGRA_VIRTUAL_SE_CMD_AES_GMAC_CMD_SIGN;
	else
		ivc_tx->cmd = TEGRA_VIRTUAL_SE_CMD_AES_GMAC_CMD_VERIFY;

	ivc_tx->aes.op_gcm.keyslot = gmac_ctx->aes_keyslot;
	ivc_tx->aes.op_gcm.key_length = gmac_ctx->keylen;
	ivc_tx->aes.op_gcm.aad_addr_hi = req->nbytes;
	ivc_tx->aes.op_gcm.aad_addr_lo = (u32)(aad_buf_addr & U32_MAX);

	if (gmac_req_data->request_type == GMAC_SIGN) {
		ivc_tx->aes.op_gcm.tag_addr_hi = gmac_ctx->authsize;
		ivc_tx->aes.op_gcm.tag_addr_lo = (u32)(tag_buf_addr & U32_MAX);
	}

	if (gmac_req_data->is_first)
		ivc_tx->aes.op_gcm.config |=
					(1 << TEGRA_VIRTUAL_SE_AES_GMAC_SV_CFG_FIRST_REQ_SHIFT);

	if (is_last == true) {
		ivc_tx->aes.op_gcm.config |= (1 << TEGRA_VIRTUAL_SE_AES_GMAC_SV_CFG_LAST_REQ_SHIFT);

		if (gmac_req_data->request_type == GMAC_VERIFY) {
			memcpy(ivc_tx->aes.op_gcm.iv, gmac_req_data->iv,
								TEGRA_VIRTUAL_SE_AES_GCM_IV_SIZE);
			memcpy(ivc_tx->aes.op_gcm.expected_tag, req->result, gmac_ctx->authsize);
		}
	}

	vse_thread_start = true;
	init_completion(&priv->alg_complete);
	mutex_lock(&se_dev->server_lock);
	/* Return error if engine is in suspended state */
	if (atomic_read(&se_dev->se_suspended)) {
		mutex_unlock(&se_dev->server_lock);
		dev_err(se_dev->dev, "%s: engine is in suspended state\n", __func__);
		err = -ENODEV;
		goto free_exit;
	}

	err = tegra_hv_vse_safety_send_ivc(se_dev, pivck, ivc_req_msg,
			sizeof(struct tegra_virtual_se_ivc_msg_t));
	if (err) {
		mutex_unlock(&se_dev->server_lock);
		dev_err(se_dev->dev, "%s: send_ivc failed %d\n", __func__, err);
		goto free_exit;
	}

	time_left = wait_for_completion_timeout(&priv->alg_complete,
			TEGRA_HV_VSE_TIMEOUT);
	mutex_unlock(&se_dev->server_lock);
	if (time_left == 0UL) {
		dev_err(se_dev->dev, "%s: completion timeout\n", __func__);
		err = -ETIMEDOUT;
		goto free_exit;
	}

	if (priv->rx_status != 0) {
		dev_err(se_dev->dev, "%s: SE server returned error %u\n", __func__,
									priv->rx_status);
		if (is_last && (gmac_req_data->request_type == GMAC_VERIFY)
				&& (priv->rx_status == 11U)) {
			gmac_req_data->result = 1;
		} else {
			err = status_to_errno(priv->rx_status);
		}
	} else {
		if (is_last) {
			if (gmac_req_data->request_type == GMAC_VERIFY) {
				gmac_req_data->result = 0;
			} else {
				/* copy tag to req for last GMAC_SIGN requests */
				memcpy(req->result, tag_buf, gmac_ctx->authsize);
			}
		}
	}

free_exit:
	if (ivc_req_msg)
		devm_kfree(se_dev->dev, ivc_req_msg);

	if (priv)
		devm_kfree(se_dev->dev, priv);

	if (tag_buf)
		dma_free_coherent(se_dev->dev, gmac_ctx->authsize, tag_buf, tag_buf_addr);

	if (aad_buf)
		dma_free_coherent(se_dev->dev, req->nbytes, aad_buf, aad_buf_addr);

exit:
	return err;
}

static int tegra_hv_vse_aes_gmac_sv_update(struct ahash_request *req)
{
	struct tegra_virtual_se_aes_gmac_context *gmac_ctx = NULL;
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_AES0];
	int ret = 0;

	/* Return error if engine is in suspended state */
	if (atomic_read(&se_dev->se_suspended)) {
		dev_err(se_dev->dev, "%s: engine is in suspended state\n", __func__);
		ret = -ENODEV;
		goto exit;
	}

	if (!req) {
		dev_err(se_dev->dev, "%s: request not valid\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	gmac_ctx = crypto_ahash_ctx(crypto_ahash_reqtfm(req));
	if (!gmac_ctx->req_context_initialized) {
		dev_err(se_dev->dev,
			"%s Request ctx not initialized\n", __func__);
		ret = -EPERM;
		goto exit;
	}

	mutex_lock(&se_dev->mtx);

	ret = tegra_hv_vse_aes_gmac_sv_op(req, false);
	if (ret)
		dev_err(se_dev->dev, "%s failed %d\n", __func__, ret);

	mutex_unlock(&se_dev->mtx);

exit:
	return ret;
}

static int tegra_hv_vse_aes_gmac_sv_finup(struct ahash_request *req)
{
	struct tegra_virtual_se_aes_gmac_context *gmac_ctx = NULL;
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_AES0];
	int ret = 0;

	/* Return error if engine is in suspended state */
	if (atomic_read(&se_dev->se_suspended)) {
		dev_err(se_dev->dev, "%s: engine is in suspended state\n", __func__);
		ret = -ENODEV;
		goto exit;
	}

	if (!req) {
		dev_err(se_dev->dev, "%s: request not valid\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	gmac_ctx = crypto_ahash_ctx(crypto_ahash_reqtfm(req));
	if (!gmac_ctx->req_context_initialized) {
		dev_err(se_dev->dev,
			"%s: Request ctx not initialized\n", __func__);
		ret = -EPERM;
		goto exit;
	}

	mutex_lock(&se_dev->mtx);

	ret = tegra_hv_vse_aes_gmac_sv_op(req, true);
	if (ret)
		dev_err(se_dev->dev, "%s failed %d\n", __func__, ret);

	mutex_unlock(&se_dev->mtx);

	tegra_hv_vse_aes_gmac_deinit(req);

exit:
	return ret;
}

static int tegra_hv_vse_aes_gmac_sv_final(struct ahash_request *req)
{
	struct tegra_virtual_se_dev *se_dev = g_virtual_se_dev[VIRTUAL_SE_AES0];

	dev_err(se_dev->dev, "%s: final not supported", __func__);
	return -EPERM;
}

#define HV_SAFETY_AES_CTX_SIZE sizeof(struct tegra_virtual_se_aes_context)

static struct rng_alg rng_alg = {
	.generate = tegra_hv_vse_safety_rng_drbg_get_random,
	.seed = tegra_hv_vse_safety_rng_drbg_reset,
	.seedsize = TEGRA_VIRTUAL_SE_RNG_SEED_SIZE,
	.base = {
		.cra_name = "rng_drbg",
		.cra_driver_name = "rng_drbg-aes-tegra",
		.cra_priority = 100,
		.cra_flags = CRYPTO_ALG_TYPE_RNG,
		.cra_ctxsize = sizeof(struct tegra_virtual_se_rng_context),
		.cra_module = THIS_MODULE,
		.cra_init = tegra_hv_vse_safety_rng_drbg_init,
		.cra_exit = tegra_hv_vse_safety_rng_drbg_exit,
	}
};

static struct aead_alg aead_algs[] = {
	{
		.setkey		= tegra_vse_aes_gcm_setkey,
		.setauthsize	= tegra_vse_aes_gcm_setauthsize,
		.encrypt	= tegra_vse_aes_gcm_encrypt,
		.decrypt	= tegra_vse_aes_gcm_decrypt,
		.init		= tegra_vse_aes_gcm_init,
		.exit		= tegra_vse_aes_gcm_exit,
		.ivsize		= TEGRA_VIRTUAL_SE_AES_GCM_IV_SIZE,
		.maxauthsize	= TEGRA_VIRTUAL_SE_AES_GCM_TAG_SIZE,
		.chunksize	= TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE,
		.base = {
			.cra_name	= "gcm-vse(aes)",
			.cra_driver_name = "gcm-aes-tegra-safety",
			.cra_priority	= 1000,
			.cra_blocksize	= TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE,
			.cra_ctxsize	= HV_SAFETY_AES_CTX_SIZE,
			.cra_module	= THIS_MODULE,
		}
	}
};

static struct skcipher_alg aes_algs[] = {
	{
		.base.cra_name		= "cbc-vse(aes)",
		.base.cra_driver_name	= "cbc-aes-tegra",
		.base.cra_priority	= 400,
		.base.cra_flags		= CRYPTO_ALG_TYPE_SKCIPHER |
					  CRYPTO_ALG_ASYNC,
		.base.cra_blocksize	= TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE,
		.base.cra_ctxsize	= HV_SAFETY_AES_CTX_SIZE,
		.base.cra_alignmask	= 0,
		.base.cra_module	= THIS_MODULE,
		.init			= tegra_hv_vse_safety_aes_cra_init,
		.exit			= tegra_hv_vse_safety_aes_cra_exit,
		.setkey			= tegra_hv_vse_safety_aes_setkey,
		.encrypt		= tegra_hv_vse_safety_aes_cbc_encrypt,
		.decrypt		= tegra_hv_vse_safety_aes_cbc_decrypt,
		.min_keysize		= TEGRA_VIRTUAL_SE_AES_MIN_KEY_SIZE,
		.max_keysize		= TEGRA_VIRTUAL_SE_AES_MAX_KEY_SIZE,
		.ivsize			= TEGRA_VIRTUAL_SE_AES_IV_SIZE,
	},
	{
		.base.cra_name		= "ecb-vse(aes)",
		.base.cra_driver_name	= "ecb-aes-tegra",
		.base.cra_priority	= 400,
		.base.cra_flags		= CRYPTO_ALG_TYPE_SKCIPHER |
					  CRYPTO_ALG_ASYNC,
		.base.cra_blocksize	= TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE,
		.base.cra_ctxsize	= HV_SAFETY_AES_CTX_SIZE,
		.base.cra_alignmask	= 0,
		.base.cra_module	= THIS_MODULE,
		.init			= tegra_hv_vse_safety_aes_cra_init,
		.exit			= tegra_hv_vse_safety_aes_cra_exit,
		.setkey			= tegra_hv_vse_safety_aes_setkey,
		.encrypt		= tegra_hv_vse_safety_aes_ecb_encrypt,
		.decrypt		= tegra_hv_vse_safety_aes_ecb_decrypt,
		.min_keysize		= TEGRA_VIRTUAL_SE_AES_MIN_KEY_SIZE,
		.max_keysize		= TEGRA_VIRTUAL_SE_AES_MAX_KEY_SIZE,
		.ivsize			= TEGRA_VIRTUAL_SE_AES_IV_SIZE,
	},
	{
		.base.cra_name		= "ctr-vse(aes)",
		.base.cra_driver_name	= "ctr-aes-tegra-safety",
		.base.cra_priority	= 400,
		.base.cra_flags		= CRYPTO_ALG_TYPE_SKCIPHER |
					  CRYPTO_ALG_ASYNC,
		.base.cra_blocksize	= TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE,
		.base.cra_ctxsize	= HV_SAFETY_AES_CTX_SIZE,
		.base.cra_alignmask	= 0,
		.base.cra_module	= THIS_MODULE,
		.init			= tegra_hv_vse_safety_aes_cra_init,
		.exit			= tegra_hv_vse_safety_aes_cra_exit,
		.setkey			= tegra_hv_vse_safety_aes_setkey,
		.encrypt		= tegra_hv_vse_safety_aes_ctr_encrypt,
		.decrypt		= tegra_hv_vse_safety_aes_ctr_decrypt,
		.min_keysize		= TEGRA_VIRTUAL_SE_AES_MIN_KEY_SIZE,
		.max_keysize		= TEGRA_VIRTUAL_SE_AES_MAX_KEY_SIZE,
		.ivsize			= TEGRA_VIRTUAL_SE_AES_IV_SIZE,
	},
};

static struct ahash_alg cmac_alg = {
	.init = tegra_hv_vse_safety_cmac_init,
	.update = tegra_hv_vse_safety_cmac_update,
	.final = tegra_hv_vse_safety_cmac_final,
	.finup = tegra_hv_vse_safety_cmac_finup,
	.digest = tegra_hv_vse_safety_cmac_digest,
	.setkey = tegra_hv_vse_safety_cmac_setkey,
	.halg.digestsize = TEGRA_VIRTUAL_SE_AES_CMAC_DIGEST_SIZE,
	.halg.statesize = TEGRA_VIRTUAL_SE_AES_CMAC_STATE_SIZE,
	.halg.base = {
		.cra_name = "cmac-vse(aes)",
		.cra_driver_name = "tegra-hv-vse-safety-cmac(aes)",
		.cra_priority = 400,
		.cra_flags = CRYPTO_ALG_TYPE_AHASH,
		.cra_blocksize = TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE,
		.cra_ctxsize = sizeof(struct tegra_virtual_se_aes_cmac_context),
		.cra_alignmask = 0,
		.cra_module = THIS_MODULE,
		.cra_init = tegra_hv_vse_safety_cmac_cra_init,
		.cra_exit = tegra_hv_vse_safety_cmac_cra_exit,
	}
};

static struct ahash_alg gmac_alg = {
	.init = tegra_hv_vse_aes_gmac_sv_init,
	.update = tegra_hv_vse_aes_gmac_sv_update,
	.finup = tegra_hv_vse_aes_gmac_sv_finup,
	.final = tegra_hv_vse_aes_gmac_sv_final,
	.setkey = tegra_hv_vse_aes_gmac_setkey,
	.halg.digestsize = TEGRA_VIRTUAL_SE_AES_GCM_TAG_SIZE,
	.halg.statesize = TEGRA_VIRTUAL_SE_AES_GCM_TAG_SIZE,
	.halg.base = {
		.cra_name = "gmac-vse(aes)",
		.cra_driver_name = "tegra-hv-vse-gmac(aes)",
		.cra_priority = 400,
		.cra_flags = CRYPTO_ALG_TYPE_AHASH,
		.cra_blocksize = TEGRA_VIRTUAL_SE_AES_BLOCK_SIZE,
		.cra_ctxsize = sizeof(struct tegra_virtual_se_aes_gmac_context),
		.cra_alignmask = 0,
		.cra_module = THIS_MODULE,
		.cra_init = tegra_hv_vse_safety_gmac_cra_init,
		.cra_exit = tegra_hv_vse_safety_gmac_cra_exit,
	}
};

static struct ahash_alg sha_algs[] = {
	{
		.init = tegra_hv_vse_safety_sha_init,
		.update = tegra_hv_vse_safety_sha_update,
		.final = tegra_hv_vse_safety_sha_final,
		.finup = tegra_hv_vse_safety_sha_finup,
		.digest = tegra_hv_vse_safety_sha_digest,
		.export = tegra_hv_vse_safety_sha_export,
		.import = tegra_hv_vse_safety_sha_import,
		.halg.digestsize = SHA1_DIGEST_SIZE,
		.halg.statesize = sizeof(struct tegra_virtual_se_req_context),
		.halg.base = {
			.cra_name = "sha1",
			.cra_driver_name = "tegra-hv-vse-sha1",
			.cra_priority = 300,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA1_BLOCK_SIZE,
			.cra_ctxsize =
				sizeof(struct tegra_virtual_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_hv_vse_safety_sha_cra_init,
			.cra_exit = tegra_hv_vse_safety_sha_cra_exit,
		}
	}, {
		.init = tegra_hv_vse_safety_sha_init,
		.update = tegra_hv_vse_safety_sha_update,
		.final = tegra_hv_vse_safety_sha_final,
		.finup = tegra_hv_vse_safety_sha_finup,
		.digest = tegra_hv_vse_safety_sha_digest,
		.export = tegra_hv_vse_safety_sha_export,
		.import = tegra_hv_vse_safety_sha_import,
		.halg.digestsize = SHA224_DIGEST_SIZE,
		.halg.statesize = sizeof(struct tegra_virtual_se_req_context),
		.halg.base = {
			.cra_name = "sha224",
			.cra_driver_name = "tegra-hv-vse-sha224",
			.cra_priority = 300,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA224_BLOCK_SIZE,
			.cra_ctxsize =
				sizeof(struct tegra_virtual_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_hv_vse_safety_sha_cra_init,
			.cra_exit = tegra_hv_vse_safety_sha_cra_exit,
		}
	}, {
		.init = tegra_hv_vse_safety_sha_init,
		.update = tegra_hv_vse_safety_sha_update,
		.final = tegra_hv_vse_safety_sha_final,
		.finup = tegra_hv_vse_safety_sha_finup,
		.digest = tegra_hv_vse_safety_sha_digest,
		.export = tegra_hv_vse_safety_sha_export,
		.import = tegra_hv_vse_safety_sha_import,
		.halg.digestsize = SHA256_DIGEST_SIZE,
		.halg.statesize = sizeof(struct tegra_virtual_se_req_context),
		.halg.base = {
			.cra_name = "sha256",
			.cra_driver_name = "tegra-hv-vse-safety-sha256",
			.cra_priority = 300,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA256_BLOCK_SIZE,
			.cra_ctxsize =
				sizeof(struct tegra_virtual_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_hv_vse_safety_sha_cra_init,
			.cra_exit = tegra_hv_vse_safety_sha_cra_exit,
		}
	}, {
		.init = tegra_hv_vse_safety_sha_init,
		.update = tegra_hv_vse_safety_sha_update,
		.final = tegra_hv_vse_safety_sha_final,
		.finup = tegra_hv_vse_safety_sha_finup,
		.digest = tegra_hv_vse_safety_sha_digest,
		.export = tegra_hv_vse_safety_sha_export,
		.import = tegra_hv_vse_safety_sha_import,
		.halg.digestsize = SHA384_DIGEST_SIZE,
		.halg.statesize = sizeof(struct tegra_virtual_se_req_context),
		.halg.base = {
			.cra_name = "sha384",
			.cra_driver_name = "tegra-hv-vse-safety-sha384",
			.cra_priority = 300,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA384_BLOCK_SIZE,
			.cra_ctxsize =
				sizeof(struct tegra_virtual_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_hv_vse_safety_sha_cra_init,
			.cra_exit = tegra_hv_vse_safety_sha_cra_exit,
		}
	}, {
		.init = tegra_hv_vse_safety_sha_init,
		.update = tegra_hv_vse_safety_sha_update,
		.final = tegra_hv_vse_safety_sha_final,
		.finup = tegra_hv_vse_safety_sha_finup,
		.digest = tegra_hv_vse_safety_sha_digest,
		.export = tegra_hv_vse_safety_sha_export,
		.import = tegra_hv_vse_safety_sha_import,
		.halg.digestsize = SHA512_DIGEST_SIZE,
		.halg.statesize = sizeof(struct tegra_virtual_se_req_context),
		.halg.base = {
			.cra_name = "sha512",
			.cra_driver_name = "tegra-hv-vse-safety-sha512",
			.cra_priority = 300,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA512_BLOCK_SIZE,
			.cra_ctxsize =
				sizeof(struct tegra_virtual_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_hv_vse_safety_sha_cra_init,
			.cra_exit = tegra_hv_vse_safety_sha_cra_exit,
		}
	}, {
		.init = tegra_hv_vse_safety_sha_init,
		.update = tegra_hv_vse_safety_sha_update,
		.final = tegra_hv_vse_safety_sha_final,
		.finup = tegra_hv_vse_safety_sha_finup,
		.digest = tegra_hv_vse_safety_sha_digest,
		.export = tegra_hv_vse_safety_sha_export,
		.import = tegra_hv_vse_safety_sha_import,
		.halg.digestsize = SHA3_256_DIGEST_SIZE,
		.halg.statesize = sizeof(struct tegra_virtual_se_req_context),
		.halg.base = {
			.cra_name = "sha3-256",
			.cra_driver_name = "tegra-hv-vse-safety-sha3-256",
			.cra_priority = 300,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA3_256_BLOCK_SIZE,
			.cra_ctxsize =
				sizeof(struct tegra_virtual_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_hv_vse_safety_sha_cra_init,
			.cra_exit = tegra_hv_vse_safety_sha_cra_exit,
		}
	}, {
		.init = tegra_hv_vse_safety_sha_init,
		.update = tegra_hv_vse_safety_sha_update,
		.final = tegra_hv_vse_safety_sha_final,
		.finup = tegra_hv_vse_safety_sha_finup,
		.digest = tegra_hv_vse_safety_sha_digest,
		.export = tegra_hv_vse_safety_sha_export,
		.import = tegra_hv_vse_safety_sha_import,
		.halg.digestsize = SHA3_384_DIGEST_SIZE,
		.halg.statesize = sizeof(struct tegra_virtual_se_req_context),
		.halg.base = {
			.cra_name = "sha3-384",
			.cra_driver_name = "tegra-hv-vse-safety-sha3-384",
			.cra_priority = 300,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA3_384_BLOCK_SIZE,
			.cra_ctxsize =
				sizeof(struct tegra_virtual_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_hv_vse_safety_sha_cra_init,
			.cra_exit = tegra_hv_vse_safety_sha_cra_exit,
		}
	}, {
		.init = tegra_hv_vse_safety_sha_init,
		.update = tegra_hv_vse_safety_sha_update,
		.final = tegra_hv_vse_safety_sha_final,
		.finup = tegra_hv_vse_safety_sha_finup,
		.digest = tegra_hv_vse_safety_sha_digest,
		.export = tegra_hv_vse_safety_sha_export,
		.import = tegra_hv_vse_safety_sha_import,
		.halg.digestsize = SHA3_512_DIGEST_SIZE,
		.halg.statesize = sizeof(struct tegra_virtual_se_req_context),
		.halg.base = {
			.cra_name = "sha3-512",
			.cra_driver_name = "tegra-hv-vse-safety-sha3-512",
			.cra_priority = 300,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA3_512_BLOCK_SIZE,
			.cra_ctxsize =
				sizeof(struct tegra_virtual_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_hv_vse_safety_sha_cra_init,
			.cra_exit = tegra_hv_vse_safety_sha_cra_exit,
		}
	}, {
		.init = tegra_hv_vse_safety_sha_init,
		.update = tegra_hv_vse_safety_sha_update,
		.final = tegra_hv_vse_safety_sha_final,
		.finup = tegra_hv_vse_safety_sha_finup,
		.digest = tegra_hv_vse_safety_sha_digest,
		.export = tegra_hv_vse_safety_sha_export,
		.import = tegra_hv_vse_safety_sha_import,
		.halg.digestsize = SHA3_512_DIGEST_SIZE,
		.halg.statesize = sizeof(struct tegra_virtual_se_req_context),
		.halg.base = {
			.cra_name = "shake128",
			.cra_driver_name = "tegra-hv-vse-safety-shake128",
			.cra_priority = 300,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA3_512_BLOCK_SIZE,
			.cra_ctxsize =
				sizeof(struct tegra_virtual_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_hv_vse_safety_sha_cra_init,
			.cra_exit = tegra_hv_vse_safety_sha_cra_exit,
		}
	}, {
		.init = tegra_hv_vse_safety_sha_init,
		.update = tegra_hv_vse_safety_sha_update,
		.final = tegra_hv_vse_safety_sha_final,
		.finup = tegra_hv_vse_safety_sha_finup,
		.digest = tegra_hv_vse_safety_sha_digest,
		.export = tegra_hv_vse_safety_sha_export,
		.import = tegra_hv_vse_safety_sha_import,
		.halg.digestsize = SHA3_512_DIGEST_SIZE,
		.halg.statesize = sizeof(struct tegra_virtual_se_req_context),
		.halg.base = {
			.cra_name = "shake256",
			.cra_driver_name = "tegra-hv-vse-safety-shake256",
			.cra_priority = 300,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA3_512_BLOCK_SIZE,
			.cra_ctxsize =
				sizeof(struct tegra_virtual_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_hv_vse_safety_sha_cra_init,
			.cra_exit = tegra_hv_vse_safety_sha_cra_exit,
		}
	},
};

static const struct tegra_vse_soc_info t194_vse_sinfo = {
	.cmac_hw_padding_supported = false,
	.gcm_decrypt_supported = false,
};

static const struct tegra_vse_soc_info t234_vse_sinfo = {
	.cmac_hw_padding_supported = true,
	.gcm_decrypt_supported = true,
};

static const struct of_device_id tegra_hv_vse_safety_of_match[] = {
	{ .compatible = "nvidia,tegra194-hv-vse-safety", .data = &t194_vse_sinfo, },
	{ .compatible = "nvidia,tegra234-hv-vse-safety", .data = &t234_vse_sinfo, },
	{},
};
MODULE_DEVICE_TABLE(of, tegra_hv_vse_safety_of_match);

static irqreturn_t tegra_vse_irq_handler(int irq, void *data)
{
	if (tegra_hv_ivc_can_read(g_ivck)) {
		complete(&tegra_vse_complete);
	}

	return IRQ_HANDLED;
}

static int tegra_vse_kthread(void *unused)
{
	struct tegra_virtual_se_dev *se_dev = NULL;
	struct tegra_hv_ivc_cookie *pivck = g_ivck;
	struct tegra_virtual_se_ivc_msg_t *ivc_msg;
	struct tegra_virtual_se_aes_req_context *req_ctx;
	int err = 0;
	struct tegra_vse_tag *p_dat;
	struct tegra_vse_priv_data *priv;
	int timeout;
	struct tegra_virtual_se_ivc_resp_msg_t *ivc_rx;
	int ret;
	int read_size = 0;

	ivc_msg =
		kmalloc(sizeof(struct tegra_virtual_se_ivc_msg_t), GFP_KERNEL);
	if (!ivc_msg)
		return -ENOMEM;

	while (!kthread_should_stop()) {
		err = 0;
		ret = wait_for_completion_interruptible(&tegra_vse_complete);
		if (ret < 0) {
			pr_err("%s completion err\n", __func__);
			reinit_completion(&tegra_vse_complete);
			continue;
		}

		if (!vse_thread_start) {
			reinit_completion(&tegra_vse_complete);
			continue;
		}
		timeout = TEGRA_VIRTUAL_SE_TIMEOUT_1S;
		while (tegra_hv_ivc_channel_notified(pivck) != 0) {
			if (!timeout) {
				reinit_completion(
					&tegra_vse_complete);
				pr_err("%s:%d ivc channel_notifier timeout\n",
					__func__, __LINE__);
				err = -EAGAIN;
				break;
			}
			udelay(1);
			timeout--;
		}

		if (err == -EAGAIN) {
			err = 0;
			continue;
		}

		while (tegra_hv_ivc_can_read(pivck)) {
			read_size = tegra_hv_ivc_read(pivck,
					ivc_msg,
					sizeof(struct tegra_virtual_se_ivc_msg_t));
			if (read_size > 0 &&
					read_size < sizeof(struct tegra_virtual_se_ivc_msg_t)) {
				pr_err("Wrong read msg len %d\n", read_size);
				continue;
			}
			p_dat =
				(struct tegra_vse_tag *)ivc_msg->ivc_hdr.tag;
			priv = (struct tegra_vse_priv_data *)p_dat->priv_data;
			if (!priv) {
				pr_err("%s no call back info\n", __func__);
				continue;
			}
			se_dev = priv->se_dev;

			switch (priv->cmd) {
			case VIRTUAL_SE_AES_CRYPTO:
				priv->rx_status = ivc_msg->rx[0].status;
				req_ctx = skcipher_request_ctx(priv->req);
				if ((!priv->rx_status) && (req_ctx->encrypt == true) &&
						((req_ctx->op_mode == AES_CTR) ||
						(req_ctx->op_mode == AES_CBC))) {
					memcpy(priv->iv, ivc_msg->rx[0].iv,
							TEGRA_VIRTUAL_SE_AES_IV_SIZE);
				}
				complete(&priv->alg_complete);
				break;
			case VIRTUAL_SE_KEY_SLOT:
				ivc_rx = &ivc_msg->rx[0];
				priv->slot_num = ivc_rx->keyslot;
				complete(&priv->alg_complete);
				break;
			case VIRTUAL_SE_PROCESS:
				ivc_rx = &ivc_msg->rx[0];
				priv->rx_status = ivc_rx->status;
				complete(&priv->alg_complete);
				break;
			case VIRTUAL_CMAC_PROCESS:
				ivc_rx = &ivc_msg->rx[0];
				priv->rx_status = ivc_rx->status;
				priv->cmac.status = ivc_rx->status;
				if (!ivc_rx->status) {
					memcpy(priv->cmac.data, ivc_rx->cmac_result, \
							TEGRA_VIRTUAL_SE_AES_CMAC_DIGEST_SIZE);
				}
				complete(&priv->alg_complete);
				break;
			case VIRTUAL_SE_AES_GCM_ENC_PROCESS:
				ivc_rx = &ivc_msg->rx[0];
				priv->rx_status = ivc_rx->status;
				if (!ivc_rx->status)
					memcpy(priv->iv, ivc_rx->iv,
							TEGRA_VIRTUAL_SE_AES_GCM_IV_SIZE);

				complete(&priv->alg_complete);
				break;
			default:
				dev_err(se_dev->dev, "Unknown command\n");
			}
		}
	}

	kfree(ivc_msg);
	return 0;
}

static int tegra_hv_vse_safety_probe(struct platform_device *pdev)
{
	struct tegra_virtual_se_dev *se_dev = NULL;
	int err = 0;
	int i;
	unsigned int ivc_id;
	unsigned int mempool_id;
	unsigned int engine_id;
	const struct of_device_id *match;
	struct tegra_vse_soc_info *pdata = NULL;

	se_dev = devm_kzalloc(&pdev->dev,
				sizeof(struct tegra_virtual_se_dev),
				GFP_KERNEL);
	if (!se_dev)
		return -ENOMEM;

	se_dev->dev = &pdev->dev;
	err = of_property_read_u32(pdev->dev.of_node, "se-engine-id",
				&engine_id);
	if (err) {
		dev_err(&pdev->dev, "se-engine-id property not present\n");
		err = -ENODEV;
		goto exit;
	}

	if (pdev->dev.of_node) {
		match = of_match_device(of_match_ptr(tegra_hv_vse_safety_of_match),
					&pdev->dev);
		if (!match) {
			dev_err(&pdev->dev, "Error: No device match found\n");
			return -ENODEV;
		}
		pdata = (struct tegra_vse_soc_info *)match->data;
	} else {
		pdata =
		(struct tegra_vse_soc_info *)pdev->id_entry->driver_data;
	}

	se_dev->chipdata = pdata;

	if ((se_dev->chipdata->gcm_decrypt_supported) &&
			(!g_ivmk) &&
			((engine_id == VIRTUAL_SE_AES0) || (engine_id == VIRTUAL_SE_AES1))) {
		err = of_property_read_u32(pdev->dev.of_node, "mempool_id", &mempool_id);
		if (err) {
			dev_err(&pdev->dev, "mempool_id property not present\n");
			err = -ENODEV;
			goto exit;
		}

		dev_info(se_dev->dev, "Virtual SE IVM channel number: %d", mempool_id);
		g_ivmk = tegra_hv_mempool_reserve(mempool_id);
		if (IS_ERR_OR_NULL(g_ivmk)) {
			dev_err(&pdev->dev, "Failed to reserve IVM channel %d\n", mempool_id);
			err = -ENODEV;
			goto exit;
		}

		mempool_buf = devm_memremap(&pdev->dev, g_ivmk->ipa, g_ivmk->size, MEMREMAP_WB);
		if (IS_ERR_OR_NULL(mempool_buf)) {
			dev_err(&pdev->dev, "Failed to map mempool area %d\n", mempool_id);
			err = -ENOMEM;
			goto exit;
		}
		atomic_set(&se_dev->mempoolbuf_in_use, false);
	}

	if (!g_ivck) {
		err = of_property_read_u32(pdev->dev.of_node, "ivc", &ivc_id);
		if (err) {
			dev_err(&pdev->dev, "ivc property not present\n");
			err = -ENODEV;
			goto exit;
		}
		dev_info(se_dev->dev, "Virtual SE channel number: %d", ivc_id);

		g_ivck = tegra_hv_ivc_reserve(NULL, ivc_id, NULL);
		if (IS_ERR_OR_NULL(g_ivck)) {
			dev_err(&pdev->dev, "Failed reserve channel number\n");
			err = -ENODEV;
			goto exit;
		}
		tegra_hv_ivc_channel_reset(g_ivck);
		init_completion(&tegra_vse_complete);

		tegra_vse_task = kthread_run(tegra_vse_kthread,
				NULL, "tegra_vse_kthread");
		if (IS_ERR(tegra_vse_task)) {
			dev_err(se_dev->dev,
				"Couldn't create kthread for vse\n");
			err = PTR_ERR(tegra_vse_task);
			goto exit;
		}

		if (request_irq(g_ivck->irq,
			tegra_vse_irq_handler, 0, "vse", se_dev)) {
			dev_err(se_dev->dev,
				"Failed to request irq %d\n", g_ivck->irq);
			err = -EINVAL;
			goto exit;
		}
	}

	g_virtual_se_dev[engine_id] = se_dev;
	mutex_init(&se_dev->mtx);

	if (engine_id == VIRTUAL_SE_AES0) {
		err = crypto_register_ahash(&cmac_alg);
		if (err) {
			dev_err(&pdev->dev,
				"cmac alg register failed. Err %d\n", err);
			goto exit;
		}

		err = crypto_register_ahash(&gmac_alg);
		if (err) {
			dev_err(&pdev->dev,
				"gmac alg register failed. Err %d\n", err);
			goto exit;
		}

		err = crypto_register_rng(&rng_alg);
		if (err) {
			dev_err(&pdev->dev,
				"rng alg register failed. Err %d\n", err);
			goto exit;
		}

	}

	if (engine_id == VIRTUAL_SE_AES1) {

		err = crypto_register_skciphers(aes_algs, ARRAY_SIZE(aes_algs));
		if (err) {
			dev_err(&pdev->dev, "aes alg register failed: %d\n",
				err);
			goto exit;
		}


		if (se_dev->chipdata->gcm_decrypt_supported) {
			err = crypto_register_aeads(aead_algs, ARRAY_SIZE(aead_algs));
			if (err) {
				dev_err(&pdev->dev, "aead alg register failed: %d\n",
					err);
				goto exit;
			}
		}
	}

	if (engine_id == VIRTUAL_SE_SHA) {
		for (i = 0; i < ARRAY_SIZE(sha_algs); i++) {
			err = crypto_register_ahash(&sha_algs[i]);
			if (err) {
				dev_err(&pdev->dev,
					"sha alg register failed idx[%d]\n", i);
				goto exit;
			}
		}
	}

	se_dev->engine_id = engine_id;

	/* Set Engine suspended state to false*/
	atomic_set(&se_dev->se_suspended, 0);
	platform_set_drvdata(pdev, se_dev);
	mutex_init(&se_dev->server_lock);

	return 0;

exit:
	return err;
}

static void tegra_hv_vse_safety_shutdown(struct platform_device *pdev)
{
	struct tegra_virtual_se_dev *se_dev = platform_get_drvdata(pdev);

	/* Set engine to suspend state */
	atomic_set(&se_dev->se_suspended, 1);

	/* Wait for  SE server to be free*/
	while (mutex_is_locked(&se_dev->server_lock))
		usleep_range(8, 10);
}

static int tegra_hv_vse_safety_remove(struct platform_device *pdev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sha_algs); i++)
		crypto_unregister_ahash(&sha_algs[i]);

	return 0;
}

#if defined(CONFIG_PM)
static int tegra_hv_vse_safety_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	/* Keep engine in suspended state */
	tegra_hv_vse_safety_shutdown(pdev);
	return 0;
}

static int tegra_hv_vse_safety_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_virtual_se_dev *se_dev = platform_get_drvdata(pdev);

	/* Set engine to suspend state to 1 to make it as false */
	atomic_set(&se_dev->se_suspended, 0);

	return 0;
}

static const struct dev_pm_ops tegra_hv_pm_ops = {
	.suspend = tegra_hv_vse_safety_suspend,
	.resume = tegra_hv_vse_safety_resume,
};
#endif /* CONFIG_PM */

static struct platform_driver tegra_hv_vse_safety_driver = {
	.probe = tegra_hv_vse_safety_probe,
	.remove = tegra_hv_vse_safety_remove,
	.shutdown = tegra_hv_vse_safety_shutdown,
	.driver = {
		.name = "tegra_hv_vse_safety",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tegra_hv_vse_safety_of_match),
#if defined(CONFIG_PM)
		.pm = &tegra_hv_pm_ops,
#endif
	},
};

static int __init tegra_hv_vse_safety_module_init(void)
{
	return platform_driver_register(&tegra_hv_vse_safety_driver);
}

static void __exit tegra_hv_vse_safety_module_exit(void)
{
	platform_driver_unregister(&tegra_hv_vse_safety_driver);
}

module_init(tegra_hv_vse_safety_module_init);
module_exit(tegra_hv_vse_safety_module_exit);

MODULE_AUTHOR("Mallikarjun Kasoju <mkasoju@nvidia.com>");
MODULE_DESCRIPTION("Virtual Security Engine driver over Tegra Hypervisor IVC channel");
MODULE_LICENSE("GPL");
