/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
 */

#ifndef INCLUDE_CAMRTC_ISP5_TILING_H
#define INCLUDE_CAMRTC_ISP5_TILING_H

#include "camrtc-common.h"
#include "camrtc-capture.h"

struct isp5_tile_width {
	uint16_t tile_width_first;
	uint16_t tile_width_middle;
	uint16_t tiles_in_slice;
};

struct isp5_slice_height {
	uint16_t slice_height;
	uint16_t vi_first_slice_height;
	uint16_t slices_in_image;
};

#define ISP5_MIN_TILE_WIDTH	U16_C(128)
#define ISP5_MAX_TILE_WIDTH	U16_C(1024)
#define ISP5_MIN_SLICE_HEIGHT	U16_C(128)
#define ISP5_MAX_SLICE_HEIGHT	U16_C(540)

static inline uint16_t isp5_min_u16(const uint16_t a, const uint16_t b)
{
	return (a < b) ? a : b;
}

static inline uint16_t isp5_max_u16(const uint16_t a, const uint16_t b)
{
	return (a > b) ? a : b;
}

static inline uint16_t isp5_align_down(const uint16_t val, const uint16_t alignment)
{
	/* Assuming no alignment for alignment = 0 */
	if (alignment == 0U) {
		return val;
	}
	//coverity[cert_int31_c_violation] # 0 <= (val % alignment) < alignment
	const uint16_t rem = val % alignment;
	//coverity[cert_int31_c_violation] # 0 <= rem <= val
	return (val - rem);
}

static inline bool isp5_align_up(const uint16_t val, const uint16_t alignment, uint16_t *result)
{
	/* Assuming no alignment for alignment = 0 */
	if (alignment == 0U) {
		*result = val;
		return true;
	}
	//coverity[cert_int31_c_violation] # 0 <= (val % alignment) < alignment
	const uint16_t rem = val % alignment;
	bool ret_val = false;
	uint16_t aligned_width = 0U;

	if (rem == 0U) {
		*result = val;
		ret_val = true;
	} else {
		aligned_width = isp5_align_down(val, alignment);
		if ((aligned_width + alignment) > (uint32_t)UINT16_MAX) {
			ret_val = false;
		} else {
			*result = aligned_width + alignment;
			ret_val = true;
		}
	}
	return ret_val;
}

static inline bool isp5_div_round_up(const uint16_t x, const uint16_t y, uint16_t *result)
{
	if (y == 0U) {
		return false;
	}
	//coverity[cert_int30_c_violation] # x & y are uint16_t
	*result = (uint16_t)(((uint32_t)x + ((uint32_t)y - 1U)) / (uint32_t)y);

	return true;
}

/**
 * Calculate suitable tile width for given capture descriptor and ISP program
 */
__attribute__((unused))
static bool isp5_find_tile_width(const struct isp5_program * const prg,
				 const struct isp_capture_descriptor * const cd,
				 struct isp5_tile_width * const tiling)
{
	const uint16_t img_width = cd->surface_configs.mr_width;
	uint16_t aligned_width = 0U;
	bool ret_val = false;
	int32_t tmp_width = 0;
	uint16_t tmp_round_up = 0U;

	if (img_width <= ISP5_MAX_TILE_WIDTH) {
		tiling->tile_width_first = img_width;
		tiling->tile_width_middle = 0U;
		tiling->tiles_in_slice = 1U;
		return true;
	}

	const uint16_t alignment = prg->overfetch.alignment;

	if (alignment == 0U) {
		return false;
	}

	tmp_width = (ISP5_MAX_TILE_WIDTH - prg->overfetch.right) + prg->overfetch.pru_ovf_h;

	if ((0 > tmp_width) || (tmp_width > UINT16_MAX)) {
		return false;
	}

	aligned_width = isp5_align_down((uint16_t)tmp_width, alignment);

	tmp_width = aligned_width - prg->overfetch.right + prg->overfetch.pru_ovf_h;

