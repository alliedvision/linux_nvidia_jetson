/* SPDX-License-Identifier: (GPL-2.0 WITH Linux-syscall-note)
 *
 * Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 */

#ifndef _UAPI_TEGRA_FSICOM_H_
#define _UAPI_TEGRA_FSICOM_H_

#include <linux/ioctl.h>

struct rw_data {
	uint32_t handle;
	uint64_t pa;
	uint64_t iova;
	uint64_t dmabuf;
	uint64_t attach;
	uint64_t sgt;
};

/*Data type for sending the offset,IOVA and channel Id details to FSI */
struct iova_data {
	uint32_t offset;
	uint32_t iova;
	uint32_t chid;
};

/* signal value */
#define SIG_DRIVER_RESUME	43
#define SIG_FSI_WRITE_EVENT	44

/* ioctl call macros */
#define NVMAP_SMMU_MAP    _IOWR('q', 1, struct rw_data *)
#define NVMAP_SMMU_UNMAP  _IOWR('q', 2, struct rw_data *)
#define TEGRA_HSP_WRITE   _IOWR('q', 3, struct rw_data *)
#define TEGRA_SIGNAL_REG  _IOWR('q', 4, struct rw_data *)
#define TEGRA_IOVA_DATA   _IOWR('q', 5, struct iova_data *)

#endif	/* _UAPI_TEGRA_FSICOM_H_ */
