/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/firmware.h>
#include <linux/nvhost.h>
#include <linux/slab.h>

#include "pva.h"
#include "pva_bit_helpers.h"
#include "pva_vpu_app_auth.h"
#include "pva_sha256.h"

struct pva_buff_s {
	const uint8_t	*buff;
	uint32_t	pos;
	uint32_t	size;
};

s32 read_buff(struct pva_buff_s *src_buf, void *dst, u32 size)
{
	u32  pos = src_buf->pos + size;

	if (pos > src_buf->size)
		return -1;

	memcpy(dst, (src_buf->buff +  src_buf->pos), size);
	src_buf->pos = pos;

	return size;
}

static int
pva_auth_allow_list_parse_pva_buff(struct platform_device *pdev,
				   struct pva_vpu_auth_s *pva_auth,
				   struct pva_buff_s *auth_list_buf)
{
	int err = 0;

	ssize_t read_bytes = 0;
	struct vpu_hash_key_pair_s *vhashk;
	size_t vkey_size = 0;
	size_t vhash_size = 0;

	//Destroy previously parsed allowlist data
	pva_auth_allow_list_destroy(pva_auth);
	vhashk = kzalloc(sizeof(struct vpu_hash_key_pair_s), GFP_KERNEL);
	if (vhashk == NULL) {
		nvpva_warn(&pdev->dev, "ERROR: Unable to allocate memory");
		err = -ENOMEM;
		goto out;
	}

	read_bytes = read_buff(auth_list_buf,
			       &(vhashk->num_keys),
			       sizeof(vhashk->num_keys));
	if (read_bytes != (ssize_t)(sizeof(vhashk->num_keys))) {
		nvpva_warn(&pdev->dev, "ERROR: read failed");
		err = -EINVAL;
		goto free_vhashk;
	}

	vkey_size = sizeof(struct shakey_s)*(vhashk->num_keys);
	vhashk->psha_key = kzalloc(vkey_size, GFP_KERNEL);
	if (vhashk->psha_key == NULL) {
		nvpva_warn(&pdev->dev, "ERROR: Unable to allocate memory");
		err = -ENOMEM;
		goto free_vhashk;
	}

	read_bytes = read_buff(auth_list_buf, vhashk->psha_key, vkey_size);
	if (read_bytes != (ssize_t)vkey_size) {
		err = -EINVAL;
		goto free_shakeys;
	}

	read_bytes = read_buff(auth_list_buf,
			       &(vhashk->num_hashes),
			       sizeof(vhashk->num_hashes));
	if (read_bytes != (ssize_t)(sizeof(vhashk->num_hashes))) {
		nvpva_warn(&pdev->dev, "ERROR: read failed");
		err = -EINVAL;
		goto free_shakeys;
	}

	vhash_size = sizeof(struct vpu_hash_vector_s)*(vhashk->num_hashes);
	vhashk->pvpu_hash_vector = kzalloc(vhash_size, GFP_KERNEL);
	if (vhashk->pvpu_hash_vector == NULL) {
		nvpva_warn(&pdev->dev, "ERROR: read failed");
		err = -ENOMEM;
		goto free_shakeys;
	}

	read_bytes = read_buff(auth_list_buf,
			       vhashk->pvpu_hash_vector,
			       vhash_size);
	if (read_bytes != (ssize_t)vhash_size) {
		nvpva_warn(&pdev->dev, "ERROR: read failed");
		err = -EINVAL;
		goto free_hashes;
	}

	pva_auth->pva_auth_allow_list_parsed = true;
	pva_auth->pva_auth_enable = true;
	pva_auth->vpu_hash_keys = vhashk;
	goto out;

free_hashes:
	kfree(vhashk->pvpu_hash_vector);
	vhashk->pvpu_hash_vector = NULL;

free_shakeys:
	kfree(vhashk->psha_key);
	vhashk->psha_key = NULL;

free_vhashk:

	kfree(vhashk);
	vhashk = NULL;

out:
	return err;
}