	if ((0 > tmp_width) || (tmp_width > UINT16_MAX)) {
		return false;
	}

	const uint16_t max_width_first = (uint16_t)tmp_width;
	tmp_width = (ISP5_MAX_TILE_WIDTH - prg->overfetch.right) - prg->overfetch.left;

	if ((0 > tmp_width) || (tmp_width > UINT16_MAX)) {
		return false;
	}

	const uint16_t max_width_middle = isp5_align_down((uint16_t)tmp_width, alignment);

	/* Last tile right edge does not need to be aligned */
	const uint16_t max_width_last = ISP5_MAX_TILE_WIDTH - prg->overfetch.left;
	const uint16_t prg_right = prg->overfetch.right;
	const uint16_t min_width = isp5_max_u16(ISP5_MIN_TILE_WIDTH, prg_right);

	uint16_t tile_count = 2U;

	if (img_width > (max_width_first + max_width_last)) {
		uint16_t middle_tiles;
		const uint16_t pixels_left = img_width - max_width_first - max_width_last;

		ret_val = isp5_div_round_up(pixels_left,
					isp5_min_u16(max_width_middle, max_width_first),
					&middle_tiles);
		if (!ret_val) {
			return false;
		}
		//coverity[cert_int30_c_violation] # tile_count < image width
		tile_count += middle_tiles;
	}

	/*
	 * Divide image into roughly evenly spaced aligned tiles
	 * 0 <= (img_width + alignment -1)/tile_count <= ISP5_MAX_TILE_WIDTH
	 */
	ret_val = isp5_div_round_up(img_width, alignment, &tmp_round_up);
	if (!ret_val) {
		return false;
	}
	// coverity[cert_int31_c_violation] # see above comment.
	const uint16_t tile_width = (tmp_round_up / tile_count) * alignment;

	tmp_width = (tile_width + prg_right) - prg->overfetch.pru_ovf_h;
	if ((0 > tmp_width) || (tmp_width > UINT16_MAX)) {
		return false;
	}

	aligned_width = isp5_align_down((uint16_t)tmp_width, alignment);

	tmp_width = aligned_width - prg_right + prg->overfetch.pru_ovf_h;

	if ((0 > tmp_width) || (tmp_width > UINT16_MAX)) {
		return false;
	}
	/*
	 * The right edge of a tile as seen by AP must be aligned
	 * correctly for CAR filter.  When first tile width fulfills
	 * this condition, the rest of tiles are simple to andle by
	 * just aligning their active width
	 */
	uint16_t first_width = isp5_min_u16(max_width_first, (uint16_t)tmp_width);
	uint16_t middle_width = (tile_count > 2U) ? isp5_min_u16(max_width_middle, tile_width) : 0U;
	 //coverity[cert_int30_c_violation] # tile dimensions <= image dimensions
	 //coverity[cert_int31_c_violation] # tile dimensions <= image dimensions and tile_count >= 2
	uint16_t last_width = img_width - first_width - ((tile_count - 2U) * middle_width);

