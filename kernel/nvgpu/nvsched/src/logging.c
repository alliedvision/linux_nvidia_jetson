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
#include <nvs/impl-internal.h>

#define LOG_INC(value, max)			\
	do {					\
		(value) += 1U;			\
		if ((value) >= (max)) {		\
			(value) = 0U;		\
		}				\
	} while (false)

static bool nvs_log_full(struct nvs_log_buffer *logger)
{
	u32 updated_put = logger->put;

	LOG_INC(updated_put, logger->entries);

	/*
	 * If the next put is the same as get, then put has caught up to get,
	 * and the log is therefore full.
	 */
	return updated_put == logger->get;
}

int nvs_log_init(struct nvs_sched *sched)
{
	struct nvs_log_buffer *logger;

	logger = nvs_malloc(sched, sizeof(*logger));
	if (logger == NULL) {
		return -ENOMEM;
	}

	nvs_memset(logger, 0, sizeof(*logger));

	logger->ts_offset = nvs_timestamp();
	logger->entries = NVS_LOG_ENTRIES;
	logger->events = nvs_malloc(sched,
				    NVS_LOG_ENTRIES * sizeof(*logger->events));
	if (logger->events == NULL) {
		nvs_free(sched, logger);
		return -ENOMEM;
	}

	nvs_memset(logger->events, 0,
		   NVS_LOG_ENTRIES * sizeof(*logger->events));

	sched->log = logger;

	return 0;
}

void nvs_log_destroy(struct nvs_sched *sched)
{
	nvs_free(sched, sched->log->events);
	nvs_free(sched, sched->log);
	sched->log = NULL;
}

void nvs_log_event(struct nvs_sched *sched, enum nvs_event event, u32 data)
{
	struct nvs_log_buffer *logger = sched->log;
	struct nvs_log_event *ev;

	nvs_log(sched, "ev: %d", event);
	nvs_log(sched, "  Starting: G=%05u P=%05u", logger->get, logger->put);

	/*
	 * If the log fills, just consume the oldest entry like with nvs_log_get().
	 *
	 * TODO: insert a "log overrun" entry, too, so readers will know.
	 */
	if (nvs_log_full(logger)) {
		nvs_log(sched, "Log full; killing entry.");
		LOG_INC(logger->get, logger->entries);
	}

	ev = &logger->events[logger->put];
	ev->data  = data;
	ev->event = event;
	ev->timestamp = nvs_timestamp() - logger->ts_offset;

	LOG_INC(logger->put, logger->entries);
	nvs_log(sched, "  New:      G=%05u P=%05u", logger->get, logger->put);
}

void nvs_log_get(struct nvs_sched *sched, struct nvs_log_event *ev)
{
	struct nvs_log_buffer *logger = sched->log;

	nvs_log(sched, "Getting log event.");
	nvs_log(sched, "  Starting: G=%05u P=%05u", logger->get, logger->put);

	/*
	 * Check if the log is empty; if so, clear *ev to signal that.
	 */
	if (logger->get == logger->put) {
		ev->event = NVS_EV_NO_EVENT;
		nvs_log(sched, "  Log empty!");
		return;
	}

	*ev = logger->events[logger->get];
	LOG_INC(logger->get, logger->entries);

	nvs_log(sched, "  New:      G=%05u P=%05u", logger->get, logger->put);
}

const char *nvs_log_event_string(enum nvs_event ev)
{
	switch (ev) {
	case NVS_EV_NO_EVENT:      return "No event";
	case NVS_EV_CREATE_SCHED:  return "Create scheduler";
	case NVS_EV_CREATE_DOMAIN: return "Create domain";
	case NVS_EV_REMOVE_DOMAIN: return "Remove domain";
	case NVS_EV_MAX:           return "Invalid MAX event";
	}

	return "Undefined event";
}
