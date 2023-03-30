/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef HW_AST_DCE_H
#define HW_AST_DCE_H

static inline u32 ast_ast0_control_r(void)
{
	return 0x40000U;
}
static inline u32 ast_ast0_control_write_mask_v(void)
{
	return 0xffdf83e7U;
}
static inline u32 ast_ast0_control_apbovron_shift_v(void)
{
	return 0x0000001fU;
}
static inline u32 ast_ast0_control_nicovron_shift_v(void)
{
	return 0x0000001eU;
}
static inline u32 ast_ast0_control_physstreamid_shift_v(void)
{
	return 0x00000016U;
}
static inline u32 ast_ast0_control_carveoutlock_true_f(void)
{
	return 0x100000U;
}
static inline u32 ast_ast0_control_carveoutlock_false_f(void)
{
	return 0x0U;
}
static inline u32 ast_ast0_control_carveoutlock_defphysical_shift_v(void)
{
	return 0x00000013U;
}
static inline u32 ast_ast0_control_carveoutlock_defvmindex_shift_v(void)
{
	return 0x0000000fU;
}
static inline u32 ast_ast0_control_carveoutlock_defcarveoutid_shift_v(void)
{
	return 0x00000005U;
}
static inline u32 ast_ast0_control_defsnoop_enable_f(void)
{
	return 0x4U;
}
static inline u32 ast_ast0_control_defsnoop_disable_f(void)
{
	return 0x0U;
}
static inline u32 ast_ast0_control_matcherrctl_no_decerr_f(void)
{
	return 0x0U;
}
static inline u32 ast_ast0_control_matcherrctl_decerr_f(void)
{
	return 0x2U;
}
static inline u32 ast_ast0_control_lock_true_f(void)
{
	return 0x1U;
}
static inline u32 ast_ast0_control_lock_false_f(void)
{
	return 0x0U;
}
static inline u32 ast_ast0_error_status_r(void)
{
	return 0x40004U;
}
static inline u32 ast_ast0_error_addr_lo_r(void)
{
	return 0x40008U;
}
static inline u32 ast_ast0_error_addr_hi_r(void)
{
	return 0x4000cU;
}
static inline u32 ast_ast0_streamid_ctl_0_r(void)
{
	return 0x40020U;
}
static inline u32 ast_ast0_streamid_ctl_0_write_mask_v(void)
{
	return 0x0000ff01U;
}
static inline u32 ast_ast0_streamid_ctl_0_streamid_shift_v(void)
{
	return 0x00000008U;
}
static inline u32 ast_ast0_streamid_ctl_0_enable_enable_f(void)
{
	return 0x1U;
}
static inline u32 ast_ast0_streamid_ctl_0_enable_disable_f(void)
{
	return 0x0U;
}
static inline u32 ast_ast0_streamid_ctl_1_r(void)
{
	return 0x40024U;
}
static inline u32 ast_ast0_streamid_ctl_1_write_mask_v(void)
{
	return 0x0000ff01U;
}
static inline u32 ast_ast0_streamid_ctl_1_streamid_shift_v(void)
{
	return 0x00000008U;
}
static inline u32 ast_ast0_streamid_ctl_1_enable_enable_f(void)
{
	return 0x1U;
}
static inline u32 ast_ast0_streamid_ctl_1_enable_disable_f(void)
{
	return 0x0U;
}
static inline u32 ast_ast0_region_0_slave_base_lo_r(void)
{
	return 0x40100U;
}
static inline u32 ast_ast0_region_0_slave_base_lo_slvbase_shift_v(void)
{
	return 0x0000000cU;
}
static inline u32 ast_ast0_region_0_slave_base_lo_write_mask_v(void)
{
	return 0xfffff001U;
}
static inline u32 ast_ast0_region_0_slave_base_lo_enable_true_f(void)
{
	return 0x1U;
}
static inline u32 ast_ast0_region_0_slave_base_lo_enable_false_f(void)
{
	return 0x0U;
}
static inline u32 ast_ast0_region_0_slave_base_hi_r(void)
{
	return 0x40104U;
}
static inline u32 ast_ast0_region_0_mask_lo_r(void)
{
	return 0x40108U;
}
static inline u32 ast_ast0_region_0_mask_lo_write_mask_v(void)
{
	return 0xfffff000U;
}
static inline u32 ast_ast0_region_0_mask_hi_r(void)
{
	return 0x4010cU;
}
static inline u32 ast_ast0_region_0_mask_hi_write_mask_v(void)
{
	return 0xffffffffU;
}
static inline u32 ast_ast0_region_0_master_base_lo_r(void)
{
	return 0x40110U;
}
static inline u32 ast_ast0_region_0_master_base_lo_write_mask_v(void)
{
	return 0xfffff000U;
}
static inline u32 ast_ast0_region_0_master_base_hi_r(void)
{
	return 0x40114U;
}
static inline u32 ast_ast0_region_0_control_r(void)
{
	return 0x40118U;
}
static inline u32 ast_ast0_region_0_control_write_mask_v(void)
{
	return 0x000f83e5U;
}
static inline u32 ast_ast0_region_0_control_physical_shift_v(void)
{
	return 0x00000013U;
}
static inline u32 ast_ast0_region_0_control_vmindex_shift_v(void)
{
	return 0x0000000fU;
}
static inline u32 ast_ast0_region_0_control_carveoutid_shift_v(void)
{
	return 0x00000005U;
}
static inline u32 ast_ast0_region_0_control_snoop_enable_f(void)
{
	return 0x4U;
}
static inline u32 ast_ast0_region_0_control_snoop_disable_f(void)
{
	return 0x0U;
}
static inline u32 ast_ast0_region_0_control_lock_true_f(void)
{
	return 0x1U;
}
static inline u32 ast_ast0_region_0_control_lock_false_f(void)
{
	return 0x0U;
}
static inline u32 ast_ast0_region_1_slave_base_lo_r(void)
{
	return 0x40120U;
}
static inline u32 ast_ast0_region_1_slave_base_hi_r(void)
{
	return 0x40124U;
}
static inline u32 ast_ast0_region_1_mask_lo_r(void)
{
	return 0x40128U;
}
static inline u32 ast_ast0_region_1_mask_hi_r(void)
{
	return 0x4012cU;
}
static inline u32 ast_ast0_region_1_master_base_lo_r(void)
{
	return 0x40130U;
}
static inline u32 ast_ast0_region_1_master_base_hi_r(void)
{
	return 0x40134U;
}
static inline u32 ast_ast0_region_1_control_r(void)
{
	return 0x40138U;
}
static inline u32 ast_ast1_control_r(void)
{
	return 0x50000U;
}
static inline u32 ast_ast1_control_write_mask_v(void)
{
	return 0xffdf83e7U;
}
static inline u32 ast_ast1_control_apbovron_shift_v(void)
{
	return 0x0000001fU;
}
static inline u32 ast_ast1_control_nicovron_shift_v(void)
{
	return 0x0000001eU;
}
static inline u32 ast_ast1_control_physstreamid_shift_v(void)
{
	return 0x00000016U;
}
static inline u32 ast_ast1_control_carveoutlock_true_f(void)
{
	return 0x100000U;
}
static inline u32 ast_ast1_control_carveoutlock_false_f(void)
{
	return 0x0U;
}
static inline u32 ast_ast1_control_carveoutlock_defphysical_shift_v(void)
{
	return 0x00000013U;
}
static inline u32 ast_ast1_control_carveoutlock_defvmindex_shift_v(void)
{
	return 0x0000000fU;
}
static inline u32 ast_ast1_control_carveoutlock_defcarveoutid_shift_v(void)
{
	return 0x00000005U;
}
static inline u32 ast_ast1_control_defsnoop_enable_f(void)
{
	return 0x4U;
}
static inline u32 ast_ast1_control_defsnoop_disable_f(void)
{
	return 0x0U;
}
static inline u32 ast_ast1_control_matcherrctl_no_decerr_f(void)
{
	return 0x0U;
}
static inline u32 ast_ast1_control_matcherrctl_decerr_f(void)
{
	return 0x2U;
}
static inline u32 ast_ast1_control_lock_true_f(void)
{
	return 0x1U;
}
static inline u32 ast_ast1_control_lock_false_f(void)
{
	return 0x0U;
}
static inline u32 ast_ast1_error_status_r(void)
{
	return 0x50004U;
}
static inline u32 ast_ast1_error_addr_lo_r(void)
{
	return 0x50008U;
}
static inline u32 ast_ast1_error_addr_hi_r(void)
{
	return 0x5000cU;
}
static inline u32 ast_ast1_streamid_ctl_0_r(void)
{
	return 0x50020U;
}
static inline u32 ast_ast1_streamid_ctl_0_write_mask_v(void)
{
	return 0x0000ff01U;
}
static inline u32 ast_ast1_streamid_ctl_0_streamid_shift_v(void)
{
	return 0x00000008U;
}
static inline u32 ast_ast1_streamid_ctl_0_enable_enable_f(void)
{
	return 0x1U;
}
static inline u32 ast_ast1_streamid_ctl_0_enable_disable_f(void)
{
	return 0x0U;
}
static inline u32 ast_ast1_streamid_ctl_1_r(void)
{
	return 0x50024U;
}
static inline u32 ast_ast1_streamid_ctl_1_write_mask_v(void)
{
	return 0x0000ff01U;
}
static inline u32 ast_ast1_streamid_ctl_1_streamid_shift_v(void)
{
	return 0x00000008U;
}
static inline u32 ast_ast1_streamid_ctl_1_enable_enable_f(void)
{
	return 0x1U;
}
static inline u32 ast_ast1_streamid_ctl_1_enable_disable_f(void)
{
	return 0x0U;
}
static inline u32 ast_ast1_region_0_slave_base_lo_r(void)
{
	return 0x50100U;
}
static inline u32 ast_ast1_region_0_slave_base_lo_slvbase_shift_v(void)
{
	return 0x0000000cU;
}
static inline u32 ast_ast1_region_0_slave_base_lo_write_mask_v(void)
{
	return 0xfffff001U;
}
static inline u32 ast_ast1_region_0_slave_base_lo_enable_true_f(void)
{
	return 0x1U;
}
static inline u32 ast_ast1_region_0_slave_base_lo_enable_false_f(void)
{
	return 0x0U;
}
static inline u32 ast_ast1_region_0_slave_base_hi_r(void)
{
	return 0x50104U;
}
static inline u32 ast_ast1_region_0_mask_lo_r(void)
{
	return 0x50108U;
}
static inline u32 ast_ast1_region_0_mask_lo_write_mask_v(void)
{
	return 0xfffff000U;
}
static inline u32 ast_ast1_region_0_mask_hi_r(void)
{
	return 0x5010cU;
}
static inline u32 ast_ast1_region_0_mask_hi_write_mask_v(void)
{
	return 0xffffffffU;
}
static inline u32 ast_ast1_region_0_master_base_lo_r(void)
{
	return 0x50110U;
}
static inline u32 ast_ast1_region_0_master_base_lo_write_mask_v(void)
{
	return 0xfffff000U;
}
static inline u32 ast_ast1_region_0_master_base_hi_r(void)
{
	return 0x50114U;
}
static inline u32 ast_ast1_region_0_control_r(void)
{
	return 0x50118U;
}
static inline u32 ast_ast1_region_0_control_write_mask_v(void)
{
	return 0x000f83e5U;
}
static inline u32 ast_ast1_region_0_control_physical_shift_v(void)
{
	return 0x00000013U;
}
static inline u32 ast_ast1_region_0_control_vmindex_shift_v(void)
{
	return 0x0000000fU;
}
static inline u32 ast_ast1_region_0_control_carveoutid_shift_v(void)
{
	return 0x00000005U;
}
static inline u32 ast_ast1_region_0_control_snoop_enable_f(void)
{
	return 0x4U;
}
static inline u32 ast_ast1_region_0_control_snoop_disable_f(void)
{
	return 0x0U;
}
static inline u32 ast_ast1_region_0_control_lock_true_f(void)
{
	return 0x1U;
}
static inline u32 ast_ast1_region_0_control_lock_false_f(void)
{
	return 0x0U;
}
static inline u32 ast_ast1_region_1_slave_base_lo_r(void)
{
	return 0x50120U;
}
static inline u32 ast_ast1_region_1_slave_base_hi_r(void)
{
	return 0x50124U;
}
static inline u32 ast_ast1_region_1_mask_lo_r(void)
{
	return 0x50128U;
}
static inline u32 ast_ast1_region_1_mask_hi_r(void)
{
	return 0x5012cU;
}
static inline u32 ast_ast1_region_1_master_base_lo_r(void)
{
	return 0x50130U;
}
static inline u32 ast_ast1_region_1_master_base_hi_r(void)
{
	return 0x50134U;
}
static inline u32 ast_ast1_region_1_control_r(void)
{
	return 0x50138U;
}
#endif
