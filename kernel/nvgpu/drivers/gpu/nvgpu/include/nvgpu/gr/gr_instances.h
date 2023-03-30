/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_GR_INSTANCES_H
#define NVGPU_GR_INSTANCES_H

#include <nvgpu/types.h>
#include <nvgpu/grmgr.h>
#include <nvgpu/gr/gr.h>
#include <nvgpu/lock.h>
#include <nvgpu/bug.h>

#ifdef CONFIG_NVGPU_MIG
#define nvgpu_gr_get_cur_instance_id(g) \
	({ \
		u32 current_gr_instance_id = 0U; \
		if (nvgpu_grmgr_is_multi_gr_enabled(g)) { \
			if (nvgpu_mutex_tryacquire(&g->mig.gr_syspipe_lock) == 0) { \
				current_gr_instance_id = g->mig.cur_gr_instance; \
			} else { \
				nvgpu_mutex_release(&g->mig.gr_syspipe_lock); \
			} \
		} \
		current_gr_instance_id; \
	})
#else
#define nvgpu_gr_get_cur_instance_id(g)		(0U)
#endif

#define nvgpu_gr_get_cur_instance_ptr(g) \
	({ \
		u32 current_gr_instance_id = nvgpu_gr_get_cur_instance_id(g); \
		&g->gr[current_gr_instance_id]; \
	})

#define nvgpu_gr_get_instance_ptr(g, gr_instance_id) \
	({ \
		nvgpu_assert(gr_instance_id < g->num_gr_instances); \
		&g->gr[gr_instance_id]; \
	})

#ifdef CONFIG_NVGPU_MIG
#define nvgpu_gr_exec_for_each_instance(g, func) \
	({ \
		if (nvgpu_grmgr_is_multi_gr_enabled(g)) { \
			u32 gr_instance_id = 0U; \
			for (; gr_instance_id < g->num_gr_instances; gr_instance_id++) { \
				u32 gr_syspipe_id = nvgpu_gr_get_syspipe_id(g, gr_instance_id); \
				nvgpu_grmgr_config_gr_remap_window(g, gr_syspipe_id, true); \
				g->mig.cur_gr_instance = gr_instance_id; \
				(func); \
				nvgpu_grmgr_config_gr_remap_window(g, gr_syspipe_id, false); \
			} \
		} else { \
			(func); \
		} \
	})
#else
#define nvgpu_gr_exec_for_each_instance(g, func)	(func)
#endif

#ifdef CONFIG_NVGPU_MIG
#define nvgpu_gr_exec_with_ret_for_each_instance(g, func) \
	({ \
		int err = 0; \
		if (nvgpu_grmgr_is_multi_gr_enabled(g)) { \
			u32 gr_instance_id = 0U; \
			for (; gr_instance_id < g->num_gr_instances; gr_instance_id++) { \
				u32 gr_syspipe_id = nvgpu_gr_get_syspipe_id(g, gr_instance_id); \
				nvgpu_grmgr_config_gr_remap_window(g, gr_syspipe_id, true); \
				g->mig.cur_gr_instance = gr_instance_id; \
				err = (func); \
				nvgpu_grmgr_config_gr_remap_window(g, gr_syspipe_id, false); \
				if (err != 0) { \
					break; \
				} \
			} \
		} else { \
			err = (func); \
		} \
		err; \
	})
#else
#define nvgpu_gr_exec_with_ret_for_each_instance(g, func)	(func)
#endif

#ifdef CONFIG_NVGPU_MIG
#define nvgpu_gr_exec_for_all_instances(g, func) \
	({ \
		if (nvgpu_grmgr_is_multi_gr_enabled(g)) { \
			nvgpu_grmgr_config_gr_remap_window(g, NVGPU_MIG_INVALID_GR_SYSPIPE_ID, false); \
			g->mig.cur_gr_instance = 0; \
			(func); \
			nvgpu_grmgr_config_gr_remap_window(g, NVGPU_MIG_INVALID_GR_SYSPIPE_ID, true); \
		} else { \
			(func); \
		} \
	})
#else
#define nvgpu_gr_exec_for_all_instances(g, func)	(func)
#endif

