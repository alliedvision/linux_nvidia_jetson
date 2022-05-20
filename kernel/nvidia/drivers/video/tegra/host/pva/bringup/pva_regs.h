/*
 *
 * Copyright (c) 2016-2019 NVIDIA Corporation.  All rights reserved.
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
#ifndef _PVA_REGS_H_
#define _PVA_REGS_H_

#include "pva-bit.h"
#include "hw_cfg_pva_v1.h"
#include "hw_cfg_pva_v2.h"
#include "hw_dma_ch_pva.h"
#include "hw_dma_desc_pva.h"
#include "hw_proc_pva.h"
#include "hw_hsp_pva.h"
#include "hw_sec_pva_v1.h"
#include "hw_sec_pva_v2.h"
#include "hw_evp_pva.h"
#include "pva-interface.h"
#include "pva_mailbox.h"
#include "pva-ucode-header.h"

/* Definition for LIC_INTR_ENABLE bits */
#define SEC_LIC_INTR_HSP1	0x1
#define SEC_LIC_INTR_HSP2	0x2
#define SEC_LIC_INTR_HSP3	0x4
#define SEC_LIC_INTR_HSP4	0x8
#define SEC_LIC_INTR_HSP_ALL	0xF
#define SEC_LIC_INTR_H1X_ALL	0x7

/* Watchdog support */
#define SEC_LIC_INTR_WDT	0x1

//unfied register interface for both v1 and v2
static inline u32 sec_lic_intr_status_r(int version)
{
	if (version == 1)
		return v1_sec_lic_intr_status_r();
	else
		return v2_sec_lic_intr_status_r();
}

static inline u32 cfg_ccq_status_r(int version, u32 ccq_idx, u32 status_idx)
{
	if (version == 1)
		return v1_cfg_ccq_status_r(status_idx);
	else
		return v2_cfg_ccq_status_r(ccq_idx, status_idx);
}

static inline u32 cfg_ccq_r(int version, u32 ccq_idx)
{
	if (version == 1)
		return v1_cfg_ccq_r();
	else
		return v2_cfg_ccq_r(ccq_idx);
}

static inline u32 cfg_r5user_lsegreg_r(int version)
{
	if (version == 1)
		return v1_cfg_r5user_lsegreg_r();
	else
		return v2_cfg_r5user_lsegreg_r();
}

static inline u32 cfg_priv_ar1_lsegreg_r(int version)
{
	if (version == 1)
		return v1_cfg_priv_ar1_lsegreg_r();
	else
		return v2_cfg_priv_ar1_lsegreg_r();
}

static inline u32 cfg_priv_ar2_lsegreg_r(int version)
{
	if (version == 1)
		return v1_cfg_priv_ar2_lsegreg_r();
	else
		return v2_cfg_priv_ar2_lsegreg_r();
}

static inline u32 cfg_r5user_usegreg_r(int version)
{
	if (version == 1)
		return v1_cfg_r5user_usegreg_r();
	else
		return v2_cfg_r5user_usegreg_r();
}

static inline u32 cfg_priv_ar1_usegreg_r(int version)
{
	if (version == 1)
		return v1_cfg_priv_ar1_usegreg_r();
	else
		return v2_cfg_priv_ar1_usegreg_r();
}

static inline u32 cfg_priv_ar2_usegreg_r(int version)
{
	if (version == 1)
		return v1_cfg_priv_ar2_usegreg_r();
	else
		return v2_cfg_priv_ar2_usegreg_r();
}

static inline u32 cfg_priv_ar1_start_r(int version)
{
	if (version == 1)
		return v1_cfg_priv_ar1_start_r();
	else
		return v2_cfg_priv_ar1_start_r();
}

static inline u32 cfg_priv_ar1_end_r(int version)
{
	if (version == 1)
		return v1_cfg_priv_ar1_end_r();
	else
		return v2_cfg_priv_ar1_end_r();
}

static inline u32 cfg_priv_ar2_start_r(int version)
{
	if (version == 1)
		return v1_cfg_priv_ar2_start_r();
	else
		return v2_cfg_priv_ar2_start_r();
}

static inline u32 cfg_priv_ar2_end_r(int version)
{
	if (version == 1)
		return v1_cfg_priv_ar2_end_r();
	else
		return v2_cfg_priv_ar2_end_r();
}

static inline u32 sec_lic_intr_enable_r(int version)
{
	if (version == 1)
		return v1_sec_lic_intr_enable_r();
	else
		return v2_sec_lic_intr_enable_r();
}
#endif
