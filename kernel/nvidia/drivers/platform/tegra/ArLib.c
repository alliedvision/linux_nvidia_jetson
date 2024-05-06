// SPDX-License-Identifier: GPL-2.0
/* SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
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
 * @file ArLib.c
 * @brief <b>E2E profile 5 source file</b>
 *
 * Runnable definitions for E2E profile 5 functions
 */

/* ==================[Includes]=============================================== */
#include "ArLib.h"

/* ==================[Macros]=============================================== */

#define MAX_P05_COUNTER_VALUE           (255U)
#define MAX_P05_DATA_LENGTH_IN_BYTES    (4096U)
#define MAX_P05_DATA_LENGTH_IN_BITS     (32768U)
#define MIN_P05_DATA_LENGTH_IN_BITS     (24U)
#define MIN_P05_DATA_LENGTH_IN_BYTES    (3U)

#define ARLIB_CRC16_START_VALUE ((uint16_t)(0xFFFFU))
#define ARLIB_CRC16_XOR_VALUE   ((uint16_t)(0x0000U))

#define UINT8MAX        (0xFFU)
#define UINT16MAX       (0xFFFFU)

#define E2EP05_CRC_WIDTH_IN_BYTES       2U
/*==================[static variables]========================================*/

/*==================[local function definitions]==============================*/

static inline uint16_t
lMulU16Cert(uint16_t factor1, uint16_t factor2, uint16_t failValue)
{
	uint16_t product = failValue;

	if (factor2 != 0x00U) {
		if (factor1 <= ((uint16_t) (UINT16MAX) / (factor2)))
			product = ((factor1) * (factor2));
	} else {
		product = 0x00U;
	}

	return (uint16_t) (product);
}

static inline uint16_t
lSubU16Cert(uint16_t minuend, uint16_t subtrahend, uint16_t failValue)
{
	uint16_t difference = failValue;

	if (minuend >= subtrahend)
		difference = ((minuend) - (subtrahend));

	return (uint16_t) (difference);
}

static uint16_t
CRC_CalculateCRC16(const uint8_t *Crc_DataPtr,
		   uint32_t Crc_Length,
		   uint16_t Crc_StartValue16, bool Crc_IsFirstCall)
{
	uint16_t lCrc;
	uint16_t lLookUpVal;
	uint32_t lIndex;
    /** lookup table forCrc16 */
	static const uint16_t sLookupTableCrc16[UINT8MAX + 1U] = {
		0x0, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
		0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad,
		0xe1ce, 0xf1ef, 0x1231, 0x210, 0x3273, 0x2252, 0x52b5, 0x4294,
		0x72f7, 0x62d6, 0x9339, 0x8318, 0xb37b,
		0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de, 0x2462, 0x3443, 0x420,
		0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
		0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
		0x3653, 0x2672, 0x1611, 0x630, 0x76d7,
		0x66f6, 0x5695, 0x46b4, 0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df,
		0xe7fe, 0xd79d, 0xc7bc, 0x48c4, 0x58e5,
		0x6886, 0x78a7, 0x840, 0x1861, 0x2802, 0x3823, 0xc9cc, 0xd9ed,
		0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a,
		0xb92b, 0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0xa50, 0x3a33,
		0x2a12, 0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e,
		0x9b79, 0x8b58, 0xbb3b, 0xab1a, 0x6ca6, 0x7c87, 0x4ce4, 0x5cc5,
		0x2c22, 0x3c03, 0xc60, 0x1c41, 0xedae,
		0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49, 0x7e97,
		0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32,
		0x1e51, 0xe70, 0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a,
		0x9f59, 0x8f78, 0x9188, 0x81a9, 0xb1ca,
		0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f, 0x1080, 0xa1, 0x30c2,
		0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
		0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
		0x2b1, 0x1290, 0x22f3, 0x32d2, 0x4235,
		0x5214, 0x6277, 0x7256, 0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e,
		0xe54f, 0xd52c, 0xc50d, 0x34e2, 0x24c3,
		0x14a0, 0x481, 0x7466, 0x6447, 0x5424, 0x4405, 0xa7db, 0xb7fa,
		0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d,
		0xd73c, 0x26d3, 0x36f2, 0x691, 0x16b0, 0x6657, 0x7676, 0x4615,
		0x5634, 0xd94c, 0xc96d, 0xf90e, 0xe92f,
		0x99c8, 0x89e9, 0xb98a, 0xa9ab, 0x5844, 0x4865, 0x7806, 0x6827,
		0x18c0, 0x8e1, 0x3882, 0x28a3, 0xcb7d,
		0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a, 0x4a75,
		0x5a54, 0x6a37, 0x7a16, 0xaf1, 0x1ad0,
		0x2ab3, 0x3a92, 0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b,
		0x9de8, 0x8dc9, 0x7c26, 0x6c07, 0x5c64,
		0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0xcc1, 0xef1f, 0xff3e, 0xcf5d,
		0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
		0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0xed1, 0x1ef0
	};

	if (true == Crc_IsFirstCall) {
		lCrc = ARLIB_CRC16_START_VALUE;
	} else {
		lCrc =
		    ((Crc_StartValue16 ^ ARLIB_CRC16_XOR_VALUE) & (uint16_t)
		     UINT16MAX);
	}

	for (lIndex = 0U; lIndex < Crc_Length; lIndex++) {
		lLookUpVal =
		    sLookupTableCrc16[Crc_DataPtr[lIndex] ^ (lCrc >> 8U)];
		lCrc = ((lLookUpVal ^ (lCrc << 8u)) & (uint16_t) UINT16MAX);
	}
	lCrc ^= ARLIB_CRC16_XOR_VALUE;

	return lCrc;
}

