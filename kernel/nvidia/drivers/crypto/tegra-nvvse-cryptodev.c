/*
 * Copyright (c) 2021-2022, NVIDIA Corporation. All Rights Reserved.
 *
 * Tegra NVVSE crypto device for crypto operation to NVVSE linux library.
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
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <linux/uaccess.h>
#include <linux/nospec.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/platform/tegra/common.h>
#include <soc/tegra/fuse.h>
#include <crypto/rng.h>
#include <crypto/hash.h>
#include <crypto/akcipher.h>
#include <crypto/aead.h>
#include <crypto/internal/skcipher.h>
#include <uapi/misc/tegra-nvvse-cryptodev.h>
#include <asm/barrier.h>

#define NBUFS				2
#define XBUFSIZE			8
#define AES_IV_SIZE			16
#define CRYPTO_KEY_LEN_MASK		0x3FF
#define TEGRA_CRYPTO_KEY_512_SIZE	64
#define TEGRA_CRYPTO_KEY_256_SIZE	32
#define TEGRA_CRYPTO_KEY_192_SIZE	24
#define TEGRA_CRYPTO_KEY_128_SIZE	16
#define AES_KEYSLOT_NAME_SIZE		32

#define GCM_MAX_CHUNKS			33
#define GCM_CHUNK_SIZE			(1024*1024) /* 1MB */
#define GCM_PT_MAX_LEN			(16*1024*1024 - 1) /* 16MB */
#define GCM_AAD_MAX_LEN			(16*1024*1024 - 1) /* 16MB */
#define GMAC_MAX_LEN			(16*1024*1024 - 1) /* 16MB */

/** Defines the Maximum Random Number length supported */
#define NVVSE_MAX_RANDOM_NUMBER_LEN_SUPPORTED		512U

/**
 * Define preallocated SHA result buffer size, if digest size is bigger
 * than this then allocate new buffer
 */
#define NVVSE_MAX_ALLOCATED_SHA_RESULT_BUFF_SIZE	256U

/* SHA Algorithm Names */
static const char *sha_alg_names[] = {
	"sha256",
	"sha384",
	"sha512",
	"sha3-256",
	"sha3-384",
	"sha3-512",
	"shake128",
	"shake256",
};

struct tnvvse_crypto_completion {
	struct completion restart;
	int req_err;
};

struct crypto_sha_state {
	uint32_t			sha_type;
	uint32_t			digest_size;
	uint64_t			total_bytes;
	uint64_t			remaining_bytes;
	unsigned long			*xbuf[XBUFSIZE];
	struct tnvvse_crypto_completion	sha_complete;
	struct ahash_request		*req;
	struct crypto_ahash		*tfm;
	char				*result_buff;
	bool				sha_done_success;
};

/* Tegra NVVSE crypt context */
struct tnvvse_crypto_ctx {
	struct mutex			lock;
	struct crypto_sha_state		sha_state;
	char				*rng_buff;
	uint32_t			max_rng_buff;
	char				*sha_result;
};

enum tnvvse_gmac_request_type {
	GMAC_INIT = 0u,
	GMAC_SIGN,
	GMAC_VERIFY
};

/* GMAC request data */
struct tnvvse_gmac_req_data {
	enum tnvvse_gmac_request_type request_type;
	/* Return IV after GMAC_INIT and Pass IV during GMAC_VERIFY */
	char *iv;
	uint8_t is_first;
	/* For GMAC_VERIFY tag comparison result */
	uint8_t result;
};

enum tnvvse_cmac_request_type {
	CMAC_SIGN,
	CMAC_VERIFY
};

/* CMAC request data */
struct tnvvse_cmac_req_data {
	enum tnvvse_cmac_request_type request_type;
	/* For CMAC_VERIFY tag comparison result */
	uint8_t result;
};

static void tnvvse_crypto_complete(struct crypto_async_request *req, int err)
{
	struct tnvvse_crypto_completion *done = req->data;

	if (err != -EINPROGRESS) {
		done->req_err = err;
		complete(&done->restart);
	}
}

static int alloc_bufs(unsigned long *buf[NBUFS])
{
	int i;

	for (i = 0; i < NBUFS; i++) {
		buf[i] = (void *)__get_free_page(GFP_KERNEL);
		if (!buf[i])
			goto free_buf;
	}

	return 0;

free_buf:
	while (i-- > 0)
		free_page((unsigned long)buf[i]);

	return -ENOMEM;
}

static void free_bufs(unsigned long *buf[NBUFS])
{
	int i;

	for (i = 0; i < NBUFS; i++) {
		free_page((unsigned long)buf[i]);
		buf[i] = NULL;
	}
}

static int wait_async_op(struct tnvvse_crypto_completion *tr, int ret)
{
	if (ret == -EINPROGRESS || ret == -EBUSY) {
		wait_for_completion(&tr->restart);
		reinit_completion(&tr->restart);
		ret = tr->req_err;
	}

	return ret;
}

static int tnvvse_crypto_sha_init(struct tnvvse_crypto_ctx *ctx,
		struct tegra_nvvse_sha_init_ctl *init_ctl)
{
	struct crypto_sha_state *sha_state = &ctx->sha_state;
	struct crypto_ahash *tfm;
	struct ahash_request *req;
	const char *driver_name;
	int ret = -ENOMEM;
	char *result_buff = NULL;

	if (init_ctl->sha_type < TEGRA_NVVSE_SHA_TYPE_SHA256  ||
			init_ctl->sha_type >= TEGRA_NVVSE_SHA_TYPE_MAX) {
		pr_err("%s(): SHA Type requested %d is not supported\n",
					__func__, init_ctl->sha_type);
		return -EINVAL;
	}

	tfm = crypto_alloc_ahash(sha_alg_names[init_ctl->sha_type], 0, 0);
	if (IS_ERR(tfm)) {
		pr_err("%s(): Failed to load transform for %s:%ld\n",
					__func__, sha_alg_names[init_ctl->sha_type], PTR_ERR(tfm));
		ret = PTR_ERR(tfm);
		goto out;
	}

	driver_name = crypto_tfm_alg_driver_name(crypto_ahash_tfm(tfm));;
	if (driver_name == NULL) {
		pr_err("%s(): Failed to get driver name\n", __func__);
		goto free_tfm;
	}
	pr_debug("%s(): Algo name %s, driver name %s\n",
					__func__, sha_alg_names[init_ctl->sha_type], driver_name);

	req = ahash_request_alloc(tfm, GFP_KERNEL);
	if (!req) {
		pr_err("%s(): Failed to allocate request for %s\n",
					__func__, sha_alg_names[init_ctl->sha_type]);
		goto free_tfm;
	}

	ahash_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
				   tnvvse_crypto_complete, &sha_state->sha_complete);

	ret = alloc_bufs(sha_state->xbuf);
	if (ret < 0) {
		pr_err("%s(): Failed to allocate Xbuffer\n", __func__);
		goto free_req;
	}

	init_completion(&sha_state->sha_complete.restart);
	sha_state->sha_complete.req_err = 0;

	/* Shake128/Shake256 have variable digest size */
	if ((init_ctl->sha_type == TEGRA_NVVSE_SHA_TYPE_SHAKE128) ||
	     (init_ctl->sha_type == TEGRA_NVVSE_SHA_TYPE_SHAKE256)) {
		req->dst_size = init_ctl->digest_size;
		if (init_ctl->digest_size > NVVSE_MAX_ALLOCATED_SHA_RESULT_BUFF_SIZE) {
			result_buff = kzalloc(init_ctl->digest_size, GFP_KERNEL);
			if (!result_buff) {
				ret = -ENOMEM;
				goto free_xbuf;
			}
		}
	}

	ret = wait_async_op(&sha_state->sha_complete, crypto_ahash_init(req));
	if (ret) {
		pr_err("%s(): Failed to ahash_init for %s: ret=%d\n",
					__func__, sha_alg_names[init_ctl->sha_type], ret);
		goto free_result_buf;
	}

	sha_state->req = req;
	sha_state->tfm = tfm;
	sha_state->result_buff = (result_buff) ? result_buff : ctx->sha_result;
	sha_state->sha_type = init_ctl->sha_type;
	sha_state->total_bytes = init_ctl->total_msg_size;
	sha_state->digest_size = init_ctl->digest_size;
	sha_state->remaining_bytes = init_ctl->total_msg_size;
	sha_state->sha_done_success = false;

	memset(sha_state->result_buff , 0, 64);

	ret = 0;
	goto out;

free_result_buf:
	kfree(result_buff);
free_xbuf:
	free_bufs(ctx->sha_state.xbuf);
free_req:
	ahash_request_free(req);
free_tfm:
	crypto_free_ahash(tfm);
out:
	return ret;
}

