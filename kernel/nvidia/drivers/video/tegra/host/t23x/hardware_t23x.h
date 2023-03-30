/*
 * drivers/video/tegra/host/t23x/hardware_t23x.h
 *
 * Tegra T23X HOST1X Register Definitions
 *
 * Copyright (c) 2016-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __NVHOST_HARDWARE_T23X_H
#define __NVHOST_HARDWARE_T23X_H

#include "host1x/hw_host1x08_sync.h"
#include "host1x/hw_host1x6_uclass.h"
#include "host1x/hw_host1x7_channel.h"
#include "host1x/hw_host1x5_actmon.h"

/* sync registers */
#define NV_HOST1X_SYNCPT_NB_PTS 1024
#define NV_HOST1X_NB_MLOCKS 	24

#define NV_HOST1X_MLOCK_ID_NVCSI	9
#define NV_HOST1X_MLOCK_ID_ISP		10
#define NV_HOST1X_MLOCK_ID_ISP_THI	11
#define NV_HOST1X_MLOCK_ID_VI		17
#define NV_HOST1X_MLOCK_ID_VI_THI	7
#define NV_HOST1X_MLOCK_ID_VI2		13
#define NV_HOST1X_MLOCK_ID_VI2_THI	14
#define NV_HOST1X_MLOCK_ID_VIC		18
#define NV_HOST1X_MLOCK_ID_NVENC	19
#define NV_HOST1X_MLOCK_ID_NVDEC	20
#define NV_HOST1X_MLOCK_ID_NVJPG	21
#define NV_HOST1X_MLOCK_ID_NVJPG1	16
#define NV_HOST1X_MLOCK_ID_TSEC		22
#define NV_HOST1X_MLOCK_ID_OFA		8

#define HOST1X_THOST_ACTMON_NVENC	0x00000
#define HOST1X_THOST_ACTMON_VIC 	0x10000
#define HOST1X_THOST_ACTMON_NVDEC	0x20000
#define HOST1X_THOST_ACTMON_NVJPG	0x30000

/* Generic support */
static inline u32 nvhost_class_host_wait_syncpt(
	unsigned int indx, unsigned int threshold)
{
	return (indx << 24) | (threshold & 0xffffff);
}

static inline u32 nvhost_class_host_load_syncpt_base(
	unsigned int indx, unsigned int threshold)
{
	return host1x_uclass_wait_syncpt_indx_f(indx)
		| host1x_uclass_wait_syncpt_thresh_f(threshold);
}

static inline u32 nvhost_class_host_incr_syncpt(
	unsigned int cond, unsigned int indx)
{
	return host1x_uclass_incr_syncpt_cond_f(cond)
		| host1x_uclass_incr_syncpt_indx_f(indx);
}

static inline void __iomem *host1x_channel_aperture(void __iomem *p, int ndx)
{
	p += host1x_channel_ch_aperture_start_r() +
		ndx * host1x_channel_ch_aperture_size_r();
	return p;
}

enum {
	NV_HOST_MODULE_HOST1X = 0,
	NV_HOST_MODULE_MPE = 1,
	NV_HOST_MODULE_GR3D = 6
};

/* cdma opcodes */
static inline u32 nvhost_opcode_setclass(
	unsigned int class_id, unsigned int offset, unsigned int mask)
{
	return (0 << 28) | (offset << 16) | (class_id << 6) | mask;
}

static inline u32 nvhost_opcode_incr(unsigned int offset, unsigned int count)
{
	return (1 << 28) | (offset << 16) | count;
}

static inline u32 nvhost_opcode_nonincr(unsigned int offset, unsigned int count)
{
	return (2 << 28) | (offset << 16) | count;
}

static inline u32 nvhost_opcode_mask(unsigned int offset, unsigned int mask)
{
	return (3 << 28) | (offset << 16) | mask;
}

static inline u32 nvhost_opcode_imm(unsigned int offset, unsigned int value)
{
	return (4 << 28) | (offset << 16) | value;
}

static inline u32 nvhost_opcode_imm_incr_syncpt(
	unsigned int cond, unsigned int indx)
{
	return nvhost_opcode_imm(host1x_uclass_incr_syncpt_r(),
		nvhost_class_host_incr_syncpt(cond, indx));
}

static inline u32 nvhost_opcode_restart(unsigned int address)
{
	return (5 << 28) | (address >> 4);
}

static inline u32 nvhost_opcode_gather(unsigned int count)
{
	return (6 << 28) | count;
}

static inline u32 nvhost_opcode_gather_nonincr(
	unsigned int offset, unsigned int count)
{
	return (6 << 28) | (offset << 16) | BIT(15) | count;
}

static inline u32 nvhost_opcode_gather_incr(
	unsigned int offset, unsigned int count)
{
	return (6 << 28) | (offset << 16) | BIT(15) | BIT(14) | count;
}

static inline u32 nvhost_opcode_gather_insert(
	unsigned int offset, unsigned int incr, unsigned int count)
{
	return (6 << 28) | (offset << 16) | BIT(15) | (incr << 14) | count;
}

static inline u32 nvhost_opcode_setstreamid(unsigned int streamid)
{
	return (7 << 28) | streamid;
}

static inline u32 nvhost_opcode_setpayload(unsigned int payload)
{
	return (9 << 28) | payload;
}

static inline u32 nvhost_opcode_acquire_mlock(unsigned int id)
{
	return (14 << 28) | id;
}

static inline u32 nvhost_opcode_release_mlock(unsigned int id)
{
	return (14 << 28) | (1 << 24) | id;
}

static inline u32 nvhost_opcode_nonincr_w(unsigned int offset)
{
	return (11 << 28) | offset;
}

static inline u32 nvhost_opcode_incr_w(unsigned int offset)
{
	return (10 << 28) | offset;
}

#define NVHOST_OPCODE_NOOP nvhost_opcode_nonincr(0, 0)

static inline u32 nvhost_mask2(unsigned int x, unsigned int y)
{
	return 1 | (1 << (y - x));
}

#endif /* __NVHOST_HARDWARE_T23X_H */
