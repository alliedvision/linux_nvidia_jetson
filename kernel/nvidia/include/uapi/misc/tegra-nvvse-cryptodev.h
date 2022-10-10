/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION. All Rights Reserved.
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

#ifndef __UAPI_TEGRA_NVVSE_CRYPTODEV_H
#define __UAPI_TEGRA_NVVSE_CRYPTODEV_H

#include <asm-generic/ioctl.h>

#define TEGRA_NVVSE_IOC_MAGIC				0x98

/* Command ID for various IO Control */
#define TEGRA_NVVSE_CMDID_AES_SET_KEY			1
#define TEGRA_NVVSE_CMDID_AES_ENCDEC			2
#define TEGRA_NVVSE_CMDID_AES_CMAC			3
#define TEGRA_NVVSE_CMDID_INIT_SHA			5
#define TEGRA_NVVSE_CMDID_UPDATE_SHA			6
#define TEGRA_NVVSE_CMDID_FINAL_SHA			7
#define TEGRA_NVVSE_CMDID_AES_DRNG			8
#define TEGRA_NVVSE_CMDID_AES_GMAC_INIT			9
#define TEGRA_NVVSE_CMDID_AES_GMAC_SIGN_VERIFY		10
#define TEGRA_NVVSE_CMDID_AES_CMAC_SIGN_VERIFY		11

/** Defines the length of the AES-CBC Initial Vector */
#define TEGRA_NVVSE_AES_IV_LEN				16U
/** Defines the length of the AES-CTR Initial counter*/
#define TEGRA_NVVSE_AES_CTR_LEN				16U
/** Defines the length of the AES-GCM Initial Vector */
#define TEGRA_NVVSE_AES_GCM_IV_LEN			12U
/** Defines the length of the AES-GCM Tag buffer */
#define TEGRA_NVVSE_AES_GCM_TAG_SIZE			16U
/** Defines the length of the AES-CMAC */
#define TEGRA_NVVSE_AES_CMAC_LEN			16U
/** Defines the counter offset byte in the AES Initial counter*/
#define TEGRA_COUNTER_OFFSET				12U

/**
  * @brief Defines SHA Types.
  */
enum tegra_nvvse_sha_type {
	/** Defines SHA-256 Type */
	TEGRA_NVVSE_SHA_TYPE_SHA256 = 0u,
	/** Defines SHA-384 Type */
	TEGRA_NVVSE_SHA_TYPE_SHA384,
	/** Defines SHA-512 Type */
	TEGRA_NVVSE_SHA_TYPE_SHA512,
	/** Defines SHA3-256 Type */
	TEGRA_NVVSE_SHA_TYPE_SHA3_256,
	/** Defines SHA3-384 Type */
	TEGRA_NVVSE_SHA_TYPE_SHA3_384,
	/** Defines SHA3-512 Type */
	TEGRA_NVVSE_SHA_TYPE_SHA3_512,
	/** Defines SHAKE-128 Type */
	TEGRA_NVVSE_SHA_TYPE_SHAKE128,
	/** Defines SHAKE256 Type */
	TEGRA_NVVSE_SHA_TYPE_SHAKE256,
	/** Defines maximum SHA Type, must be last entry */
	TEGRA_NVVSE_SHA_TYPE_MAX,
};

/**
  * \brief Defines AES modes.
  */
enum tegra_nvvse_aes_mode {
	/** Defines AES MODE CBC */
	TEGRA_NVVSE_AES_MODE_CBC = 0u,
	/** Defines AES MODE ECB */
	TEGRA_NVVSE_AES_MODE_ECB,
	/** Defines AES MODE CTR */
	TEGRA_NVVSE_AES_MODE_CTR,
	/** Defines AES MODE GCM */
	TEGRA_NVVSE_AES_MODE_GCM,
	/** Defines maximum AES MODE, must be last entry*/
	TEGRA_NVVSE_AES_MODE_MAX,
};

/**
 * \brief Defines AES GMAC type.
 */
enum tegra_nvvse_gmac_type {
	/** Defines AES GMAC Sign */
	TEGRA_NVVSE_AES_GMAC_SIGN = 0u,
	/** Defines AES GMAC Verify */
	TEGRA_NVVSE_AES_GMAC_VERIFY,
};

/**
 * \brief Defines AES CMAC type.
 */
enum tegra_nvvse_cmac_type {
	/** Defines AES CMAC Sign */
	TEGRA_NVVSE_AES_CMAC_SIGN = 0u,
	/** Defines AES CMAC Verify */
	TEGRA_NVVSE_AES_CMAC_VERIFY,
};