static int tnvvse_crypto_sha_update(struct tnvvse_crypto_ctx *ctx,
		struct tegra_nvvse_sha_update_ctl *update_ctl)
{
	struct crypto_sha_state *sha_state = &ctx->sha_state;
	void *hash_buff;
	char *result_buff;
	unsigned long size = 0, total = 0;
	struct ahash_request *req;
	struct scatterlist sg[1];
	char *input_buffer = update_ctl->in_buff;
	int ret;

	hash_buff = sha_state->xbuf[0];
	result_buff = sha_state->result_buff;
	req = sha_state->req;
	total = update_ctl->input_buffer_size;

	while (true) {
		size = (total < PAGE_SIZE) ? total : PAGE_SIZE;
		ret = copy_from_user((void *)hash_buff, (void __user *)input_buffer, size);
		if (ret) {
			pr_err("%s(): Failed to copy_from_user: %d\n", __func__, ret);
			goto stop_sha;
		}

		sg_init_one(&sg[0], hash_buff, size);
		ahash_request_set_crypt(req, &sg[0], result_buff, size);
		ret = wait_async_op(&sha_state->sha_complete, crypto_ahash_update(req));
		if (ret) {
			pr_err("%s(): Failed to ahash_update for %s: %d\n",
						__func__, sha_alg_names[sha_state->sha_type], ret);
			goto stop_sha;
		}

		if (update_ctl->last_buffer && (size >= total)) {
			ret = wait_async_op(&sha_state->sha_complete, crypto_ahash_final(req));
			if (ret) {
				pr_err("%s(): Failed to ahash_final for %s: %d\n",
						__func__, sha_alg_names[sha_state->sha_type], ret);
				goto stop_sha;
			}
			sha_state->sha_done_success = true;
			break;
		}

		total -= size;
		input_buffer += size;
		if (!update_ctl->last_buffer && (total == 0)) {
			break;
		}
	}

	goto done;

stop_sha:
	free_bufs(sha_state->xbuf);
	ahash_request_free(sha_state->req);
	crypto_free_ahash(sha_state->tfm);

	sha_state->req = NULL;
	sha_state->tfm = NULL;
	if (sha_state->result_buff != ctx->sha_result)
		kfree(sha_state->result_buff);
	sha_state->result_buff = NULL;
	sha_state->total_bytes = 0;
	sha_state->digest_size = 0;
	sha_state->remaining_bytes = 0;

done:
	return ret;
}

static int tnvvse_crypto_sha_final(struct tnvvse_crypto_ctx *ctx,
		struct tegra_nvvse_sha_final_ctl *final_ctl)
{
	struct crypto_sha_state *sha_state = &ctx->sha_state;
	struct crypto_ahash *tfm = sha_state->tfm;
	int ret = -ENOMEM;

	if (!sha_state->sha_done_success) {
		pr_err("%s(): SHA is not completed successfully\n", __func__);
		ret = -EFAULT;
		goto stop_sha;
	}

	if (sha_state->result_buff == NULL) {
		pr_err("%s(): SHA is either aborted or not initialized\n", __func__);
		ret = -EFAULT;
		goto stop_sha;
	}

	/* Shake128/Shake256 have variable digest size */
	if ((sha_state->sha_type == TEGRA_NVVSE_SHA_TYPE_SHAKE128) ||
	    (sha_state->sha_type == TEGRA_NVVSE_SHA_TYPE_SHAKE256)) {
		ret = copy_to_user((void __user *)final_ctl->digest_buffer,
				(const void *)sha_state->result_buff,
				final_ctl->digest_size);
	} else {
		if (final_ctl->digest_size != crypto_ahash_digestsize(tfm)) {
			pr_err("%s(): digest size not matching req %d and calculated %d for %s\n",
				__func__, final_ctl->digest_size, crypto_ahash_digestsize(tfm),
				sha_alg_names[sha_state->sha_type]);
			ret = -EINVAL;
			goto stop_sha;
		}

		ret = copy_to_user((void __user *)final_ctl->digest_buffer,
				(const void *)sha_state->result_buff,
				crypto_ahash_digestsize(tfm));
	}
	if (ret) {
		pr_err("%s(): Failed to copy_to_user for %s: %d\n",
					__func__, sha_alg_names[sha_state->sha_type], ret);
	}

stop_sha:
	free_bufs(sha_state->xbuf);
	ahash_request_free(sha_state->req);
	crypto_free_ahash(sha_state->tfm);

	sha_state->req = NULL;
	sha_state->tfm = NULL;
	if (sha_state->result_buff != ctx->sha_result)
		kfree(sha_state->result_buff);
	sha_state->result_buff = NULL;
	sha_state->total_bytes = 0;
	sha_state->digest_size = 0;
	sha_state->remaining_bytes = 0;

	return ret;
}

static int tnvvse_crypto_aes_cmac_sign_verify(struct tnvvse_crypto_ctx *ctx,
				struct tegra_nvvse_aes_cmac_sign_verify_ctl *aes_cmac_ctl)
{
	struct crypto_ahash *tfm;
	char *result, *src_buffer;
	const char *driver_name;
	struct ahash_request *req;
	struct tnvvse_crypto_completion sha_complete;
	unsigned long *xbuf[XBUFSIZE];
	char key_as_keyslot[AES_KEYSLOT_NAME_SIZE] = {0,};
	struct tnvvse_cmac_req_data priv_data;
	int ret = -ENOMEM;
	struct scatterlist sg[1];
	uint32_t size = 0, total = 0;
	void *hash_buff;

	result = kzalloc(64, GFP_KERNEL);
	if (!result)
		return -ENOMEM;

	tfm = crypto_alloc_ahash("cmac-vse(aes)", 0, 0);
	if (IS_ERR(tfm)) {
		ret = PTR_ERR(tfm);
		pr_err("%s(): Failed to allocate ahash for cmac-vse(aes): %d\n", __func__, ret);
		goto free_result;
	}

	driver_name = crypto_tfm_alg_driver_name(crypto_ahash_tfm(tfm));
	if (driver_name == NULL) {
		pr_err("%s(): Failed to get_driver_name for cmac-vse(aes) returned NULL", __func__);
		goto free_tfm;
	}
	pr_debug("%s(): Algo name cmac(aes), driver name %s\n", __func__, driver_name);

	req = ahash_request_alloc(tfm, GFP_KERNEL);
	if (!req) {
		pr_err("%s(): Failed to allocate request for cmac-vse(aes)\n", __func__);
		goto free_tfm;
	}

	ahash_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
				   tnvvse_crypto_complete, &sha_complete);

	ret = alloc_bufs(xbuf);
	if (ret < 0) {
		pr_err("%s(): Failed to allocate xbuffer: %d\n", __func__, ret);
		goto free_req;
	}

	init_completion(&sha_complete.restart);
	sha_complete.req_err = 0;

	crypto_ahash_clear_flags(tfm, ~0U);

	if (aes_cmac_ctl->cmac_type == TEGRA_NVVSE_AES_CMAC_SIGN)
		priv_data.request_type = CMAC_SIGN;
	else
		priv_data.request_type = CMAC_VERIFY;
	ret = snprintf(key_as_keyslot, AES_KEYSLOT_NAME_SIZE, "NVSEAES %x",
						aes_cmac_ctl->key_slot);
	if (ret >= AES_KEYSLOT_NAME_SIZE) {
		pr_err("%s(): Buffer overflow while setting key for cmac-vse(aes): %d\n",
						__func__, ret);
		ret = -EINVAL;
		goto free_xbuf;
	}

	req->priv = &priv_data;
	priv_data.result = 0;
	ret = crypto_ahash_setkey(tfm, key_as_keyslot, aes_cmac_ctl->key_length);
	if (ret) {
		pr_err("%s(): Failed to set keys for cmac-vse(aes): %d\n", __func__, ret);
		goto free_xbuf;
	}

	ret = wait_async_op(&sha_complete, crypto_ahash_init(req));
	if (ret) {
		pr_err("%s(): Failed to initialize ahash: %d\n", __func__, ret);
		goto free_xbuf;
	}

	if (aes_cmac_ctl->cmac_type == TEGRA_NVVSE_AES_CMAC_VERIFY) {
		/* Copy digest */
		ret = copy_from_user((void *)result,
				(void __user *)aes_cmac_ctl->cmac_buffer,
						TEGRA_NVVSE_AES_CMAC_LEN);
		if (ret) {
			pr_err("%s(): Failed to copy_from_user: %d\n", __func__, ret);
			goto free_xbuf;
		}
	}

	hash_buff = xbuf[0];
	total = aes_cmac_ctl->data_length;
	src_buffer = aes_cmac_ctl->src_buffer;
	while (true) {
		size = (total < PAGE_SIZE) ? total : PAGE_SIZE;
		ret = copy_from_user((void *)hash_buff, (void __user *)src_buffer, size);
		if (ret) {
			pr_err("%s(): Failed to copy_from_user: %d\n", __func__, ret);
			goto free_xbuf;
		}

		sg_init_one(&sg[0], hash_buff, size);
		ahash_request_set_crypt(req, sg, result, size);

		if (size < total) {
			ret = wait_async_op(&sha_complete, crypto_ahash_update(req));
			if (ret) {
				pr_err("%s(): Failed to ahash_update: %d\n", __func__, ret);
				goto free_xbuf;
			}
		} else {
			ret = wait_async_op(&sha_complete, crypto_ahash_finup(req));
			if (ret) {
				pr_err("%s(): Failed to ahash_finup: %d\n", __func__, ret);
				goto free_xbuf;
			}
			break;
		}

		total -= size;
		src_buffer += size;
	}

	if (aes_cmac_ctl->cmac_type == TEGRA_NVVSE_AES_CMAC_SIGN) {
		ret = copy_to_user((void __user *)aes_cmac_ctl->cmac_buffer, (const void *)result,
								crypto_ahash_digestsize(tfm));
		if (ret)
			pr_err("%s(): Failed to copy_to_user: %d\n", __func__, ret);
	} else {
		aes_cmac_ctl->result = priv_data.result;
	}

