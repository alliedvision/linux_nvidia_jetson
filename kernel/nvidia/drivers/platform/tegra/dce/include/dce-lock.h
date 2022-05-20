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

#ifndef DCE_LOCK_H
#define DCE_LOCK_H

struct dce_mutex {
	struct mutex mutex;
};

/**
 * dce_mutex_init - Initialize dce_mutex in Linux style
 *
 * mutex : pointer to the mutext to be initialized.
 *
 * Return : 0 if successful
 */
static inline int dce_mutex_init(struct dce_mutex *mutex)
{
	mutex_init(&mutex->mutex);
	return 0;
};

/**
 * dce_mutex_lock - Acquire the dce_mutex in Linux style
 *
 * mutex : pointer to the mutext to be acquired.
 *
 * Return : void
 */
static inline void dce_mutex_lock(struct dce_mutex *mutex)
{
	mutex_lock(&mutex->mutex);
};

/**
 * dce_mutex_unlock - Release the dce_mutex in Linux style
 *
 * mutex : pointer to the mutext to be released.
 *
 * Return : void
 */
static inline void dce_mutex_unlock(struct dce_mutex *mutex)
{
	mutex_unlock(&mutex->mutex);
};

/**
 * dce_mutex_destroy - Destroy the dce_mutex in Linux style
 *
 * mutex : pointer to the mutext to be destroyed.
 *
 * Return : 0 if successful
 */
static inline void dce_mutex_destroy(struct dce_mutex *mutex)
{
	mutex_destroy(&mutex->mutex);
};

#endif
