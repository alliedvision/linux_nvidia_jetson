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

#ifndef NVGPU_GR_FALCON_PRIV_H
#define NVGPU_GR_FALCON_PRIV_H

#include <nvgpu/types.h>
#include <nvgpu/nvgpu_mem.h>

struct nvgpu_ctxsw_ucode_segments;

/** GPCCS boot signature for T18X chip, type: with reserved. */
#define FALCON_UCODE_SIG_T18X_GPCCS_WITH_RESERVED	0x68edab34U

/** FECS boot signature for T21X chip, type: with DMEM size. */
#define FALCON_UCODE_SIG_T21X_FECS_WITH_DMEM_SIZE	0x9121ab5cU
/** FECS boot signature for T21X chip, type: with reserved. */
#define FALCON_UCODE_SIG_T21X_FECS_WITH_RESERVED	0x9125ab5cU
/** FECS boot signature for T21X chip, type: without reserved. */
#define FALCON_UCODE_SIG_T21X_FECS_WITHOUT_RESERVED	0x93671b7dU
/** FECS boot signature for T21X chip, type: without reserved2. */
#define FALCON_UCODE_SIG_T21X_FECS_WITHOUT_RESERVED2	0x4d6cbc10U
/** GPCCS boot signature for T21X chip, type: with reserved. */
#define FALCON_UCODE_SIG_T21X_GPCCS_WITH_RESERVED	0x3d3d65e2U
/** GPCCS boot signature for T21X chip, type: without reserved. */
#define FALCON_UCODE_SIG_T21X_GPCCS_WITHOUT_RESERVED	0x393161daU

/** FECS boot signature for T12X chip, type: with reserved. */
#define FALCON_UCODE_SIG_T12X_FECS_WITH_RESERVED	0x8a621f78U
/** FECS boot signature for T12X chip, type: without reserved. */
#define FALCON_UCODE_SIG_T12X_FECS_WITHOUT_RESERVED	0x67e5344bU
/** FECS boot signature for T12X chip, type: older. */
#define FALCON_UCODE_SIG_T12X_FECS_OLDER		0x56da09fU

/** GPCCS boot signature for T12X chip, type: with reserved. */
#define FALCON_UCODE_SIG_T12X_GPCCS_WITH_RESERVED	0x303465d5U
/** GPCCS boot signature for T12X chip, type: without reserved. */
#define FALCON_UCODE_SIG_T12X_GPCCS_WITHOUT_RESERVED	0x3fdd33d3U
/** GPCCS boot signature for T12X chip, type: older. */
#define FALCON_UCODE_SIG_T12X_GPCCS_OLDER		0x53d7877U

enum wait_ucode_status {
	/** Status of ucode wait operation : LOOP. */
	WAIT_UCODE_LOOP,
	/** Status of ucode wait operation : timedout. */
	WAIT_UCODE_TIMEOUT,
	/** Status of ucode wait operation : error. */
	WAIT_UCODE_ERROR,
	/** Status of ucode wait operation : success. */
	WAIT_UCODE_OK
};

/** Falcon operation condition : EQUAL. */
#define	GR_IS_UCODE_OP_EQUAL			0U
/** Falcon operation condition : NOT_EQUAL. */
#define	GR_IS_UCODE_OP_NOT_EQUAL		1U
/** Falcon operation condition : AND. */
#define	GR_IS_UCODE_OP_AND			2U
/** Falcon operation condition : LESSER. */
#define	GR_IS_UCODE_OP_LESSER			3U
/** Falcon operation condition : LESSER_EQUAL. */
#define	GR_IS_UCODE_OP_LESSER_EQUAL		4U
/** Falcon operation condition : SKIP. */
#define	GR_IS_UCODE_OP_SKIP			5U

/** Mailbox value in case of successful operation. */
#define FALCON_UCODE_HANDSHAKE_INIT_COMPLETE	1U

struct fecs_mthd_op_method {
	/** Method address to send to FECS microcontroller. */
	u32 addr;
	/** Method data to send to FECS microcontroller. */
	u32 data;
};

struct fecs_mthd_op_mailbox {
	/** Mailbox ID to perform operation. */
	u32 id;
	/** Mailbox data to be written. */
	u32 data;
	/** Mailbox clear value. */
	u32 clr;
	/** Last read mailbox value. */
	u32 *ret;
	/** Mailbox value in case of operation success. */
	u32 ok;
	/** Mailbox value in case of operation failure. */
	u32 fail;
};

struct fecs_mthd_op_cond {
	/** Operation success condition. */
	u32 ok;
	/** Operation fail condition. */
	u32 fail;
};

/**
 * FECS method operation structure.
 *
 * This structure defines the protocol for communication with FECS
 * microcontroller.
 */
struct nvgpu_fecs_method_op {
	/** Method struct */
	struct fecs_mthd_op_method method;
	/** Mailbox struct */
	struct fecs_mthd_op_mailbox mailbox;
	/** Condition struct */
	struct fecs_mthd_op_cond cond;
};

/**
 * CTXSW falcon bootloader descriptor structure.
 */
struct nvgpu_ctxsw_bootloader_desc {
	/** Start offset, unused. */
	u32 start_offset;
	/** Size, unused. */
	u32 size;
	/** IMEM offset. */
	u32 imem_offset;
	/** Falcon boot vector. */
	u32 entry_point;
};

/**
 * CTXSW ucode information structure.
 */
struct nvgpu_ctxsw_ucode_info {
	/** Memory to store ucode instance block. */
	struct nvgpu_mem inst_blk_desc;
	/** Memory to store ucode contents locally. */
	struct nvgpu_mem surface_desc;
	/** Ucode segments for FECS. */
	struct nvgpu_ctxsw_ucode_segments fecs;
	/** Ucode segments for GPCCS. */
	struct nvgpu_ctxsw_ucode_segments gpccs;
};

/**
 * Structure to store various sizes queried from FECS
 */
struct nvgpu_gr_falcon_query_sizes {
	/** Size of golden context image. */
	u32 golden_image_size;

#ifdef CONFIG_NVGPU_DEBUGGER
	u32 pm_ctxsw_image_size;
#endif
#ifdef CONFIG_NVGPU_GFXP
	u32 preempt_image_size;
#endif
#ifdef CONFIG_NVGPU_GRAPHICS
	u32 zcull_image_size;
#endif
};

/**
 * GR falcon data structure.
 *
 * This structure stores all data required to load and boot CTXSW ucode,
 * and also to communicate with FECS microcontroller.
 */
struct nvgpu_gr_falcon {
	/**
	 * CTXSW ucode information structure.
	 */
	struct nvgpu_ctxsw_ucode_info ctxsw_ucode_info;

	/**
	 * Mutex to protect all FECS methods.
	 */
	struct nvgpu_mutex fecs_mutex;

	/**
	 * Flag to skip ucode initialization if it is already done.
	 */
	bool skip_ucode_init;

	/**
	 * Flag to trigger recovery bootstrap in case coldboot bootstrap
	 * was already done.
	 */
	bool coldboot_bootstrap_done;

	/**
	 * Structure to hold various sizes that are queried from FECS
	 * microcontroller.
	 */
	struct nvgpu_gr_falcon_query_sizes sizes;
};

#endif /* NVGPU_GR_FALCON_PRIV_H */
