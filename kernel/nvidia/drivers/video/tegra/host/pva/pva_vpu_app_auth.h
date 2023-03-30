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

#ifndef NVPVA_VPU_HASH_H
#define NVPVA_VPU_HASH_H

#include "pva_vpu_exe.h"

/**
 * Size of sha256 keys in bytes.
 */
#define NVPVA_SHA256_DIGEST_SIZE 32U
/**
 * Maximum length of allowlist file path
 */
#define ALLOWLIST_FILE_LEN 128U
/**
 * Default path (including filename) of pva vpu elf authentication allowlist file
 */
#define PVA_AUTH_ALLOW_LIST_DEFAULT "pva_auth_allowlist"
/**
 * Array of all VPU Hash'es
 */
struct vpu_hash_vector_s {
	/*! Number of Keys for this crc32_hash */
	uint32_t count;
	/*! Starting Index into Keys Array */
	uint32_t index;
	/*! CRC32 hash value */
	uint32_t crc32_hash;
};

/**
 * Stores sha256 key
 */
struct shakey_s {
	/** 256-bit (32 Bytes) SHA Key */
	uint8_t sha_key[NVPVA_SHA256_DIGEST_SIZE];
};

/**
 * Stores Hash Vector and Keys vector
 */
struct vpu_hash_key_pair_s {
	/*! Total number of Keys in binary file */
	uint32_t num_keys;
	/*! pointer to SHA keys Array. */
	struct shakey_s *psha_key;
	/*! Total number of Hashes in binary file */
	uint32_t num_hashes;
	/*! pointer to Array of Hash'es */
	struct vpu_hash_vector_s *pvpu_hash_vector;
};

/**
 * Stores all the information related to pva vpu elf authentication.
 */
struct pva_vpu_auth_s {
	/** Stores crc32-sha256 of ELFs */
	struct vpu_hash_key_pair_s *vpu_hash_keys;
	struct mutex allow_list_lock;
	/** Flag to check if allowlist is enabled */
	bool pva_auth_enable;
	/** Flag to track if the allow list is already parsed */
	bool pva_auth_allow_list_parsed;
};

struct nvpva_drv_ctx;

/**
 * \brief checks if the sha256 key of ELF has a match in allowlist.
 *
 * It first checks if the allowlist is available.
 * If its not available it returns error code.
 * If allowlist is available, then it first calculates the crc32 hash of the elf
 * and compares the calculated hash with the available hashes in allowlist.
 * If it doesn't find a match of hash in allowlist it returns error code.
 * If it finds a matched hash, then it goes ahead and calculates the sha256 key of elf
 * and compares it with the keys asscociated with the hash in the allowlist file.
 * If there is a key match then it returns successfully. Else it returs error code.
 *
 * \param[in] vpu_hash_keys  Pointer to PVA vpu elf sha256 authentication
 *            keys structure \ref struct vpu_hash_key_pair_s
 * \param[in] dataptr data pointer of ELF to be validate SHA
 * \param[in] size  32-bit unsigned int ELF size in  number of bytes
 *
 * \return  The completion status of the operation. Possible values are:
 * - 0 when there exists a match key for the elf data pointed by dataptr.
 * - -EINVAL when allowlist file doesn't exists OR
 *   when the hash of ELF has no match in allowlist file OR
 *   when the sha256 key has no match in the list of keys
 *   associated with the hash of ELF
 */
int pva_vpu_check_sha256_key(struct pva *pva,
			     struct vpu_hash_key_pair_s *vpu_hash_keys,
			     uint8_t *dataptr,
			     size_t size);


/**
 * Parse binary file containing authentication list stored in firmware dir
 * This binary file has
	32-bit num_hashes,
	32-bit num_keys,
	Array of {32-bit(4 byte) CRC32 as Hash, 32-bit index into Array of keys,
		32-bit count of keys for this hash}
	Array of 256-bit keys.
 * Allocate memory for all the fileds and Store them.
 * Parse Hash Array and Store in memory
 * Parse Keys Array and Store in memory.
 *
 * \param[in] pva_auth  Pointer to PVA vpu elf authentication data struct \ref pva_vpu_auth
 * \return
 *  - 0, if everything is successful.
 *  - -ENOENT, if allowlist file is not found at /proc/boot/
 *  - negative of error code from fstat() if fstat fails.
 *  - -ERANGE, if file size is less than 0 or greater than  NVPVA_VPU_ELF_MAX_SZ.
 *  - -ENOMEM, if any memory allocation fails.
 *  - negative of error code return from read()
 *  - -EINVAL, if read() doesn't read expected number of bytes from the file.
 */
int
pva_auth_allow_list_parse(struct platform_device *pdev,
			 struct pva_vpu_auth_s *pva_auth);

/**
 * Parse allow list stored in memory
 * This binary file has
	32-bit num_hashes,
	32-bit num_keys,
	Array of {32-bit(4 byte) CRC32 as Hash, 32-bit index into Array of keys,
		32-bit count of keys for this hash}
	Array of 256-bit keys.
 * Allocate memory for all the fileds and Store them.
 * Parse Hash Array and Store in memory
 * Parse Keys Array and Store in memory.
 *
 * \param[in] pva_auth  Pointer to PVA vpu elf authentication data struct \ref pva_vpu_auth
 * \return
 *  - 0, if everything is successful.
 *  - -ENOENT, if allowlist file is not found at /proc/boot/
 *  - negative of error code from fstat() if fstat fails.
 *  - -ERANGE, if file size is less than 0 or greater than  NVPVA_VPU_ELF_MAX_SZ.
 *  - -ENOMEM, if any memory allocation fails.
 *  - negative of error code return from read()
 *  - -EINVAL, if read() doesn't read expected number of bytes from the file.
 */
int pva_auth_allow_list_parse_buf(struct platform_device *pdev,
				  struct pva_vpu_auth_s *pva_auth,
				  u8 *buffer,
				  u32 length);

/**
 * @brief Frees all the memory utilized for storing elf authentication data.
 * @param[in] pva_auth  Pointer to PVA vpu elf authentication data struct \ref pva_vpu_auth
 */
void pva_auth_allow_list_destroy(struct pva_vpu_auth_s *pva_auth);

/**
 * The binary_search() function performs a binary search
 * on the sorted array of num elements pointed to by base,
 * for an item that matches the object pointed to by key.
 *
 * \param[in] key The object to search for.
 * \param[in] base A pointer to the first element in the array
 * \param[in] num_elems The number of elements in the array.
 * \param[in] size The size of an element, in bytes.
 * \param[in] compare A pointer to a user-supplied function
 *	that lfind() calls to compare an array element with the key.
 * \param[in] pkey the same pointer as key
 * \param[in] pbase a pointer to an element in the array.
 *
 * \return  A pointer to a matching member of the array,
 *  or NULL if a matching object couldn't be found
 */
const void *binary_search(const void *key,
			  const void *base,
			  size_t num_elems,
			  size_t size,
			  int (*compare)(const void *pkey,
			  const void *pbase));
#endif
