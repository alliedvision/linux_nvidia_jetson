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

#ifndef NVGPU_ENGINE_STATUS_H
#define NVGPU_ENGINE_STATUS_H

/**
 * @file
 *
 * Abstract interface for interpreting engine status info read from h/w.
 */
/**
 * H/w defined value for Channel ID type in engine status h/w register.
 */
#define ENGINE_STATUS_CTX_ID_TYPE_CHID 0U
/**
 * H/w defined value for Tsg ID type in engine status h/w register.
 */
#define ENGINE_STATUS_CTX_ID_TYPE_TSGID 1U
/**
 * S/w defined value for unknown ID type.
 */
#define ENGINE_STATUS_CTX_ID_TYPE_INVALID (~U32(0U))

/**
 * H/w defined value for next Channel ID type in engine status h/w register.
 */
#define ENGINE_STATUS_CTX_NEXT_ID_TYPE_CHID ENGINE_STATUS_CTX_ID_TYPE_CHID
/**
 * H/w defined value for next Tsg ID type in engine status h/w register.
 */
#define ENGINE_STATUS_CTX_NEXT_ID_TYPE_TSGID ENGINE_STATUS_CTX_ID_TYPE_TSGID
/**
 * S/w defined value for unknown ID type.
 */
#define ENGINE_STATUS_CTX_NEXT_ID_TYPE_INVALID ENGINE_STATUS_CTX_ID_TYPE_INVALID

/**
 * S/w defined value for unknown ID.
 */
#define ENGINE_STATUS_CTX_ID_INVALID (~U32(0U))
/**
 * S/w defined value for unknown next ID.
 */
#define ENGINE_STATUS_CTX_NEXT_ID_INVALID ENGINE_STATUS_CTX_ID_INVALID

enum nvgpu_engine_status_ctx_status {
	/**
	 * Context is not loaded on engine. Both id and next_id are invalid.
	 */
	NVGPU_CTX_STATUS_INVALID,
	/**
	 * Context is loaded on the engine. id field of engine_status
	 * h/w register is valid. Since no context is on deck for switching,
	 * next_id is not valid.
	 */
	NVGPU_CTX_STATUS_VALID,
	/**
	 * Host is loading a new context and the previous context is
	 * invalid. In this state only next_id is valid.
	 */
	NVGPU_CTX_STATUS_CTXSW_LOAD,
	/**
	 * Host is saving the current context and not loading a new one.
	 * In this state only id is valid.
	 */
	NVGPU_CTX_STATUS_CTXSW_SAVE,
	/**
	 * Host is switching between two valid contexts. In this state both
	 * id and next_id are valid.
	 */
	NVGPU_CTX_STATUS_CTXSW_SWITCH,
};


struct nvgpu_engine_status_info {
	/** Engine status h/w register's read value. */
	u32 reg_data;
	/** @cond DOXYGEN_SHOULD_SKIP_THIS */
	/* Ampere+ engine status additions */

	/** Engine status_1 h/w register's read value. */
	u32 reg1_data;
	/** @endcond DOXYGEN_SHOULD_SKIP_THIS */
	/** Channel or tsg id that is currently assigned to the engine. */
	u32 ctx_id;
	/** Ctx_status field of engine_status h/w register. */
	u32 ctxsw_state;
	/** Specifies whether #ctx_id is of channel or tsg type. */
	u32 ctx_id_type;
	/** Channel or tsg id that will be assigned to the engine. */
	u32 ctx_next_id;
	/** Specifies whether #ctx_next_id is of channel or tsg type. */
	u32 ctx_next_id_type;
	/**
	 * Field #is_faulted is applicable for ce engine only and should be
	 * ignored for other types of engines. This is set when host
	 * receives a fault message from ce engine.
	 */
	bool is_faulted;
	/**
	 * Field #is_busy is set if engine is not idle. Host will report
	 * non-idle if Host is about to send methods as well as when a context
	 * request is outstanding to the engine, even when the engine itself is
	 * idle.
	 */
	bool is_busy;
	/**
	 * Set to true if host is switching between two valid contexts.
	 */
	bool ctxsw_in_progress;
	/**
	 * This is applicable to gr and ce engines.
	 * if #in_reload_status is set to true and #ctxsw_status is
	 * #NVGPU_CTX_STATUS_VALID, it indicates that the context currently on
	 * the engine was reloaded.
	 * if #in_reload_status is set to true and #ctxsw_status is
	 * #NVGPU_CTX_STATUS_INVALID, it indicates that the last context on the
	 * channel was reloaded.
	 */
	bool in_reload_status;
	/** Enum for ctx_status field of engine_status h/w register */
	enum nvgpu_engine_status_ctx_status ctxsw_status;
};

/**
 * @brief Check if #ctxsw_status is set to switch.
 *
 * @param engine_status [in]	Pointer to struct containing engine_status h/w
 * 				reg/field value.
 *
 * @return Interprets #engine_status and returns true if channel
 *         status is set to #NVGPU_CTX_STATUS_CTXSW_SWITCH else returns false.
 */
