/*
 * Cryptographic API.
 * drivers/crypto/tegra-se-nvhost.c
 *
 * Support for Tegra Security Engine hardware crypto algorithms.
 *
 * Copyright (c) 2015-2023, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/nvhost.h>
#include <crypto/scatterwalk.h>
#include <crypto/algapi.h>
#include <crypto/aes.h>
#include <crypto/akcipher.h>
#include <crypto/skcipher.h>
#include <crypto/internal/rng.h>
#include <crypto/internal/hash.h>
#include <crypto/internal/aead.h>
#include <crypto/internal/akcipher.h>
#include <crypto/internal/skcipher.h>
#include <crypto/sha.h>
#include <crypto/sha3.h>
#include <crypto/internal/kpp.h>
#include <crypto/kpp.h>
#include <crypto/dh.h>
#include <linux/version.h>
#include <linux/pm_qos.h>
#include <linux/jiffies.h>
#include <linux/platform/tegra/emc_bwmgr.h>
#include <dt-bindings/interconnect/tegra_icc_id.h>

#include "tegra-se-nvhost.h"
#include "t194/hardware_t194.h"
#include "nvhost_job.h"
#include "nvhost_channel.h"
#include "nvhost_acm.h"

#define DRIVER_NAME	"tegra-se-nvhost"
#define NV_SE1_CLASS_ID		0x3A
#define NV_SE2_CLASS_ID		0x3B
#define NV_SE3_CLASS_ID		0x3C
#define NV_SE4_CLASS_ID		0x3D
#define NUM_SE_ALGO	6
#define MIN_DH_SZ_BITS	1536
#define GCM_IV_SIZE	12

#define __nvhost_opcode_nonincr(x, y)	nvhost_opcode_nonincr((x) / 4, (y))
#define __nvhost_opcode_incr(x, y)	nvhost_opcode_incr((x) / 4, (y))
#define __nvhost_opcode_nonincr_w(x)	nvhost_opcode_nonincr_w((x) / 4)
#define __nvhost_opcode_incr_w(x)	nvhost_opcode_incr_w((x) / 4)

/* Security Engine operation modes */
enum tegra_se_aes_op_mode {
	SE_AES_OP_MODE_CBC,	/* Cipher Block Chaining (CBC) mode */
	SE_AES_OP_MODE_ECB,	/* Electronic Codebook (ECB) mode */
	SE_AES_OP_MODE_CTR,	/* Counter (CTR) mode */
	SE_AES_OP_MODE_OFB,	/* Output feedback (CFB) mode */
	SE_AES_OP_MODE_CMAC,	/* Cipher-based MAC (CMAC) mode */
	SE_AES_OP_MODE_RNG_DRBG,	/* Deterministic Random Bit Generator */
	SE_AES_OP_MODE_SHA1,	/* Secure Hash Algorithm-1 (SHA1) mode */
	SE_AES_OP_MODE_SHA224,	/* Secure Hash Algorithm-224  (SHA224) mode */
	SE_AES_OP_MODE_SHA256,	/* Secure Hash Algorithm-256  (SHA256) mode */
	SE_AES_OP_MODE_SHA384,	/* Secure Hash Algorithm-384  (SHA384) mode */
	SE_AES_OP_MODE_SHA512,	/* Secure Hash Algorithm-512  (SHA512) mode */
	SE_AES_OP_MODE_SHA3_224,/* Secure Hash Algorithm3-224 (SHA3-224) mode */
	SE_AES_OP_MODE_SHA3_256,/* Secure Hash Algorithm3-256 (SHA3-256) mode */
	SE_AES_OP_MODE_SHA3_384,/* Secure Hash Algorithm3-384 (SHA3-384) mode */
	SE_AES_OP_MODE_SHA3_512,/* Secure Hash Algorithm3-512 (SHA3-512) mode */
	SE_AES_OP_MODE_SHAKE128,/* Secure Hash Algorithm3 (SHAKE128) mode */
	SE_AES_OP_MODE_SHAKE256,/* Secure Hash Algorithm3 (SHAKE256) mode */
	SE_AES_OP_MODE_HMAC_SHA224,	/* Hash based MAC (HMAC) - 224 */
	SE_AES_OP_MODE_HMAC_SHA256,	/* Hash based MAC (HMAC) - 256 */
	SE_AES_OP_MODE_HMAC_SHA384,	/* Hash based MAC (HMAC) - 384 */
	SE_AES_OP_MODE_HMAC_SHA512,	/* Hash based MAC (HMAC) - 512 */
	SE_AES_OP_MODE_XTS,		/* XTS mode */
	SE_AES_OP_MODE_INS,		/* key insertion */
	SE_AES_OP_MODE_CBC_MAC,		/* CBC MAC mode */
	SE_AES_OP_MODE_GCM		/* GCM mode */
};

enum tegra_se_aes_gcm_mode {
	SE_AES_GMAC,
	SE_AES_GCM_ENC,
	SE_AES_GCM_DEC,
	SE_AES_GCM_FINAL
};

/* Security Engine key table type */
enum tegra_se_key_table_type {
	SE_KEY_TABLE_TYPE_KEY,	/* Key */
	SE_KEY_TABLE_TYPE_KEY_IN_MEM,	/* Key in Memory */
	SE_KEY_TABLE_TYPE_ORGIV,	/* Original IV */
	SE_KEY_TABLE_TYPE_UPDTDIV,	/* Updated IV */
	SE_KEY_TABLE_TYPE_XTS_KEY1,	/* XTS Key1 */
	SE_KEY_TABLE_TYPE_XTS_KEY2,	/* XTS Key2 */
	SE_KEY_TABLE_TYPE_XTS_KEY1_IN_MEM,	/* XTS Key1 in Memory */
	SE_KEY_TABLE_TYPE_XTS_KEY2_IN_MEM,	/* XTS Key2 in Memory */
	SE_KEY_TABLE_TYPE_CMAC,
	SE_KEY_TABLE_TYPE_HMAC,
	SE_KEY_TABLE_TYPE_GCM			/* GCM */
};

/* Key access control type */
enum tegra_se_kac_type {
	SE_KAC_T18X,
	SE_KAC_T23X
};

struct tegra_se_chipdata {
	unsigned long aes_freq;
	unsigned int cpu_freq_mhz;
	enum tegra_se_kac_type kac_type;
};

/* Security Engine Linked List */
struct tegra_se_ll {
	dma_addr_t addr; /* DMA buffer address */
	u32 data_len; /* Data length in DMA buffer */
};

enum tegra_se_algo {
	SE_DRBG,
	SE_AES,
	SE_CMAC,
	SE_RSA,
	SE_SHA,
	SE_AEAD,
};

enum tegra_se_callback {
	NONE,
	AES_CB,
	SHA_CB,
};

struct tegra_se_dev {
	struct platform_device *pdev;
	struct device *dev;
	void __iomem *io_regs;	/* se device memory/io */
	void __iomem *pmc_io_reg;	/* pmc device memory/io */
	struct mutex lock;	/* Protect request queue */
	/* Mutex lock (mtx) is to protect Hardware, as it can be used
	 * in parallel by different threads. For example, set_key
	 * request can come from a different thread and access HW
	 */
	struct mutex mtx;
	struct clk *pclk;	/* Security Engine clock */
	struct clk *enclk;	/* Security Engine clock */
	struct crypto_queue queue; /* Security Engine crypto queue */
	struct tegra_se_slot *slot_list;	/* pointer to key slots */
	struct tegra_se_rsa_slot *rsa_slot_list; /* rsa key slot pointer */
	struct tegra_se_cmdbuf *cmdbuf_addr_list;
	unsigned int cmdbuf_list_entry;
	struct tegra_se_chipdata *chipdata; /* chip specific data */
	u32 *src_ll_buf;	/* pointer to source linked list buffer */
	dma_addr_t src_ll_buf_adr; /* Source linked list buffer dma address */
	u32 src_ll_size;	/* Size of source linked list buffer */
	u32 *dst_ll_buf;	/* pointer to destination linked list buffer */
	dma_addr_t dst_ll_buf_adr; /* Destination linked list dma address */
	u32 dst_ll_size;	/* Size of destination linked list buffer */
	struct tegra_se_ll *src_ll;
	struct tegra_se_ll *dst_ll;
	struct tegra_se_ll *aes_src_ll;
	struct tegra_se_ll *aes_dst_ll;
	u32 *dh_buf1, *dh_buf2;
	struct skcipher_request *reqs[SE_MAX_TASKS_PER_SUBMIT];
	struct ahash_request *sha_req;
	unsigned int req_cnt;
	u32 syncpt_id;
	u32 opcode_addr;
	bool work_q_busy;	/* Work queue busy status */
	struct nvhost_channel *channel;
	struct work_struct se_work;
	struct workqueue_struct *se_work_q;
	struct scatterlist sg;
	bool dynamic_mem;
	u32 *total_aes_buf;
	dma_addr_t total_aes_buf_addr;
	void *aes_buf;
	dma_addr_t aes_buf_addr;
	void *aes_bufs[SE_MAX_AESBUF_ALLOC];
	dma_addr_t aes_buf_addrs[SE_MAX_AESBUF_ALLOC];
	atomic_t aes_buf_stat[SE_MAX_AESBUF_ALLOC];
	dma_addr_t aes_addr;
	dma_addr_t aes_cur_addr;
	unsigned int cmdbuf_cnt;
	unsigned int src_bytes_mapped;
	unsigned int dst_bytes_mapped;
	unsigned int gather_buf_sz;
	unsigned int aesbuf_entry;
	u32 *aes_cmdbuf_cpuvaddr;
	dma_addr_t aes_cmdbuf_iova;
	struct pm_qos_request boost_cpufreq_req;
	/* Lock to protect cpufreq boost status */
	struct mutex boost_cpufreq_lock;
	struct delayed_work restore_cpufreq_work;
	unsigned long cpufreq_last_boosted;
	bool cpufreq_boosted;
	bool ioc;
	bool sha_last;
	bool sha_src_mapped;
	bool sha_dst_mapped;
};

static struct tegra_se_dev *se_devices[NUM_SE_ALGO];

/* Security Engine request context */
struct tegra_se_req_context {
	enum tegra_se_aes_op_mode op_mode; /* Security Engine operation mode */
	bool encrypt;	/* Operation type */
	u32 config;
	u32 crypto_config;
	bool init;	/* For GCM */
	u8 *hash_result; /* Hash result buffer */
	struct tegra_se_dev *se_dev;
};

struct tegra_se_priv_data {
	struct skcipher_request *reqs[SE_MAX_TASKS_PER_SUBMIT];
	struct ahash_request *sha_req;
	struct tegra_se_dev *se_dev;
	unsigned int req_cnt;
	unsigned int src_bytes_mapped;
	unsigned int dst_bytes_mapped;
	unsigned int gather_buf_sz;
	struct scatterlist sg;
	void *buf;
	bool dynmem;
	bool sha_last;
	bool sha_src_mapped;
	bool sha_dst_mapped;
	dma_addr_t buf_addr;
	dma_addr_t iova;
	unsigned int cmdbuf_node;
	unsigned int aesbuf_entry;
};

/* Security Engine AES context */
struct tegra_se_aes_context {
	struct tegra_se_dev *se_dev;	/* Security Engine device */
	struct skcipher_request *req;
	struct tegra_se_slot *slot;	/* Security Engine key slot */
	struct tegra_se_slot *slot2;    /* Security Engine key slot 2 */
	u32 keylen;	/* key length in bits */
	u32 op_mode;	/* AES operation mode */
	bool is_key_in_mem; /* Whether key is in memory */
	u8 key[64]; /* To store key if is_key_in_mem set */
};

/* Security Engine random number generator context */
struct tegra_se_rng_context {
	struct tegra_se_dev *se_dev;	/* Security Engine device */
	struct skcipher_request *req;
	struct tegra_se_slot *slot;	/* Security Engine key slot */
	u32 *dt_buf;	/* Destination buffer pointer */
	dma_addr_t dt_buf_adr;	/* Destination buffer dma address */
	u32 *rng_buf;	/* RNG buffer pointer */
	dma_addr_t rng_buf_adr;	/* RNG buffer dma address */
};

/* Security Engine SHA context */
struct tegra_se_sha_context {
	struct tegra_se_dev	*se_dev;	/* Security Engine device */
	u32 op_mode;	/* SHA operation mode */
	bool is_first; /* Represents first block */
	u8 *sha_buf[2];	/* Buffer to store residual data */
	dma_addr_t sha_buf_addr[2];	/* DMA address to residual data */
	u32 total_count; /* Total bytes in all the requests */
	u32 residual_bytes; /* Residual byte count */
	u32 blk_size; /* SHA block size */
	bool is_final; /* To know it is final/finup request */

	/* HMAC Support*/
	struct tegra_se_slot *slot;	/* Security Engine key slot */
	u32 keylen;	/* key length in bits */
};

struct tegra_se_sha_zero_length_vector {
	unsigned int size;
	char *digest;
};

/* Security Engine AES CMAC context */
struct tegra_se_aes_cmac_context {
	struct tegra_se_dev *se_dev;	/* Security Engine device */
	struct tegra_se_slot *slot;	/* Security Engine key slot */
	u32 keylen;	/* key length in bits */
	u8 K1[TEGRA_SE_KEY_128_SIZE];	/* Key1 */
	u8 K2[TEGRA_SE_KEY_128_SIZE];	/* Key2 */
	u8	*buf;		 /* Buffer pointer to store update request*/
	dma_addr_t buf_dma_addr; /* DMA address of buffer */
	u32 nbytes;		 /* Total bytes in input buffer */
};

struct tegra_se_dh_context {
	struct tegra_se_dev *se_dev;	/* Security Engine device */
	struct tegra_se_rsa_slot *slot;	/* Security Engine rsa key slot */
	void *key;
	void *p;
	void *g;
	unsigned int key_size;
	unsigned int p_size;
	unsigned int g_size;
};

/* Security Engine AES CCM context */
struct tegra_se_aes_ccm_ctx {
	struct tegra_se_dev *se_dev;	/* Security Engine device */
	struct tegra_se_slot *slot;	/* Security Engine key slot */
	u32 keylen;			/* key length in bits */

	u8 *mac;
	dma_addr_t mac_addr;
	u8 *enc_mac;			/* Used during encryption */
	dma_addr_t enc_mac_addr;
	u8 *dec_mac;			/* Used during decryption */
	dma_addr_t dec_mac_addr;

	u8 *buf[4];			/* Temporary buffers */
	dma_addr_t buf_addr[4];		/* DMA address for buffers */
	unsigned int authsize;		/* MAC size */
};

/* Security Engine AES GCM context */
struct tegra_se_aes_gcm_ctx {
	struct tegra_se_dev *se_dev;	/* Security Engine device */
	struct tegra_se_slot *slot;	/* Security Engine key slot */
	u32 keylen;			/* key length in bits */

	u8 *mac;
	dma_addr_t mac_addr;
	unsigned int authsize;		/* MAC size */
};

/* Security Engine key slot */
struct tegra_se_slot {
	struct list_head node;
	u8 slot_num;	/* Key slot number */
	bool available; /* Tells whether key slot is free to use */
};

static struct tegra_se_slot ssk_slot = {
	.slot_num = 15,
	.available = false,
};

static struct tegra_se_slot keymem_slot = {
	.slot_num = 14,
	.available = false,
};

static struct tegra_se_slot srk_slot = {
	.slot_num = 0,
	.available = false,
};

static struct tegra_se_slot pre_allocated_slot = {
	.slot_num = 0,
	.available = false,
};

struct tegra_se_cmdbuf {
	atomic_t free;
	u32 *cmdbuf_addr;
	dma_addr_t iova;
};

static LIST_HEAD(key_slot);
static LIST_HEAD(rsa_key_slot);
static DEFINE_SPINLOCK(rsa_key_slot_lock);
static DEFINE_SPINLOCK(key_slot_lock);

#define RNG_RESEED_INTERVAL	0x00773594

/* create a work for handling the async transfers */
static void tegra_se_work_handler(struct work_struct *work);

static int force_reseed_count;

#define GET_MSB(x)  ((x) >> (8 * sizeof(x) - 1))
#define BOOST_PERIOD	(msecs_to_jiffies(2 * 1000)) /* 2 seconds */

static unsigned int boost_cpu_freq;
module_param(boost_cpu_freq, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(boost_cpu_freq, "CPU frequency (in MHz) to boost");

static void tegra_se_restore_cpu_freq_fn(struct work_struct *work)
{
	struct tegra_se_dev *se_dev = container_of(
			work, struct tegra_se_dev, restore_cpufreq_work.work);
	unsigned long delay = BOOST_PERIOD;

	mutex_lock(&se_dev->boost_cpufreq_lock);
	if (time_is_after_jiffies(se_dev->cpufreq_last_boosted + delay)) {
		schedule_delayed_work(&se_dev->restore_cpufreq_work, delay);
	} else {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0)
		pm_qos_update_request(&se_dev->boost_cpufreq_req,
				      PM_QOS_DEFAULT_VALUE);
#else
		cpu_latency_qos_update_request(&se_dev->boost_cpufreq_req,
					       PM_QOS_DEFAULT_VALUE);
#endif
		se_dev->cpufreq_boosted = false;
	}
	mutex_unlock(&se_dev->boost_cpufreq_lock);
}

static void tegra_se_boost_cpu_freq(struct tegra_se_dev *se_dev)
{
	unsigned long delay = BOOST_PERIOD;
	s32 cpufreq_hz = boost_cpu_freq * 1000;

	mutex_lock(&se_dev->boost_cpufreq_lock);
	if (!se_dev->cpufreq_boosted) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0)
		pm_qos_update_request(&se_dev->boost_cpufreq_req, cpufreq_hz);
#else
		cpu_latency_qos_update_request(&se_dev->boost_cpufreq_req,
					       cpufreq_hz);
#endif
		schedule_delayed_work(&se_dev->restore_cpufreq_work, delay);
		se_dev->cpufreq_boosted = true;
	}

	se_dev->cpufreq_last_boosted = jiffies;
	mutex_unlock(&se_dev->boost_cpufreq_lock);
}

static void tegra_se_boost_cpu_init(struct tegra_se_dev *se_dev)
{
	boost_cpu_freq = se_dev->chipdata->cpu_freq_mhz;

	INIT_DELAYED_WORK(&se_dev->restore_cpufreq_work,
			  tegra_se_restore_cpu_freq_fn);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0)
	pm_qos_add_request(&se_dev->boost_cpufreq_req, PM_QOS_CPU_FREQ_MIN,
			   PM_QOS_DEFAULT_VALUE);
#else
	cpu_latency_qos_add_request(&se_dev->boost_cpufreq_req,
				    PM_QOS_DEFAULT_VALUE);
#endif

	mutex_init(&se_dev->boost_cpufreq_lock);
}

static void tegra_se_boost_cpu_deinit(struct tegra_se_dev *se_dev)
{
	mutex_destroy(&se_dev->boost_cpufreq_lock);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 7, 0)
	pm_qos_remove_request(&se_dev->boost_cpufreq_req);
#else
	cpu_latency_qos_remove_request(&se_dev->boost_cpufreq_req);
#endif
	cancel_delayed_work_sync(&se_dev->restore_cpufreq_work);
}

static void tegra_se_leftshift_onebit(u8 *in_buf, u32 size, u8 *org_msb)
{
	u8 carry;
	u32 i;

	*org_msb = GET_MSB(in_buf[0]);

	/* left shift one bit */
	in_buf[0] <<= 1;
	for (carry = 0, i = 1; i < size; i++) {
		carry = GET_MSB(in_buf[i]);
		in_buf[i - 1] |= carry;
		in_buf[i] <<= 1;
	}
}

static inline void se_writel(struct tegra_se_dev *se_dev, unsigned int val,
			     unsigned int reg_offset)
{
	writel(val, se_dev->io_regs + reg_offset);
}

static inline unsigned int se_readl(struct tegra_se_dev *se_dev,
				    unsigned int reg_offset)
{
	unsigned int val;

	val = readl(se_dev->io_regs + reg_offset);

	return val;
}

static int tegra_se_init_cmdbuf_addr(struct tegra_se_dev *se_dev)
{
	int i = 0;

	se_dev->cmdbuf_addr_list = devm_kzalloc(
				se_dev->dev, sizeof(struct tegra_se_cmdbuf) *
				SE_MAX_SUBMIT_CHAIN_SZ, GFP_KERNEL);
	if (!se_dev->cmdbuf_addr_list)
		return -ENOMEM;

	for (i = 0; i < SE_MAX_SUBMIT_CHAIN_SZ; i++) {
		se_dev->cmdbuf_addr_list[i].cmdbuf_addr =
			se_dev->aes_cmdbuf_cpuvaddr + (i * SZ_4K);
		se_dev->cmdbuf_addr_list[i].iova = se_dev->aes_cmdbuf_iova +
					(i * SZ_4K * SE_WORD_SIZE_BYTES);
		atomic_set(&se_dev->cmdbuf_addr_list[i].free, 1);
	}

	return 0;
}

static void tegra_se_free_key_slot(struct tegra_se_slot *slot)
{
	if (slot) {
		spin_lock(&key_slot_lock);
		slot->available = true;
		spin_unlock(&key_slot_lock);
	}
}

static struct tegra_se_slot *tegra_se_alloc_key_slot(void)
{
	struct tegra_se_slot *slot = NULL;
	bool found = false;

	spin_lock(&key_slot_lock);
	list_for_each_entry(slot, &key_slot, node) {
		if (slot->available &&
		    (slot->slot_num != pre_allocated_slot.slot_num)) {
			slot->available = false;
			found = true;
			break;
		}
	}
	spin_unlock(&key_slot_lock);

	return found ? slot : NULL;
}

static int tegra_init_key_slot(struct tegra_se_dev *se_dev)
{
	int i;

	spin_lock_init(&key_slot_lock);
	spin_lock(&key_slot_lock);
	/*
	 *To avoid multiple secure engine initializing
	 *key-slots.
	 */
	if (key_slot.prev != key_slot.next) {
		spin_unlock(&key_slot_lock);
		return 0;
	}
	spin_unlock(&key_slot_lock);

	se_dev->slot_list = devm_kzalloc(se_dev->dev,
					 sizeof(struct tegra_se_slot) *
					 TEGRA_SE_KEYSLOT_COUNT, GFP_KERNEL);
	if (!se_dev->slot_list)
		return -ENOMEM;

	spin_lock(&key_slot_lock);
	for (i = 0; i < TEGRA_SE_KEYSLOT_COUNT; i++) {
		/*
		 * Slot 0, 14 and 15 are reserved and will not be added to the
		 * free slots pool. Slot 0 is used for SRK generation, Slot 14
		 * for handling keys which are stored in memories and Slot 15 is
		 * is used for SSK operation.
		 */
		if (se_dev->chipdata->kac_type == SE_KAC_T18X) {
			if ((i == srk_slot.slot_num) || (i == ssk_slot.slot_num)
				|| (i == keymem_slot.slot_num))
				continue;
		}
		se_dev->slot_list[i].available = true;
		se_dev->slot_list[i].slot_num = i;
		INIT_LIST_HEAD(&se_dev->slot_list[i].node);
		list_add_tail(&se_dev->slot_list[i].node, &key_slot);
	}
	spin_unlock(&key_slot_lock);

	return 0;
}

static int tegra_se_alloc_ll_buf(struct tegra_se_dev *se_dev, u32 num_src_sgs,
				 u32 num_dst_sgs)
{
	if (se_dev->src_ll_buf || se_dev->dst_ll_buf) {
		dev_err(se_dev->dev,
			"trying to allocate memory to allocated memory\n");
		return -EBUSY;
	}

	if (num_src_sgs) {
		se_dev->src_ll_size = sizeof(struct tegra_se_ll) * num_src_sgs;
		se_dev->src_ll_buf = dma_alloc_coherent(
					se_dev->dev, se_dev->src_ll_size,
					&se_dev->src_ll_buf_adr, GFP_KERNEL);
		if (!se_dev->src_ll_buf) {
			dev_err(se_dev->dev,
				"can not allocate src lldma buffer\n");
			return -ENOMEM;
		}
	}
	if (num_dst_sgs) {
		se_dev->dst_ll_size = sizeof(struct tegra_se_ll) * num_dst_sgs;
		se_dev->dst_ll_buf = dma_alloc_coherent(
					se_dev->dev, se_dev->dst_ll_size,
					&se_dev->dst_ll_buf_adr, GFP_KERNEL);
		if (!se_dev->dst_ll_buf) {
			dev_err(se_dev->dev,
				"can not allocate dst ll dma buffer\n");
			return -ENOMEM;
		}
	}

	return 0;
}

static void tegra_se_free_ll_buf(struct tegra_se_dev *se_dev)
{
	if (se_dev->src_ll_buf) {
		dma_free_coherent(se_dev->dev, se_dev->src_ll_size,
				  se_dev->src_ll_buf, se_dev->src_ll_buf_adr);
		se_dev->src_ll_buf = NULL;
	}

	if (se_dev->dst_ll_buf) {
		dma_free_coherent(se_dev->dev, se_dev->dst_ll_size,
				  se_dev->dst_ll_buf, se_dev->dst_ll_buf_adr);
		se_dev->dst_ll_buf = NULL;
	}
}

static u32 tegra_se_get_config(struct tegra_se_dev *se_dev,
			       enum tegra_se_aes_op_mode mode, bool encrypt,
			       u32 data)
{
	u32 val = 0, key_len, is_last;

	switch (mode) {
	case SE_AES_OP_MODE_CBC:
	case SE_AES_OP_MODE_CMAC:
		key_len = data;
		if (encrypt) {
			val = SE_CONFIG_ENC_ALG(ALG_AES_ENC);
			switch (se_dev->chipdata->kac_type) {
			case SE_KAC_T23X:
				if (mode == SE_AES_OP_MODE_CMAC)
					val |= SE_CONFIG_ENC_MODE(MODE_CMAC);
				break;
			case SE_KAC_T18X:
				if (key_len == TEGRA_SE_KEY_256_SIZE)
					val |= SE_CONFIG_ENC_MODE(MODE_KEY256);
				else if (key_len == TEGRA_SE_KEY_192_SIZE)
					val |= SE_CONFIG_ENC_MODE(MODE_KEY192);
				else
					val |= SE_CONFIG_ENC_MODE(MODE_KEY128);
				break;
			default:
				break;
			}

			val |= SE_CONFIG_DEC_ALG(ALG_NOP);
		} else {
			val = SE_CONFIG_DEC_ALG(ALG_AES_DEC);
			if (se_dev->chipdata->kac_type == SE_KAC_T18X) {
				if (key_len == TEGRA_SE_KEY_256_SIZE)
					val |= SE_CONFIG_DEC_MODE(MODE_KEY256);
				else if (key_len == TEGRA_SE_KEY_192_SIZE)
					val |= SE_CONFIG_DEC_MODE(MODE_KEY192);
				else
					val |= SE_CONFIG_DEC_MODE(MODE_KEY128);
			}
		}
		if (mode == SE_AES_OP_MODE_CMAC)
			val |= SE_CONFIG_DST(DST_HASHREG);
		else
			val |= SE_CONFIG_DST(DST_MEMORY);
		break;

	case SE_AES_OP_MODE_CBC_MAC:
		val = SE_CONFIG_ENC_ALG(ALG_AES_ENC)
			| SE_CONFIG_DEC_ALG(ALG_NOP)
			| SE_CONFIG_DST(DST_HASHREG);
		break;

	case SE_AES_OP_MODE_GCM:
		if (data == SE_AES_GMAC) {
			if (encrypt) {
				val = SE_CONFIG_ENC_ALG(ALG_AES_ENC)
					| SE_CONFIG_DEC_ALG(ALG_NOP)
					| SE_CONFIG_ENC_MODE(MODE_GMAC);
			} else {
				val = SE_CONFIG_ENC_ALG(ALG_NOP)
					| SE_CONFIG_DEC_ALG(ALG_AES_DEC)
					| SE_CONFIG_DEC_MODE(MODE_GMAC);
			}
		} else if (data == SE_AES_GCM_ENC) {
			val = SE_CONFIG_ENC_ALG(ALG_AES_ENC)
				| SE_CONFIG_DEC_ALG(ALG_NOP)
				| SE_CONFIG_ENC_MODE(MODE_GCM);
		} else if (data == SE_AES_GCM_DEC) {
			val = SE_CONFIG_ENC_ALG(ALG_NOP)
				| SE_CONFIG_DEC_ALG(ALG_AES_DEC)
				| SE_CONFIG_DEC_MODE(MODE_GCM);
		} else if (data == SE_AES_GCM_FINAL) {
			if (encrypt) {
				val = SE_CONFIG_ENC_ALG(ALG_AES_ENC)
					| SE_CONFIG_DEC_ALG(ALG_NOP)
					| SE_CONFIG_ENC_MODE(MODE_GCM_FINAL);
			} else {
				val = SE_CONFIG_ENC_ALG(ALG_NOP)
					| SE_CONFIG_DEC_ALG(ALG_AES_DEC)
					| SE_CONFIG_DEC_MODE(MODE_GCM_FINAL);
			}
		}
		break;

	case SE_AES_OP_MODE_RNG_DRBG:
		if (se_dev->chipdata->kac_type == SE_KAC_T23X) {
			val = SE_CONFIG_ENC_ALG(ALG_RNG) |
				SE_CONFIG_DEC_ALG(ALG_NOP) |
				SE_CONFIG_DST(DST_MEMORY);
		} else {
			val = SE_CONFIG_ENC_ALG(ALG_RNG) |
				SE_CONFIG_ENC_MODE(MODE_KEY192) |
				SE_CONFIG_DST(DST_MEMORY);
		}
		break;

	case SE_AES_OP_MODE_ECB:
		key_len = data;
		if (encrypt) {
			val = SE_CONFIG_ENC_ALG(ALG_AES_ENC);
			if (se_dev->chipdata->kac_type == SE_KAC_T18X) {
				if (key_len == TEGRA_SE_KEY_256_SIZE)
					val |= SE_CONFIG_ENC_MODE(MODE_KEY256);
				else if (key_len == TEGRA_SE_KEY_192_SIZE)
					val |= SE_CONFIG_ENC_MODE(MODE_KEY192);
				else
					val |= SE_CONFIG_ENC_MODE(MODE_KEY128);
			}
		} else {
			val = SE_CONFIG_DEC_ALG(ALG_AES_DEC);
			if (se_dev->chipdata->kac_type == SE_KAC_T18X) {
				if (key_len == TEGRA_SE_KEY_256_SIZE)
					val |= SE_CONFIG_DEC_MODE(MODE_KEY256);
				else if (key_len == TEGRA_SE_KEY_192_SIZE)
					val |= SE_CONFIG_DEC_MODE(MODE_KEY192);
				else
					val |= SE_CONFIG_DEC_MODE(MODE_KEY128);
			}
		}
		val |= SE_CONFIG_DST(DST_MEMORY);
		break;
	case SE_AES_OP_MODE_CTR:
		key_len = data;
		if (encrypt) {
			val = SE_CONFIG_ENC_ALG(ALG_AES_ENC);
			if (se_dev->chipdata->kac_type == SE_KAC_T18X) {
				if (key_len == TEGRA_SE_KEY_256_SIZE)
					val |= SE_CONFIG_ENC_MODE(MODE_KEY256);
				else if (key_len == TEGRA_SE_KEY_192_SIZE)
					val |= SE_CONFIG_ENC_MODE(MODE_KEY192);
				else
					val |= SE_CONFIG_ENC_MODE(MODE_KEY128);
			}
		} else {
			val = SE_CONFIG_DEC_ALG(ALG_AES_DEC);
			if (se_dev->chipdata->kac_type == SE_KAC_T18X) {
				if (key_len == TEGRA_SE_KEY_256_SIZE)
					val |= SE_CONFIG_DEC_MODE(MODE_KEY256);
				else if (key_len == TEGRA_SE_KEY_192_SIZE)
					val |= SE_CONFIG_DEC_MODE(MODE_KEY192);
				else
					val |= SE_CONFIG_DEC_MODE(MODE_KEY128);
			}
		}
		val |= SE_CONFIG_DST(DST_MEMORY);
		break;
	case SE_AES_OP_MODE_OFB:
		key_len = data;
		if (encrypt) {
			val = SE_CONFIG_ENC_ALG(ALG_AES_ENC);
			if (se_dev->chipdata->kac_type == SE_KAC_T18X) {
				if (key_len == TEGRA_SE_KEY_256_SIZE)
					val |= SE_CONFIG_ENC_MODE(MODE_KEY256);
				else if (key_len == TEGRA_SE_KEY_192_SIZE)
					val |= SE_CONFIG_ENC_MODE(MODE_KEY192);
				else
					val |= SE_CONFIG_ENC_MODE(MODE_KEY128);
			}
		} else {
			val = SE_CONFIG_DEC_ALG(ALG_AES_DEC);
			if (se_dev->chipdata->kac_type == SE_KAC_T18X) {
				if (key_len == TEGRA_SE_KEY_256_SIZE)
					val |= SE_CONFIG_DEC_MODE(MODE_KEY256);
				else if (key_len == TEGRA_SE_KEY_192_SIZE)
					val |= SE_CONFIG_DEC_MODE(MODE_KEY192);
				else
					val |= SE_CONFIG_DEC_MODE(MODE_KEY128);
			}
		}
		val |= SE_CONFIG_DST(DST_MEMORY);
		break;

	case SE_AES_OP_MODE_SHA1:
		is_last = data;
		val = SE_CONFIG_DEC_ALG(ALG_NOP) |
			SE_CONFIG_ENC_ALG(ALG_SHA) |
			SE_CONFIG_ENC_MODE(MODE_SHA1) |
			SE_CONFIG_DST(DST_MEMORY);
		if (!is_last)
			val |= SE_CONFIG_DST(DST_HASHREG);
		break;
	case SE_AES_OP_MODE_SHA224:
		is_last = data;
		val = SE_CONFIG_DEC_ALG(ALG_NOP) |
			SE_CONFIG_ENC_ALG(ALG_SHA) |
			SE_CONFIG_ENC_MODE(MODE_SHA224) |
			SE_CONFIG_DST(DST_MEMORY);
		if (!is_last)
			val |= SE_CONFIG_DST(DST_HASHREG);
		break;
	case SE_AES_OP_MODE_SHA256:
		is_last = data;
		val = SE_CONFIG_DEC_ALG(ALG_NOP) |
			SE_CONFIG_ENC_ALG(ALG_SHA) |
			SE_CONFIG_ENC_MODE(MODE_SHA256) |
			SE_CONFIG_DST(DST_MEMORY);
		if (!is_last)
			val |= SE_CONFIG_DST(DST_HASHREG);
		break;
	case SE_AES_OP_MODE_SHA384:
		is_last = data;
		val = SE_CONFIG_DEC_ALG(ALG_NOP) |
			SE_CONFIG_ENC_ALG(ALG_SHA) |
			SE_CONFIG_ENC_MODE(MODE_SHA384) |
			SE_CONFIG_DST(DST_MEMORY);
		if (!is_last)
			val |= SE_CONFIG_DST(DST_HASHREG);
		break;
	case SE_AES_OP_MODE_SHA512:
		is_last = data;
		val = SE_CONFIG_DEC_ALG(ALG_NOP) |
			SE_CONFIG_ENC_ALG(ALG_SHA) |
			SE_CONFIG_ENC_MODE(MODE_SHA512) |
			SE_CONFIG_DST(DST_MEMORY);
		if (!is_last)
			val |= SE_CONFIG_DST(DST_HASHREG);
		break;
	case SE_AES_OP_MODE_SHA3_224:
		is_last = data;
		val = SE_CONFIG_DEC_ALG(ALG_NOP) |
			SE_CONFIG_ENC_ALG(ALG_SHA) |
			SE_CONFIG_ENC_MODE(MODE_SHA3_224) |
			SE_CONFIG_DST(DST_MEMORY);
		if (!is_last)
			val |= SE_CONFIG_DST(DST_HASHREG);
		break;
	case SE_AES_OP_MODE_SHA3_256:
		is_last = data;
		val = SE_CONFIG_DEC_ALG(ALG_NOP) |
			SE_CONFIG_ENC_ALG(ALG_SHA) |
			SE_CONFIG_ENC_MODE(MODE_SHA3_256) |
			SE_CONFIG_DST(DST_MEMORY);
		if (!is_last)
			val |= SE_CONFIG_DST(DST_HASHREG);
		break;
	case SE_AES_OP_MODE_SHA3_384:
		is_last = data;
		val = SE_CONFIG_DEC_ALG(ALG_NOP) |
			SE_CONFIG_ENC_ALG(ALG_SHA) |
			SE_CONFIG_ENC_MODE(MODE_SHA3_384) |
			SE_CONFIG_DST(DST_MEMORY);
		if (!is_last)
			val |= SE_CONFIG_DST(DST_HASHREG);
		break;
	case SE_AES_OP_MODE_SHA3_512:
		is_last = data;
		val = SE_CONFIG_DEC_ALG(ALG_NOP) |
			SE_CONFIG_ENC_ALG(ALG_SHA) |
			SE_CONFIG_ENC_MODE(MODE_SHA3_512) |
			SE_CONFIG_DST(DST_MEMORY);
		if (!is_last)
			val |= SE_CONFIG_DST(DST_HASHREG);
		break;
	case SE_AES_OP_MODE_SHAKE128:
		is_last = data;
		val = SE_CONFIG_DEC_ALG(ALG_NOP) |
			SE_CONFIG_ENC_ALG(ALG_SHA) |
			SE_CONFIG_ENC_MODE(MODE_SHAKE128) |
			SE_CONFIG_DST(DST_MEMORY);
		if (!is_last)
			val |= SE_CONFIG_DST(DST_HASHREG);
		break;
	case SE_AES_OP_MODE_SHAKE256:
		is_last = data;
		val = SE_CONFIG_DEC_ALG(ALG_NOP) |
			SE_CONFIG_ENC_ALG(ALG_SHA) |
			SE_CONFIG_ENC_MODE(MODE_SHAKE256) |
			SE_CONFIG_DST(DST_MEMORY);
		if (!is_last)
			val |= SE_CONFIG_DST(DST_HASHREG);
		break;
	case SE_AES_OP_MODE_HMAC_SHA224:
		is_last = data;
		val = SE_CONFIG_DEC_ALG(ALG_NOP) |
			SE_CONFIG_ENC_ALG(ALG_HMAC) |
			SE_CONFIG_ENC_MODE(MODE_SHA224) |
			SE_CONFIG_DST(DST_MEMORY);
		if (!is_last)
			val |= SE_CONFIG_DST(DST_HASHREG);
		break;
	case SE_AES_OP_MODE_HMAC_SHA256:
		is_last = data;
		val = SE_CONFIG_DEC_ALG(ALG_NOP) |
			SE_CONFIG_ENC_ALG(ALG_HMAC) |
			SE_CONFIG_ENC_MODE(MODE_SHA256) |
			SE_CONFIG_DST(DST_MEMORY);
		if (!is_last)
			val |= SE_CONFIG_DST(DST_HASHREG);
		break;
	case SE_AES_OP_MODE_HMAC_SHA384:
		is_last = data;
		val = SE_CONFIG_DEC_ALG(ALG_NOP) |
			SE_CONFIG_ENC_ALG(ALG_HMAC) |
			SE_CONFIG_ENC_MODE(MODE_SHA384) |
			SE_CONFIG_DST(DST_MEMORY);
		if (!is_last)
			val |= SE_CONFIG_DST(DST_HASHREG);
		break;
	case SE_AES_OP_MODE_HMAC_SHA512:
		is_last = data;
		val = SE_CONFIG_DEC_ALG(ALG_NOP) |
			SE_CONFIG_ENC_ALG(ALG_HMAC) |
			SE_CONFIG_ENC_MODE(MODE_SHA512) |
			SE_CONFIG_DST(DST_MEMORY);
		if (!is_last)
			val |= SE_CONFIG_DST(DST_HASHREG);
		break;
	case SE_AES_OP_MODE_XTS:
		key_len = data;
		if (encrypt) {
			val = SE_CONFIG_ENC_ALG(ALG_AES_ENC);
			if (se_dev->chipdata->kac_type == SE_KAC_T18X) {
				if ((key_len / 2) == TEGRA_SE_KEY_256_SIZE)
					val |= SE_CONFIG_ENC_MODE(MODE_KEY256);
				else
					val |= SE_CONFIG_ENC_MODE(MODE_KEY128);
			}
			val |= SE_CONFIG_DEC_ALG(ALG_NOP);
		} else {
			val = SE_CONFIG_DEC_ALG(ALG_AES_DEC);
			if (se_dev->chipdata->kac_type == SE_KAC_T18X) {
				if (key_len / 2 == TEGRA_SE_KEY_256_SIZE)
					val |= SE_CONFIG_DEC_MODE(MODE_KEY256);
				else
					val |= SE_CONFIG_DEC_MODE(MODE_KEY128);
			}
			val |= SE_CONFIG_ENC_ALG(ALG_NOP);
		}
			val |= SE_CONFIG_DST(DST_MEMORY);
		break;
	case SE_AES_OP_MODE_INS:
		val = SE_CONFIG_ENC_ALG(ALG_INS);
		val |= SE_CONFIG_DEC_ALG(ALG_NOP);
		break;
	default:
		dev_warn(se_dev->dev, "Invalid operation mode\n");
		break;
	}

	pr_debug("%s:%d config val = 0x%x\n", __func__, __LINE__, val);

	return val;
}

