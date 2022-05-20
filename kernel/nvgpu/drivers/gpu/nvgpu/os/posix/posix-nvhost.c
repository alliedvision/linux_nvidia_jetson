/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/bug.h>
#include <nvgpu/posix/posix-nvhost.h>

void nvgpu_free_nvhost_dev(struct gk20a *g) {

	if (g->nvhost != NULL) {
		nvgpu_kfree(g, g->nvhost);
		g->nvhost = NULL;
	}
}

bool nvgpu_has_syncpoints(struct gk20a *g)
{
	return nvgpu_is_enabled(g, NVGPU_HAS_SYNCPOINTS);
}

static void allocate_new_syncpt(struct nvgpu_nvhost_dev *nvgpu_syncpt_dev)
{
	u32 syncpt_id, syncpt_val;

	srand(time(NULL));

	/* Limit the range between {1, NUM_HW_PTS} */
	syncpt_id = (rand() % NUM_HW_PTS) + 1;
	/* Limit the range between {1, UINT_MAX - SYNCPT_SAFE_STATE_INCR - 1} */
	syncpt_val = (rand() % (UINT_MAX - SYNCPT_SAFE_STATE_INCR - 1));

	nvgpu_syncpt_dev->syncpt_id = syncpt_id;
	nvgpu_syncpt_dev->syncpt_value = syncpt_val;
}

int nvgpu_get_nvhost_dev(struct gk20a *g)
{
	int ret = 0;
	g->nvhost = nvgpu_kzalloc(g, sizeof(struct nvgpu_nvhost_dev));
	if (g->nvhost == NULL) {
		return -ENOMEM;
	}

	g->nvhost->host1x_sp_base = 0x60000000;
	g->nvhost->host1x_sp_size = 0x4000;
	g->nvhost->nb_hw_pts = 704U;
	ret = nvgpu_nvhost_get_syncpt_aperture(
				g->nvhost, &g->syncpt_unit_base,
				&g->syncpt_unit_size);
	if (ret != 0) {
		nvgpu_err(g, "Failed to get syncpt interface");
		goto fail_nvgpu_get_nvhost_dev;
	}
	g->syncpt_size =
		nvgpu_nvhost_syncpt_unit_interface_get_byte_offset(g, 1);

	return 0;

fail_nvgpu_get_nvhost_dev:
	nvgpu_free_nvhost_dev(g);

	return ret;
}

int nvgpu_nvhost_get_syncpt_aperture(
		struct nvgpu_nvhost_dev *nvgpu_syncpt_dev,
		u64 *base, size_t *size)
{
	if (nvgpu_syncpt_dev == NULL || base == NULL || size == NULL) {
		return -ENOSYS;
	}

	*base = (u64)nvgpu_syncpt_dev->host1x_sp_base;
	*size = nvgpu_syncpt_dev->host1x_sp_size;

	return 0;

}

const char *nvgpu_nvhost_syncpt_get_name(
	struct nvgpu_nvhost_dev *nvgpu_syncpt_dev, int id)
{
	return NULL;
}

u32 nvgpu_nvhost_syncpt_unit_interface_get_byte_offset(struct gk20a *g,
	u32 syncpt_id)
{
	return nvgpu_safe_mult_u32(syncpt_id, 0x1000U);
}

void nvgpu_nvhost_syncpt_set_minval(struct nvgpu_nvhost_dev *nvgpu_syncpt_dev,
	u32 id, u32 val)
{
}

void nvgpu_nvhost_syncpt_put_ref_ext(
	struct nvgpu_nvhost_dev *nvhost_dev, u32 id)
{
	nvhost_dev->syncpt_id = 0U;
	nvhost_dev->syncpt_value = 0U;
}

u32 nvgpu_nvhost_get_syncpt_client_managed(
	struct nvgpu_nvhost_dev *nvhost_dev,
	const char *syncpt_name)
{
	/* Only allocate new syncpt if nothing exists already */
	if (nvhost_dev->syncpt_id == 0U) {
		allocate_new_syncpt(nvhost_dev);
	} else {
		nvhost_dev->syncpt_id = 0U;
	}

	return nvhost_dev->syncpt_id;
}

void nvgpu_nvhost_syncpt_set_safe_state(
	struct nvgpu_nvhost_dev *nvhost_dev, u32 id)
{
	u32 syncpt_value_cur;

	if (nvhost_dev->syncpt_id == id) {
		syncpt_value_cur = nvhost_dev->syncpt_value;
		nvhost_dev->syncpt_value =
			nvgpu_safe_add_u32(syncpt_value_cur,
				SYNCPT_SAFE_STATE_INCR);
	}
}

bool nvgpu_nvhost_syncpt_is_expired_ext(
	struct nvgpu_nvhost_dev *nvhost_dev, u32 id, u32 thresh)
{
	return true;
}

bool nvgpu_nvhost_syncpt_is_valid_pt_ext(
	struct nvgpu_nvhost_dev *nvhost_dev, u32 id)
{
	return true;
}

int nvgpu_nvhost_intr_register_notifier(
	struct nvgpu_nvhost_dev *nvhost_dev, u32 id, u32 thresh,
	void (*callback)(void *, int), void *private_data)
{
	return -ENOSYS;
}

int nvgpu_nvhost_syncpt_read_ext_check(
	struct nvgpu_nvhost_dev *nvhost_dev, u32 id, u32 *val)
{
	return -ENOSYS;
}

int nvgpu_nvhost_syncpt_wait_timeout_ext(
	struct nvgpu_nvhost_dev *nvhost_dev, u32 id,
	u32 thresh, u32 timeout, u32 waiter_index)
{
	return -ENOSYS;
}
