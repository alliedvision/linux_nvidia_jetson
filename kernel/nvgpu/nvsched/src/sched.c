/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvs/log.h>
#include <nvs/sched.h>
#include <nvs/domain.h>

int nvs_sched_create(struct nvs_sched *sched,
		     struct nvs_sched_ops *ops, void *priv)
{
	int err;

	if (ops == NULL) {
		return -EINVAL;
	}

	nvs_memset(sched, 0, sizeof(*sched));

	sched->ops  = ops;
	sched->priv = priv;

	sched->domain_list = nvs_malloc(sched, sizeof(*sched->domain_list));
	if (sched->domain_list == NULL) {
		return -ENOMEM;
	}

	nvs_memset(sched->domain_list, 0, sizeof(*sched->domain_list));

	err = nvs_log_init(sched);
	if (err != 0) {
		nvs_free(sched, sched->domain_list);
		return err;
	}

	nvs_log_event(sched, NVS_EV_CREATE_SCHED, 0U);

	return 0;
}

void nvs_sched_close(struct nvs_sched *sched)
{
	nvs_domain_clear_all(sched);
	nvs_free(sched, sched->domain_list);
	nvs_log_destroy(sched);

	nvs_memset(sched, 0, sizeof(*sched));
}
