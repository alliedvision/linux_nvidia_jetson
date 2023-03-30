/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION. All rights reserved.
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
 * headers are stored in the parameter_data_iova memory area of
 * parameter_info_base field.
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

/**
 * @brief The structure holds the wrapper information
 * for the VMEM parameters that is provided by the user.
 */
struct PVA_PACKED pva_vpu_parameter_info_s {
	/**
	 * @brief The IOVA address of the parameter data.
	 * This should point to an array of type @ref pva_vpu_parameter_list_t .
	 * If no parameters are present this should be set to 0
	 */
	pva_iova parameter_data_iova;

	/**
	 * @brief The starting IOVA address of the parameter data whose size
	 * is lower than @ref PVA_DMA_VMEM_COPY_THRESHOLD . This data needs to be
	 * memcpied by FW to VMEM and DMA should not be used. If no small
	 * parameters are present this should be set to 0.
	 */
	pva_iova small_vpu_param_data_iova;

	/**
	 * @brief The number of bytes of small VPU parameter data, i.e the
	 * data whose size is lower than @ref PVA_DMA_VMEM_COPY_THRESHOLD . If no small
	 * parameters are present, this should be set to 0
	 */
	uint32_t small_vpu_parameter_data_size;

	/**
	 * @brief The index of the array of type @ref pva_vpu_parameter_list_t from which
	 * the VPU large parameters are present, i.e the vpu parameters whose size is greater
	 * than @ref PVA_DMA_VMEM_COPY_THRESHOLD . This value will always point to the index
	 * immediately after the small parameters. If no large parameter is present, then
	 * this field value will be same as the value of
	 * @ref pva_vpu_parameter_info_t.vpu_instance_parameter_list_start_index field
	 */
	uint32_t large_vpu_parameter_list_start_index;

	/**
	 * @brief The index of the array of type @ref pva_vpu_parameter_list_t from which
	 * the VPU instance parameters are present. This value will always point to the index
	 * immediately after the large parameters if large parameters are present,
	 * else it will be the same value as
	 * @ref pva_vpu_parameter_info_t.large_vpu_parameter_list_start_index field.
	 */
	uint32_t vpu_instance_parameter_list_start_index;
};

/**
 * @brief The minimuim size of the VPU parameter for it to be considered
 * as a large parameter
 */
#define PVA_DMA_VMEM_COPY_THRESHOLD		((uint32_t)(256U))

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
