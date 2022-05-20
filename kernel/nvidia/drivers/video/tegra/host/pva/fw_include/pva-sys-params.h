/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file pva-sys-params.h
 *
 * @brief Types and constants related to VPU application parameters.
 */

#ifndef PVA_SYS_PARAMS_H
#define PVA_SYS_PARAMS_H

#include <pva-types.h>
#include <pva-packed.h>

/** VPU parameter header.
 *
 * The VPU App parameters contains kernel-user-provided data to be
 * copied into the VMEM before executing the VPU app. The parameter
 * headers are stored in the parameter_base memory area.
 *
 * The FW can also initialize complex datatypes, which are marked by
 * special param_base outside the normal IOVA space. See the structure
 * struct pva_vpu_instance_data_s for an example.
 */
struct PVA_PACKED pva_vpu_parameters_s {
	pva_iova param_base; /**< I/O address of the parameter data */
	uint32_t addr; /**< Target address (VMEM offset) */
	uint32_t size; /**< Size of the parameter data in bytes */
};

/** Prefix for special param_base markers */
#define PVA_COMPLEX_IOVA (0xDA7AULL << 48ULL)
/** Versioned param_base marker */
#define PVA_COMPLEX_IOVA_V(v) (PVA_COMPLEX_IOVA | ((uint64_t)(v) << 32ULL))

/** Marker for struct pva_vpu_instance_data_s */
#define PVA_SYS_INSTANCE_DATA_V1_IOVA (PVA_COMPLEX_IOVA_V(1) | 0x00000001ULL)

/** ELF symbol for struct pva_vpu_instance_data_s */
#define PVA_SYS_INSTANCE_DATA_V1_SYMBOL "_sys_instance_data_v1"

/** FW-provided instance data */
struct PVA_PACKED pva_vpu_instance_data_s {
	uint32_t vpu_id;
	uint32_t vmem_base;
	uint32_t dma_descriptor_base;
	uint32_t l2ram_base;
	uint32_t l2ram_size;
};

#endif /* PVA_SYS_PARAMS_H */