static void tegra_unmap_sg(struct device *dev, struct scatterlist *sg,
			   enum dma_data_direction dir, u32 total)
{
	u32 total_loop;

	total_loop = total;
	while (sg && (total_loop > 0)) {
		dma_unmap_sg(dev, sg, 1, dir);
		total_loop -= min_t(size_t, (size_t)sg->length,
				(size_t)total_loop);
		sg = sg_next(sg);
	}
}

static unsigned int tegra_se_count_sgs(struct scatterlist *sl, u32 nbytes)
{
	unsigned int sg_nents = 0;

	while (sl) {
		sg_nents++;
		nbytes -= min((size_t)sl->length, (size_t)nbytes);
		if (!nbytes)
			break;
		sl = sg_next(sl);
	}

	return sg_nents;
}

static int tegra_se_get_free_cmdbuf(struct tegra_se_dev *se_dev)
{
	int i = 0;
	unsigned int index = se_dev->cmdbuf_list_entry + 1;

	for (i = 0; i < SE_MAX_CMDBUF_TIMEOUT; i++, index++) {
		index = index % SE_MAX_SUBMIT_CHAIN_SZ;
		if (atomic_read(&se_dev->cmdbuf_addr_list[index].free)) {
			atomic_set(&se_dev->cmdbuf_addr_list[index].free, 0);
			break;
		}
		if (i % SE_MAX_SUBMIT_CHAIN_SZ == 0)
			udelay(SE_WAIT_UDELAY);
	}

	return (i == SE_MAX_CMDBUF_TIMEOUT) ? -ENOMEM : index;
}

static void tegra_se_sha_complete_callback(void *priv, int nr_completed)
{
	struct tegra_se_priv_data *priv_data = priv;
	struct ahash_request *req;
	struct tegra_se_dev *se_dev;
	struct crypto_ahash *tfm;
	struct tegra_se_req_context *req_ctx;
	struct tegra_se_sha_context *sha_ctx;
	int dst_len;

	pr_debug("%s:%d sha callback", __func__, __LINE__);

	se_dev = priv_data->se_dev;
	atomic_set(&se_dev->cmdbuf_addr_list[priv_data->cmdbuf_node].free, 1);

	req = priv_data->sha_req;
	if (!req) {
		dev_err(se_dev->dev, "Invalid request for callback\n");
		devm_kfree(se_dev->dev, priv_data);
		return;
	}

	tfm = crypto_ahash_reqtfm(req);
	req_ctx = ahash_request_ctx(req);
	dst_len = crypto_ahash_digestsize(tfm);
	sha_ctx = crypto_ahash_ctx(tfm);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	/* For SHAKE128/SHAKE256, digest size can vary */
	if (sha_ctx->op_mode == SE_AES_OP_MODE_SHAKE128
			|| sha_ctx->op_mode == SE_AES_OP_MODE_SHAKE256)
		dst_len = req->dst_size;
#endif

	/* copy result */
	if (sha_ctx->is_final)
		memcpy(req->result, req_ctx->hash_result, dst_len);

	if (priv_data->sha_src_mapped)
		tegra_unmap_sg(se_dev->dev, req->src, DMA_TO_DEVICE,
			       priv_data->src_bytes_mapped);

	if (priv_data->sha_dst_mapped)
		tegra_unmap_sg(se_dev->dev, &priv_data->sg, DMA_FROM_DEVICE,
			       priv_data->dst_bytes_mapped);

	devm_kfree(se_dev->dev, req_ctx->hash_result);

	req->base.complete(&req->base, 0);

	devm_kfree(se_dev->dev, priv_data);

	pr_debug("%s:%d sha callback complete", __func__, __LINE__);
}

static void tegra_se_aes_complete_callback(void *priv, int nr_completed)
{
	int i = 0;
	struct tegra_se_priv_data *priv_data = priv;
	struct skcipher_request *req;
	struct tegra_se_dev *se_dev;
	void *buf;
	u32 num_sgs;

	pr_debug("%s(%d) aes callback\n", __func__, __LINE__);

	se_dev = priv_data->se_dev;
	atomic_set(&se_dev->cmdbuf_addr_list[priv_data->cmdbuf_node].free, 1);

	if (!priv_data->req_cnt) {
		devm_kfree(se_dev->dev, priv_data);
		return;
	}

	if (!se_dev->ioc)
		dma_sync_single_for_cpu(se_dev->dev, priv_data->buf_addr,
				priv_data->gather_buf_sz, DMA_BIDIRECTIONAL);

	buf = priv_data->buf;
	for (i = 0; i < priv_data->req_cnt; i++) {
		req = priv_data->reqs[i];
		if (!req) {
			dev_err(se_dev->dev, "Invalid request for callback\n");
			if (priv_data->dynmem)
				kfree(priv_data->buf);
			devm_kfree(se_dev->dev, priv_data);
			return;
		}

		num_sgs = tegra_se_count_sgs(req->dst, req->cryptlen);
		if (num_sgs == 1)
			memcpy(sg_virt(req->dst), buf, req->cryptlen);
		else
			sg_copy_from_buffer(req->dst, num_sgs, buf,
					    req->cryptlen);

		buf += req->cryptlen;
		req->base.complete(&req->base, 0);
	}

	if (!se_dev->ioc)
		dma_unmap_sg(se_dev->dev, &priv_data->sg, 1, DMA_BIDIRECTIONAL);

	if (unlikely(priv_data->dynmem)) {
		if (se_dev->ioc)
			dma_free_coherent(se_dev->dev, priv_data->gather_buf_sz,
					  priv_data->buf, priv_data->buf_addr);
		else
			kfree(priv_data->buf);
	} else {
		atomic_set(&se_dev->aes_buf_stat[priv_data->aesbuf_entry], 1);
	}

	devm_kfree(se_dev->dev, priv_data);
	pr_debug("%s(%d) aes callback complete\n", __func__, __LINE__);
}

static void se_nvhost_write_method(u32 *buf, u32 op1, u32 op2, u32 *offset)
{
	int i = 0;

	buf[i++] = op1;
	buf[i++] = op2;
	*offset = *offset + 2;
}

static int tegra_se_channel_submit_gather(struct tegra_se_dev *se_dev,
					  u32 *cpuvaddr, dma_addr_t iova,
					  u32 offset, u32 num_words,
					  enum tegra_se_callback callback)
{
	int i = 0;
	struct nvhost_job *job = NULL;
	u32 syncpt_id = 0;
	int err = 0;
	struct tegra_se_priv_data *priv = NULL;
	struct nvhost_device_data *pdata = platform_get_drvdata(se_dev->pdev);

	if (callback) {
		priv = devm_kzalloc(se_dev->dev,
				    sizeof(struct tegra_se_priv_data),
				    GFP_KERNEL);
		if (!priv)
			return -ENOMEM;
	}

	err = nvhost_module_busy(se_dev->pdev);
	if (err) {
		dev_err(se_dev->dev, "nvhost_module_busy failed for se_dev\n");
		if (priv)
			devm_kfree(se_dev->dev, priv);
		return err;
	}

	if (!se_dev->channel) {
		err = nvhost_channel_map(pdata, &se_dev->channel, pdata);
		if (err) {
			dev_err(se_dev->dev, "Nvhost Channel map failed\n");
			goto exit;
		}
	}

	job = nvhost_job_alloc(se_dev->channel, 1, 0, 0, 1);
	if (!job) {
		dev_err(se_dev->dev, "Nvhost Job allocation failed\n");
		err = -ENOMEM;
		goto exit;
	}

	if (!se_dev->syncpt_id) {
		se_dev->syncpt_id = nvhost_get_syncpt_host_managed(
					se_dev->pdev, 0, se_dev->pdev->name);
		if (!se_dev->syncpt_id) {
			dev_err(se_dev->dev, "Cannot get syncpt_id for SE(%s)\n",
				se_dev->pdev->name);
			err = -ENOMEM;
			goto error;
		}
	}
	syncpt_id = se_dev->syncpt_id;

	/* initialize job data */
	job->sp->id = syncpt_id;
	job->sp->incrs = 1;
	job->num_syncpts = 1;

	/* push increment after work has been completed */
	se_nvhost_write_method(&cpuvaddr[num_words], nvhost_opcode_nonincr(
					host1x_uclass_incr_syncpt_r(), 1),
			       nvhost_class_host_incr_syncpt(
				host1x_uclass_incr_syncpt_cond_op_done_v(),
				syncpt_id), &num_words);

	err = nvhost_job_add_client_gather_address(job, num_words,
						   pdata->class, iova);
	if (err) {
		dev_err(se_dev->dev, "Nvhost failed to add gather\n");
		goto error;
	}

	err = nvhost_channel_submit(job);
	if (err) {
		dev_err(se_dev->dev, "Nvhost submit failed\n");
		goto error;
	}
	pr_debug("%s:%d submitted job\n", __func__, __LINE__);

	if (callback == AES_CB) {
		priv->se_dev = se_dev;
		for (i = 0; i < se_dev->req_cnt; i++)
			priv->reqs[i] = se_dev->reqs[i];

		if (!se_dev->ioc)
			priv->sg = se_dev->sg;

		if (unlikely(se_dev->dynamic_mem)) {
			priv->buf = se_dev->aes_buf;
			priv->dynmem = se_dev->dynamic_mem;
		} else {
			priv->buf = se_dev->aes_bufs[se_dev->aesbuf_entry];
			priv->aesbuf_entry = se_dev->aesbuf_entry;
		}

		priv->buf_addr = se_dev->aes_addr;
		priv->req_cnt = se_dev->req_cnt;
		priv->gather_buf_sz = se_dev->gather_buf_sz;
		priv->cmdbuf_node = se_dev->cmdbuf_list_entry;

		/* Register callback to be called once
		 * syncpt value has been reached
		 */
		err = nvhost_intr_register_fast_notifier(
			se_dev->pdev, job->sp->id, job->sp->fence,
			tegra_se_aes_complete_callback, priv);
		if (err) {
			dev_err(se_dev->dev,
				"add nvhost interrupt action failed for AES\n");
			goto error;
		}
	} else if (callback == SHA_CB) {
		priv->se_dev = se_dev;
		priv->sha_req = se_dev->sha_req;
		priv->sg = se_dev->sg;
		priv->src_bytes_mapped = se_dev->src_bytes_mapped;
		priv->dst_bytes_mapped = se_dev->dst_bytes_mapped;
		priv->sha_src_mapped = se_dev->sha_src_mapped;
		priv->sha_dst_mapped = se_dev->sha_dst_mapped;
		priv->sha_last = se_dev->sha_last;
		priv->buf_addr = se_dev->dst_ll->addr;
		priv->cmdbuf_node = se_dev->cmdbuf_list_entry;

		err = nvhost_intr_register_fast_notifier(
			se_dev->pdev, job->sp->id, job->sp->fence,
			tegra_se_sha_complete_callback, priv);
		if (err) {
			dev_err(se_dev->dev,
				"add nvhost interrupt action failed for SHA\n");
			goto error;
		}
	} else {
		/* wait until host1x has processed work */
		nvhost_syncpt_wait_timeout_ext(
			se_dev->pdev, job->sp->id, job->sp->fence,
			U32_MAX, NULL, NULL);

		if (se_dev->cmdbuf_addr_list)
			atomic_set(&se_dev->cmdbuf_addr_list[
				   se_dev->cmdbuf_list_entry].free, 1);
	}

	se_dev->req_cnt = 0;
	se_dev->gather_buf_sz = 0;
	se_dev->cmdbuf_cnt = 0;
	se_dev->src_bytes_mapped = 0;
	se_dev->dst_bytes_mapped = 0;
	se_dev->sha_src_mapped = false;
	se_dev->sha_dst_mapped = false;
	se_dev->sha_last = false;
error:
	nvhost_job_put(job);
	job = NULL;
exit:
	nvhost_module_idle(se_dev->pdev);
	if (err)
		devm_kfree(se_dev->dev, priv);

	return err;
}

static void tegra_se_send_ctr_seed(struct tegra_se_dev *se_dev, u32 *pdata,
				   unsigned int opcode_addr, u32 *cpuvaddr)
{
	u32 j;
	u32 cmdbuf_num_words = 0, i = 0;

	i = se_dev->cmdbuf_cnt;

	if (se_dev->chipdata->kac_type != SE_KAC_T23X) {
		cpuvaddr[i++] = __nvhost_opcode_nonincr(opcode_addr +
						SE_AES_CRYPTO_CTR_SPARE, 1);
		cpuvaddr[i++] = SE_AES_CTR_LITTLE_ENDIAN;
	}
	cpuvaddr[i++] = nvhost_opcode_setpayload(4);
	cpuvaddr[i++] = __nvhost_opcode_incr_w(opcode_addr +
					     SE_AES_CRYPTO_LINEAR_CTR);
	for (j = 0; j < SE_CRYPTO_CTR_REG_COUNT; j++)
		cpuvaddr[i++] = pdata[j];

	cmdbuf_num_words = i;
	se_dev->cmdbuf_cnt = i;
}

static int tegra_se_aes_ins_op(struct tegra_se_dev *se_dev, u8 *pdata,
				  u32 data_len, u8 slot_num,
				  enum tegra_se_key_table_type type,
				  unsigned int opcode_addr, u32 *cpuvaddr,
				  dma_addr_t iova,
				  enum tegra_se_callback callback)
{
	u32 *pdata_buf = (u32 *)pdata;
	u32 val = 0, j;
	u32 cmdbuf_num_words = 0, i = 0;
	int err = 0;

	struct {
		u32 OPERATION;
		u32 CRYPTO_KEYTABLE_KEYMANIFEST;
		u32 CRYPTO_KEYTABLE_DST;
		u32 CRYPTO_KEYTABLE_ADDR;
		u32 CRYPTO_KEYTABLE_DATA;
	} OFFSETS;

	if (!pdata_buf) {
		dev_err(se_dev->dev, "No key data available\n");
		return -ENODATA;
	}

	pr_debug("%s(%d) data_len = %d slot_num = %d\n", __func__, __LINE__,
						data_len, slot_num);

	/* Initialize register offsets based on SHA/AES operation. */
	switch (type) {
	case SE_KEY_TABLE_TYPE_HMAC:
		OFFSETS.OPERATION = SE_SHA_OPERATION_OFFSET;
		OFFSETS.CRYPTO_KEYTABLE_KEYMANIFEST =
				SE_SHA_CRYPTO_KEYTABLE_KEYMANIFEST_OFFSET;
		OFFSETS.CRYPTO_KEYTABLE_DST = SE_SHA_CRYPTO_KEYTABLE_DST_OFFSET;
		OFFSETS.CRYPTO_KEYTABLE_ADDR =
				SE_SHA_CRYPTO_KEYTABLE_ADDR_OFFSET;
		OFFSETS.CRYPTO_KEYTABLE_DATA =
				SE_SHA_CRYPTO_KEYTABLE_DATA_OFFSET;
		break;
	default:
		/* Use default case for AES operations. */
		OFFSETS.OPERATION = SE_AES_OPERATION_OFFSET;
		OFFSETS.CRYPTO_KEYTABLE_KEYMANIFEST =
				SE_AES_CRYPTO_KEYTABLE_KEYMANIFEST_OFFSET;
		OFFSETS.CRYPTO_KEYTABLE_DST = SE_AES_CRYPTO_KEYTABLE_DST_OFFSET;
		OFFSETS.CRYPTO_KEYTABLE_ADDR =
				SE_AES_CRYPTO_KEYTABLE_ADDR_OFFSET;
		OFFSETS.CRYPTO_KEYTABLE_DATA =
				SE_AES_CRYPTO_KEYTABLE_DATA_OFFSET;
		break;
	}

	i = se_dev->cmdbuf_cnt;

	if (!se_dev->cmdbuf_cnt) {
		cpuvaddr[i++] = nvhost_opcode_setpayload(1);
		cpuvaddr[i++] = __nvhost_opcode_incr_w(
				opcode_addr + OFFSETS.OPERATION);
		cpuvaddr[i++] = SE_OPERATION_WRSTALL(WRSTALL_TRUE) |
				SE_OPERATION_OP(OP_DUMMY);
	}

	/* key manifest */
	/* user */
	val = SE_KEYMANIFEST_USER(NS);
	/* purpose */
	switch (type) {
	case SE_KEY_TABLE_TYPE_XTS_KEY1:
	case SE_KEY_TABLE_TYPE_XTS_KEY2:
		val |= SE_KEYMANIFEST_PURPOSE(XTS);
		break;
	case SE_KEY_TABLE_TYPE_CMAC:
		val |= SE_KEYMANIFEST_PURPOSE(CMAC);
		break;
	case SE_KEY_TABLE_TYPE_HMAC:
		val |= SE_KEYMANIFEST_PURPOSE(HMAC);
		break;
	case SE_KEY_TABLE_TYPE_GCM:
		val |= SE_KEYMANIFEST_PURPOSE(GCM);
		break;
	default:
		val |= SE_KEYMANIFEST_PURPOSE(ENC);
		break;
	}
	/* size */
	if (data_len == 16)
		val |= SE_KEYMANIFEST_SIZE(KEY128);
	else if (data_len == 24)
		val |= SE_KEYMANIFEST_SIZE(KEY192);
	else if (data_len == 32)
		val |= SE_KEYMANIFEST_SIZE(KEY256);
	/* exportable */
	val |= SE_KEYMANIFEST_EX(false);

	cpuvaddr[i++] = nvhost_opcode_setpayload(1);
	cpuvaddr[i++] = __nvhost_opcode_incr_w(opcode_addr +
				OFFSETS.CRYPTO_KEYTABLE_KEYMANIFEST);
	cpuvaddr[i++] = val;

	pr_debug("%s(%d) key manifest = 0x%x\n", __func__, __LINE__, val);

	if (type != SE_KEY_TABLE_TYPE_HMAC) {
		/* configure slot number */
		cpuvaddr[i++] = nvhost_opcode_setpayload(1);
		cpuvaddr[i++] = __nvhost_opcode_incr_w(
				opcode_addr + OFFSETS.CRYPTO_KEYTABLE_DST);
		cpuvaddr[i++] = SE_AES_KEY_INDEX(slot_num);
	}

	/* write key data */
	for (j = 0; j < data_len; j += 4) {
		pr_debug("%s(%d) data_len = %d j = %d\n", __func__,
						__LINE__, data_len, j);
		cpuvaddr[i++] = nvhost_opcode_setpayload(1);
		cpuvaddr[i++] = __nvhost_opcode_incr_w(
				opcode_addr +
				OFFSETS.CRYPTO_KEYTABLE_ADDR);
		val = (j / 4); /* program quad */
		cpuvaddr[i++] = val;

		cpuvaddr[i++] = nvhost_opcode_setpayload(1);
		cpuvaddr[i++] = __nvhost_opcode_incr_w(
				opcode_addr +
				OFFSETS.CRYPTO_KEYTABLE_DATA);
		cpuvaddr[i++] = *pdata_buf++;
	}

	/* configure INS operation */
	cpuvaddr[i++] = nvhost_opcode_setpayload(1);
	cpuvaddr[i++] = __nvhost_opcode_incr_w(opcode_addr);
	cpuvaddr[i++] = tegra_se_get_config(se_dev, SE_AES_OP_MODE_INS, 0, 0);

	/* initiate key write operation to key slot */
	cpuvaddr[i++] = nvhost_opcode_setpayload(1);
	cpuvaddr[i++] = __nvhost_opcode_incr_w(
			opcode_addr + OFFSETS.OPERATION);
	cpuvaddr[i++] = SE_OPERATION_WRSTALL(WRSTALL_TRUE) |
				SE_OPERATION_OP(OP_START) |
				SE_OPERATION_LASTBUF(LASTBUF_TRUE);

	cmdbuf_num_words = i;
	se_dev->cmdbuf_cnt = i;

	err = tegra_se_channel_submit_gather(
		se_dev, cpuvaddr, iova, 0, cmdbuf_num_words, callback);

	pr_debug("%s(%d) complete\n", __func__, __LINE__);

	return err;
}

static int tegra_se_send_key_data(struct tegra_se_dev *se_dev, u8 *pdata,
				  u32 data_len, u8 slot_num,
				  enum tegra_se_key_table_type type,
				  unsigned int opcode_addr, u32 *cpuvaddr,
				  dma_addr_t iova,
				  enum tegra_se_callback callback)
{
	u32 data_size;
	u32 *pdata_buf = (u32 *)pdata;
	u8 pkt = 0, quad = 0;
	u32 val = 0, j;
	u32 cmdbuf_num_words = 0, i = 0;
	int err = 0;

	if (!pdata_buf) {
		dev_err(se_dev->dev, "No Key Data available\n");
		return -ENODATA;
	}

	if (se_dev->chipdata->kac_type == SE_KAC_T23X) {
		if (type == SE_KEY_TABLE_TYPE_ORGIV ||
			type == SE_KEY_TABLE_TYPE_UPDTDIV) {
			pr_debug("%s(%d) IV programming\n", __func__, __LINE__);

			/* Program IV using CTR registers */
			tegra_se_send_ctr_seed(se_dev, pdata_buf, opcode_addr,
							cpuvaddr);
		} else {
			/* Program key using INS operation */
			err = tegra_se_aes_ins_op(se_dev, pdata, data_len,
			slot_num, type, opcode_addr, cpuvaddr, iova, callback);
		}
		return err;
	}

	if ((type == SE_KEY_TABLE_TYPE_KEY) &&
	    (slot_num == ssk_slot.slot_num)) {
		dev_err(se_dev->dev, "SSK Key Slot used\n");
		return -EINVAL;
	}

	if ((type == SE_KEY_TABLE_TYPE_ORGIV) ||
	    (type == SE_KEY_TABLE_TYPE_XTS_KEY2) ||
	    (type == SE_KEY_TABLE_TYPE_XTS_KEY2_IN_MEM))
		quad = QUAD_ORG_IV;
	else if (type == SE_KEY_TABLE_TYPE_UPDTDIV)
		quad = QUAD_UPDTD_IV;
	else if ((type == SE_KEY_TABLE_TYPE_KEY) ||
		 (type == SE_KEY_TABLE_TYPE_XTS_KEY1) ||
		 (type == SE_KEY_TABLE_TYPE_KEY_IN_MEM) ||
		 (type == SE_KEY_TABLE_TYPE_XTS_KEY1_IN_MEM))
		quad = QUAD_KEYS_128;

	i = se_dev->cmdbuf_cnt;

	if (!se_dev->cmdbuf_cnt) {
		cpuvaddr[i++] = __nvhost_opcode_nonincr(
				opcode_addr + SE_AES_OPERATION_OFFSET, 1);
		cpuvaddr[i++] = SE_OPERATION_WRSTALL(WRSTALL_TRUE) |
				SE_OPERATION_OP(OP_DUMMY);
	}

	data_size = SE_KEYTABLE_QUAD_SIZE_BYTES;

	do {
		if (type == SE_KEY_TABLE_TYPE_XTS_KEY2 ||
				type == SE_KEY_TABLE_TYPE_XTS_KEY2_IN_MEM)
			pkt = SE_CRYPTO_KEYIV_PKT_SUBKEY_SEL(SUBKEY_SEL_KEY2);
		else if (type == SE_KEY_TABLE_TYPE_XTS_KEY1 ||
				type == SE_KEY_TABLE_TYPE_XTS_KEY1_IN_MEM)
			pkt = SE_CRYPTO_KEYIV_PKT_SUBKEY_SEL(SUBKEY_SEL_KEY1);

		pkt |= (SE_KEYTABLE_SLOT(slot_num) | SE_KEYTABLE_QUAD(quad));

		for (j = 0; j < data_size; j += 4, data_len -= 4) {
			cpuvaddr[i++] = __nvhost_opcode_nonincr(
					opcode_addr +
					SE_AES_CRYPTO_KEYTABLE_ADDR_OFFSET, 1);

			val = (SE_KEYTABLE_PKT(pkt) | (j / 4));
			cpuvaddr[i++] = val;

			cpuvaddr[i++] = __nvhost_opcode_incr(
					opcode_addr +
					SE_AES_CRYPTO_KEYTABLE_DATA_OFFSET, 1);
			cpuvaddr[i++] = *pdata_buf++;
		}
		data_size = data_len;
		if ((type == SE_KEY_TABLE_TYPE_KEY) ||
		    (type == SE_KEY_TABLE_TYPE_XTS_KEY1) ||
		    (type == SE_KEY_TABLE_TYPE_KEY_IN_MEM) ||
		    (type == SE_KEY_TABLE_TYPE_XTS_KEY1_IN_MEM))
			quad = QUAD_KEYS_256;
		else if ((type == SE_KEY_TABLE_TYPE_XTS_KEY2) ||
			(type == SE_KEY_TABLE_TYPE_XTS_KEY2_IN_MEM))
			quad = QUAD_UPDTD_IV;

	} while (data_len);

	if ((type != SE_KEY_TABLE_TYPE_ORGIV) &&
	    (type != SE_KEY_TABLE_TYPE_UPDTDIV) &&
		(type != SE_KEY_TABLE_TYPE_KEY_IN_MEM) &&
		(type != SE_KEY_TABLE_TYPE_XTS_KEY1_IN_MEM) &&
		(type != SE_KEY_TABLE_TYPE_XTS_KEY2_IN_MEM)) {
		cpuvaddr[i++] = __nvhost_opcode_nonincr(
				opcode_addr + SE_AES_OPERATION_OFFSET, 1);
		cpuvaddr[i++] = SE_OPERATION_WRSTALL(WRSTALL_TRUE) |
				SE_OPERATION_OP(OP_DUMMY);
	}

	cmdbuf_num_words = i;
	se_dev->cmdbuf_cnt = i;

	if ((type != SE_KEY_TABLE_TYPE_ORGIV) &&
	    (type != SE_KEY_TABLE_TYPE_UPDTDIV) &&
		(type != SE_KEY_TABLE_TYPE_KEY_IN_MEM) &&
		(type != SE_KEY_TABLE_TYPE_XTS_KEY1_IN_MEM) &&
		(type != SE_KEY_TABLE_TYPE_XTS_KEY2_IN_MEM))
		err = tegra_se_channel_submit_gather(
			se_dev, cpuvaddr, iova, 0, cmdbuf_num_words, callback);

	return err;
}

static u32 tegra_se_get_crypto_config(struct tegra_se_dev *se_dev,
				      enum tegra_se_aes_op_mode mode,
				      bool encrypt, u8 slot_num,
				      u8 slot2_num, bool org_iv)
{
	u32 val = 0;
	unsigned long freq = 0;
	int err = 0;

	switch (mode) {
	case SE_AES_OP_MODE_XTS:
		if (encrypt) {
			val = SE_CRYPTO_INPUT_SEL(INPUT_MEMORY) |
				SE_CRYPTO_VCTRAM_SEL(VCTRAM_TWEAK) |
				SE_CRYPTO_XOR_POS(XOR_BOTH) |
				SE_CRYPTO_CORE_SEL(CORE_ENCRYPT);
		} else {
			val = SE_CRYPTO_INPUT_SEL(INPUT_MEMORY) |
				SE_CRYPTO_VCTRAM_SEL(VCTRAM_TWEAK) |
				SE_CRYPTO_XOR_POS(XOR_BOTH) |
				SE_CRYPTO_CORE_SEL(CORE_DECRYPT);
		}
		if (se_dev->chipdata->kac_type == SE_KAC_T23X)
			val |= SE_CRYPTO_KEY2_INDEX(slot2_num);
		freq = se_dev->chipdata->aes_freq;
		break;
	case SE_AES_OP_MODE_CMAC:
	case SE_AES_OP_MODE_CBC:
		if (encrypt) {
			if (se_dev->chipdata->kac_type == SE_KAC_T18X ||
			   (se_dev->chipdata->kac_type == SE_KAC_T23X &&
			    mode == SE_AES_OP_MODE_CBC)) {
				val = SE_CRYPTO_INPUT_SEL(INPUT_MEMORY) |
					SE_CRYPTO_VCTRAM_SEL(VCTRAM_AESOUT) |
					SE_CRYPTO_XOR_POS(XOR_TOP) |
					SE_CRYPTO_CORE_SEL(CORE_ENCRYPT);
			}
		} else {
			val = SE_CRYPTO_INPUT_SEL(INPUT_MEMORY) |
				SE_CRYPTO_VCTRAM_SEL(VCTRAM_PREVAHB) |
				SE_CRYPTO_XOR_POS(XOR_BOTTOM) |
				SE_CRYPTO_CORE_SEL(CORE_DECRYPT);
		}
		freq = se_dev->chipdata->aes_freq;
		break;
	case SE_AES_OP_MODE_CBC_MAC:
		val = SE_CRYPTO_XOR_POS(XOR_TOP) |
			SE_CRYPTO_INPUT_SEL(INPUT_MEMORY) |
			SE_CRYPTO_VCTRAM_SEL(VCTRAM_AESOUT) |
			SE_CRYPTO_HASH(HASH_ENABLE);

		if (se_dev->chipdata->kac_type == SE_KAC_T23X)
			val |= SE_CRYPTO_CORE_SEL(CORE_ENCRYPT);
		break;
	case SE_AES_OP_MODE_RNG_DRBG:
		val = SE_CRYPTO_INPUT_SEL(INPUT_RANDOM) |
			SE_CRYPTO_XOR_POS(XOR_BYPASS) |
			SE_CRYPTO_CORE_SEL(CORE_ENCRYPT);
		break;
	case SE_AES_OP_MODE_ECB:
		if (encrypt) {
			val = SE_CRYPTO_INPUT_SEL(INPUT_MEMORY) |
				SE_CRYPTO_XOR_POS(XOR_BYPASS) |
				SE_CRYPTO_CORE_SEL(CORE_ENCRYPT);
		} else {
			val = SE_CRYPTO_INPUT_SEL(INPUT_MEMORY) |
				SE_CRYPTO_XOR_POS(XOR_BYPASS) |
				SE_CRYPTO_CORE_SEL(CORE_DECRYPT);
		}
		freq = se_dev->chipdata->aes_freq;
		break;
	case SE_AES_OP_MODE_CTR:
		val = SE_CRYPTO_INPUT_SEL(INPUT_LNR_CTR) |
			SE_CRYPTO_VCTRAM_SEL(VCTRAM_MEMORY) |
			SE_CRYPTO_XOR_POS(XOR_BOTTOM) |
			SE_CRYPTO_CORE_SEL(CORE_ENCRYPT);
		freq = se_dev->chipdata->aes_freq;
		break;
	case SE_AES_OP_MODE_OFB:
		val = SE_CRYPTO_INPUT_SEL(INPUT_AESOUT) |
			SE_CRYPTO_VCTRAM_SEL(VCTRAM_MEMORY) |
			SE_CRYPTO_XOR_POS(XOR_BOTTOM) |
			SE_CRYPTO_CORE_SEL(CORE_ENCRYPT);
		freq = se_dev->chipdata->aes_freq;
		break;
	case SE_AES_OP_MODE_GCM:
		break;
	default:
		dev_warn(se_dev->dev, "Invalid operation mode\n");
		break;
	}

	if (mode == SE_AES_OP_MODE_CTR) {
		val |= SE_CRYPTO_HASH(HASH_DISABLE) |
			SE_CRYPTO_KEY_INDEX(slot_num) |
			SE_CRYPTO_CTR_CNTN(1);
		if (se_dev->chipdata->kac_type == SE_KAC_T23X)
			val |= SE_CRYPTO_IV_SEL(IV_REG);
	} else {
		val |= SE_CRYPTO_HASH(HASH_DISABLE) |
			SE_CRYPTO_KEY_INDEX(slot_num);
		if (se_dev->chipdata->kac_type == SE_KAC_T23X) {
			if (mode != SE_AES_OP_MODE_ECB &&
			    mode != SE_AES_OP_MODE_CMAC &&
			    mode != SE_AES_OP_MODE_GCM)
				val |= SE_CRYPTO_IV_SEL(IV_REG);
		} else {
			val |= (org_iv ? SE_CRYPTO_IV_SEL(IV_ORIGINAL) :
				SE_CRYPTO_IV_SEL(IV_UPDATED));
		}
	}

	/* Enable hash for CMAC if running on T18X. */
	if (mode == SE_AES_OP_MODE_CMAC &&
		se_dev->chipdata->kac_type == SE_KAC_T18X)
		val |= SE_CRYPTO_HASH(HASH_ENABLE);

	if (mode == SE_AES_OP_MODE_RNG_DRBG) {
		/* Make sure engine is powered ON*/
		err = nvhost_module_busy(se_dev->pdev);
		if (err < 0) {
			dev_err(se_dev->dev,
			"nvhost_module_busy failed for se with err: %d\n",
			err);

			/*
			 * Do not program force reseed if nvhost_module_busy
			 * failed. This can result in a crash as clocks might
			 * be disabled.
			 *
			 * Return val if unable to power on SE
			 */
			return val;
		}

		if (force_reseed_count <= 0) {
			se_writel(se_dev,
				  SE_RNG_CONFIG_MODE(DRBG_MODE_FORCE_RESEED) |
				  SE_RNG_CONFIG_SRC(DRBG_SRC_ENTROPY),
				  SE_RNG_CONFIG_REG_OFFSET);
		force_reseed_count = RNG_RESEED_INTERVAL;
		} else {
			se_writel(se_dev,
				  SE_RNG_CONFIG_MODE(DRBG_MODE_NORMAL) |
				  SE_RNG_CONFIG_SRC(DRBG_SRC_ENTROPY),
				  SE_RNG_CONFIG_REG_OFFSET);
		}
		--force_reseed_count;

		se_writel(se_dev, RNG_RESEED_INTERVAL,
			  SE_RNG_RESEED_INTERVAL_REG_OFFSET);

		/* Power off device after register access done */
		nvhost_module_idle(se_dev->pdev);
	}

	pr_debug("%s:%d crypto_config val = 0x%x\n", __func__, __LINE__, val);

	return val;
}