bool nvgpu_engine_status_is_ctxsw_switch(struct nvgpu_engine_status_info
		*engine_status);
/**
 * @brief Check if #ctxsw_status is set to load.
 *
 * @param engine_status [in]	Pointer to struct containing engine_status h/w
 * 				reg/field value.
 *
 * @return Interprets #engine_status and returns true if channel
 *         status is set to #NVGPU_CTX_STATUS_CTXSW_LOAD else returns false.
 */
bool nvgpu_engine_status_is_ctxsw_load(struct nvgpu_engine_status_info
		*engine_status);
/**
 * @brief Check if #ctxsw_status is set to save.
 *
 * @param engine_status [in]	Pointer to struct containing engine_status h/w
 * 				reg/field value.
 *
 * @return Interprets #engine_status and returns true if channel
 *         status is set to #NVGPU_CTX_STATUS_CTXSW_SAVE else returns false.
 */
bool nvgpu_engine_status_is_ctxsw_save(struct nvgpu_engine_status_info
		*engine_status);
/**
 * @brief Check if #ctxsw_status is set to switch or load or save.
 *
 * @param engine_status [in]	Pointer to struct containing engine_status h/w
 * 				reg/field value.
 *
 * @return Interprets #engine_status and returns true if channel
 *         status is set to #NVGPU_CTX_STATUS_CTXSW_SWITCH or
 *         #NVGPU_CTX_STATUS_CTXSW_LOAD or #NVGPU_CTX_STATUS_CTXSW_SAVE
 *         else returns false.
 */
bool nvgpu_engine_status_is_ctxsw(struct nvgpu_engine_status_info
		*engine_status);
/**
 * @brief Check if #ctxsw_status is set to invalid.
 *
 * @param engine_status [in]	Pointer to struct containing engine_status h/w
 * 				reg/field value.
 *
 * @return Interprets #engine_status and returns true if channel
 *         status is set to #NVGPU_CTX_STATUS_INVALID else returns false.
 */
bool nvgpu_engine_status_is_ctxsw_invalid(struct nvgpu_engine_status_info
		*engine_status);
/**
 * @brief - Check if #ctxsw_status is set to valid.
 *
 * @param engine_status [in]	Pointer to struct containing engine_status h/w
 * 				reg/field value.
 *
 * @return Interprets #engine_status and returns true if channel
 *         status is set to #NVGPU_CTX_STATUS_VALID else returns false.
 */
bool nvgpu_engine_status_is_ctxsw_valid(struct nvgpu_engine_status_info
		*engine_status);
/**
 * @brief Check if #ctx_id_type is tsg.
 *
 * @param engine_status [in]	Pointer to struct containing engine_status h/w
 * 				reg/field value.
 *
 * @return Interprets #engine_status and returns true if #ctx_id_type
 *         is #ENGINE_STATUS_CTX_ID_TYPE_TSGID else returns false.
 */
bool nvgpu_engine_status_is_ctx_type_tsg(struct nvgpu_engine_status_info
		*engine_status);
/**
 * @brief Check if #ctx_next_id_type is tsg.
 *
 * @param engine_status [in]	Pointer to struct containing engine_status h/w
 * 				reg/field value.
 *
 * @return Interprets #engine_status and returns true if
 *         #ctx_next_id_type is #ENGINE_STATUS_CTX_NEXT_ID_TYPE_TSGID else
 *         returns false.
 */
bool nvgpu_engine_status_is_next_ctx_type_tsg(struct nvgpu_engine_status_info
		*engine_status);
/**
 * @brief Get ctx_id and ctx_id_type info.
 *
 * @param engine_status [in]	Pointer to struct containing engine_status h/w
 * 				reg/field value.
 * @param ctx_id [out]		Pointer that is updated with #ctx_id as set in
 *				input param #engine_status.
 * @param ctx_type [out]	Pointer that is updated with #ctx_id_type as set
 *				in input param #engine_status.
 *
 * @return Interprets #engine_status and updates input params #ctx_id
 *         and #ctx_type.
 */
void nvgpu_engine_status_get_ctx_id_type(struct nvgpu_engine_status_info
		*engine_status, u32 *ctx_id, u32 *ctx_type);
/**
 * @brief Get next_ctx_id and next_ctx_id_type info.
 *
 * @param engine_status [in]	Pointer to struct containing engine_status h/w
 * 				reg/field value.
 * @param ctx_next_id [out]	Pointer that is updated with #ctx_next_id as set
 * 				in input param #engine_status.
 * @param ctx_next_type [out]	Pointer that is updated with #ctx_next_id_type
 * 				as set in input param #engine_status.
 *
 * @return Interprets #engine_status and updates input params
 *         #ctx_next_id and #ctx_next_type.
 */
void nvgpu_engine_status_get_next_ctx_id_type(struct nvgpu_engine_status_info
		*engine_status, u32 *ctx_next_id,
		u32 *ctx_next_type);

#endif /* NVGPU_ENGINE_STATUS_H */
