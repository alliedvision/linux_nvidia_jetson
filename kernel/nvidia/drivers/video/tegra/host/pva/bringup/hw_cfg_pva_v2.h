/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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
/*
 * Function naming determines intended use:
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
#ifndef _hw_cfg_pva_gen2_h_
#define _hw_cfg_pva_gen2_h_
#include "hw_cfg_pva_v1.h"
#define V2_SID_CONTROL_BASE 0x240000U

static inline u32 v2_cfg_user_sid_vm_r(u32 idx)
{
	return V2_SID_CONTROL_BASE + 0x4U * idx;
}

static inline u32 v2_cfg_priv_sid_r(void)
{
	return V2_SID_CONTROL_BASE + 0x20U;
}

static inline u32 v2_cfg_vps_sid_r(void)
{
	return V2_SID_CONTROL_BASE + 0x24U;
}

#define V2_ADDRESS_CONTROL_BASE 0x250000U

static inline u32 v2_cfg_r5user_lsegreg_r(void)
{
	return V2_ADDRESS_CONTROL_BASE + 0x8U;
}

static inline u32 v2_cfg_priv_ar1_lsegreg_r(void)
{
	return V2_ADDRESS_CONTROL_BASE + 0xCU;
}

static inline u32 v2_cfg_priv_ar2_lsegreg_r(void)
{
	return V2_ADDRESS_CONTROL_BASE + 0x10U;
}

static inline u32 v2_cfg_r5user_usegreg_r(void)
{
	return V2_ADDRESS_CONTROL_BASE + 0x1CU;
}

static inline u32 v2_cfg_priv_ar1_usegreg_r(void)
{
	return V2_ADDRESS_CONTROL_BASE + 0x20U;
}

static inline u32 v2_cfg_priv_ar2_usegreg_r(void)
{
	return V2_ADDRESS_CONTROL_BASE + 0x24U;
}

static inline u32 v2_cfg_priv_ar1_start_r(void)
{
	return V2_ADDRESS_CONTROL_BASE + 0x28U;
}

static inline u32 v2_cfg_priv_ar1_end_r(void)
{
	return V2_ADDRESS_CONTROL_BASE + 0x2CU;
}

static inline u32 v2_cfg_priv_ar2_start_r(void)
{
	return V2_ADDRESS_CONTROL_BASE + 0x30U;
}

static inline u32 v2_cfg_priv_ar2_end_r(void)
{
	return V2_ADDRESS_CONTROL_BASE + 0x34U;
}

#define V2_CFG_CCQ_BASE 0x260000U
#define V2_CFG_CCQ_SIZE 0x010000U

static inline u32 v2_cfg_ccq_r(u32 idx)
{
	return V2_CFG_CCQ_BASE + V2_CFG_CCQ_SIZE * idx;
}

static inline u32 v2_cfg_ccq_status_r(u32 ccq_idx, u32 status_idx)
{
	return V2_CFG_CCQ_BASE + V2_CFG_CCQ_SIZE * ccq_idx + 0x4U
		+ 0x4U * status_idx;
}

#endif
