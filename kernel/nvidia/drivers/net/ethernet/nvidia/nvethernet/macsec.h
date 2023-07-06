/*
 * Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
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

#ifndef INCLUDED_MACSEC_H
#define INCLUDED_MACSEC_H

#include <osi_macsec.h>
#include <linux/random.h>
#include <net/genetlink.h>
#include <linux/crypto.h>


/**
 * @brief Expected number of inputs in BYP or SCI LUT sysfs config
 */
#define LUT_INPUTS_LEN			39

/**
 * @brief Expected number of extra inputs in BYP LUT sysfs config
 */
#define BYP_LUT_INPUTS			1

/**
 * @brief MACSEC SECTAG + ICV + 2B ethertype adds up to 34B
 */
#define MACSEC_TAG_ICV_LEN		34U

/**
 * @brief Size of Macsec IRQ name.
 */
#define MACSEC_IRQ_NAME_SZ		32

/**
 * @brief Maximum number of supplicants allowed per VF
 */
#define MAX_SUPPLICANTS_ALLOWED		1

#define NV_MACSEC_GENL_VERSION	1

#ifdef MACSEC_KEY_PROGRAM
#define MACSEC_SIZE 0x10000U
#endif

#define KEY2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5],\
		   (a)[6], (a)[7], (a)[8], (a)[9], (a)[10], (a)[11],\
		   (a)[12], (a)[13], (a)[14], (a)[15]
#define KEYSTR "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x \
%02x %02x %02x %02x %02x %02x"

/* For 128 bit SAK, key len is 16 bytes, wrapped key len is 24 bytes
 * and for 256 SAK, key len is 32 bytes, wrapped key len is 40 bytes
 */
#define NV_SAK_WRAPPED_LEN 40
/* PKCS KEK CK_OBJECT_HANDLE is u64 type */
#define NV_KEK_HANDLE_SIZE 8

/* keep the same enum definition in nv macsec supplicant driver */
enum nv_macsec_sa_attrs {
	NV_MACSEC_SA_ATTR_UNSPEC,
	NV_MACSEC_SA_ATTR_SCI,
	NV_MACSEC_SA_ATTR_AN,
	NV_MACSEC_SA_ATTR_PN,
	NV_MACSEC_SA_ATTR_LOWEST_PN,
#ifdef NVPKCS_MACSEC
	NV_MACSEC_SA_PKCS_KEY_WRAP,
	NV_MACSEC_SA_PKCS_KEK_HANDLE,
#else
	NV_MACSEC_SA_ATTR_KEY,
#endif /* NVPKCS_MACSEC */
	__NV_MACSEC_SA_ATTR_END,
	NUM_NV_MACSEC_SA_ATTR = __NV_MACSEC_SA_ATTR_END,
	NV_MACSEC_SA_ATTR_MAX = __NV_MACSEC_SA_ATTR_END - 1,
};

enum nv_macsec_tz_attrs {
	NV_MACSEC_TZ_ATTR_UNSPEC,
	NV_MACSEC_TZ_INSTANCE_ID,
	NV_MACSEC_TZ_ATTR_CTRL,
	NV_MACSEC_TZ_ATTR_RW,
	NV_MACSEC_TZ_ATTR_INDEX,
#ifdef NVPKCS_MACSEC
	NV_MACSEC_TZ_PKCS_KEY_WRAP,
	NV_MACSEC_TZ_PKCS_KEK_HANDLE,
#else
	NV_MACSEC_TZ_ATTR_KEY,
#endif /* NVPKCS_MACSEC */
	NV_MACSEC_TZ_ATTR_FLAG,
	__NV_MACSEC_TZ_ATTR_END,
	NUM_NV_MACSEC_TZ_ATTR = __NV_MACSEC_TZ_ATTR_END,
	NV_MACSEC_TZ_ATTR_MAX = __NV_MACSEC_TZ_ATTR_END - 1,
};

enum nv_macsec_tz_kt_reset_attrs {
	NV_MACSEC_TZ_KT_RESET_ATTR_UNSPEC,
	NV_MACSEC_TZ_KT_RESET_INSTANCE_ID,
	__NV_MACSEC_TZ_KT_RESET_ATTR_END,
	NUM_KT_RESET_ATTR = __NV_MACSEC_TZ_KT_RESET_ATTR_END,
	NV_MACSEC_TZ_KT_RESET_ATTR_MAX = __NV_MACSEC_TZ_KT_RESET_ATTR_END - 1,
};