static uint16_t
lCalculateCRC(const uint8_t *DataPtr,
	      uint16_t length, uint16_t offset, uint16_t dataId)
{
	uint16_t lLengthBytes;
	uint16_t lCrc = 0u;
	uint8_t lDataIdNibble;
	uint16_t lOffset2;

	/* all necessary checks are already perfromed in calling function
	 * checks added here for CERT-C compliance with ocverity
	 */

	if (offset <= (MAX_P05_DATA_LENGTH_IN_BYTES - 2U))
		lOffset2 = offset + (uint16_t) E2EP05_CRC_WIDTH_IN_BYTES;
	else
		lOffset2 = E2EP05_CRC_WIDTH_IN_BYTES;

	lLengthBytes = lSubU16Cert(length, lOffset2, UINT16MAX);

	if (offset > 0U) {
		/* compute CRC over bytes that are before CRC */
		lCrc =
		    CRC_CalculateCRC16(&DataPtr[0U], offset,
				       ARLIB_CRC16_START_VALUE, true);

		/* Compute CRC over bytes that are after CRC (if any) */
		lCrc =
		    CRC_CalculateCRC16(&DataPtr[lOffset2], lLengthBytes, lCrc,
				       false);
	} else
		/* Compute CRC over bytes that are after CRC (if any) */
		lCrc =
		    CRC_CalculateCRC16(&DataPtr[lOffset2], lLengthBytes,
				       ARLIB_CRC16_START_VALUE, true);

	lDataIdNibble = (uint8_t) ((dataId) & (uint16_t) 0x00FFU);
	lCrc = CRC_CalculateCRC16(&lDataIdNibble, 1U, lCrc, false);

	lDataIdNibble =
	    (uint8_t) ((dataId >> (uint8_t) 8U) & (uint16_t) 0x00FFU);
	lCrc = CRC_CalculateCRC16(&lDataIdNibble, 1U, lCrc, false);
	return lCrc;
}

static inline void
lDoChecksP05(struct E2E_P05CheckStateType *StatePtr,
	     const bool *NewDataAvailable,
	     const struct E2E_P05ConfigType *ConfigPtr,
	     uint16_t receivedCRC,
	     uint16_t computedCRC, uint8_t receivedCounter)
{
	uint8_t lDeltaCounter;

	if (false == *NewDataAvailable)
		StatePtr->Status = E2E_P05STATUS_NONEWDATA;
	else if (receivedCRC != computedCRC)
		StatePtr->Status = E2E_P05STATUS_ERROR;
	else {
		lDeltaCounter =
		    (uint8_t) ((receivedCounter -
				StatePtr->Counter) & (uint8_t) UINT8MAX);

		if (lDeltaCounter > ConfigPtr->MaxDeltaCounter)
			StatePtr->Status = E2E_P05STATUS_WRONGSEQUENCE;
		else if (lDeltaCounter == 0U)
			StatePtr->Status = E2E_P05STATUS_REPEATED;
		else if (lDeltaCounter == 1U)
			StatePtr->Status = E2E_P05STATUS_OK;
		else
			StatePtr->Status = E2E_P05STATUS_OKSOMELOST;

		StatePtr->Counter = receivedCounter;
	}
}

