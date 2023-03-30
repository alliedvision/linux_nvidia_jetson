/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
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
/*
 * Function/Macro naming determines intended use:
 *
 *     <x>_r(void) : Returns the offset for register <x>.
 *
 *     <x>_o(void) : Returns the offset for element <x>.
 *
 *     <x>_w(void) : Returns the word offset for word (4 byte) element <x>.
 *
 *     <x>_<y>_s(void) : Returns size of field <y> of register <x> in bits.
 *
 *     <x>_<y>_f(u32 v) : Returns a value based on 'v' which has been shifted
 *         and masked to place it at field <y> of register <x>.  This value
 *         can be |'d with others to produce a full register value for
 *         register <x>.
 *
 *     <x>_<y>_m(void) : Returns a mask for field <y> of register <x>.  This
 *         value can be ~'d and then &'d to clear the value of field <y> for
 *         register <x>.
 *
 *     <x>_<y>_<z>_f(void) : Returns the constant value <z> after being shifted
 *         to place it at field <y> of register <x>.  This value can be |'d
 *         with others to produce a full register value for <x>.
 *
 *     <x>_<y>_v(u32 r) : Returns the value of field <y> from a full register
 *         <x> value 'r' after being shifted to place its LSB at bit 0.
 *         This value is suitable for direct comparison with other unshifted
 *         values appropriate for use in field <y> of register <x>.
 *
 *     <x>_<y>_<z>_v(void) : Returns the constant value for <z> defined for
 *         field <y> of register <x>.  This value is suitable for direct
 *         comparison with unshifted values appropriate for use in field <y>
 *         of register <x>.
 */
#ifndef NVGPU_HW_IOCTRLMIF_TU104_H
#define NVGPU_HW_IOCTRLMIF_TU104_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define ioctrlmif_rx_err_contain_en_0_r()                          (0x00000e0cU)
#define ioctrlmif_rx_err_contain_en_0_rxramdataparityerr_f(v)\
				((U32(v) & 0x1U) << 3U)
#define ioctrlmif_rx_err_contain_en_0_rxramdataparityerr_m()   (U32(0x1U) << 3U)
#define ioctrlmif_rx_err_contain_en_0_rxramdataparityerr_v(r)\
				(((r) >> 3U) & 0x1U)
#define ioctrlmif_rx_err_contain_en_0_rxramhdrparityerr_f(v)\
				((U32(v) & 0x1U) << 4U)
#define ioctrlmif_rx_err_contain_en_0_rxramhdrparityerr_m()    (U32(0x1U) << 4U)
#define ioctrlmif_rx_err_contain_en_0_rxramhdrparityerr_v(r)\
				(((r) >> 4U) & 0x1U)
#define ioctrlmif_rx_err_contain_en_0_rxramhdrparityerr__prod_v()  (0x00000001U)
#define ioctrlmif_rx_err_contain_en_0_rxramhdrparityerr__prod_f()        (0x10U)
#define ioctrlmif_rx_err_log_en_0_r()                              (0x00000e04U)
#define ioctrlmif_rx_err_log_en_0_rxramdataparityerr_f(v)\
				((U32(v) & 0x1U) << 3U)
#define ioctrlmif_rx_err_log_en_0_rxramdataparityerr_m()       (U32(0x1U) << 3U)
#define ioctrlmif_rx_err_log_en_0_rxramdataparityerr_v(r)   (((r) >> 3U) & 0x1U)
#define ioctrlmif_rx_err_log_en_0_rxramhdrparityerr_f(v) ((U32(v) & 0x1U) << 4U)
#define ioctrlmif_rx_err_log_en_0_rxramhdrparityerr_m()        (U32(0x1U) << 4U)
#define ioctrlmif_rx_err_log_en_0_rxramhdrparityerr_v(r)    (((r) >> 4U) & 0x1U)
#define ioctrlmif_rx_err_report_en_0_r()                           (0x00000e08U)
#define ioctrlmif_rx_err_report_en_0_rxramdataparityerr_f(v)\
				((U32(v) & 0x1U) << 3U)
#define ioctrlmif_rx_err_report_en_0_rxramdataparityerr_m()    (U32(0x1U) << 3U)
#define ioctrlmif_rx_err_report_en_0_rxramdataparityerr_v(r)\
				(((r) >> 3U) & 0x1U)
#define ioctrlmif_rx_err_report_en_0_rxramhdrparityerr_f(v)\
				((U32(v) & 0x1U) << 4U)
#define ioctrlmif_rx_err_report_en_0_rxramhdrparityerr_m()     (U32(0x1U) << 4U)
#define ioctrlmif_rx_err_report_en_0_rxramhdrparityerr_v(r) (((r) >> 4U) & 0x1U)
#define ioctrlmif_rx_err_status_0_r()                              (0x00000e00U)
#define ioctrlmif_rx_err_status_0_rxramdataparityerr_f(v)\
				((U32(v) & 0x1U) << 3U)
