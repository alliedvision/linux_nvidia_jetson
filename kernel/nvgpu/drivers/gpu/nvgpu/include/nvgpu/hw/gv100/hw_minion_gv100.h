/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_HW_MINION_GV100_H
#define NVGPU_HW_MINION_GV100_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define minion_minion_status_r()                                   (0x00000830U)
#define minion_minion_status_status_f(v)                ((U32(v) & 0xffU) << 0U)
#define minion_minion_status_status_m()                       (U32(0xffU) << 0U)
#define minion_minion_status_status_v(r)                   (((r) >> 0U) & 0xffU)
#define minion_minion_status_status_boot_v()                       (0x00000001U)
#define minion_minion_status_status_boot_f()                              (0x1U)
#define minion_minion_status_intr_code_f(v)         ((U32(v) & 0xffffffU) << 8U)
#define minion_minion_status_intr_code_m()                (U32(0xffffffU) << 8U)
#define minion_minion_status_intr_code_v(r)            (((r) >> 8U) & 0xffffffU)
#define minion_falcon_irqstat_r()                                  (0x00000008U)
#define minion_falcon_irqstat_halt_f(v)                  ((U32(v) & 0x1U) << 4U)
#define minion_falcon_irqstat_halt_v(r)                     (((r) >> 4U) & 0x1U)
#define minion_falcon_irqstat_exterr_f(v)                ((U32(v) & 0x1U) << 5U)
#define minion_falcon_irqstat_exterr_v(r)                   (((r) >> 5U) & 0x1U)
#define minion_falcon_irqstat_exterr_true_v()                      (0x00000001U)
#define minion_falcon_irqstat_exterr_true_f()                            (0x20U)
#define minion_falcon_irqmask_r()                                  (0x00000018U)
#define minion_falcon_irqsclr_r()                                  (0x00000004U)
#define minion_falcon_irqsset_r()                                  (0x00000000U)
#define minion_falcon_irqmset_r()                                  (0x00000010U)
#define minion_falcon_irqmset_wdtmr_f(v)                 ((U32(v) & 0x1U) << 1U)
#define minion_falcon_irqmset_wdtmr_m()                        (U32(0x1U) << 1U)
#define minion_falcon_irqmset_wdtmr_v(r)                    (((r) >> 1U) & 0x1U)
#define minion_falcon_irqmset_wdtmr_set_v()                        (0x00000001U)
#define minion_falcon_irqmset_wdtmr_set_f()                               (0x2U)
#define minion_falcon_irqmset_halt_f(v)                  ((U32(v) & 0x1U) << 4U)
#define minion_falcon_irqmset_halt_m()                         (U32(0x1U) << 4U)
#define minion_falcon_irqmset_halt_v(r)                     (((r) >> 4U) & 0x1U)
#define minion_falcon_irqmset_halt_set_v()                         (0x00000001U)
#define minion_falcon_irqmset_halt_set_f()                               (0x10U)
#define minion_falcon_irqmset_exterr_f(v)                ((U32(v) & 0x1U) << 5U)
#define minion_falcon_irqmset_exterr_m()                       (U32(0x1U) << 5U)
#define minion_falcon_irqmset_exterr_v(r)                   (((r) >> 5U) & 0x1U)
#define minion_falcon_irqmset_exterr_set_v()                       (0x00000001U)
#define minion_falcon_irqmset_exterr_set_f()                             (0x20U)
#define minion_falcon_irqmset_swgen0_f(v)                ((U32(v) & 0x1U) << 6U)
#define minion_falcon_irqmset_swgen0_m()                       (U32(0x1U) << 6U)
#define minion_falcon_irqmset_swgen0_v(r)                   (((r) >> 6U) & 0x1U)
#define minion_falcon_irqmset_swgen0_set_v()                       (0x00000001U)
#define minion_falcon_irqmset_swgen0_set_f()                             (0x40U)
#define minion_falcon_irqmset_swgen1_f(v)                ((U32(v) & 0x1U) << 7U)
#define minion_falcon_irqmset_swgen1_m()                       (U32(0x1U) << 7U)
#define minion_falcon_irqmset_swgen1_v(r)                   (((r) >> 7U) & 0x1U)
#define minion_falcon_irqmset_swgen1_set_v()                       (0x00000001U)
#define minion_falcon_irqmset_swgen1_set_f()                             (0x80U)
#define minion_falcon_irqdest_r()                                  (0x0000001cU)
#define minion_falcon_irqdest_host_wdtmr_f(v)            ((U32(v) & 0x1U) << 1U)
#define minion_falcon_irqdest_host_wdtmr_m()                   (U32(0x1U) << 1U)
#define minion_falcon_irqdest_host_wdtmr_v(r)               (((r) >> 1U) & 0x1U)
#define minion_falcon_irqdest_host_wdtmr_host_v()                  (0x00000001U)
#define minion_falcon_irqdest_host_wdtmr_host_f()                         (0x2U)
#define minion_falcon_irqdest_host_halt_f(v)             ((U32(v) & 0x1U) << 4U)
#define minion_falcon_irqdest_host_halt_m()                    (U32(0x1U) << 4U)
#define minion_falcon_irqdest_host_halt_v(r)                (((r) >> 4U) & 0x1U)
#define minion_falcon_irqdest_host_halt_host_v()                   (0x00000001U)
#define minion_falcon_irqdest_host_halt_host_f()                         (0x10U)
#define minion_falcon_irqdest_host_exterr_f(v)           ((U32(v) & 0x1U) << 5U)
#define minion_falcon_irqdest_host_exterr_m()                  (U32(0x1U) << 5U)
#define minion_falcon_irqdest_host_exterr_v(r)              (((r) >> 5U) & 0x1U)
#define minion_falcon_irqdest_host_exterr_host_v()                 (0x00000001U)
#define minion_falcon_irqdest_host_exterr_host_f()                       (0x20U)
#define minion_falcon_irqdest_host_swgen0_f(v)           ((U32(v) & 0x1U) << 6U)
#define minion_falcon_irqdest_host_swgen0_m()                  (U32(0x1U) << 6U)
#define minion_falcon_irqdest_host_swgen0_v(r)              (((r) >> 6U) & 0x1U)
#define minion_falcon_irqdest_host_swgen0_host_v()                 (0x00000001U)
#define minion_falcon_irqdest_host_swgen0_host_f()                       (0x40U)
#define minion_falcon_irqdest_host_swgen1_f(v)           ((U32(v) & 0x1U) << 7U)
#define minion_falcon_irqdest_host_swgen1_m()                  (U32(0x1U) << 7U)
#define minion_falcon_irqdest_host_swgen1_v(r)              (((r) >> 7U) & 0x1U)
#define minion_falcon_irqdest_host_swgen1_host_v()                 (0x00000001U)
#define minion_falcon_irqdest_host_swgen1_host_f()                       (0x80U)
#define minion_falcon_irqdest_target_wdtmr_f(v)         ((U32(v) & 0x1U) << 17U)
#define minion_falcon_irqdest_target_wdtmr_m()                (U32(0x1U) << 17U)
#define minion_falcon_irqdest_target_wdtmr_v(r)            (((r) >> 17U) & 0x1U)
#define minion_falcon_irqdest_target_wdtmr_host_normal_v()         (0x00000000U)
#define minion_falcon_irqdest_target_wdtmr_host_normal_f()                (0x0U)
#define minion_falcon_irqdest_target_halt_f(v)          ((U32(v) & 0x1U) << 20U)
#define minion_falcon_irqdest_target_halt_m()                 (U32(0x1U) << 20U)
#define minion_falcon_irqdest_target_halt_v(r)             (((r) >> 20U) & 0x1U)
#define minion_falcon_irqdest_target_halt_host_normal_v()          (0x00000000U)
#define minion_falcon_irqdest_target_halt_host_normal_f()                 (0x0U)
#define minion_falcon_irqdest_target_exterr_f(v)        ((U32(v) & 0x1U) << 21U)
#define minion_falcon_irqdest_target_exterr_m()               (U32(0x1U) << 21U)
#define minion_falcon_irqdest_target_exterr_v(r)           (((r) >> 21U) & 0x1U)
#define minion_falcon_irqdest_target_exterr_host_normal_v()        (0x00000000U)
#define minion_falcon_irqdest_target_exterr_host_normal_f()               (0x0U)
#define minion_falcon_irqdest_target_swgen0_f(v)        ((U32(v) & 0x1U) << 22U)
#define minion_falcon_irqdest_target_swgen0_m()               (U32(0x1U) << 22U)
#define minion_falcon_irqdest_target_swgen0_v(r)           (((r) >> 22U) & 0x1U)
#define minion_falcon_irqdest_target_swgen0_host_normal_v()        (0x00000000U)
#define minion_falcon_irqdest_target_swgen0_host_normal_f()               (0x0U)
#define minion_falcon_irqdest_target_swgen1_f(v)        ((U32(v) & 0x1U) << 23U)
#define minion_falcon_irqdest_target_swgen1_m()               (U32(0x1U) << 23U)
#define minion_falcon_irqdest_target_swgen1_v(r)           (((r) >> 23U) & 0x1U)
#define minion_falcon_irqdest_target_swgen1_host_normal_v()        (0x00000000U)
#define minion_falcon_irqdest_target_swgen1_host_normal_f()               (0x0U)
#define minion_falcon_os_r()                                       (0x00000080U)
#define minion_falcon_mailbox1_r()                                 (0x00000044U)
#define minion_minion_intr_r()                                     (0x00000810U)
#define minion_minion_intr_fatal_f(v)                    ((U32(v) & 0x1U) << 0U)
#define minion_minion_intr_fatal_m()                           (U32(0x1U) << 0U)
#define minion_minion_intr_fatal_v(r)                       (((r) >> 0U) & 0x1U)
#define minion_minion_intr_nonfatal_f(v)                 ((U32(v) & 0x1U) << 1U)
#define minion_minion_intr_nonfatal_m()                        (U32(0x1U) << 1U)
#define minion_minion_intr_nonfatal_v(r)                    (((r) >> 1U) & 0x1U)
#define minion_minion_intr_falcon_stall_f(v)             ((U32(v) & 0x1U) << 2U)
#define minion_minion_intr_falcon_stall_m()                    (U32(0x1U) << 2U)
#define minion_minion_intr_falcon_stall_v(r)                (((r) >> 2U) & 0x1U)
#define minion_minion_intr_falcon_nostall_f(v)           ((U32(v) & 0x1U) << 3U)
#define minion_minion_intr_falcon_nostall_m()                  (U32(0x1U) << 3U)
#define minion_minion_intr_falcon_nostall_v(r)              (((r) >> 3U) & 0x1U)
#define minion_minion_intr_link_f(v)                 ((U32(v) & 0xffffU) << 16U)
#define minion_minion_intr_link_m()                        (U32(0xffffU) << 16U)
#define minion_minion_intr_link_v(r)                    (((r) >> 16U) & 0xffffU)
#define minion_minion_intr_nonstall_en_r()                         (0x0000081cU)
#define minion_minion_intr_stall_en_r()                            (0x00000818U)
#define minion_minion_intr_stall_en_fatal_f(v)           ((U32(v) & 0x1U) << 0U)
#define minion_minion_intr_stall_en_fatal_m()                  (U32(0x1U) << 0U)
#define minion_minion_intr_stall_en_fatal_v(r)              (((r) >> 0U) & 0x1U)
#define minion_minion_intr_stall_en_fatal_enable_v()               (0x00000001U)
#define minion_minion_intr_stall_en_fatal_enable_f()                      (0x1U)
#define minion_minion_intr_stall_en_fatal_disable_v()              (0x00000000U)
#define minion_minion_intr_stall_en_fatal_disable_f()                     (0x0U)
#define minion_minion_intr_stall_en_nonfatal_f(v)        ((U32(v) & 0x1U) << 1U)
#define minion_minion_intr_stall_en_nonfatal_m()               (U32(0x1U) << 1U)
#define minion_minion_intr_stall_en_nonfatal_v(r)           (((r) >> 1U) & 0x1U)
#define minion_minion_intr_stall_en_nonfatal_enable_v()            (0x00000001U)
#define minion_minion_intr_stall_en_nonfatal_enable_f()                   (0x2U)
#define minion_minion_intr_stall_en_nonfatal_disable_v()           (0x00000000U)
#define minion_minion_intr_stall_en_nonfatal_disable_f()                  (0x0U)
#define minion_minion_intr_stall_en_falcon_stall_f(v)    ((U32(v) & 0x1U) << 2U)
#define minion_minion_intr_stall_en_falcon_stall_m()           (U32(0x1U) << 2U)
#define minion_minion_intr_stall_en_falcon_stall_v(r)       (((r) >> 2U) & 0x1U)
#define minion_minion_intr_stall_en_falcon_stall_enable_v()        (0x00000001U)
#define minion_minion_intr_stall_en_falcon_stall_enable_f()               (0x4U)
#define minion_minion_intr_stall_en_falcon_stall_disable_v()       (0x00000000U)
#define minion_minion_intr_stall_en_falcon_stall_disable_f()              (0x0U)
#define minion_minion_intr_stall_en_falcon_nostall_f(v)  ((U32(v) & 0x1U) << 3U)
#define minion_minion_intr_stall_en_falcon_nostall_m()         (U32(0x1U) << 3U)
#define minion_minion_intr_stall_en_falcon_nostall_v(r)     (((r) >> 3U) & 0x1U)
#define minion_minion_intr_stall_en_falcon_nostall_enable_v()      (0x00000001U)
#define minion_minion_intr_stall_en_falcon_nostall_enable_f()             (0x8U)
#define minion_minion_intr_stall_en_falcon_nostall_disable_v()     (0x00000000U)
#define minion_minion_intr_stall_en_falcon_nostall_disable_f()            (0x0U)
#define minion_minion_intr_stall_en_link_f(v)        ((U32(v) & 0xffffU) << 16U)
#define minion_minion_intr_stall_en_link_m()               (U32(0xffffU) << 16U)
#define minion_minion_intr_stall_en_link_v(r)           (((r) >> 16U) & 0xffffU)
#define minion_nvlink_dl_cmd_r(i)\
		(nvgpu_safe_add_u32(0x00000900U, nvgpu_safe_mult_u32((i), 4U)))
