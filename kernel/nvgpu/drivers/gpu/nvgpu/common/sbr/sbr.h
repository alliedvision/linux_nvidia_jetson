/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_COMMON_SBR_H_
#define NVGPU_COMMON_SBR_H_

#define PUB_PROD_BIN		"pub.bin"
#define PUB_DBG_BIN		"pub_dbg.bin"
#define PUB_TIMEOUT		100U /* msec */

struct pub_bin_hdr {
	u32 bin_magic;
	u32 bin_ver;
	u32 bin_size;
	u32 header_offset;
	u32 data_offset;
	u32 data_size;
};

struct pub_fw_header {
	u32 sig_dbg_offset;
	u32 sig_dbg_size;
	u32 sig_prod_offset;
	u32 sig_prod_size;
	u32 patch_loc;
	u32 patch_sig;
	u32 hdr_offset;
	u32 hdr_size;
};

#endif /* NVGPU_COMMON_SBR_H_ */