static int tegra_se_send_sha_data(struct tegra_se_dev *se_dev,
				  struct ahash_request *req,
				  struct tegra_se_sha_context *sha_ctx,
				  u32 count, bool last)
{
	int err = 0;
	u32 cmdbuf_num_words = 0, i = 0;
	u32 *cmdbuf_cpuvaddr = NULL;
	dma_addr_t cmdbuf_iova = 0;
	struct tegra_se_req_context *req_ctx = ahash_request_ctx(req);
	struct tegra_se_ll *src_ll = se_dev->src_ll;
	struct tegra_se_ll *dst_ll = se_dev->dst_ll;
	unsigned int total = count, val, index;
	u64 msg_len;

	err = tegra_se_get_free_cmdbuf(se_dev);
	if (err < 0) {
		dev_err(se_dev->dev, "Couldn't get free cmdbuf\n");
		goto index_out;
	}
	index = err;

	cmdbuf_cpuvaddr = se_dev->cmdbuf_addr_list[index].cmdbuf_addr;
	cmdbuf_iova = se_dev->cmdbuf_addr_list[index].iova;
	se_dev->cmdbuf_list_entry = index;

	while (total) {
		if (src_ll->data_len & SE_BUFF_SIZE_MASK) {
			atomic_set(&se_dev->cmdbuf_addr_list[index].free, 1);
			return -EINVAL;
		}

		if (total == count) {
			cmdbuf_cpuvaddr[i++] = nvhost_opcode_setpayload(8);
			cmdbuf_cpuvaddr[i++] = __nvhost_opcode_incr_w(
						se_dev->opcode_addr +
						SE_SHA_MSG_LENGTH_OFFSET);

			/* Program message length in next 4 four registers */
			if (sha_ctx->op_mode == SE_AES_OP_MODE_SHAKE128
			|| sha_ctx->op_mode == SE_AES_OP_MODE_SHAKE256) {
				/* Reduce 4 extra bits in padded 8 bits */
				msg_len = (count * 8) - 4;
			} else
				msg_len = (count * 8);
			cmdbuf_cpuvaddr[i++] =
					(sha_ctx->total_count * 8);
			cmdbuf_cpuvaddr[i++] = (u32)(msg_len >> 32);
			cmdbuf_cpuvaddr[i++] = 0;
			cmdbuf_cpuvaddr[i++] = 0;

			/* Program message left in next 4 four registers */

			/* If it is not last request, length of message left
			 * should be more than input buffer length.
			 */
			if (!last)
				cmdbuf_cpuvaddr[i++] =
					(u32)(msg_len + 8) & 0xFFFFFFFFULL;
			else
				cmdbuf_cpuvaddr[i++] =
					(u32)((msg_len) &  0xFFFFFFFFULL);
			cmdbuf_cpuvaddr[i++] = (u32)(msg_len >> 32);
			cmdbuf_cpuvaddr[i++] = 0;
			cmdbuf_cpuvaddr[i++] = 0;

			cmdbuf_cpuvaddr[i++] = nvhost_opcode_setpayload(6);
			cmdbuf_cpuvaddr[i++] = __nvhost_opcode_incr_w(
						se_dev->opcode_addr);

			cmdbuf_cpuvaddr[i++] = req_ctx->config;

			if (sha_ctx->is_first)
				cmdbuf_cpuvaddr[i++] =
					SE4_HW_INIT_HASH(HW_INIT_HASH_ENABLE);
			else
				cmdbuf_cpuvaddr[i++] =
					SE4_HW_INIT_HASH(HW_INIT_HASH_DISABLE);
		} else {
			cmdbuf_cpuvaddr[i++] = nvhost_opcode_setpayload(4);
			cmdbuf_cpuvaddr[i++] = __nvhost_opcode_incr_w(
						se_dev->opcode_addr +
						SE4_SHA_IN_ADDR_OFFSET);
		}
		cmdbuf_cpuvaddr[i++] = src_ll->addr;
		cmdbuf_cpuvaddr[i++] = (u32)(SE_ADDR_HI_MSB(MSB(src_ll->addr)) |
					SE_ADDR_HI_SZ(src_ll->data_len));
		cmdbuf_cpuvaddr[i++] = dst_ll->addr;
		cmdbuf_cpuvaddr[i++] = (u32)(SE_ADDR_HI_MSB(MSB(dst_ll->addr)) |
					SE_ADDR_HI_SZ(dst_ll->data_len));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		/* For SHAKE128/SHAKE256 program digest size */
		if (sha_ctx->op_mode == SE_AES_OP_MODE_SHAKE128
			|| sha_ctx->op_mode == SE_AES_OP_MODE_SHAKE256) {
			cmdbuf_cpuvaddr[i++] = nvhost_opcode_setpayload(1);
			cmdbuf_cpuvaddr[i++] = __nvhost_opcode_nonincr_w(
						se_dev->opcode_addr +
						SE_SHA_HASH_LENGTH);
			cmdbuf_cpuvaddr[i++] = (req->dst_size * 8) << 2;
		}
#endif
		cmdbuf_cpuvaddr[i++] = nvhost_opcode_setpayload(1);
		cmdbuf_cpuvaddr[i++] = __nvhost_opcode_nonincr_w(
						se_dev->opcode_addr +
						SE_SHA_OPERATION_OFFSET);

		val = SE_OPERATION_WRSTALL(WRSTALL_TRUE);
		if (total == count) {
			if (total == src_ll->data_len)
				val |= SE_OPERATION_LASTBUF(LASTBUF_TRUE) |
					SE_OPERATION_OP(OP_START);
			else
				val |= SE_OPERATION_LASTBUF(LASTBUF_FALSE) |
					SE_OPERATION_OP(OP_START);
		} else {
			if (total == src_ll->data_len)
				val |= SE_OPERATION_LASTBUF(LASTBUF_TRUE) |
					SE_OPERATION_OP(OP_RESTART_IN);
			else
				val |= SE_OPERATION_LASTBUF(LASTBUF_FALSE) |
					SE_OPERATION_OP(OP_RESTART_IN);
		}
		cmdbuf_cpuvaddr[i++] = val;
		total -= src_ll->data_len;
		src_ll++;
	}

	cmdbuf_num_words = i;

	err = tegra_se_channel_submit_gather(se_dev, cmdbuf_cpuvaddr,
					     cmdbuf_iova, 0, cmdbuf_num_words,
					     SHA_CB);
	if (err) {
		dev_err(se_dev->dev, "Channel submission fail err = %d\n", err);
		atomic_set(&se_dev->cmdbuf_addr_list[index].free, 1);
	}

index_out:
	return err;
}

static int tegra_se_read_cmac_result(struct tegra_se_dev *se_dev, u8 *pdata,
				      u32 nbytes, bool swap32)
{
	u32 *result = (u32 *)pdata;
	u32 i;
	int err = 0;

	/* Make SE engine is powered ON */
	err = nvhost_module_busy(se_dev->pdev);
	if (err < 0) {
		dev_err(se_dev->dev,
			"nvhost_module_busy failed for se with err: %d\n",
			err);
		return err;
	}

	for (i = 0; i < nbytes / 4; i++) {
		if (se_dev->chipdata->kac_type == SE_KAC_T23X) {
			result[i] = se_readl(se_dev,
					     se_dev->opcode_addr +
					     T234_SE_CMAC_RESULT_REG_OFFSET +
					     (i * sizeof(u32)));
		} else  {
			result[i] = se_readl(se_dev, SE_CMAC_RESULT_REG_OFFSET +
					     (i * sizeof(u32)));
		}
		if (swap32)
			result[i] = be32_to_cpu((__force __be32)result[i]);
	}

	nvhost_module_idle(se_dev->pdev);

	return 0;
}

static int tegra_se_clear_cmac_result(struct tegra_se_dev *se_dev, u32 nbytes)
{
	u32 i;
	int err = 0;

	err = nvhost_module_busy(se_dev->pdev);
	if (err < 0) {
		dev_err(se_dev->dev,
			"nvhost_module_busy failed for se with err: %d\n",
			err);
		return err;
	}

	for (i = 0; i < nbytes / 4; i++) {
		if (se_dev->chipdata->kac_type == SE_KAC_T23X) {
			se_writel(se_dev, 0x0, se_dev->opcode_addr +
				  T234_SE_CMAC_RESULT_REG_OFFSET +
				  (i * sizeof(u32)));
		} else {
			se_writel(se_dev, 0x0,
				  SE_CMAC_RESULT_REG_OFFSET +
				  (i * sizeof(u32)));
		}
	}

	nvhost_module_idle(se_dev->pdev);

	return 0;
}

static void tegra_se_send_data(struct tegra_se_dev *se_dev,
			       struct tegra_se_req_context *req_ctx,
			       struct skcipher_request *req, u32 nbytes,
			       unsigned int opcode_addr, u32 *cpuvaddr)
{
	u32 cmdbuf_num_words = 0, i = 0;
	u32 total, val, rbits;
	unsigned int restart_op;
	struct tegra_se_ll *src_ll;
	struct tegra_se_ll *dst_ll;

	if (req) {
		src_ll = se_dev->aes_src_ll;
		dst_ll = se_dev->aes_dst_ll;
		src_ll->addr = se_dev->aes_cur_addr;
		dst_ll->addr = se_dev->aes_cur_addr;
		src_ll->data_len = req->cryptlen;
		dst_ll->data_len = req->cryptlen;
	} else {
		src_ll = se_dev->src_ll;
		dst_ll = se_dev->dst_ll;
	}

	i = se_dev->cmdbuf_cnt;
	total = nbytes;

	/* Create Gather Buffer Command */
	while (total) {
		if (total == nbytes) {
			cpuvaddr[i++] = __nvhost_opcode_nonincr(
					opcode_addr +
					SE_AES_CRYPTO_LAST_BLOCK_OFFSET, 1);
			val = ((nbytes / TEGRA_SE_AES_BLOCK_SIZE)) - 1;

			if (req_ctx->op_mode == SE_AES_OP_MODE_CMAC &&
			    se_dev->chipdata->kac_type == SE_KAC_T23X) {
				rbits = (nbytes % TEGRA_SE_AES_BLOCK_SIZE) * 8;
				if (rbits) {
					val++;
					val |=
					SE_LAST_BLOCK_RESIDUAL_BITS(rbits);
				}
			}

			cpuvaddr[i++] = val;
			cpuvaddr[i++] = __nvhost_opcode_incr(opcode_addr, 6);
			cpuvaddr[i++] = req_ctx->config;
			cpuvaddr[i++] = req_ctx->crypto_config;
		} else {
			cpuvaddr[i++] = __nvhost_opcode_incr(
					opcode_addr + SE_AES_IN_ADDR_OFFSET, 4);
		}

		cpuvaddr[i++] = (u32)(src_ll->addr);
		cpuvaddr[i++] = (u32)(SE_ADDR_HI_MSB(MSB(src_ll->addr)) |
				SE_ADDR_HI_SZ(src_ll->data_len));
		cpuvaddr[i++] = (u32)(dst_ll->addr);
		cpuvaddr[i++] = (u32)(SE_ADDR_HI_MSB(MSB(dst_ll->addr)) |
				SE_ADDR_HI_SZ(dst_ll->data_len));

		if (req_ctx->op_mode == SE_AES_OP_MODE_CMAC ||
			req_ctx->op_mode == SE_AES_OP_MODE_CBC_MAC)
			restart_op = OP_RESTART_IN;
		else if (req_ctx->op_mode == SE_AES_OP_MODE_RNG_DRBG)
			restart_op = OP_RESTART_OUT;
		else
			restart_op = OP_RESTART_INOUT;

		cpuvaddr[i++] = __nvhost_opcode_nonincr(
				opcode_addr + SE_AES_OPERATION_OFFSET, 1);

		val = SE_OPERATION_WRSTALL(WRSTALL_TRUE);
		if (total == nbytes) {
			if (total == src_ll->data_len) {
				val |= SE_OPERATION_LASTBUF(LASTBUF_TRUE) |
					SE_OPERATION_OP(OP_START);

				if ((req_ctx->op_mode == SE_AES_OP_MODE_CMAC ||
				 req_ctx->op_mode == SE_AES_OP_MODE_CBC_MAC) &&
				 se_dev->chipdata->kac_type == SE_KAC_T23X)
					val |= SE_OPERATION_FINAL(FINAL_TRUE);
			} else {
				val |= SE_OPERATION_LASTBUF(LASTBUF_FALSE) |
					SE_OPERATION_OP(OP_START);
			}
		} else {
			if (total == src_ll->data_len) {
				val |= SE_OPERATION_LASTBUF(LASTBUF_TRUE) |
					SE_OPERATION_OP(restart_op);

				if ((req_ctx->op_mode == SE_AES_OP_MODE_CMAC ||
				 req_ctx->op_mode == SE_AES_OP_MODE_CBC_MAC) &&
				 se_dev->chipdata->kac_type == SE_KAC_T23X)
					val |= SE_OPERATION_FINAL(FINAL_TRUE);
			} else {
				val |= SE_OPERATION_LASTBUF(LASTBUF_FALSE) |
					SE_OPERATION_OP(restart_op);
			}
		}
		cpuvaddr[i++] = val;
		total -= src_ll->data_len;
		src_ll++;
		dst_ll++;
	}

	cmdbuf_num_words = i;
	se_dev->cmdbuf_cnt = i;
	if (req)
		se_dev->aes_cur_addr += req->cryptlen;
}

static void tegra_se_send_gcm_data(struct tegra_se_dev *se_dev,
			       struct tegra_se_req_context *req_ctx,
			       u32 nbytes, unsigned int opcode_addr,
			       u32 *cpuvaddr,
			       enum tegra_se_aes_gcm_mode sub_mode)
{
	u32 cmdbuf_num_words = 0, i = 0;
	u32 total, val, rbits;
	struct tegra_se_ll *src_ll;
	struct tegra_se_ll *dst_ll;
	unsigned int restart_op;

	src_ll = se_dev->src_ll;
	dst_ll = se_dev->dst_ll;

	i = se_dev->cmdbuf_cnt;
	total = nbytes;

	/* Create Gather Buffer Commands in multiple buffer */
	/* (not multiple task) format. */

	/* Program LAST_BLOCK */
	cpuvaddr[i++] = __nvhost_opcode_nonincr(
			opcode_addr +
			SE_AES_CRYPTO_LAST_BLOCK_OFFSET, 1);
	val = ((nbytes / TEGRA_SE_AES_BLOCK_SIZE)) - 1;
	rbits = (nbytes % TEGRA_SE_AES_BLOCK_SIZE) * 8;
	if (rbits) {
		val++;
		val |= SE_LAST_BLOCK_RESIDUAL_BITS(rbits);
	}
	cpuvaddr[i++] = val;

	/* Program config and crypto config */
	cpuvaddr[i++] = __nvhost_opcode_incr(opcode_addr, 2);
	cpuvaddr[i++] = req_ctx->config;
	cpuvaddr[i++] = req_ctx->crypto_config;

	while (total) {
		/* Program IN and OUT addresses */
		if (sub_mode != SE_AES_GMAC) {
			cpuvaddr[i++] = __nvhost_opcode_incr(
					opcode_addr + SE_AES_IN_ADDR_OFFSET, 4);
		} else {
			cpuvaddr[i++] = __nvhost_opcode_incr(
					opcode_addr + SE_AES_IN_ADDR_OFFSET, 2);
		}
		cpuvaddr[i++] = (u32)(src_ll->addr);
		cpuvaddr[i++] = (u32)(SE_ADDR_HI_MSB(MSB(src_ll->addr)) |
				SE_ADDR_HI_SZ(src_ll->data_len));
		if (sub_mode != SE_AES_GMAC) {
			cpuvaddr[i++] = (u32)(dst_ll->addr);
			cpuvaddr[i++] = (u32)(SE_ADDR_HI_MSB(MSB(dst_ll->addr))
					| SE_ADDR_HI_SZ(dst_ll->data_len));
		}

		/* Program operation register */
		if (sub_mode == SE_AES_GCM_ENC || sub_mode == SE_AES_GCM_DEC)
			restart_op = OP_RESTART_INOUT;
		else
			restart_op = OP_RESTART_IN;

		cpuvaddr[i++] = __nvhost_opcode_nonincr(
				opcode_addr + SE_AES_OPERATION_OFFSET, 1);
		val = SE_OPERATION_WRSTALL(WRSTALL_TRUE);
		if (req_ctx->init != true) {/* GMAC not operated */
			val |= SE_OPERATION_INIT(INIT_TRUE);
			req_ctx->init = true;
		}
		val |= SE_OPERATION_FINAL(FINAL_TRUE);
		if (total == nbytes) {
			if (total == src_ll->data_len) {
				val |= SE_OPERATION_LASTBUF(LASTBUF_TRUE) |
					SE_OPERATION_OP(OP_START);
			} else {
				val |= SE_OPERATION_LASTBUF(LASTBUF_FALSE) |
					SE_OPERATION_OP(OP_START);
			}
		} else {
			if (total == src_ll->data_len) {
				val |= SE_OPERATION_LASTBUF(LASTBUF_TRUE) |
					SE_OPERATION_OP(restart_op);
			} else {
				val |= SE_OPERATION_LASTBUF(LASTBUF_FALSE) |
					SE_OPERATION_OP(restart_op);
			}
		}
		cpuvaddr[i++] = val;
		total -= src_ll->data_len;
		src_ll++;
		dst_ll++;
	}

	cmdbuf_num_words = i;
	se_dev->cmdbuf_cnt = i;
}

static int tegra_map_sg(struct device *dev, struct scatterlist *sg,
			unsigned int nents, enum dma_data_direction dir,
			struct tegra_se_ll *se_ll, u32 total)
{
	u32 total_loop = 0;
	int ret = 0;

	total_loop = total;
	while (sg && (total_loop > 0)) {
		ret = dma_map_sg(dev, sg, nents, dir);
		if (!ret) {
			dev_err(dev, "dma_map_sg  error\n");
			return ret;
		}
		se_ll->addr = sg_dma_address(sg);
		se_ll->data_len = min((size_t)sg->length, (size_t)total_loop);
		total_loop -= min((size_t)sg->length, (size_t)total_loop);
		sg = sg_next(sg);
		se_ll++;
	}

	return ret;
}

static int tegra_se_setup_ablk_req(struct tegra_se_dev *se_dev)
{
	struct skcipher_request *req;
	void *buf;
	int i, ret = 0;
	u32 num_sgs;
	unsigned int index = 0;

	if (unlikely(se_dev->dynamic_mem)) {
		if (se_dev->ioc)
			se_dev->aes_buf = dma_alloc_coherent(
					se_dev->dev, se_dev->gather_buf_sz,
					&se_dev->aes_buf_addr, GFP_KERNEL);
		else
			se_dev->aes_buf = kmalloc(se_dev->gather_buf_sz,
						  GFP_KERNEL);
		if (!se_dev->aes_buf)
			return -ENOMEM;
		buf = se_dev->aes_buf;
	} else {
		index = se_dev->aesbuf_entry + 1;
		for (i = 0; i < SE_MAX_AESBUF_TIMEOUT; i++, index++) {
			index = index % SE_MAX_AESBUF_ALLOC;
			if (atomic_read(&se_dev->aes_buf_stat[index])) {
				se_dev->aesbuf_entry = index;
				atomic_set(&se_dev->aes_buf_stat[index], 0);
				break;
			}
			if (i % SE_MAX_AESBUF_ALLOC == 0)
				udelay(SE_WAIT_UDELAY);
		}

		if (i == SE_MAX_AESBUF_TIMEOUT) {
			pr_err("aes_buffer not available\n");
			return -ETIMEDOUT;
		}
		buf = se_dev->aes_bufs[index];
	}

	for (i = 0; i < se_dev->req_cnt; i++) {
		req = se_dev->reqs[i];

		num_sgs = tegra_se_count_sgs(req->src, req->cryptlen);

		if (num_sgs == 1)
			memcpy(buf, sg_virt(req->src), req->cryptlen);
		else
			sg_copy_to_buffer(req->src, num_sgs, buf, req->cryptlen);
		buf += req->cryptlen;
	}

	if (se_dev->ioc) {
		if (unlikely(se_dev->dynamic_mem))
			se_dev->aes_addr = se_dev->aes_buf_addr;
		else
			se_dev->aes_addr = se_dev->aes_buf_addrs[index];
	} else {
		if (unlikely(se_dev->dynamic_mem))
			sg_init_one(&se_dev->sg, se_dev->aes_buf,
				    se_dev->gather_buf_sz);
		else
			sg_init_one(&se_dev->sg, se_dev->aes_bufs[index],
				    se_dev->gather_buf_sz);

		ret = dma_map_sg(se_dev->dev, &se_dev->sg, 1,
				 DMA_BIDIRECTIONAL);
		if (!ret) {
			dev_err(se_dev->dev, "dma_map_sg  error\n");

			if (unlikely(se_dev->dynamic_mem))
				kfree(se_dev->aes_buf);
			else
				atomic_set(&se_dev->aes_buf_stat[index], 1);
			return ret;
		}

		se_dev->aes_addr = sg_dma_address(&se_dev->sg);
	}

	se_dev->aes_cur_addr = se_dev->aes_addr;

	return 0;
}

static int tegra_se_prepare_cmdbuf(struct tegra_se_dev *se_dev,
				   u32 *cpuvaddr, dma_addr_t iova)
{
	int i, ret = 0;
	struct tegra_se_aes_context *aes_ctx;
	struct skcipher_request *req;
	struct tegra_se_req_context *req_ctx;
	struct crypto_skcipher *tfm;
	u32 keylen;

	pr_debug("%s:%d req_cnt = %d\n", __func__, __LINE__, se_dev->req_cnt);

	for (i = 0; i < se_dev->req_cnt; i++) {
		req = se_dev->reqs[i];
		tfm = crypto_skcipher_reqtfm(req);
		aes_ctx = crypto_skcipher_ctx(tfm);
		/* Ensure there is valid slot info */
		if (!aes_ctx->slot) {
			dev_err(se_dev->dev, "Invalid AES Ctx Slot\n");
			return -EINVAL;
		}

		if (aes_ctx->is_key_in_mem) {
			if (strcmp(crypto_tfm_alg_name(&tfm->base),
					       "xts(aes)")) {
				ret = tegra_se_send_key_data(
					se_dev, aes_ctx->key, aes_ctx->keylen,
					aes_ctx->slot->slot_num,
					SE_KEY_TABLE_TYPE_KEY_IN_MEM,
					se_dev->opcode_addr, cpuvaddr, iova,
					AES_CB);
			} else {
				keylen = aes_ctx->keylen / 2;
				ret = tegra_se_send_key_data(se_dev,
					aes_ctx->key, keylen,
					aes_ctx->slot->slot_num,
					SE_KEY_TABLE_TYPE_XTS_KEY1_IN_MEM,
					se_dev->opcode_addr, cpuvaddr, iova,
					AES_CB);
				if (ret) {
					dev_err(se_dev->dev, "Error in setting Key\n");
					goto out;
				}

				ret = tegra_se_send_key_data(se_dev,
					aes_ctx->key + keylen, keylen,
					aes_ctx->slot->slot_num,
					SE_KEY_TABLE_TYPE_XTS_KEY2_IN_MEM,
					se_dev->opcode_addr, cpuvaddr, iova,
					AES_CB);
			}
			if (ret) {
				dev_err(se_dev->dev, "Error in setting Key\n");
				goto out;
			}
		}

		req_ctx = skcipher_request_ctx(req);

		if (req->iv) {
			if (req_ctx->op_mode == SE_AES_OP_MODE_CTR ||
			    req_ctx->op_mode == SE_AES_OP_MODE_XTS) {
				tegra_se_send_ctr_seed(se_dev, (u32 *)req->iv,
						       se_dev->opcode_addr,
						       cpuvaddr);
			} else {
				ret = tegra_se_send_key_data(
				se_dev, req->iv, TEGRA_SE_AES_IV_SIZE,
				aes_ctx->slot->slot_num,
				SE_KEY_TABLE_TYPE_UPDTDIV, se_dev->opcode_addr,
				cpuvaddr, iova, AES_CB);
			}
		}

		if (ret)
			return ret;

		req_ctx->config = tegra_se_get_config(se_dev, req_ctx->op_mode,
						      req_ctx->encrypt,
						      aes_ctx->keylen);

		if (aes_ctx->slot2 != NULL) {
			req_ctx->crypto_config = tegra_se_get_crypto_config(
						se_dev, req_ctx->op_mode,
						req_ctx->encrypt,
						aes_ctx->slot->slot_num,
						aes_ctx->slot2->slot_num,
						false);
		} else {
			req_ctx->crypto_config = tegra_se_get_crypto_config(
						se_dev, req_ctx->op_mode,
						req_ctx->encrypt,
						aes_ctx->slot->slot_num,
						0,
						false);
		}

		tegra_se_send_data(se_dev, req_ctx, req, req->cryptlen,
				   se_dev->opcode_addr, cpuvaddr);
	}

out:
	return ret;
}

static void tegra_se_process_new_req(struct tegra_se_dev *se_dev)
{
	struct skcipher_request *req;
	u32 *cpuvaddr = NULL;
	dma_addr_t iova = 0;
	unsigned int index = 0;
	int err = 0, i = 0;

	pr_debug("%s:%d start req_cnt = %d\n", __func__, __LINE__,
				se_dev->req_cnt);

	tegra_se_boost_cpu_freq(se_dev);

	for (i = 0; i < se_dev->req_cnt; i++) {
		req = se_dev->reqs[i];
		if (req->cryptlen != SE_STATIC_MEM_ALLOC_BUFSZ) {
			se_dev->dynamic_mem = true;
			break;
		}
	}

	err = tegra_se_setup_ablk_req(se_dev);
	if (err)
		goto mem_out;

	err = tegra_se_get_free_cmdbuf(se_dev);
	if (err < 0) {
		dev_err(se_dev->dev, "Couldn't get free cmdbuf\n");
		goto index_out;
	}

	index = err;

	cpuvaddr = se_dev->cmdbuf_addr_list[index].cmdbuf_addr;
	iova = se_dev->cmdbuf_addr_list[index].iova;
	se_dev->cmdbuf_list_entry = index;

	err = tegra_se_prepare_cmdbuf(se_dev, cpuvaddr, iova);
	if (err)
		goto cmdbuf_out;

	err = tegra_se_channel_submit_gather(se_dev, cpuvaddr, iova, 0,
					     se_dev->cmdbuf_cnt, AES_CB);
	if (err)
		goto cmdbuf_out;
	se_dev->dynamic_mem = false;

	pr_debug("%s:%d complete\n", __func__, __LINE__);

	return;
cmdbuf_out:
	atomic_set(&se_dev->cmdbuf_addr_list[index].free, 1);
index_out:
	dma_unmap_sg(se_dev->dev, &se_dev->sg, 1, DMA_BIDIRECTIONAL);
	kfree(se_dev->aes_buf);
mem_out:
	for (i = 0; i < se_dev->req_cnt; i++) {
		req = se_dev->reqs[i];
		req->base.complete(&req->base, err);
	}
	se_dev->req_cnt = 0;
	se_dev->gather_buf_sz = 0;
	se_dev->cmdbuf_cnt = 0;
	se_dev->dynamic_mem = false;
}

static void tegra_se_work_handler(struct work_struct *work)
{
	struct tegra_se_dev *se_dev = container_of(work, struct tegra_se_dev,
						   se_work);
	struct crypto_async_request *async_req = NULL;
	struct crypto_async_request *backlog = NULL;
	struct skcipher_request *req;
	bool process_requests;

	mutex_lock(&se_dev->mtx);
	do {
		process_requests = false;
		mutex_lock(&se_dev->lock);
		do {
			backlog = crypto_get_backlog(&se_dev->queue);
			async_req = crypto_dequeue_request(&se_dev->queue);
			if (!async_req)
				se_dev->work_q_busy = false;

			if (backlog) {
				backlog->complete(backlog, -EINPROGRESS);
				backlog = NULL;
			}

			if (async_req) {
				req = skcipher_request_cast(async_req);
				se_dev->reqs[se_dev->req_cnt] = req;
				se_dev->gather_buf_sz += req->cryptlen;
				se_dev->req_cnt++;
				process_requests = true;
			} else {
				break;
			}
		} while (se_dev->queue.qlen &&
			 (se_dev->req_cnt < SE_MAX_TASKS_PER_SUBMIT));
		mutex_unlock(&se_dev->lock);

		if (process_requests)
			tegra_se_process_new_req(se_dev);
	} while (se_dev->work_q_busy);
	mutex_unlock(&se_dev->mtx);
}

static int tegra_se_aes_queue_req(struct tegra_se_dev *se_dev,
				  struct skcipher_request *req)
{
	int err = 0;

	mutex_lock(&se_dev->lock);
	err = crypto_enqueue_request(&se_dev->queue, &req->base);

	if (!se_dev->work_q_busy) {
		se_dev->work_q_busy = true;
		mutex_unlock(&se_dev->lock);
		queue_work(se_dev->se_work_q, &se_dev->se_work);
	} else {
		mutex_unlock(&se_dev->lock);
	}

	return err;
}

static int tegra_se_aes_xts_encrypt(struct skcipher_request *req)
{
	struct tegra_se_req_context *req_ctx = skcipher_request_ctx(req);

	req_ctx->se_dev = se_devices[SE_AES];
	if (!req_ctx->se_dev) {
		pr_err("Device is NULL\n");
		return -ENODEV;
	}

	req_ctx->encrypt = true;
	req_ctx->op_mode = SE_AES_OP_MODE_XTS;

	return tegra_se_aes_queue_req(req_ctx->se_dev, req);
}

static int tegra_se_aes_xts_decrypt(struct skcipher_request *req)
{
	struct tegra_se_req_context *req_ctx = skcipher_request_ctx(req);

	req_ctx->se_dev = se_devices[SE_AES];
	if (!req_ctx->se_dev) {
		pr_err("Device is NULL\n");
		return -ENODEV;
	}

	req_ctx->encrypt = false;
	req_ctx->op_mode = SE_AES_OP_MODE_XTS;

	return tegra_se_aes_queue_req(req_ctx->se_dev, req);
}

static int tegra_se_aes_cbc_encrypt(struct skcipher_request *req)
{
	struct tegra_se_req_context *req_ctx = skcipher_request_ctx(req);

	req_ctx->se_dev = se_devices[SE_AES];
	req_ctx->encrypt = true;
	req_ctx->op_mode = SE_AES_OP_MODE_CBC;

	return tegra_se_aes_queue_req(req_ctx->se_dev, req);
}

static int tegra_se_aes_cbc_decrypt(struct skcipher_request *req)
{
	struct tegra_se_req_context *req_ctx = skcipher_request_ctx(req);

	req_ctx->se_dev = se_devices[SE_AES];
	req_ctx->encrypt = false;
	req_ctx->op_mode = SE_AES_OP_MODE_CBC;

	return tegra_se_aes_queue_req(req_ctx->se_dev, req);
}

static int tegra_se_aes_ecb_encrypt(struct skcipher_request *req)
{
	struct tegra_se_req_context *req_ctx = skcipher_request_ctx(req);

	req_ctx->se_dev = se_devices[SE_AES];
	req_ctx->encrypt = true;
	req_ctx->op_mode = SE_AES_OP_MODE_ECB;

	return tegra_se_aes_queue_req(req_ctx->se_dev, req);
}

static int tegra_se_aes_ecb_decrypt(struct skcipher_request *req)
{
	struct tegra_se_req_context *req_ctx = skcipher_request_ctx(req);

	req_ctx->se_dev = se_devices[SE_AES];
	req_ctx->encrypt = false;
	req_ctx->op_mode = SE_AES_OP_MODE_ECB;

	return tegra_se_aes_queue_req(req_ctx->se_dev, req);
}

static int tegra_se_aes_ctr_encrypt(struct skcipher_request *req)
{
	struct tegra_se_req_context *req_ctx = skcipher_request_ctx(req);

	req_ctx->se_dev = se_devices[SE_AES];
	req_ctx->encrypt = true;
	req_ctx->op_mode = SE_AES_OP_MODE_CTR;

	return tegra_se_aes_queue_req(req_ctx->se_dev, req);
}

static int tegra_se_aes_ctr_decrypt(struct skcipher_request *req)
{
	struct tegra_se_req_context *req_ctx = skcipher_request_ctx(req);

	req_ctx->se_dev = se_devices[SE_AES];
	req_ctx->encrypt = false;
	req_ctx->op_mode = SE_AES_OP_MODE_CTR;

	return tegra_se_aes_queue_req(req_ctx->se_dev, req);
}

static int tegra_se_aes_ofb_encrypt(struct skcipher_request *req)
{
	struct tegra_se_req_context *req_ctx = skcipher_request_ctx(req);

	req_ctx->se_dev = se_devices[SE_AES];
	req_ctx->encrypt = true;
	req_ctx->op_mode = SE_AES_OP_MODE_OFB;

	return tegra_se_aes_queue_req(req_ctx->se_dev, req);
}

static int tegra_se_aes_ofb_decrypt(struct skcipher_request *req)
{
	struct tegra_se_req_context *req_ctx = skcipher_request_ctx(req);

	req_ctx->se_dev = se_devices[SE_AES];
	req_ctx->encrypt = false;
	req_ctx->op_mode = SE_AES_OP_MODE_OFB;

	return tegra_se_aes_queue_req(req_ctx->se_dev, req);
}

static void tegra_se_init_aesbuf(struct tegra_se_dev *se_dev)
{
	int i;
	void *buf = se_dev->total_aes_buf;
	dma_addr_t buf_addr = se_dev->total_aes_buf_addr;

	for (i = 0; i < SE_MAX_AESBUF_ALLOC; i++) {
		se_dev->aes_bufs[i] = buf + (i * SE_MAX_GATHER_BUF_SZ);
		if (se_dev->ioc)
			se_dev->aes_buf_addrs[i] = buf_addr +
						(i * SE_MAX_GATHER_BUF_SZ);
		atomic_set(&se_dev->aes_buf_stat[i], 1);
	}
}