	/*
	 * Ensure that last tile is wide enough. Width of the first
	 * tile a this point is guaranteed to be greater than:
	 *
	 * ((max_tile_width - total overfetch - 2*alignment) / 2) - alignment >= 407 pixels
	 *
	 * So there is no risk that this correction will cause it to
	 * be too narrow.
	 */
	if (last_width < min_width) {
		uint16_t corr = 0U;
		ret_val = isp5_align_up(min_width-last_width, alignment, &corr);
		if (!ret_val) {
			return false;
		}

		//coverity[cert_int30_c_violation] # corr <= tile width <= image width
		first_width -= corr;
		//coverity[cert_int30_c_violation] # corr <= tile width <= image width
		last_width  += corr;
	} else if (last_width > max_width_last) {
		/* Try first increasing middle tile width */
		if (tile_count > 2U) {
			//coverity[cert_int31_c_violation] # max_width_middle >= middle_width
			const uint16_t max_middle_corr = max_width_middle - middle_width;
			const uint16_t corr = last_width - max_width_last;
			ret_val = isp5_div_round_up(corr, tile_count - 2U, &tmp_round_up);
			if (!ret_val) {
				return false;
			}
			ret_val = isp5_align_up(tmp_round_up, alignment, &aligned_width);
			if (!ret_val) {
				return false;
			}
			const uint16_t middle_corr = isp5_min_u16(max_middle_corr, aligned_width);
			//coverity[cert_int30_c_violation] # middle_corr <= tile width <= image width
			middle_width += middle_corr;
			/* middle_corr <= tile width <= image width, tile_count < image width */
			//coverity[cert_int30_c_violation] # see above comment
			last_width -= middle_corr * (tile_count - 2U);
		}

		if (last_width > max_width_last) {
			uint16_t first_corr = 0U;
			ret_val = isp5_align_up(last_width - max_width_last, alignment, &first_corr);

			if (!ret_val) {
				return false;
			}
			//coverity[cert_int30_c_violation] # first_corr <= tile width <= image width
			first_width += first_corr;
			//coverity[cert_int30_c_violation] # first_corr <= tile width <= image width
			last_width -= first_corr;
		}
	} else {
		// for MISRA C++ 2008 6-4-2 compliance
	}

	if ((first_width < min_width) ||
		(first_width > max_width_first) ||
		(last_width < min_width) ||
		(last_width > max_width_last)) {
		return false;
	}

	if ((tile_count > 2U) &&
		((middle_width < min_width) || (middle_width > max_width_middle))) {
		return false;
	}

	tiling->tile_width_first = first_width;
	tiling->tile_width_middle = middle_width;
	tiling->tiles_in_slice = tile_count;

	return true;
}

__attribute__((unused))
static bool isp5_find_tile_width_dpcm(const struct isp5_program * const prg,
					const struct isp_capture_descriptor * const cd,
					struct isp5_tile_width * const tiling)
{
	const uint16_t prg_alignment = prg->overfetch.alignment;
	const uint16_t alignment = isp5_max_u16(prg_alignment, 8U);

	const uint16_t prg_right = prg->overfetch.right;
	const uint16_t max_width_middle = isp5_align_down(ISP5_MAX_TILE_WIDTH - prg_right -
						prg->overfetch.left,
						alignment);
	uint16_t aligned_width = 0U;
	bool ret_val = false;
	int32_t tmp_width = 0;
	uint16_t tmp_round_up;

	if (cd->surface_configs.chunk_width_middle > max_width_middle) {
		return false;
	}

	tiling->tile_width_middle = cd->surface_configs.chunk_width_middle;

	tmp_width = (cd->surface_configs.chunk_width_first + prg->overfetch.left + prg_right) -
						prg->overfetch.pru_ovf_h;

	if ((0 > tmp_width) || (tmp_width > UINT16_MAX)) {
		return false;
	}

	ret_val = isp5_align_up((uint16_t)tmp_width, alignment, &aligned_width);

	if (!ret_val) {
		return false;
	}

	tmp_width = aligned_width - prg_right +  prg->overfetch.pru_ovf_h;

	if ((0 > tmp_width) || (tmp_width > UINT16_MAX)) {
		return false;
	}

	/*
	 * Width of first tile must set so that left overfetch area of
	 * 2nd tile fits into 2nd chunk.
	 */
	tiling->tile_width_first = (uint16_t)tmp_width;

	const uint16_t min_width = isp5_max_u16(ISP5_MIN_TILE_WIDTH, prg_right);
	const uint16_t max_width_first = isp5_align_down(ISP5_MAX_TILE_WIDTH - prg_right,
							 alignment);
	if ((tiling->tile_width_first < min_width) ||
	    (tiling->tile_width_first > max_width_first)) {
		return false;
	}