free_xbuf:
	free_bufs(xbuf);
free_req:
	ahash_request_free(req);
free_tfm:
	crypto_free_ahash(tfm);
free_result:
	kfree(result);

	return ret;
}

static int tnvvse_crypto_aes_gmac_init(struct tnvvse_crypto_ctx *ctx,
		struct tegra_nvvse_aes_gmac_init_ctl *gmac_init_ctl)
{
	struct crypto_sha_state *sha_state = &ctx->sha_state;
	char key_as_keyslot[AES_KEYSLOT_NAME_SIZE] = {0,};
	struct crypto_ahash *tfm;
	struct ahash_request *req;
	const char *driver_name;
	uint8_t iv[TEGRA_NVVSE_AES_GCM_IV_LEN];
	struct tnvvse_gmac_req_data priv_data;
	int ret = -ENOMEM, klen;

	tfm = crypto_alloc_ahash("gmac-vse(aes)", 0, 0);
	if (IS_ERR(tfm)) {
		pr_err("%s(): Failed to allocate transform for gmac-vse(aes):%ld\n", __func__,
						PTR_ERR(tfm));
		ret = PTR_ERR(tfm);
		goto out;
	}

	driver_name = crypto_tfm_alg_driver_name(crypto_ahash_tfm(tfm));
	if (driver_name == NULL) {
		pr_err("%s(): Failed to get driver name\n", __func__);
		goto free_tfm;
	}
	pr_debug("%s(): Algo name gmac-vse(aes), driver name %s\n", __func__, driver_name);

	req = ahash_request_alloc(tfm, GFP_KERNEL);
	if (!req) {
		pr_err("%s(): Failed to allocate request for gmac-vse(aes)\n", __func__);
		goto free_tfm;
	}

	ahash_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
				   tnvvse_crypto_complete, &sha_state->sha_complete);

	init_completion(&sha_state->sha_complete.restart);
	sha_state->sha_complete.req_err = 0;

	ret = snprintf(key_as_keyslot, AES_KEYSLOT_NAME_SIZE, "NVSEAES %x", gmac_init_ctl->key_slot);
	if (ret >= AES_KEYSLOT_NAME_SIZE) {
		pr_err("%s(): Buffer overflow while setting key for gmac-vse(aes): %d\n",
					__func__, ret);
		ret = -EINVAL;
		goto free_req;
	}

	klen = gmac_init_ctl->key_length;
	ret = crypto_ahash_setkey(tfm, key_as_keyslot, klen);
	if (ret) {
		pr_err("%s(): Failed to set keys for gmac-vse(aes): %d\n", __func__, ret);
		goto free_req;
	}

	memset(iv, 0, TEGRA_NVVSE_AES_GCM_IV_LEN);
	priv_data.request_type = GMAC_INIT;
	priv_data.iv = iv;
	req->priv = &priv_data;

	ret = wait_async_op(&sha_state->sha_complete, crypto_ahash_init(req));
	if (ret) {
		pr_err("%s(): Failed to ahash_init for gmac-vse(aes): ret=%d\n",
					__func__, ret);
	}

	memcpy(gmac_init_ctl->IV, priv_data.iv, TEGRA_NVVSE_AES_GCM_IV_LEN);

free_req:
	ahash_request_free(req);
free_tfm:
	crypto_free_ahash(tfm);
out:
	return ret;
}

static int tnvvse_crypto_aes_gmac_sign_verify_init(struct tnvvse_crypto_ctx *ctx,
		struct tegra_nvvse_aes_gmac_sign_verify_ctl *gmac_sign_verify_ctl)
{
	struct crypto_sha_state *sha_state = &ctx->sha_state;
	char key_as_keyslot[AES_KEYSLOT_NAME_SIZE] = {0,};
	struct crypto_ahash *tfm;
	struct ahash_request *req;
	struct tnvvse_gmac_req_data priv_data;
	const char *driver_name;
	int ret = -EINVAL, klen;

	tfm = crypto_alloc_ahash("gmac-vse(aes)", 0, 0);
	if (IS_ERR(tfm)) {
		pr_err("%s(): Failed to load transform for gmac-vse(aes):%ld\n", __func__,
							PTR_ERR(tfm));
		ret = PTR_ERR(tfm);
		goto out;
	}

	driver_name = crypto_tfm_alg_driver_name(crypto_ahash_tfm(tfm));
	if (driver_name == NULL) {
		pr_err("%s(): Failed to get driver name\n", __func__);
		goto free_tfm;
	}
	pr_debug("%s(): Algo name gmac-vse(aes), driver name %s\n", __func__, driver_name);

	req = ahash_request_alloc(tfm, GFP_KERNEL);
	if (!req) {
		pr_err("%s(): Failed to allocate request for gmac-vse(aes)\n", __func__);
		goto free_tfm;
	}

	ahash_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
				   tnvvse_crypto_complete, &sha_state->sha_complete);

	ret = alloc_bufs(sha_state->xbuf);
	if (ret < 0) {
		pr_err("%s(): Failed to allocate Xbuffer\n", __func__);
		goto free_req;
	}

	init_completion(&sha_state->sha_complete.restart);
	sha_state->sha_complete.req_err = 0;

	ret = snprintf(key_as_keyslot, AES_KEYSLOT_NAME_SIZE, "NVSEAES %x",
					gmac_sign_verify_ctl->key_slot);
	if (ret >= AES_KEYSLOT_NAME_SIZE) {
		pr_err("%s(): Buffer overflow while setting key for gmac-vse(aes): %d\n",
					__func__, ret);
		ret = -EINVAL;
		goto free_xbuf;
	}

	klen = gmac_sign_verify_ctl->key_length;
	ret = crypto_ahash_setkey(tfm, key_as_keyslot, klen);
	if (ret) {
		pr_err("%s(): Failed to set keys for gmac-vse(aes): %d\n", __func__, ret);
		goto free_xbuf;
	}

	if (gmac_sign_verify_ctl->gmac_type == TEGRA_NVVSE_AES_GMAC_SIGN)
		priv_data.request_type = GMAC_SIGN;
	else
		priv_data.request_type = GMAC_VERIFY;
	req->priv = &priv_data;

	ret = wait_async_op(&sha_state->sha_complete, crypto_ahash_init(req));
	if (ret) {
		pr_err("%s(): Failed to ahash_init for gmac-vse(aes): ret=%d\n",
					__func__, ret);
		goto free_xbuf;
	}

	sha_state->req = req;
	sha_state->tfm = tfm;
	sha_state->result_buff = ctx->sha_result;

	memset(sha_state->result_buff, 0, TEGRA_NVVSE_AES_GCM_TAG_SIZE);

	ret = 0;
	goto out;

free_xbuf:
	free_bufs(ctx->sha_state.xbuf);
free_req:
	ahash_request_free(req);
free_tfm:
	crypto_free_ahash(tfm);
out:
	return ret;
}

static int tnvvse_crypto_aes_gmac_sign_verify(struct tnvvse_crypto_ctx *ctx,
		struct tegra_nvvse_aes_gmac_sign_verify_ctl *gmac_sign_verify_ctl)
{
	struct crypto_sha_state *sha_state = &ctx->sha_state;
	void *hash_buff;
	char *result_buff;
	unsigned long size = 0, total = 0;
	uint8_t iv[TEGRA_NVVSE_AES_GCM_IV_LEN];
	struct ahash_request *req;
	struct scatterlist sg[1];
	char *input_buffer = gmac_sign_verify_ctl->src_buffer;
	struct tnvvse_gmac_req_data priv_data;
	int ret = -EINVAL;

