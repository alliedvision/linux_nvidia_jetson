/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/dma-buf.h>

#include <nvgpu/comptags.h>
#include <nvgpu/gk20a.h>

#include <nvgpu/linux/vm.h>

#include "dmabuf_priv.h"

void gk20a_get_comptags(struct nvgpu_os_buffer *buf,
			struct gk20a_comptags *comptags)
{
	struct gk20a_dmabuf_priv *priv =
		gk20a_dma_buf_get_drvdata(buf->dmabuf, buf->dev);

	if (!comptags)
		return;

	if (!priv) {
		(void) memset(comptags, 0, sizeof(*comptags));
		return;
	}

	nvgpu_mutex_acquire(&priv->lock);
	*comptags = priv->comptags;
	nvgpu_mutex_release(&priv->lock);
}

int gk20a_alloc_comptags(struct gk20a *g, struct nvgpu_os_buffer *buf,
			 struct gk20a_comptag_allocator *allocator)
{
	struct gk20a_dmabuf_priv *priv = NULL;
	u64 ctag_granularity;
	u32 offset = 0;
	u32 lines = 0;
	int err;

	ctag_granularity = g->ops.fb.compression_page_size(g);
	lines = DIV_ROUND_UP_ULL(buf->dmabuf->size, ctag_granularity);

	/* 0-sized buffer? Shouldn't occur, but let's check anyways. */
	if (lines < 1) {
		nvgpu_err(g, "zero sized buffer. comptags not allocated.");
		return -EINVAL;
	}

	err = gk20a_comptaglines_alloc(allocator, &offset, lines);
	if (err != 0) {
		/*
		 * Note: we must prevent reallocation attempt in case the
		 * allocation failed. Otherwise a later successful allocation
		 * could cause corruption because interop endpoints have
		 * conflicting compression states with the maps
		 */
		nvgpu_err(g, "Comptags allocation failed %d", err);
		lines = 0;
	}

	priv = gk20a_dma_buf_get_drvdata(buf->dmabuf, buf->dev);

	nvgpu_assert(priv != NULL);

	/* store the allocator so we can use it when we free the ctags */
	priv->comptag_allocator = allocator;

	priv->comptags.offset = offset;
	priv->comptags.lines = lines;
	priv->comptags.needs_clear = (lines != 0);
	priv->comptags.allocated = true;
	priv->comptags.enabled = (lines != 0);

	return err;
}

void gk20a_alloc_or_get_comptags(struct gk20a *g,
				 struct nvgpu_os_buffer *buf,
				 struct gk20a_comptag_allocator *allocator,
				 struct gk20a_comptags *comptags)
{
	struct gk20a_dmabuf_priv *priv = NULL;
	int err;

	if (!comptags)
		return;

	err = gk20a_dmabuf_alloc_or_get_drvdata(buf->dmabuf, buf->dev, &priv);

	if (err != 0) {
		(void) memset(comptags, 0, sizeof(*comptags));
		return;
	}

	nvgpu_mutex_acquire(&priv->lock);

	/*
	 * Try to allocate only if metadata is not locked. However, we
	 * don't re-enable explicitly disabled comptags.
	 */
	if (!priv->registered || priv->mutable_metadata) {
		if (!priv->comptags.allocated) {
			gk20a_alloc_comptags(g, buf, allocator);
		}
	}

	*comptags = priv->comptags;
	nvgpu_mutex_release(&priv->lock);
}

bool gk20a_comptags_start_clear(struct nvgpu_os_buffer *buf)
{
	struct gk20a_dmabuf_priv *priv = gk20a_dma_buf_get_drvdata(buf->dmabuf,
						buf->dev);
	bool clear_started = false;

	if (priv) {
		nvgpu_mutex_acquire(&priv->lock);

		clear_started = priv->comptags.needs_clear;

		if (!clear_started)
			nvgpu_mutex_release(&priv->lock);
	}

	return clear_started;
}

void gk20a_comptags_finish_clear(struct nvgpu_os_buffer *buf,
				 bool clear_successful)
{
	struct gk20a_dmabuf_priv *priv = gk20a_dma_buf_get_drvdata(buf->dmabuf,
						buf->dev);
	if (priv) {
		if (clear_successful)
			priv->comptags.needs_clear = false;

		nvgpu_mutex_release(&priv->lock);
	}
}
