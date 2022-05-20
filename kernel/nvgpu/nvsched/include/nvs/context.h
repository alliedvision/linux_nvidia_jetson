/*
 * Copyright (c) 2021 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVS_CONTEXT_H
#define NVS_CONTEXT_H

#include <nvs/types-internal.h>

struct nvs_context;

/**
 * Similar to a nvs_domain_list this is a singly linked list of contexts.
 * If sub-scheduler algorithms ever want something more sophisticated they'll
 * likely have the build it themselves.
 */
struct nvs_context_list {
	u32			 nr;
	struct nvs_context	*contexts;
};

#endif
