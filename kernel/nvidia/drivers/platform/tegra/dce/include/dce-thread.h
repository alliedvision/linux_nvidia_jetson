/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef DCE_THREAD_H
#define DCE_THREAD_H

#include <linux/sched.h>

struct task_struct;

struct dce_thread {
	struct task_struct *task;
	bool running;
	int (*fn)(void *);
	void *data;
};

struct dce_thread;

int dce_thread_create(struct dce_thread *thread,
		void *data,
		int (*threadfn)(void *data), const char *name);

void dce_thread_stop(struct dce_thread *thread);

bool dce_thread_should_stop(struct dce_thread *thread);

bool dce_thread_is_running(struct dce_thread *thread);

void dce_thread_join(struct dce_thread *thread);

#endif
