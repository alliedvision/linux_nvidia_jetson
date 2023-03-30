/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "debug_ce.h"
#include "os_linux.h"

#include <nvgpu/ce_app.h>

#include <common/ce/ce_priv.h>

#include <linux/debugfs.h>

void nvgpu_ce_debugfs_init(struct gk20a *g)
{
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);

	debugfs_create_u32("ce_app_ctx_count", S_IWUSR | S_IRUGO,
			   l->debugfs, &g->ce_app->ctx_count);
	debugfs_create_u32("ce_app_state", S_IWUSR | S_IRUGO,
			   l->debugfs, &g->ce_app->app_state);
	debugfs_create_u32("ce_app_next_ctx_id", S_IWUSR | S_IRUGO,
			   l->debugfs, &g->ce_app->next_ctx_id);
}
