/*
 * Copyright (c) 2021 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVS_LOG_H
#define NVS_LOG_H

#include <nvs/types-internal.h>

/*
 * Default log size; 64K entries at 8 bytes each is 512Kb of space. For a space
 * constrained system this is obviously a lot. It can be overridden.
 */
#ifndef NVS_LOG_ENTRIES
#define NVS_LOG_ENTRIES		(64 * 1024)
#endif

/*
 * Fast and efficient logging, even on microcontrollers, is an absolute
 * must for nvsched. The logging provided here is binary encoded to take up
 * a small amount of space and reduce time spent writing the logs.
 *
 * An implementation of nvsched should decode the logs later, when not in
 * a time critical path. The event type can be decoded with nvs_log_event_string().
 */
enum nvs_event {
	NVS_EV_NO_EVENT,
	NVS_EV_CREATE_SCHED,
	NVS_EV_CREATE_DOMAIN,
	NVS_EV_REMOVE_DOMAIN,
	NVS_EV_MAX = 0xffffffff /* Force to 32 bit enum size. */
};

struct nvs_sched;

/**
 * @brief A single log event used to track event type, timestamp, etc. Note this
 * is 8 byte aligned.
 */
struct nvs_log_event {
	u64		timestamp;
	u32		data;
	enum nvs_event	event;
};

/**
 * Simple circular buffer for putting and getting events.
 */
struct nvs_log_buffer {
	struct nvs_log_event	*events;
	u32			 entries;

	u32			 get;
	u32			 put;

	u64			 ts_offset;
};

int  nvs_log_init(struct nvs_sched *sched);
void nvs_log_destroy(struct nvs_sched *sched);
void nvs_log_event(struct nvs_sched *sched, enum nvs_event event, u32 data);
void nvs_log_get(struct nvs_sched *sched, struct nvs_log_event *ev);
const char *nvs_log_event_string(enum nvs_event ev);

#endif
