/*
 * Copyright (c) 2019-2021, NVIDIA Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

static struct nvhost_actmon_register __attribute__((__unused__))
	t19x_vic_actmon_registers[] = {
	/* NV_PVIC_TFBIF_ACTMON_ACTIVE_WEIGHT */
	{ .addr = 0x00002054, .val = 0x000000d8 },
	{}
};

static struct nvhost_actmon_register __attribute__((__unused__))
	t19x_nvdec_actmon_registers[] = {
	/* NV_PNVDEC_TFBIF_ACTMON_ACTIVE_WEIGHT */
	{ .addr = 0x00002c54, .val = 0x000000f8 },
	{}
};

static struct nvhost_actmon_register __attribute__((__unused__))
	t19x_nvenc_actmon_registers[] = {
	/* NV_PNVENC_TFBIF_ACTMON_ACTIVE_WEIGHT */
	{ .addr = 0x00001854, .val = 0x000000e0 },
	{}
};
