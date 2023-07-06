/*
 * Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef MGBE_DESC_H_
#define MGBE_DESC_H_

#ifndef OSI_STRIPPED_LIB
/**
 * @addtogroup MGBE MAC FRP Stats.
 *
 * @brief Values defined for the MGBE Flexible Receive Parser Receive Status
 * @{
 */
#define MGBE_RDES2_FRPSM			OSI_BIT(10)
#define MGBE_RDES3_FRPSL			OSI_BIT(14)
/** @} */
#endif /* !OSI_STRIPPED_LIB */

/**
 * @addtogroup MGBE RDESC bits.
 *
 * @brief Values defined for the MGBE rx descriptor bit fields
 * @{
 */

#define MGBE_RDES3_PT_MASK	(OSI_BIT(20) | OSI_BIT(21) | OSI_BIT(22) | OSI_BIT(23))
#define MGBE_RDES3_PT_IPV4_TCP	OSI_BIT(20)
#define MGBE_RDES3_PT_IPV4_UDP	OSI_BIT(21)
#define MGBE_RDES3_PT_IPV6_TCP	(OSI_BIT(20) | OSI_BIT(23))
#define MGBE_RDES3_PT_IPV6_UDP	(OSI_BIT(21) | OSI_BIT(23))
/** @} */

#endif /* MGBE_DESC_H_ */