	if (gmac_sign_verify_ctl->data_length > GMAC_MAX_LEN ||
			gmac_sign_verify_ctl->data_length == 0) {
		pr_err("%s(): Failed due to invalid input size: %d\n", __func__, ret);
		goto done;
	}

	if (gmac_sign_verify_ctl->is_last &&
			gmac_sign_verify_ctl->tag_length != TEGRA_NVVSE_AES_GCM_TAG_SIZE) {
		pr_err("%s(): Failed due to invalid tag length (%d) invalid", __func__,
					gmac_sign_verify_ctl->tag_length);
		goto done;
	}

	ret = tnvvse_crypto_aes_gmac_sign_verify_init(ctx, gmac_sign_verify_ctl);
	if (ret) {
		pr_err("%s(): Failed to init: %d\n", __func__, ret);
		goto done;
	}

	hash_buff = sha_state->xbuf[0];
	result_buff = sha_state->result_buff;
	req = sha_state->req;
	total = gmac_sign_verify_ctl->data_length;

	if (gmac_sign_verify_ctl->gmac_type == TEGRA_NVVSE_AES_GMAC_SIGN)
		priv_data.request_type = GMAC_SIGN;
	else
		priv_data.request_type = GMAC_VERIFY;
	priv_data.iv = NULL;
	priv_data.is_first = gmac_sign_verify_ctl->is_first;
	req->priv = &priv_data;

	while (total > 0) {
		size = (total < PAGE_SIZE) ? total : PAGE_SIZE;
		ret = copy_from_user((void *)hash_buff, (void __user *)input_buffer, size);
		if (ret) {
			pr_err("%s(): Failed to copy_from_user: %d\n", __func__, ret);
			goto stop_sha;
		}

		sg_init_one(&sg[0], hash_buff, size);
		ahash_request_set_crypt(req, &sg[0], result_buff, size);
		if (size < total) {
			ret = wait_async_op(&sha_state->sha_complete, crypto_ahash_update(req));
			if (ret) {
				pr_err("%s(): Failed to ahash_update for gmac-vse(aes): %d\n",
							__func__, ret);
				goto stop_sha;
			}
		} else {
			if (gmac_sign_verify_ctl->is_last == 0) {
				ret = wait_async_op(&sha_state->sha_complete,
								crypto_ahash_update(req));
				if (ret) {
					pr_err("%s(): Failed to ahash_update for gmac-vse(aes): %d\n",
								__func__, ret);
					goto stop_sha;
				}
			} else {
				if (gmac_sign_verify_ctl->gmac_type ==
							TEGRA_NVVSE_AES_GMAC_VERIFY) {
					/* Copy tag/digest */
					ret = copy_from_user((void *)result_buff,
						(void __user *)gmac_sign_verify_ctl->tag_buffer,
						TEGRA_NVVSE_AES_GCM_TAG_SIZE);
					if (ret) {
						pr_err("%s(): Failed to copy_from_user: %d\n",
										__func__, ret);
						goto stop_sha;
					}

					memcpy(iv, gmac_sign_verify_ctl->initial_vector,
								TEGRA_NVVSE_AES_GCM_IV_LEN);
					priv_data.iv = iv;
				}
				ret = wait_async_op(&sha_state->sha_complete,
							crypto_ahash_finup(req));
				if (ret) {
					pr_err("%s(): Failed to ahash_finup for gmac-vse(aes): %d\n",
								__func__, ret);
					goto stop_sha;
				}
			}
		}

		total -= size;
		input_buffer += size;
		priv_data.is_first = 0;
	}

	if (gmac_sign_verify_ctl->is_last) {
		if (gmac_sign_verify_ctl->gmac_type == TEGRA_NVVSE_AES_GMAC_SIGN) {
			ret = copy_to_user((void __user *)gmac_sign_verify_ctl->tag_buffer,
				(const void *)result_buff,
				gmac_sign_verify_ctl->tag_length);
			if (ret)
				pr_err("%s(): Failed to copy_to_user:%d\n", __func__, ret);
		} else {
			gmac_sign_verify_ctl->result = priv_data.result;
		}
	}

stop_sha:
	if (sha_state->xbuf[0])
		free_bufs(sha_state->xbuf);
	if (sha_state->req)
		ahash_request_free(sha_state->req);
	if (sha_state->tfm)
		crypto_free_ahash(sha_state->tfm);

	sha_state->req = NULL;
	sha_state->tfm = NULL;
	sha_state->result_buff = NULL;

done:
	return ret;
}

static int tnvvse_crypto_aes_enc_dec(struct tnvvse_crypto_ctx *ctx,
					struct tegra_nvvse_aes_enc_dec_ctl *aes_enc_dec_ctl)
{
	struct crypto_skcipher *tfm;
	struct skcipher_request *req = NULL;
	struct scatterlist in_sg;
	struct scatterlist out_sg;
	unsigned long *xbuf[NBUFS];
	int ret = 0, size = 0;
	unsigned long total = 0;
	struct tnvvse_crypto_completion tcrypt_complete;
	char aes_algo[5][15] = {"cbc-vse(aes)", "ecb-vse(aes)", "ctr-vse(aes)"};
	const char *driver_name;
	char key_as_keyslot[AES_KEYSLOT_NAME_SIZE] = {0,};
	int klen;
	char *pbuf;
	uint8_t next_block_iv[TEGRA_NVVSE_AES_IV_LEN];
	bool first_loop = true;
	uint32_t counter_val;

	if (aes_enc_dec_ctl->aes_mode >= TEGRA_NVVSE_AES_MODE_MAX) {
		pr_err("%s(): The requested AES ENC/DEC (%d) is not supported\n",
					__func__, aes_enc_dec_ctl->aes_mode);
		ret = -EINVAL;
		goto out;
	}

	tfm = crypto_alloc_skcipher(aes_algo[aes_enc_dec_ctl->aes_mode],
						CRYPTO_ALG_TYPE_SKCIPHER | CRYPTO_ALG_ASYNC, 0);
	if (IS_ERR(tfm)) {
		pr_err("%s(): Failed to load transform for %s: %ld\n",
					__func__, aes_algo[aes_enc_dec_ctl->aes_mode], PTR_ERR(tfm));
		ret = PTR_ERR(tfm);
		goto out;
	}

	req = skcipher_request_alloc(tfm, GFP_KERNEL);
	if (!req) {
		pr_err("%s(): Failed to allocate skcipher request\n", __func__);
		ret = -ENOMEM;
		goto free_tfm;
	}

	driver_name = crypto_tfm_alg_driver_name(crypto_skcipher_tfm(tfm));
	if (driver_name == NULL) {
		pr_err("%s(): Failed to get driver name for %s\n", __func__,
						aes_algo[aes_enc_dec_ctl->aes_mode]);
		goto free_req;
	}
	pr_debug("%s(): The skcipher driver name is %s for %s\n",
				__func__, driver_name, aes_algo[aes_enc_dec_ctl->aes_mode]);

	if (((aes_enc_dec_ctl->key_length & CRYPTO_KEY_LEN_MASK) != TEGRA_CRYPTO_KEY_128_SIZE) &&
		((aes_enc_dec_ctl->key_length & CRYPTO_KEY_LEN_MASK) != TEGRA_CRYPTO_KEY_192_SIZE) &&
		((aes_enc_dec_ctl->key_length & CRYPTO_KEY_LEN_MASK) != TEGRA_CRYPTO_KEY_256_SIZE) &&
		((aes_enc_dec_ctl->key_length & CRYPTO_KEY_LEN_MASK) != TEGRA_CRYPTO_KEY_512_SIZE)) {
		ret = -EINVAL;
		pr_err("%s(): crypt_req keylen(%d) invalid", __func__, aes_enc_dec_ctl->key_length);
		goto free_req;
	}

	crypto_skcipher_clear_flags(tfm, ~0);

	if (!aes_enc_dec_ctl->skip_key) {
		ret = snprintf(key_as_keyslot, AES_KEYSLOT_NAME_SIZE, "NVSEAES %x",
			       aes_enc_dec_ctl->key_slot);
		if (ret >= AES_KEYSLOT_NAME_SIZE) {
			pr_err("%s(): Buffer overflow while preparing key for %s: %d\n",
						__func__, aes_algo[aes_enc_dec_ctl->aes_mode], ret);
			ret = -EINVAL;
			goto free_req;
		}

		klen = strlen(key_as_keyslot);
		if(klen != 16) {
			pr_err("%s(): key length is invalid, length %d, key %s\n", __func__, klen, key_as_keyslot);
			ret = -EINVAL;
			goto free_req;
		}
		/* Null key is only allowed in SE driver */
		if (!strstr(driver_name, "tegra")) {
			ret = -EINVAL;
			pr_err("%s(): Failed to identify as tegra se driver\n", __func__);
			goto free_req;
		}

		ret = crypto_skcipher_setkey(tfm, key_as_keyslot, aes_enc_dec_ctl->key_length);
		if (ret < 0) {
			pr_err("%s(): Failed to set key: %d\n", __func__, ret);
			goto free_req;
		}
	}