	if ((tiling->tile_width_first + prg_right) >
		(cd->surface_configs.chunk_width_first +
		 cd->surface_configs.chunk_overfetch_width)) {
		return false;
	}

	/* cd->surface_configs.chunk_width_middle can't 0 here */
	if (cd->surface_configs.chunk_width_middle == 0U) {
		return false;
	}
	/*
	 * chunk_width_first >= 0 and chunk_width_middle >= 1
	 * mr_width >= chunk_width_first
	 */
	//coverity[cert_int31_c_violation] # see above comment
	ret_val = isp5_div_round_up(cd->surface_configs.mr_width -
					cd->surface_configs.chunk_width_first,
					cd->surface_configs.chunk_width_middle,
					&tmp_round_up);
	if ((!ret_val) || (tmp_round_up == 0xffffU)) {
		return false;
	}

	tiling->tiles_in_slice = 1U + tmp_round_up;
	/*
	 * CERT INT30-C deviations:
	 * Tile properties are governed by image properties.
	 * tiles_in_slice < image width, tile_width_middle <= image width
	 * tile_width_first <= image width.
	 */
	//coverity[cert_int30_c_violation] # see above comment
	//coverity[cert_int31_c_violation] # see above comment
	const uint16_t last_width = cd->surface_configs.mr_width -
					tiling->tile_width_first -
					//coverity[cert_int30_c_violation] # see above comment
					//coverity[cert_int31_c_violation] # see above comment
					((tiling->tiles_in_slice - 1U) * tiling->tile_width_middle);

	const uint16_t max_width_last = ISP5_MAX_TILE_WIDTH - prg->overfetch.left;

	if ((last_width > max_width_last) || (last_width < min_width)) {
		return false;
	}

	return true;
}

__attribute__((unused))
static bool isp5_find_slice_height(const uint16_t img_height,
				   struct isp5_slice_height * const slicing)
{
	bool ret;
	uint16_t tmp_round_up = 0U;
	if (img_height < ISP5_MIN_SLICE_HEIGHT) {
		ret = false;
	} else if ((img_height % 2U) != 0U) {
		ret = false;
	} else if (img_height <= ISP5_MAX_SLICE_HEIGHT) {
		slicing->slices_in_image = U16_C(1);
		slicing->slice_height = img_height;
		slicing->vi_first_slice_height = img_height;
		ret = true;
	} else {
		uint16_t slice_height = ISP5_MAX_SLICE_HEIGHT;
		uint16_t slice_count = 0U;

		ret = isp5_div_round_up(img_height, ISP5_MAX_SLICE_HEIGHT, &slice_count);

		if (!ret) {
			return false;
		}

		const uint16_t last_height = img_height -
					/*
					 * img_height >= slice_count * max_slice_height and
					 * slice_count >= 1U always
					 */
					 //coverity[cert_int30_c_violation] # above comment
					 //coverity[cert_int31_c_violation] # above comment
					(ISP5_MAX_SLICE_HEIGHT * (slice_count - 1U));

		if (last_height < ISP5_MIN_SLICE_HEIGHT) {
			uint16_t slice_corr = 0U;
			const uint16_t corr = ISP5_MIN_SLICE_HEIGHT - last_height;

			ret = isp5_div_round_up(corr, slice_count - 1U, &tmp_round_up);
			if (!ret) {
				return false;
			}

			//coverity[cert_int31_c_violation] # slice_count >= 1U always
			ret = isp5_align_up(tmp_round_up, 2U, &slice_corr);
			if (!ret) {
				return false;
			}
			//coverity[cert_int30_c_violation] # slice_corr < slice_height always
			slice_height -= slice_corr;
		}

		slicing->slice_height = slice_height;
		slicing->slices_in_image = slice_count;
		slicing->vi_first_slice_height = (slice_count == 1U) ?
						 slice_height : (slice_height + 18U);
		ret = true;
	}
	return ret;
}

#endif /* INCLUDE_CAMRTC_ISP5_TILING_H */

