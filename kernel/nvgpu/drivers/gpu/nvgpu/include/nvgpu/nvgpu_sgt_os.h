/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_SGT_OS_H
#define NVGPU_SGT_OS_H

/**
 * @file
 *
 * Abstract interface for exposing the OS interface for creating an nvgpu_sgt
 * from an nvgpu_mem. The reason this is handled by the OS is because the
 * nvgpu_mem - especially the part that defines the underlying SGT - is
 * intrinsically tied to the OS.
 */

struct gk20a;
struct nvgpu_sgt;
struct nvgpu_mem;

/**
 * nvgpu_sgt_os_create_from_mem - Create an nvgpu_sgt from an nvgpu_mem.
 *
 * @param g   [in]	The GPU.
 * @param mem [in]	Pointer to nvgpu_mem object.
 *
 * Since a DMA allocation may well be discontiguous nvgpu requires that a
 * table describe the chunks of memory that make up the DMA allocation. This
 * scatter gather table, SGT, must be created from an nvgpu_mem. Since the
 * representation of a DMA allocation varies wildly from OS to OS the OS is
 * tasked with creating an implementation of the nvgpu_sgt op interface.
 *
 * This function should be defined by each OS. It should create an nvgpu_sgt
 * object from the passed nvgpu_mem. The nvgpu_mem and the nvgpu_sgt together
 * abstract the DMA allocation in such a way that the GMMU can map any buffer
 * from any OS.
 *
 * The nvgpu_sgt object returned by this function must be freed by the
 * nvgpu_sgt_free() function.
 *
 * @return nvgpu_sgt object on success.
 * @return NULL when there's an error.
 * Possible failure case:
 * - Insufficient system memory.
 */
struct nvgpu_sgt *nvgpu_sgt_os_create_from_mem(struct gk20a *g,
					       struct nvgpu_mem *mem);

#endif /* NVGPU_SGT_OS_H */