	ret = alloc_bufs(xbuf);
	if (ret < 0) {
		pr_err("%s(): Failed to allocate xbuffer: %d\n", __func__, ret);
		goto free_req;
	}

	init_completion(&tcrypt_complete.restart);

	skcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
					tnvvse_crypto_complete, &tcrypt_complete);

	total = aes_enc_dec_ctl->data_length;
	if (aes_enc_dec_ctl->aes_mode == TEGRA_NVVSE_AES_MODE_CBC)
		memcpy(next_block_iv, aes_enc_dec_ctl->initial_vector, TEGRA_NVVSE_AES_IV_LEN);
	else if (aes_enc_dec_ctl->aes_mode == TEGRA_NVVSE_AES_MODE_CTR)
		memcpy(next_block_iv, aes_enc_dec_ctl->initial_counter, TEGRA_NVVSE_AES_CTR_LEN);
	else
		memset(next_block_iv, 0, TEGRA_NVVSE_AES_IV_LEN);

	while (total > 0) {
		size = (total < PAGE_SIZE) ? total : PAGE_SIZE;
		ret = copy_from_user((void *)xbuf[0], (void __user *)aes_enc_dec_ctl->src_buffer, size);
		if (ret) {
			pr_err("%s(): Failed to copy_from_user: %d\n", __func__, ret);
			goto free_req;
		}
		sg_init_one(&in_sg, xbuf[0], size);
		sg_init_one(&out_sg, xbuf[1], size);

		skcipher_request_set_crypt(req, &in_sg, &out_sg, size, next_block_iv);

		reinit_completion(&tcrypt_complete.restart);
		skcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
					tnvvse_crypto_complete, &tcrypt_complete);
		tcrypt_complete.req_err = 0;

		/* Set first byte of IV to 1 for first encryption request and 0 for other
		 * encryption requests. This is used to invoke generation of random IV.
		 */
		if (aes_enc_dec_ctl->is_encryption) {
			if (first_loop && !aes_enc_dec_ctl->is_non_first_call)
				next_block_iv[0] = 1;
			else
				next_block_iv[0] = 0;
		}

		ret = aes_enc_dec_ctl->is_encryption ? crypto_skcipher_encrypt(req) :
						crypto_skcipher_decrypt(req);
		if ((ret == -EINPROGRESS) || (ret == -EBUSY)) {
			/* crypto driver is asynchronous */
			ret = wait_for_completion_timeout(&tcrypt_complete.restart,
						msecs_to_jiffies(5000));
			if (ret == 0)
				goto free_xbuf;

			if (tcrypt_complete.req_err < 0) {
				ret = tcrypt_complete.req_err;
				goto free_xbuf;
			}
		} else if (ret < 0) {
			pr_err("%s(): Failed to %scrypt: %d\n",
					__func__, aes_enc_dec_ctl->is_encryption ? "en" : "de", ret);
			goto free_xbuf;
		}

		ret = copy_to_user((void __user *)aes_enc_dec_ctl->dest_buffer,
							(const void *)xbuf[1], size);
		if (ret) {
			ret = -EFAULT;
			pr_err("%s(): Failed to copy_to_user: %d\n", __func__, ret);
			goto free_xbuf;
		}

		if (first_loop && aes_enc_dec_ctl->is_encryption) {
			if (aes_enc_dec_ctl->aes_mode == TEGRA_NVVSE_AES_MODE_CBC)
				memcpy(aes_enc_dec_ctl->initial_vector, req->iv, TEGRA_NVVSE_AES_IV_LEN);
			else if (aes_enc_dec_ctl->aes_mode == TEGRA_NVVSE_AES_MODE_CTR)
				memcpy(aes_enc_dec_ctl->initial_counter, req->iv, TEGRA_NVVSE_AES_CTR_LEN);
		}

		if (!aes_enc_dec_ctl->is_encryption) {
			if (aes_enc_dec_ctl->aes_mode == TEGRA_NVVSE_AES_MODE_CBC) {
				/* Next block IV is last 16bytes of current cypher text */
				pbuf = (char *)xbuf[0];
				memcpy(next_block_iv, pbuf + size - TEGRA_NVVSE_AES_IV_LEN, TEGRA_NVVSE_AES_IV_LEN);
			}  else if (aes_enc_dec_ctl->aes_mode == TEGRA_NVVSE_AES_MODE_CTR) {
				counter_val =  (next_block_iv[12] << 24);
				counter_val |= (next_block_iv[13] << 16);
				counter_val |= (next_block_iv[14] << 8);
				counter_val |= (next_block_iv[15] << 0);
				counter_val += size/16;
				next_block_iv[12] = (counter_val >> 24)	& 0xFFU;
				next_block_iv[13] = (counter_val >> 16)	& 0xFFU;
				next_block_iv[14] = (counter_val >> 8)	& 0xFFU;
				next_block_iv[15] = (counter_val >> 0)	& 0xFFU;
			}
		}
		first_loop = false;
		total -= size;
		aes_enc_dec_ctl->dest_buffer += size;
		aes_enc_dec_ctl->src_buffer += size;
	}

free_xbuf:
	free_bufs(xbuf);

free_req:
	skcipher_request_free(req);

free_tfm:
	crypto_free_skcipher(tfm);

out:
	return ret;
}

static int tnvvse_crypt_aes_gcm_alloc_buf(struct scatterlist **sg, uint8_t *buf[], uint32_t size)
{
	uint32_t nents;
	int32_t ret = 0, i;

	if (sg == NULL) {
		ret = -EINVAL;
		goto out;
	}

	nents = (size/GCM_CHUNK_SIZE + 1);

	*sg = kmalloc_array(nents, sizeof(**sg), GFP_KERNEL);
	if (!*sg) {
		ret = -ENOMEM;
		goto out;
	}

	sg_init_table(*sg, nents);

	for (i = 0; i < nents; i++) {
		buf[i] = kcalloc(GCM_CHUNK_SIZE, sizeof(uint8_t), GFP_KERNEL);
		if (buf[i] == NULL) {
			ret = -ENOMEM;
			goto free_sg;
		}
	}

	for (i = 0 ; i < nents-1; i++)
		sg_set_buf(*sg+i, buf[i], GCM_CHUNK_SIZE);

	sg_set_buf(*sg+i, buf[i], size % GCM_CHUNK_SIZE);

	return ret;

free_sg:
	i--;
	while (i >= 0) {
		kfree(buf[i]);
		i--;
	}

	kfree(*sg);
out:
	return ret;
}

static void tnvvse_crypt_aes_gcm_free_buf(struct scatterlist **sg, uint8_t *buf[], uint32_t size)
{
	uint32_t nents, i;

	nents = (size/GCM_CHUNK_SIZE + 1);
	kfree(*sg);

	for (i = 0 ; i < nents; i++)
		kfree(buf[i]);
}

static int tnvvse_crypto_aes_enc_dec_gcm(struct tnvvse_crypto_ctx *ctx,
				struct tegra_nvvse_aes_enc_dec_ctl *aes_enc_dec_ctl)
{
	struct crypto_aead *tfm;
	struct aead_request *req = NULL;
	struct scatterlist *in_sg, *out_sg;
	uint8_t *in_buf[GCM_MAX_CHUNKS], *out_buf[GCM_MAX_CHUNKS];
	int32_t ret = 0, nents;
	uint32_t in_sz, out_sz, aad_length, data_length, tag_length;
	uint32_t i, idx, offset, data_length_copied, data_length_remaining, tag_length_copied;
	struct tnvvse_crypto_completion tcrypt_complete;
	const char *driver_name;
	char key_as_keyslot[AES_KEYSLOT_NAME_SIZE] = {0,};
	uint8_t iv[TEGRA_NVVSE_AES_GCM_IV_LEN];
	bool enc;

	if (aes_enc_dec_ctl->aes_mode != TEGRA_NVVSE_AES_MODE_GCM) {
		pr_err("%s(): The requested AES ENC/DEC (%d) is not supported\n",
					__func__, aes_enc_dec_ctl->aes_mode);
		ret = -EINVAL;
		goto out;
	}

