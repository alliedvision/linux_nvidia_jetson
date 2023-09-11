/*
 * Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PVA_HWSEQ_H
#define PVA_HWSEQ_H

#include <linux/kref.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>

#define PVA_HWSEQ_FRAME_ADDR	0xC0DE
#define PVA_HWSEQ_DESC_ADDR	0xDEAD
#define PVA_HWSEQ_COL_ROW_LIMIT 1
#define PVA_HWSEQ_DESC_LIMIT	2

struct pva_hwseq_frame_header_s {
	u16	fid;
	u8	fr;
	u8	no_cr;
	s16	to;
	s16	fo;
	u8	pad_r;
	u8	pad_t;
	u8	pad_l;
	u8	pad_b;
} __packed;

struct pva_hwseq_cr_header_s {
	u8	dec;
	u8	crr;
	s16	cro;
} __packed;

struct pva_hwseq_desc_header_s {
	u8	did1;
	u8	dr1;
	u8	did2;
	u8	dr2;
} __packed;

struct pva_dma_hwseq_desc_entry_s {
	uint8_t did; // desc id
	uint8_t dr; // desc repetition
};

struct pva_hw_sweq_blob_s {
	struct pva_hwseq_frame_header_s f_header;
	struct pva_hwseq_cr_header_s cr_header;
	struct pva_hwseq_desc_header_s desc_header;
} __packed;

static inline bool is_frame_mode(u16 id)
{
	return (id == PVA_HWSEQ_FRAME_ADDR);
}

static inline bool is_desc_mode(u16 id)
{
	return (id == PVA_HWSEQ_DESC_ADDR);
}

struct pva_hwseq_grid_info_s {
	int32_t	tile_x[2];
	int32_t	tile_y[2];
	int32_t	pad_x[2];
	int32_t	pad_y[2];
	int32_t	grid_size_x;
	int32_t	grid_size_y;
	int32_t	grid_step_x;
	int32_t	grid_step_y;
	int32_t	head_tile_count;
	bool	is_split_padding;
};

struct pva_hwseq_frame_info_s{
	int32_t	start_x;
	int32_t	start_y;
	int32_t	end_x;
	int32_t	end_y;
};

struct pva_hwseq_buffer_s {
	const uint8_t	*data;
	uint32_t	bytes_left;
};

struct pva_hwseq_priv_s{
	struct pva_hwseq_buffer_s	*blob;
	struct pva_hwseq_frame_header_s	*hdr;
	struct pva_hwseq_cr_header_s	*colrow;
	struct pva_submit_task		*task;
	struct nvpva_dma_channel	*dma_ch;
	struct nvpva_dma_descriptor	*head_desc;
	struct nvpva_dma_descriptor	*tail_desc;
	struct pva_hwseq_desc_header_s	*dma_descs;
	uint32_t			tiles_per_packet;
	int32_t				max_tx;
	int32_t				max_ty;
	bool				is_split_padding;
	bool				is_raster_scan;
	bool				verify_bounds;
};
#endif

