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
#ifndef NVGPU_HW_NVTLC_TU104_H
#define NVGPU_HW_NVTLC_TU104_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define nvtlc_tx_err_status_0_r()                                  (0x00000700U)
#define nvtlc_rx_err_status_0_r()                                  (0x00000f00U)
#define nvtlc_rx_err_status_1_r()                                  (0x00000f18U)
#define nvtlc_tx_err_first_0_r()                                   (0x00000714U)
#define nvtlc_rx_err_first_0_r()                                   (0x00000f14U)
#define nvtlc_rx_err_first_1_r()                                   (0x00000f2cU)
#define nvtlc_tx_err_report_en_0_r()                               (0x00000708U)
#define nvtlc_tx_err_report_en_0_txhdrcreditovferr_f(v) ((U32(v) & 0xffU) << 0U)
#define nvtlc_tx_err_report_en_0_txhdrcreditovferr__prod_f()             (0xffU)
#define nvtlc_tx_err_report_en_0_txdatacreditovferr_f(v)\
				((U32(v) & 0xffU) << 8U)
#define nvtlc_tx_err_report_en_0_txdatacreditovferr__prod_f()          (0xff00U)
#define nvtlc_tx_err_report_en_0_txdlcreditovferr_f(v)  ((U32(v) & 0x1U) << 16U)
#define nvtlc_tx_err_report_en_0_txdlcreditovferr__prod_f()           (0x10000U)
#define nvtlc_tx_err_report_en_0_txdlcreditparityerr_f(v)\
				((U32(v) & 0x1U) << 17U)
#define nvtlc_tx_err_report_en_0_txdlcreditparityerr__prod_f()        (0x20000U)
#define nvtlc_tx_err_report_en_0_txramhdrparityerr_f(v) ((U32(v) & 0x1U) << 18U)
#define nvtlc_tx_err_report_en_0_txramhdrparityerr__prod_f()          (0x40000U)
#define nvtlc_tx_err_report_en_0_txramdataparityerr_f(v)\
				((U32(v) & 0x1U) << 19U)
#define nvtlc_tx_err_report_en_0_txramdataparityerr__prod_f()         (0x80000U)
#define nvtlc_tx_err_report_en_0_txunsupvcovferr_f(v)   ((U32(v) & 0x1U) << 20U)
#define nvtlc_tx_err_report_en_0_txunsupvcovferr__prod_f()           (0x100000U)
#define nvtlc_tx_err_report_en_0_txstompdet_f(v)        ((U32(v) & 0x1U) << 22U)
#define nvtlc_tx_err_report_en_0_txstompdet__prod_f()                (0x400000U)
#define nvtlc_tx_err_report_en_0_txpoisondet_f(v)       ((U32(v) & 0x1U) << 23U)
#define nvtlc_tx_err_report_en_0_txpoisondet__prod_f()               (0x800000U)
#define nvtlc_tx_err_report_en_0_targeterr_f(v)         ((U32(v) & 0x1U) << 24U)
#define nvtlc_tx_err_report_en_0_targeterr__prod_f()                (0x1000000U)
#define nvtlc_tx_err_report_en_0_unsupportedrequesterr_f(v)\
				((U32(v) & 0x1U) << 25U)