enum nv_macsec_attrs {
	NV_MACSEC_ATTR_UNSPEC,
	NV_MACSEC_ATTR_IFNAME,
	NV_MACSEC_ATTR_TXSC_PORT,
	NV_MACSEC_ATTR_PROT_FRAMES_EN,
	NV_MACSEC_ATTR_REPLAY_PROT_EN,
	NV_MACSEC_ATTR_REPLAY_WINDOW,
	NV_MACSEC_ATTR_CIPHER_SUITE,
	NV_MACSEC_ATTR_CTRL_PORT_EN,
	NV_MACSEC_ATTR_SA_CONFIG, /* Nested SA config */
	NV_MACSEC_ATTR_TZ_CONFIG, /* Nested TZ config */
	NV_MACSEC_ATTR_TZ_KT_RESET, /* Nested TZ KT config */
	__NV_MACSEC_ATTR_END,
	NUM_NV_MACSEC_ATTR = __NV_MACSEC_ATTR_END,
	NV_MACSEC_ATTR_MAX = __NV_MACSEC_ATTR_END - 1,
};

static const struct nla_policy nv_macsec_sa_genl_policy[NUM_NV_MACSEC_SA_ATTR] = {
	[NV_MACSEC_SA_ATTR_SCI] = { .type = NLA_BINARY,
				    .len = 8, }, /* SCI is 64bit */
	[NV_MACSEC_SA_ATTR_AN] = { .type = NLA_U8 },
	[NV_MACSEC_SA_ATTR_PN] = { .type = NLA_U32 },
	[NV_MACSEC_SA_ATTR_LOWEST_PN] = { .type = NLA_U32 },
#ifdef NVPKCS_MACSEC
	[NV_MACSEC_SA_PKCS_KEY_WRAP] = { .type = NLA_BINARY,
					 .len = NV_SAK_WRAPPED_LEN,},
	[NV_MACSEC_SA_PKCS_KEK_HANDLE] = { .type = NLA_U64 },
#else
	[NV_MACSEC_SA_ATTR_KEY] = { .type = NLA_BINARY,
				    .len = OSI_KEY_LEN_256,},
#endif /* NVPKCS_MACSEC */
};

static const struct nla_policy nv_macsec_tz_genl_policy[NUM_NV_MACSEC_TZ_ATTR] = {
	[NV_MACSEC_TZ_INSTANCE_ID] = { .type = NLA_U32 },
	[NV_MACSEC_TZ_ATTR_CTRL] = { .type = NLA_U8 }, /* controller Tx or Rx */
	[NV_MACSEC_TZ_ATTR_RW] = { .type = NLA_U8 },
	[NV_MACSEC_TZ_ATTR_INDEX] = { .type = NLA_U8 },
#ifdef NVPKCS_MACSEC
	[NV_MACSEC_SA_PKCS_KEY_WRAP] = { .type = NLA_BINARY,
					 .len = NV_SAK_WRAPPED_LEN,},
	[NV_MACSEC_SA_PKCS_KEK_HANDLE] = { .type = NLA_U64 },
#else
	[NV_MACSEC_TZ_ATTR_KEY] = { .type = NLA_BINARY,
				    .len = OSI_KEY_LEN_256 },
#endif /* NVPKCS_MACSEC */
	[NV_MACSEC_TZ_ATTR_FLAG] = { .type = NLA_U32 },
};

static const struct nla_policy nv_kt_reset_genl_policy[NUM_KT_RESET_ATTR] = {
	[NV_MACSEC_TZ_KT_RESET_INSTANCE_ID] = { .type = NLA_U32 },
};

static const struct nla_policy nv_macsec_genl_policy[NUM_NV_MACSEC_ATTR] = {
	[NV_MACSEC_ATTR_IFNAME] = { .type = NLA_STRING },
	[NV_MACSEC_ATTR_TXSC_PORT] = { .type = NLA_U16 },
	[NV_MACSEC_ATTR_REPLAY_PROT_EN] = { .type = NLA_U32 },
	[NV_MACSEC_ATTR_REPLAY_WINDOW] = { .type = NLA_U32 },
	[NV_MACSEC_ATTR_SA_CONFIG] = { .type = NLA_NESTED },
	[NV_MACSEC_ATTR_TZ_CONFIG] = { .type = NLA_NESTED },
	[NV_MACSEC_ATTR_TZ_KT_RESET] = { .type = NLA_NESTED },
};

