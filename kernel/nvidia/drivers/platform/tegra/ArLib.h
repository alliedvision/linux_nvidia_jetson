/* SPDX-License-Identifier: GPL-2.0-only
 * SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ArLib.h
 *
 *  This module implements and exposes Autosar E2E Profile 5
 *  The E2E Profile 5 provides a consistent set of data protection mechanisms,
 *  designed to protecting against the faults considered in the fault model.
 */

#ifndef ARLIB_H_
#define ARLIB_H_

/* ==================[Includes]============================================== */
#include <linux/types.h>
/* ====================[Macros]============================================== */
#define E2E_E_OK                0x00U
#define E2E_E_INPUTERR_WRONG    0x17U
#define E2E_E_INPUTERR_NULL     0x13U

/*==================[type definitions]========================================*/

/**
 * Status of the reception on one single Data in one cycle,
 * protected with E2E Profile 5.
 */
enum E2E_P05CheckStatusType {
/**
 * OK: the checks of the Data in this cycle were successful
 * (including counter check, which was incremented by 1).
 */
	E2E_P05STATUS_OK = 0x00U,
/**
 * Error: the Check function has been invoked but no new Data is
 * not available since the last call.
 * As a result, no E2E checks of Data have been consequently executed.
 * This may be considered similar to E2E_P05STATUS_REPEATED.
 */
	E2E_P05STATUS_NONEWDATA = 0x01U,
/**
 * Error: error not related to counters occurred (e.g. wrong crc,
 * wrong length, wrong options, wrong Data ID).
 */
	E2E_P05STATUS_ERROR = 0x07U,
/**
 * Error: the checks of the Data in this cycle were successful,
 * with the exception of the repetition.
 */
	E2E_P05STATUS_REPEATED = 0x08U,
/**
 * OK: the checks of the Data in this cycle were successful
 * (including counter check, which was incremented within
 * the allowed configured delta).
 */
	E2E_P05STATUS_OKSOMELOST = 0x20U,
/**
 * Error: the checks of the Data in this cycle were successful,
 * with the exception of counter jump, which changed more than
 * the allowed delta.
 */
	E2E_P05STATUS_WRONGSEQUENCE = 0x40U
};

/**
 * Configuration of transmitted Data (Data Element), for E2E Profile 5.
 * For each transmitted Data, there is an instance of this typedef.
 */
struct E2E_P05ConfigType {

/**
 * Bit offset of the first bit of the E2E header from the beginning
 * of the Data (bit numbering: bit 0 is the least important).
 * The offset shall be a multiple of 8 and 0 ≤ Offset ≤ DataLength-(3*8).
 * Example: If Offset equals 8, then the low byte of the E2E Crc (16 bit) i
 * is written to Byte 1,
 * the high Byte is written to Byte 2.
 */
	uint16_t Offset;

/**
 * Length of Data, in bits. The value shall be = 4096*8 (4kB)
 * and shall be ≥ 3*8
 */
	uint16_t DataLength;

/**
 * A system-unique identifier of the Data.
 */
	uint16_t DataID;

/**
 * Maximum allowed gap between two counter values of two consecutively
 * received valid Data. For example, if the receiver gets Data with counter
 * 1 and MaxDeltaCounter is 3,
 * then at the next reception the receiver can accept Counters with values
 * 2, 3 or 4.
 */
	uint8_t MaxDeltaCounter;
	uint8_t Reserved;	/* Padding byte */
};

/**
 * State of the sender for a Data protected with E2E Profile 5.
 */
struct E2E_P05ProtectStateType {

  /**
   * Counter to be used for protecting the next Data. The initial value is 0,
   * which means that in the first cycle, Counter is 0.
   * Each time E2E_P05Protect() is called, it increments the counter up to 0xFF.
   */
	uint8_t Counter;
};

/**
 * State of the reception on one single Data protected with E2E Profile 5.
 */
struct  E2E_P05CheckStateType {

  /**
   * Result of the verification of the Data in this cycle,
   * determined by the Check function.
   */
	enum E2E_P05CheckStatusType Status;

  /**
   * Counter of the data in previous cycle.
   */
	uint8_t Counter;
	uint8_t Reserved[3];	/* Padding bytes */
};

/**
 *E2E function return type
 */
#define E2E_returntype_t uint32_t

/* ==================[Global function declarations]=========================== */

/** @brief Protects the array/buffer to be transmitted using the E2E profile 5.
 *         This includes checksum calculation, handling of counter.
 *
 *  @param ConfigPtr Pointer to static configuration.
 *  @param StatePtr Pointer to port/data communication state.
 *  @param DataPtr Pointer to Data to be transmitted.
 *  @param Length of the data in bytes.
 *
 *  @return Error code (E2E_E_INPUTERR_NULL || E2E_E_INPUTERR_WRONG || E2E_E_OK).
 *
 *
 */
E2E_returntype_t E2E_P05Protect(const struct E2E_P05ConfigType *ConfigPtr,
				struct E2E_P05ProtectStateType *StatePtr,
				uint8_t *DataPtr, uint16_t Length);

/** @brief Checks the Data received using the E2E profile 5.
 *         This includes CRC calculation, handling of Counter.
 *         The function checks only one single data in one cycle,
 *         it does not determine/compute the accumulated state of
 *         the communication link.
 *
 *  @param ConfigPtr Pointer to static configuration.
 *  @param StatePtr Pointer to port/data communication state.
 *  @param DataPtr Pointer to received data.
 *  @param Length of the data in bytes.
 *
 *  @return Error code (E2E_E_INPUTERR_NULL ||
 *                      E2E_E_INPUTERR_WRONG || E2E_E_OK).
 */
E2E_returntype_t E2E_P05Check(const struct E2E_P05ConfigType *ConfigPtr,
			      struct E2E_P05CheckStateType *StatePtr,
			      const uint8_t *DataPtr, uint16_t Length);

/** @brief Initializes the protection state.
 *
 *  @param StatePtr Pointer to port/data communication state.
 *
 *  @return Error code (E2E_E_INPUTERR_NULL - null pointer passed E2E_E_OK).
 */
E2E_returntype_t E2E_P05ProtectInit(struct E2E_P05ProtectStateType *StatePtr);

/** @brief Initializes the check state.
 *
 *  @param StatePtr Pointer to port/data communication state.
 *
 *  @return Error code (E2E_E_INPUTERR_NULL - null pointer passed E2E_E_OK).
 */
E2E_returntype_t E2E_P05CheckInit(struct E2E_P05CheckStateType *StatePtr);

#endif				/* ARLIB_H_ */