#define nvtlc_tx_err_report_en_0_unsupportedrequesterr__prod_f()    (0x2000000U)
#define nvtlc_tx_err_log_en_0_r()                                  (0x00000704U)
#define nvtlc_tx_err_log_en_0_txhdrcreditovferr_f(v)    ((U32(v) & 0xffU) << 0U)
#define nvtlc_tx_err_log_en_0_txhdrcreditovferr__prod_f()                (0xffU)
#define nvtlc_tx_err_log_en_0_txdatacreditovferr_f(v)   ((U32(v) & 0xffU) << 8U)
#define nvtlc_tx_err_log_en_0_txdatacreditovferr__prod_f()             (0xff00U)
#define nvtlc_tx_err_log_en_0_txdlcreditovferr_f(v)     ((U32(v) & 0x1U) << 16U)
#define nvtlc_tx_err_log_en_0_txdlcreditovferr__prod_f()              (0x10000U)
#define nvtlc_tx_err_log_en_0_txdlcreditparityerr_f(v)  ((U32(v) & 0x1U) << 17U)
#define nvtlc_tx_err_log_en_0_txdlcreditparityerr__prod_f()           (0x20000U)
#define nvtlc_tx_err_log_en_0_txramhdrparityerr_f(v)    ((U32(v) & 0x1U) << 18U)
#define nvtlc_tx_err_log_en_0_txramhdrparityerr__prod_f()             (0x40000U)
#define nvtlc_tx_err_log_en_0_txramdataparityerr_f(v)   ((U32(v) & 0x1U) << 19U)
#define nvtlc_tx_err_log_en_0_txramdataparityerr__prod_f()            (0x80000U)
#define nvtlc_tx_err_log_en_0_txunsupvcovferr_f(v)      ((U32(v) & 0x1U) << 20U)
#define nvtlc_tx_err_log_en_0_txunsupvcovferr__prod_f()              (0x100000U)
#define nvtlc_tx_err_log_en_0_txstompdet_f(v)           ((U32(v) & 0x1U) << 22U)
#define nvtlc_tx_err_log_en_0_txstompdet__prod_f()                   (0x400000U)
#define nvtlc_tx_err_log_en_0_txpoisondet_f(v)          ((U32(v) & 0x1U) << 23U)
#define nvtlc_tx_err_log_en_0_txpoisondet__prod_f()                  (0x800000U)
#define nvtlc_tx_err_log_en_0_targeterr_f(v)            ((U32(v) & 0x1U) << 24U)
#define nvtlc_tx_err_log_en_0_targeterr__prod_f()                   (0x1000000U)
#define nvtlc_tx_err_log_en_0_unsupportedrequesterr_f(v)\
				((U32(v) & 0x1U) << 25U)
#define nvtlc_tx_err_log_en_0_unsupportedrequesterr__prod_f()       (0x2000000U)
#define nvtlc_tx_err_contain_en_0_r()                              (0x0000070cU)
#define nvtlc_tx_err_contain_en_0_txhdrcreditovferr_f(v)\
				((U32(v) & 0xffU) << 0U)
#define nvtlc_tx_err_contain_en_0_txhdrcreditovferr__prod_f()            (0xffU)
#define nvtlc_tx_err_contain_en_0_txdatacreditovferr_f(v)\
				((U32(v) & 0xffU) << 8U)
#define nvtlc_tx_err_contain_en_0_txdatacreditovferr__prod_f()         (0xff00U)
#define nvtlc_tx_err_contain_en_0_txdlcreditovferr_f(v) ((U32(v) & 0x1U) << 16U)
#define nvtlc_tx_err_contain_en_0_txdlcreditovferr__prod_f()          (0x10000U)
#define nvtlc_tx_err_contain_en_0_txdlcreditparityerr_f(v)\
				((U32(v) & 0x1U) << 17U)
#define nvtlc_tx_err_contain_en_0_txdlcreditparityerr__prod_f()       (0x20000U)
#define nvtlc_tx_err_contain_en_0_txramhdrparityerr_f(v)\
				((U32(v) & 0x1U) << 18U)
#define nvtlc_tx_err_contain_en_0_txramhdrparityerr__prod_f()         (0x40000U)
#define nvtlc_tx_err_contain_en_0_txunsupvcovferr_f(v)  ((U32(v) & 0x1U) << 20U)
#define nvtlc_tx_err_contain_en_0_txunsupvcovferr__prod_f()          (0x100000U)
#define nvtlc_tx_err_contain_en_0_txstompdet_f(v)       ((U32(v) & 0x1U) << 22U)
#define nvtlc_tx_err_contain_en_0_txstompdet__prod_f()               (0x400000U)
#define nvtlc_tx_err_contain_en_0_txpoisondet_f(v)      ((U32(v) & 0x1U) << 23U)
#define nvtlc_tx_err_contain_en_0_targeterr_f(v)        ((U32(v) & 0x1U) << 24U)
#define nvtlc_tx_err_contain_en_0_unsupportedrequesterr_f(v)\
				((U32(v) & 0x1U) << 25U)