	if (aes_enc_dec_ctl->data_length > GCM_PT_MAX_LEN
				|| aes_enc_dec_ctl->aad_length > GCM_AAD_MAX_LEN) {
		pr_err("%s(): Input size is (data = %d, aad = %d) is not supported\n",
					__func__, aes_enc_dec_ctl->data_length,
					aes_enc_dec_ctl->aad_length);
		ret = -EINVAL;
		goto out;
	}

	tfm = crypto_alloc_aead("gcm-vse(aes)", CRYPTO_ALG_TYPE_AEAD | CRYPTO_ALG_ASYNC, 0);
	if (IS_ERR(tfm)) {
		pr_err("%s(): Failed to load transform for gcm-vse(aes): %ld\n",
					__func__, PTR_ERR(tfm));
		ret = PTR_ERR(tfm);
		goto out;
	}

	req = aead_request_alloc(tfm, GFP_KERNEL);
	if (!req) {
		pr_err("%s(): Failed to allocate skcipher request\n", __func__);
		ret = -ENOMEM;
		goto free_tfm;
	}

	driver_name = crypto_tfm_alg_driver_name(crypto_aead_tfm(tfm));
	if (driver_name == NULL) {
		pr_err("%s(): Failed to get driver name for gcm-vse(aes)\n", __func__);
		goto free_req;
	}
	pr_debug("%s(): The aead driver name is %s for gcm-vse(aes)\n",
						__func__, driver_name);

	if ((aes_enc_dec_ctl->key_length != TEGRA_CRYPTO_KEY_128_SIZE) &&
		(aes_enc_dec_ctl->key_length != TEGRA_CRYPTO_KEY_192_SIZE) &&
		(aes_enc_dec_ctl->key_length != TEGRA_CRYPTO_KEY_256_SIZE)) {
		ret = -EINVAL;
		pr_err("%s(): crypt_req keylen(%d) invalid", __func__, aes_enc_dec_ctl->key_length);
		goto free_req;
	}

	if (aes_enc_dec_ctl->tag_length != TEGRA_NVVSE_AES_GCM_TAG_SIZE) {
		ret = -EINVAL;
		pr_err("%s(): crypt_req taglen(%d) invalid", __func__, aes_enc_dec_ctl->tag_length);
		goto free_req;
	}

	crypto_aead_clear_flags(tfm, ~0);

	if (!aes_enc_dec_ctl->skip_key) {
		ret = snprintf(key_as_keyslot, AES_KEYSLOT_NAME_SIZE, "NVSEAES %x",
				aes_enc_dec_ctl->key_slot);
		if (ret >= AES_KEYSLOT_NAME_SIZE) {
			pr_err("%s(): Buffer overflow while preparing key for gcm(aes): %d\n",
						__func__, ret);
			ret = -EINVAL;
			goto free_req;
		}

		ret = crypto_aead_setkey(tfm, key_as_keyslot, aes_enc_dec_ctl->key_length);
		if (ret < 0) {
			pr_err("%s(): Failed to set key: %d\n", __func__, ret);
			goto free_req;
		}
	}

	ret = crypto_aead_setauthsize(tfm, aes_enc_dec_ctl->tag_length);
	if (ret < 0) {
		pr_err("%s(): Failed to set tag size: %d\n", __func__, ret);
		goto free_req;
	}

	init_completion(&tcrypt_complete.restart);
	tcrypt_complete.req_err = 0;

	enc = aes_enc_dec_ctl->is_encryption;
	data_length = aes_enc_dec_ctl->data_length;
	tag_length = aes_enc_dec_ctl->tag_length;
	aad_length = aes_enc_dec_ctl->aad_length;

	aead_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
					tnvvse_crypto_complete, &tcrypt_complete);
	aead_request_set_ad(req, aad_length);

	memset(iv, 0, TEGRA_NVVSE_AES_GCM_IV_LEN);
	if (!enc)
		memcpy(iv, aes_enc_dec_ctl->initial_vector, TEGRA_NVVSE_AES_GCM_IV_LEN);
	else if (enc && !aes_enc_dec_ctl->is_non_first_call)
		/* Set first byte of IV to 1 for first encryption request. This is used to invoke
		 * generation of random IV.
		 */
		iv[0] = 1;

	/* Prepare buffers
	 * - AEAD encryption input:  assoc data || plaintext
	 * - AEAD encryption output: assoc data || ciphertext || auth tag
	 * - AEAD decryption input:  assoc data || ciphertext || auth tag
	 * - AEAD decryption output: assoc data || plaintext
	 */
	in_sz = enc ? aad_length + data_length :
			aad_length + data_length + tag_length;
	ret = tnvvse_crypt_aes_gcm_alloc_buf(&in_sg, in_buf, in_sz);
	if (ret < 0)
		goto free_req;

	out_sz = enc ? aad_length + data_length + tag_length :
			aad_length + data_length;
	ret = tnvvse_crypt_aes_gcm_alloc_buf(&out_sg, out_buf, out_sz);
	if (ret < 0)
		goto free_in_buf;

	/* copy AAD buffer */
	nents = (in_sz / GCM_CHUNK_SIZE + 1);
	idx = 0;
	offset = 0;

	/* copy AAD of size GCM_CHUNK_SIZE */
	for (i = 0; i < (aad_length/GCM_CHUNK_SIZE); i++) {
		ret = copy_from_user((void *)in_buf[idx],
			(void __user *)(aes_enc_dec_ctl->aad_buffer + i*GCM_CHUNK_SIZE),
			GCM_CHUNK_SIZE);
		if (ret) {
			pr_err("%s(): Failed to copy_from_user assoc data: %d\n", __func__, ret);
			goto free_buf;
		}
		idx++;
	}

	/* copy residual AAD */
	if (aad_length % GCM_CHUNK_SIZE) {
		ret = copy_from_user((void *)in_buf[idx],
			(void __user *)(aes_enc_dec_ctl->aad_buffer + i*GCM_CHUNK_SIZE),
			aad_length % GCM_CHUNK_SIZE);
		if (ret) {
			pr_err("%s(): Failed copy_from_user residual aad data:%d\n", __func__, ret);
			goto free_buf;
		}
		offset = aad_length % GCM_CHUNK_SIZE;
	}

	data_length_copied = 0;
	data_length_remaining = data_length;
	/* copy data buffer */
	/* if in_buf chunk filled with partial aad */
	if (offset && data_length) {
		data_length_copied = min(GCM_CHUNK_SIZE - offset, data_length);
		ret = copy_from_user((void *)(in_buf[idx]+offset),
			(void __user *)(aes_enc_dec_ctl->src_buffer),
			data_length_copied);
		if (ret) {
			pr_err("%s(): Failed to copy_from_user src data: %d\n", __func__, ret);
			goto free_buf;
		}
		/* Check if src data is completely copied */
		if (data_length_copied == data_length) {
			offset = offset + data_length_copied;
		} else {
			offset = 0;
			idx++;
		}
		data_length_remaining = data_length - data_length_copied;
	}

	/* copy data of size GCM_CHUNK_SIZE */
	for (i = 0; data_length_remaining &&
			(i < (data_length_remaining/GCM_CHUNK_SIZE)); i++) {
		ret = copy_from_user((void *)in_buf[idx],
			(void __user *)(aes_enc_dec_ctl->src_buffer
			 + data_length_copied + i*GCM_CHUNK_SIZE),
			GCM_CHUNK_SIZE);
		if (ret) {
			pr_err("%s(): Failed to copy_from_user src data: %d\n", __func__, ret);
			goto free_buf;
		}
		idx++;
	}

	/* copy residual of data */
	if (data_length_remaining % GCM_CHUNK_SIZE) {
		ret = copy_from_user((void *)in_buf[idx],
			(void __user *)(aes_enc_dec_ctl->src_buffer
			 + data_length_copied + i*GCM_CHUNK_SIZE),
			data_length_remaining % GCM_CHUNK_SIZE);
		if (ret) {
			pr_err("%s(): Failed copy_from_user residual src data:%d\n", __func__, ret);
			goto free_buf;
		}
		offset = (data_length_remaining % GCM_CHUNK_SIZE);
	}

	/* copy TAG buffer in case of decryption */
	if (!enc) {
		/* Check if tag fits in last buffer */
		if (GCM_CHUNK_SIZE - offset > TEGRA_NVVSE_AES_GCM_TAG_SIZE) {
			ret = copy_from_user((void *)(in_buf[idx] + offset),
					(void __user *)aes_enc_dec_ctl->tag_buffer, tag_length);
			if (ret) {
				pr_err("%s(): Failed copy_from_user tag data: %d\n", __func__, ret);
				goto free_buf;
			}
		} else {
			ret = copy_from_user((void *)(in_buf[idx] + offset),
					(void __user *)aes_enc_dec_ctl->tag_buffer,
					GCM_CHUNK_SIZE - offset);
			if (ret) {
				pr_err("%s(): Failed copy_from_user tag data: %d\n", __func__, ret);
				goto free_buf;
			}
			tag_length_copied = GCM_CHUNK_SIZE - offset;

			ret = copy_from_user((void *)(in_buf[idx+1]),
					(void __user *)(aes_enc_dec_ctl->tag_buffer
					+ tag_length_copied),
					TEGRA_NVVSE_AES_GCM_TAG_SIZE - tag_length_copied);
			if (ret) {
				pr_err("%s(): Failed copy_from_user tag data: %d\n", __func__, ret);
				goto free_buf;
			}
		}
	}

	aead_request_set_crypt(req, in_sg, out_sg,
				enc ? data_length : data_length + tag_length,
				iv);

	ret =  enc ? crypto_aead_encrypt(req) : crypto_aead_decrypt(req);
	if ((ret == -EINPROGRESS) || (ret == -EBUSY)) {
		/* crypto driver is asynchronous */
		ret = wait_for_completion_timeout(&tcrypt_complete.restart,
					msecs_to_jiffies(5000));
		if (ret == 0)
			goto free_buf;

		if (tcrypt_complete.req_err < 0) {
			ret = tcrypt_complete.req_err;
			goto free_buf;
		}
	} else if (ret < 0) {
		pr_err("%s(): Failed to %scrypt: %d\n",
				__func__, enc ? "en" : "de", ret);
		goto free_buf;
	}

	/* copy to output data buffer */
	/* skip aad part from out_buf */
	idx = aad_length / GCM_CHUNK_SIZE;
	offset = aad_length % GCM_CHUNK_SIZE;

	/* if out_buf chunk filled with partial aad */
	data_length_copied = 0;
	data_length_remaining = data_length;
	if (offset && data_length) {
		data_length_copied = min(GCM_CHUNK_SIZE - offset, data_length);
		ret = copy_to_user((void __user *)aes_enc_dec_ctl->dest_buffer,
					(const void *)(out_buf[idx] + offset),
					data_length_copied);
		if (ret) {
			ret = -EFAULT;
			pr_err("%s(): Failed to copy_to_user dst data: %d\n", __func__, ret);
			goto free_buf;
		}

		if (data_length_copied == data_length) {
			offset = offset + data_length_copied;
		} else {
			offset = 0;
			idx++;
		}
		data_length_remaining = data_length - data_length_copied;
	}

	/* copy data of size GCM_CHUNK_SIZE */
	for (i = 0; data_length_remaining &&
			(i < (data_length_remaining/GCM_CHUNK_SIZE)); i++) {
		ret = copy_to_user((void __user *)aes_enc_dec_ctl->dest_buffer
					+ data_length_copied + i*GCM_CHUNK_SIZE,
					(const void *)(out_buf[idx]),
					GCM_CHUNK_SIZE);
		if (ret) {
			pr_err("%s(): Failed to copy_to_user dst data: %d\n", __func__, ret);
			goto free_buf;
		}
		idx++;
	}

	/* copy residual of data */
	if (data_length_remaining % GCM_CHUNK_SIZE) {
		ret = copy_to_user((void __user *)aes_enc_dec_ctl->dest_buffer
					+ data_length_copied + i*GCM_CHUNK_SIZE,
					(const void *)(out_buf[idx]),
					data_length_remaining % GCM_CHUNK_SIZE);
		if (ret) {
			pr_err("%s(): Failed copy_from_user residual dst data:%d\n", __func__, ret);
			goto free_buf;
		}
		offset = (data_length_remaining % GCM_CHUNK_SIZE);
	}

	if (enc) {
		/* Check if tag fits in last buffer */
		if (GCM_CHUNK_SIZE - offset > TEGRA_NVVSE_AES_GCM_TAG_SIZE) {
			ret = copy_to_user(
					(void __user *)aes_enc_dec_ctl->tag_buffer,
					(const void *)(out_buf[idx] + offset), tag_length);
			if (ret) {
				pr_err("%s(): Failed copy_to_user tag data: %d\n", __func__, ret);
				goto free_buf;
			}
		} else {
			ret = copy_to_user(
					(void __user *)aes_enc_dec_ctl->tag_buffer,
					(const void *)(out_buf[idx] + offset),
					GCM_CHUNK_SIZE - offset);
			if (ret) {
				pr_err("%s(): Failed copy_to_user tag data: %d\n", __func__, ret);
				goto free_buf;
			}
			tag_length_copied = GCM_CHUNK_SIZE - offset;

			ret = copy_to_user((void __user *)aes_enc_dec_ctl->tag_buffer
					+ tag_length_copied,
					(const void *)(out_buf[idx+1]),
					TEGRA_NVVSE_AES_GCM_TAG_SIZE - tag_length_copied);
			if (ret) {
				pr_err("%s(): Failed copy_to_user tag data: %d\n", __func__, ret);
				goto free_buf;
			}
		}

		memcpy(aes_enc_dec_ctl->initial_vector, req->iv, TEGRA_NVVSE_AES_GCM_IV_LEN);
	}