int
pva_auth_allow_list_parse_buf(struct platform_device *pdev,
			      struct pva_vpu_auth_s *pva_auth,
			      u8 *buffer,
			      u32 length)
{
	int err = 0;
	struct pva_buff_s auth_list_buf = {0};

	auth_list_buf.buff = buffer;
	auth_list_buf.size = length;
	auth_list_buf.pos  = 0;

	err = pva_auth_allow_list_parse_pva_buff(pdev,
						 pva_auth,
						 &auth_list_buf);
	return err;
}

int
pva_auth_allow_list_parse(struct platform_device *pdev,
			  struct pva_vpu_auth_s *pva_auth)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct pva *pva = pdata->private_data;
	const struct firmware *pallow_list;
	struct pva_buff_s auth_list_buf = {0};
	int err = 0;

	err = nvpva_request_firmware(pdev,
		PVA_AUTH_ALLOW_LIST_DEFAULT, &pallow_list);

	if (err) {
		nvpva_dbg_fn(pva, "pva allow list request failed");
		nvpva_warn(&pdev->dev,
			"Failed to load the allow list\n");
		err = -ENOENT;
		goto out;
	}

	auth_list_buf.buff = pallow_list->data;
	auth_list_buf.size = pallow_list->size;
	auth_list_buf.pos  = 0;
	err = pva_auth_allow_list_parse_pva_buff(pdev,
						 pva_auth,
						 &auth_list_buf);
	release_firmware(pallow_list);
out:
	return err;
}

void
pva_auth_allow_list_destroy(struct pva_vpu_auth_s *pva_auth)
{
	if (pva_auth->vpu_hash_keys == NULL)
		return;

	kfree(pva_auth->vpu_hash_keys->pvpu_hash_vector);
	kfree(pva_auth->vpu_hash_keys->psha_key);
	kfree(pva_auth->vpu_hash_keys);
	pva_auth->vpu_hash_keys = NULL;
}

/**
 * \brief
 * is_key_match calculates the sha256 key of ELF and checks if it matches with key.
 * \param[in] dataptr Pointer to the data to which sha256 to ba calculated
 * \param[in] size length in bytes of the data to which sha256 to be calculated.
 * \param[in] key the key with which calculated key would be compared for match.
 * \return The completion status of the operation. Possible values are:
 * \ref 0 Success. Passed in key matched wth calculated key.
 * \ref -EINVAL. Passed in Key doesn't match with calcualted key.
 */
static int32_t
is_key_match(uint8_t *dataptr,
	     size_t size,
	     struct shakey_s key)
{
	int32_t err = 0;
	uint32_t calc_key[8];
	size_t off;
	struct sha256_ctx_s ctx1;
	struct sha256_ctx_s ctx2;

	sha256_init(&ctx1);
	off = (size / 64U) * 64U;
	if (off > 0U)
		pva_sha256_update(&ctx1, dataptr, off);

	/* clone */
	sha256_copy(&ctx1, &ctx2);

	/* finalize with leftover, if any */
	sha256_finalize(&ctx2, dataptr + off, size % 64U, calc_key);

	err = memcmp((void *)&(key.sha_key),
		     (void *)calc_key,
		     NVPVA_SHA256_DIGEST_SIZE);
	if (err != 0)
		err = -EINVAL;

	return err;
}

/**
 * \brief
 * Keeps checking all the keys accociated with match_hash
 * against the calculated sha256 key for dataptr, until it finds a match.
 * \param[in] pva  Pointer to PVA driver context structure struct \ref nvpva_drv_ctx
 * \param[in] dataptr pointer to ELF data
 * \param[in] size length (in bytes) of ELF data
 * \param[in] match_hash pointer to matching hash structure, \ref struct vpu_hash_vector_s.
 * \return Matching status of the calculated key
 * against the keys asscociated with match_hash. possible values:
 * - 0 Success, one of the keys associated with match_hash
 * matches with the calculated sha256 key.
 * - -EINVAL, None matches.
 */