#define nvtlc_rx_err_report_en_0_r()                               (0x00000f08U)
#define nvtlc_rx_err_report_en_0_rxdlhdrparityerr_f(v)   ((U32(v) & 0x1U) << 0U)
#define nvtlc_rx_err_report_en_0_rxdlhdrparityerr__prod_f()               (0x1U)
#define nvtlc_rx_err_report_en_0_rxdldataparityerr_f(v)  ((U32(v) & 0x1U) << 1U)
#define nvtlc_rx_err_report_en_0_rxdldataparityerr__prod_f()              (0x2U)
#define nvtlc_rx_err_report_en_0_rxdlctrlparityerr_f(v)  ((U32(v) & 0x1U) << 2U)
#define nvtlc_rx_err_report_en_0_rxdlctrlparityerr__prod_f()              (0x4U)
#define nvtlc_rx_err_report_en_0_rxramdataparityerr_f(v) ((U32(v) & 0x1U) << 3U)
#define nvtlc_rx_err_report_en_0_rxramdataparityerr__prod_f()             (0x8U)
#define nvtlc_rx_err_report_en_0_rxramhdrparityerr_f(v)  ((U32(v) & 0x1U) << 4U)
#define nvtlc_rx_err_report_en_0_rxramhdrparityerr__prod_f()             (0x10U)
#define nvtlc_rx_err_report_en_0_rxinvalidaeerr_f(v)     ((U32(v) & 0x1U) << 5U)
#define nvtlc_rx_err_report_en_0_rxinvalidaeerr__prod_f()                (0x20U)
#define nvtlc_rx_err_report_en_0_rxinvalidbeerr_f(v)     ((U32(v) & 0x1U) << 6U)
#define nvtlc_rx_err_report_en_0_rxinvalidbeerr__prod_f()                (0x40U)
#define nvtlc_rx_err_report_en_0_rxinvalidaddralignerr_f(v)\
				((U32(v) & 0x1U) << 7U)
#define nvtlc_rx_err_report_en_0_rxinvalidaddralignerr__prod_f()         (0x80U)
#define nvtlc_rx_err_report_en_0_rxpktlenerr_f(v)        ((U32(v) & 0x1U) << 8U)
#define nvtlc_rx_err_report_en_0_rxpktlenerr__prod_f()                  (0x100U)
#define nvtlc_rx_err_report_en_0_datlengtatomicreqmaxerr_f(v)\
				((U32(v) & 0x1U) << 17U)
#define nvtlc_rx_err_report_en_0_datlengtatomicreqmaxerr__prod_f()    (0x20000U)
#define nvtlc_rx_err_report_en_0_datlengtrmwreqmaxerr_f(v)\
				((U32(v) & 0x1U) << 18U)
#define nvtlc_rx_err_report_en_0_datlengtrmwreqmaxerr__prod_f()       (0x40000U)
#define nvtlc_rx_err_report_en_0_datlenltatrrspminerr_f(v)\
				((U32(v) & 0x1U) << 19U)
#define nvtlc_rx_err_report_en_0_datlenltatrrspminerr__prod_f()       (0x80000U)
#define nvtlc_rx_err_report_en_0_invalidcacheattrpoerr_f(v)\
				((U32(v) & 0x1U) << 20U)
#define nvtlc_rx_err_report_en_0_invalidcacheattrpoerr__prod_f()     (0x100000U)
#define nvtlc_rx_err_report_en_0_invalidcrerr_f(v)      ((U32(v) & 0x1U) << 21U)
#define nvtlc_rx_err_report_en_0_invalidcrerr__prod_f()              (0x200000U)
#define nvtlc_rx_err_report_en_0_rxrespstatustargeterr_f(v)\
				((U32(v) & 0x1U) << 22U)
#define nvtlc_rx_err_report_en_0_rxrespstatustargeterr__prod_f()     (0x400000U)
#define nvtlc_rx_err_report_en_0_rxrespstatusunsupportedrequesterr_f(v)\
				((U32(v) & 0x1U) << 23U)
#define nvtlc_rx_err_report_en_0_rxrespstatusunsupportedrequesterr__prod_f()\
				(0x800000U)
