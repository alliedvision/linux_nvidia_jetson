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

#ifndef NVGPU_CLASS_H
#define NVGPU_CLASS_H
/**
 * @defgroup NVGPU_CLASS_VALID_NUM
 *
 * List of valid class numbers allowed in FuSa code.
 */

/**
 * Class number for DMA copy class methods on Kepler chips.
 */
#define	KEPLER_DMA_COPY_A		0xA0B5U
/**
 * Class number for Inline to memory class methods on Kepler and chips beyond.
 * The source data is always from the pushbuffer, hence the name
 * inline_to_memory.
 */
#define	KEPLER_INLINE_TO_MEMORY_B	0xA140U

/**
 * Class number for Channel_GPFIFO class methods on Maxwell chips.
 */
#define	MAXWELL_CHANNEL_GPFIFO_A	0xB06FU
/**
 * Class number for DMA copy class methods on Maxwell chips.
 */
#define	MAXWELL_DMA_COPY_A		0xB0B5U

/**
 * Class number for Channel_GPFIFO class methods on Pascal chips.
 */
#define	PASCAL_CHANNEL_GPFIFO_A		0xC06FU
/**
 * Class number for DMA copy class methods on Pascal chips.
 */
#define	PASCAL_DMA_COPY_A		0xC0B5U

/**
 * @ingroup NVGPU_CLASS_VALID_NUM
 *
 * Class number for Channel_GPFIFO class methods on Volta chips.
 */
#define	VOLTA_CHANNEL_GPFIFO_A		0xC36FU
/**
 * @ingroup NVGPU_CLASS_VALID_NUM
 *
 * Class number for compute class methods on Volta chips.
 */
#define	VOLTA_COMPUTE_A			0xC3C0U
/**
 * @ingroup NVGPU_CLASS_VALID_NUM
 *
 * Class number for DMA copy class methods on Volta chips.
 */
#define	VOLTA_DMA_COPY_A		0xC3B5U

#ifdef CONFIG_NVGPU_GRAPHICS
#define	FERMI_TWOD_A			0x902DU
#define	MAXWELL_B			0xB197U
#define	PASCAL_A			0xC097U
#define	VOLTA_A				0xC397U
#define	TURING_A			0xC597U
#endif

/* FIXME: below defines are used in dGPU safety build. */
#define	MAXWELL_COMPUTE_B		0xB1C0U
#define	PASCAL_COMPUTE_A		0xC0C0U
#define	TURING_CHANNEL_GPFIFO_A		0xC46FU
#define	TURING_COMPUTE_A		0xC5C0U
#define	TURING_DMA_COPY_A		0xC5B5U

#define AMPERE_SMC_PARTITION_REF	0xC637U
#define	AMPERE_B			0xC797U
#define	AMPERE_A			0xC697U
#define AMPERE_DMA_COPY_A		0xC6B5U
#define AMPERE_DMA_COPY_B		0xC7B5U
#define AMPERE_COMPUTE_A		0xC6C0U
#define	AMPERE_COMPUTE_B		0xC7C0U
#define AMPERE_CHANNEL_GPFIFO_A		0xC56FU
#define AMPERE_CHANNEL_GPFIFO_B		0xC76FU

#endif /* NVGPU_CLASS_H */
