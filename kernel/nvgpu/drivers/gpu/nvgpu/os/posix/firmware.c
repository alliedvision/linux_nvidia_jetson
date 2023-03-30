/*
 * Copyright (c) 2018-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <nvgpu/types.h>
#include <nvgpu/kmem.h>
#include <nvgpu/firmware.h>
#include <nvgpu/gk20a.h>

#define FW_MAX_PATH_SIZE 2048U

#if defined(__QNX__)
#define NVGPU_UNITTEST_UCODE_PATH "/gv11b/"
#else
#define NVGPU_UNITTEST_UCODE_PATH "/firmware/gv11b/"
#endif

static int nvgpu_ucode_load(struct gk20a *g, const char *path,
	struct nvgpu_firmware *ucode)
{
	struct stat buf;
	size_t bufsize;
	ssize_t len;
	int fd = -1;
	int ret = -1;
	u8 *data, *data1;

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		nvgpu_err(g, "fw: %s open failed", path);
		goto done;
	}

	if (fstat(fd, &buf) == -1) {
		nvgpu_err(g, "fw: fstat failed");
		goto done;
	}

	if (buf.st_size <= 0) {
		nvgpu_err(g, "fw: invalid buf.st_size");
		goto done;
	}
	bufsize = (size_t)buf.st_size;

	data = (u8 *)nvgpu_kmalloc(g, bufsize);
	if (data == NULL) {
		nvgpu_err(g, "fw: malloc failed");
		goto done;
	}
	data1 = data;

	while (bufsize > 0UL) {
		len = read(fd, data, bufsize);
		if (len < 0) {
			ret = errno;
			nvgpu_err(g, "fw: read failed");
			goto err;
		}

		if (len == 0) {
			nvgpu_err(g, "fw load failed: read size mismatch");
			ret = -1;
			goto err;
		}

		data += len;
		bufsize -= (size_t)len;
	}

	ucode->data = data1;
	ucode->size = (size_t)buf.st_size;
	ret = 0;
	goto done;

err:
	nvgpu_kfree(g, data);

done:
	if (fd >= 0) {
		if (close(fd) != 0) {
			nvgpu_err(g, "close() failed");
		}
	}

	return ret;
}

struct nvgpu_firmware *nvgpu_request_firmware(struct gk20a *g,
					      const char *fw_name,
					      u32 flags)
{
	struct nvgpu_firmware *fw;
	char full_path[FW_MAX_PATH_SIZE] = "";
	int ret;
	size_t full_path_len;

	(void)flags;

	if (fw_name == NULL) {
		return NULL;
	}

#if defined(__QNX__)
	strcpy(full_path, "/proc/boot/");
#else
	getcwd(full_path, FW_MAX_PATH_SIZE);
#endif

	full_path_len = strlen(full_path);
	full_path_len += strlen(fw_name);
	full_path_len += strlen(NVGPU_UNITTEST_UCODE_PATH);
	if (full_path_len >=  FW_MAX_PATH_SIZE) {
		nvgpu_err(g, "Invalid MAX_PATH_SIZE %lu %u", full_path_len,
			FW_MAX_PATH_SIZE);
		goto err;
	}

	strcat(full_path, NVGPU_UNITTEST_UCODE_PATH);
	strcat(full_path, fw_name);

	fw = nvgpu_kzalloc(g, sizeof(*fw));
	if (fw == NULL) {
		nvgpu_err(g, "malloc failed");
		goto err;
	}

	ret = nvgpu_ucode_load(g, full_path, fw);
	if (ret != 0) {
		nvgpu_err(g, "failed to load %s ucode", fw_name);
		goto err_fw;
	}
	return fw;

err_fw:
	nvgpu_kfree(g, fw);
err:
	return NULL;
}

void nvgpu_release_firmware(struct gk20a *g, struct nvgpu_firmware *fw)
{
	nvgpu_kfree(g, fw->data);
	nvgpu_kfree(g, fw);
}
