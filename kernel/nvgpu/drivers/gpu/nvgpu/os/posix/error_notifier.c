/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/error_notifier.h>
#include <nvgpu/channel.h>

#include <nvgpu/posix/posix-channel.h>

void nvgpu_set_err_notifier_locked(struct nvgpu_channel *ch, u32 error)
{
	struct nvgpu_posix_channel *cp = ch->os_priv;
	if (cp != NULL) {
		cp->err_notifier.error = error;
		cp->err_notifier.status = 0xffff;
	}
}

void nvgpu_set_err_notifier(struct nvgpu_channel *ch, u32 error)
{
	nvgpu_set_err_notifier_locked(ch, error);
}

void nvgpu_set_err_notifier_if_empty(struct nvgpu_channel *ch, u32 error)
{
	struct nvgpu_posix_channel *cp = ch->os_priv;
	if (cp != NULL && cp->err_notifier.status == 0) {
		nvgpu_set_err_notifier_locked(ch, error);
	}
}

bool nvgpu_is_err_notifier_set(struct nvgpu_channel *ch, u32 error_notifier)
{
	struct nvgpu_posix_channel *cp = ch->os_priv;
	if ((cp != NULL) && (cp->err_notifier.status != 0)) {
		return cp->err_notifier.error == error_notifier;
	}
	return false;
}