#define minion_nvlink_dl_cmd___size_1_v()                          (0x00000006U)
#define minion_nvlink_dl_cmd_command_f(v)               ((U32(v) & 0xffU) << 0U)
#define minion_nvlink_dl_cmd_command_v(r)                  (((r) >> 0U) & 0xffU)
#define minion_nvlink_dl_cmd_command_configeom_v()                 (0x00000040U)
#define minion_nvlink_dl_cmd_command_configeom_f()                       (0x40U)
#define minion_nvlink_dl_cmd_command_nop_v()                       (0x00000000U)
#define minion_nvlink_dl_cmd_command_nop_f()                              (0x0U)
#define minion_nvlink_dl_cmd_command_initphy_v()                   (0x00000001U)
#define minion_nvlink_dl_cmd_command_initphy_f()                          (0x1U)
#define minion_nvlink_dl_cmd_command_initlaneenable_v()            (0x00000003U)
#define minion_nvlink_dl_cmd_command_initlaneenable_f()                   (0x3U)
#define minion_nvlink_dl_cmd_command_initdlpl_v()                  (0x00000004U)
#define minion_nvlink_dl_cmd_command_initdlpl_f()                         (0x4U)
#define minion_nvlink_dl_cmd_command_lanedisable_v()               (0x00000008U)
#define minion_nvlink_dl_cmd_command_lanedisable_f()                      (0x8U)
#define minion_nvlink_dl_cmd_command_fastlanedisable_v()           (0x00000009U)
#define minion_nvlink_dl_cmd_command_fastlanedisable_f()                  (0x9U)
#define minion_nvlink_dl_cmd_command_laneshutdown_v()              (0x0000000cU)
#define minion_nvlink_dl_cmd_command_laneshutdown_f()                     (0xcU)
#define minion_nvlink_dl_cmd_command_setacmode_v()                 (0x0000000aU)
#define minion_nvlink_dl_cmd_command_setacmode_f()                        (0xaU)
#define minion_nvlink_dl_cmd_command_clracmode_v()                 (0x0000000bU)
#define minion_nvlink_dl_cmd_command_clracmode_f()                        (0xbU)
#define minion_nvlink_dl_cmd_command_enablepm_v()                  (0x00000010U)
#define minion_nvlink_dl_cmd_command_enablepm_f()                        (0x10U)
#define minion_nvlink_dl_cmd_command_disablepm_v()                 (0x00000011U)
#define minion_nvlink_dl_cmd_command_disablepm_f()                       (0x11U)
#define minion_nvlink_dl_cmd_command_savestate_v()                 (0x00000018U)
#define minion_nvlink_dl_cmd_command_savestate_f()                       (0x18U)
#define minion_nvlink_dl_cmd_command_restorestate_v()              (0x00000019U)
#define minion_nvlink_dl_cmd_command_restorestate_f()                    (0x19U)
#define minion_nvlink_dl_cmd_command_initpll_0_v()                 (0x00000020U)
#define minion_nvlink_dl_cmd_command_initpll_0_f()                       (0x20U)
#define minion_nvlink_dl_cmd_command_initpll_1_v()                 (0x00000021U)
#define minion_nvlink_dl_cmd_command_initpll_1_f()                       (0x21U)
#define minion_nvlink_dl_cmd_command_initpll_2_v()                 (0x00000022U)
#define minion_nvlink_dl_cmd_command_initpll_2_f()                       (0x22U)
#define minion_nvlink_dl_cmd_command_initpll_3_v()                 (0x00000023U)
#define minion_nvlink_dl_cmd_command_initpll_3_f()                       (0x23U)
#define minion_nvlink_dl_cmd_command_initpll_4_v()                 (0x00000024U)
#define minion_nvlink_dl_cmd_command_initpll_4_f()                       (0x24U)
#define minion_nvlink_dl_cmd_command_initpll_5_v()                 (0x00000025U)
#define minion_nvlink_dl_cmd_command_initpll_5_f()                       (0x25U)
#define minion_nvlink_dl_cmd_command_initpll_6_v()                 (0x00000026U)
#define minion_nvlink_dl_cmd_command_initpll_6_f()                       (0x26U)
#define minion_nvlink_dl_cmd_command_initpll_7_v()                 (0x00000027U)
#define minion_nvlink_dl_cmd_command_initpll_7_f()                       (0x27U)
#define minion_nvlink_dl_cmd_fault_f(v)                 ((U32(v) & 0x1U) << 30U)
#define minion_nvlink_dl_cmd_fault_v(r)                    (((r) >> 30U) & 0x1U)
#define minion_nvlink_dl_cmd_fault_fault_clear_v()                 (0x00000001U)
#define minion_nvlink_dl_cmd_ready_f(v)                 ((U32(v) & 0x1U) << 31U)
#define minion_nvlink_dl_cmd_ready_v(r)                    (((r) >> 31U) & 0x1U)
#define minion_misc_0_r()                                          (0x000008b0U)
#define minion_misc_0_scratch_swrw_0_f(v)         ((U32(v) & 0xffffffffU) << 0U)
#define minion_misc_0_scratch_swrw_0_v(r)            (((r) >> 0U) & 0xffffffffU)
#define minion_nvlink_link_intr_r(i)\
		(nvgpu_safe_add_u32(0x00000a00U, nvgpu_safe_mult_u32((i), 4U)))
