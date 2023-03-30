/*
 * Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <dce.h>
#include <dce-log.h>
#include <dce-util-common.h>

#define DCE_MAX_NO_SMB 8
#define DCE_MAX_HSP_IE 8

/**
 * smb_regs is a 2D array of read-only pointers to a function returning u32.
 *
 * Array of functions that retrun base addresses of shared maiboxes registers
 * in DCE cluster based on the mailbox id and HSP id.
 */
__weak u32 (*const smb_regs[DCE_MAX_HSP][DCE_MAX_NO_SMB])(void) = {
	{
		hsp_sm0_r,
		hsp_sm1_r,
		hsp_sm2_r,
		hsp_sm3_r,
		hsp_sm4_r,
		hsp_sm5_r,
		hsp_sm6_r,
		hsp_sm7_r,
	},
};

/**
 * smb_full_ie_regs is a 2D array of read-only pointers to a function
 * returning u32.
 *
 * Array of functions that retrun base addresses of full IE for shared
 * maiboxes registers in DCE cluster based on the mailbox id and HSP id.
 */
__weak u32 (*const smb_full_ie_regs[DCE_MAX_HSP][DCE_MAX_NO_SMB])(void) = {
	{
		hsp_sm0_full_int_ie_r,
		hsp_sm1_full_int_ie_r,
		hsp_sm2_full_int_ie_r,
		hsp_sm3_full_int_ie_r,
		hsp_sm4_full_int_ie_r,
		hsp_sm5_full_int_ie_r,
		hsp_sm6_full_int_ie_r,
		hsp_sm7_full_int_ie_r,
	},
};

/**
 * smb_empty_ie_regs is a 2D array of read-only pointers to a function
 * returning u32.
 *
 * Array of functions that retrun base addresses of empty IE for shared
 * maiboxes registers in DCE cluster based on the mailbox id and HSP id.
 */
__weak u32 (*const smb_empty_ie_regs[DCE_MAX_HSP][DCE_MAX_NO_SMB])(void) = {
	{
		hsp_sm0_empty_int_ie_r,
		hsp_sm1_empty_int_ie_r,
		hsp_sm2_empty_int_ie_r,
		hsp_sm3_empty_int_ie_r,
		hsp_sm4_empty_int_ie_r,
		hsp_sm5_empty_int_ie_r,
		hsp_sm6_empty_int_ie_r,
		hsp_sm7_empty_int_ie_r,
	},
};

/**
 * dce_smb_set - Set an u32 value to smb_#n in the DCE Cluster
 *
 * @d : Pointer to tegra_dce struct.
 * @val : val to set.
 * @id : Shared Mailbox Id.
 *
 * Return : Void
 */
void dce_smb_set(struct tegra_dce *d, u32 val, u8 id)
{
	u32 hsp = d->hsp_id;

	if (id >= DCE_MAX_NO_SMB || hsp >= DCE_MAX_HSP) {
		dce_err(d, "Invalid Shared Mailbox ID:%u or hsp:%u", id, hsp);
		return;
	}

	dce_writel(d, smb_regs[hsp][id](), val);
}

/**
 * dce_smb_set_full_ie - Set an u32 value to smb_#n in the DCE Cluster
 *
 * @d : Pointer to tegra_dce struct.
 * @en : enable if true and disable if false
 * @id : Shared Mailbox Id.
 *
 * Return : Void
 */
void dce_smb_set_full_ie(struct tegra_dce *d, bool en, u8 id)
{
	u32 val = en ? 1U : 0U;
	u32 hsp = d->hsp_id;

	if (id >= DCE_MAX_NO_SMB || hsp >= DCE_MAX_HSP) {
		dce_err(d, "Invalid Shared Mailbox ID:%u or hsp:%u", id, hsp);
		return;
	}

	dce_writel(d, smb_full_ie_regs[hsp][id](), val);
}

/**
 * dce_smb_read_full_ie - Set an u32 value to smb_#n in the DCE Cluster
 *
 * @d : Pointer to tegra_dce struct.
 * @id : Shared Mailbox Id.
 *
 * Return : u32 register value
 */
u32 dce_smb_read_full_ie(struct tegra_dce *d, u8 id)
{
	u32 hsp = d->hsp_id;

	if (id >= DCE_MAX_NO_SMB || hsp >= DCE_MAX_HSP) {
		dce_err(d, "Invalid Shared Mailbox ID:%u or hsp:%u", id, hsp);
		return 0xffffffff; /* TODO : Add DCE Error Numbers */
	}

	return dce_readl(d, smb_full_ie_regs[hsp][id]());
}

