/*
 * Copyright (c) 2016-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_FIRMWARE_H
#define NVGPU_FIRMWARE_H

#include <nvgpu/types.h>

/**
 * @file
 *
 * Firmare load/unload interfaces.
 */

struct gk20a;

/** Do not display warning on failed load. */
#define NVGPU_REQUEST_FIRMWARE_NO_WARN		BIT32(0)
/** Do not attempt loading from path <SOC_NAME>. */
#define NVGPU_REQUEST_FIRMWARE_NO_SOC		BIT32(1)

/** Structure to store firmware blob. */
struct nvgpu_firmware {
	/** Points to the firmware data. */
	u8 *data;
	/** Firmware data size. */
	size_t size;
};

/**
 * @brief load a firmware blob from filesystem.
 *
 * @param g [in]	The GPU driver struct for device to load firmware for
 * @param fw_name [in]	The base name of the firmware file.
 * @param flags [in]	Flags for loading;
 * 			(These flags are inconsequential in case of rmos.)
 *
 * - nvgpu_request_firmware() will load firmware from:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *   <system firmware load path>/<GPU name>/<fw_name>
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * - It'll allocate a nvgpu_firmware structure and initializes it and returns
 *   it to caller.
 * - nvgpu_firmware will have pointer to fw data which will be filled with the
 *   data of ucode. Size of this ucode will be saved to nvgpu_firmware->size.
 *
 * @return Pointer to allocated nvgpu_firmware structure in case of success or
 * else NULL.
 */
struct nvgpu_firmware *nvgpu_request_firmware(struct gk20a *g,
					      const char *fw_name,
					      u32 flags);

/**
 * @brief Free the allocated memory space for firmware and associated
 * nvgpu_firmware blob.
 *
 * @param g [in]	The GPU driver struct for device to free firmware for
 * @param fw [in]	The firmware to free. fw blob will also be freed.
 */
void nvgpu_release_firmware(struct gk20a *g, struct nvgpu_firmware *fw);

#endif /* NVGPU_FIRMWARE_H */
