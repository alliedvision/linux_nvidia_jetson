/*
 * Copyright (c) 2016-2021, NVIDIA CORPORATION. All rights reserved.
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

#ifndef PVA_UCODE_HEADER_H
#define PVA_UCODE_HEADER_H

#include <pva-types.h>
#include <pva-ucode-header-types.h>

#define MAX_SEGMENT_NAME_LEN 64

/*
 * PVA uCode Header.
 *
 * There is a basic header that describes the uCode.  Other than the
 * validation information (such as versions, checksums (MD5 hash?), etc)
 * it describes the various segments of the uCode image.  The important
 * thing to note is that there are multiple segments for various parts of
 * the uCode.
 *
 * Each segment has:
 *	- type: this indicates the type of segment it is.
 *	- id: this gives a uniqueness to the segment when there are multiple
 *	segments of the same type.  It also allows different segments types
 *	to be related by using the same segment ID (such as relating VPU code,
 *	R5 application code and parameter data together).
 *	- name: this is NUL terminated string that is the "name" of the segment
 *	- size: size of the segment in bytes
 *	- offset: this is the offset from the start of the binary as to
 *	where the data contained in the segment is to be placed.
 *	- address: this is the address of where the data in the segment is
 *	to be written to.
 *      - physical address: this is used in some segments to denote where in
 *      the 40-bit address space the segment is located.  This allows for
 *      setting up some of the segment registers.
 *
 * A segment can define a region but contain no data.  In those cases, the
 * file offset would be 0.
 *
 * In the case of DRAM the load address and size can be used to setup the
 * relevant segment registers and DRAM apertures.
 *
 */

/*
 * There can be multiple segments of the same type.
 */
struct pva_ucode_seg_s {
	uint32_t type; /* type of segment */
	uint32_t id; /* ID of segment */
	uint32_t size; /* size of the segment */
	uint32_t offset; /* offset from header to segment start */
	uint32_t addr; /* load address of segment */
	uint8_t name[MAX_SEGMENT_NAME_LEN];
	uint64_t phys_addr __aligned(8);
};

/*
 * Ucode header gives information on what kind of images are contained in
 * a binary.
 *
 * nsegments      : Number of segments available in pva_ucode_r5_sysfw_info_t.
 *
 * R5 system image layout used for booting R5.
 *	+--------------------------------+
 *	+          Ucode header          +
 *	+--------------------------------+
 *	+           struct               +
 *	+   pva_ucode_r5_sysfw_info_t    +
 *	+--------------------------------+
 *	+                                +
 *	+   pva firwmare data/code       +
 *	+--------------------------------+
 */
struct __packed pva_ucode_hdr_s {
	uint32_t magic;
	uint32_t hdr_version;
	uint32_t ucode_version;
	uint32_t nsegments;
};

struct pva_ucode_r5_sysfw_info_s {
	struct pva_ucode_seg_s evp __aligned(128);
	struct pva_ucode_seg_s dram __aligned(128);
	struct pva_ucode_seg_s crash_dump __aligned(128);
	struct pva_ucode_seg_s trace_log __aligned(128);
	struct pva_ucode_seg_s code_coverage __aligned(128);
	struct pva_ucode_seg_s debug_log __aligned(128);
	struct pva_ucode_seg_s cached_dram __aligned(128);
};

#endif
