/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
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

#ifndef PVA_FW_ADDRESS_MAP_H
#define PVA_FW_ADDRESS_MAP_H

/**
 * @brief Starting R5 address where FW code and data is placed.
 * This address is expected to be programmed in PVA_CFG_AR1PRIV_START by KMD.
 * This address is also expected to be used as offset where
 * PVA_CFG_R5PRIV_LSEGREG1 and PVA_CFG_R5PRIV_USEGREG1 registers would point.
 */
#define FW_CODE_DATA_START_ADDR                     1610612736 //0x60000000

/**
 * @brief R5 address where FW code and data is expected to end.
 * This address is expected to be programmed in PVA_CFG_AR1PRIV_END by KMD.
 */
#define FW_CODE_DATA_END_ADDR                       1612840960 //0x60220000

/**
 * @defgroup PVA_EXCEPTION_VECTORS
 *
 * @brief Following macros define R5 addresses that are expected to be
 * programmed by KMD in EVP registers as is.
 * @{
 */
/**
 * @brief R5 address of reset exception vector
 */
#define EVP_RESET_VECTOR                       (1610877952) //0x60040C00
/**
 * @brief R5 address of undefined instruction exception vector
 */
#define EVP_UNDEFINED_INSTRUCTION_VECTOR       (1610878976) //0x60041000
/**
 * @brief R5 address of svc exception vector
 */
#define EVP_SVC_VECTOR                         (1610880000) //0x60041400
/**
 * @brief R5 address of prefetch abort exception vector
 */
#define EVP_PREFETCH_ABORT_VECTOR              (1610881024) //0x60041800
/**
 * @brief R5 address of data abort exception vector
 */
#define EVP_DATA_ABORT_VECTOR                  (1610882048) //0x60041C00
/**
 * @brief R5 address of reserved exception vector.
 * It points to a dummy handler.
 */
#define EVP_RESERVED_VECTOR                    (1610883072) //0x60042000
/**
 * @brief R5 address of IRQ exception vector
 */
#define EVP_IRQ_VECTOR                         (1610884096) //0x60042400
/**
 * @brief R5 address of FIQ exception vector
 */
#define EVP_FIQ_VECTOR                         (1610885120) //0x60042800
/** @} */

/**
 * @defgroup PVA_DEBUG_BUFFERS
 *
 * @brief These buffers are arranged in the following order:
 * TRACE_BUFFER followed by CODE_COVERAGE_BUFFER followed by DEBUG_LOG_BUFFER.
 * @{
 */
/**
 * @brief Maximum size of trace buffer in bytes.
 */
#define FW_TRACE_BUFFER_SIZE                        262144 //0x40000
/**
 * @brief Maximum size of code coverage buffer in bytes.
 */
#define FW_CODE_COVERAGE_BUFFER_SIZE                524288 //0x80000
/**
 * @brief Maximum size of debug log buffer in bytes.
 */
#define FW_DEBUG_LOG_BUFFER_SIZE                    262144 //0x40000
/** @} */

/**
 * @brief Total size of buffers used for FW debug in bytes.
 * TBD: Update this address based on build configuration once KMD changes
 * are merged.
 */
#define FW_DEBUG_DATA_TOTAL_SIZE	(FW_TRACE_BUFFER_SIZE + \
					 FW_DEBUG_LOG_BUFFER_SIZE + \
					 FW_CODE_COVERAGE_BUFFER_SIZE)

/**
 * @brief Starting R5 address where FW debug related data is placed.
 * This address is expected to be programmed in PVA_CFG_AR2PRIV_START by KMD.
 * This address is also expected to be used as offset where
 * PVA_CFG_R5PRIV_LSEGREG2 and PVA_CFG_R5PRIV_USEGREG2 registers would point.
 */
#define FW_DEBUG_DATA_START_ADDR                    1879048192 //0x70000000

/**
 * @brief R5 address where FW debug related data is expected to end.
 * This address is expected to be programmed in PVA_CFG_AR2PRIV_END by KMD.
 */
#define FW_DEBUG_DATA_END_ADDR	(FW_DEBUG_DATA_START_ADDR + \
				 FW_DEBUG_DATA_TOTAL_SIZE)

/**
 * @brief Starting R5 address where FW expects shared buffers between KMD and
 * FW to be placed. This is to be used as offset when programming
 * PVA_CFG_R5USER_LSEGREG and PVA_CFG_R5USER_USEGREG.
 */
#define FW_SHARED_MEMORY_START                 2147483648 //0x80000000

#endif
