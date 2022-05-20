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
#ifndef NVGPU_HW_NVLINKIP_DISCOVERY_GV100_H
#define NVGPU_HW_NVLINKIP_DISCOVERY_GV100_H

#include <nvgpu/types.h>
#include <nvgpu/static_analysis.h>

#define nvlinkip_discovery_common_entry_f(v)             ((U32(v) & 0x3U) << 0U)
#define nvlinkip_discovery_common_entry_v(r)                (((r) >> 0U) & 0x3U)
#define nvlinkip_discovery_common_entry_invalid_v()                (0x00000000U)
#define nvlinkip_discovery_common_entry_enum_v()                   (0x00000001U)
#define nvlinkip_discovery_common_entry_data1_v()                  (0x00000002U)
#define nvlinkip_discovery_common_entry_data2_v()                  (0x00000003U)
#define nvlinkip_discovery_common_contents_f(v)   ((U32(v) & 0x1fffffffU) << 2U)
#define nvlinkip_discovery_common_contents_v(r)      (((r) >> 2U) & 0x1fffffffU)
#define nvlinkip_discovery_common_chain_f(v)            ((U32(v) & 0x1U) << 31U)
#define nvlinkip_discovery_common_chain_v(r)               (((r) >> 31U) & 0x1U)
#define nvlinkip_discovery_common_chain_enable_v()                 (0x00000001U)
#define nvlinkip_discovery_common_device_f(v)           ((U32(v) & 0x3fU) << 2U)
#define nvlinkip_discovery_common_device_v(r)              (((r) >> 2U) & 0x3fU)
#define nvlinkip_discovery_common_device_invalid_v()               (0x00000000U)
#define nvlinkip_discovery_common_device_ioctrl_v()                (0x00000001U)
#define nvlinkip_discovery_common_device_nvltl_v()                 (0x00000002U)
#define nvlinkip_discovery_common_device_nvlink_v()                (0x00000003U)
#define nvlinkip_discovery_common_device_minion_v()                (0x00000004U)
#define nvlinkip_discovery_common_device_nvlipt_v()                (0x00000005U)
#define nvlinkip_discovery_common_device_nvltlc_v()                (0x00000006U)
#define nvlinkip_discovery_common_device_dlpl_v()                  (0x0000000bU)
#define nvlinkip_discovery_common_device_ioctrlmif_v()             (0x00000007U)
#define nvlinkip_discovery_common_device_dlpl_multicast_v()        (0x00000008U)
#define nvlinkip_discovery_common_device_nvltlc_multicast_v()      (0x00000009U)
#define nvlinkip_discovery_common_device_ioctrlmif_multicast_v()   (0x0000000aU)
#define nvlinkip_discovery_common_device_sioctrl_v()               (0x0000000cU)
#define nvlinkip_discovery_common_device_tioctrl_v()               (0x0000000dU)
#define nvlinkip_discovery_common_id_f(v)               ((U32(v) & 0xffU) << 8U)
#define nvlinkip_discovery_common_id_v(r)                  (((r) >> 8U) & 0xffU)
#define nvlinkip_discovery_common_version_f(v)        ((U32(v) & 0x7ffU) << 20U)
#define nvlinkip_discovery_common_version_v(r)           (((r) >> 20U) & 0x7ffU)
#define nvlinkip_discovery_common_pri_base_f(v)       ((U32(v) & 0xfffU) << 12U)
#define nvlinkip_discovery_common_pri_base_v(r)          (((r) >> 12U) & 0xfffU)
#define nvlinkip_discovery_common_intr_f(v)             ((U32(v) & 0x1fU) << 7U)
#define nvlinkip_discovery_common_intr_v(r)                (((r) >> 7U) & 0x1fU)
#define nvlinkip_discovery_common_reset_f(v)            ((U32(v) & 0x1fU) << 2U)
#define nvlinkip_discovery_common_reset_v(r)               (((r) >> 2U) & 0x1fU)
#define nvlinkip_discovery_common_ioctrl_length_f(v)   ((U32(v) & 0x3fU) << 24U)
#define nvlinkip_discovery_common_ioctrl_length_v(r)      (((r) >> 24U) & 0x3fU)
#define nvlinkip_discovery_common_dlpl_num_tx_f(v)      ((U32(v) & 0x7U) << 24U)
#define nvlinkip_discovery_common_dlpl_num_tx_v(r)         (((r) >> 24U) & 0x7U)
#define nvlinkip_discovery_common_dlpl_num_rx_f(v)      ((U32(v) & 0x7U) << 27U)
#define nvlinkip_discovery_common_dlpl_num_rx_v(r)         (((r) >> 27U) & 0x7U)
#define nvlinkip_discovery_common_data1_ioctrl_length_f(v)\
				((U32(v) & 0x7ffffU) << 12U)
#define nvlinkip_discovery_common_data1_ioctrl_length_v(r)\
				(((r) >> 12U) & 0x7ffffU)
#define nvlinkip_discovery_common_data2_type_f(v)      ((U32(v) & 0x1fU) << 26U)
#define nvlinkip_discovery_common_data2_type_v(r)         (((r) >> 26U) & 0x1fU)
#define nvlinkip_discovery_common_data2_type_invalid_v()           (0x00000000U)
#define nvlinkip_discovery_common_data2_type_pllcontrol_v()        (0x00000001U)
#define nvlinkip_discovery_common_data2_type_resetreg_v()          (0x00000002U)
#define nvlinkip_discovery_common_data2_type_intrreg_v()           (0x00000003U)
#define nvlinkip_discovery_common_data2_type_discovery_v()         (0x00000004U)
#define nvlinkip_discovery_common_data2_type_unicast_v()           (0x00000005U)
#define nvlinkip_discovery_common_data2_type_broadcast_v()         (0x00000006U)
#define nvlinkip_discovery_common_data2_addr_f(v)   ((U32(v) & 0xffffffU) << 2U)
#define nvlinkip_discovery_common_data2_addr_v(r)      (((r) >> 2U) & 0xffffffU)
#define nvlinkip_discovery_common_dlpl_data2_type_f(v) ((U32(v) & 0x1fU) << 26U)
#define nvlinkip_discovery_common_dlpl_data2_type_v(r)    (((r) >> 26U) & 0x1fU)
#define nvlinkip_discovery_common_dlpl_data2_master_f(v)\
				((U32(v) & 0x1U) << 15U)
#define nvlinkip_discovery_common_dlpl_data2_master_v(r)   (((r) >> 15U) & 0x1U)
#define nvlinkip_discovery_common_dlpl_data2_masterid_f(v)\
				((U32(v) & 0x7fU) << 8U)
#define nvlinkip_discovery_common_dlpl_data2_masterid_v(r) (((r) >> 8U) & 0x7fU)
#endif
