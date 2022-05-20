/*
 * board-id.c: Function definitions to get board IDs of internal panel
 * connectors.
 *
 * Copyright (C) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/highmem.h>
#include "board-id.h"

static int panel_id;
static struct board_info display_board_info;

static int __init display_tegra_dts_info(void)
{
	int ret_d;
	int ret_t;
	unsigned long dt_root;
	const char *dts_fname;
	const char *dtb_bdate;
	const char *dtb_btime;
	struct device_node *root;

	dt_root = of_get_flat_dt_root();

	dts_fname = of_get_flat_dt_prop(dt_root, "nvidia,dtsfilename", NULL);
	if (dts_fname)
		pr_info("DTS File Name: %s\n", dts_fname);
	else
		pr_info("DTS File Name: <unknown>\n");

	root = of_find_node_by_path("/");
	if (!root)
		pr_info("root node NULL\n");
	ret_d = of_property_read_string_index(root,
			"nvidia,dtbbuildtime", 0, &dtb_bdate);
	ret_t = of_property_read_string_index(root,
			"nvidia,dtbbuildtime", 1, &dtb_btime);
	if (!ret_d && !ret_t)
		pr_info("DTB Build time: %s %s\n", dtb_bdate, dtb_btime);
	else
		pr_info("DTB Build time: <unknown>\n");

	if (root)
		of_node_put(root);
	return 0;
}
early_initcall(display_tegra_dts_info);

int tegra_dc_get_board_panel_id(void)
{
	return panel_id;
}
static int __init tegra_board_panel_id(char *options)
{
	char *p = options;

	panel_id = memparse(p, &p);
	return panel_id;
}
__setup("display_panel=", tegra_board_panel_id);

#define BOARD_INFO_PATH_LEN 50
static int tegra_get_board_info_properties(struct board_info *bi,
		const char *property_name)
{
	struct device_node *board_info = NULL;
	char board_info_path[BOARD_INFO_PATH_LEN+1] = {0};
	u32 prop_val;
	int err;

	if (strlen("/chosen/") + strlen(property_name) > BOARD_INFO_PATH_LEN) {
		pr_err("property_name too long\n");
		goto out;
	}

	strlcpy(board_info_path, "/chosen/", BOARD_INFO_PATH_LEN);
	strlcat(board_info_path, property_name,
			BOARD_INFO_PATH_LEN - strlen("/chosen/"));

	board_info = of_find_node_by_path(board_info_path);
	memset(bi, 0, sizeof(*bi));

	if (board_info) {
		err = of_property_read_u32(board_info, "id", &prop_val);
		if (err < 0) {
			pr_err("failed to read %s/id\n", board_info_path);
			goto out;
		}
		bi->board_id = prop_val;

		err = of_property_read_u32(board_info, "sku", &prop_val);
		if (err < 0) {
			pr_err("failed to read %s/sku\n", board_info_path);
			goto out;
		}
		bi->sku = prop_val;

		err = of_property_read_u32(board_info, "fab", &prop_val);
		if (err < 0) {
			pr_err("failed to read %s/fab\n", board_info_path);
			goto out;
		}
		bi->fab = prop_val;

		err = of_property_read_u32(board_info,
					"major_revision", &prop_val);
		if (err < 0) {
			pr_err("failed to read %s/major_revision\n",
					board_info_path);
			goto out;
		}
		bi->major_revision = prop_val;

		err = of_property_read_u32(board_info,
					"minor_revision", &prop_val);
		if (err < 0) {
			pr_err("failed to read %s/minor_revision\n",
					board_info_path);
			goto out;
		}
		bi->minor_revision = prop_val;
		of_node_put(board_info);
		return 0;
	}

	pr_err("Node path %s not found\n", board_info_path);
out:
	if (board_info != NULL)
		of_node_put(board_info);
	return -1;
}

void tegra_dc_get_display_board_info(struct board_info *bi)
{
	static bool parsed;

	if (!parsed) {
		int ret;

		parsed = 1;
		ret = tegra_get_board_info_properties(bi, "display-board");
		if (!ret)
			memcpy(&display_board_info, bi,
					sizeof(struct board_info));
	}
	memcpy(bi, &display_board_info, sizeof(struct board_info));
}