static int tegra_se_aes_setkey(struct crypto_skcipher *tfm,
			       const u8 *key, u32 keylen)
{
	struct tegra_se_aes_context *ctx = crypto_tfm_ctx(&tfm->base);
	struct tegra_se_dev *se_dev;
	struct tegra_se_slot *pslot;
	u8 *pdata = (u8 *)key;
	int ret = 0;
	unsigned int index = 0;
	u32 *cpuvaddr = NULL;
	dma_addr_t iova = 0;

	se_dev = se_devices[SE_AES];

	if (!ctx || !se_dev) {
		pr_err("invalid context or dev");
		return -EINVAL;
	}
	ctx->se_dev = se_dev;

	if (((keylen & SE_KEY_LEN_MASK) != TEGRA_SE_KEY_128_SIZE) &&
	    ((keylen & SE_KEY_LEN_MASK) != TEGRA_SE_KEY_192_SIZE) &&
	    ((keylen & SE_KEY_LEN_MASK) != TEGRA_SE_KEY_256_SIZE) &&
	    ((keylen & SE_KEY_LEN_MASK) != TEGRA_SE_KEY_512_SIZE)) {
		dev_err(se_dev->dev, "invalid key size");
		return -EINVAL;
	}

	if ((keylen >> SE_MAGIC_PATTERN_OFFSET) == SE_STORE_KEY_IN_MEM) {
		ctx->is_key_in_mem = true;
		ctx->keylen = (keylen & SE_KEY_LEN_MASK);
		ctx->slot = &keymem_slot;
		memcpy(ctx->key, key, ctx->keylen);
		return 0;
	}
	ctx->is_key_in_mem = false;

	mutex_lock(&se_dev->mtx);
	if (key) {
		if (!ctx->slot ||
		    (ctx->slot &&
		     ctx->slot->slot_num == ssk_slot.slot_num)) {
			pslot = tegra_se_alloc_key_slot();
			if (!pslot) {
				dev_err(se_dev->dev, "no free key slot\n");
				mutex_unlock(&se_dev->mtx);
				return -ENOMEM;
			}
			ctx->slot = pslot;
			ctx->slot2 = NULL;
			if (!strcmp(crypto_tfm_alg_name(&tfm->base),
							"xts(aes)")) {
				if (se_dev->chipdata->kac_type == SE_KAC_T23X) {
					pslot = tegra_se_alloc_key_slot();
					if (!pslot) {
						dev_err(se_dev->dev, "no free key slot\n");
						mutex_unlock(&se_dev->mtx);
						return -ENOMEM;
					}
					ctx->slot2 = pslot;
				}
			}
		}
		ctx->keylen = keylen;
	} else if ((keylen >> SE_MAGIC_PATTERN_OFFSET) == SE_MAGIC_PATTERN) {
		ctx->slot = &pre_allocated_slot;
		spin_lock(&key_slot_lock);
		pre_allocated_slot.slot_num =
			((keylen & SE_SLOT_NUM_MASK) >> SE_SLOT_POSITION);
		spin_unlock(&key_slot_lock);
		ctx->keylen = (keylen & SE_KEY_LEN_MASK);
		goto out;
	} else {
		tegra_se_free_key_slot(ctx->slot);
		ctx->slot = &ssk_slot;
		ctx->keylen = AES_KEYSIZE_128;
		goto out;
	}

	ret = tegra_se_get_free_cmdbuf(se_dev);
	if (ret < 0) {
		dev_err(se_dev->dev, "Couldn't get free cmdbuf\n");
		goto keyslt_free;
	}

	index = ret;

	cpuvaddr = se_dev->cmdbuf_addr_list[index].cmdbuf_addr;
	iova = se_dev->cmdbuf_addr_list[index].iova;
	atomic_set(&se_dev->cmdbuf_addr_list[index].free, 0);
	se_dev->cmdbuf_list_entry = index;

	/* load the key */

	if (strcmp(crypto_tfm_alg_name(&tfm->base), "xts(aes)")) {
		ret = tegra_se_send_key_data(
			se_dev, pdata, keylen, ctx->slot->slot_num,
			SE_KEY_TABLE_TYPE_KEY, se_dev->opcode_addr, cpuvaddr,
			iova, AES_CB);
	} else {
		keylen = keylen / 2;
		ret = tegra_se_send_key_data(
			se_dev, pdata, keylen, ctx->slot->slot_num,
			SE_KEY_TABLE_TYPE_XTS_KEY1, se_dev->opcode_addr,
			cpuvaddr, iova, AES_CB);
		if (ret)
			goto keyslt_free;

		if (se_dev->chipdata->kac_type == SE_KAC_T23X) {
			ret = tegra_se_send_key_data(se_dev, pdata + keylen,
					     keylen, ctx->slot2->slot_num,
					     SE_KEY_TABLE_TYPE_XTS_KEY2,
					     se_dev->opcode_addr, cpuvaddr,
					     iova, AES_CB);
		} else {
			ret = tegra_se_send_key_data(se_dev, pdata + keylen,
					     keylen, ctx->slot->slot_num,
					     SE_KEY_TABLE_TYPE_XTS_KEY2,
					     se_dev->opcode_addr, cpuvaddr,
					     iova, AES_CB);
		}
	}
keyslt_free:
	if (ret) {
		tegra_se_free_key_slot(ctx->slot);
		tegra_se_free_key_slot(ctx->slot2);
	}
out:
	mutex_unlock(&se_dev->mtx);

	return ret;
}

static int tegra_se_aes_cra_init(struct crypto_skcipher *tfm)
{
	tfm->reqsize = sizeof(struct tegra_se_req_context);

	return 0;
}

static void tegra_se_aes_cra_exit(struct crypto_skcipher *tfm)
{
	struct tegra_se_aes_context *ctx = crypto_tfm_ctx(&tfm->base);

	tegra_se_free_key_slot(ctx->slot);
	tegra_se_free_key_slot(ctx->slot2);
	ctx->slot = NULL;
	ctx->slot2 = NULL;
}

static int tegra_se_rng_drbg_init(struct crypto_tfm *tfm)
{
	struct tegra_se_rng_context *rng_ctx = crypto_tfm_ctx(tfm);
	struct tegra_se_dev *se_dev;

	se_dev = se_devices[SE_DRBG];

	mutex_lock(&se_dev->mtx);

	rng_ctx->se_dev = se_dev;
	rng_ctx->dt_buf = dma_alloc_coherent(se_dev->dev, TEGRA_SE_RNG_DT_SIZE,
					     &rng_ctx->dt_buf_adr, GFP_KERNEL);
	if (!rng_ctx->dt_buf) {
		dev_err(se_dev->dev, "can not allocate rng dma buffer");
		mutex_unlock(&se_dev->mtx);
		return -ENOMEM;
	}

	rng_ctx->rng_buf = dma_alloc_coherent(
				rng_ctx->se_dev->dev, TEGRA_SE_RNG_DT_SIZE,
				&rng_ctx->rng_buf_adr, GFP_KERNEL);
	if (!rng_ctx->rng_buf) {
		dev_err(se_dev->dev, "can not allocate rng dma buffer");
		dma_free_coherent(rng_ctx->se_dev->dev, TEGRA_SE_RNG_DT_SIZE,
				  rng_ctx->dt_buf, rng_ctx->dt_buf_adr);
		mutex_unlock(&se_dev->mtx);
		return -ENOMEM;
	}
	mutex_unlock(&se_dev->mtx);

	return 0;
}

static int tegra_se_rng_drbg_get_random(struct crypto_rng *tfm, const u8 *src,
					unsigned int slen, u8 *rdata,
					unsigned int dlen)
{
	struct tegra_se_rng_context *rng_ctx = crypto_rng_ctx(tfm);
	struct tegra_se_dev *se_dev = rng_ctx->se_dev;
	u8 *rdata_addr;
	int ret = 0, j;
	unsigned int num_blocks, data_len = 0;
	struct tegra_se_req_context *req_ctx =
		devm_kzalloc(se_dev->dev,
			     sizeof(struct tegra_se_req_context), GFP_KERNEL);
	if (!req_ctx)
		return -ENOMEM;

	num_blocks = (dlen / TEGRA_SE_RNG_DT_SIZE);
	data_len = (dlen % TEGRA_SE_RNG_DT_SIZE);
	if (data_len == 0)
		num_blocks = num_blocks - 1;

	mutex_lock(&se_dev->mtx);
	req_ctx->op_mode = SE_AES_OP_MODE_RNG_DRBG;

	se_dev->src_ll = (struct tegra_se_ll *)(se_dev->src_ll_buf);
	se_dev->dst_ll = (struct tegra_se_ll *)(se_dev->dst_ll_buf);

	req_ctx->config = tegra_se_get_config(se_dev, req_ctx->op_mode, true,
					      TEGRA_SE_KEY_128_SIZE);

	if (se_dev->chipdata->kac_type != SE_KAC_T23X)
		req_ctx->crypto_config = tegra_se_get_crypto_config(se_dev,
							    req_ctx->op_mode,
							    true, 0, 0, true);
	for (j = 0; j <= num_blocks; j++) {
		se_dev->src_ll->addr = rng_ctx->dt_buf_adr;
		se_dev->src_ll->data_len = TEGRA_SE_RNG_DT_SIZE;
		se_dev->dst_ll->addr = rng_ctx->rng_buf_adr;
		se_dev->dst_ll->data_len = TEGRA_SE_RNG_DT_SIZE;

		tegra_se_send_data(se_dev, req_ctx, NULL, TEGRA_SE_RNG_DT_SIZE,
				   se_dev->opcode_addr,
				   se_dev->aes_cmdbuf_cpuvaddr);
		ret = tegra_se_channel_submit_gather(
				se_dev, se_dev->aes_cmdbuf_cpuvaddr,
				se_dev->aes_cmdbuf_iova, 0, se_dev->cmdbuf_cnt,
				NONE);
		if (ret)
			break;

		rdata_addr = (rdata + (j * TEGRA_SE_RNG_DT_SIZE));

		if (data_len && num_blocks == j)
			memcpy(rdata_addr, rng_ctx->rng_buf, data_len);
		else
			memcpy(rdata_addr, rng_ctx->rng_buf,
			       TEGRA_SE_RNG_DT_SIZE);
	}

	if (!ret)
		ret = dlen;

	mutex_unlock(&se_dev->mtx);
	devm_kfree(se_dev->dev, req_ctx);

	return ret;
}

static int tegra_se_rng_drbg_reset(struct crypto_rng *tfm, const u8 *seed,
				   unsigned int slen)
{
	return 0;
}

static void tegra_se_rng_drbg_exit(struct crypto_tfm *tfm)
{
	struct tegra_se_rng_context *rng_ctx = crypto_tfm_ctx(tfm);

	if (rng_ctx->dt_buf)
		dma_free_coherent(rng_ctx->se_dev->dev, TEGRA_SE_RNG_DT_SIZE,
				  rng_ctx->dt_buf, rng_ctx->dt_buf_adr);

	if (rng_ctx->rng_buf)
		dma_free_coherent(rng_ctx->se_dev->dev, TEGRA_SE_RNG_DT_SIZE,
				  rng_ctx->rng_buf, rng_ctx->rng_buf_adr);

	rng_ctx->se_dev = NULL;
}

static void tegra_se_sha_copy_residual_data(
		struct ahash_request *req, struct tegra_se_sha_context *sha_ctx,
		u32 bytes_to_copy)
{
	struct sg_mapping_iter miter;
	unsigned int sg_flags, total = 0;
	u32 num_sgs, last_block_bytes = bytes_to_copy;
	unsigned long flags;
	struct scatterlist *src_sg;
	u8 *temp_buffer = NULL;

	src_sg = req->src;
	num_sgs = tegra_se_count_sgs(req->src, req->nbytes);
	sg_flags = SG_MITER_ATOMIC | SG_MITER_FROM_SG;
	sg_miter_start(&miter, req->src, num_sgs, sg_flags);
	local_irq_save(flags);

	temp_buffer = sha_ctx->sha_buf[0];
	while (sg_miter_next(&miter) && total < req->nbytes) {
		unsigned int len;

		len = min(miter.length, (size_t)(req->nbytes - total));
		if ((req->nbytes - (total + len)) <= last_block_bytes) {
			bytes_to_copy = last_block_bytes -
					(req->nbytes - (total + len));
			memcpy(temp_buffer, miter.addr + (len - bytes_to_copy),
			       bytes_to_copy);
			last_block_bytes -= bytes_to_copy;
			temp_buffer += bytes_to_copy;
		}
		total += len;
	}
	sg_miter_stop(&miter);
	local_irq_restore(flags);
}

static int tegra_se_sha_process_buf(struct ahash_request *req, bool is_last,
				    bool process_cur_req)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct tegra_se_sha_context *sha_ctx = crypto_ahash_ctx(tfm);
	struct tegra_se_req_context *req_ctx = ahash_request_ctx(req);
	struct tegra_se_dev *se_dev = se_devices[SE_SHA];
	struct scatterlist *src_sg = req->src;
	struct tegra_se_ll *src_ll;
	struct tegra_se_ll *dst_ll;
	u32 current_total = 0, num_sgs, bytes_process_in_req = 0, num_blks;
	int err = 0, dst_len = crypto_ahash_digestsize(tfm);

	pr_debug("%s:%d process sha buffer\n", __func__, __LINE__);

	num_sgs = tegra_se_count_sgs(req->src, req->nbytes);
	if (num_sgs > SE_MAX_SRC_SG_COUNT) {
		dev_err(se_dev->dev, "num of SG buffers are more\n");
		return -ENOTSUPP;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	/* For SHAKE128/SHAKE256, digest size can vary */
	if (sha_ctx->op_mode == SE_AES_OP_MODE_SHAKE128
			|| sha_ctx->op_mode == SE_AES_OP_MODE_SHAKE256) {
		dst_len = req->dst_size;
		if (dst_len == 0) {
			req->base.complete(&req->base, 0);
			return 0;
		}
	}
#endif

	req_ctx->hash_result = devm_kzalloc(se_dev->dev, dst_len, GFP_KERNEL);
	if (!req_ctx->hash_result)
		return -ENOMEM;

	sg_init_one(&se_dev->sg, req_ctx->hash_result, dst_len);

	se_dev->sha_last = is_last;

	if (is_last) {
		/* Prepare buf for residual and current data */
		/* Fill sgs entries */
		se_dev->src_ll = (struct tegra_se_ll *)(se_dev->src_ll_buf);
		src_ll = se_dev->src_ll;
		se_dev->dst_ll = (struct tegra_se_ll *)(se_dev->dst_ll_buf);
		dst_ll = se_dev->dst_ll;

		if (sha_ctx->residual_bytes) {
			src_ll->addr = sha_ctx->sha_buf_addr[0];
			src_ll->data_len = sha_ctx->residual_bytes;
			src_ll++;
		}

		if (process_cur_req) {
			bytes_process_in_req = req->nbytes;
			err = tegra_map_sg(se_dev->dev, src_sg, 1,
					   DMA_TO_DEVICE, src_ll,
					   bytes_process_in_req);
			if (!err)
				return -EINVAL;

			current_total = req->nbytes + sha_ctx->residual_bytes;
			sha_ctx->total_count += current_total;

			err = tegra_map_sg(se_dev->dev, &se_dev->sg, 1,
					   DMA_FROM_DEVICE, dst_ll,
					   dst_len);
			if (!err)
				return -EINVAL;

			se_dev->src_bytes_mapped = bytes_process_in_req;
			se_dev->dst_bytes_mapped = dst_len;
			se_dev->sha_src_mapped = true;
			se_dev->sha_dst_mapped = true;
		} else {
			current_total = sha_ctx->residual_bytes;
			sha_ctx->total_count += current_total;
			if (!current_total) {
				req->base.complete(&req->base, 0);
				return 0;
			}
			err = tegra_map_sg(se_dev->dev, &se_dev->sg, 1,
					   DMA_FROM_DEVICE, dst_ll,
					   dst_len);
			if (!err)
				return -EINVAL;

			se_dev->dst_bytes_mapped = dst_len;
			se_dev->sha_dst_mapped = true;
		}

		/* Pad last byte to be 0xff for SHAKE128/256 */
		if (sha_ctx->op_mode == SE_AES_OP_MODE_SHAKE128
			|| sha_ctx->op_mode == SE_AES_OP_MODE_SHAKE256) {
			while (num_sgs && process_cur_req) {
				src_ll++;
				num_sgs--;
			}
			sha_ctx->sha_buf[1][0] = 0xff;
			src_ll->addr = sha_ctx->sha_buf_addr[1];
			src_ll->data_len = 1;
			src_ll++;
			current_total += 1;
		}
	} else {
		current_total = req->nbytes + sha_ctx->residual_bytes;
		num_blks = current_total / sha_ctx->blk_size;

		/* If total bytes is less than or equal to one blk,
		 * copy to residual and return.
		 */
		if (num_blks <= 1) {
			sg_copy_to_buffer(req->src, num_sgs,
					  sha_ctx->sha_buf[0] +
					  sha_ctx->residual_bytes, req->nbytes);
			sha_ctx->residual_bytes += req->nbytes;
			req->base.complete(&req->base, 0);
			return 0;
		}

		/* Number of bytes to be processed from given request buffers */
		bytes_process_in_req = (num_blks * sha_ctx->blk_size) -
					sha_ctx->residual_bytes;
		sha_ctx->total_count += bytes_process_in_req;

		/* Fill sgs entries */
		/* If residual bytes are present copy it to second buffer */
		if (sha_ctx->residual_bytes)
			memcpy(sha_ctx->sha_buf[1], sha_ctx->sha_buf[0],
			       sha_ctx->residual_bytes);

		sha_ctx->total_count += sha_ctx->residual_bytes;

		se_dev->src_ll = (struct tegra_se_ll *)(se_dev->src_ll_buf);
		src_ll = se_dev->src_ll;
		if (sha_ctx->residual_bytes) {
			src_ll->addr = sha_ctx->sha_buf_addr[1];
			src_ll->data_len = sha_ctx->residual_bytes;
			src_ll++;
		}

		se_dev->dst_ll = (struct tegra_se_ll *)(se_dev->dst_ll_buf);
		dst_ll = se_dev->dst_ll;

		/* Map required bytes to process in given request */
		err = tegra_map_sg(se_dev->dev, src_sg, 1, DMA_TO_DEVICE,
				   src_ll, bytes_process_in_req);
		if (!err)
			return -EINVAL;

		/* Copy residual data */
		sha_ctx->residual_bytes = current_total - (num_blks *
							   sha_ctx->blk_size);
		tegra_se_sha_copy_residual_data(req, sha_ctx,
						sha_ctx->residual_bytes);

		/* Total bytes to be processed */
		current_total = (num_blks * sha_ctx->blk_size);

		err = tegra_map_sg(se_dev->dev, &se_dev->sg, 1, DMA_FROM_DEVICE,
				   dst_ll, dst_len);
		if (!err)
			return -EINVAL;

		se_dev->src_bytes_mapped = bytes_process_in_req;
		se_dev->dst_bytes_mapped = dst_len;
		se_dev->sha_src_mapped = true;
		se_dev->sha_dst_mapped = true;
	}

	req_ctx->config = tegra_se_get_config(se_dev, sha_ctx->op_mode,
					      false, is_last);
	err = tegra_se_send_sha_data(se_dev, req, sha_ctx,
				     current_total, is_last);
	if (err) {
		if (se_dev->sha_src_mapped)
			tegra_unmap_sg(se_dev->dev, src_sg, DMA_TO_DEVICE,
				       bytes_process_in_req);
		if (se_dev->sha_dst_mapped)
			tegra_unmap_sg(se_dev->dev, &se_dev->sg,
				       DMA_FROM_DEVICE, dst_len);
		return err;
	}
	sha_ctx->is_first = false;

	pr_debug("%s:%d process sha buffer complete\n", __func__, __LINE__);

	return 0;
}

static int tegra_se_sha_op(struct ahash_request *req, bool is_last,
			   bool process_cur_req)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct tegra_se_sha_context *sha_ctx = crypto_ahash_ctx(tfm);
	struct tegra_se_dev *se_dev = se_devices[SE_SHA];
	u32 mode;
	int ret;

	struct tegra_se_sha_zero_length_vector zero_vec[] = {
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
			.size = SHA3_224_DIGEST_SIZE,
			.digest = "\x6b\x4e\x03\x42\x36\x67\xdb\xb7"
				  "\x3b\x6e\x15\x45\x4f\x0e\xb1\xab"
				  "\xd4\x59\x7f\x9a\x1b\x07\x8e\x3f"
				  "\x5b\x5a\x6b\xc7",
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

	if (strcmp(crypto_ahash_alg_name(tfm), "sha1") == 0) {
		sha_ctx->op_mode = SE_AES_OP_MODE_SHA1;
	} else if (strcmp(crypto_ahash_alg_name(tfm), "sha224") == 0) {
		sha_ctx->op_mode = SE_AES_OP_MODE_SHA224;
	} else if (strcmp(crypto_ahash_alg_name(tfm), "sha256") == 0) {
		sha_ctx->op_mode = SE_AES_OP_MODE_SHA256;
	} else if (strcmp(crypto_ahash_alg_name(tfm), "sha384") == 0) {
		sha_ctx->op_mode = SE_AES_OP_MODE_SHA384;
	} else if (strcmp(crypto_ahash_alg_name(tfm), "sha512") == 0) {
		sha_ctx->op_mode = SE_AES_OP_MODE_SHA512;
	} else if (strcmp(crypto_ahash_alg_name(tfm), "sha3-224") == 0) {
		sha_ctx->op_mode = SE_AES_OP_MODE_SHA3_224;
	} else if (strcmp(crypto_ahash_alg_name(tfm), "sha3-256") == 0) {
		sha_ctx->op_mode = SE_AES_OP_MODE_SHA3_256;
	} else if (strcmp(crypto_ahash_alg_name(tfm), "sha3-384") == 0) {
		sha_ctx->op_mode = SE_AES_OP_MODE_SHA3_384;
	} else if (strcmp(crypto_ahash_alg_name(tfm), "sha3-512") == 0) {
		sha_ctx->op_mode = SE_AES_OP_MODE_SHA3_512;
	} else if (strcmp(crypto_ahash_alg_name(tfm), "shake128") == 0) {
		sha_ctx->op_mode = SE_AES_OP_MODE_SHAKE128;
	} else if (strcmp(crypto_ahash_alg_name(tfm), "shake256") == 0) {
		sha_ctx->op_mode = SE_AES_OP_MODE_SHAKE256;
	} else if (strcmp(crypto_ahash_alg_name(tfm), "hmac(sha224)") == 0) {
		sha_ctx->op_mode = SE_AES_OP_MODE_HMAC_SHA224;
	} else  if (strcmp(crypto_ahash_alg_name(tfm), "hmac(sha256)") == 0) {
		sha_ctx->op_mode = SE_AES_OP_MODE_HMAC_SHA256;
	} else  if (strcmp(crypto_ahash_alg_name(tfm), "hmac(sha384)") == 0) {
		sha_ctx->op_mode = SE_AES_OP_MODE_HMAC_SHA384;
	} else  if (strcmp(crypto_ahash_alg_name(tfm), "hmac(sha512)") == 0) {
		sha_ctx->op_mode = SE_AES_OP_MODE_HMAC_SHA512;
	} else {
		dev_err(se_dev->dev, "Invalid SHA digest size\n");
		return -EINVAL;
	}

	/* If the request length is zero, */
	if (!req->nbytes && !is_last) {
		if (sha_ctx->total_count) {
			req->base.complete(&req->base, 0);
			return 0;	/* allow empty packets */
		} else {	/* total_count equals zero */
			if (is_last) {
			/* If this is the last packet SW WAR for zero
			 * length SHA operation since SE HW can't accept
			 * zero length SHA operation.
			 */
				mode = sha_ctx->op_mode - SE_AES_OP_MODE_SHA1;
				if (mode < ARRAY_SIZE(zero_vec))
					memcpy(req->result, zero_vec[mode].digest,
						zero_vec[mode].size);

				req->base.complete(&req->base, 0);
				return 0;
			} else {
				req->base.complete(&req->base, 0);
				return 0;	/* allow empty first packet */
			}
		}
	}

	sha_ctx->is_final = false;
	if (is_last)
		sha_ctx->is_final = true;

	ret = tegra_se_sha_process_buf(req, is_last, process_cur_req);
	if (ret) {
		mutex_unlock(&se_dev->mtx);
		return ret;
	}

	return 0;
}

static int tegra_se_sha_init(struct ahash_request *req)
{
	struct tegra_se_req_context *req_ctx;
	struct crypto_ahash *tfm = NULL;
	struct tegra_se_sha_context *sha_ctx;
	struct tegra_se_dev *se_dev = se_devices[SE_SHA];

	pr_debug("%s:%d start\n", __func__, __LINE__);

	if (!req) {
		dev_err(se_dev->dev, "SHA request not valid\n");
		return -EINVAL;
	}

	tfm = crypto_ahash_reqtfm(req);
	if (!tfm) {
		dev_err(se_dev->dev, "SHA transform not valid\n");
		return -EINVAL;
	}

	sha_ctx = crypto_ahash_ctx(tfm);
	if (!sha_ctx) {
		dev_err(se_dev->dev, "SHA context not valid\n");
		return -EINVAL;
	}

	req_ctx = ahash_request_ctx(req);
	if (!req_ctx) {
		dev_err(se_dev->dev, "Request context not valid\n");
		return -EINVAL;
	}

	mutex_lock(&se_dev->mtx);

	sha_ctx->total_count = 0;
	sha_ctx->is_first = true;
	sha_ctx->blk_size = crypto_tfm_alg_blocksize(crypto_ahash_tfm(tfm));
	sha_ctx->residual_bytes = 0;
	mutex_unlock(&se_dev->mtx);

	pr_debug("%s:%d end\n", __func__, __LINE__);

	return 0;
}

static int tegra_se_sha_update(struct ahash_request *req)
{
	struct tegra_se_dev *se_dev = se_devices[SE_SHA];
	int ret = 0;

	pr_debug("%s:%d start\n", __func__, __LINE__);

	if (!req) {
		dev_err(se_dev->dev, "SHA request not valid\n");
		return -EINVAL;
	}
	mutex_lock(&se_dev->mtx);

	se_dev->sha_req = req;

	ret = tegra_se_sha_op(req, false, false);
	if (ret)
		dev_err(se_dev->dev, "tegra_se_sha_update failed - %d\n", ret);
	else
		ret = -EBUSY;

	mutex_unlock(&se_dev->mtx);

	pr_debug("%s:%d end\n", __func__, __LINE__);

	return ret;
}

static int tegra_se_sha_finup(struct ahash_request *req)
{
	struct tegra_se_dev *se_dev = se_devices[SE_SHA];
	int ret = 0;

	pr_debug("%s:%d start\n", __func__, __LINE__);

	if (!req) {
		dev_err(se_dev->dev, "SHA request not valid\n");
		return -EINVAL;
	}
	mutex_lock(&se_dev->mtx);

	se_dev->sha_req = req;

	ret = tegra_se_sha_op(req, true, true);
	if (ret)
		dev_err(se_dev->dev, "tegra_se_sha_finup failed - %d\n", ret);
	else
		ret = -EBUSY;

	mutex_unlock(&se_dev->mtx);

	pr_debug("%s:%d end\n", __func__, __LINE__);

	return ret;
}

static int tegra_se_sha_final(struct ahash_request *req)
{
	struct tegra_se_dev *se_dev = se_devices[SE_SHA];
	int ret = 0;

	pr_debug("%s:%d start\n", __func__, __LINE__);

	if (!req) {
		dev_err(se_dev->dev, "SHA request not valid\n");
		return -EINVAL;
	}
	mutex_lock(&se_dev->mtx);

	se_dev->sha_req = req;

	/* Do not process data in given request */
	ret = tegra_se_sha_op(req, true, false);
	if (ret)
		dev_err(se_dev->dev, "tegra_se_sha_final failed - %d\n", ret);
	else
		ret = -EBUSY;

	mutex_unlock(&se_dev->mtx);

	pr_debug("%s:%d end\n", __func__, __LINE__);

	return ret;
}

static int tegra_se_sha_digest(struct ahash_request *req)
{
	struct tegra_se_dev *se_dev = se_devices[SE_SHA];
	int ret = 0;

	pr_debug("%s:%d start\n", __func__, __LINE__);

	ret = tegra_se_sha_init(req);
	if (ret)
		return ret;
	mutex_lock(&se_dev->mtx);

	se_dev->sha_req = req;

	ret = tegra_se_sha_op(req, true, true);
	if (ret)
		dev_err(se_dev->dev, "tegra_se_sha_digest failed - %d\n", ret);
	else
		ret = -EBUSY;

	mutex_unlock(&se_dev->mtx);

	pr_debug("%s:%d end\n", __func__, __LINE__);

	return ret;
}

static int tegra_se_sha_export(struct ahash_request *req, void *out)
{
	return 0;
}

static int tegra_se_sha_import(struct ahash_request *req, const void *in)
{
	return 0;
}

static int tegra_se_sha_cra_init(struct crypto_tfm *tfm)
{
	struct tegra_se_sha_context *sha_ctx;
	struct tegra_se_dev *se_dev = se_devices[SE_SHA];

	crypto_ahash_set_reqsize(__crypto_ahash_cast(tfm),
			sizeof(struct tegra_se_sha_context));
	sha_ctx = crypto_tfm_ctx(tfm);
	if (!sha_ctx) {
		dev_err(se_dev->dev, "SHA context not valid\n");
		return -EINVAL;
	}

	mutex_lock(&se_dev->mtx);
	sha_ctx->sha_buf[0] = dma_alloc_coherent(
			se_dev->dev, (TEGRA_SE_SHA_MAX_BLOCK_SIZE * 2),
			&sha_ctx->sha_buf_addr[0], GFP_KERNEL);
	if (!sha_ctx->sha_buf[0]) {
		dev_err(se_dev->dev, "Cannot allocate memory to sha_buf[0]\n");
		mutex_unlock(&se_dev->mtx);
		return -ENOMEM;
	}
	sha_ctx->sha_buf[1] = dma_alloc_coherent(
			se_dev->dev, (TEGRA_SE_SHA_MAX_BLOCK_SIZE * 2),
			&sha_ctx->sha_buf_addr[1], GFP_KERNEL);
	if (!sha_ctx->sha_buf[1]) {
		dma_free_coherent(
				se_dev->dev, (TEGRA_SE_SHA_MAX_BLOCK_SIZE * 2),
				sha_ctx->sha_buf[0], sha_ctx->sha_buf_addr[0]);
		sha_ctx->sha_buf[0] = NULL;
		dev_err(se_dev->dev, "Cannot allocate memory to sha_buf[1]\n");
		mutex_unlock(&se_dev->mtx);
		return -ENOMEM;
	}
	mutex_unlock(&se_dev->mtx);

	return 0;
}

static void tegra_se_sha_cra_exit(struct crypto_tfm *tfm)
{
	struct tegra_se_sha_context *sha_ctx = crypto_tfm_ctx(tfm);
	struct tegra_se_dev *se_dev = se_devices[SE_SHA];
	int i;

	mutex_lock(&se_dev->mtx);
	for (i = 0; i < 2; i++) {
		/* dma_free_coherent does not panic if addr is NULL */
		dma_free_coherent(
				se_dev->dev, (TEGRA_SE_SHA_MAX_BLOCK_SIZE * 2),
				sha_ctx->sha_buf[i], sha_ctx->sha_buf_addr[i]);
		sha_ctx->sha_buf[i] = NULL;
	}
	mutex_unlock(&se_dev->mtx);
}

static int tegra_se_aes_cmac_export(struct ahash_request *req, void *out)
{
	return 0;
}

static int tegra_se_aes_cmac_import(struct ahash_request *req, const void *in)
{
	return 0;
}

static int tegra_t23x_se_aes_cmac_op(struct ahash_request *req,
					bool process_cur_req)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct tegra_se_aes_cmac_context *cmac_ctx = crypto_ahash_ctx(tfm);
	struct tegra_se_req_context *req_ctx = ahash_request_ctx(req);
	struct tegra_se_dev *se_dev;
	struct tegra_se_ll *src_ll;
	u32 num_sgs, total_bytes = 0;
	int ret = 0;

	se_dev = se_devices[SE_CMAC];

	mutex_lock(&se_dev->mtx);

	/*
	 * SE doesn't support CMAC input where message length is 0 bytes.
	 * See Bug 200472457.
	 */
	if (!(cmac_ctx->nbytes + req->nbytes)) {
		ret = -EINVAL;
		goto out;
	}

	if (process_cur_req) {
		if ((cmac_ctx->nbytes + req->nbytes)
				> TEGRA_SE_CMAC_MAX_INPUT_SIZE) {
			dev_err(se_dev->dev, "num of SG buffers bytes are more\n");
			mutex_unlock(&se_dev->mtx);
			return -EOPNOTSUPP;
		}

		num_sgs = tegra_se_count_sgs(req->src, req->nbytes);
		if (num_sgs > SE_MAX_SRC_SG_COUNT) {
			dev_err(se_dev->dev, "num of SG buffers are more\n");
			ret = -EDOM;
			goto out;
		}

		sg_copy_to_buffer(req->src, num_sgs,
				cmac_ctx->buf + cmac_ctx->nbytes, req->nbytes);
		cmac_ctx->nbytes += req->nbytes;
	}

	se_dev->src_ll = (struct tegra_se_ll *)(se_dev->src_ll_buf);
	src_ll = se_dev->src_ll;
	src_ll->addr = cmac_ctx->buf_dma_addr;
	src_ll->data_len = cmac_ctx->nbytes;
	src_ll++;
	total_bytes = cmac_ctx->nbytes;

	req_ctx->op_mode = SE_AES_OP_MODE_CMAC;
	req_ctx->config = tegra_se_get_config(se_dev, req_ctx->op_mode,
					      true, cmac_ctx->keylen);
	req_ctx->crypto_config = tegra_se_get_crypto_config(
			se_dev, req_ctx->op_mode, true,
			cmac_ctx->slot->slot_num, 0, true);

	tegra_se_send_data(se_dev, req_ctx, NULL, total_bytes,
			   se_dev->opcode_addr,
			   se_dev->aes_cmdbuf_cpuvaddr);
	ret = tegra_se_channel_submit_gather(
		se_dev, se_dev->aes_cmdbuf_cpuvaddr,
		se_dev->aes_cmdbuf_iova, 0, se_dev->cmdbuf_cnt, NONE);
	if (ret)
		goto out;

	ret = tegra_se_read_cmac_result(se_dev, req->result,
				  TEGRA_SE_AES_CMAC_DIGEST_SIZE, false);
	if (ret) {
		dev_err(se_dev->dev, "failed to read cmac result\n");
		goto out;
	}

	ret = tegra_se_clear_cmac_result(se_dev,
				   TEGRA_SE_AES_CMAC_DIGEST_SIZE);
	if (ret) {
		dev_err(se_dev->dev, "failed to clear cmac result\n");
		goto out;
	}

out:
	mutex_unlock(&se_dev->mtx);

	return ret;
}

