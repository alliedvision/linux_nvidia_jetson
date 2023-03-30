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

struct pva_hwseq_frame_header_s {
	u16	fid;
	u8	fr;
	u8	no_cr;
	u16	to;
	u16	fo;
	u8	pad_r;
	u8	pad_t;
	u8	pad_l;
	u8	pad_b;
} __packed;

struct pva_hwseq_cr_header_s {
	u8	dec;
	u8	crr;
	u16	cro;
} __packed;

struct pva_hwseq_desc_header_s {
	u8	did1;
	u8	dr1;
	u8	did2;
	u8	dr2;
} __packed;

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
#endif