/**
  * \brief Holds SHA Init Header Params
  */
struct tegra_nvvse_sha_init_ctl {
	enum tegra_nvvse_sha_type	sha_type;
	uint32_t			digest_size;
	uint64_t			total_msg_size;
};
#define NVVSE_IOCTL_CMDID_INIT_SHA _IOW(TEGRA_NVVSE_IOC_MAGIC, TEGRA_NVVSE_CMDID_INIT_SHA, \
						struct tegra_nvvse_sha_init_ctl)

/**
  * \brief Holds SHA Update Header Params
  */
struct tegra_nvvse_sha_update_ctl {
	/** Holds the pointer of the input buffer */
	char		*in_buff;
	/** Holds the size of the input buffer */
	uint32_t	input_buffer_size;
	/** Indicates the last chunk of the input message. 1 means last buffer
	  * else not the last buffer
	  */
	uint8_t		last_buffer;
};
#define NVVSE_IOCTL_CMDID_UPDATE_SHA _IOW(TEGRA_NVVSE_IOC_MAGIC, TEGRA_NVVSE_CMDID_UPDATE_SHA, \
						struct tegra_nvvse_sha_update_ctl)

/**
  * \brief Holds SHA Final Header Params
  */
struct tegra_nvvse_sha_final_ctl {
	/** Holds the pointer of the digest buffer */
	uint8_t		*digest_buffer;
	/** Holds the size of the digest buffer */
	uint32_t	digest_size;
};
#define NVVSE_IOCTL_CMDID_FINAL_SHA _IOWR(TEGRA_NVVSE_IOC_MAGIC, TEGRA_NVVSE_CMDID_FINAL_SHA, \
						struct tegra_nvvse_sha_final_ctl)

/**
  *  \brief Holds AES encrypt/decrypt parameters for IO Control.
  */
struct  tegra_nvvse_aes_enc_dec_ctl {
	/** [in] Holds a Boolean that specifies whether to encrypt the buffer. */
	/** value '0' indicates Decryption and non zero value indicates Encryption */
	uint8_t		is_encryption;
	/** [in] Holds a Boolean that specifies whether this is first
	 * NvVseAESEncryptDecrypt() call for encrypting a given message .
	 * '0' value indicates First call and Non zero value indicates it is not the first call */
	uint8_t		is_non_first_call;
	/** [in] Holds a keyslot number */
	uint32_t	key_slot;
	/** [in] Holds the Key length */
	/** Supported keylengths are 16 and 32 bytes */
	uint8_t		key_length;
	/** [in] Holds whether key configuration is required or not, 0 means do key configuration */
	uint8_t		skip_key;
	/** [in] Holds an AES Mode */
	enum		tegra_nvvse_aes_mode aes_mode;
	/** [inout] Initial Vector (IV) used for AES Encryption and Decryption.
	  * During Encryption, the nvvse generates IV and populates in oIV in the
	  * first NvVseAESEncryptDecrypt() call.
	  * During Decryption, the application shall populate oIV
	  * with IV used for Encryption
	  */
	uint8_t		initial_vector[TEGRA_NVVSE_AES_IV_LEN];
	/** [inout] Initial Counter (CTR) used for AES Encryption and Decryption.
	  * During Encryption, the nvvse generates nonce(96 bit) + counter (32 bit)
	  * and populates intial counter(128 bit) in counter.Initial 128-bit counter
	  * value would be returned in the first call to NvVseAESEncryptDecrypt().
	  * During Decryption, the application shall populate initial Counter
	  * (128 bit) used for Encryption.
	  * Counter value for each block is fixed and always incremented by 1
	  * for successive blocks Encryption and Decryption operation.
	  */
	uint8_t		initial_counter[TEGRA_NVVSE_AES_CTR_LEN];

	/** [in] Holds the Length of the input buffer.
	  * uDataLength shall be multiple of AES block size i.e 16 bytes.
	  * uDataLength shall not be more than the size configured through "-aes_ip_max"
	  * option during launch of driver (devc-nvvse-safety). The max value that can be
	  * configured through "-aes_ip_max" is 1GB.
	  * For Encryption: Range supported for data length is 16 to ((16 MB - 16) * 64) bytes.
	  * For Decryption: Range supported for data length is "16" to the size configured through
	  *  "-aes_ip_max" option. If the size is greater than "-aes_ip_max", then the buffer can be split
	  * into multiple chunks and API NvVseAESEncryptDecrypt() can be called multiple times.
	  * For AES CBC, it is required to set the last block of encrypted data
	  * of a chunk as the IV for decrypting next chunk.
	  */
	uint32_t	data_length;
	/** [in] Holds a pointer to input buffer to be encrypted/decrypted. */
	uint8_t		*src_buffer;
	/** [out] Holds a pointer to the encrypted/decrypted buffer. */
	uint8_t		*dest_buffer;

