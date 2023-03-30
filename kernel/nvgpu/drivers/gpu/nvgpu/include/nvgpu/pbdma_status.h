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

#ifndef NVGPU_PBDMA_STATUS_H
#define NVGPU_PBDMA_STATUS_H

#include <nvgpu/types.h>
/**
 * @file
 *
 * Abstract interface for interpreting pbdma status info read from h/w.
 */
/**
 * H/w defined value for Channel ID type in pbdma status h/w register.
 */
#define PBDMA_STATUS_ID_TYPE_CHID 0U
/**
 * H/w defined value for Tsg ID type in pbdma status h/w register.
 */
#define PBDMA_STATUS_ID_TYPE_TSGID 1U
/**
 * S/w defined value for unknown ID type.
 */
#define PBDMA_STATUS_ID_TYPE_INVALID (~U32(0U))
/**
 * H/w defined value for next Channel ID type in pbdma status h/w register.
 */
#define PBDMA_STATUS_NEXT_ID_TYPE_CHID PBDMA_STATUS_ID_TYPE_CHID
/**
 * H/w defined value for next Tsg ID type in pbdma status h/w register.
 */
#define PBDMA_STATUS_NEXT_ID_TYPE_TSGID PBDMA_STATUS_ID_TYPE_TSGID
/**
 * S/w defined value for unknown ID type.
 */
#define PBDMA_STATUS_NEXT_ID_TYPE_INVALID PBDMA_STATUS_ID_TYPE_INVALID

/**
 * S/w defined value for unknown ID.
 */
#define PBDMA_STATUS_ID_INVALID (~U32(0U))
/**
 * S/w defined value for unknown next ID.
 */
#define PBDMA_STATUS_NEXT_ID_INVALID PBDMA_STATUS_ID_INVALID

enum nvgpu_pbdma_status_chsw_status {
	/** Channel is not loaded on pbdma. Both id and next_id are invalid. */
	NVGPU_PBDMA_CHSW_STATUS_INVALID,
	/**
	 * Channel is loaded on the pbdma. id field of pbdma_status
	 * h/w register is valid but next_id is not valid. Also host
	 * is currently not channel switching this pbdma.
	 */
	NVGPU_PBDMA_CHSW_STATUS_VALID,
	/**
	 * Host is loading a new channel and the previous channel is
	 * invalid. In this state only next_id is valid.
	 */
	NVGPU_PBDMA_CHSW_STATUS_LOAD,
	/**
	 * Host is saving the current channel and not loading a new one.
	 * In this state only id is valid.
	 */
	NVGPU_PBDMA_CHSW_STATUS_SAVE,
	/**
	 * Host is switching between two valid channels. In this state both
	 * id and next_id are valid.
	 */
	NVGPU_PBDMA_CHSW_STATUS_SWITCH,
};

struct nvgpu_pbdma_status_info {
	/** Pbdma_status h/w register's read value. */
	u32 pbdma_reg_status;
	/** Chan_status field of pbdma_status h/w register. */
	u32 pbdma_channel_status;
	/** Channel or tsg id of the context currently loaded on the pbdma. */
	u32 id;
	/** Specifies whether #id is of channel or tsg type. */
	u32 id_type;
	/** Channel or tsg id of the next context to be loaded on the pbdma. */
	u32 next_id;
	/** Specifies whether #next_id is of channel or tsg type. */
	u32 next_id_type;
	/** Enum for chan_status field of pbdma_status h/w register. */
	enum nvgpu_pbdma_status_chsw_status chsw_status;
};

/**
 * @brief Check if chsw_status is set to switch.
 *
 * @param pbdma_status [in]	Pointer to struct containing pbdma_status h/w
 * 				reg/field value.
 *
 * @return Interprets #pbdma_status and returns true if channel
 *         status is set to #NVGPU_PBDMA_CHSW_STATUS_SWITCH else returns false.
 */
bool nvgpu_pbdma_status_is_chsw_switch(struct nvgpu_pbdma_status_info
		*pbdma_status);
/**
 * @brief Check if chsw_status is set to load.
 *
 * @param pbdma_status [in]	Pointer to struct containing pbdma_status h/w
 * 				reg/field value.
 *
 * @return Interprets #pbdma_status and returns true if channel
 *         status is set to #NVGPU_PBDMA_CHSW_STATUS_LOAD else returns false.
 */
bool nvgpu_pbdma_status_is_chsw_load(struct nvgpu_pbdma_status_info
		*pbdma_status);
/**
 * @brief Check if chsw_status is set to save.
 *
 * @param pbdma_status [in]	Pointer to struct containing pbdma_status h/w
 * 				reg/field value.
 *
 * @return Interprets #pbdma_status and returns true if channel
 *         status is set to #NVGPU_PBDMA_CHSW_STATUS_SAVE else returns false.
 */
bool nvgpu_pbdma_status_is_chsw_save(struct nvgpu_pbdma_status_info
		*pbdma_status);
/**
 * @brief Check if chsw_status is set to valid.
 *
 * @param pbdma_status [in]	Pointer to struct containing pbdma_status h/w
 * 				reg/field value.
 *
 * @return Interprets #pbdma_status and returns true if channel
 *         status is set to #NVGPU_PBDMA_CHSW_STATUS_VALID else returns false.
 */
bool nvgpu_pbdma_status_is_chsw_valid(struct nvgpu_pbdma_status_info
		*pbdma_status);
/**
 * @brief Check if id_type is tsg.
 *
 * @param pbdma_status [in]	Pointer to struct containing pbdma_status h/w
 * 				reg/field value.
 *
 * @return Interprets #pbdma_status and returns true if id_type
 *         is #PBDMA_STATUS_ID_TYPE_TSGID else returns false.
 */
bool nvgpu_pbdma_status_is_id_type_tsg(struct nvgpu_pbdma_status_info
		*pbdma_status);
/**
 * @brief Check if next_id_type is tsg.
 *
 * @param pbdma_status [in]	Pointer to struct containing pbdma_status h/w
 * 				reg/field value.
 *
 * @return Interprets #pbdma_status and returns true if next_id_type
 *         is #PBDMA_STATUS_NEXT_ID_TYPE_TSGID else returns false.
 */
bool nvgpu_pbdma_status_is_next_id_type_tsg(struct nvgpu_pbdma_status_info
		*pbdma_status);

#endif /* NVGPU_PBDMA_STATUS_H */
