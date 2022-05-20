/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/nvlink.h>
#include <nvgpu/nvlink_link_mode_transitions.h>

#ifdef CONFIG_NVGPU_NVLINK
/*
 * Fix: use this function to find detault link, as only one is supported
 * on the library for now
 * Returns NVLINK_MAX_LINKS_SW on failure
 */
static u32 nvgpu_nvlink_get_link(struct gk20a *g)
{
	u32 link_id;

	if (g == NULL) {
		return NVLINK_MAX_LINKS_SW;
	}

	/* Lets find the detected link */
	if (g->nvlink.initialized_links != 0U) {
		link_id = (u32)(nvgpu_ffs(g->nvlink.initialized_links) - 1UL);
	} else {
		return NVLINK_MAX_LINKS_SW;
	}

	if (g->nvlink.links[link_id].remote_info.is_connected) {
		return link_id;
	}

	return NVLINK_MAX_LINKS_SW;
}

enum nvgpu_nvlink_link_mode nvgpu_nvlink_get_link_mode(struct gk20a *g)
{
	u32 link_id;

	link_id = nvgpu_nvlink_get_link(g);
	if (link_id == NVLINK_MAX_LINKS_SW) {
		return nvgpu_nvlink_link__last;
	}

	return g->ops.nvlink.link_mode_transitions.get_link_mode(g, link_id);
}

u32 nvgpu_nvlink_get_link_state(struct gk20a *g)
{
	u32 link_id;

	link_id = nvgpu_nvlink_get_link(g);
	if (link_id == NVLINK_MAX_LINKS_SW) {
		/* 0xff is an undefined link_state */
		return U32_MAX;
	}

	return g->ops.nvlink.link_mode_transitions.get_link_state(g, link_id);
}

int nvgpu_nvlink_set_link_mode(struct gk20a *g,
					enum nvgpu_nvlink_link_mode mode)
{

	u32 link_id;

	link_id = nvgpu_nvlink_get_link(g);
	if (link_id == NVLINK_MAX_LINKS_SW) {
		return -EINVAL;
	}

	return g->ops.nvlink.link_mode_transitions.set_link_mode(g, link_id,
									mode);
}

void nvgpu_nvlink_get_tx_sublink_state(struct gk20a *g, u32 *state)
{
	u32 link_id;

	link_id = nvgpu_nvlink_get_link(g);
	if (link_id == NVLINK_MAX_LINKS_SW) {
		return;
	}
	if (state != NULL) {
		*state = g->ops.nvlink.link_mode_transitions.
					get_tx_sublink_state(g, link_id);
	}
}

void nvgpu_nvlink_get_rx_sublink_state(struct gk20a *g, u32 *state)
{
	u32 link_id;

	link_id = nvgpu_nvlink_get_link(g);
	if (link_id == NVLINK_MAX_LINKS_SW) {
		return;
	}
	if (state != NULL) {
		*state = g->ops.nvlink.link_mode_transitions.
					get_rx_sublink_state(g, link_id);
	}
}

enum nvgpu_nvlink_sublink_mode nvgpu_nvlink_get_sublink_mode(struct gk20a *g,
							bool is_rx_sublink)
{
	u32 link_id;

	link_id = nvgpu_nvlink_get_link(g);
	if (link_id == NVLINK_MAX_LINKS_SW) {
		return nvgpu_nvlink_sublink_rx__last;
	}

	return g->ops.nvlink.link_mode_transitions.get_sublink_mode(g,
							link_id, is_rx_sublink);
}

int nvgpu_nvlink_set_sublink_mode(struct gk20a *g,
			bool is_rx_sublink, enum nvgpu_nvlink_sublink_mode mode)
{
	u32 link_id;

	link_id = nvgpu_nvlink_get_link(g);
	if (link_id == NVLINK_MAX_LINKS_SW) {
		return -EINVAL;
	}

	return g->ops.nvlink.link_mode_transitions.set_sublink_mode(g, link_id,
							is_rx_sublink, mode);
}

#endif
