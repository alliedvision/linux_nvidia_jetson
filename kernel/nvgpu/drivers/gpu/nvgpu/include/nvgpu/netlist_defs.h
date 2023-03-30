/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_NETLIST_DEFS_H
#define NVGPU_NETLIST_DEFS_H

#include <nvgpu/types.h>

/* emulation netlists, match majorV with HW */
#define NVGPU_NETLIST_IMAGE_A	"NETA_img.bin"
#define NVGPU_NETLIST_IMAGE_B	"NETB_img.bin"
#define NVGPU_NETLIST_IMAGE_C	"NETC_img.bin"
#define NVGPU_NETLIST_IMAGE_D	"NETD_img.bin"

/* index for emulation netlists */
#define NETLIST_FINAL		-1
#define NETLIST_SLOT_A		0
#define NETLIST_SLOT_B		1
#define NETLIST_SLOT_C		2
#define NETLIST_SLOT_D		3
#define MAX_NETLIST		4

#endif /* NVGPU_NETLIST_DEFS_H */
