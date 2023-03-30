/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_PMU_MUTEX_H
#define NVGPU_PMU_MUTEX_H

#include <nvgpu/types.h>

struct gk20a;
struct nvgpu_pmu;

/* List of valid logical mutex identifiers */
#define	PMU_MUTEX_ID_RSVD1		0U
#define	PMU_MUTEX_ID_GPUSER		1U
#define	PMU_MUTEX_ID_QUEUE_BIOS		2U
#define	PMU_MUTEX_ID_QUEUE_SMI		3U
#define	PMU_MUTEX_ID_GPMUTEX		4U
#define	PMU_MUTEX_ID_I2C		5U
#define	PMU_MUTEX_ID_RMLOCK		6U
#define	PMU_MUTEX_ID_MSGBOX		7U
#define	PMU_MUTEX_ID_FIFO		8U
#define	PMU_MUTEX_ID_PG			9U
#define	PMU_MUTEX_ID_GR			10U
#define	PMU_MUTEX_ID_CLK		11U
#define	PMU_MUTEX_ID_RSVD6		12U
#define	PMU_MUTEX_ID_RSVD7		13U
#define	PMU_MUTEX_ID_RSVD8		14U
#define	PMU_MUTEX_ID_RSVD9		15U
#define	PMU_MUTEX_ID_INVALID		16U

#define PMU_MUTEX_ID_IS_VALID(id)	\
		((id) < PMU_MUTEX_ID_INVALID)

#define PMU_INVALID_MUTEX_OWNER_ID	0U

struct pmu_mutex {
	u32 id;
	u32 index;
	u32 ref_cnt;
};

struct pmu_mutexes {
	struct pmu_mutex *mutex;
	u32 cnt;
};

int nvgpu_pmu_mutex_acquire(struct gk20a *g, struct pmu_mutexes *mutexes,
			u32 id, u32 *token);
int nvgpu_pmu_mutex_release(struct gk20a *g, struct pmu_mutexes *mutexes,
			u32 id, u32 *token);

void nvgpu_pmu_mutex_sw_setup(struct gk20a *g, struct nvgpu_pmu *pmu,
	struct pmu_mutexes *mutexes);
int nvgpu_pmu_init_mutexe(struct gk20a *g, struct nvgpu_pmu *pmu,
	struct pmu_mutexes **mutexes_p);
void nvgpu_pmu_mutexe_deinit(struct gk20a *g, struct nvgpu_pmu *pmu,
	struct pmu_mutexes *mutexes);
#endif /* NVGPU_PMU_MUTEX_H */