#define ioctrlmif_rx_err_status_0_rxramdataparityerr_m()       (U32(0x1U) << 3U)
#define ioctrlmif_rx_err_status_0_rxramdataparityerr_v(r)   (((r) >> 3U) & 0x1U)
#define ioctrlmif_rx_err_status_0_rxramhdrparityerr_f(v) ((U32(v) & 0x1U) << 4U)
#define ioctrlmif_rx_err_status_0_rxramhdrparityerr_m()        (U32(0x1U) << 4U)
#define ioctrlmif_rx_err_status_0_rxramhdrparityerr_v(r)    (((r) >> 4U) & 0x1U)
#define ioctrlmif_rx_err_first_0_r()                               (0x00000e14U)
#define ioctrlmif_tx_err_contain_en_0_r()                          (0x00000a90U)
#define ioctrlmif_tx_err_contain_en_0_txramdataparityerr_f(v)\
				((U32(v) & 0x1U) << 0U)
#define ioctrlmif_tx_err_contain_en_0_txramdataparityerr_m()   (U32(0x1U) << 0U)
#define ioctrlmif_tx_err_contain_en_0_txramdataparityerr_v(r)\
				(((r) >> 0U) & 0x1U)
#define ioctrlmif_tx_err_contain_en_0_txramdataparityerr__prod_v() (0x00000001U)
#define ioctrlmif_tx_err_contain_en_0_txramdataparityerr__prod_f()        (0x1U)
#define ioctrlmif_tx_err_contain_en_0_txramhdrparityerr_f(v)\
				((U32(v) & 0x1U) << 1U)
#define ioctrlmif_tx_err_contain_en_0_txramhdrparityerr_m()    (U32(0x1U) << 1U)
#define ioctrlmif_tx_err_contain_en_0_txramhdrparityerr_v(r)\
				(((r) >> 1U) & 0x1U)
#define ioctrlmif_tx_err_contain_en_0_txramhdrparityerr__prod_v()  (0x00000001U)
#define ioctrlmif_tx_err_contain_en_0_txramhdrparityerr__prod_f()         (0x2U)
#define ioctrlmif_tx_err_log_en_0_r()                              (0x00000a88U)
#define ioctrlmif_tx_err_log_en_0_txramdataparityerr_f(v)\
				((U32(v) & 0x1U) << 0U)
#define ioctrlmif_tx_err_log_en_0_txramdataparityerr_m()       (U32(0x1U) << 0U)
#define ioctrlmif_tx_err_log_en_0_txramdataparityerr_v(r)   (((r) >> 0U) & 0x1U)
#define ioctrlmif_tx_err_log_en_0_txramhdrparityerr_f(v) ((U32(v) & 0x1U) << 1U)
#define ioctrlmif_tx_err_log_en_0_txramhdrparityerr_m()        (U32(0x1U) << 1U)
#define ioctrlmif_tx_err_log_en_0_txramhdrparityerr_v(r)    (((r) >> 1U) & 0x1U)
#define ioctrlmif_tx_err_report_en_0_r()                           (0x00000a8cU)
#define ioctrlmif_tx_err_report_en_0_txramdataparityerr_f(v)\
				((U32(v) & 0x1U) << 0U)
#define ioctrlmif_tx_err_report_en_0_txramdataparityerr_m()    (U32(0x1U) << 0U)
#define ioctrlmif_tx_err_report_en_0_txramdataparityerr_v(r)\
				(((r) >> 0U) & 0x1U)
#define ioctrlmif_tx_err_report_en_0_txramhdrparityerr_f(v)\
				((U32(v) & 0x1U) << 1U)
#define ioctrlmif_tx_err_report_en_0_txramhdrparityerr_m()     (U32(0x1U) << 1U)
#define ioctrlmif_tx_err_report_en_0_txramhdrparityerr_v(r) (((r) >> 1U) & 0x1U)
#define ioctrlmif_tx_err_status_0_r()                              (0x00000a84U)
#define ioctrlmif_tx_err_status_0_txramdataparityerr_f(v)\
				((U32(v) & 0x1U) << 0U)
#define ioctrlmif_tx_err_status_0_txramdataparityerr_m()       (U32(0x1U) << 0U)
#define ioctrlmif_tx_err_status_0_txramdataparityerr_v(r)   (((r) >> 0U) & 0x1U)
#define ioctrlmif_tx_err_status_0_txramhdrparityerr_f(v) ((U32(v) & 0x1U) << 1U)
#define ioctrlmif_tx_err_status_0_txramhdrparityerr_m()        (U32(0x1U) << 1U)
#define ioctrlmif_tx_err_status_0_txramhdrparityerr_v(r)    (((r) >> 1U) & 0x1U)
#define ioctrlmif_tx_err_first_0_r()                               (0x00000a98U)
#define ioctrlmif_tx_ctrl_buffer_ready_r()                         (0x00000a7cU)
#define ioctrlmif_rx_ctrl_buffer_ready_r()                         (0x00000dfcU)
#define ioctrlmif_tx_err_misc_0_r()                                (0x00000a9cU)
#define ioctrlmif_tx_err_misc_0_txramdataparitypois_f(v) ((U32(v) & 0x1U) << 0U)
#define ioctrlmif_tx_err_misc_0_txramdataparitypois_m()        (U32(0x1U) << 0U)
#define ioctrlmif_tx_err_misc_0_txramdataparitypois_v(r)    (((r) >> 0U) & 0x1U)
#endif