free_buf:
	tnvvse_crypt_aes_gcm_free_buf(&out_sg, out_buf, out_sz);
free_in_buf:
	tnvvse_crypt_aes_gcm_free_buf(&in_sg, in_buf, in_sz);
free_req:
	aead_request_free(req);
free_tfm:
	crypto_free_aead(tfm);
out:
	return ret;
}

static int tnvvse_crypto_get_aes_drng(struct tnvvse_crypto_ctx *ctx,
		struct tegra_nvvse_aes_drng_ctl *aes_drng_ctl)
{
	struct crypto_rng *rng;
	int ret = -ENOMEM;

	rng = crypto_alloc_rng("rng_drbg", 0, 0);
	if (IS_ERR(rng)) {
		ret = PTR_ERR(rng);
		pr_err("(%s(): Failed to allocate crypto for rng_dbg, %d\n", __func__, ret);
		goto out;
	}

	memset(ctx->rng_buff, 0, ctx->max_rng_buff);
	ret = crypto_rng_get_bytes(rng, ctx->rng_buff, aes_drng_ctl->data_length);
	if (ret < 0) {
		pr_err("%s(): Failed to obtain the correct amount of random data for (req %d), %d\n",
				__func__, aes_drng_ctl->data_length, ret);
		goto free_rng;
	}

	ret = copy_to_user((void __user *)aes_drng_ctl->dest_buff,
				(const void *)ctx->rng_buff, aes_drng_ctl->data_length);
	if (ret) {
		pr_err("%s(): Failed to copy_to_user for length %d: %d\n",
				__func__, aes_drng_ctl->data_length, ret);
	}

free_rng:
	crypto_free_rng(rng);
out:
	return ret;
}

static int tnvvse_crypto_dev_open(struct inode *inode, struct file *filp)
{
	struct tnvvse_crypto_ctx *ctx;
	int ret = 0;

	ctx = kzalloc(sizeof(struct tnvvse_crypto_ctx), GFP_KERNEL);
	if (!ctx) {
		return -ENOMEM;
	}

	mutex_init(&ctx->lock);

	ctx->rng_buff = kzalloc(NVVSE_MAX_RANDOM_NUMBER_LEN_SUPPORTED, GFP_KERNEL);
	if (!ctx->rng_buff) {
		ret = -ENOMEM;
		goto free_mutex;
	}
	ctx->max_rng_buff = NVVSE_MAX_RANDOM_NUMBER_LEN_SUPPORTED;

	/* Allocate buffer for SHA result */
	ctx->sha_result = kzalloc(NVVSE_MAX_ALLOCATED_SHA_RESULT_BUFF_SIZE, GFP_KERNEL);
	if (!ctx->sha_result) {
		ret = -ENOMEM;
		goto free_rng_buf;
	}

	filp->private_data = ctx;

	return ret;

free_rng_buf:
	kfree(ctx->rng_buff);
free_mutex:
	mutex_destroy(&ctx->lock);
	kfree(ctx);
	return ret;
}

static int tnvvse_crypto_dev_release(struct inode *inode, struct file *filp)
{
	struct tnvvse_crypto_ctx *ctx = filp->private_data;
	int ret = 0;

	mutex_destroy(&ctx->lock);
	kfree(ctx->sha_result);
	kfree(ctx->rng_buff);
	kfree(ctx);
	filp->private_data = NULL;

	return ret;
}