#define minion_nvlink_link_intr___size_1_v()                       (0x00000006U)
#define minion_nvlink_link_intr_code_f(v)               ((U32(v) & 0xffU) << 0U)
#define minion_nvlink_link_intr_code_m()                      (U32(0xffU) << 0U)
#define minion_nvlink_link_intr_code_v(r)                  (((r) >> 0U) & 0xffU)
#define minion_nvlink_link_intr_code_na_v()                        (0x00000000U)
#define minion_nvlink_link_intr_code_na_f()                               (0x0U)
#define minion_nvlink_link_intr_code_swreq_v()                     (0x00000001U)
#define minion_nvlink_link_intr_code_swreq_f()                            (0x1U)
#define minion_nvlink_link_intr_code_dlreq_v()                     (0x00000002U)
#define minion_nvlink_link_intr_code_dlreq_f()                            (0x2U)
#define minion_nvlink_link_intr_code_pmdisabled_v()                (0x00000003U)
#define minion_nvlink_link_intr_code_pmdisabled_f()                       (0x3U)
#define minion_nvlink_link_intr_subcode_f(v)            ((U32(v) & 0xffU) << 8U)
#define minion_nvlink_link_intr_subcode_m()                   (U32(0xffU) << 8U)
#define minion_nvlink_link_intr_subcode_v(r)               (((r) >> 8U) & 0xffU)
#define minion_nvlink_link_intr_state_f(v)              ((U32(v) & 0x1U) << 31U)
#define minion_nvlink_link_intr_state_m()                     (U32(0x1U) << 31U)
#define minion_nvlink_link_intr_state_v(r)                 (((r) >> 31U) & 0x1U)
#define minion_falcon_csberrstat_r()                               (0x00000244U)
#define minion_falcon_csberr_info_r()                              (0x00000248U)
#define minion_falcon_csberr_addr_r()                              (0x0000024cU)
#endif