static inline E2E_returntype_t
lNullPtrCheck(const struct E2E_P05ConfigType *ConfigPtr,
	      struct E2E_P05CheckStateType *StatePtr,
	      const uint8_t *DataPtr, uint16_t Length)
{
	E2E_returntype_t lRet = E2E_E_OK;

	if ((ConfigPtr == NULL) || (StatePtr == NULL))
		lRet = E2E_E_INPUTERR_NULL;

	else if (!(((DataPtr != NULL) && (Length != 0U)) ||
		   ((DataPtr == NULL) && (Length == 0U))))
		lRet = E2E_E_INPUTERR_WRONG;

	else if (DataPtr == NULL)
		lRet = E2E_E_OK;

	return lRet;
}

static inline E2E_returntype_t
lVerifyCheckInput(bool *NewDataAvailable,
		  const struct E2E_P05ConfigType *ConfigPtr, uint16_t Length)
{
	E2E_returntype_t lRet = E2E_E_OK;
	uint16_t lLengthInBits = 0U;
	*NewDataAvailable = false;

	lLengthInBits = lMulU16Cert(8U, Length, 0U);

	if ((ConfigPtr->DataLength < MIN_P05_DATA_LENGTH_IN_BITS) ||
	    (ConfigPtr->DataLength > MAX_P05_DATA_LENGTH_IN_BITS) ||
	    (lLengthInBits != ConfigPtr->DataLength))
		lRet = E2E_E_INPUTERR_WRONG;

	/* The offset shall be a multiple of 8 and 0 ≤ Offset ≤ MaxDataLength-(3*8). */
	else if (((ConfigPtr->Offset % 8U) != 0U) ||
		 (ConfigPtr->Offset >
		  (MAX_P05_DATA_LENGTH_IN_BITS - MIN_P05_DATA_LENGTH_IN_BITS))
		 || (ConfigPtr->Offset >
		     lSubU16Cert(lLengthInBits, MIN_P05_DATA_LENGTH_IN_BITS,
				 UINT16MAX)))
		lRet = E2E_E_INPUTERR_WRONG;

	else
		*NewDataAvailable = true;

	return lRet;
}

static inline E2E_returntype_t
lVerifyProtectInput(const struct E2E_P05ConfigType *ConfigPtr,
		    struct E2E_P05ProtectStateType *StatePtr,
		    uint8_t *DataPtr, uint16_t Length)
{
	E2E_returntype_t lRet = E2E_E_OK;
	uint16_t lLengthInBits = 0U;

	lLengthInBits = lMulU16Cert(8U, Length, 0U);

	/* Check for NULL pointers */
	if ((ConfigPtr == NULL) || (StatePtr == NULL) || (DataPtr == NULL))
		lRet = E2E_E_INPUTERR_NULL;

	else if ((ConfigPtr->DataLength < MIN_P05_DATA_LENGTH_IN_BITS) ||
		 (ConfigPtr->DataLength > MAX_P05_DATA_LENGTH_IN_BITS) ||
		 (Length != (ConfigPtr->DataLength / 8u)))
		lRet = E2E_E_INPUTERR_WRONG;

	/* The offset shall be a multiple of 8 and 0 ≤ Offset ≤ MaxDataLength-(3*8). */
	else if (((ConfigPtr->Offset % 8U) != 0U) ||
		 (ConfigPtr->Offset >
		  (MAX_P05_DATA_LENGTH_IN_BITS - MIN_P05_DATA_LENGTH_IN_BITS))
		 || (ConfigPtr->Offset >
		     lSubU16Cert(lLengthInBits, MIN_P05_DATA_LENGTH_IN_BITS,
				 UINT16MAX)))
		lRet = E2E_E_INPUTERR_WRONG;

	return lRet;
}

/*==================[API function definitions]================================*/

/* Initializes the protection state.
 */
E2E_returntype_t
E2E_P05ProtectInit(struct E2E_P05ProtectStateType *StatePtr)
{
	E2E_returntype_t lRet;

	lRet = E2E_E_OK;

	if (StatePtr == NULL)
		lRet = E2E_E_INPUTERR_NULL;
	else
		StatePtr->Counter = 0U;

	return lRet;
}

