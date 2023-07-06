/*
 * Copyright (c) 2018-2023, NVIDIA CORPORATION. All rights reserved.
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

#ifndef INCLUDED_OSI_DMA_TXRX_H
#define INCLUDED_OSI_DMA_TXRX_H

/**
 * @addtogroup EQOS_Help Descriptor Helper MACROS
 *
 * @brief Helper macros for defining Tx/Rx descriptor count
 * @{
 */
#define OSI_EQOS_TX_DESC_CNT		1024U
#define OSI_EQOS_RX_DESC_CNT		1024U
#define OSI_MGBE_TX_DESC_CNT		4096U
#define OSI_MGBE_MAX_RX_DESC_CNT	16384U
/** @} */

/** TSO Header length divisor */
#define OSI_TSO_HDR_LEN_DIVISOR	4U

/**
 * @addtogroup EQOS_Help1 Helper MACROS for descriptor index operations
 *
 * @brief Helper macros for incrementing or decrementing Tx/Rx descriptor index
 * @{
 */
/** Increment the tx descriptor index */
#define INCR_TX_DESC_INDEX(idx, x) ((idx) = ((idx) + (1U)) & ((x) - 1U))
/** Increment the rx descriptor index */
#define INCR_RX_DESC_INDEX(idx, x) ((idx) = ((idx) + (1U)) & ((x) - 1U))
#ifdef OSI_DEBUG
/** Decrement the tx descriptor index */
#define DECR_TX_DESC_INDEX(idx, x) ((idx) = ((idx) - (1U)) & ((x) - 1U))
#endif /* OSI_DEBUG */
#ifndef OSI_STRIPPED_LIB
/** Decrement the rx descriptor index */
#define DECR_RX_DESC_INDEX(idx, x) ((idx) = ((idx) - (1U)) & ((x) - 1U))
#endif /* !OSI_STRIPPED_LIB */
/** @} */
#endif /* INCLUDED_OSI_DMA_TXRX_H */