/**
 * dce_smb_enable_empty_ie - Set an u32 value to smb_#n in the DCE Cluster
 *
 * @d : Pointer to tegra_dce struct.
 * @en : enable if true and disable if false
 * @id : Shared Mailbox Id.
 *
 * Return : Void
 */
void dce_smb_set_empty_ie(struct tegra_dce *d, bool en, u8 id)
{
	u32 val = en ? 1U : 0U;
	u32 hsp = d->hsp_id;

	if (id >= DCE_MAX_NO_SMB || hsp >= DCE_MAX_HSP) {
		dce_err(d, "Invalid Shared Mailbox ID:%u or hsp:%u", id, hsp);
		return;
	}

	dce_writel(d, smb_empty_ie_regs[hsp][id](), val);
}

/**
 * dce_smb_read - Read the u32 value from smb_#n in the DCE Cluster
 *
 * @d : Pointer to tegra_dce struct.
 * @id : Shared Mailbox Id.
 *
 * Return : actual value if successful, 0xffffffff for errors scenarios
 */
u32 dce_smb_read(struct tegra_dce *d, u8 id)
{
	u32 hsp = d->hsp_id;

	if (id >= DCE_MAX_NO_SMB || hsp >= DCE_MAX_HSP) {
		dce_err(d, "Invalid Shared Mailbox ID:%u or hsp:%u", id, hsp);
		return 0xffffffff; /* TODO : Add DCE Error Numbers */
	}

	return dce_readl(d, smb_regs[hsp][id]());
}

/**
 * hsp_int_ie_regs is a 2D array of read-only pointers to a
 * function returning u32.
 *
 * Array of functions that retrun base addresses of hsp IE
 * regs in DCE cluster based on the id.
 */
__weak u32 (*const hsp_int_ie_regs[DCE_MAX_HSP][DCE_MAX_HSP_IE])(void) = {
	{
		hsp_int_ie0_r,
		hsp_int_ie1_r,
		hsp_int_ie2_r,
		hsp_int_ie3_r,
		hsp_int_ie4_r,
		hsp_int_ie5_r,
		hsp_int_ie6_r,
		hsp_int_ie7_r,
	},
};

/**
 * hsp_int_ie_regs is a 1D array of read-only pointers to a
 * function returning u32.
 *
 * Array of functions that retrun addresses of hsp IR
 * regs in DCE cluster based on the id.
 */
__weak u32 (*const hsp_int_ir_regs[DCE_MAX_HSP])(void) = {

	hsp_int_ir_r,
};

/**
 * dce_hsp_ie_read - Read the u32 value from hsp_int_ie#n
 *						in the DCE Cluster
 *
 * @d : Pointer to tegra_dce struct.
 * @id : Shared IE Id.
 *
 * Return : actual value if successful, 0xffffffff for errors scenarios
 */
u32 dce_hsp_ie_read(struct tegra_dce *d, u8 id)
{
	u32 hsp = d->hsp_id;

	if (id >= DCE_MAX_HSP_IE || hsp >= DCE_MAX_HSP) {
		dce_err(d, "Invalid Shared HSP IE ID:%u or hsp:%u", id, hsp);
		return 0xffffffff; /* TODO : Add DCE Error Numbers */
	}

	return dce_readl(d, hsp_int_ie_regs[hsp][id]());
}

/**
 * dce_hsp_ie_write - Read the u32 value from hsp_int_ie#n
 *						in the DCE Cluster
 *
 * @d : Pointer to tegra_dce struct.
 * @val : Value to be written
 * @id : Shared IE Id.
 *
 * Return : void
 */
void dce_hsp_ie_write(struct tegra_dce *d, u32 val, u8 id)
{
	u32 hsp = d->hsp_id;

	if (id >= DCE_MAX_HSP_IE || hsp >= DCE_MAX_HSP) {
		dce_err(d, "Invalid Shared HSP IE ID:%u or hsp:%u", id, hsp);
		return;
	}

	dce_writel(d, hsp_int_ie_regs[hsp][id](),
			val | dce_readl(d, hsp_int_ie_regs[hsp][id]()));
}

/**
 * dce_hsp_ir_read - Read the u32 value from hsp_int_ir
 *					in the DCE Cluster
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : actual value if successful, 0xffffffff for errors scenarios
 */
u32 dce_hsp_ir_read(struct tegra_dce *d)
{
	u32 hsp = d->hsp_id;

	if (hsp >= DCE_MAX_HSP) {
		dce_err(d, "Invalid HSP ID:%u", hsp);
		return 0xffffffff; /* TODO : Add DCE Error Numbers */
	}

	return dce_readl(d, hsp_int_ir_regs[hsp]());
}