#ifdef CONFIG_NVGPU_MIG
#define nvgpu_gr_exec_with_ret_for_all_instances(g, func) \
	({ \
		int err = 0; \
		if (nvgpu_grmgr_is_multi_gr_enabled(g)) { \
			nvgpu_grmgr_config_gr_remap_window(g, \
				NVGPU_MIG_INVALID_GR_SYSPIPE_ID, false); \
			g->mig.cur_gr_instance = 0; \
			err = (func); \
			nvgpu_grmgr_config_gr_remap_window(g, \
			 NVGPU_MIG_INVALID_GR_SYSPIPE_ID, true); \
		} else { \
			err = (func); \
		} \
		err; \
	})
#else
#define nvgpu_gr_exec_with_ret_for_all_instances(g, func)	(func)
#endif

#ifdef CONFIG_NVGPU_MIG
#define nvgpu_gr_exec_for_instance(g, gr_instance_id, func) \
	({ \
		if (nvgpu_grmgr_is_multi_gr_enabled(g)) { \
			u32 gr_syspipe_id = nvgpu_gr_get_syspipe_id(g, \
				gr_instance_id); \
			nvgpu_grmgr_config_gr_remap_window(g, gr_syspipe_id, \
				true); \
			g->mig.cur_gr_instance = gr_instance_id; \
			(func); \
			nvgpu_grmgr_config_gr_remap_window(g, gr_syspipe_id, \
				false); \
		} else { \
			(func); \
		} \
	})
#else
#define nvgpu_gr_exec_for_instance(g, gr_instance_id, func) \
	({ \
		nvgpu_assert(gr_instance_id == 0U); \
		(func); \
	})
#endif

#ifdef CONFIG_NVGPU_MIG
#define nvgpu_gr_exec_with_ret_for_instance(g, gr_instance_id, func, type) \
	({ \
		typeof(type) ret; \
		if (nvgpu_grmgr_is_multi_gr_enabled(g)) { \
			u32 gr_syspipe_id = nvgpu_gr_get_syspipe_id(g, gr_instance_id); \
			nvgpu_grmgr_config_gr_remap_window(g, gr_syspipe_id, true); \
			g->mig.cur_gr_instance = gr_instance_id; \
			ret = (func); \
			nvgpu_grmgr_config_gr_remap_window(g, gr_syspipe_id, false); \
		} else { \
			ret = (func); \
		} \
		ret; \
	})
#else
#define nvgpu_gr_exec_with_ret_for_instance(g, gr_instance_id, func, type) \
	({ \
		nvgpu_assert(gr_instance_id == 0U); \
		(func); \
	})
#endif

#ifdef CONFIG_NVGPU_MIG
#define nvgpu_gr_exec_with_err_for_instance(g, gr_instance_id, func) \
	({ \
		int err; \
		err = nvgpu_gr_exec_with_ret_for_instance(g, gr_instance_id, \
				func, err); \
		err; \
	})
#else
#define nvgpu_gr_exec_with_err_for_instance(g, gr_instance_id, func) \
	({ \
		nvgpu_assert(gr_instance_id == 0U); \
		(func); \
	})
#endif

#ifdef CONFIG_NVGPU_MIG
#define nvgpu_gr_get_gpu_instance_config_ptr(g, gpu_instance_id) \
	({ \
		struct nvgpu_gr_config *gr_config = NULL; \
		if (nvgpu_grmgr_is_multi_gr_enabled(g)) { \
			u32 gr_instance_id = nvgpu_grmgr_get_gr_instance_id(g, \
				gpu_instance_id); \
			if (gr_instance_id < g->num_gr_instances) { \
				gr_config = \
					nvgpu_gr_get_gr_instance_config_ptr(g, \
					gr_instance_id); \
			} \
		} else { \
			gr_config = nvgpu_gr_get_config_ptr(g); \
		} \
		gr_config; \
	})
#else
#define nvgpu_gr_get_gpu_instance_config_ptr(g, gpu_instance_id) \
	({ \
		struct nvgpu_gr_config *gr_instance_gr_config; \
		nvgpu_assert(gpu_instance_id == 0U); \
		gr_instance_gr_config = nvgpu_gr_get_config_ptr(g); \
		gr_instance_gr_config; \
	})
#endif

#endif /* NVGPU_GR_INSTANCES_H */
