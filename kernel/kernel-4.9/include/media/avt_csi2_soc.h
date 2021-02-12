/*=============================================================================
  Copyright (C) 2020 Allied Vision Technologies.  All Rights Reserved.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
  -----------------------------------------------------------------------------
File:        avt_csi2_soc.h
version:     1.0.0
=============================================================================*/

////////////////////////////////////////////////////////////////////////////////
// DEFINES
////////////////////////////////////////////////////////////////////////////////

#ifndef AVT_CSI2_SOC_H
#define AVT_CSI2_SOC_H

/* D-PHY 1.2 clock frequency range (up to 2.5 Gbps per lane, DDR) */
#define CSI_HOST_CLK_MIN_FREQ	40000000
#define CSI_HOST_CLK_MAX_FREQ	1250000000

/* VI restrictions */
#define FRAMESIZE_MIN_W     64
#define FRAMESIZE_MAX_W     32768
#define FRAMESIZE_INC_W     32
#define FRAMESIZE_MIN_H     32
#define FRAMESIZE_MAX_H     32768
#define FRAMESIZE_INC_H     1

#define OFFSET_INC_W        8       /* Tegra doesn't seem to accept offsets that are not divisible by 8. */
#define OFFSET_INC_H        8

/* Support only 0x31 datatype */
#define DATA_IDENTIFIER_INQ_1   0x0002000000000000ull
#define DATA_IDENTIFIER_INQ_2   0x0
#define DATA_IDENTIFIER_INQ_3   0x0
#define DATA_IDENTIFIER_INQ_4   0x0
#define MIN_ANNOUNCED_FRAMES    3

#endif /* AVT_CSI2_SOC_H */