	/** [in] Holds the length of aad.
	 *  Range supported for data length is 0 to 16 MB - 1 bytes.
	 */
	uint32_t aad_length;
	/** [in] Holds a pointer to aad buffer to be used for AEAD additional data.
	 *  aad is optional, so when aad_length is 0 this pointer can be NULL.
	 */
	uint8_t *aad_buffer;
	/** [in] Holds the length of tag for GMAC.
	 *  supported tag_length is 16 bytes
	 */
	uint32_t tag_length;
	/** [inout] GMAC tag buffer
	 * During Encryption, tag of size tag_length is generated by nvvse.
	 * During Decryption, tag of size tag_length should be populated by the
	 * application.
	 */
	uint8_t *tag_buffer;
};
#define NVVSE_IOCTL_CMDID_AES_ENCDEC _IOWR(TEGRA_NVVSE_IOC_MAGIC, TEGRA_NVVSE_CMDID_AES_ENCDEC, \
						struct  tegra_nvvse_aes_enc_dec_ctl)

/**
 * \brief Holds AES GMAC Init parameters
 */
struct tegra_nvvse_aes_gmac_init_ctl {
	/** [in] Holds a keyslot number */
	uint32_t   key_slot;
	/** [in] Holds the Key length */
	/** Supported keylengths are 16 and 32 bytes */
	uint8_t   key_length;
	/** [out] Initial Vector (IV) used for GMAC Sign and Verify */
	uint8_t   IV[TEGRA_NVVSE_AES_GCM_IV_LEN];
};
#define NVVSE_IOCTL_CMDID_AES_GMAC_INIT _IOW(TEGRA_NVVSE_IOC_MAGIC, \
						TEGRA_NVVSE_CMDID_AES_GMAC_INIT, \
						struct tegra_nvvse_aes_gmac_init_ctl)

/**
 * \brief Holds AES GMAC parameters
 */
struct tegra_nvvse_aes_gmac_sign_verify_ctl {
	/** [in] Holds the enum which indicates AES GMAC Sign or Verify */
	enum  tegra_nvvse_gmac_type gmac_type;
	/** [in] Holds a Boolean that specifies whether this is first
	 * chunk of message for GMAC Signing/Verifying.
	 * '0' value indicates it is not First call and
	 * Non zero value indicates it is the first call.
	 */
	uint8_t  is_first;
	/** [in] Holds a Boolean that specifies whether this is last
	 * chunk of message for GMAC Signing/Verifying.
	 * '0' value indicates it is not Last call and
	 *  Non zero value indicates it is the Last call.
	 */
	uint8_t  is_last;
	/** [in] Holds a keyslot handle which is used for GMAC operation */
	uint32_t  key_slot;
	/** [in] Holds the Key length
	 * Supported keylength is only 16 bytes and 32 bytes
	 */
	uint8_t  key_length;
	/** [in] Holds the Length of the input source buffer.
	 * data_length shall not be "0" supported for single part sign and verify
	 * data_length shall be multiple of 16 bytes if it is not the last chunk
	 * i.e when is_last is "0"
	 */
	uint32_t data_length;
	/** [in] Holds a pointer to the input source buffer for which
	 *  AES GMAC is to be calculated/verified.
	 */
	uint8_t *src_buffer;
	/** [in] Initial Vector (IV) used for AES GMAC.
	 * For AES-GMAC iv size is 96 bits.
	 * Application will pass this IV for verification.
	 */
	uint8_t initial_vector[TEGRA_NVVSE_AES_GCM_IV_LEN];
	/** [in] Holds the length of tag for GMAC. */
	uint32_t tag_length;
	/** [inout] Holds a pointer to the AES GMAC signature.
	 * GMAC signature will updated by Virtual SE Driver when gmac_type is
	 * TEGRA_NVVSE_AES_GMAC_SIGN and when the last chunk of the message is sent i.e when
	 * is_last is non zero.
	 * GMAC signature should be provided by client when gmac_type is
	 * TEGRA_NVVSE_AES_GMAC_VERIFY and the last chunk of the message is sent i.e when is_last
	 * is non zero.
	 * The AES GMAC signature length supported is 16 bytes. Hence this buffer must be 16 bytes
	 * length.
	 */
	uint8_t *tag_buffer;
	/** [out] Holds GMAC verification result, which the driver updates.
	 * Valid only when gmac_type is TEGRA_NVVSE_AES_GMAC_VERIFY.
	 * Result values are:
	 * - '0' indicates GMAC verification success.
	 * - Non-zero value indicates GMAC verification failure.
	 */
	uint8_t  result;
};
#define NVVSE_IOCTL_CMDID_AES_GMAC_SIGN_VERIFY _IOWR(TEGRA_NVVSE_IOC_MAGIC, \
						TEGRA_NVVSE_CMDID_AES_GMAC_SIGN_VERIFY, \
						struct  tegra_nvvse_aes_gmac_sign_verify_ctl)