/* Initializes the check state.
 */
E2E_returntype_t
E2E_P05CheckInit(struct E2E_P05CheckStateType *StatePtr)
{
	E2E_returntype_t lRet;

	lRet = E2E_E_OK;

	if (StatePtr == NULL)
		lRet = E2E_E_INPUTERR_NULL;
	else {
		StatePtr->Status = E2E_P05STATUS_ERROR;
		StatePtr->Counter = UINT8MAX;
	}

	return lRet;
}

/* Protects the array/buffer to be transmitted using the E2E profile 5.
 * This includes checksum calculation, handling of counter.
 */
E2E_returntype_t
E2E_P05Protect(const struct E2E_P05ConfigType *ConfigPtr,
	       struct E2E_P05ProtectStateType *StatePtr,
	       uint8_t *DataPtr, uint16_t Length)
{
	uint16_t lOffset;
	uint16_t lCrc;
	E2E_returntype_t lRet = E2E_E_OK;

	/* Verify input argument */
	lRet = lVerifyProtectInput(ConfigPtr, StatePtr, DataPtr, Length);

	if (lRet == E2E_E_OK) {
		/* offset of E2E header in data packet */
		lOffset = (ConfigPtr->Offset) / 8U;

		/* Updating counter value in E2E header */
		DataPtr[lOffset + 2U] = (StatePtr->Counter & UINT8MAX);

		/* Computing Crc value */
		lCrc =
		    lCalculateCRC(DataPtr, Length, lOffset, ConfigPtr->DataID);

		/* Updating Crc value in E2E header */
		DataPtr[lOffset] = (uint8_t) (lCrc & UINT8MAX);
		DataPtr[lOffset + 1U] =
		    (uint8_t) ((lCrc >> 8U) & (uint16_t) UINT8MAX);

		/* Incrementing the counter value */
		if (StatePtr->Counter == MAX_P05_COUNTER_VALUE)
			StatePtr->Counter = 0U;
		else
			StatePtr->Counter++;
	}

	return lRet;
}

/* Checks the Data received using the E2E profile 5. This includes CRC calculation, handling of Counter.
 * The function checks only one single data in one cycle, it does not determine/compute the accumulated
 * state of the communication link.
 * MISRA RULE 8.13 violation:  Consider adding "const" qualifier to the
 *                             pointer "Data" as destination object
 *                             does not appear to be modified within
 *                             this function.
 * EXCEPTION : This is modified in a future function and cannot be made as a constant.
 */
E2E_returntype_t
E2E_P05Check(const struct E2E_P05ConfigType *ConfigPtr,
	     struct E2E_P05CheckStateType *StatePtr,
	     const uint8_t *DataPtr, uint16_t Length)
{
	bool lNewDataAvailable;
	uint16_t lOffset;
	uint8_t lReceivedCounter = 0U;
	uint16_t lReceivedCRC = 0U;
	uint16_t lComputedCRC = 0U;
	E2E_returntype_t lRet = E2E_E_OK;

	/* Check pointer validity */
	lRet = lNullPtrCheck(ConfigPtr, StatePtr, DataPtr, Length);

	if (lRet == E2E_E_OK) {
		/* Check input argument validity */
		lRet = lVerifyCheckInput(&lNewDataAvailable, ConfigPtr, Length);

		if (lRet == E2E_E_OK) {
			if (lNewDataAvailable == true) {
				/* offset of E2E header in data packet */
				lOffset = (ConfigPtr->Offset) / 8U;

				lReceivedCounter =
				    (DataPtr[lOffset + 2U] & UINT8MAX);

				lReceivedCRC |=
				    (((uint16_t) DataPtr[lOffset]) & 0x00FFU);
				lReceivedCRC |=
				    (((uint16_t) DataPtr[lOffset + 1U] << 8U) &
				     (uint16_t) 0xFF00U);

				lComputedCRC =
				    lCalculateCRC(DataPtr, Length, lOffset,
						  ConfigPtr->DataID);
			}

			lDoChecksP05(StatePtr,
				     &lNewDataAvailable,
				     ConfigPtr,
				     lReceivedCRC,
				     lComputedCRC, lReceivedCounter);
		}
	}

	return lRet;
}

/*==================[end of file]=============================================*/