#define nvtlc_rx_err_log_en_0_r()                                  (0x00000f04U)
#define nvtlc_rx_err_log_en_0_rxdlhdrparityerr_f(v)      ((U32(v) & 0x1U) << 0U)
#define nvtlc_rx_err_log_en_0_rxdlhdrparityerr__prod_f()                  (0x1U)
#define nvtlc_rx_err_log_en_0_rxdldataparityerr_f(v)     ((U32(v) & 0x1U) << 1U)
#define nvtlc_rx_err_log_en_0_rxdldataparityerr__prod_f()                 (0x2U)
#define nvtlc_rx_err_log_en_0_rxdlctrlparityerr_f(v)     ((U32(v) & 0x1U) << 2U)
#define nvtlc_rx_err_log_en_0_rxdlctrlparityerr__prod_f()                 (0x4U)
#define nvtlc_rx_err_log_en_0_rxramdataparityerr_f(v)    ((U32(v) & 0x1U) << 3U)
#define nvtlc_rx_err_log_en_0_rxramdataparityerr__prod_f()                (0x8U)
#define nvtlc_rx_err_log_en_0_rxramhdrparityerr_f(v)     ((U32(v) & 0x1U) << 4U)
#define nvtlc_rx_err_log_en_0_rxramhdrparityerr__prod_f()                (0x10U)
#define nvtlc_rx_err_log_en_0_rxinvalidaeerr_f(v)        ((U32(v) & 0x1U) << 5U)
#define nvtlc_rx_err_log_en_0_rxinvalidaeerr__prod_f()                   (0x20U)
#define nvtlc_rx_err_log_en_0_rxinvalidbeerr_f(v)        ((U32(v) & 0x1U) << 6U)
#define nvtlc_rx_err_log_en_0_rxinvalidbeerr__prod_f()                   (0x40U)
#define nvtlc_rx_err_log_en_0_rxinvalidaddralignerr_f(v) ((U32(v) & 0x1U) << 7U)
#define nvtlc_rx_err_log_en_0_rxinvalidaddralignerr__prod_f()            (0x80U)
#define nvtlc_rx_err_log_en_0_rxpktlenerr_f(v)           ((U32(v) & 0x1U) << 8U)
#define nvtlc_rx_err_log_en_0_rxpktlenerr__prod_f()                     (0x100U)
#define nvtlc_rx_err_log_en_0_datlengtatomicreqmaxerr_f(v)\
				((U32(v) & 0x1U) << 17U)
#define nvtlc_rx_err_log_en_0_datlengtatomicreqmaxerr__prod_f()       (0x20000U)
#define nvtlc_rx_err_log_en_0_datlengtrmwreqmaxerr_f(v) ((U32(v) & 0x1U) << 18U)
#define nvtlc_rx_err_log_en_0_datlengtrmwreqmaxerr__prod_f()          (0x40000U)
#define nvtlc_rx_err_log_en_0_datlenltatrrspminerr_f(v) ((U32(v) & 0x1U) << 19U)
#define nvtlc_rx_err_log_en_0_datlenltatrrspminerr__prod_f()          (0x80000U)
#define nvtlc_rx_err_log_en_0_invalidcacheattrpoerr_f(v)\
				((U32(v) & 0x1U) << 20U)
#define nvtlc_rx_err_log_en_0_invalidcacheattrpoerr__prod_f()        (0x100000U)
#define nvtlc_rx_err_log_en_0_invalidcrerr_f(v)         ((U32(v) & 0x1U) << 21U)
#define nvtlc_rx_err_log_en_0_invalidcrerr__prod_f()                 (0x200000U)
#define nvtlc_rx_err_log_en_0_rxrespstatustargeterr_f(v)\
				((U32(v) & 0x1U) << 22U)
#define nvtlc_rx_err_log_en_0_rxrespstatustargeterr__prod_f()        (0x400000U)
#define nvtlc_rx_err_log_en_0_rxrespstatusunsupportedrequesterr_f(v)\
				((U32(v) & 0x1U) << 23U)
#define nvtlc_rx_err_log_en_0_rxrespstatusunsupportedrequesterr__prod_f()\
				(0x800000U)
#define nvtlc_rx_err_contain_en_0_r()                              (0x00000f0cU)
#define nvtlc_rx_err_contain_en_0_rxdlhdrparityerr_f(v)  ((U32(v) & 0x1U) << 0U)
#define nvtlc_rx_err_contain_en_0_rxdlhdrparityerr__prod_f()              (0x1U)
#define nvtlc_rx_err_contain_en_0_rxdldataparityerr_f(v) ((U32(v) & 0x1U) << 1U)
#define nvtlc_rx_err_contain_en_0_rxdldataparityerr__prod_f()             (0x2U)
#define nvtlc_rx_err_contain_en_0_rxdlctrlparityerr_f(v) ((U32(v) & 0x1U) << 2U)
#define nvtlc_rx_err_contain_en_0_rxdlctrlparityerr__prod_f()             (0x4U)
#define nvtlc_rx_err_contain_en_0_rxramdataparityerr_f(v)\
				((U32(v) & 0x1U) << 3U)