static int tegra_se_aes_cmac_op(struct ahash_request *req, bool process_cur_req)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct tegra_se_aes_cmac_context *cmac_ctx = crypto_ahash_ctx(tfm);
	struct tegra_se_req_context *req_ctx = ahash_request_ctx(req);
	struct tegra_se_dev *se_dev;
	struct tegra_se_ll *src_ll;
	u32 num_sgs, blocks_to_process, last_block_bytes = 0, offset = 0;
	u8 piv[TEGRA_SE_AES_IV_SIZE];
	unsigned int total;
	int ret = 0, i = 0;
	bool padding_needed = false;
	bool use_orig_iv = true;

	se_dev = se_devices[SE_CMAC];

	if (se_dev->chipdata->kac_type == SE_KAC_T23X)
		return tegra_t23x_se_aes_cmac_op(req, process_cur_req);

	mutex_lock(&se_dev->mtx);

	if (process_cur_req) {
		if ((cmac_ctx->nbytes + req->nbytes)
				> TEGRA_SE_CMAC_MAX_INPUT_SIZE) {
			dev_err(se_dev->dev, "num of SG buffers bytes are more\n");
			mutex_unlock(&se_dev->mtx);
			return -EOPNOTSUPP;
		}

		num_sgs = tegra_se_count_sgs(req->src, req->nbytes);
		if (num_sgs > SE_MAX_SRC_SG_COUNT) {
			dev_err(se_dev->dev, "num of SG buffers are more\n");
			ret = -EDOM;
			goto out;
		}

		sg_copy_to_buffer(req->src, num_sgs,
				cmac_ctx->buf + cmac_ctx->nbytes, req->nbytes);
		cmac_ctx->nbytes += req->nbytes;
	}

	req_ctx->op_mode = SE_AES_OP_MODE_CMAC;
	blocks_to_process = cmac_ctx->nbytes / TEGRA_SE_AES_BLOCK_SIZE;
	/* num of bytes less than block size */
	if ((cmac_ctx->nbytes % TEGRA_SE_AES_BLOCK_SIZE)
				|| !blocks_to_process) {
		padding_needed = true;
	} else {
		/* decrement num of blocks */
		blocks_to_process--;
	}

	/* first process all blocks except last block */
	if (blocks_to_process) {
		total = blocks_to_process * TEGRA_SE_AES_BLOCK_SIZE;

		se_dev->src_ll = (struct tegra_se_ll *)(se_dev->src_ll_buf);
		src_ll = se_dev->src_ll;

		src_ll->addr = cmac_ctx->buf_dma_addr;
		src_ll->data_len = total;
		src_ll++;

		req_ctx->config = tegra_se_get_config(se_dev, req_ctx->op_mode,
						      true, cmac_ctx->keylen);
		/* write zero IV */
		memset(piv, 0, TEGRA_SE_AES_IV_SIZE);

		ret = tegra_se_send_key_data(
			se_dev, piv, TEGRA_SE_AES_IV_SIZE,
			cmac_ctx->slot->slot_num, SE_KEY_TABLE_TYPE_ORGIV,
			se_dev->opcode_addr, se_dev->aes_cmdbuf_cpuvaddr,
			se_dev->aes_cmdbuf_iova, NONE);
		if (ret)
			goto out;

		req_ctx->crypto_config = tegra_se_get_crypto_config(
				se_dev, req_ctx->op_mode, true,
				cmac_ctx->slot->slot_num, 0, true);

		tegra_se_send_data(se_dev, req_ctx, NULL, total,
				   se_dev->opcode_addr,
				   se_dev->aes_cmdbuf_cpuvaddr);
		ret = tegra_se_channel_submit_gather(
			se_dev, se_dev->aes_cmdbuf_cpuvaddr,
			se_dev->aes_cmdbuf_iova, 0, se_dev->cmdbuf_cnt, NONE);
		if (ret)
			goto out;

		ret = tegra_se_read_cmac_result(se_dev, piv,
					  TEGRA_SE_AES_CMAC_DIGEST_SIZE, false);
		if (ret) {
			dev_err(se_dev->dev, "failed to read cmac result\n");
			goto out;
		}

		use_orig_iv = false;
	}

	/* process last block */
	offset = blocks_to_process * TEGRA_SE_AES_BLOCK_SIZE;
	if (padding_needed) {
		/* pad with 0x80, 0, 0 ... */
		last_block_bytes = cmac_ctx->nbytes % TEGRA_SE_AES_BLOCK_SIZE;
		cmac_ctx->buf[cmac_ctx->nbytes] = 0x80;
		for (i = last_block_bytes + 1; i < TEGRA_SE_AES_BLOCK_SIZE; i++)
			cmac_ctx->buf[offset + i] = 0;
		/* XOR with K2 */
		for (i = 0; i < TEGRA_SE_AES_BLOCK_SIZE; i++)
			cmac_ctx->buf[offset + i] ^= cmac_ctx->K2[i];
	} else {
		/* XOR with K1 */
		for (i = 0; i < TEGRA_SE_AES_BLOCK_SIZE; i++)
			cmac_ctx->buf[offset + i] ^= cmac_ctx->K1[i];
	}

	se_dev->src_ll = (struct tegra_se_ll *)(se_dev->src_ll_buf);

	se_dev->src_ll->addr = cmac_ctx->buf_dma_addr + offset;
	se_dev->src_ll->data_len = TEGRA_SE_AES_BLOCK_SIZE;

	if (use_orig_iv) {
		/* use zero IV, this is when num of bytes is
		 * less <= block size
		 */
		memset(piv, 0, TEGRA_SE_AES_IV_SIZE);
		ret = tegra_se_send_key_data(
			se_dev, piv, TEGRA_SE_AES_IV_SIZE,
			cmac_ctx->slot->slot_num, SE_KEY_TABLE_TYPE_ORGIV,
			se_dev->opcode_addr, se_dev->aes_cmdbuf_cpuvaddr,
			se_dev->aes_cmdbuf_iova, NONE);
	} else {
		ret = tegra_se_send_key_data(
			se_dev, piv, TEGRA_SE_AES_IV_SIZE,
			cmac_ctx->slot->slot_num, SE_KEY_TABLE_TYPE_UPDTDIV,
			se_dev->opcode_addr, se_dev->aes_cmdbuf_cpuvaddr,
			se_dev->aes_cmdbuf_iova, NONE);
	}

	if (ret)
		goto out;

	req_ctx->config = tegra_se_get_config(se_dev, req_ctx->op_mode,
					      true, cmac_ctx->keylen);
	req_ctx->crypto_config = tegra_se_get_crypto_config(
				se_dev, req_ctx->op_mode, true,
				cmac_ctx->slot->slot_num, 0, use_orig_iv);

	tegra_se_send_data(se_dev, req_ctx, NULL, TEGRA_SE_AES_BLOCK_SIZE,
			   se_dev->opcode_addr, se_dev->aes_cmdbuf_cpuvaddr);
	ret = tegra_se_channel_submit_gather(
			se_dev, se_dev->aes_cmdbuf_cpuvaddr,
			se_dev->aes_cmdbuf_iova, 0, se_dev->cmdbuf_cnt, NONE);
	if (ret)
		goto out;

	ret = tegra_se_read_cmac_result(se_dev, req->result,
				  TEGRA_SE_AES_CMAC_DIGEST_SIZE, false);
	if (ret) {
		dev_err(se_dev->dev, "failed to read cmac result\n");
		goto out;
	}

	cmac_ctx->nbytes = 0;
out:
	mutex_unlock(&se_dev->mtx);

	return ret;
}

static int tegra_se_aes_cmac_setkey(struct crypto_ahash *tfm, const u8 *key,
				    unsigned int keylen)
{
	struct tegra_se_aes_cmac_context *ctx = crypto_ahash_ctx(tfm);
	struct tegra_se_req_context *req_ctx = NULL;
	struct tegra_se_dev *se_dev;
	struct tegra_se_slot *pslot;
	u8 piv[TEGRA_SE_AES_IV_SIZE];
	u32 *pbuf;
	dma_addr_t pbuf_adr;
	int ret = 0;
	u8 const rb = 0x87;
	u8 msb;

	se_dev = se_devices[SE_CMAC];

	mutex_lock(&se_dev->mtx);

	if (!ctx) {
		dev_err(se_dev->dev, "invalid context");
		mutex_unlock(&se_dev->mtx);
		return -EINVAL;
	}

	req_ctx = devm_kzalloc(se_dev->dev, sizeof(struct tegra_se_req_context),
			       GFP_KERNEL);
	if (!req_ctx) {
		mutex_unlock(&se_dev->mtx);
		return -ENOMEM;
	}
	if ((keylen != TEGRA_SE_KEY_128_SIZE) &&
	    (keylen != TEGRA_SE_KEY_192_SIZE) &&
	    (keylen != TEGRA_SE_KEY_256_SIZE)) {
		dev_err(se_dev->dev, "invalid key size");
		ret = -EINVAL;
		goto free_ctx;
	}

	if (key) {
		if (!ctx->slot ||
		    (ctx->slot &&
		     ctx->slot->slot_num == ssk_slot.slot_num)) {
			pslot = tegra_se_alloc_key_slot();
			if (!pslot) {
				dev_err(se_dev->dev, "no free key slot\n");
				ret = -ENOMEM;
				goto free_ctx;
			}
			ctx->slot = pslot;
		}
		ctx->keylen = keylen;
	} else {
		tegra_se_free_key_slot(ctx->slot);
		ctx->slot = &ssk_slot;
		ctx->keylen = AES_KEYSIZE_128;
	}

	pbuf = dma_alloc_coherent(se_dev->dev, TEGRA_SE_AES_BLOCK_SIZE,
				  &pbuf_adr, GFP_KERNEL);
	if (!pbuf) {
		dev_err(se_dev->dev, "can not allocate dma buffer");
		ret = -ENOMEM;
		goto keyslt_free;
	}
	memset(pbuf, 0, TEGRA_SE_AES_BLOCK_SIZE);

	se_dev->src_ll = (struct tegra_se_ll *)(se_dev->src_ll_buf);
	se_dev->dst_ll = (struct tegra_se_ll *)(se_dev->dst_ll_buf);

	se_dev->src_ll->addr = pbuf_adr;
	se_dev->src_ll->data_len = TEGRA_SE_AES_BLOCK_SIZE;
	se_dev->dst_ll->addr = pbuf_adr;
	se_dev->dst_ll->data_len = TEGRA_SE_AES_BLOCK_SIZE;

	/* load the key */
	if (se_dev->chipdata->kac_type == SE_KAC_T23X) {
		ret = tegra_se_send_key_data(
			se_dev, (u8 *)key, keylen, ctx->slot->slot_num,
			SE_KEY_TABLE_TYPE_CMAC, se_dev->opcode_addr,
			se_dev->aes_cmdbuf_cpuvaddr, se_dev->aes_cmdbuf_iova,
			NONE);
	} else {
		ret = tegra_se_send_key_data(
			se_dev, (u8 *)key, keylen, ctx->slot->slot_num,
			SE_KEY_TABLE_TYPE_KEY, se_dev->opcode_addr,
			se_dev->aes_cmdbuf_cpuvaddr, se_dev->aes_cmdbuf_iova,
			NONE);
	}
	if (ret) {
		dev_err(se_dev->dev,
			"tegra_se_send_key_data for loading cmac key failed\n");
		goto out;
	}

	/* write zero IV */
	memset(piv, 0, TEGRA_SE_AES_IV_SIZE);

	/* load IV */
	ret = tegra_se_send_key_data(
		se_dev, piv, TEGRA_SE_AES_IV_SIZE, ctx->slot->slot_num,
		SE_KEY_TABLE_TYPE_ORGIV, se_dev->opcode_addr,
		se_dev->aes_cmdbuf_cpuvaddr, se_dev->aes_cmdbuf_iova, NONE);
	if (ret) {
		dev_err(se_dev->dev,
			"tegra_se_send_key_data for loading cmac iv failed\n");
		goto out;
	}

	if (se_dev->chipdata->kac_type == SE_KAC_T23X) {
		ret = tegra_se_clear_cmac_result(se_dev,
					   TEGRA_SE_AES_CMAC_DIGEST_SIZE);
		if (ret) {
			dev_err(se_dev->dev, "failed to clear cmac result\n");
			goto out;
		}
	} else {
		/* config crypto algo */
		req_ctx->config = tegra_se_get_config(se_dev,
						      SE_AES_OP_MODE_CBC, true,
						      keylen);
		req_ctx->crypto_config = tegra_se_get_crypto_config(
					se_dev, SE_AES_OP_MODE_CBC, true,
					ctx->slot->slot_num, 0, true);

		tegra_se_send_data(se_dev, req_ctx, NULL,
				   TEGRA_SE_AES_BLOCK_SIZE, se_dev->opcode_addr,
				   se_dev->aes_cmdbuf_cpuvaddr);
		ret = tegra_se_channel_submit_gather(
			se_dev, se_dev->aes_cmdbuf_cpuvaddr,
			se_dev->aes_cmdbuf_iova,
			0, se_dev->cmdbuf_cnt, NONE);
		if (ret) {
			dev_err(se_dev->dev,
				"tegra_se_aes_cmac_setkey:: start op failed\n");
			goto out;
		}

		/* compute K1 subkey */
		memcpy(ctx->K1, pbuf, TEGRA_SE_AES_BLOCK_SIZE);
		tegra_se_leftshift_onebit(ctx->K1, TEGRA_SE_AES_BLOCK_SIZE,
					  &msb);
		if (msb)
			ctx->K1[TEGRA_SE_AES_BLOCK_SIZE - 1] ^= rb;

		/* compute K2 subkey */
		memcpy(ctx->K2, ctx->K1, TEGRA_SE_AES_BLOCK_SIZE);
		tegra_se_leftshift_onebit(ctx->K2, TEGRA_SE_AES_BLOCK_SIZE,
					  &msb);

		if (msb)
			ctx->K2[TEGRA_SE_AES_BLOCK_SIZE - 1] ^= rb;
	}
out:
	if (pbuf)
		dma_free_coherent(se_dev->dev, TEGRA_SE_AES_BLOCK_SIZE,
				  pbuf, pbuf_adr);
keyslt_free:
	if (ret)
		tegra_se_free_key_slot(ctx->slot);
free_ctx:
	devm_kfree(se_dev->dev, req_ctx);
	mutex_unlock(&se_dev->mtx);

	return ret;
}

static int tegra_se_aes_cmac_init(struct ahash_request *req)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct tegra_se_aes_cmac_context *cmac_ctx = crypto_ahash_ctx(tfm);

	cmac_ctx->nbytes = 0;

	return 0;
}

static int tegra_se_aes_cmac_update(struct ahash_request *req)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct tegra_se_aes_cmac_context *cmac_ctx = crypto_ahash_ctx(tfm);
	struct tegra_se_dev *se_dev;
	u32 num_sgs;

	se_dev = se_devices[SE_CMAC];

	if ((cmac_ctx->nbytes + req->nbytes)
			> TEGRA_SE_CMAC_MAX_INPUT_SIZE) {
		dev_err(se_dev->dev, "num of SG buffers bytes are more\n");
		return -EOPNOTSUPP;
	}

	num_sgs = tegra_se_count_sgs(req->src, req->nbytes);

	sg_copy_to_buffer(req->src, num_sgs, cmac_ctx->buf + cmac_ctx->nbytes,
				req->nbytes);
	cmac_ctx->nbytes += req->nbytes;

	return 0;
}

static int tegra_se_aes_cmac_digest(struct ahash_request *req)
{
	return tegra_se_aes_cmac_init(req) ?: tegra_se_aes_cmac_op(req, true);
}

static int tegra_se_aes_cmac_final(struct ahash_request *req)
{
	return tegra_se_aes_cmac_op(req, false);
}

static int tegra_se_aes_cmac_finup(struct ahash_request *req)
{
	return tegra_se_aes_cmac_op(req, true);
}

static int tegra_se_aes_cmac_cra_init(struct crypto_tfm *tfm)
{
	struct tegra_se_aes_cmac_context *cmac_ctx;
	struct tegra_se_dev *se_dev = se_devices[SE_CMAC];

	crypto_ahash_set_reqsize(__crypto_ahash_cast(tfm),
				 sizeof(struct tegra_se_aes_cmac_context));
	cmac_ctx = crypto_tfm_ctx(tfm);
	if (!cmac_ctx) {
		dev_err(se_dev->dev, "CMAC context not valid\n");
		return -EINVAL;
	}

	mutex_lock(&se_dev->mtx);
	cmac_ctx->buf = dma_alloc_coherent(se_dev->dev,
				(TEGRA_SE_AES_BLOCK_SIZE * 20),
				&cmac_ctx->buf_dma_addr, GFP_KERNEL);
	if (!cmac_ctx->buf) {
		dev_err(se_dev->dev, "Cannot allocate memory to buf\n");
		mutex_unlock(&se_dev->mtx);
		return -ENOMEM;
	}
	cmac_ctx->nbytes = 0;
	mutex_unlock(&se_dev->mtx);

	return 0;
}

static void tegra_se_aes_cmac_cra_exit(struct crypto_tfm *tfm)
{
	struct tegra_se_aes_cmac_context *cmac_ctx = crypto_tfm_ctx(tfm);
	struct tegra_se_dev *se_dev = se_devices[SE_CMAC];

	tegra_se_free_key_slot(cmac_ctx->slot);
	cmac_ctx->slot = NULL;

	mutex_lock(&se_dev->mtx);
	dma_free_coherent(se_dev->dev, (TEGRA_SE_AES_BLOCK_SIZE * 20),
				cmac_ctx->buf, cmac_ctx->buf_dma_addr);
	mutex_unlock(&se_dev->mtx);
}

static int tegra_se_sha_hmac_setkey(struct crypto_ahash *tfm, const u8 *key,
				    unsigned int keylen)
{
	struct tegra_se_sha_context *ctx = crypto_ahash_ctx(tfm);
	struct tegra_se_req_context *req_ctx = NULL;
	struct tegra_se_dev *se_dev;
	unsigned int index;
	dma_addr_t iova;
	u32 *cpuvaddr;
	u8 _key[TEGRA_SE_KEY_256_SIZE] = { 0 };
	int ret = 0;

	se_dev = se_devices[SE_SHA];
	mutex_lock(&se_dev->mtx);

	req_ctx = devm_kzalloc(se_dev->dev,
			       sizeof(struct tegra_se_req_context),
			       GFP_KERNEL);
	if (!req_ctx) {
		ret = -ENOMEM;
		goto out_mutex;
	}

	if (keylen > TEGRA_SE_KEY_256_SIZE) {
		dev_err(se_dev->dev, "invalid key size");
		ret = -EINVAL;
		goto free_req_ctx;
	}

	/* If keylen < TEGRA_SE_KEY_256_SIZE. Appened 0's to the end of key.
	 * This will allow the key to be stored in the keyslot.
	 * Since, TEGRA_SE_KEY_256_SIZE < 512-bits which is the minimum sha
	 * block size.
	 */
	memcpy(_key, key, keylen);

	if (!key) {
		ret = -EINVAL;
		goto free_req_ctx;
	}

	if (!ctx->slot) {
		ctx->slot = tegra_se_alloc_key_slot();
		if (!ctx->slot) {
			dev_err(se_dev->dev, "no free key slot\n");
			ret = -ENOMEM;
			goto free_req_ctx;
		}
	}

	ctx->keylen = keylen;

	ret = tegra_se_get_free_cmdbuf(se_dev);
	if (ret < 0) {
		dev_err(se_dev->dev, "Couldn't get free cmdbuf\n");
		goto free_keyslot;
	}

	index = ret;

	cpuvaddr = se_dev->cmdbuf_addr_list[index].cmdbuf_addr;
	iova = se_dev->cmdbuf_addr_list[index].iova;
	atomic_set(&se_dev->cmdbuf_addr_list[index].free, 0);
	se_dev->cmdbuf_list_entry = index;

	ret = tegra_se_send_key_data(se_dev, _key, TEGRA_SE_KEY_256_SIZE,
		ctx->slot->slot_num, SE_KEY_TABLE_TYPE_HMAC,
		se_dev->opcode_addr, cpuvaddr, iova, NONE);

	if (ret)
		dev_err(se_dev->dev,
			"tegra_se_send_key_data for loading HMAC key failed\n");

free_keyslot:
	if (ret)
		tegra_se_free_key_slot(ctx->slot);
free_req_ctx:
	devm_kfree(se_dev->dev, req_ctx);
out_mutex:
	mutex_unlock(&se_dev->mtx);

	return ret;
}

/* Security Engine rsa key slot */
struct tegra_se_rsa_slot {
	struct list_head node;
	u8 slot_num;	/* Key slot number */
	bool available; /* Tells whether key slot is free to use */
};

/* Security Engine AES RSA context */
struct tegra_se_aes_rsa_context {
	struct tegra_se_dev *se_dev;	/* Security Engine device */
	struct tegra_se_rsa_slot *slot;	/* Security Engine rsa key slot */
	u32 mod_len;
	u32 exp_len;
};

static void tegra_se_rsa_free_key_slot(struct tegra_se_rsa_slot *slot)
{
	if (slot) {
		spin_lock(&rsa_key_slot_lock);
		slot->available = true;
		spin_unlock(&rsa_key_slot_lock);
	}
}

static struct tegra_se_rsa_slot *tegra_se_alloc_rsa_key_slot(void)
{
	struct tegra_se_rsa_slot *slot = NULL;
	bool found = false;

	spin_lock(&rsa_key_slot_lock);
	list_for_each_entry(slot, &rsa_key_slot, node) {
		if (slot->available) {
			slot->available = false;
			found = true;
			break;
		}
	}
	spin_unlock(&rsa_key_slot_lock);

	return found ? slot : NULL;
}

