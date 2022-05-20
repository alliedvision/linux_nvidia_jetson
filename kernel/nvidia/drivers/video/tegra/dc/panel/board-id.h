/*
 * Copyright (C) 2020, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef __BOARD_ID_H__
#define __BOARD_ID_H__

struct board_info {
	u16 board_id;
	u16 sku;
	u8  fab;
	u8  major_revision;
	u8  minor_revision;
};

void tegra_dc_get_display_board_info(struct board_info *bi);
int tegra_dc_get_board_panel_id(void);

#endif /* __BOARD_ID_H__ */