#define nvtlc_rx_err_contain_en_0_rxramhdrparityerr_f(v) ((U32(v) & 0x1U) << 4U)
#define nvtlc_rx_err_contain_en_0_rxramhdrparityerr__prod_f()            (0x10U)
#define nvtlc_rx_err_contain_en_0_rxinvalidaeerr_f(v)    ((U32(v) & 0x1U) << 5U)
#define nvtlc_rx_err_contain_en_0_rxinvalidaeerr__prod_f()               (0x20U)
#define nvtlc_rx_err_contain_en_0_rxinvalidbeerr_f(v)    ((U32(v) & 0x1U) << 6U)
#define nvtlc_rx_err_contain_en_0_rxinvalidbeerr__prod_f()               (0x40U)
#define nvtlc_rx_err_contain_en_0_rxinvalidaddralignerr_f(v)\
				((U32(v) & 0x1U) << 7U)
#define nvtlc_rx_err_contain_en_0_rxinvalidaddralignerr__prod_f()        (0x80U)
#define nvtlc_rx_err_contain_en_0_rxpktlenerr_f(v)       ((U32(v) & 0x1U) << 8U)
#define nvtlc_rx_err_contain_en_0_rxpktlenerr__prod_f()                 (0x100U)
#define nvtlc_rx_err_contain_en_0_datlengtatomicreqmaxerr_f(v)\
				((U32(v) & 0x1U) << 17U)
#define nvtlc_rx_err_contain_en_0_datlengtatomicreqmaxerr__prod_f()   (0x20000U)
#define nvtlc_rx_err_contain_en_0_datlengtrmwreqmaxerr_f(v)\
				((U32(v) & 0x1U) << 18U)
#define nvtlc_rx_err_contain_en_0_datlengtrmwreqmaxerr__prod_f()      (0x40000U)
#define nvtlc_rx_err_contain_en_0_datlenltatrrspminerr_f(v)\
				((U32(v) & 0x1U) << 19U)
#define nvtlc_rx_err_contain_en_0_datlenltatrrspminerr__prod_f()      (0x80000U)
#define nvtlc_rx_err_contain_en_0_invalidcacheattrpoerr_f(v)\
				((U32(v) & 0x1U) << 20U)
#define nvtlc_rx_err_contain_en_0_invalidcacheattrpoerr__prod_f()    (0x100000U)
#define nvtlc_rx_err_contain_en_0_invalidcrerr_f(v)     ((U32(v) & 0x1U) << 21U)
#define nvtlc_rx_err_contain_en_0_invalidcrerr__prod_f()             (0x200000U)
#define nvtlc_rx_err_contain_en_0_rxrespstatustargeterr_f(v)\
				((U32(v) & 0x1U) << 22U)
#define nvtlc_rx_err_contain_en_0_rxrespstatustargeterr__prod_f()    (0x400000U)
#define nvtlc_rx_err_contain_en_0_rxrespstatusunsupportedrequesterr_f(v)\
				((U32(v) & 0x1U) << 23U)
#define nvtlc_rx_err_contain_en_0_rxrespstatusunsupportedrequesterr__prod_f()\
				(0x800000U)
#define nvtlc_rx_err_report_en_1_r()                               (0x00000f20U)
#define nvtlc_rx_err_report_en_1_rxhdrovferr_f(v)       ((U32(v) & 0xffU) << 0U)
#define nvtlc_rx_err_report_en_1_rxhdrovferr__prod_f()                   (0xffU)
#define nvtlc_rx_err_report_en_1_rxdataovferr_f(v)      ((U32(v) & 0xffU) << 8U)
#define nvtlc_rx_err_report_en_1_rxdataovferr__prod_f()                (0xff00U)
#define nvtlc_rx_err_report_en_1_stompdeterr_f(v)       ((U32(v) & 0x1U) << 16U)
#define nvtlc_rx_err_report_en_1_stompdeterr__prod_f()                (0x10000U)
#define nvtlc_rx_err_report_en_1_rxpoisonerr_f(v)       ((U32(v) & 0x1U) << 17U)
#define nvtlc_rx_err_report_en_1_rxpoisonerr__prod_f()                (0x20000U)
#define nvtlc_rx_err_report_en_1_rxunsupvcovferr_f(v)   ((U32(v) & 0x1U) << 19U)
#define nvtlc_rx_err_report_en_1_rxunsupvcovferr__prod_f()            (0x80000U)
#define nvtlc_rx_err_report_en_1_rxunsupnvlinkcreditrelerr_f(v)\
				((U32(v) & 0x1U) << 20U)
