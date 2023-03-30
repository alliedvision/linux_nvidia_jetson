/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/pmu/mutex.h>
#include <nvgpu/gk20a.h>

int nvgpu_pmu_mutex_acquire(struct gk20a *g, struct pmu_mutexes *mutexes,
			    u32 id, u32 *token)
{
	struct pmu_mutex *mutex;
	u32 owner;
	int err;

	WARN_ON(token == NULL);
	WARN_ON(!PMU_MUTEX_ID_IS_VALID(id));
	WARN_ON(id > mutexes->cnt);

	mutex = &mutexes->mutex[id];

	owner = g->ops.pmu.pmu_mutex_owner(g, mutexes, id);

	if (*token != PMU_INVALID_MUTEX_OWNER_ID && *token == owner) {
		WARN_ON(mutex->ref_cnt == 0U);
		nvgpu_err(g, "already acquired by owner : 0x%08x", *token);
		mutex->ref_cnt++;
		return 0;
	}

	err = g->ops.pmu.pmu_mutex_acquire(g, mutexes, id, token);

	if (err == 0) {
		mutex->ref_cnt = 1;
	}

	return err;
}

int nvgpu_pmu_mutex_release(struct gk20a *g, struct pmu_mutexes *mutexes,
			    u32 id, u32 *token)
{
	struct pmu_mutex *mutex;
	u32 owner;

	WARN_ON(token == NULL);
	WARN_ON(!PMU_MUTEX_ID_IS_VALID(id));
	WARN_ON(id > mutexes->cnt);

	mutex = &mutexes->mutex[id];

	owner = g->ops.pmu.pmu_mutex_owner(g, mutexes, id);

	if (*token != owner) {
		nvgpu_err(g, "requester 0x%08x NOT match owner 0x%08x",
			*token, owner);
		return -EINVAL;
	}

	if (--mutex->ref_cnt > 0U) {
		return -EBUSY;
	}

	g->ops.pmu.pmu_mutex_release(g, mutexes, id, token);

	return 0;
}

void nvgpu_pmu_mutex_sw_setup(struct gk20a *g, struct nvgpu_pmu *pmu,
	struct pmu_mutexes *mutexes)
{
	u32 i;

	(void)pmu;

	nvgpu_log_fn(g, " ");

	for (i = 0; i < mutexes->cnt; i++) {
		mutexes->mutex[i].id    = i;
		mutexes->mutex[i].index = i;
	}
}

int nvgpu_pmu_init_mutexe(struct gk20a *g, struct nvgpu_pmu *pmu,
	struct pmu_mutexes **mutexes_p)
{
	struct pmu_mutexes *mutexes;
	int err = 0;

	(void)pmu;

	nvgpu_log_fn(g, " ");

	if (*mutexes_p != NULL) {
		/* skip alloc/reinit for unrailgate sequence */
		nvgpu_pmu_dbg(g, "skip mutex init for unrailgate sequence");
		goto exit;
	}

	mutexes = (struct pmu_mutexes *)
		nvgpu_kzalloc(g, sizeof(struct pmu_mutexes));
	if (mutexes == NULL) {
		err = -ENOMEM;
		goto exit;
	}

	mutexes->cnt = g->ops.pmu.pmu_mutex_size();

	mutexes->mutex = nvgpu_kzalloc(g, mutexes->cnt *
		sizeof(struct pmu_mutex));
	if (mutexes->mutex == NULL) {
		nvgpu_kfree(g, mutexes);
		err = -ENOMEM;
		goto exit;
	}

	*mutexes_p = mutexes;

exit:
	return err;
}

void nvgpu_pmu_mutexe_deinit(struct gk20a *g, struct nvgpu_pmu *pmu,
	struct pmu_mutexes *mutexes)
{
	(void)pmu;

	nvgpu_log_fn(g, " ");

	if (mutexes == NULL) {
		return;
	}

	if (mutexes->mutex != NULL) {
		nvgpu_kfree(g, mutexes->mutex);
	}
	nvgpu_kfree(g, mutexes);
}