enum nv_macsec_nl_commands {
	NV_MACSEC_CMD_INIT,
	NV_MACSEC_CMD_GET_TX_NEXT_PN,
	NV_MACSEC_CMD_SET_PROT_FRAMES,
	NV_MACSEC_CMD_SET_REPLAY_PROT,
	NV_MACSEC_CMD_SET_CIPHER,
	NV_MACSEC_CMD_SET_CONTROLLED_PORT,
	NV_MACSEC_CMD_CREATE_TX_SA,
	NV_MACSEC_CMD_EN_TX_SA,
	NV_MACSEC_CMD_DIS_TX_SA,
	NV_MACSEC_CMD_CREATE_RX_SA,
	NV_MACSEC_CMD_EN_RX_SA,
	NV_MACSEC_CMD_DIS_RX_SA,
	NV_MACSEC_CMD_TZ_CONFIG,
	NV_MACSEC_CMD_TZ_KT_RESET,
	NV_MACSEC_CMD_DEINIT,
};

/**
 * @brief MACsec supplicant data structure
 */
struct macsec_supplicant_data {
	/** specific port id to identity supplicant instance */
	unsigned int snd_portid;
	/** flag check supplicant instance is allocated */
	unsigned short in_use;
	/** MACsec protect frames variable */
	unsigned int protect_frames;
	/** MACsec enabled flags for Tx/Rx controller status */
	unsigned int enabled;
	/** MACsec cipher suite */
	unsigned int cipher;
};

/**
 * @brief MACsec supplicant pkcs data structure
 */
struct nvpkcs_data {
	/** wrapped key */
	u8 nv_key[NV_SAK_WRAPPED_LEN];
	/** wrapped key length */
	int nv_key_len;
	/** pkcs KEK handle(CK_OBJECT_HANDLE ) is u64 */
	u64 nv_kek;
};

/**
 * @brief MACsec private data structure
 */
struct macsec_priv_data {
	/** Non secure reset */
	struct reset_control *ns_rst;
	/** MGBE Macsec clock */
	struct clk *mgbe_clk;
	/** EQOS Macsec TX clock */
	struct clk *eqos_tx_clk;
	/** EQOS Macsec RX clock */
	struct clk *eqos_rx_clk;
	/** Secure irq */
	int s_irq;
	/** Non secure irq */
	int ns_irq;
	/** is_irq_allocated BIT(0)for s_irq and BIT(1)for ns_irq*/
	unsigned int is_irq_allocated;
	/** pointer to ether private data struct */
	struct ether_priv_data *ether_pdata;
	/** macsec IRQ name strings */
	char irq_name[2][MACSEC_IRQ_NAME_SZ];
	/** loopback mode */
	unsigned int loopback_mode;
	/** macsec cipher, aes128 or aes256 bit */
	unsigned int cipher;
	/** MACsec protect frames variable */
	unsigned int protect_frames;
	/** MACsec enabled flags for Tx/Rx controller status */
	unsigned int enabled;
	/** MACsec Rx PN Window */
	unsigned int pn_window;
	/** MACsec controller init reference count */
	atomic_t ref_count;
	/** supplicant instance specific data */
	struct macsec_supplicant_data supplicant[OSI_MAX_NUM_SC];
	/** next supplicant instance index */
	unsigned short next_supp_idx;
	/** macsec mutex lock */
	struct mutex lock;
	/** macsec hw instance id */
	unsigned int id;
	/** Macsec enable flag in DT */
	unsigned int is_macsec_enabled_in_dt;
	/** Context family name  */
	struct genl_family nv_macsec_fam;
	/** Flag to check if nv macsec nl registered */
	unsigned int is_nv_macsec_fam_registered;
	/** Macsec TX currently enabled AN */
	unsigned int macsec_tx_an_map;
	/** Macsec RX currently enabled AN */
	unsigned int macsec_rx_an_map;
};

int macsec_probe(struct ether_priv_data *pdata);
void macsec_remove(struct ether_priv_data *pdata);
int macsec_open(struct macsec_priv_data *macsec_pdata,
		void *const genl_info);
int macsec_close(struct macsec_priv_data *macsec_pdata);
int macsec_suspend(struct macsec_priv_data *macsec_pdata);
int macsec_resume(struct macsec_priv_data *macsec_pdata);

#ifdef DEBUG_MACSEC
#define PRINT_ENTRY()	(printk(KERN_DEBUG "-->%s()\n", __func__))
#define PRINT_EXIT()	(printk(KERN_DEBUG "<--%s()\n", __func__))
#else
#define PRINT_ENTRY()
#define PRINT_EXIT()
#endif /* DEBUG_MACSEC */

#endif /* INCLUDED_MACSEC_H */