static int tegra_init_rsa_key_slot(struct tegra_se_dev *se_dev)
{
	int i;

	se_dev->rsa_slot_list = devm_kzalloc(
			se_dev->dev, sizeof(struct tegra_se_rsa_slot) *
			TEGRA_SE_RSA_KEYSLOT_COUNT, GFP_KERNEL);
	if (!se_dev->rsa_slot_list)
		return -ENOMEM;

	spin_lock_init(&rsa_key_slot_lock);
	spin_lock(&rsa_key_slot_lock);
	for (i = 0; i < TEGRA_SE_RSA_KEYSLOT_COUNT; i++) {
		se_dev->rsa_slot_list[i].available = true;
		se_dev->rsa_slot_list[i].slot_num = i;
		INIT_LIST_HEAD(&se_dev->rsa_slot_list[i].node);
		list_add_tail(&se_dev->rsa_slot_list[i].node, &rsa_key_slot);
	}
	spin_unlock(&rsa_key_slot_lock);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
static unsigned int tegra_se_rsa_max_size(struct crypto_akcipher *tfm)
#else
static int tegra_se_rsa_max_size(struct crypto_akcipher *tfm)
#endif
{
	struct tegra_se_aes_rsa_context *ctx = akcipher_tfm_ctx(tfm);

	if (!ctx) {
		pr_err("No RSA context\n");
		return -EINVAL;
	}

	return ctx->mod_len;
}

static int tegra_se_send_rsa_data(struct tegra_se_dev *se_dev,
				  struct tegra_se_aes_rsa_context *rsa_ctx)
{
	u32 *cmdbuf_cpuvaddr = NULL;
	dma_addr_t cmdbuf_iova = 0;
	u32 cmdbuf_num_words = 0, i = 0;
	int err = 0;
	u32 val = 0;

	cmdbuf_cpuvaddr = dma_alloc_attrs(se_dev->dev->parent, SZ_4K,
					  &cmdbuf_iova, GFP_KERNEL,
					  0);
	if (!cmdbuf_cpuvaddr) {
		dev_err(se_dev->dev, "Failed to allocate memory for cmdbuf\n");
		return -ENOMEM;
	}

	cmdbuf_cpuvaddr[i++] = __nvhost_opcode_nonincr(
			se_dev->opcode_addr + SE_RSA_OPERATION_OFFSET, 1);
	cmdbuf_cpuvaddr[i++] = SE_OPERATION_WRSTALL(WRSTALL_TRUE);

	val = SE_CONFIG_ENC_ALG(ALG_RSA) |
		SE_CONFIG_DEC_ALG(ALG_NOP) |
		SE_CONFIG_DST(DST_MEMORY);
	cmdbuf_cpuvaddr[i++] = __nvhost_opcode_incr(se_dev->opcode_addr, 8);
	cmdbuf_cpuvaddr[i++] = val;
	cmdbuf_cpuvaddr[i++] = RSA_KEY_SLOT(rsa_ctx->slot->slot_num);
	cmdbuf_cpuvaddr[i++] = (rsa_ctx->mod_len / 64) - 1;
	cmdbuf_cpuvaddr[i++] = (rsa_ctx->exp_len / 4);
	cmdbuf_cpuvaddr[i++] = (u32)(se_dev->src_ll->addr);
	cmdbuf_cpuvaddr[i++] = (u32)(SE_ADDR_HI_MSB(MSB(se_dev->src_ll->addr)) |
				SE_ADDR_HI_SZ(se_dev->src_ll->data_len));
	cmdbuf_cpuvaddr[i++] = (u32)(se_dev->dst_ll->addr);
	cmdbuf_cpuvaddr[i++] = (u32)(SE_ADDR_HI_MSB(MSB(se_dev->dst_ll->addr)) |
				SE_ADDR_HI_SZ(se_dev->dst_ll->data_len));
	cmdbuf_cpuvaddr[i++] = __nvhost_opcode_nonincr(
			se_dev->opcode_addr + SE_RSA_OPERATION_OFFSET, 1);
	cmdbuf_cpuvaddr[i++] = SE_OPERATION_WRSTALL(WRSTALL_TRUE) |
				SE_OPERATION_LASTBUF(LASTBUF_TRUE) |
				SE_OPERATION_OP(OP_START);

	cmdbuf_num_words = i;

	err = tegra_se_channel_submit_gather(se_dev, cmdbuf_cpuvaddr,
					     cmdbuf_iova, 0, cmdbuf_num_words,
					     NONE);

	dma_free_attrs(se_dev->dev->parent, SZ_4K, cmdbuf_cpuvaddr,
		       cmdbuf_iova, 0);

	return err;
}

static int tegra_se_rsa_setkey(struct crypto_akcipher *tfm, const void *key,
			       unsigned int keylen)
{
	struct tegra_se_aes_rsa_context *ctx = akcipher_tfm_ctx(tfm);
	struct tegra_se_dev *se_dev;
	u32 module_key_length = 0;
	u32 exponent_key_length = 0;
	u32 pkt, val;
	u32 key_size_words;
	u32 key_word_size = 4;
	u32 *pkeydata = (u32 *)key;
	s32 j = 0;
	struct tegra_se_rsa_slot *pslot;
	u32 cmdbuf_num_words = 0, i = 0;
	u32 *cmdbuf_cpuvaddr = NULL;
	dma_addr_t cmdbuf_iova = 0;
	int err = 0;
	int timeout = 0;

	se_dev = se_devices[SE_RSA];

	if (!ctx || !key) {
		dev_err(se_dev->dev, "No RSA context or Key\n");
		return -EINVAL;
	}

	/* Allocate rsa key slot */
	if (!ctx->slot) {
		for (timeout = 0; timeout < SE_KEYSLOT_TIMEOUT; timeout++) {
			pslot = tegra_se_alloc_rsa_key_slot();
			if (!pslot) {
				mdelay(SE_KEYSLOT_MDELAY);
				continue;
			} else {
				break;
			}
		}

		if (!pslot) {
			dev_err(se_dev->dev, "no free key slot\n");
			return -ENOMEM;
		} else {
			ctx->slot = pslot;
		}
	}

	module_key_length = (keylen >> 16);
	exponent_key_length = (keylen & (0xFFFF));

	if (!(((module_key_length / 64) >= 1) &&
	      ((module_key_length / 64) <= 4))) {
		tegra_se_rsa_free_key_slot(ctx->slot);
		dev_err(se_dev->dev, "Invalid RSA modulus length\n");
		return -EDOM;
	}

	ctx->mod_len = module_key_length;
	ctx->exp_len = exponent_key_length;

	cmdbuf_cpuvaddr = dma_alloc_attrs(se_dev->dev->parent, SZ_64K,
					  &cmdbuf_iova, GFP_KERNEL,
					  0);
	if (!cmdbuf_cpuvaddr) {
		tegra_se_rsa_free_key_slot(ctx->slot);
		dev_err(se_dev->dev, "Failed to allocate memory for cmdbuf\n");
		return -ENOMEM;
	}

	cmdbuf_cpuvaddr[i++] = __nvhost_opcode_nonincr(
			se_dev->opcode_addr + SE_RSA_OPERATION_OFFSET, 1);
	cmdbuf_cpuvaddr[i++] = SE_OPERATION_WRSTALL(WRSTALL_TRUE);

	if (exponent_key_length) {
		key_size_words = (exponent_key_length / key_word_size);
		/* Write exponent */
		for (j = (key_size_words - 1); j >= 0; j--) {
			pkt = RSA_KEY_NUM(ctx->slot->slot_num) |
				RSA_KEY_TYPE(RSA_KEY_TYPE_EXP) |
				RSA_KEY_PKT_WORD_ADDR(j);
			val = SE_RSA_KEYTABLE_PKT(pkt);
			cmdbuf_cpuvaddr[i++] = __nvhost_opcode_nonincr(
					se_dev->opcode_addr +
					SE_RSA_KEYTABLE_ADDR_OFFSET, 1);
			cmdbuf_cpuvaddr[i++] = val;
			cmdbuf_cpuvaddr[i++] = __nvhost_opcode_nonincr(
					se_dev->opcode_addr +
					SE_RSA_KEYTABLE_DATA_OFFSET, 1);
			cmdbuf_cpuvaddr[i++] = *pkeydata++;
		}
	}

	if (module_key_length) {
		key_size_words = (module_key_length / key_word_size);
		/* Write modulus */
		for (j = (key_size_words - 1); j >= 0; j--) {
			pkt = RSA_KEY_NUM(ctx->slot->slot_num) |
				RSA_KEY_TYPE(RSA_KEY_TYPE_MOD) |
				RSA_KEY_PKT_WORD_ADDR(j);
			val = SE_RSA_KEYTABLE_PKT(pkt);
			cmdbuf_cpuvaddr[i++] = __nvhost_opcode_nonincr(
					se_dev->opcode_addr +
					SE_RSA_KEYTABLE_ADDR_OFFSET, 1);
			cmdbuf_cpuvaddr[i++] = val;
			cmdbuf_cpuvaddr[i++] = __nvhost_opcode_nonincr(
					se_dev->opcode_addr +
					SE_RSA_KEYTABLE_DATA_OFFSET, 1);
			cmdbuf_cpuvaddr[i++] = *pkeydata++;
		}
	}

	cmdbuf_cpuvaddr[i++] = __nvhost_opcode_nonincr(
			se_dev->opcode_addr + SE_RSA_OPERATION_OFFSET, 1);
	cmdbuf_cpuvaddr[i++] = SE_OPERATION_WRSTALL(WRSTALL_TRUE) |
				SE_OPERATION_LASTBUF(LASTBUF_TRUE) |
				SE_OPERATION_OP(OP_DUMMY);
	cmdbuf_num_words = i;

	mutex_lock(&se_dev->mtx);
	err = tegra_se_channel_submit_gather(se_dev, cmdbuf_cpuvaddr,
					     cmdbuf_iova, 0, cmdbuf_num_words,
					     NONE);
	mutex_unlock(&se_dev->mtx);
	if (err)
		tegra_se_rsa_free_key_slot(ctx->slot);
	dma_free_attrs(se_dev->dev->parent, SZ_64K, cmdbuf_cpuvaddr,
		       cmdbuf_iova, 0);

	return err;
}

static int tegra_se_rsa_op(struct akcipher_request *req)
{
	struct crypto_akcipher *tfm = NULL;
	struct tegra_se_aes_rsa_context *rsa_ctx = NULL;
	struct tegra_se_dev *se_dev;
	u32 num_src_sgs, num_dst_sgs;
	int ret1 = 0, ret2 = 0;

	se_dev = se_devices[SE_RSA];

	if (!req) {
		dev_err(se_dev->dev, "Invalid RSA request\n");
		return -EINVAL;
	}

	tfm = crypto_akcipher_reqtfm(req);
	if (!tfm) {
		dev_err(se_dev->dev, "Invalid RSA transform\n");
		return -EINVAL;
	}

	rsa_ctx = akcipher_tfm_ctx(tfm);
	if (!rsa_ctx || !rsa_ctx->slot) {
		dev_err(se_dev->dev, "Invalid RSA context\n");
		return -EINVAL;
	}

	if ((req->src_len < TEGRA_SE_RSA512_INPUT_SIZE) ||
	    (req->src_len > TEGRA_SE_RSA2048_INPUT_SIZE)) {
		dev_err(se_dev->dev, "RSA src input length not in range\n");
		return -EDOM;
	}

	if ((req->dst_len < TEGRA_SE_RSA512_INPUT_SIZE) ||
	    (req->dst_len > TEGRA_SE_RSA2048_INPUT_SIZE)) {
		dev_err(se_dev->dev, "RSA dst input length not in range\n");
		return -EDOM;
	}

	if (req->src_len != rsa_ctx->mod_len) {
		dev_err(se_dev->dev, "Invalid RSA src input length\n");
		return -EINVAL;
	}

	num_src_sgs = tegra_se_count_sgs(req->src, req->src_len);
	num_dst_sgs = tegra_se_count_sgs(req->dst, req->dst_len);
	if ((num_src_sgs > SE_MAX_SRC_SG_COUNT) ||
	    (num_dst_sgs > SE_MAX_DST_SG_COUNT)) {
		dev_err(se_dev->dev, "num of SG buffers are more\n");
		return -EDOM;
	}

	mutex_lock(&se_dev->mtx);
	se_dev->src_ll = (struct tegra_se_ll *)(se_dev->src_ll_buf);

	if (req->src == req->dst) {
		se_dev->dst_ll = se_dev->src_ll;
		ret1 = tegra_map_sg(se_dev->dev, req->src, 1, DMA_BIDIRECTIONAL,
				    se_dev->src_ll, req->src_len);
		if (!ret1) {
			mutex_unlock(&se_dev->mtx);
			return -EINVAL;
		}
	} else {
		se_dev->dst_ll = (struct tegra_se_ll *)(se_dev->dst_ll_buf);
		ret1 = tegra_map_sg(se_dev->dev, req->src, 1, DMA_TO_DEVICE,
				    se_dev->src_ll, req->src_len);
		ret2 = tegra_map_sg(se_dev->dev, req->dst, 1, DMA_FROM_DEVICE,
				    se_dev->dst_ll, req->dst_len);
		if (!ret1 || !ret2) {
			mutex_unlock(&se_dev->mtx);
			return -EINVAL;
		}
	}

	ret1 = tegra_se_send_rsa_data(se_dev, rsa_ctx);
	if (ret1)
		dev_err(se_dev->dev, "RSA send data failed err = %d\n", ret1);

	if (req->src == req->dst) {
		tegra_unmap_sg(se_dev->dev, req->src, DMA_BIDIRECTIONAL,
			       req->src_len);
	} else {
		tegra_unmap_sg(se_dev->dev, req->src, DMA_TO_DEVICE,
			       req->src_len);
		tegra_unmap_sg(se_dev->dev, req->dst, DMA_FROM_DEVICE,
			       req->dst_len);
	}

	mutex_unlock(&se_dev->mtx);
	return ret1;
}

static void tegra_se_rsa_exit(struct crypto_akcipher *tfm)
{
	struct tegra_se_aes_rsa_context *ctx = akcipher_tfm_ctx(tfm);

	tegra_se_rsa_free_key_slot(ctx->slot);
	ctx->slot = NULL;
}

static inline struct tegra_se_dh_context *tegra_se_dh_get_ctx(
						struct crypto_kpp *tfm)
{
	return kpp_tfm_ctx(tfm);
}

static int tegra_se_dh_check_params_length(unsigned int p_len)
{
	if (p_len < MIN_DH_SZ_BITS) {
		pr_err("DH Modulus length not in range\n");
		return -EDOM;
	}

	return 0;
}

static int tegra_se_dh_set_params(struct tegra_se_dh_context *ctx,
				  struct dh *params)
{
	int ret = 0;

	ret = tegra_se_dh_check_params_length(params->p_size << 3);
	if (ret)
		return ret;

	ctx->key = (void *)params->key;
	ctx->key_size = params->key_size;
	if (!ctx->key) {
		dev_err(ctx->se_dev->dev, "Invalid DH Key\n");
		return -ENODATA;
	}

	ctx->p = (void *)params->p;
	ctx->p_size = params->p_size;
	if (!ctx->p) {
		dev_err(ctx->se_dev->dev, "Invalid DH Modulus\n");
		return -ENODATA;
	}

	ctx->g = (void *)params->g;
	ctx->g_size = params->g_size;
	if (!ctx->g) {
		dev_err(ctx->se_dev->dev, "Invalid DH generator\n");
		return -ENODATA;
	}

	if (ctx->g_size > ctx->p_size) {
		dev_err(ctx->se_dev->dev, "Invalid DH generator size\n");
		return -EDOM;
	}

	return 0;
}

static int tegra_se_dh_setkey(struct crypto_kpp *tfm)
{
	struct tegra_se_dh_context *ctx = tegra_se_dh_get_ctx(tfm);
	struct tegra_se_dev *se_dev;
	u32 module_key_length = 0;
	u32 exponent_key_length = 0;
	u32 pkt, val;
	u32 key_size_words;
	u32 key_word_size = 4;
	u32 *pkeydata, *cmdbuf_cpuvaddr = NULL;
	struct tegra_se_rsa_slot *pslot;
	u32 cmdbuf_num_words = 0;
	dma_addr_t cmdbuf_iova = 0;
	int i = 0, err, j;

	if (!ctx) {
		pr_err("Invalid DH context\n");
		return -EINVAL;
	}

	se_dev = ctx->se_dev;
	pkeydata = (u32 *)ctx->key;

	/* Allocate rsa key slot */
	if (!ctx->slot) {
		pslot = tegra_se_alloc_rsa_key_slot();
		if (!pslot) {
			dev_err(se_dev->dev, "no free key slot\n");
			return -ENOMEM;
		}
		ctx->slot = pslot;
	}

	module_key_length = ctx->p_size;
	exponent_key_length = ctx->key_size;

	if (!(((module_key_length / 64) >= 1) &&
	      ((module_key_length / 64) <= 4))) {
		tegra_se_rsa_free_key_slot(ctx->slot);
		dev_err(se_dev->dev, "DH Modulus length not in range\n");
		return -EDOM;
	}

	cmdbuf_cpuvaddr = dma_alloc_attrs(se_dev->dev->parent, SZ_64K,
					  &cmdbuf_iova, GFP_KERNEL,
					  0);
	if (!cmdbuf_cpuvaddr) {
		tegra_se_rsa_free_key_slot(ctx->slot);
		dev_err(se_dev->dev, "Failed to allocate cmdbuf\n");
		return -ENOMEM;
	}

	cmdbuf_cpuvaddr[i++] = __nvhost_opcode_nonincr(
			se_dev->opcode_addr + SE_RSA_OPERATION_OFFSET, 1);
	cmdbuf_cpuvaddr[i++] = SE_OPERATION_WRSTALL(WRSTALL_TRUE);

	if (exponent_key_length) {
		key_size_words = (exponent_key_length / key_word_size);
		/* Write exponent */
		for (j = (key_size_words - 1); j >= 0; j--) {
			pkt = RSA_KEY_NUM(ctx->slot->slot_num) |
				RSA_KEY_TYPE(RSA_KEY_TYPE_EXP) |
				RSA_KEY_PKT_WORD_ADDR(j);
			val = SE_RSA_KEYTABLE_PKT(pkt);
			cmdbuf_cpuvaddr[i++] = __nvhost_opcode_nonincr(
					se_dev->opcode_addr +
					SE_RSA_KEYTABLE_ADDR_OFFSET, 1);
			cmdbuf_cpuvaddr[i++] = val;
			cmdbuf_cpuvaddr[i++] = __nvhost_opcode_nonincr(
					se_dev->opcode_addr +
					SE_RSA_KEYTABLE_DATA_OFFSET, 1);
			cmdbuf_cpuvaddr[i++] =
				be32_to_cpu((__force __be32)*pkeydata++);
		}
	}

	if (module_key_length) {
		pkeydata = (u32 *)ctx->p;
		key_size_words = (module_key_length / key_word_size);
		/* Write modulus */
		for (j = (key_size_words - 1); j >= 0; j--) {
			pkt = RSA_KEY_NUM(ctx->slot->slot_num) |
				RSA_KEY_TYPE(RSA_KEY_TYPE_MOD) |
				RSA_KEY_PKT_WORD_ADDR(j);
			val = SE_RSA_KEYTABLE_PKT(pkt);
			cmdbuf_cpuvaddr[i++] = __nvhost_opcode_nonincr(
					se_dev->opcode_addr +
					SE_RSA_KEYTABLE_ADDR_OFFSET, 1);
			cmdbuf_cpuvaddr[i++] = val;
			cmdbuf_cpuvaddr[i++] = __nvhost_opcode_nonincr(
					se_dev->opcode_addr +
					SE_RSA_KEYTABLE_DATA_OFFSET, 1);
			cmdbuf_cpuvaddr[i++] =
				be32_to_cpu((__force __be32)*pkeydata++);
		}
	}

	cmdbuf_cpuvaddr[i++] = __nvhost_opcode_nonincr(
			se_dev->opcode_addr + SE_RSA_OPERATION_OFFSET, 1);
	cmdbuf_cpuvaddr[i++] = SE_OPERATION_WRSTALL(WRSTALL_TRUE) |
				SE_OPERATION_LASTBUF(LASTBUF_TRUE) |
				SE_OPERATION_OP(OP_DUMMY);
	cmdbuf_num_words = i;

	err = tegra_se_channel_submit_gather(se_dev, cmdbuf_cpuvaddr,
					     cmdbuf_iova, 0, cmdbuf_num_words,
					     NONE);
	if (err) {
		dev_err(se_dev->dev, "%s: channel_submit failed\n", __func__);
		tegra_se_rsa_free_key_slot(ctx->slot);
	}
	dma_free_attrs(se_dev->dev->parent, SZ_64K, cmdbuf_cpuvaddr,
		       cmdbuf_iova, 0);

	return err;
}

static void tegra_se_fix_endianness(struct tegra_se_dev *se_dev,
				    struct scatterlist *sg, u32 num_sgs,
				    unsigned int nbytes, bool be)
{
	int j, k;

	sg_copy_to_buffer(sg, num_sgs, se_dev->dh_buf1, nbytes);

	for (j = (nbytes / 4 - 1), k = 0; j >= 0; j--, k++) {
		if (be)
			se_dev->dh_buf2[k] =
				be32_to_cpu((__force __be32)se_dev->dh_buf1[j]);
		else
			se_dev->dh_buf2[k] =
				(__force u32)cpu_to_be32(se_dev->dh_buf1[j]);
	}

	sg_copy_from_buffer(sg, num_sgs, se_dev->dh_buf2, nbytes);
}

static int tegra_se_dh_compute_value(struct kpp_request *req)
{
	struct crypto_kpp *tfm = NULL;
	struct tegra_se_dh_context *dh_ctx = NULL;
	struct tegra_se_dev *se_dev;
	struct scatterlist *src_sg;
	u32 num_src_sgs, num_dst_sgs;
	u8 *base_buff = NULL;
	struct scatterlist src;
	u32 *cmdbuf_cpuvaddr = NULL;
	dma_addr_t cmdbuf_iova = 0;
	u32 cmdbuf_num_words = 0, i = 0;
	int err, j;
	unsigned int total, zpad_sz;
	u32 val;

	if (!req) {
		pr_err("Invalid DH request\n");
		return -EINVAL;
	}

	tfm = crypto_kpp_reqtfm(req);
	if (!tfm) {
		pr_err("Invalid DH transform\n");
		return -EINVAL;
	}

	dh_ctx = tegra_se_dh_get_ctx(tfm);
	if (!dh_ctx || !dh_ctx->slot) {
		pr_err("Invalid DH context\n");
		return -EINVAL;
	}

	se_dev = dh_ctx->se_dev;

	if (req->src) {
		src_sg = req->src;
		total = req->src_len;
	} else {
		if (dh_ctx->g_size < dh_ctx->p_size) {
			base_buff = (u8 *)devm_kzalloc(se_dev->dev,
					       dh_ctx->p_size, GFP_KERNEL);
			if (!base_buff)
				return -ENOMEM;
			zpad_sz = dh_ctx->p_size - dh_ctx->g_size;

			for (j = 0; j < zpad_sz; j++)
				base_buff[j] = 0x0;
			for (j = zpad_sz; j < dh_ctx->p_size; j++)
				base_buff[j] = *(u8 *)(dh_ctx->g++);

			dh_ctx->g_size = dh_ctx->p_size;
		} else {
			base_buff = (u8 *)devm_kzalloc(se_dev->dev,
					       dh_ctx->g_size, GFP_KERNEL);
			if (!base_buff)
				return -ENOMEM;
			memcpy(base_buff, (u8 *)(dh_ctx->g), dh_ctx->g_size);
		}

		sg_init_one(&src, base_buff, dh_ctx->g_size);

		src_sg = &src;
		total = dh_ctx->g_size;
	}

	num_src_sgs = tegra_se_count_sgs(src_sg, total);
	num_dst_sgs = tegra_se_count_sgs(req->dst, req->dst_len);
	if ((num_src_sgs > SE_MAX_SRC_SG_COUNT) ||
	    (num_dst_sgs > SE_MAX_DST_SG_COUNT)) {
		dev_err(se_dev->dev, "num of SG buffers are more\n");
		err = -EDOM;
		goto free;
	}

	tegra_se_fix_endianness(se_dev, src_sg, num_src_sgs, total, true);

	se_dev->src_ll = (struct tegra_se_ll *)(se_dev->src_ll_buf);
	se_dev->dst_ll = (struct tegra_se_ll *)(se_dev->dst_ll_buf);

	err = tegra_map_sg(se_dev->dev, src_sg, 1, DMA_TO_DEVICE,
			   se_dev->src_ll, total);
	if (!err) {
		err = -EINVAL;
		goto free;
	}
	err = tegra_map_sg(se_dev->dev, req->dst, 1, DMA_FROM_DEVICE,
			   se_dev->dst_ll, req->dst_len);
	if (!err) {
		err = -EINVAL;
		goto unmap_src;
	}

	cmdbuf_cpuvaddr = dma_alloc_attrs(se_dev->dev->parent, SZ_4K,
					  &cmdbuf_iova, GFP_KERNEL,
					  0);
	if (!cmdbuf_cpuvaddr) {
		dev_err(se_dev->dev, "%s: dma_alloc_attrs failed\n", __func__);
		err = -ENOMEM;
		goto unmap_dst;
	}

	cmdbuf_cpuvaddr[i++] = __nvhost_opcode_nonincr(
			se_dev->opcode_addr + SE_RSA_OPERATION_OFFSET, 1);
	cmdbuf_cpuvaddr[i++] = SE_OPERATION_WRSTALL(WRSTALL_TRUE);

	val = SE_CONFIG_ENC_ALG(ALG_RSA) | SE_CONFIG_DEC_ALG(ALG_NOP) |
		SE_CONFIG_DST(DST_MEMORY);
	cmdbuf_cpuvaddr[i++] = __nvhost_opcode_incr(se_dev->opcode_addr, 8);
	cmdbuf_cpuvaddr[i++] = val;
	cmdbuf_cpuvaddr[i++] = RSA_KEY_SLOT(dh_ctx->slot->slot_num);
	cmdbuf_cpuvaddr[i++] = (dh_ctx->p_size / 64) - 1;
	cmdbuf_cpuvaddr[i++] = (dh_ctx->key_size / 4);
	cmdbuf_cpuvaddr[i++] = (u32)(se_dev->src_ll->addr);
	cmdbuf_cpuvaddr[i++] = (u32)(SE_ADDR_HI_MSB(MSB(se_dev->src_ll->addr)) |
				SE_ADDR_HI_SZ(se_dev->src_ll->data_len));
	cmdbuf_cpuvaddr[i++] = (u32)(se_dev->dst_ll->addr);
	cmdbuf_cpuvaddr[i++] = (u32)(SE_ADDR_HI_MSB(MSB(se_dev->dst_ll->addr)) |
				SE_ADDR_HI_SZ(se_dev->dst_ll->data_len));

	cmdbuf_cpuvaddr[i++] = __nvhost_opcode_nonincr(
			se_dev->opcode_addr + SE_RSA_OPERATION_OFFSET, 1);
	cmdbuf_cpuvaddr[i++] = SE_OPERATION_WRSTALL(WRSTALL_TRUE) |
				SE_OPERATION_LASTBUF(LASTBUF_TRUE) |
				SE_OPERATION_OP(OP_START);

	cmdbuf_num_words = i;

	err = tegra_se_channel_submit_gather(
				se_dev, cmdbuf_cpuvaddr, cmdbuf_iova, 0,
				cmdbuf_num_words, NONE);
	if (err) {
		dev_err(se_dev->dev, "%s: channel_submit failed\n", __func__);
		goto exit;
	}

	tegra_se_fix_endianness(se_dev, req->dst, num_dst_sgs,
				req->dst_len, false);
exit:
	dma_free_attrs(se_dev->dev->parent, SZ_4K, cmdbuf_cpuvaddr,
		       cmdbuf_iova, 0);
unmap_dst:
	tegra_unmap_sg(se_dev->dev, req->dst, DMA_FROM_DEVICE, req->dst_len);
unmap_src:
	tegra_unmap_sg(se_dev->dev, src_sg, DMA_TO_DEVICE, total);
free:
	if (!req->src)
		devm_kfree(se_dev->dev, base_buff);

	return err;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
static int tegra_se_dh_set_secret(struct crypto_kpp *tfm,
				  const void *buf,
				  unsigned int len)
#else
static int tegra_se_dh_set_secret(struct crypto_kpp *tfm, void *buf,
				  unsigned int len)
#endif
{
	int ret = 0;

	struct tegra_se_dh_context *ctx = tegra_se_dh_get_ctx(tfm);
	struct dh params;

	ctx->se_dev = se_devices[SE_RSA];

	ret = crypto_dh_decode_key(buf, len, &params);
	if (ret) {
		dev_err(ctx->se_dev->dev, "failed to decode DH input\n");
		return ret;
	}

	ret = tegra_se_dh_set_params(ctx, &params);
	if (ret)
		return ret;

	ret = tegra_se_dh_setkey(tfm);
	if (ret)
		return ret;

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
static unsigned int tegra_se_dh_max_size(struct crypto_kpp *tfm)
#else
static int tegra_se_dh_max_size(struct crypto_kpp *tfm)
#endif
{
	struct tegra_se_dh_context *ctx = tegra_se_dh_get_ctx(tfm);

	return ctx->p_size;
}

static void tegra_se_dh_exit_tfm(struct crypto_kpp *tfm)
{
	struct tegra_se_dh_context *ctx = tegra_se_dh_get_ctx(tfm);

	tegra_se_rsa_free_key_slot(ctx->slot);

	ctx->key = NULL;
	ctx->p = NULL;
	ctx->g = NULL;
}

static int tegra_se_aes_ccm_setkey(struct crypto_aead *tfm, const u8 *key,
					unsigned int keylen)
{
	struct tegra_se_aes_ccm_ctx *ctx = crypto_aead_ctx(tfm);
	struct tegra_se_dev *se_dev;
	struct tegra_se_slot *pslot;
	u32 *cpuvaddr = NULL;
	dma_addr_t iova = 0;
	int ret = 0;
	unsigned int index = 0;

	se_dev = se_devices[SE_AEAD];

	mutex_lock(&se_dev->mtx);

	/* Check for valid arguments */
	if (!ctx || !key) {
		dev_err(se_dev->dev, "invalid context or key");
		ret = -EINVAL;
		goto free_ctx;
	}

	if ((keylen != TEGRA_SE_KEY_128_SIZE) &&
	    (keylen != TEGRA_SE_KEY_192_SIZE) &&
	    (keylen != TEGRA_SE_KEY_256_SIZE)) {
		dev_err(se_dev->dev, "invalid key size");
		ret = -EINVAL;
		goto free_ctx;
	}

	/* Get free key slot */
	pslot = tegra_se_alloc_key_slot();
	if (!pslot) {
		dev_err(se_dev->dev, "no free key slot\n");
		ret = -ENOMEM;
		goto free_ctx;
	}
	ctx->slot = pslot;
	ctx->keylen = keylen;

	/* Get free command buffer */
	ret = tegra_se_get_free_cmdbuf(se_dev);
	if (ret < 0) {
		dev_err(se_dev->dev, "Couldn't get free cmdbuf\n");
		goto keyslt_free;
	}

	index = ret;

	cpuvaddr = se_dev->cmdbuf_addr_list[index].cmdbuf_addr;
	iova = se_dev->cmdbuf_addr_list[index].iova;
	atomic_set(&se_dev->cmdbuf_addr_list[index].free, 0);
	se_dev->cmdbuf_list_entry = index;

	ret = tegra_se_send_key_data(
		se_dev, (u8 *)key, keylen, ctx->slot->slot_num,
		SE_KEY_TABLE_TYPE_KEY, se_dev->opcode_addr,
		cpuvaddr, iova, NONE);

keyslt_free:
	tegra_se_free_key_slot(ctx->slot);

free_ctx:
	mutex_unlock(&se_dev->mtx);

	return ret;
}

static int tegra_se_aes_ccm_setauthsize(struct crypto_aead *tfm,
			unsigned int authsize)
{
	struct tegra_se_aes_ccm_ctx *ctx = crypto_aead_ctx(tfm);

	switch (authsize) {
	case 4:
	case 8:
	case 10:
	case 12:
	case 14:
	case 16:
		ctx->authsize = authsize;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static inline int tegra_se_ccm_check_iv(const u8 *iv)
{
	/* iv[0] gives value of q-1
	 * 2 <= q <= 8 as per NIST 800-38C notation
	 * 2 <= L <= 8, so 1 <= L' <= 7. as per rfc 3610 notation
	 */
	if (iv[0] < 1 || iv[0] > 7) {
		pr_err("ccm_ccm_check_iv failed %d\n", iv[0]);
		return -EINVAL;
	}

	return 0;
}

static int tegra_se_ccm_init_crypt(struct aead_request *req)
{
	u8 *iv = req->iv;
	int err;

	err = tegra_se_ccm_check_iv(iv);
	if (err)
		return err;

	/* Note: rfc 3610 and NIST 800-38C require counter (ctr_0) of
	 * zero to encrypt auth tag.
	 * req->iv has the formatted ctr_0 (i.e. Flags || N || 0).
	 */
	memset(iv + 15 - iv[0], 0, iv[0] + 1);

	return 0;
}

static int ccm_set_msg_len(u8 *block, unsigned int msglen, int csize)
{
	__be32 data;

	memset(block, 0, csize);
	block += csize;

	if (csize >= 4)
		csize = 4;
	else if (msglen > (1 << (8 * csize)))
		return -EOVERFLOW;

	data = cpu_to_be32(msglen);
	memcpy(block - csize, (u8 *)&data + 4 - csize, csize);

	return 0;
}

/**
 * @brief Encode B0 block as per below format (16 bytes):
 *
 * +----------------------------------------------------+
 * | Octet # |  0     |  [1 .. (15-q)] | [(16-q) .. 15] |
 * +----------------------------------------------------+
 * | Content | Flags  |      N         |      Q         |
 * +----------------------------------------------------+
 *
 * 1. "Flags" octet format (byte 0 of big endian B0):
 *
 * +-------------------------------------------------------------------------+
 * | Bit no. |  7     | 6     | 5     | 4     | 3     | 2     | 1     | 0    |
 * +-------------------------------------------------------------------------+
 * | Content |reserved| Adata |      (t-2)/2          |         q-1          |
 * +-------------------------------------------------------------------------+
 *
 * Bit  7      => always zero
 * Bit  6      => flag for AAD presence
 * Bits [5..3] => one of the valid values of element t (mac length)
 * Bits [2..0] => one of the valid values of element q (bytes needs to encode
 *		  message length), already present in iv[0].
 *
 * 2. N - nounce present in req->iv.
 *
 * 3. Q - contains the msg length(in bytes) in q octet/bytes space.
 *
 */
static int ccm_encode_b0(u8 *cinfo, struct aead_request *req,
	unsigned int cryptlen)
{
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct tegra_se_aes_ccm_ctx *ctx = crypto_aead_ctx(tfm);
	unsigned int q, t;
	u8 *Q_ptr;
	int ret;

	memcpy(cinfo, req->iv, 16);

	/*** 1. Prepare Flags Octet ***/

	/* Adata */
	if (req->assoclen)
		*cinfo |= (1 << 6);

	/* Encode t (mac length) */
	t = ctx->authsize;
	*cinfo |= (((t-2)/2) << 3);

	/* q already present in iv[0], do nothing */

	/*** 2. N is already present in iv, do nothing ***/

	/*** 3. Encode Q - message length ***/
	q = req->iv[0] + 1;
	Q_ptr = cinfo + 16 - q;

	ret = ccm_set_msg_len(Q_ptr, cryptlen, q);

	return ret;
}

/**
 * @brief Encode AAD length
 *
 * Function returns the number of bytes used in encoding,
 * the specs set the AAD length encoding limits as follows:
 *
 * (0)          <  a < (2^16-2^8) => two bytes, no prefix
 * (2^16 - 2^8) <= a < (2^32)     => six bytes, prefix: 0xFF || 0xFE
 *
 * Note that zero length AAD is not encoded at all; bit 6
 * in B0 flags is zero if there is no AAD.
 */
static int ccm_encode_adata_len(u8 *adata, unsigned int a)
{
	int len = 0;

	/* add control info for associated data
	 * RFC 3610 and NIST Special Publication 800-38C
	 */

	if (a < 65280) {
		*(__be16 *)adata = cpu_to_be16(a);
		len = 2;
	} else {
		*(__be16 *)adata = cpu_to_be16(0xfffe);
		*(__be32 *)&adata[2] = cpu_to_be32(a);
		len = 6;
	}

	return len;
}

static int tegra_se_ccm_compute_auth(struct aead_request *req, bool encrypt)
{
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct tegra_se_aes_ccm_ctx *ctx = crypto_aead_ctx(tfm);
	struct tegra_se_req_context *req_ctx = aead_request_ctx(req);
	unsigned int assoclen, cryptlen;
	struct scatterlist *sg, *sg_start;
	unsigned int ilen, pad_bytes_len, pt_bytes = 0, total = 0;
	u32 num_sgs, mapped_len = 0, index, *cpuvaddr = NULL;
	struct tegra_se_dev *se_dev;
	struct tegra_se_ll *src_ll;
	dma_addr_t pt_addr, adata_addr, iova = 0;
	u8 *buf, *adata = NULL;
	int ret, count;

	se_dev = se_devices[SE_AEAD];

	/* 0 - Setup */
	assoclen = req->assoclen;
	sg = req->src;
	if (encrypt)
		cryptlen = req->cryptlen;
	else
		cryptlen = req->cryptlen - ctx->authsize;

	/* 1 - Formatting of the Control Information and the Nonce */
	buf = ctx->buf[0];
	ret = ccm_encode_b0(buf, req, cryptlen);
	if (ret)
		return -EINVAL;

	/* 1.1 - Map B0 block */
	se_dev->src_ll = (struct tegra_se_ll *)(se_dev->src_ll_buf);
	src_ll = se_dev->src_ll;
	src_ll->addr = ctx->buf_addr[0];
	src_ll->data_len = 16;
	src_ll++;
	total += 16;

	/* 2 - Formatting of the Associated Data */
	if (assoclen) {
		buf = ctx->buf[1];

		/* 2.0 - Prepare alen encoding */
		ilen = ccm_encode_adata_len(buf, assoclen);

		/* 2.1 - Map alen encoding */
		src_ll->addr = ctx->buf_addr[1];
		src_ll->data_len = ilen;
		src_ll++;
		total += ilen;

		/* 2.2 - Copy adata and map it */
		adata = dma_alloc_coherent(se_dev->dev, assoclen, &adata_addr,
					(req->base.flags & CRYPTO_TFM_REQ_MAY_SLEEP) ?
					GFP_KERNEL : GFP_ATOMIC);
		num_sgs = tegra_se_count_sgs(sg, assoclen);
		sg_copy_to_buffer(sg, num_sgs, adata, assoclen);

		src_ll->addr = adata_addr;
		src_ll->data_len = assoclen;
		src_ll++;
		total += assoclen;

		/* 2.3 - Prepare and Map padding zero bytes data  */
		pad_bytes_len = 16 - (assoclen + ilen) % 16;
		if (pad_bytes_len) {
			memset(ctx->buf[2], 0, pad_bytes_len);
			src_ll->addr = ctx->buf_addr[2];
			src_ll->data_len = pad_bytes_len;
			src_ll++;
			total += pad_bytes_len;
		}
	}

	/* 3 - Formatting of plain text */
	/* In case of ccm decryption plain text is present in dst buffer */
	sg = encrypt ? req->src : req->dst;
	sg_start = sg;

	/* 3.1 Iterative over sg, to skip associated data */
	count = assoclen;
	while (count) {
		/* If a SG has both assoc data and PT data
		 * make note of it and use for PT mac computation task.
		 */
		if (count < sg->length) {
			ret = dma_map_sg(se_dev->dev, sg, 1, DMA_TO_DEVICE);
			if (!ret) {
				dev_err(se_dev->dev, "dma_map_sg error\n");
				goto out;
			}
			pt_addr = sg_dma_address(sg) + count;
			/* SG length can be more than cryptlen and associated
			 * data, in this case encrypted length should be
			 * cryptlen.
			 */
			if (cryptlen + count <= sg->length)
				pt_bytes = cryptlen;
			else
				pt_bytes = sg->length - count;

			sg_start = sg;
			mapped_len = sg->length;
			sg = sg_next(sg);
			break;
		}
		count -= min_t(size_t, (size_t)sg->length,
					(size_t)count);
		sg = sg_next(sg);
	}

	/* 3.2 Map the plain text buffer */
	if (pt_bytes) {
		src_ll->addr = pt_addr;
		src_ll->data_len = pt_bytes;
		src_ll++;
	}

	count = cryptlen - pt_bytes;
	while (count) {
		ret = dma_map_sg(se_dev->dev, sg, 1, DMA_TO_DEVICE);
		if (!ret) {
			dev_err(se_dev->dev, "dma_map_sg  error\n");
			goto out;
		}
		src_ll->addr = sg_dma_address(sg);
		src_ll->data_len = min_t(size_t,
					(size_t)sg->length, (size_t)count);
		count -= min_t(size_t, (size_t)sg->length, (size_t)count);
		mapped_len += sg->length;
		sg = sg_next(sg);
		src_ll++;
	}
	total += cryptlen;

	/* 3.3 - Map padding data */
	pad_bytes_len = 16 - (cryptlen % 16);
	if (pad_bytes_len) {
		memset(ctx->buf[3], 0, pad_bytes_len);
		src_ll->addr = ctx->buf_addr[3];
		src_ll->data_len = pad_bytes_len;
		src_ll++;
	}
	total += pad_bytes_len;

	/* 4 - Compute CBC_MAC */
	/* 4.1 - Get free command buffer */
	ret = tegra_se_get_free_cmdbuf(se_dev);
	if (ret < 0) {
		dev_err(se_dev->dev, "Couldn't get free cmdbuf\n");
		goto out;
	}

	index = ret;

	cpuvaddr = se_dev->cmdbuf_addr_list[index].cmdbuf_addr;
	iova = se_dev->cmdbuf_addr_list[index].iova;
	atomic_set(&se_dev->cmdbuf_addr_list[index].free, 0);
	se_dev->cmdbuf_list_entry = index;
	se_dev->dst_ll = se_dev->src_ll; /* dummy */

	req_ctx->op_mode = SE_AES_OP_MODE_CBC_MAC;
	req_ctx->config = tegra_se_get_config(se_dev, req_ctx->op_mode,
					0, ctx->keylen);
	req_ctx->crypto_config = tegra_se_get_crypto_config(
				se_dev, req_ctx->op_mode, 0,
				ctx->slot->slot_num, 0, true);

	tegra_se_send_data(se_dev, req_ctx, NULL, total, se_dev->opcode_addr,
			cpuvaddr);
	ret = tegra_se_channel_submit_gather(se_dev, cpuvaddr, iova,
			0, se_dev->cmdbuf_cnt, NONE);

	/* 4.1 - Read result */
	ret = tegra_se_read_cmac_result(se_dev, ctx->mac,
				TEGRA_SE_AES_CBC_MAC_DIGEST_SIZE, false);
	if (ret) {
		dev_err(se_dev->dev, "failed to read cmac result\n");
		goto out;
	}

	ret = tegra_se_clear_cmac_result(se_dev,
			TEGRA_SE_AES_CBC_MAC_DIGEST_SIZE);
	if (ret) {
		dev_err(se_dev->dev, "failed to clear cmac result\n");
		goto out;
	}

	/* 5 - Clean up */
	tegra_unmap_sg(se_dev->dev, sg_start, DMA_TO_DEVICE, mapped_len);
out:
	if (assoclen)
		dma_free_coherent(se_dev->dev, assoclen, adata, adata_addr);

	return ret;
}

static int ccm_ctr_extract_encrypted_mac(struct aead_request *req)
{
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct tegra_se_aes_ccm_ctx *ctx = crypto_aead_ctx(tfm);
	u32 num_sgs;

	/* Extract cipher mac from buf */
	memset(ctx->enc_mac, 0, 16); /* Padding zeros will be taken care */

	num_sgs = tegra_se_count_sgs(req->src, req->cryptlen + req->assoclen);
	sg_pcopy_to_buffer(req->src, num_sgs, ctx->enc_mac,
			ctx->authsize,
			req->cryptlen + req->assoclen - ctx->authsize);

	return 0;
}

static int tegra_se_ccm_ctr(struct aead_request *req, bool encrypt)
{
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct tegra_se_aes_ccm_ctx *ctx = crypto_aead_ctx(tfm);
	struct tegra_se_req_context *req_ctx = aead_request_ctx(req);
	struct tegra_se_ll *src_ll, *dst_ll;
	struct scatterlist *src_sg, *dst_sg, *src_sg_start;
	unsigned int assoclen, cryptlen, total, pad_bytes_len;
	unsigned int mapped_cryptlen, mapped_len;
	int ret = 0, count, index = 0, num_sgs;
	struct tegra_se_dev *se_dev;
	u8 *dst_buf;
	dma_addr_t dst_buf_dma_addr;

	se_dev = se_devices[SE_AEAD];

	/* 0. Setup */
	assoclen = req->assoclen;
	cryptlen = req->cryptlen;
	se_dev->src_ll = (struct tegra_se_ll *)(se_dev->src_ll_buf);
	se_dev->dst_ll = (struct tegra_se_ll *)(se_dev->dst_ll_buf);
	src_ll = se_dev->src_ll;
	dst_ll = se_dev->dst_ll;
	total = 0; mapped_cryptlen = 0; mapped_len = 0;
	if (encrypt)
		cryptlen = req->cryptlen;
	else
		cryptlen = req->cryptlen - ctx->authsize;

	/* If destination buffer is scattered over multiple sg, destination
	 * buffer splits need not be same as src_sg so create
	 * separate buffer for destination. Onebyte extra to handle zero
	 * cryptlen case.
	 */
	dst_buf = dma_alloc_coherent(se_dev->dev, cryptlen+1, &dst_buf_dma_addr,
		(req->base.flags & CRYPTO_TFM_REQ_MAY_SLEEP) ? GFP_KERNEL : GFP_ATOMIC);
	if (!dst_buf)
		return -ENOMEM;


	/* 1. Add mac to src */
	if (encrypt) {
		/* 1.1 Add mac to src_ll list */
		src_ll->addr = ctx->mac_addr;
		src_ll->data_len = 16;
		src_ll++;
		total += 16;

		/* 1.2 Add corresponding entry to dst_ll list */
		dst_ll->addr = ctx->enc_mac_addr;
		dst_ll->data_len = 16;
		dst_ll++;
	} else {
		/* 1.0 Extract encrypted mac to ctx->enc_mac */
		ccm_ctr_extract_encrypted_mac(req);

		/* 1.1 Add encrypted mac to src_ll list */
		src_ll->addr = ctx->enc_mac_addr;
		src_ll->data_len = 16;
		src_ll++;
		total += 16;

		/* 1.2 Add corresponding entry to dst_ll list */
		memset(ctx->dec_mac, 0, 16);
		dst_ll->addr = ctx->dec_mac_addr;
		dst_ll->data_len = 16;
		dst_ll++;
	}

	/* 2. Add plain text to src */
	src_sg = req->src;
	src_sg_start = src_sg;
	dst_sg = req->dst;
	/* 2.1 Iterative over assoclen, to skip it for encryption */
	count = assoclen;
	while (count) {
		/* If a SG has both assoc data and PT/CT data */
		if (count < src_sg->length) {
			ret = dma_map_sg(se_dev->dev, src_sg, 1,
						DMA_TO_DEVICE);
			if (!ret) {
				pr_err("dma_map_sg error\n");
				goto out;
			}

			/* Fill PT/CT src list details from this sg */
			src_ll->addr = sg_dma_address(src_sg) + count;
			/* SG length can be more than cryptlen and associated
			 * data, in this case encrypted length should be
			 * cryptlen.
			 */
			if (cryptlen + count <= src_sg->length)
				src_ll->data_len = cryptlen;
			else
				src_ll->data_len = src_sg->length - count;
			mapped_cryptlen = src_ll->data_len;
			mapped_len = src_sg->length;

			/* Fill dst list details */
			dst_ll->addr = dst_buf_dma_addr;
			dst_ll->data_len = src_ll->data_len;
			index += src_ll->data_len;
			dst_ll++;
			src_ll++;

			/* Store from where src_sg is mapped, so it can be
			 * unmapped.
			 */
			src_sg_start = src_sg;
			src_sg = sg_next(src_sg);
			break;
		}

		/* If whole SG is associated data iterate over it */
		count -= min_t(size_t, (size_t)src_sg->length, (size_t)count);
		src_sg = sg_next(src_sg);
	}

	/* 2.2 Add plain text to src_ll and dst_ll list */
	count = cryptlen - mapped_cryptlen;
	while (count) {
		ret = dma_map_sg(se_dev->dev, src_sg, 1, DMA_TO_DEVICE);
		if (!ret) {
			dev_err(se_dev->dev, "dma_map_sg  error\n");
			goto out;
		}
		src_ll->addr = sg_dma_address(src_sg);
		src_ll->data_len = min_t(size_t,
					(size_t)src_sg->length, (size_t)count);
		dst_ll->addr = dst_buf_dma_addr + index;
		dst_ll->data_len = src_ll->data_len;
		index += dst_ll->data_len;
		count -= min_t(size_t, (size_t)src_sg->length, (size_t)count);
		mapped_len += src_sg->length;
		src_sg = sg_next(src_sg);
		src_ll++;
		dst_ll++;
	}
	total += cryptlen;

	/* 3. Pad necessary zeros */
	memset(ctx->buf[0], 0, 16);
	pad_bytes_len = 16 - (cryptlen % 16);
	if (pad_bytes_len) {
		src_ll->addr = ctx->buf_addr[0];
		src_ll->data_len = pad_bytes_len;
		src_ll++;
		dst_ll->addr = ctx->buf_addr[0];
		dst_ll->data_len = pad_bytes_len;
		dst_ll++;
	}
	total += pad_bytes_len;

	/* 4. Encrypt/Decrypt using CTR */
	req_ctx->op_mode = SE_AES_OP_MODE_CTR;
	req_ctx->config = tegra_se_get_config(se_dev, req_ctx->op_mode,
					0, ctx->keylen);
	req_ctx->crypto_config = tegra_se_get_crypto_config(
				se_dev, req_ctx->op_mode, 0,
				ctx->slot->slot_num, 0, true);

	tegra_se_send_ctr_seed(se_dev, (u32 *)req->iv, se_dev->opcode_addr,
				se_dev->aes_cmdbuf_cpuvaddr);

	tegra_se_send_data(se_dev, req_ctx, NULL, total, se_dev->opcode_addr,
			se_dev->aes_cmdbuf_cpuvaddr);
	ret = tegra_se_channel_submit_gather(se_dev,
			se_dev->aes_cmdbuf_cpuvaddr, se_dev->aes_cmdbuf_iova,
			0, se_dev->cmdbuf_cnt, NONE);

	num_sgs = tegra_se_count_sgs(req->dst, assoclen + cryptlen);
	sg_pcopy_from_buffer(req->dst, num_sgs, dst_buf, cryptlen,
		assoclen);
out:
	/* 5 Clean up */
	tegra_unmap_sg(se_dev->dev, src_sg_start, DMA_TO_DEVICE, mapped_len);
	dma_free_coherent(se_dev->dev, cryptlen+1, dst_buf, dst_buf_dma_addr);

	return ret;
}

static void ccm_ctr_add_encrypted_mac_to_dest(struct aead_request *req)
{
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct tegra_se_aes_ccm_ctx *ctx = crypto_aead_ctx(tfm);
	struct scatterlist *dst_sg;
	u32 num_sgs;

	dst_sg = req->dst;
	num_sgs = tegra_se_count_sgs(dst_sg,
			req->assoclen + req->cryptlen + ctx->authsize);
	sg_pcopy_from_buffer(dst_sg, num_sgs, ctx->enc_mac,
				ctx->authsize, req->assoclen + req->cryptlen);

}

/*
 * CCM Generation-Encryption operation.
 *
 * Input:
 * valid nonce N of length Nlen bits or n octet length.
 * valid payload P of length Plen bits.
 * valid associated data A of length Alen bits or a octet length.
 *
 * Output:
 * ciphertext C.
 * authentication tag T of length Tlen bits or t octet length.
 *
 * Steps:
 * 1. Apply the formatting function to (N, A, P) to produce the blocks
 *    B0, B1, B2, ..., Br. Each block is 16bytes and 128bit length.
 * 2. Set Y0 = CIPH_K(B0).
 * 3. For i = 1 to r, do Yi = CIPH_K(Bi ⊕ Yi-1).
 * 4. Set T = MSBTlen(Yr).
 * 5. Apply the counter generation function to generate the counter
 *    blocks  Ctr0, Ctr1,..., Ctrm, where m = ⎡Plen/128⎤.
 * 6. For j = 0 to m, do Sj = CIPH_K(Ctrj).
 * 7. Set S = S1 || S2 || ...|| Sm.
 * 8. Return C = (P ⊕ MSBPlen(S)) || (T ⊕ MSBTlen(S0)).
 *
 * req->iv has the formatted ctr0 (i.e. Flags || N || 0) as per
 * NIST 800-38C and rfc 3610 notation.
 */
static int tegra_se_aes_ccm_encrypt(struct aead_request *req)
{
	struct tegra_se_dev *se_dev;
	int err;

	se_dev = se_devices[SE_AEAD];

	mutex_lock(&se_dev->mtx);
	err = tegra_se_ccm_init_crypt(req);
	if (err)
		goto out;

	err = tegra_se_ccm_compute_auth(req, ENCRYPT);
	if (err)
		goto out;

	err = tegra_se_ccm_ctr(req, ENCRYPT);
	if (err)
		goto out;

	ccm_ctr_add_encrypted_mac_to_dest(req);
out:
	mutex_unlock(&se_dev->mtx);

	return err;
}

/*
 * CCM Decryption-Verification operation.
 *
 * Input:
 * valid nonce N of length Nlen bits or n octet length.
 * valid associated data A of length Alen bits or a octet length.
 * purported ciphertext C (contains MAC of length Tlen) of length Clen bits.
 *
 * Output:
 * either the payload P or INVALID.
 *
 * Steps:
 *  1. If Clen ≤ Tlen, then return INVALID.
 *  2. Apply the counter generation function to generate the counter
 *     blocks  Ctr0, Ctr1,..., Ctrm, where m = ⎡(Clen-Tlen)/128⎤.
 *  3. For j = 0 to m, do Sj = CIPH_K(Ctrj).
 *  4. Set S = S1 || S2 || ...|| Sm.
 *  5. Set P = MSB(Clen-Tlen)(C) ⊕ MSB(Clen-Tlen)(S).
 *  6. Set T = LSB(Tlen)(C) ⊕ MSB(Tlen)(S0).
 *  7. If N, A, or P is not valid, then return INVALID, else apply the
 *     formatting function to (N, A, P) to produce the blocks B0, B1,...,Br.
 *  8. Set Y0 = CIPH_K(B0).
 *  9. For i = 1 to r, do Yi = CIPH_K(Bi ⊕ Yi-1).
 * 10. If T ≠ MSBTlen(Yr), then return INVALID, else return P.
 *
 * req->iv has the formatted ctr0 (i.e. Flags || N || 0) as per
 * NIST 800-38C and rfc 3610 notation.
 */
static int tegra_se_aes_ccm_decrypt(struct aead_request *req)
{
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct tegra_se_aes_ccm_ctx *ctx = crypto_aead_ctx(tfm);
	struct tegra_se_dev *se_dev;
	int err;

	se_dev = se_devices[SE_AEAD];

	mutex_lock(&se_dev->mtx);
	err = tegra_se_ccm_init_crypt(req);
	if (err)
		goto out;

	err = tegra_se_ccm_ctr(req, DECRYPT);
	if (err)
		goto out;

	err = tegra_se_ccm_compute_auth(req, DECRYPT);
	if (err)
		goto out;

	if (crypto_memneq(ctx->mac, ctx->dec_mac, ctx->authsize))
		err = -EBADMSG;
out:
	mutex_unlock(&se_dev->mtx);
	return err;
}

static int tegra_se_aes_ccm_init(struct crypto_aead *tfm)
{
	struct tegra_se_aes_ccm_ctx *ctx = crypto_aead_ctx(tfm);
	struct tegra_se_dev *se_dev = se_devices[SE_AEAD];
	int i = 0, err = 0;

	crypto_aead_set_reqsize(tfm, sizeof(struct tegra_se_req_context));

	mutex_lock(&se_dev->mtx);

	for (i = 0; i < 4; i++) {
		ctx->buf[i] = dma_alloc_coherent(
			se_dev->dev, TEGRA_SE_AES_BLOCK_SIZE,
			&ctx->buf_addr[i], GFP_KERNEL);
		if (!ctx->buf[i]) {
			while (i > 0) {
				i--;
				dma_free_coherent(
					se_dev->dev, TEGRA_SE_AES_BLOCK_SIZE,
					ctx->buf[i], ctx->buf_addr[i]);
				ctx->buf[i] = NULL;
			}
			dev_err(se_dev->dev, "Cannot allocate memory to buf[0]\n");
			mutex_unlock(&se_dev->mtx);
			return -ENOMEM;
		}
	}

	ctx->mac = dma_alloc_coherent(se_dev->dev, TEGRA_SE_AES_BLOCK_SIZE,
			&ctx->mac_addr, GFP_KERNEL);
	if (!ctx->mac) {
		err = -ENOMEM;
		goto out;
	}

	ctx->enc_mac = dma_alloc_coherent(se_dev->dev, TEGRA_SE_AES_BLOCK_SIZE,
			&ctx->enc_mac_addr, GFP_KERNEL);
	if (!ctx->enc_mac) {
		err = -ENOMEM;
		goto out;
	}

	ctx->dec_mac = dma_alloc_coherent(se_dev->dev, TEGRA_SE_AES_BLOCK_SIZE,
			&ctx->dec_mac_addr, GFP_KERNEL);
	if (!ctx->dec_mac) {
		err = -ENOMEM;
		goto out;
	}
out:
	mutex_unlock(&se_dev->mtx);
	return err;
}

static void tegra_se_aes_ccm_exit(struct crypto_aead *tfm)
{
	struct tegra_se_aes_ccm_ctx *ctx = crypto_aead_ctx(tfm);
	struct tegra_se_dev *se_dev = se_devices[SE_AES];
	int i = 0;

	mutex_lock(&se_dev->mtx);
	tegra_se_free_key_slot(ctx->slot);
	ctx->slot = NULL;
	for (i = 0; i < 4; i++) {
		dma_free_coherent(
			se_dev->dev, (TEGRA_SE_AES_BLOCK_SIZE),
			ctx->buf[i], ctx->buf_addr[i]);
	}

	dma_free_coherent(se_dev->dev, (TEGRA_SE_AES_BLOCK_SIZE),
			ctx->mac, ctx->mac_addr);

	dma_free_coherent(se_dev->dev, (TEGRA_SE_AES_BLOCK_SIZE),
			ctx->enc_mac, ctx->enc_mac_addr);

	dma_free_coherent(se_dev->dev, (TEGRA_SE_AES_BLOCK_SIZE),
			ctx->dec_mac, ctx->dec_mac_addr);

	mutex_unlock(&se_dev->mtx);
}

static int tegra_se_aes_gcm_setkey(struct crypto_aead *tfm, const u8 *key,
					unsigned int keylen)
{
	struct tegra_se_aes_gcm_ctx *ctx = crypto_aead_ctx(tfm);
	struct tegra_se_dev *se_dev;
	struct tegra_se_slot *pslot;
	u32 *cpuvaddr = NULL;
	dma_addr_t iova = 0;
	int ret = 0;
	unsigned int index = 0;

	se_dev = se_devices[SE_AEAD];

	mutex_lock(&se_dev->mtx);

	/* Check for valid arguments */
	if (!ctx || !key) {
		dev_err(se_dev->dev, "invalid context or key");
		ret = -EINVAL;
		goto free_ctx;
	}

	if ((keylen != TEGRA_SE_KEY_128_SIZE) &&
	    (keylen != TEGRA_SE_KEY_192_SIZE) &&
	    (keylen != TEGRA_SE_KEY_256_SIZE)) {
		dev_err(se_dev->dev, "invalid key size");
		ret = -EINVAL;
		goto free_ctx;
	}

	/* Get free key slot */
	pslot = tegra_se_alloc_key_slot();
	if (!pslot) {
		dev_err(se_dev->dev, "no free key slot\n");
		ret = -ENOMEM;
		goto free_ctx;
	}
	ctx->slot = pslot;
	ctx->keylen = keylen;

	/* Get free command buffer */
	ret = tegra_se_get_free_cmdbuf(se_dev);
	if (ret < 0) {
		dev_err(se_dev->dev, "Couldn't get free cmdbuf\n");
		goto keyslt_free;
	}

	index = ret;

	cpuvaddr = se_dev->cmdbuf_addr_list[index].cmdbuf_addr;
	iova = se_dev->cmdbuf_addr_list[index].iova;
	atomic_set(&se_dev->cmdbuf_addr_list[index].free, 0);
	se_dev->cmdbuf_list_entry = index;

	ret = tegra_se_send_key_data(
		se_dev, (u8 *)key, keylen, ctx->slot->slot_num,
		SE_KEY_TABLE_TYPE_GCM, se_dev->opcode_addr,
		cpuvaddr, iova, NONE);

	atomic_set(&se_dev->cmdbuf_addr_list[index].free, 1);
keyslt_free:
	tegra_se_free_key_slot(ctx->slot);

free_ctx:
	mutex_unlock(&se_dev->mtx);

	return ret;
}

static int tegra_se_aes_gcm_setauthsize(struct crypto_aead *tfm,
				unsigned int authsize)
{
	struct tegra_se_aes_gcm_ctx *ctx = crypto_aead_ctx(tfm);

	switch (authsize) {
	case 4:
	case 8:
	case 10:
	case 12:
	case 14:
	case 16:
		ctx->authsize = authsize;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int tegra_se_gcm_gmac(struct aead_request *req, bool encrypt)
{
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct tegra_se_aes_gcm_ctx *ctx = crypto_aead_ctx(tfm);
	struct tegra_se_req_context *req_ctx = aead_request_ctx(req);
	unsigned int assoclen;
	struct tegra_se_dev *se_dev;
	struct tegra_se_ll *src_ll;
	struct scatterlist *sg;
	int ret, index;
	u32 *cpuvaddr = NULL;
	dma_addr_t iova = 0;

	/* 0 - Setup */
	se_dev = ctx->se_dev;
	assoclen = req->assoclen;
	sg = req->src;

	/* 1 - Map associated data */
	se_dev->src_ll = (struct tegra_se_ll *)(se_dev->src_ll_buf);
	src_ll = se_dev->src_ll;

	ret = tegra_map_sg(se_dev->dev, sg, 1, DMA_TO_DEVICE,
				se_dev->src_ll, assoclen);
	if (!ret)
		return ret;

	/* 2 - Prepare command buffer and submit request to hardware */
	/* 2.1 Get free command buffer */
	ret = tegra_se_get_free_cmdbuf(se_dev);
	if (ret < 0) {
		dev_err(se_dev->dev, "Couldn't get free cmdbuf\n");
		tegra_unmap_sg(se_dev->dev, sg, DMA_TO_DEVICE, assoclen);
		return -EBUSY;
	}

	index = ret;
	cpuvaddr = se_dev->cmdbuf_addr_list[index].cmdbuf_addr;
	iova = se_dev->cmdbuf_addr_list[index].iova;
	atomic_set(&se_dev->cmdbuf_addr_list[index].free, 0);
	se_dev->cmdbuf_list_entry = index;
	se_dev->dst_ll = se_dev->src_ll; /* dummy */

	/* 2.2 Prepare and submit request */
	req_ctx->config = tegra_se_get_config(se_dev, req_ctx->op_mode,
				encrypt ? ENCRYPT : DECRYPT, SE_AES_GMAC);
	req_ctx->crypto_config = tegra_se_get_crypto_config(
					se_dev, req_ctx->op_mode, 0,
					ctx->slot->slot_num, 0, true);

	tegra_se_send_gcm_data(se_dev, req_ctx, assoclen, se_dev->opcode_addr,
				cpuvaddr, SE_AES_GMAC);
	ret = tegra_se_channel_submit_gather(se_dev, cpuvaddr,
			iova, 0, se_dev->cmdbuf_cnt, NONE);

	/* 3 - Clean up */
	tegra_unmap_sg(se_dev->dev, sg, DMA_TO_DEVICE, assoclen);

	return ret;
}

static int tegra_se_gcm_op(struct aead_request *req, bool encrypt)
{
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct tegra_se_aes_gcm_ctx *ctx = crypto_aead_ctx(tfm);
	struct tegra_se_req_context *req_ctx = aead_request_ctx(req);
	unsigned int assoclen, cryptlen, mapped_cryptlen, mapped_len;
	struct tegra_se_ll *src_ll, *dst_ll;
	struct scatterlist *src_sg, *src_sg_start;
	struct tegra_se_dev *se_dev;
	u32 *cpuvaddr = NULL, num_sgs;
	dma_addr_t iova = 0;
	int ret, count, index;
	u32 iv[4];
	u8 *dst_buf;
	dma_addr_t dst_buf_dma_addr;

	/* 0 - Setup */
	se_dev = ctx->se_dev;
	se_dev->src_ll = (struct tegra_se_ll *)(se_dev->src_ll_buf);
	se_dev->dst_ll = (struct tegra_se_ll *)(se_dev->dst_ll_buf);
	src_ll = se_dev->src_ll;
	dst_ll = se_dev->dst_ll;
	mapped_cryptlen = 0;
	mapped_len = 0;
	ret = 0; index = 0;
	assoclen = req->assoclen;
	if (encrypt)
		cryptlen = req->cryptlen;
	else
		cryptlen = req->cryptlen - ctx->authsize;

	/* If destination buffer is scattered over multiple sg, destination
	 * buffer splits need not be same as src_sg so create
	 * separate buffer for destination. Onebyte extra to handle zero
	 * cryptlen case.
	 */
	dst_buf = dma_alloc_coherent(se_dev->dev, cryptlen+1, &dst_buf_dma_addr,
				    (req->base.flags & CRYPTO_TFM_REQ_MAY_SLEEP) ?
				    GFP_KERNEL : GFP_ATOMIC);
	if (!dst_buf)
		return -ENOMEM;

	/* 1. Add plain text to src */
	src_sg = req->src;
	src_sg_start = src_sg;
	/* 1.1 Iterative over assoclen, to skip it for encryption */
	count = assoclen;
	while (count) {
		/* If a SG has both assoc data and PT/CT data */
		if (count < src_sg->length) {
			ret = dma_map_sg(se_dev->dev, src_sg, 1,
						DMA_TO_DEVICE);
			if (!ret) {
				pr_err("%s:%d dma_map_sg error\n",
							__func__, __LINE__);
				goto free_dst_buf;
			}

			/* Fill PT/CT src list details from this sg */
			src_ll->addr = sg_dma_address(src_sg) + count;
			/* SG length can be more than cryptlen and associated
			 * data, in this case encrypted length should be
			 * cryptlen
			 */
			if (cryptlen + count <= src_sg->length)
				src_ll->data_len = cryptlen;
			else
				src_ll->data_len = src_sg->length - count;
			mapped_cryptlen = src_ll->data_len;
			mapped_len = src_sg->length;

			/* Fill dst list details */
			dst_ll->addr = dst_buf_dma_addr;
			dst_ll->data_len = src_ll->data_len;
			index += src_ll->data_len;
			dst_ll++;
			src_ll++;
			/* Store from where src_sg is mapped, so it can be
			 * unmapped.
			 */
			src_sg_start = src_sg;
			src_sg = sg_next(src_sg);
			break;
		}

		/* If whole SG is associated data iterate over it */
		count -= min_t(size_t, (size_t)src_sg->length, (size_t)count);
		src_sg = sg_next(src_sg);
	}

	/* 1.2 Add plain text to src_ll list */
	ret = tegra_map_sg(se_dev->dev, src_sg, 1, DMA_TO_DEVICE,
				src_ll, cryptlen - mapped_cryptlen);
	if (ret < 0)
		goto free_dst_buf;

	mapped_len += (cryptlen - mapped_cryptlen);

	/* 1.3 Fill dst_ll list */
	while (src_sg && cryptlen) {
		dst_ll->addr = dst_buf_dma_addr + index;
		dst_ll->data_len = src_ll->data_len;
		index += dst_ll->data_len;
		dst_ll++;
		src_ll++;
		src_sg = sg_next(src_sg);
	}

	/* 2. Encrypt/Decrypt using GCTR and perform GHASH on plain text */
	/* 2.1 Get free command buffer */
	ret = tegra_se_get_free_cmdbuf(se_dev);
	if (ret < 0) {
		dev_err(se_dev->dev, "Couldn't get free cmdbuf\n");
		goto out;
	}

	index = ret;

	cpuvaddr = se_dev->cmdbuf_addr_list[index].cmdbuf_addr;
	iova = se_dev->cmdbuf_addr_list[index].iova;
	atomic_set(&se_dev->cmdbuf_addr_list[index].free, 0);
	se_dev->cmdbuf_list_entry = index;

	/* 2.2 Prepare and program J0 (i.e. iv || 0^31 || 1) using IV */
	memset(iv, 0, 16);
	memcpy(iv, req->iv, 12);
	iv[3] = (1 << 24);
	tegra_se_send_ctr_seed(se_dev, iv, se_dev->opcode_addr, cpuvaddr);

	/* 2.3 Submit request */
	req_ctx->config = tegra_se_get_config(se_dev, req_ctx->op_mode,
				0, encrypt ? SE_AES_GCM_ENC : SE_AES_GCM_DEC);
	req_ctx->crypto_config = tegra_se_get_crypto_config(
					se_dev, req_ctx->op_mode, 0,
					ctx->slot->slot_num, 0, true);
	tegra_se_send_gcm_data(se_dev, req_ctx, cryptlen,
				se_dev->opcode_addr, cpuvaddr,
				encrypt ? SE_AES_GCM_ENC : SE_AES_GCM_DEC);

	ret = tegra_se_channel_submit_gather(se_dev, cpuvaddr,
			iova, 0, se_dev->cmdbuf_cnt, NONE);

out:
	/* 3 clean up */
	tegra_unmap_sg(se_dev->dev, src_sg_start, DMA_TO_DEVICE, mapped_len);
	num_sgs = tegra_se_count_sgs(req->dst, assoclen + cryptlen);
	sg_pcopy_from_buffer(req->dst, num_sgs, dst_buf, cryptlen,
							assoclen);
free_dst_buf:
	dma_free_coherent(se_dev->dev, cryptlen+1, dst_buf, dst_buf_dma_addr);

	return ret;
}

static void gcm_program_aad_msg_len(struct tegra_se_dev *se_dev,
			unsigned int alen, unsigned int clen,
			unsigned int opcode_addr, u32 *cpuvaddr)
{
	u32 i = 0;

	i = se_dev->cmdbuf_cnt;

	cpuvaddr[i++] = __nvhost_opcode_incr(opcode_addr +
				SE_AES_CRYPTO_AAD_LENGTH_0_OFFSET, 2);
	cpuvaddr[i++] = alen*8;
	cpuvaddr[i++] = 0;

	cpuvaddr[i++] = __nvhost_opcode_incr(opcode_addr +
				SE_AES_CRYPTO_MSG_LENGTH_0_OFFSET, 2);
	cpuvaddr[i++] = clen*8;
	cpuvaddr[i++] = 0;

	se_dev->cmdbuf_cnt = i;
}

static void gcm_add_encrypted_mac_to_dest(struct aead_request *req)
{
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct tegra_se_aes_gcm_ctx *ctx = crypto_aead_ctx(tfm);
	struct scatterlist *dst_sg;
	u32 num_sgs;

	dst_sg = req->dst;
	num_sgs = tegra_se_count_sgs(dst_sg,
			req->assoclen + req->cryptlen + ctx->authsize);
	sg_pcopy_from_buffer(dst_sg, num_sgs, ctx->mac,
				ctx->authsize, req->assoclen + req->cryptlen);

}

static int tegra_se_gcm_final(struct aead_request *req, bool encrypt)
{
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct tegra_se_aes_gcm_ctx *ctx = crypto_aead_ctx(tfm);
	struct tegra_se_req_context *req_ctx = aead_request_ctx(req);
	u32 *cpuvaddr = NULL;
	unsigned int cryptlen;
	dma_addr_t iova = 0;
	int ret, index, i;
	struct tegra_se_dev *se_dev;
	u32 iv[4], val;

	/* 1 Get free command buffer */
	se_dev = ctx->se_dev;
	ret = tegra_se_get_free_cmdbuf(se_dev);
	if (ret < 0) {
		dev_err(se_dev->dev, "Couldn't get free cmdbuf\n");
		return -EBUSY;
	}

	index = ret;

	cpuvaddr = se_dev->cmdbuf_addr_list[index].cmdbuf_addr;
	iova = se_dev->cmdbuf_addr_list[index].iova;
	atomic_set(&se_dev->cmdbuf_addr_list[index].free, 0);
	se_dev->cmdbuf_list_entry = index;

	/* 2. Configure AAD and MSG length */
	if (encrypt)
		cryptlen = req->cryptlen;
	else
		cryptlen = req->cryptlen - ctx->authsize;
	gcm_program_aad_msg_len(se_dev, req->assoclen, cryptlen,
			se_dev->opcode_addr, cpuvaddr);

	/* 3. Prepare and program J0 (i.e. iv || 0^31 || 1 ) using IV */
	memset(iv, 0, 16);
	memcpy(iv, req->iv, 12);
	iv[3] = (1 << 24);
	tegra_se_send_ctr_seed(se_dev, iv, se_dev->opcode_addr, cpuvaddr);

	/* 4. Program command buffer */
	req_ctx->op_mode = SE_AES_OP_MODE_GCM;
	req_ctx->config = tegra_se_get_config(se_dev, req_ctx->op_mode,
				encrypt ? ENCRYPT : DECRYPT, SE_AES_GCM_FINAL);
	req_ctx->crypto_config = tegra_se_get_crypto_config(
					se_dev, req_ctx->op_mode, 0,
					ctx->slot->slot_num, 0, true);
	i = se_dev->cmdbuf_cnt;

	/* 4.1 Config register */
	cpuvaddr[i++] = __nvhost_opcode_incr(se_dev->opcode_addr, 2);
	cpuvaddr[i++] = req_ctx->config;
	cpuvaddr[i++] = req_ctx->crypto_config;

	/* 4.2 Destination address */
	cpuvaddr[i++] = __nvhost_opcode_incr(
			se_dev->opcode_addr + SE_AES_OUT_ADDR_OFFSET, 2);
	cpuvaddr[i++] = (u32)(ctx->mac_addr);
	cpuvaddr[i++] = (u32)(SE_ADDR_HI_MSB(MSB(ctx->mac_addr)) |
				SE_ADDR_HI_SZ(SE_AES_GCM_GMAC_SIZE));

	/* 4.3 Handle zero size */
	cpuvaddr[i++] = __nvhost_opcode_nonincr(
			se_dev->opcode_addr + SE_AES_OPERATION_OFFSET, 1);
	val = SE_OPERATION_WRSTALL(WRSTALL_TRUE);
	/* If GMAC or GCM_ENC/GCM_DEC is not operated before then
	 * set SE_OPERATION.INIT to true.
	 */
	if (req_ctx->init != true)
		val |= SE_OPERATION_INIT(INIT_TRUE);
	val |= SE_OPERATION_FINAL(FINAL_TRUE);
	val |= SE_OPERATION_OP(OP_START);
	val |= SE_OPERATION_LASTBUF(LASTBUF_TRUE);

	cpuvaddr[i++] = val;
	se_dev->cmdbuf_cnt = i;

	/* 5 Submit request */
	ret = tegra_se_channel_submit_gather(se_dev, cpuvaddr,
			iova, 0, se_dev->cmdbuf_cnt, NONE);

	return ret;
}

/*
 * GCM encrypt operation.

 * Input:
 * initialization vector IV (whose length is supported).
 * plaintext P (whose length is supported).
 * additional authenticated data A (whose length is supported).

 * Output:
 * ciphertext C.
 * authentication tag T.

 * Steps:
 * 1. Let H = CIPH_K(0^128).
 * 2. Define a block, J0, as follows:
 *    If len(IV) = 96, then let J0 = IV || 0^31 || 1.
 *    If len(IV) ≠ 96, then let s = 128 * ⎡len(IV)/128⎤-len(IV),
 *		and let J0 = GHASH_H(IV || 0^(s+64) || [len(IV)]_64).
 * 3. Let C = GCTR_K(inc_32(J0), P).
 * 4. Let u = 128 * ⎡len(C)/128⎤-len(C) and
	  v = 128 * ⎡len(A)/128⎤-len(A)
 * 5. Define a block, S, as follows:
 *      S = GHASH_H (A || 0^v || C || 0^u || [len(A)]_64 || [len(C)]_64).
 * 6. Let T = MSB_t(GCTR_K(J0, S))
 * 7. Return (C, T).

 * SE hardware provide below 3 operations for above steps.
 * Op1: GMAC      - step 1 and partial of 5 (i.e. GHASH_H (A || 0^v)).
 * Op2: GCM_ENC   - step 1 to 4 and parital of 5 (i.e. GHASH_H (C || 0^u)).
 * Op3: GCM_FINAL - step partial of 5
 *		   (i.e. GHASH_H ([len(A)]_64 || [len(C)]_64) and 6.
 */

static int tegra_se_aes_gcm_encrypt(struct aead_request *req)
{
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct tegra_se_aes_gcm_ctx *ctx = crypto_aead_ctx(tfm);
	struct tegra_se_req_context *req_ctx = aead_request_ctx(req);
	int ret;

	ctx->se_dev = se_devices[SE_AEAD];
	req_ctx->init = false;
	req_ctx->op_mode = SE_AES_OP_MODE_GCM;

	mutex_lock(&ctx->se_dev->mtx);

	/* If there is associated date perform GMAC operation */
	if (req->assoclen) {
		ret = tegra_se_gcm_gmac(req, ENCRYPT);
		if (ret)
			goto out;
	}

	/* GMAC_ENC operation */
	if (req->cryptlen) {
		ret = tegra_se_gcm_op(req, ENCRYPT);
		if (ret)
			goto out;
	}

	/* GMAC_FINAL operation */
	ret = tegra_se_gcm_final(req, ENCRYPT);
	if (ret)
		goto out;

	/* Copy the mac to request destination */
	gcm_add_encrypted_mac_to_dest(req);
out:
	mutex_unlock(&ctx->se_dev->mtx);

	return ret;
}

/*
 * GCM decrypt operation.
 *
 * Input:
 * initialization vector IV (whose length is supported).
 * ciphertext C (whose length is supported).
 * additional authenticated data A (whose length is supported).
 * authentication tag T.
 *
 * Output:
 * plaintext P.
 * verify tag.
 *
 * Steps:
 * 1. If the bit lengths of IV, A or C are not supported, or if len(T) ≠t,
 *    then return FAIL.
 * 2. Let H = CIPH_K(0^128).
 * 3. Define a block, J0, as follows:
 *     If len(IV) = 96, then J0 = IV || 0^31 || 1.
 *     If len(IV) ≠ 96, then let s = 128 ⎡len(IV)/128⎤-len(IV),
 *		      and let J0 = GHASH_H(IV || 0^(s+64) || [len(IV)]_64).
 * 4. Let P = GCTR_K(inc_32(J0), C).
 * 5. Let u = 128 * ⎡len(C)/128⎤-len(C) and
	  v = 128 * ⎡len(A)/128⎤-len(A)
 * 6. Define a block, S, as follows:
 *      S = GHASH_H (A || 0^v || C || 0^u || [len(A)]_64 || [len(C)]_64)
 * 7. Let T′ = MSB_t(GCTR_K(J0, S))
 * 8. If T = T′, then return P; else return FAIL.
 *
 * SE hardware provide below 3 operations.
 * Op1: GMAC      - step 2 and partial of 6 (i.e. GHASH_H (A || 0^v)).
 * Op2: GCM_DEC   - step 2 to 4 and parital of 6 (i.e. GHASH_H (C || 0^u)).
 * Op3: GCM_FINAL - step partial of 5
 *		  (i.e. GHASH_H ([len(A)]_64 || [len(C)]_64) and 6.
 */
static int tegra_se_aes_gcm_decrypt(struct aead_request *req)
{
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct tegra_se_aes_gcm_ctx *ctx = crypto_aead_ctx(tfm);
	struct tegra_se_req_context *req_ctx = aead_request_ctx(req);
	u8 mac[16];
	struct scatterlist *sg;
	u32 num_sgs;
	int ret;

	ctx->se_dev = se_devices[SE_AEAD];
	req_ctx->init = false;
	req_ctx->op_mode = SE_AES_OP_MODE_GCM;

	mutex_lock(&ctx->se_dev->mtx);
	if (req->assoclen) {
		ret = tegra_se_gcm_gmac(req, DECRYPT);
		if (ret)
			goto out;
	}

	if (req->cryptlen - req->assoclen - ctx->authsize) {
		ret = tegra_se_gcm_op(req, DECRYPT);
		if (ret)
			goto out;
	}

	ret = tegra_se_gcm_final(req, DECRYPT);
	if (ret)
		goto out;

	/* Verify the mac */
	sg = req->src;
	num_sgs = tegra_se_count_sgs(sg, req->assoclen + req->cryptlen);
	sg_pcopy_to_buffer(sg, num_sgs, mac, ctx->authsize,
			req->assoclen + req->cryptlen - ctx->authsize);
	if (crypto_memneq(ctx->mac, mac, ctx->authsize))
		ret = -EBADMSG;
out:
	mutex_unlock(&ctx->se_dev->mtx);

	return ret;
}

static int tegra_se_aes_gcm_init(struct crypto_aead *tfm)
{
	struct tegra_se_aes_gcm_ctx *ctx = crypto_aead_ctx(tfm);
	struct tegra_se_dev *se_dev = se_devices[SE_AEAD];
	int ret = 0;

	crypto_aead_set_reqsize(tfm, sizeof(struct tegra_se_req_context));

	mutex_lock(&se_dev->mtx);
	ctx->mac = dma_alloc_coherent(se_dev->dev, TEGRA_SE_AES_BLOCK_SIZE,
			&ctx->mac_addr, GFP_KERNEL);
	if (!ctx->mac)
		ret = -ENOMEM;

	mutex_unlock(&se_dev->mtx);

	return ret;
}

static void tegra_se_aes_gcm_exit(struct crypto_aead *tfm)
{
	struct tegra_se_aes_gcm_ctx *ctx = crypto_aead_ctx(tfm);
	struct tegra_se_dev *se_dev = se_devices[SE_AEAD];

	mutex_lock(&se_dev->mtx);
	tegra_se_free_key_slot(ctx->slot);
	ctx->slot = NULL;

	dma_free_coherent(se_dev->dev, TEGRA_SE_AES_BLOCK_SIZE,
			ctx->mac, ctx->mac_addr);

	mutex_unlock(&se_dev->mtx);
}

static struct aead_alg aead_algs[] = {
	{
		.setkey		= tegra_se_aes_ccm_setkey,
		.setauthsize	= tegra_se_aes_ccm_setauthsize,
		.encrypt	= tegra_se_aes_ccm_encrypt,
		.decrypt	= tegra_se_aes_ccm_decrypt,
		.init		= tegra_se_aes_ccm_init,
		.exit		= tegra_se_aes_ccm_exit,
		.ivsize		= AES_BLOCK_SIZE,
		.maxauthsize	= AES_BLOCK_SIZE,
		.chunksize	= AES_BLOCK_SIZE,
		.base = {
			.cra_name	= "ccm(aes)",
			.cra_driver_name = "ccm-aes-tegra",
			.cra_priority	= 1000,
			.cra_blocksize	= TEGRA_SE_AES_BLOCK_SIZE,
			.cra_ctxsize	= sizeof(struct tegra_se_aes_ccm_ctx),
			.cra_module	= THIS_MODULE,
		}
	}, {
		.setkey		= tegra_se_aes_gcm_setkey,
		.setauthsize	= tegra_se_aes_gcm_setauthsize,
		.encrypt	= tegra_se_aes_gcm_encrypt,
		.decrypt	= tegra_se_aes_gcm_decrypt,
		.init		= tegra_se_aes_gcm_init,
		.exit		= tegra_se_aes_gcm_exit,
		.ivsize		= GCM_IV_SIZE,
		.maxauthsize	= AES_BLOCK_SIZE,
		.chunksize	= AES_BLOCK_SIZE,
		.base = {
			.cra_name	= "gcm(aes)",
			.cra_driver_name = "gcm-aes-tegra",
			.cra_priority	= 1000,
			.cra_blocksize	= TEGRA_SE_AES_BLOCK_SIZE,
			.cra_ctxsize	= sizeof(struct tegra_se_aes_gcm_ctx),
			.cra_module	= THIS_MODULE,
		}
	}
};

static struct kpp_alg dh_algs[] = {
	{
	.set_secret = tegra_se_dh_set_secret,
	.generate_public_key = tegra_se_dh_compute_value,
	.compute_shared_secret = tegra_se_dh_compute_value,
	.max_size = tegra_se_dh_max_size,
	.exit = tegra_se_dh_exit_tfm,
	.base = {
		.cra_name = "dh",
		.cra_driver_name = "tegra-se-dh",
		.cra_priority = 300,
		.cra_module = THIS_MODULE,
		.cra_ctxsize = sizeof(struct tegra_se_dh_context),
		}
	}
};

static struct rng_alg rng_algs[] = {
	{
		.generate = tegra_se_rng_drbg_get_random,
		.seed = tegra_se_rng_drbg_reset,
		.seedsize = TEGRA_SE_RNG_SEED_SIZE,
		.base = {
			.cra_name = "rng_drbg",
			.cra_driver_name = "rng_drbg-aes-tegra",
			.cra_priority = 100,
			.cra_flags = CRYPTO_ALG_TYPE_RNG,
			.cra_ctxsize = sizeof(struct tegra_se_rng_context),
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_rng_drbg_init,
			.cra_exit = tegra_se_rng_drbg_exit,
		}
	}
};

static struct skcipher_alg aes_algs[] = {
	{
		.base.cra_name		= "xts(aes)",
		.base.cra_driver_name	= "xts-aes-tegra",
		.base.cra_priority	= 500,
		.base.cra_flags		= CRYPTO_ALG_TYPE_SKCIPHER |
					  CRYPTO_ALG_ASYNC,
		.base.cra_blocksize	= TEGRA_SE_AES_BLOCK_SIZE,
		.base.cra_ctxsize	= sizeof(struct tegra_se_aes_context),
		.base.cra_alignmask	= 0,
		.base.cra_module	= THIS_MODULE,
		.init			= tegra_se_aes_cra_init,
		.exit			= tegra_se_aes_cra_exit,
		.setkey			= tegra_se_aes_setkey,
		.encrypt		= tegra_se_aes_xts_encrypt,
		.decrypt		= tegra_se_aes_xts_decrypt,
		.min_keysize		= TEGRA_SE_AES_MIN_KEY_SIZE,
		.max_keysize		= TEGRA_SE_AES_MAX_KEY_SIZE,
		.ivsize			= TEGRA_SE_AES_IV_SIZE,
	},
	{
		.base.cra_name		= "cbc(aes)",
		.base.cra_driver_name	= "cbc-aes-tegra",
		.base.cra_priority	= 500,
		.base.cra_flags		= CRYPTO_ALG_TYPE_SKCIPHER |
					  CRYPTO_ALG_ASYNC,
		.base.cra_blocksize	= TEGRA_SE_AES_BLOCK_SIZE,
		.base.cra_ctxsize	= sizeof(struct tegra_se_aes_context),
		.base.cra_alignmask	= 0,
		.base.cra_module	= THIS_MODULE,
		.init			= tegra_se_aes_cra_init,
		.exit			= tegra_se_aes_cra_exit,
		.setkey			= tegra_se_aes_setkey,
		.encrypt		= tegra_se_aes_cbc_encrypt,
		.decrypt		= tegra_se_aes_cbc_decrypt,
		.min_keysize		= TEGRA_SE_AES_MIN_KEY_SIZE,
		.max_keysize		= TEGRA_SE_AES_MAX_KEY_SIZE,
		.ivsize			= TEGRA_SE_AES_IV_SIZE,
	},
	{
		.base.cra_name		= "ecb(aes)",
		.base.cra_driver_name	= "ecb-aes-tegra",
		.base.cra_priority	= 500,
		.base.cra_flags		= CRYPTO_ALG_TYPE_SKCIPHER |
					  CRYPTO_ALG_ASYNC,
		.base.cra_blocksize	= TEGRA_SE_AES_BLOCK_SIZE,
		.base.cra_ctxsize	= sizeof(struct tegra_se_aes_context),
		.base.cra_alignmask	= 0,
		.base.cra_module	= THIS_MODULE,
		.init			= tegra_se_aes_cra_init,
		.exit			= tegra_se_aes_cra_exit,
		.setkey			= tegra_se_aes_setkey,
		.encrypt		= tegra_se_aes_ecb_encrypt,
		.decrypt		= tegra_se_aes_ecb_decrypt,
		.min_keysize		= TEGRA_SE_AES_MIN_KEY_SIZE,
		.max_keysize		= TEGRA_SE_AES_MAX_KEY_SIZE,
		.ivsize			= TEGRA_SE_AES_IV_SIZE,
	},
	{
		.base.cra_name		= "ctr(aes)",
		.base.cra_driver_name	= "ctr-aes-tegra",
		.base.cra_priority	= 500,
		.base.cra_flags		= CRYPTO_ALG_TYPE_SKCIPHER |
					  CRYPTO_ALG_ASYNC,
		.base.cra_blocksize	= TEGRA_SE_AES_BLOCK_SIZE,
		.base.cra_ctxsize	= sizeof(struct tegra_se_aes_context),
		.base.cra_alignmask	= 0,
		.base.cra_module	= THIS_MODULE,
		.init			= tegra_se_aes_cra_init,
		.exit			= tegra_se_aes_cra_exit,
		.setkey			= tegra_se_aes_setkey,
		.encrypt		= tegra_se_aes_ctr_encrypt,
		.decrypt		= tegra_se_aes_ctr_decrypt,
		.min_keysize		= TEGRA_SE_AES_MIN_KEY_SIZE,
		.max_keysize		= TEGRA_SE_AES_MAX_KEY_SIZE,
		.ivsize			= TEGRA_SE_AES_IV_SIZE,
	},
	{
		.base.cra_name		= "ofb(aes)",
		.base.cra_driver_name	= "ofb-aes-tegra",
		.base.cra_priority	= 500,
		.base.cra_flags		= CRYPTO_ALG_TYPE_SKCIPHER |
					  CRYPTO_ALG_ASYNC,
		.base.cra_blocksize	= TEGRA_SE_AES_BLOCK_SIZE,
		.base.cra_ctxsize	= sizeof(struct tegra_se_aes_context),
		.base.cra_alignmask	= 0,
		.base.cra_module	= THIS_MODULE,
		.init			= tegra_se_aes_cra_init,
		.exit			= tegra_se_aes_cra_exit,
		.setkey			= tegra_se_aes_setkey,
		.encrypt		= tegra_se_aes_ofb_encrypt,
		.decrypt		= tegra_se_aes_ofb_decrypt,
		.min_keysize		= TEGRA_SE_AES_MIN_KEY_SIZE,
		.max_keysize		= TEGRA_SE_AES_MAX_KEY_SIZE,
		.ivsize			= TEGRA_SE_AES_IV_SIZE,
	},
};

static struct ahash_alg hash_algs[] = {
	{
		.init = tegra_se_aes_cmac_init,
		.update = tegra_se_aes_cmac_update,
		.final = tegra_se_aes_cmac_final,
		.finup = tegra_se_aes_cmac_finup,
		.digest = tegra_se_aes_cmac_digest,
		.setkey = tegra_se_aes_cmac_setkey,
		.export = tegra_se_aes_cmac_export,
		.import = tegra_se_aes_cmac_import,
		.halg.digestsize = TEGRA_SE_AES_CMAC_DIGEST_SIZE,
		.halg.statesize = TEGRA_SE_AES_CMAC_STATE_SIZE,
		.halg.base = {
			.cra_name = "cmac(aes)",
			.cra_driver_name = "tegra-se-cmac(aes)",
			.cra_priority = 500,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = TEGRA_SE_AES_BLOCK_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_aes_cmac_context),
			.cra_alignmask = 0,
			.cra_module	= THIS_MODULE,
			.cra_init	= tegra_se_aes_cmac_cra_init,
			.cra_exit	= tegra_se_aes_cmac_cra_exit,
		}
	}, {
		.init = tegra_se_sha_init,
		.update = tegra_se_sha_update,
		.final = tegra_se_sha_final,
		.finup = tegra_se_sha_finup,
		.digest = tegra_se_sha_digest,
		.export = tegra_se_sha_export,
		.import = tegra_se_sha_import,
		.halg.digestsize = SHA1_DIGEST_SIZE,
		.halg.statesize = SHA1_STATE_SIZE,
		.halg.base = {
			.cra_name = "sha1",
			.cra_driver_name = "tegra-se-sha1",
			.cra_priority = 300,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA1_BLOCK_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_sha_cra_init,
			.cra_exit = tegra_se_sha_cra_exit,
		}
	}, {
		.init = tegra_se_sha_init,
		.update = tegra_se_sha_update,
		.final = tegra_se_sha_final,
		.finup = tegra_se_sha_finup,
		.digest = tegra_se_sha_digest,
		.export = tegra_se_sha_export,
		.import = tegra_se_sha_import,
		.halg.digestsize = SHA224_DIGEST_SIZE,
		.halg.statesize = SHA224_STATE_SIZE,
		.halg.base = {
			.cra_name = "sha224",
			.cra_driver_name = "tegra-se-sha224",
			.cra_priority = 300,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA224_BLOCK_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_sha_cra_init,
			.cra_exit = tegra_se_sha_cra_exit,
		}
	}, {
		.init = tegra_se_sha_init,
		.update = tegra_se_sha_update,
		.final = tegra_se_sha_final,
		.finup = tegra_se_sha_finup,
		.digest = tegra_se_sha_digest,
		.export = tegra_se_sha_export,
		.import = tegra_se_sha_import,
		.halg.digestsize = SHA256_DIGEST_SIZE,
		.halg.statesize = SHA256_STATE_SIZE,
		.halg.base = {
			.cra_name = "sha256",
			.cra_driver_name = "tegra-se-sha256",
			.cra_priority = 300,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA256_BLOCK_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_sha_cra_init,
			.cra_exit = tegra_se_sha_cra_exit,
		}
	}, {
		.init = tegra_se_sha_init,
		.update = tegra_se_sha_update,
		.final = tegra_se_sha_final,
		.finup = tegra_se_sha_finup,
		.digest = tegra_se_sha_digest,
		.export = tegra_se_sha_export,
		.import = tegra_se_sha_import,
		.halg.digestsize = SHA384_DIGEST_SIZE,
		.halg.statesize = SHA384_STATE_SIZE,
		.halg.base = {
			.cra_name = "sha384",
			.cra_driver_name = "tegra-se-sha384",
			.cra_priority = 300,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA384_BLOCK_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_sha_cra_init,
			.cra_exit = tegra_se_sha_cra_exit,
		}
	}, {
		.init = tegra_se_sha_init,
		.update = tegra_se_sha_update,
		.final = tegra_se_sha_final,
		.finup = tegra_se_sha_finup,
		.digest = tegra_se_sha_digest,
		.export = tegra_se_sha_export,
		.import = tegra_se_sha_import,
		.halg.digestsize = SHA512_DIGEST_SIZE,
		.halg.statesize = SHA512_STATE_SIZE,
		.halg.base = {
			.cra_name = "sha512",
			.cra_driver_name = "tegra-se-sha512",
			.cra_priority = 300,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA512_BLOCK_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_sha_cra_init,
			.cra_exit = tegra_se_sha_cra_exit,
		}
	}, {
		.init = tegra_se_sha_init,
		.update = tegra_se_sha_update,
		.final = tegra_se_sha_final,
		.finup = tegra_se_sha_finup,
		.digest = tegra_se_sha_digest,
		.export = tegra_se_sha_export,
		.import = tegra_se_sha_import,
		.halg.digestsize = SHA3_224_DIGEST_SIZE,
		.halg.statesize = SHA3_224_STATE_SIZE,
		.halg.base = {
			.cra_name = "sha3-224",
			.cra_driver_name = "tegra-se-sha3-224",
			.cra_priority = 300,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA3_224_BLOCK_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_sha_cra_init,
			.cra_exit = tegra_se_sha_cra_exit,
		}
	}, {
		.init = tegra_se_sha_init,
		.update = tegra_se_sha_update,
		.final = tegra_se_sha_final,
		.finup = tegra_se_sha_finup,
		.digest = tegra_se_sha_digest,
		.export = tegra_se_sha_export,
		.import = tegra_se_sha_import,
		.halg.digestsize = SHA3_256_DIGEST_SIZE,
		.halg.statesize = SHA3_256_STATE_SIZE,
		.halg.base = {
			.cra_name = "sha3-256",
			.cra_driver_name = "tegra-se-sha3-256",
			.cra_priority = 300,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA3_256_BLOCK_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_sha_cra_init,
			.cra_exit = tegra_se_sha_cra_exit,
		}
	}, {
		.init = tegra_se_sha_init,
		.update = tegra_se_sha_update,
		.final = tegra_se_sha_final,
		.finup = tegra_se_sha_finup,
		.digest = tegra_se_sha_digest,
		.export = tegra_se_sha_export,
		.import = tegra_se_sha_import,
		.halg.digestsize = SHA3_384_DIGEST_SIZE,
		.halg.statesize = SHA3_384_STATE_SIZE,
		.halg.base = {
			.cra_name = "sha3-384",
			.cra_driver_name = "tegra-se-sha3-384",
			.cra_priority = 300,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA3_384_BLOCK_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_sha_cra_init,
			.cra_exit = tegra_se_sha_cra_exit,
		}
	}, {
		.init = tegra_se_sha_init,
		.update = tegra_se_sha_update,
		.final = tegra_se_sha_final,
		.finup = tegra_se_sha_finup,
		.digest = tegra_se_sha_digest,
		.export = tegra_se_sha_export,
		.import = tegra_se_sha_import,
		.halg.digestsize = SHA3_512_DIGEST_SIZE,
		.halg.statesize = SHA3_512_STATE_SIZE,
		.halg.base = {
			.cra_name = "sha3-512",
			.cra_driver_name = "tegra-se-sha3-512",
			.cra_priority = 300,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA3_512_BLOCK_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_sha_cra_init,
			.cra_exit = tegra_se_sha_cra_exit,
		}
	}, {
		.init = tegra_se_sha_init,
		.update = tegra_se_sha_update,
		.final = tegra_se_sha_final,
		.finup = tegra_se_sha_finup,
		.digest = tegra_se_sha_digest,
		.export = tegra_se_sha_export,
		.import = tegra_se_sha_import,
		.halg.digestsize = SHA3_512_DIGEST_SIZE,
		.halg.statesize = SHA3_512_STATE_SIZE,
		.halg.base = {
			.cra_name = "shake128",
			.cra_driver_name = "tegra-se-shake128",
			.cra_priority = 300,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA3_512_BLOCK_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_sha_cra_init,
			.cra_exit = tegra_se_sha_cra_exit,
		}
	}, {
		.init = tegra_se_sha_init,
		.update = tegra_se_sha_update,
		.final = tegra_se_sha_final,
		.finup = tegra_se_sha_finup,
		.digest = tegra_se_sha_digest,
		.export = tegra_se_sha_export,
		.import = tegra_se_sha_import,
		.halg.digestsize = SHA3_512_DIGEST_SIZE,
		.halg.statesize = SHA3_512_STATE_SIZE,
		.halg.base = {
			.cra_name = "shake256",
			.cra_driver_name = "tegra-se-shake256",
			.cra_priority = 300,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA3_512_BLOCK_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_sha_cra_init,
			.cra_exit = tegra_se_sha_cra_exit,
		}
	}, {
		.init = tegra_se_sha_init,
		.update = tegra_se_sha_update,
		.final = tegra_se_sha_final,
		.finup = tegra_se_sha_finup,
		.digest = tegra_se_sha_digest,
		.export = tegra_se_sha_export,
		.import = tegra_se_sha_import,
		.setkey = tegra_se_sha_hmac_setkey,
		.halg.digestsize = SHA224_DIGEST_SIZE,
		.halg.statesize = SHA224_STATE_SIZE,
		.halg.base = {
			.cra_name = "hmac(sha224)",
			.cra_driver_name = "tegra-se-hmac-sha224",
			.cra_priority = 500,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA224_BLOCK_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_sha_cra_init,
			.cra_exit = tegra_se_sha_cra_exit,
		}
	}, {
		.init = tegra_se_sha_init,
		.update = tegra_se_sha_update,
		.final = tegra_se_sha_final,
		.finup = tegra_se_sha_finup,
		.digest = tegra_se_sha_digest,
		.export = tegra_se_sha_export,
		.import = tegra_se_sha_import,
		.setkey = tegra_se_sha_hmac_setkey,
		.halg.digestsize = SHA256_DIGEST_SIZE,
		.halg.statesize = SHA256_STATE_SIZE,
		.halg.base = {
			.cra_name = "hmac(sha256)",
			.cra_driver_name = "tegra-se-hmac-sha256",
			.cra_priority = 500,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA256_BLOCK_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_sha_cra_init,
			.cra_exit = tegra_se_sha_cra_exit,
		}
	}, {
		.init = tegra_se_sha_init,
		.update = tegra_se_sha_update,
		.final = tegra_se_sha_final,
		.finup = tegra_se_sha_finup,
		.digest = tegra_se_sha_digest,
		.export = tegra_se_sha_export,
		.import = tegra_se_sha_import,
		.setkey = tegra_se_sha_hmac_setkey,
		.halg.digestsize = SHA384_DIGEST_SIZE,
		.halg.statesize = SHA384_STATE_SIZE,
		.halg.base = {
			.cra_name = "hmac(sha384)",
			.cra_driver_name = "tegra-se-hmac-sha384",
			.cra_priority = 500,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA384_BLOCK_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_sha_cra_init,
			.cra_exit = tegra_se_sha_cra_exit,
		}
	}, {
		.init = tegra_se_sha_init,
		.update = tegra_se_sha_update,
		.final = tegra_se_sha_final,
		.finup = tegra_se_sha_finup,
		.digest = tegra_se_sha_digest,
		.export = tegra_se_sha_export,
		.import = tegra_se_sha_import,
		.setkey = tegra_se_sha_hmac_setkey,
		.halg.digestsize = SHA512_DIGEST_SIZE,
		.halg.statesize = SHA512_STATE_SIZE,
		.halg.base = {
			.cra_name = "hmac(sha512)",
			.cra_driver_name = "tegra-se-hmac-sha512",
			.cra_priority = 500,
			.cra_flags = CRYPTO_ALG_TYPE_AHASH,
			.cra_blocksize = SHA512_BLOCK_SIZE,
			.cra_ctxsize = sizeof(struct tegra_se_sha_context),
			.cra_alignmask = 0,
			.cra_module = THIS_MODULE,
			.cra_init = tegra_se_sha_cra_init,
			.cra_exit = tegra_se_sha_cra_exit,
		}
	}
};

static struct akcipher_alg rsa_alg = {
	.encrypt = tegra_se_rsa_op,
	.decrypt = tegra_se_rsa_op,
	.sign = tegra_se_rsa_op,
	.verify = tegra_se_rsa_op,
	.set_priv_key = tegra_se_rsa_setkey,
	.set_pub_key = tegra_se_rsa_setkey,
	.max_size = tegra_se_rsa_max_size,
	.exit = tegra_se_rsa_exit,
	.base = {
		.cra_name = "rsa-pka0",
		.cra_driver_name = "tegra-se-pka0-rsa",
		.cra_priority = 300,
		.cra_ctxsize = sizeof(struct tegra_se_aes_rsa_context),
		.cra_module = THIS_MODULE,
	}
};

static int tegra_se_nvhost_prepare_poweroff(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct tegra_se_dev *se_dev = pdata->private_data;

	if (se_dev->channel) {
		nvhost_syncpt_put_ref_ext(se_dev->pdev, se_dev->syncpt_id);
		nvhost_putchannel(se_dev->channel, 1);
		se_dev->channel = NULL;

		/* syncpt will be released along with channel */
		se_dev->syncpt_id = 0;
	}

	return 0;
}

static struct tegra_se_chipdata tegra18_se_chipdata = {
	.aes_freq = 600000000,
	.cpu_freq_mhz = 2400,
	.kac_type = SE_KAC_T18X,
};

static struct nvhost_device_data nvhost_se1_info = {
	.clocks = {{"se", 600000000},
		   {"emc", UINT_MAX,
		   NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER,
		   0, TEGRA_BWMGR_SET_EMC_FLOOR}, {} },
	.can_powergate          = true,
	.autosuspend_delay      = 500,
	.class = NV_SE1_CLASS_ID,
	.private_data = &tegra18_se_chipdata,
	.serialize = 1,
	.push_work_done = 1,
	.vm_regs		= {{SE_STREAMID_REG_OFFSET, true} },
	.kernel_only = true,
	.bwmgr_client_id = TEGRA_BWMGR_CLIENT_SE1,
	.prepare_poweroff = tegra_se_nvhost_prepare_poweroff,
};

static struct nvhost_device_data nvhost_se2_info = {
	.clocks = {{"se", 600000000},
		   {"emc", UINT_MAX,
		   NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER,
		   0, TEGRA_BWMGR_SET_EMC_FLOOR}, {} },
	.can_powergate          = true,
	.autosuspend_delay      = 500,
	.class = NV_SE2_CLASS_ID,
	.private_data = &tegra18_se_chipdata,
	.serialize = 1,
	.push_work_done = 1,
	.vm_regs		= {{SE_STREAMID_REG_OFFSET, true} },
	.kernel_only = true,
	.bwmgr_client_id = TEGRA_BWMGR_CLIENT_SE2,
	.prepare_poweroff = tegra_se_nvhost_prepare_poweroff,
};

static struct nvhost_device_data nvhost_se3_info = {
	.clocks = {{"se", 600000000},
		   {"emc", UINT_MAX,
		   NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER,
		   0, TEGRA_BWMGR_SET_EMC_FLOOR}, {} },
	.can_powergate          = true,
	.autosuspend_delay      = 500,
	.class = NV_SE3_CLASS_ID,
	.private_data = &tegra18_se_chipdata,
	.serialize = 1,
	.push_work_done = 1,
	.vm_regs		= {{SE_STREAMID_REG_OFFSET, true} },
	.kernel_only = true,
	.bwmgr_client_id = TEGRA_BWMGR_CLIENT_SE3,
	.prepare_poweroff = tegra_se_nvhost_prepare_poweroff,
};

static struct nvhost_device_data nvhost_se4_info = {
	.clocks = {{"se", 600000000},
		   {"emc", UINT_MAX,
		   NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER,
		   0, TEGRA_BWMGR_SET_EMC_FLOOR}, {} },
	.can_powergate          = true,
	.autosuspend_delay      = 500,
	.class = NV_SE4_CLASS_ID,
	.private_data = &tegra18_se_chipdata,
	.serialize = 1,
	.push_work_done = 1,
	.vm_regs		= {{SE_STREAMID_REG_OFFSET, true} },
	.kernel_only = true,
	.bwmgr_client_id = TEGRA_BWMGR_CLIENT_SE4,
	.prepare_poweroff = tegra_se_nvhost_prepare_poweroff,
};

static struct tegra_se_chipdata tegra23_se_chipdata = {
	.aes_freq = 600000000,
	.cpu_freq_mhz = 2400,
	.kac_type = SE_KAC_T23X,
};

static struct nvhost_device_data nvhost_t234_se1_info = {
	.clocks = {{"se", 600000000},
		   {"emc", UINT_MAX,
		   NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER,
		   TEGRA_SET_EMC_FLOOR}, {} },
	.can_powergate          = true,
	.autosuspend_delay      = 500,
	.class = NV_SE1_CLASS_ID,
	.private_data = &tegra23_se_chipdata,
	.serialize = 1,
	.push_work_done = 1,
	.vm_regs		= {{SE_STREAMID_REG_OFFSET, true} },
	.kernel_only = true,
	.icc_id	= TEGRA_ICC_SE,
	.prepare_poweroff = tegra_se_nvhost_prepare_poweroff,
};

static struct nvhost_device_data nvhost_t234_se2_info = {
	.clocks = {{"se", 600000000},
		   {"emc", UINT_MAX,
		   NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER,
		   TEGRA_SET_EMC_FLOOR}, {} },
	.can_powergate          = true,
	.autosuspend_delay      = 500,
	.class = NV_SE2_CLASS_ID,
	.private_data = &tegra23_se_chipdata,
	.serialize = 1,
	.push_work_done = 1,
	.vm_regs		= {{SE_STREAMID_REG_OFFSET, true} },
	.kernel_only = true,
	.icc_id	= TEGRA_ICC_SE,
	.prepare_poweroff = tegra_se_nvhost_prepare_poweroff,
};

static struct nvhost_device_data nvhost_t234_se4_info = {
	.clocks = {{"se", 600000000},
		   {"emc", UINT_MAX,
		   NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER,
		   TEGRA_SET_EMC_FLOOR}, {} },
	.can_powergate          = true,
	.autosuspend_delay      = 500,
	.class = NV_SE4_CLASS_ID,
	.private_data = &tegra23_se_chipdata,
	.serialize = 1,
	.push_work_done = 1,
	.vm_regs		= {{SE_STREAMID_REG_OFFSET, true} },
	.kernel_only = true,
	.icc_id	= TEGRA_ICC_SE,
	.prepare_poweroff = tegra_se_nvhost_prepare_poweroff,
};

static const struct of_device_id tegra_se_of_match[] = {
	{
		.compatible = "nvidia,tegra186-se1-nvhost",
		.data = &nvhost_se1_info,
	}, {
		.compatible = "nvidia,tegra186-se2-nvhost",
		.data = &nvhost_se2_info,
	}, {
		.compatible = "nvidia,tegra186-se3-nvhost",
		.data = &nvhost_se3_info,
	}, {
		.compatible = "nvidia,tegra186-se4-nvhost",
		.data = &nvhost_se4_info,
	}, {
		.compatible = "nvidia,tegra234-se1-nvhost",
		.data = &nvhost_t234_se1_info,
	}, {
		.compatible = "nvidia,tegra234-se2-nvhost",
		.data = &nvhost_t234_se2_info,
	}, {
		.compatible = "nvidia,tegra234-se4-nvhost",
		.data = &nvhost_t234_se4_info,
	}, {}
};
MODULE_DEVICE_TABLE(of, tegra_se_of_match);

static bool is_algo_supported(struct device_node *node, char *algo)
{
	if (of_property_match_string(node, "supported-algos", algo) >= 0)
		return true;
	else
		return false;
}

static void tegra_se_fill_se_dev_info(struct tegra_se_dev *se_dev)
{
	struct device_node *node = of_node_get(se_dev->dev->of_node);

	if (is_algo_supported(node, "aes"))
		se_devices[SE_AES] = se_dev;
	if (is_algo_supported(node, "drbg"))
		se_devices[SE_DRBG] = se_dev;
	if (is_algo_supported(node, "sha"))
		se_devices[SE_SHA] = se_dev;
	if (is_algo_supported(node, "rsa"))
		se_devices[SE_RSA] = se_dev;
	if (is_algo_supported(node, "cmac"))
		se_devices[SE_CMAC] = se_dev;
	if (is_algo_supported(node, "aead"))
		se_devices[SE_AEAD] = se_dev;
}

static int tegra_se_probe(struct platform_device *pdev)
{
	struct tegra_se_dev *se_dev = NULL;
	struct nvhost_device_data *pdata = NULL;
	const struct of_device_id *match;
	int err = 0, i = 0;
	unsigned int val;
	struct device_node *node = NULL;
	const char *rsa_name;

	se_dev = devm_kzalloc(&pdev->dev, sizeof(struct tegra_se_dev),
			      GFP_KERNEL);
	if (!se_dev)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		match = of_match_device(of_match_ptr(tegra_se_of_match),
					&pdev->dev);
		if (!match) {
			dev_err(&pdev->dev, "Error: No device match found\n");
			return -ENODEV;
		}
		pdata = (struct nvhost_device_data *)match->data;
	} else {
		pdata =
		(struct nvhost_device_data *)pdev->id_entry->driver_data;
	}

	mutex_init(&se_dev->lock);
	crypto_init_queue(&se_dev->queue, TEGRA_SE_CRYPTO_QUEUE_LENGTH);

	se_dev->dev = &pdev->dev;
	se_dev->pdev = pdev;

	dma_set_mask_and_coherent(se_dev->dev, DMA_BIT_MASK(39));

	mutex_init(&pdata->lock);
	pdata->pdev = pdev;

	/* store chipdata inside se_dev and store se_dev into private_data */
	se_dev->chipdata = pdata->private_data;
	pdata->private_data = se_dev;

	/* store the pdata into drvdata */
	platform_set_drvdata(pdev, pdata);

	err = nvhost_client_device_get_resources(pdev);
	if (err) {
		dev_err(se_dev->dev,
			"nvhost_client_device_get_resources failed for SE(%s)\n",
			pdev->name);
		return err;
	}

	err = nvhost_module_init(pdev);
	if (err) {
		dev_err(se_dev->dev,
			"nvhost_module_init failed for SE(%s)\n", pdev->name);
		return err;
	}

	err = nvhost_client_device_init(pdev);
	if (err) {
		dev_err(se_dev->dev,
			"nvhost_client_device_init failed for SE(%s)\n",
			pdev->name);
		return err;
	}

	err = nvhost_channel_map(pdata, &se_dev->channel, pdata);
	if (err) {
		dev_err(se_dev->dev, "Nvhost Channel map failed\n");
		return err;
	}

	se_dev->io_regs = pdata->aperture[0];

	node = of_node_get(se_dev->dev->of_node);

	se_dev->ioc = of_property_read_bool(node, "nvidia,io-coherent");

	err = of_property_read_u32(node, "opcode_addr", &se_dev->opcode_addr);
	if (err) {
		dev_err(se_dev->dev, "Missing opcode_addr property\n");
		return err;
	}

	if (!of_property_count_strings(node, "supported-algos"))
		return -ENOTSUPP;

	tegra_se_fill_se_dev_info(se_dev);

	if (is_algo_supported(node, "aes") || is_algo_supported(node, "drbg")) {
		err = tegra_init_key_slot(se_dev);
		if (err) {
			dev_err(se_dev->dev, "init_key_slot failed\n");
			return err;
		}
	}

	if (is_algo_supported(node, "rsa")) {
		err = tegra_init_rsa_key_slot(se_dev);
		if (err) {
			dev_err(se_dev->dev, "init_rsa_key_slot failed\n");
			return err;
		}
	}

	mutex_init(&se_dev->mtx);
	INIT_WORK(&se_dev->se_work, tegra_se_work_handler);
	se_dev->se_work_q = alloc_workqueue("se_work_q",
					    WQ_HIGHPRI | WQ_UNBOUND, 1);
	if (!se_dev->se_work_q) {
		dev_err(se_dev->dev, "alloc_workqueue failed\n");
		return -ENOMEM;
	}

	err = tegra_se_alloc_ll_buf(se_dev, SE_MAX_SRC_SG_COUNT,
				    SE_MAX_DST_SG_COUNT);
	if (err) {
		dev_err(se_dev->dev, "can not allocate ll dma buffer\n");
		goto ll_alloc_fail;
	}

	if (is_algo_supported(node, "drbg")) {
		INIT_LIST_HEAD(&rng_algs[0].base.cra_list);
		err = crypto_register_rng(&rng_algs[0]);
		if (err) {
			dev_err(se_dev->dev, "crypto_register_rng failed\n");
			goto reg_fail;
		}
	}

	if (is_algo_supported(node, "xts")) {
		INIT_LIST_HEAD(&aes_algs[0].base.cra_list);
		err = crypto_register_skcipher(&aes_algs[0]);
		if (err) {
			dev_err(se_dev->dev,
				"crypto_register_alg xts failed\n");
			goto reg_fail;
		}
	}

	if (is_algo_supported(node, "aes")) {
		for (i = 1; i < ARRAY_SIZE(aes_algs); i++) {
			INIT_LIST_HEAD(&aes_algs[i].base.cra_list);
			err = crypto_register_skcipher(&aes_algs[i]);
			if (err) {
				dev_err(se_dev->dev,
					"crypto_register_alg %s failed\n",
					aes_algs[i].base.cra_name);
				goto reg_fail;
			}
		}
	}

	if (is_algo_supported(node, "cmac")) {
		err = crypto_register_ahash(&hash_algs[0]);
		if (err) {
			dev_err(se_dev->dev,
				"crypto_register_ahash cmac failed\n");
			goto reg_fail;
		}
	}

	if (is_algo_supported(node, "sha")) {
		for (i = 1; i < 6; i++) {
			err = crypto_register_ahash(&hash_algs[i]);
			if (err) {
				dev_err(se_dev->dev,
					"crypto_register_ahash %s failed\n",
					hash_algs[i].halg.base.cra_name);
				goto reg_fail;
			}
		}
	}

	if (is_algo_supported(node, "sha3")) {
		for (i = 6; i < 12; i++) {
			err = crypto_register_ahash(&hash_algs[i]);
			if (err) {
				dev_err(se_dev->dev,
					"crypto_register_ahash %s failed\n",
					hash_algs[i].halg.base.cra_name);
				goto reg_fail;
			}
		}
	}

	if (is_algo_supported(node, "hmac")) {
		for (i = 12; i < 16; i++) {
			err = crypto_register_ahash(&hash_algs[i]);
			if (err) {
				dev_err(se_dev->dev,
					"crypto_register_ahash %s failed\n",
					hash_algs[i].halg.base.cra_name);
				goto reg_fail;
			}
		}
	}

	if (is_algo_supported(node, "aead")) {
		for (i = 0; i < 2; i++) {
			err = crypto_register_aead(&aead_algs[i]);
			if (err) {
				dev_err(se_dev->dev,
					"crypto_register_aead %s failed\n",
					aead_algs[i].base.cra_name);
				goto reg_fail;
			}
		}
	}

	node = of_node_get(se_dev->dev->of_node);

	err = of_property_read_u32(node, "pka0-rsa-priority", &val);
	if (!err)
		rsa_alg.base.cra_priority = val;

	err = of_property_read_string(node, "pka0-rsa-name", &rsa_name);
	if (!err)
		strncpy(rsa_alg.base.cra_name, rsa_name,
				sizeof(rsa_alg.base.cra_name) - 1);

	if (is_algo_supported(node, "rsa")) {
		err = crypto_register_akcipher(&rsa_alg);
		if (err) {
			dev_err(se_dev->dev, "crypto_register_akcipher fail");
			goto reg_fail;
		}
		err = crypto_register_kpp(&dh_algs[0]);
		if (err) {
			dev_err(se_dev->dev, "crypto_register_kpp fail");
			goto reg_fail;
		}

		se_dev->dh_buf1 = (u32 *)devm_kzalloc(
				se_dev->dev, TEGRA_SE_RSA2048_INPUT_SIZE,
				GFP_KERNEL);
		se_dev->dh_buf2 = (u32 *)devm_kzalloc(
				se_dev->dev, TEGRA_SE_RSA2048_INPUT_SIZE,
				GFP_KERNEL);
		if (!se_dev->dh_buf1 || !se_dev->dh_buf2)
			goto reg_fail;
	}

	if (is_algo_supported(node, "drbg")) {
		/* Make sure engine is powered ON with clk enabled */
		err = nvhost_module_busy(pdev);
		if (err) {
			dev_err(se_dev->dev,
				"nvhost_module_busy failed for se_dev\n");
			goto reg_fail;
		}
		se_writel(se_dev,
			  SE_RNG_SRC_CONFIG_RO_ENT_SRC(DRBG_RO_ENT_SRC_ENABLE) |
			  SE_RNG_SRC_CONFIG_RO_ENT_SRC_LOCK(
						DRBG_RO_ENT_SRC_LOCK_ENABLE),
			  SE_RNG_SRC_CONFIG_REG_OFFSET);
		/* Power OFF after SE register update */
		nvhost_module_idle(pdev);
	}

	se_dev->syncpt_id = nvhost_get_syncpt_host_managed(se_dev->pdev,
							   0, pdev->name);
	if (!se_dev->syncpt_id) {
		err = -EINVAL;
		dev_err(se_dev->dev, "Cannot get syncpt_id for SE(%s)\n",
			pdev->name);
		goto reg_fail;
	}

	se_dev->aes_src_ll = devm_kzalloc(&pdev->dev, sizeof(struct tegra_se_ll),
				      GFP_KERNEL);
	se_dev->aes_dst_ll = devm_kzalloc(&pdev->dev, sizeof(struct tegra_se_ll),
				      GFP_KERNEL);
	if (!se_dev->aes_src_ll || !se_dev->aes_dst_ll) {
		dev_err(se_dev->dev, "Linked list memory allocation failed\n");
		goto aes_buf_alloc_fail;
	}

	if (se_dev->ioc)
		se_dev->total_aes_buf = dma_alloc_coherent(
						se_dev->dev, SE_MAX_MEM_ALLOC,
						&se_dev->total_aes_buf_addr,
						GFP_KERNEL);
	else
		se_dev->total_aes_buf = kzalloc(SE_MAX_MEM_ALLOC, GFP_KERNEL);

	if (!se_dev->total_aes_buf) {
		err = -ENOMEM;
		goto aes_buf_alloc_fail;
	}

	tegra_se_init_aesbuf(se_dev);

	if (is_algo_supported(node, "drbg") || is_algo_supported(node, "aes") ||
	    is_algo_supported(node, "cmac") || is_algo_supported(node, "sha")) {
		se_dev->aes_cmdbuf_cpuvaddr = dma_alloc_attrs(
			se_dev->dev->parent, SZ_16K * SE_MAX_SUBMIT_CHAIN_SZ,
			&se_dev->aes_cmdbuf_iova, GFP_KERNEL,
			0);
		if (!se_dev->aes_cmdbuf_cpuvaddr)
			goto cmd_buf_alloc_fail;

		err = tegra_se_init_cmdbuf_addr(se_dev);
		if (err) {
			dev_err(se_dev->dev, "failed to init cmdbuf addr\n");
			goto dma_free;
		}
	}

	tegra_se_boost_cpu_init(se_dev);

	dev_info(se_dev->dev, "%s: complete", __func__);

	return 0;
dma_free:
	dma_free_attrs(se_dev->dev->parent, SZ_16K * SE_MAX_SUBMIT_CHAIN_SZ,
		       se_dev->aes_cmdbuf_cpuvaddr, se_dev->aes_cmdbuf_iova,
		       0);
cmd_buf_alloc_fail:
	kfree(se_dev->total_aes_buf);
aes_buf_alloc_fail:
	nvhost_syncpt_put_ref_ext(se_dev->pdev, se_dev->syncpt_id);
reg_fail:
	tegra_se_free_ll_buf(se_dev);
ll_alloc_fail:
	if (se_dev->se_work_q)
		destroy_workqueue(se_dev->se_work_q);

	return err;
}

static int tegra_se_remove(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct tegra_se_dev *se_dev = pdata->private_data;
	struct device_node *node;
	int i;

	if (!se_dev) {
		pr_err("Device is NULL\n");
		return -ENODEV;
	}

	tegra_se_boost_cpu_deinit(se_dev);

	if (se_dev->aes_cmdbuf_cpuvaddr)
		dma_free_attrs(
		se_dev->dev->parent, SZ_16K * SE_MAX_SUBMIT_CHAIN_SZ,
		se_dev->aes_cmdbuf_cpuvaddr, se_dev->aes_cmdbuf_iova,
		0);

	node = of_node_get(se_dev->dev->of_node);
	if (is_algo_supported(node, "drbg"))
		crypto_unregister_rng(&rng_algs[0]);

	if (is_algo_supported(node, "xts"))
		crypto_unregister_skcipher(&aes_algs[0]);

	if (is_algo_supported(node, "aes")) {
		crypto_unregister_skcipher(&aes_algs[1]);
		for (i = 2; i < ARRAY_SIZE(aes_algs); i++)
			crypto_unregister_skcipher(&aes_algs[i]);
	}

	if (is_algo_supported(node, "cmac"))
		crypto_unregister_ahash(&hash_algs[0]);

	if (is_algo_supported(node, "sha")) {
		for (i = 1; i < 6; i++)
			crypto_unregister_ahash(&hash_algs[i]);
	}

	if (is_algo_supported(node, "rsa")) {
		crypto_unregister_akcipher(&rsa_alg);
		crypto_unregister_kpp(&dh_algs[0]);
	}

	if (is_algo_supported(node, "aead")) {
		for (i = 0; i < 2; i++)
			crypto_unregister_aead(&aead_algs[i]);
	}

	tegra_se_free_ll_buf(se_dev);
	kfree(se_dev->total_aes_buf);

	cancel_work_sync(&se_dev->se_work);
	if (se_dev->se_work_q)
		destroy_workqueue(se_dev->se_work_q);

	mutex_destroy(&se_dev->mtx);
	nvhost_client_device_release(pdev);
	mutex_destroy(&pdata->lock);

	return 0;
}

static struct platform_driver tegra_se_driver = {
	.probe  = tegra_se_probe,
	.remove = tegra_se_remove,
	.driver = {
		.name   = "tegra-se-nvhost",
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(tegra_se_of_match),
		.pm = &nvhost_module_pm_ops,
		.suppress_bind_attrs = true,
	},
};

static int __init tegra_se_module_init(void)
{
	return  platform_driver_register(&tegra_se_driver);
}

static void __exit tegra_se_module_exit(void)
{
	platform_driver_unregister(&tegra_se_driver);
}

late_initcall(tegra_se_module_init);
module_exit(tegra_se_module_exit);

MODULE_DESCRIPTION("Tegra Crypto algorithm support using Host1x Interface");
MODULE_AUTHOR("NVIDIA Corporation");
MODULE_LICENSE("GPL");
MODULE_ALIAS("tegra-se-nvhost");