/**
 * \brief Holds AES CMAC parameters
 */
struct tegra_nvvse_aes_cmac_sign_verify_ctl {
	/** [in] Holds the enum which indicates AES CMAC Sign or Verify */
	enum  tegra_nvvse_cmac_type cmac_type;
	/** [in] Holds a Boolean that specifies whether this is first
	 * chunk of message for CMAC Signing/Verifying.
	 * '0' value indicates it is not First call and
	 * Non zero value indicates it is the first call.
	 */
	uint8_t  is_first;
	/** [in] Holds a Boolean that specifies whether this is last
	 * chunk of message for CMAC Signing/Verifying.
	 * '0' value indicates it is not Last call and
	 *  Non zero value indicates it is the Last call.
	 */
	uint8_t  is_last;
	/** [in] Holds a keyslot handle which is used for CMAC operation */
	uint32_t  key_slot;
	/** [in] Holds the Key length
	 * Supported keylength is only 16 bytes and 32 bytes
	 */
	uint8_t  key_length;
	/** [in] Holds the Length of the input source buffer.
	 * data_length shall not be "0" supported for single part sign and verify
	 * data_length shall be multiple of 16 bytes if it is not the last chunk
	 * i.e when is_last is "0"
	 */
	uint32_t data_length;
	/** [in] Holds a pointer to the input source buffer for which
	 *  AES CMAC is to be calculated/verified.
	 */
	uint8_t *src_buffer;
	/** [in] Holds the length of tag for CMAC. */
	uint32_t cmac_length;
	/** [inout] Holds a pointer to the AES CMAC signature.
	 * CMAC signature will updated by Virtual SE Driver when gmac_type is
	 * TEGRA_NVVSE_AES_CMAC_SIGN and when the last chunk of the message is sent i.e when
	 * is_last is non zero.
	 * CMAC signature should be provided by client when gmac_type is
	 * TEGRA_NVVSE_AES_CMAC_VERIFY and the last chunk of the message is sent i.e when is_last
	 * is non zero.
	 * The AES CMAC signature length supported is 16 bytes. Hence this buffer must be 16 bytes
	 * length.
	 */
	uint8_t *cmac_buffer;
	/** [out] Holds CMAC verification result, which the driver updates.
	 * Valid only when gmac_type is TEGRA_NVVSE_AES_CMAC_VERIFY.
	 * Result values are:
	 * - '0' indicates CMAC verification success.
	 * - Non-zero value indicates CMAC verification failure.
	 */
	uint8_t  result;
};
#define NVVSE_IOCTL_CMDID_AES_CMAC_SIGN_VERIFY _IOWR(TEGRA_NVVSE_IOC_MAGIC, \
						TEGRA_NVVSE_CMDID_AES_CMAC_SIGN_VERIFY, \
						struct  tegra_nvvse_aes_cmac_sign_verify_ctl)

/**
  * \brief Holds AES generated RNG IO control params
  */
struct tegra_nvvse_aes_drng_ctl {
	/** Holds the pointer of the destination buffer where generate random number will be copied */
	uint8_t		*dest_buff;
	/** Holds the size of the RNG buffer */
	uint32_t	data_length;
};
#define NVVSE_IOCTL_CMDID_AES_DRNG _IOWR(TEGRA_NVVSE_IOC_MAGIC, TEGRA_NVVSE_CMDID_AES_DRNG, \
						struct tegra_nvvse_aes_drng_ctl)

#endif

