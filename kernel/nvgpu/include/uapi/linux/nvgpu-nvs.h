/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _UAPI__LINUX_NVGPU_NVS_H
#define _UAPI__LINUX_NVGPU_NVS_H

#include "nvgpu-uapi-common.h"

#define NVGPU_NVS_IOCTL_MAGIC 'N'

/**
 * Domain parameters to pass to the kernel.
 */
struct nvgpu_nvs_ioctl_domain {
	/*
	 * Human readable name for this domain.
	 */
	char		 name[32];

	/*
	 * Scheduling parameters: specify how long this domain should be scheduled
	 * for and what the grace period the scheduler should give this domain when
	 * preempting. A value of zero is treated as an infinite timeslice or an
	 * infinite grace period, respectively.
	 */
	__u32		 timeslice_us;
	__u32		 preempt_grace_us;

	/*
	 * Pick which subscheduler to use. These will be implemented by the kernel
	 * as needed. There'll always be at least one, which is the host HW built in
	 * round-robin scheduler.
	 */
	__u32		 subscheduler;

/*
 * GPU host hardware round-robin.
 */
#define NVGPU_SCHED_IOCTL_SUBSCHEDULER_HOST_HW_RR 0x0

	/*
	 * Populated by the IOCTL when created: unique identifier. User space
	 * should never set this variable.
	 */
	__u64		 dom_id;

	__u64		 reserved1;
	__u64		 reserved2;
};

/**
 * NVGPU_NVS_IOCTL_CREATE_DOMAIN
 *
 * Create a domain - essentially a group of GPU contexts. Applications
 * cacan be bound into this domain on request for each TSG.
 *
 * The domain ID is returned in dom_id; this id is _not_ secure. The
 * nvsched device needs to have restricted permissions such that only a
 * single user, or group of users, has permissions to modify the
 * scheduler.
 *
 * It's fine to allow read-only access to the device node for other
 * users; this lets other users query scheduling information that may be
 * of interest to them.
 */
struct nvgpu_nvs_ioctl_create_domain {
	/*
	 * In/out: domain parameters that userspace configures.
	 *
	 * The domain ID is returned here.
	 */
	struct nvgpu_nvs_ioctl_domain domain_params;

	__u64		reserved1;
};

struct nvgpu_nvs_ioctl_remove_domain {
	/*
	 * In: a domain_id to remove.
	 */
	__u64		dom_id;

	__u64		reserved1;
};

/**
 * NVGPU_NVS_IOCTL_QUERY_DOMAINS
 *
 * Query the current list of domains in the scheduler. This is a two
 * part IOCTL.
 *
 * If domains is NULL, then this IOCTL will populate nr with the number
 * of present domains.
 *
 * If domains is not NULL, then this IOCTL will treat domains as an
 * array with nr elements and write up to nr domains into that array.
 */
struct nvgpu_nvs_ioctl_query_domains {
	/*
	 * In/Out: If NULL, leave untouched. If not NULL, then write
	 * up to nr domains into the domain elements pointed to by
	 * domains.
	 */
	__u64 domains;

	/*
	 * In/Out: If domains is NULL, then populate with the number
	 * of domains present. Otherwise nr specifies the capacity of
	 * the domains array pointed to by domains.
	 */
	__u32		nr;
};

#define NVGPU_NVS_IOCTL_CREATE_DOMAIN			\
	_IOWR(NVGPU_NVS_IOCTL_MAGIC, 1,			\
	      struct nvgpu_nvs_ioctl_create_domain)
#define NVGPU_NVS_IOCTL_REMOVE_DOMAIN			\
	_IOW(NVGPU_NVS_IOCTL_MAGIC, 2,			\
	      struct nvgpu_nvs_ioctl_remove_domain)
#define NVGPU_NVS_IOCTL_QUERY_DOMAINS			\
	_IOWR(NVGPU_NVS_IOCTL_MAGIC, 3,			\
	      struct nvgpu_nvs_ioctl_query_domains)


#define NVGPU_NVS_IOCTL_LAST				\
	_IOC_NR(NVGPU_NVS_IOCTL_QUERY_DOMAINS)
#define NVGPU_NVS_IOCTL_MAX_ARG_SIZE			\
	sizeof(struct nvgpu_nvs_ioctl_create_domain)

#endif
