/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
 */

#ifndef INCLUDE_CAMRTC_DIAG_H
#define INCLUDE_CAMRTC_DIAG_H

#include <camrtc-common.h>

#pragma GCC diagnostic error "-Wpadded"

#define CAMRTC_DIAG_IVC_ALIGNOF	MK_ALIGN(8)
#define CAMRTC_DIAG_DMA_ALIGNOF MK_ALIGN(64)

#define CAMRTC_DIAG_IVC_ALIGN	CAMRTC_ALIGN(CAMRTC_DIAG_IVC_ALIGNOF)
#define CAMRTC_DIAG_DMA_ALIGN	CAMRTC_ALIGN(CAMRTC_DIAG_DMA_ALIGNOF)

/**
 * Camera SDL - ISP5 SDL test vectors binary
 */

#define ISP5_SDL_PARAM_UNSPECIFIED	MK_U32(0xFFFFFFFF)

/**
 * @brief isp5_sdl_header - ISP5 SDL binary header
 */
struct isp5_sdl_header {
	/** Monotonically-increasing version number. */
	uint32_t version;
	/** No. of test descriptors (vectors). */
	uint32_t num_vectors;
	/** CRC32 (unsigned) on binary payload (after header). */
	uint32_t payload_crc32;
	/** Byte offset to the payload region (also the header size). */
	uint32_t payload_offset;
	/** Byte offset to input images region from payload_offset. */
	uint32_t input_base_offset;
	/** Byte offset to push_buffer2 allocation from payload_offset. */
	uint32_t push_buffer2_offset;
	/** Byte offset to MW[0/1/2] output buffers scratch surface from payload_offset.*/
	uint32_t output_buffers_offset;
	/** Reserved. */
	uint32_t reserved__[9];
} CAMRTC_DIAG_DMA_ALIGN;

/**
 * @brief isp5_sdl_test_descriptor - ISP5 SDL binary test descriptor
 */
struct isp5_sdl_test_descriptor {
	/** Zero-index test number (0...num_vectors-1). */
	uint32_t test_index;
	/** Input image width [px] (same for all inputs). */
	uint16_t input_width;
	/** Input image height [px] (same for all inputs). */
	uint16_t input_height;
	/** Offset [byte] to the nth input image of the test vector from input_base_offset. */
	uint32_t input_offset[3];
	/** Golden CRC32 values for MW0, MW1 and MW2 output. */
	uint32_t output_crc32[3];
	/** Reserved. */
	uint32_t reserved__[7];
	/** ISP5 push buffer 1 size in dwords. */
	uint32_t push_buffer1_size;
	/** ISP5 frame push buffer 1 */
	uint32_t push_buffer1[4096] CAMRTC_DIAG_DMA_ALIGN;
	/** ISP5 frame config buffer structure (isp5_configbuffer). */
	uint8_t config_buffer[128] CAMRTC_DIAG_DMA_ALIGN;
} CAMRTC_DIAG_DMA_ALIGN;

#pragma GCC diagnostic ignored "-Wpadded"

#endif /* INCLUDE_CAMRTC_DIAG_H */
