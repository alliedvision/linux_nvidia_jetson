/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/pbdma_status.h>

bool nvgpu_pbdma_status_is_chsw_switch(struct nvgpu_pbdma_status_info
		*pbdma_status)
{
	return pbdma_status->chsw_status == NVGPU_PBDMA_CHSW_STATUS_SWITCH;
}
bool nvgpu_pbdma_status_is_chsw_load(struct nvgpu_pbdma_status_info
		*pbdma_status)
{
	return pbdma_status->chsw_status == NVGPU_PBDMA_CHSW_STATUS_LOAD;
}
bool nvgpu_pbdma_status_is_chsw_save(struct nvgpu_pbdma_status_info
		*pbdma_status)
{
	return pbdma_status->chsw_status == NVGPU_PBDMA_CHSW_STATUS_SAVE;
}
bool nvgpu_pbdma_status_is_chsw_valid(struct nvgpu_pbdma_status_info
		*pbdma_status)
{
	return pbdma_status->chsw_status == NVGPU_PBDMA_CHSW_STATUS_VALID;
}
bool nvgpu_pbdma_status_is_id_type_tsg(struct nvgpu_pbdma_status_info
		*pbdma_status)
{
	return pbdma_status->id_type == PBDMA_STATUS_ID_TYPE_TSGID;
}
bool nvgpu_pbdma_status_is_next_id_type_tsg(struct nvgpu_pbdma_status_info
		*pbdma_status)
{
	return pbdma_status->next_id_type == PBDMA_STATUS_NEXT_ID_TYPE_TSGID;
}
