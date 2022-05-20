/*
 * Copyright (c) 2016-2020, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/gk20a.h>
#include <nvgpu/boardobjgrpmask.h>
#include "boardobj.h"

/*
* Assures that unused bits (size .. (maskDataCount * 32 - 1)) are always zero.
*/
#define BOARDOBJGRPMASK_NORMALIZE(_pmask)                                      \
	((_pmask)->data[(_pmask)->maskdatacount-1U] &= (_pmask)->lastmaskfilter)

static int import_mask_data(struct boardobjgrpmask *mask, u8 bitsize,
		struct ctrl_boardobjgrp_mask *extmask)
{
	u8 index;

	if (mask == NULL) {
		return -EINVAL;
	}
	if (extmask == NULL) {
		return -EINVAL;
	}
	if (mask->bitcount != bitsize) {
		return -EINVAL;
	}

	for (index = 0; index < mask->maskdatacount; index++) {
		mask->data[index] = extmask->data[index];
	}

	BOARDOBJGRPMASK_NORMALIZE(mask);

	return 0;
}


static int clr_mask_data(struct boardobjgrpmask *mask)
{
	u8 index;

	if (mask == NULL) {
		return -EINVAL;
	}
	for (index = 0; index < mask->maskdatacount; index++) {
		mask->data[index] = 0;
	}

	return 0;
}

int nvgpu_boardobjgrpmask_init(struct boardobjgrpmask *mask, u8 bitsize,
		struct ctrl_boardobjgrp_mask *extmask)
{
	if (mask == NULL) {
		return -EINVAL;
	}
	if ((bitsize != CTRL_BOARDOBJGRP_E32_MAX_OBJECTS) &&
		(bitsize != CTRL_BOARDOBJGRP_E255_MAX_OBJECTS)) {
		return -EINVAL;
	}

	mask->bitcount = bitsize;
	mask->maskdatacount = CTRL_BOARDOBJGRP_MASK_DATA_SIZE(bitsize);
	mask->lastmaskfilter = U32(bitsize) %
		CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_BIT_SIZE;

	mask->lastmaskfilter = (mask->lastmaskfilter == 0U) ?
		0xFFFFFFFFU : (BIT32(mask->lastmaskfilter) - 1U);

	return (extmask == NULL) ?
		clr_mask_data(mask) :
		import_mask_data(mask, bitsize, extmask);
}

bool nvgpu_boardobjgrpmask_bit_get(struct boardobjgrpmask *mask, u8 bitidx)
{
	u8 index;
	u8 offset;

	if (mask == NULL) {
		return false;
	}
	if (bitidx >= mask->bitcount) {
		return false;
	}

	index = CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_INDEX(bitidx);
	offset = CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_OFFSET(bitidx);

	return (mask->data[index] & BIT32(offset)) != 0U;
}

int nvgpu_boardobjgrpmask_export(struct boardobjgrpmask *mask, u8 bitsize,
		struct ctrl_boardobjgrp_mask *extmask)
{
	u8 index;

	if (mask == NULL) {
		return -EINVAL;
	}
	if (extmask == NULL) {
		return -EINVAL;
	}
	if (mask->bitcount != bitsize) {
		return -EINVAL;
	}

	for (index = 0; index < mask->maskdatacount; index++) {
		extmask->data[index] = mask->data[index];
	}

	return 0;
}

u8 nvgpu_boardobjgrpmask_bit_set_count(struct boardobjgrpmask *mask)
{
	u8 index;
	u8 result = 0;

	if (mask == NULL) {
		return result;
	}

	for (index = 0; index < mask->maskdatacount; index++) {
		u32 m = mask->data[index];

		NUMSETBITS_32(m);
		result += (u8)m;
	}

	return result;
}

u8 nvgpu_boardobjgrpmask_bit_idx_highest(struct boardobjgrpmask *mask)
{
	u8 index;
	u8 result = CTRL_BOARDOBJ_IDX_INVALID;

	if (mask == NULL) {
		return result;
	}

	for (index = 0; index < mask->maskdatacount; index++) {
		u32 m = mask->data[index];

		if (m != 0U) {
			HIGHESTBITIDX_32(m);
			result = (u8)m + index *
			CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_BIT_SIZE;
			break;
		}
	}

	return result;
}

int nvgpu_boardobjgrpmask_bit_clr(struct boardobjgrpmask *mask, u8 bitidx)
{
	u8 index;
	u8 offset;

	if (mask == NULL) {
		return -EINVAL;
	}
	if (bitidx >= mask->bitcount) {
		return -EINVAL;
	}

	index = CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_INDEX(bitidx);
	offset = CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_OFFSET(bitidx);

	mask->data[index] &= ~BIT32(offset);

	return 0;
}

int nvgpu_boardobjgrpmask_bit_set(struct boardobjgrpmask *mask, u8 bitidx)
{
	u8 index;
	u8 offset;

	if (mask == NULL) {
		return -EINVAL;
	}
	if (bitidx >= mask->bitcount) {
		return -EINVAL;
	}

	index = CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_INDEX(bitidx);
	offset = CTRL_BOARDOBJGRP_MASK_MASK_ELEMENT_OFFSET(bitidx);

	mask->data[index] |= BIT32(offset);

	return 0;
}

bool nvgpu_boardobjgrpmask_sizeeq(struct boardobjgrpmask *op1,
		struct boardobjgrpmask *op2)
{
	if (op1 == NULL) {
		return false;
	}
	if (op2 == NULL) {
		return false;
	}

	return op1->bitcount == op2->bitcount;
}

int nvgpu_boardobjmask_or(struct boardobjgrpmask *dst,
		struct boardobjgrpmask *mask1, struct boardobjgrpmask *mask2)
{
	u8 idx;

	for (idx = 0; idx < dst->maskdatacount; idx++) {
		dst->data[idx] = mask1->data[idx] | mask2->data[idx];
	}

	return 0;

}

int nvgpu_boardobjmask_and(struct boardobjgrpmask *dst,
		struct boardobjgrpmask *mask1, struct boardobjgrpmask *mask2)
{
	u8 idx;

	for (idx = 0; idx < dst->maskdatacount; idx++) {
		dst->data[idx] = mask1->data[idx] & mask2->data[idx];
	}

	return 0;

}

