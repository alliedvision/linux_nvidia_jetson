/*
 * Copyright (c) 2016-2020, NVIDIA Corporation.  All rights reserved.
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

#ifndef _NVHOST_SUBMIT_HOST1X_STREAMID_H_
#define _NVHOST_SUBMIT_HOST1X_STREAMID_H_

static void submit_host1xstreamid(struct nvhost_job *job)
{
	int streamid;
	struct platform_device *host_dev = nvhost_get_host(job->ch->dev)->dev;

	/* If VM is defined, use VM specific streamid*/
	if (job->ch->vm) {
		streamid = nvhost_vm_get_id(job->ch->vm);
	} else {
		/* Get host1x streamid */
		streamid = nvhost_vm_get_hwid(host_dev,
					      nvhost_host1x_get_vmid(host_dev));
		if (streamid < 0)
			streamid = nvhost_vm_get_bypass_hwid();
	}

	nvhost_cdma_push(&job->ch->cdma,
			nvhost_opcode_setpayload(streamid),
			nvhost_opcode_setstreamid(
			host1x_channel_ch_strmid_0_offset_base_v() >> 2));
}

#endif /* _NVHOST_SUBMIT_HOST1X_STREAMID_H_ */
