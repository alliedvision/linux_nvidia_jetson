/*
 * Copyright (c) 2021 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
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
