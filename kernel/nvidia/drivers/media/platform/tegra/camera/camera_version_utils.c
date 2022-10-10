/*
 * camera_version_utils.c - utilities for different kernel versions
 * camera driver supports
 *
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
#include <media/camera_common.h>

int tegra_media_entity_init(struct media_entity *entity, u16 num_pads,
			struct media_pad *pad, bool is_subdev, bool is_sensor)
{
	if (!is_subdev) {
		entity->obj_type = MEDIA_ENTITY_TYPE_VIDEO_DEVICE;
		entity->function = MEDIA_ENT_F_IO_V4L;
	} else {
		entity->obj_type = MEDIA_ENTITY_TYPE_V4L2_SUBDEV;
		entity->function = is_sensor ? MEDIA_ENT_F_CAM_SENSOR :
					MEDIA_ENT_F_CAM_HW;
	}
	return media_entity_pads_init(entity, num_pads, pad);
}
EXPORT_SYMBOL(tegra_media_entity_init);

bool tegra_is_v4l2_subdev(struct media_entity *entity)
{
	return is_media_entity_v4l2_subdev(entity);
}
EXPORT_SYMBOL(tegra_is_v4l2_subdev);

int tegra_media_create_link(struct media_entity *source, u16 source_pad,
		struct media_entity *sink, u16 sink_pad, u32 flags)
{
	int ret = 0;

	ret = media_create_pad_link(source, source_pad,
			       sink, sink_pad, flags);
	return ret;
}

bool tegra_v4l2_match_dv_timings(struct v4l2_dv_timings *t1,
				struct v4l2_dv_timings *t2,
				unsigned pclock_delta,
				bool match_reduced_fps)
{
	return v4l2_match_dv_timings(t1, t2, pclock_delta, match_reduced_fps);
}
EXPORT_SYMBOL(tegra_v4l2_match_dv_timings);

int tegra_vb2_dma_init(struct device *dev, void **alloc_ctx,
	unsigned int size, atomic_t *refcount)
{
	int ret = 0;

	if (atomic_inc_return(refcount) > 1)
		return 0;

	if (vb2_dma_contig_set_max_seg_size(dev, SZ_64K)) {
		dev_err(dev, "failed to init vb2 buffer\n");
		ret = -ENOMEM;
	}
	return ret;
}
EXPORT_SYMBOL(tegra_vb2_dma_init);

void tegra_vb2_dma_cleanup(struct device *dev, void *alloc_ctx,
			atomic_t *refcount)
{
	if (atomic_dec_return(refcount) < 0)
		dev_err(dev, "%s: put to negative references\n", __func__);
	/* dont call vb2_dma_contig_clear_max_seg_size as it will */
	/* call kfree dma_parms but dma_parms is static member */
}
EXPORT_SYMBOL(tegra_vb2_dma_cleanup);
