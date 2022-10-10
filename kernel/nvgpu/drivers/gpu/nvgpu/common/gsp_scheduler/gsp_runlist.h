/*
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_GSP_RUNLIST
#define NVGPU_GSP_RUNLIST

#include <nvgpu/device.h>

#define NVGPU_GSP_MAX_DEVTYPE 1U

struct nvgpu_gsp_device_info {
	/*
	 * Device index
	 */
	u8 device_id;
	/*
	 * TRUE when the device is a Host-driven method engine. FALSE otherwise.
	 */
	bool is_engine;
	/*
	 * The device's DEV_RUNLIST_PRI_BASE is the offset into BAR0 for the device's
	 * NV_RUNLIST PRI space.
	 */
	u32 runlist_pri_base;
	/*
	 * Engine description, like graphics, or copy engine.
	 */
	u32 engine_type;
	/*
	 * The unique per-device ID that host uses to identify any given engine.
	 */
	u32 engine_id;
	/*
	 * Specifies instance of a device, allowing SW to distinguish between
	 * multiple copies of a device present on the chip.
	 */
	u32 instance_id;
	/*
	 * Device's runlist-based engine ID.
	 */
	u32 rl_engine_id;
	/*
	 * The device's DEV_PRI_BASE is the offset into BAR0 for accessing the
	 * register space for the target device.
	 */
	u32 dev_pri_base;
};

struct nvgpu_gsp_runlist_info {
	/*
	 * Device id to which this runlist belongs to
	 */
	u8 device_id;
	/*
	 * Domain id to which this runlist need to mapped to
	 */
	u8 domain_id;
	/*
	 * Indicates how many runlist entries are in the newly submitted runlist
	 */
	u32 num_entries;
	/*
	 * Indicates how many runlist aperture
	 */
	u32 aperture;
	/*
	 * ID contains the identifier of the runlist.
	 */
	u32 runlist_id;
	/*
	 *NV_RUNLIST_SUBMIT_BASE_L0 in-memory location of runlist.
	 */
	u32 runlist_base_lo;
	/*
	 *NV_RUNLIST_SUBMIT_BASE_Hi in-memory location of runlist.
	 */
	u32 runlist_base_hi;
};

int nvgpu_gsp_send_devices_info(struct gk20a *g);
#endif // NVGPU_GSP_RUNLIST
