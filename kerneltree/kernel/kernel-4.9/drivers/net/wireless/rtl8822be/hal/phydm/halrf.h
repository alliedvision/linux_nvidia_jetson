/******************************************************************************
 *
 * Copyright(c) 2007 - 2011 Realtek Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 *
 ******************************************************************************/


#ifndef	_HALRF_H__
#define _HALRF_H__

/*============================================================*/
/*include files*/
/*============================================================*/

struct _hal_rf_ {
	u32		rf_supportability;

};


enum halrf_ability_e {

	HAL_RF_TX_PWR_TRACK	= BIT(0),
	HAL_RF_IQK				= BIT(1),
	HAL_RF_LCK				= BIT(2),
	HAL_RF_DPK				= BIT(3)
};

enum halrf_cmninfo_e {
    
	HALRF_CMNINFO_ABILITY = 0,
	ODM_CMNINFO_tmp
};

void
halrf_support_ability_debug(
	void		*p_dm_void,
	char		input[][16],
	u32		*_used,
	char		*output,
	u32		*_out_len
);

void
halrf_cmn_info_set(
	void		        *p_dm_void,
	u32			cmn_info,
	u64			value
);

u64
halrf_cmn_info_get(
	void		        *p_dm_void,
	u32			cmn_info
);

void
halrf_watchdog(
	void			*p_dm_void
);

void
halrf_supportability_init(
	void		*p_dm_void
);

void
halrf_init(
	void			*p_dm_void
);



#endif