static long tnvvse_crypto_dev_ioctl(struct file *filp,
	unsigned int ioctl_num, unsigned long arg)
{
	struct tnvvse_crypto_ctx *ctx = filp->private_data;
	struct tegra_nvvse_aes_enc_dec_ctl __user *arg_aes_enc_dec_ctl = (void __user *)arg;
	struct tegra_nvvse_aes_gmac_init_ctl __user *arg_aes_gmac_init_ctl = (void __user *)arg;
	struct tegra_nvvse_aes_gmac_sign_verify_ctl __user *arg_aes_gmac_sign_verify_ctl;
	struct tegra_nvvse_aes_cmac_sign_verify_ctl __user *arg_aes_cmac_sign_verify_ctl;
	struct tegra_nvvse_sha_init_ctl sha_init_ctl;
	struct tegra_nvvse_sha_update_ctl sha_update_ctl;
	struct tegra_nvvse_sha_final_ctl sha_final_ctl;
	struct tegra_nvvse_aes_enc_dec_ctl aes_enc_dec_ctl;
	struct tegra_nvvse_aes_cmac_sign_verify_ctl aes_cmac_sign_verify_ctl;
	struct tegra_nvvse_aes_drng_ctl aes_drng_ctl;
	struct tegra_nvvse_aes_gmac_init_ctl aes_gmac_init_ctl;
	struct tegra_nvvse_aes_gmac_sign_verify_ctl aes_gmac_sign_verify_ctl;
	int ret = 0;

	/*
	 * Avoid processing ioctl if the file has been closed.
	 * This will prevent crashes caused by NULL pointer dereference.
	 */
	if (!ctx) {
		pr_err("%s(): ctx not allocated\n", __func__);
		return -EPERM;
	}

	mutex_lock(&ctx->lock);

	switch (ioctl_num) {
	case NVVSE_IOCTL_CMDID_INIT_SHA:
		ret = copy_from_user(&sha_init_ctl, (void __user *)arg, sizeof(sha_init_ctl));
		if (ret) {
			pr_err("%s(): Failed to copy_from_user sha_init_ctl:%d\n", __func__, ret);
			goto out;
		}

		ret = tnvvse_crypto_sha_init(ctx, &sha_init_ctl);
		break;

	case NVVSE_IOCTL_CMDID_UPDATE_SHA:
		ret = copy_from_user(&sha_update_ctl, (void __user *)arg, sizeof(sha_update_ctl));
		if (ret) {
			pr_err("%s(): Failed to copy_from_user sha_update_ctl:%d\n", __func__, ret);
			goto out;
		}

		ret = tnvvse_crypto_sha_update(ctx, &sha_update_ctl);
		break;


	case NVVSE_IOCTL_CMDID_FINAL_SHA:
		ret = copy_from_user(&sha_final_ctl, (void __user *)arg, sizeof(sha_final_ctl));
		if (ret) {
			pr_err("%s(): Failed to copy_from_user sha_final_ctl:%d\n", __func__, ret);
			goto out;
		}

		ret = tnvvse_crypto_sha_final(ctx, &sha_final_ctl);
		break;

	case NVVSE_IOCTL_CMDID_AES_ENCDEC:
		ret = copy_from_user(&aes_enc_dec_ctl, (void __user *)arg, sizeof(aes_enc_dec_ctl));
		if (ret) {
			pr_err("%s(): Failed to copy_from_user aes_enc_dec_ctl:%d\n", __func__, ret);
			goto out;
		}

		if (aes_enc_dec_ctl.aes_mode == TEGRA_NVVSE_AES_MODE_GCM)
			ret = tnvvse_crypto_aes_enc_dec_gcm(ctx, &aes_enc_dec_ctl);
		else
			ret = tnvvse_crypto_aes_enc_dec(ctx, &aes_enc_dec_ctl);

		if (ret) {
			goto out;
		}

		/* Copy IV returned by VSE */
		if (aes_enc_dec_ctl.is_encryption) {
			if (aes_enc_dec_ctl.aes_mode == TEGRA_NVVSE_AES_MODE_CBC ||
				aes_enc_dec_ctl.aes_mode == TEGRA_NVVSE_AES_MODE_GCM)
				ret = copy_to_user(arg_aes_enc_dec_ctl->initial_vector,
							aes_enc_dec_ctl.initial_vector,
							sizeof(aes_enc_dec_ctl.initial_vector));
			else if (aes_enc_dec_ctl.aes_mode == TEGRA_NVVSE_AES_MODE_CTR)
				ret = copy_to_user(arg_aes_enc_dec_ctl->initial_counter,
							aes_enc_dec_ctl.initial_counter,
							sizeof(aes_enc_dec_ctl.initial_counter));
			if (ret) {
				pr_err("%s(): Failed to copy_to_user:%d\n", __func__, ret);
				goto out;
			}
		}
		break;

	case NVVSE_IOCTL_CMDID_AES_GMAC_INIT:
		ret = copy_from_user(&aes_gmac_init_ctl, (void __user *)arg,
						sizeof(aes_gmac_init_ctl));
		if (ret) {
			pr_err("%s(): Failed to copy_from_user aes_gmac_init_ctl:%d\n",
								__func__, ret);
			goto out;
		}

		ret = tnvvse_crypto_aes_gmac_init(ctx, &aes_gmac_init_ctl);
		if (ret)
			goto out;

		/* Copy IV returned by VSE */
		ret = copy_to_user(arg_aes_gmac_init_ctl->IV, aes_gmac_init_ctl.IV,
							sizeof(aes_gmac_init_ctl.IV));
		if (ret) {
			pr_err("%s(): Failed to copy_to_user:%d\n", __func__, ret);
			goto out;
		}
		break;

	case NVVSE_IOCTL_CMDID_AES_GMAC_SIGN_VERIFY:
		arg_aes_gmac_sign_verify_ctl = (void __user *)arg;
		ret = copy_from_user(&aes_gmac_sign_verify_ctl, (void __user *)arg,
						sizeof(aes_gmac_sign_verify_ctl));
		if (ret) {
			pr_err("%s(): Failed to copy_from_user aes_gmac_sign_verify_ctl:%d\n",
								 __func__, ret);
			goto out;
		}

		ret = tnvvse_crypto_aes_gmac_sign_verify(ctx, &aes_gmac_sign_verify_ctl);
		if (ret)
			goto out;

		if (aes_gmac_sign_verify_ctl.gmac_type == TEGRA_NVVSE_AES_GMAC_VERIFY) {
			ret = copy_to_user(&arg_aes_gmac_sign_verify_ctl->result,
						&aes_gmac_sign_verify_ctl.result,
								sizeof(uint8_t));
			if (ret)
				pr_err("%s(): Failed to copy_to_user:%d\n", __func__, ret);
		}
		break;

	case NVVSE_IOCTL_CMDID_AES_CMAC_SIGN_VERIFY:
		arg_aes_cmac_sign_verify_ctl = (void __user *)arg;
		ret = copy_from_user(&aes_cmac_sign_verify_ctl, (void __user *)arg,
					sizeof(aes_cmac_sign_verify_ctl));
		if (ret) {
			pr_err("%s(): Failed to copy_from_user aes_cmac_sign_verify:%d\n",
						__func__, ret);
			goto out;
		}

		ret = tnvvse_crypto_aes_cmac_sign_verify(ctx, &aes_cmac_sign_verify_ctl);
		if (ret)
			goto out;

		if (aes_cmac_sign_verify_ctl.cmac_type == TEGRA_NVVSE_AES_CMAC_VERIFY) {
			ret = copy_to_user(&arg_aes_cmac_sign_verify_ctl->result,
						&aes_cmac_sign_verify_ctl.result,
								sizeof(uint8_t));
			if (ret)
				pr_err("%s(): Failed to copy_to_user:%d\n", __func__, ret);
		}
		break;

	case NVVSE_IOCTL_CMDID_AES_DRNG:
		ret = copy_from_user(&aes_drng_ctl, (void __user *)arg, sizeof(aes_drng_ctl));
		if (ret) {
			pr_err("%s(): Failed to copy_from_user aes_drng_ctl:%d\n", __func__, ret);
			goto out;
		}
		ret = tnvvse_crypto_get_aes_drng(ctx, &aes_drng_ctl);
		break;

	default:
		pr_err("%s(): invalid ioctl code(%d[0x%08x])", __func__, ioctl_num, ioctl_num);
		ret = -EINVAL;
		break;
	}

out:
	mutex_unlock(&ctx->lock);

	return ret;
}

static const struct file_operations tnvvse_crypto_fops = {
	.owner			= THIS_MODULE,
	.open			= tnvvse_crypto_dev_open,
	.release		= tnvvse_crypto_dev_release,
	.unlocked_ioctl		= tnvvse_crypto_dev_ioctl,
};

static struct miscdevice tnvvse_crypto_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "tegra-nvvse-crypto",
	.fops = &tnvvse_crypto_fops,
};

module_misc_device(tnvvse_crypto_device);

MODULE_DESCRIPTION("Tegra NVVSE Crypto device driver.");
MODULE_AUTHOR("NVIDIA Corporation");
MODULE_LICENSE("GPL v2");