static int
check_all_keys_for_match(struct shakey_s *pallkeys,
			 uint8_t *dataptr,
			 size_t size,
			 const struct vpu_hash_vector_s *match_hash)
{
	int32_t err = 0;
	uint32_t idx;
	uint32_t count;
	struct shakey_s key;
	uint32_t i;

	idx = match_hash->index;
	count = match_hash->count;
	if (idx > UINT_MAX - count) {
		err = -ERANGE;
		goto fail;
	}

	for (i = 0; i < count; i++) {
		key = pallkeys[idx+i];
		err = is_key_match(dataptr, size, key);
		if (err == 0)
			break;
	}
fail:
	return err;
}
/**
 * @brief
 * Helper function for \ref binary_search.
 * Uses a specific field in @ref pkey to compare with the same filed in @ref pbase.
 * @param[in] pkey pointer to the object that needs to be compared.
 * @param[in] pbase pointer to the starting element of the array.
 * @retval
 * - -1 when @ref pkey is less than starting element of array pointed to by @ref pbase.
 * - 1 when @ref pkey is greater than starting element of array pointed to by @ref pbase.
 * - 0 when @ref pkey is equal to starting element of array pointed to by @ref pbase.
 */
static int
compare_hash_value(const void *pkey,
		   const void *pbase)
{
	int ret;

	if ((((const struct vpu_hash_vector_s *)pkey)->crc32_hash) <
		(((const struct vpu_hash_vector_s *)pbase)->crc32_hash))
		ret = -1;
	else if ((((const struct vpu_hash_vector_s *)pkey)->crc32_hash) >
		 (((const struct vpu_hash_vector_s *)pbase)->crc32_hash))
		ret = 1;
	else
		ret = 0;

	return ret;
}

/**
 * @brief
 * calculates crc32.
 * @param[in] crc initial crc value. usually 0.
 * @param[in] buf pointer to the buffer whose crc32 to be calculated.
 * @param[in] len length (in bytes) of data at @ref buf.
 * @retval value of calculated crc32.
 */
static uint32_t
pva_crc32(uint32_t crc,
	  unsigned char *buf,
	  size_t len)
{
	int k;

	crc = ~crc;
	while (len != 0U) {
		crc ^= *buf++;
		for (k = 0; k < 8; k++)
			crc = ((crc & 1U) == 1U) ?
			(crc >> 1U) ^ 0xedb88320U : crc >> 1U;

		len--;
	}

	return ~crc;
}

const void
*binary_search(const void *key,
	       const void *base,
	       size_t num_elems,
	       size_t size,
	       int (*compare)(const void *pkey, const void *pbase))
{
	size_t low = 0U;
	size_t high;

	if (num_elems == 0U || size == 0U)
		return NULL;

	high = num_elems - 1U;
	for (;;) {
		const void *mid_elem;
		int r;
		size_t mid = low + ((high - low) / 2U);

		/* coverity CERT INT30-C Unsigned integer */
		/* operation mid * size may wrap. */
		if (mid > UINT_MAX/size)
			return NULL;

		mid_elem = ((const unsigned char *) base) +
					 mid * size;
		r = compare(key, mid_elem);

		if (r < 0) {
			if (mid == 0U)
				return NULL;

			high = mid - 1U;
		} else if (r > 0) {
			low = mid + 1U;
			if (low < mid || low > high)
				return NULL;
		} else {
			return mid_elem;
		}
	}
}

int
pva_vpu_check_sha256_key(struct pva *pva,
			 struct vpu_hash_key_pair_s *vpu_hash_keys,
			 uint8_t *dataptr,
			 size_t size)
{
	int err = 0;
	struct vpu_hash_vector_s cal_Hash;
	const struct vpu_hash_vector_s *match_Hash;

	cal_Hash.crc32_hash = pva_crc32(0L, dataptr, size);

	match_Hash = (const struct vpu_hash_vector_s *)
		binary_search(&cal_Hash,
			      vpu_hash_keys->pvpu_hash_vector,
			      vpu_hash_keys->num_hashes,
			      sizeof(struct vpu_hash_vector_s),
			      compare_hash_value);
	if (match_Hash == NULL) {
		nvpva_dbg_info(pva, "ERROR: No Hash Match Found");
		err = -EINVAL;
		goto fail;
	}

	err = check_all_keys_for_match(vpu_hash_keys->psha_key,
				       dataptr,
				       size,
				       match_Hash);
	if (err != 0)
		nvpva_dbg_info(pva, "Error: Match key not found");
fail:
	return err;
}