#define nvtlc_rx_err_report_en_1_rxunsupnvlinkcreditrelerr__prod_f() (0x100000U)
#define nvtlc_rx_err_report_en_1_rxunsupncisoccreditrelerr_f(v)\
				((U32(v) & 0x1U) << 21U)
#define nvtlc_rx_err_report_en_1_rxunsupncisoccreditrelerr__prod_f() (0x200000U)
#define nvtlc_rx_err_log_en_1_r()                                  (0x00000f1cU)
#define nvtlc_rx_err_log_en_1_rxhdrovferr_f(v)          ((U32(v) & 0xffU) << 0U)
#define nvtlc_rx_err_log_en_1_rxhdrovferr__prod_f()                      (0xffU)
#define nvtlc_rx_err_log_en_1_rxdataovferr_f(v)         ((U32(v) & 0xffU) << 8U)
#define nvtlc_rx_err_log_en_1_rxdataovferr__prod_f()                   (0xff00U)
#define nvtlc_rx_err_log_en_1_stompdeterr_f(v)          ((U32(v) & 0x1U) << 16U)
#define nvtlc_rx_err_log_en_1_stompdeterr__prod_f()                   (0x10000U)
#define nvtlc_rx_err_log_en_1_rxpoisonerr_f(v)          ((U32(v) & 0x1U) << 17U)
#define nvtlc_rx_err_log_en_1_rxpoisonerr__prod_f()                   (0x20000U)
#define nvtlc_rx_err_log_en_1_rxunsupvcovferr_f(v)      ((U32(v) & 0x1U) << 19U)
#define nvtlc_rx_err_log_en_1_rxunsupvcovferr__prod_f()               (0x80000U)
#define nvtlc_rx_err_log_en_1_rxunsupnvlinkcreditrelerr_f(v)\
				((U32(v) & 0x1U) << 20U)
#define nvtlc_rx_err_log_en_1_rxunsupnvlinkcreditrelerr__prod_f()    (0x100000U)
#define nvtlc_rx_err_log_en_1_rxunsupncisoccreditrelerr_f(v)\
				((U32(v) & 0x1U) << 21U)
#define nvtlc_rx_err_log_en_1_rxunsupncisoccreditrelerr__prod_f()    (0x200000U)
#define nvtlc_rx_err_contain_en_1_r()                              (0x00000f24U)
#define nvtlc_rx_err_contain_en_1_rxhdrovferr_f(v)      ((U32(v) & 0xffU) << 0U)
#define nvtlc_rx_err_contain_en_1_rxhdrovferr__prod_f()                  (0xffU)
#define nvtlc_rx_err_contain_en_1_rxdataovferr_f(v)     ((U32(v) & 0xffU) << 8U)
#define nvtlc_rx_err_contain_en_1_rxdataovferr__prod_f()               (0xff00U)
#define nvtlc_rx_err_contain_en_1_stompdeterr_f(v)      ((U32(v) & 0x1U) << 16U)
#define nvtlc_rx_err_contain_en_1_stompdeterr__prod_f()               (0x10000U)
#define nvtlc_rx_err_contain_en_1_rxpoisonerr_f(v)      ((U32(v) & 0x1U) << 17U)
#define nvtlc_rx_err_contain_en_1_rxpoisonerr__prod_f()               (0x20000U)
#define nvtlc_rx_err_contain_en_1_rxunsupvcovferr_f(v)  ((U32(v) & 0x1U) << 19U)
#define nvtlc_rx_err_contain_en_1_rxunsupvcovferr__prod_f()           (0x80000U)
#define nvtlc_rx_err_contain_en_1_rxunsupnvlinkcreditrelerr_f(v)\
				((U32(v) & 0x1U) << 20U)
#define nvtlc_rx_err_contain_en_1_rxunsupnvlinkcreditrelerr__prod_f()\
				(0x100000U)
#define nvtlc_rx_err_contain_en_1_rxunsupncisoccreditrelerr_f(v)\
				((U32(v) & 0x1U) << 21U)
#define nvtlc_rx_err_contain_en_1_rxunsupncisoccreditrelerr__prod_f()\
				(0x200000U)
#endif
