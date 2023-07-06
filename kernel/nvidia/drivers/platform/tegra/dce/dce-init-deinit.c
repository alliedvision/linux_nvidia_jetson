/*
 * Copyright (c) 2019-2023, NVIDIA CORPORATION & AFFILIATES.  All rights reserved.
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
#include <dce-util-common.h>

/**
 * dce_driver_init - Initializes the various sw components
 *					and few hw elements dce.
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : 0 if successful.
 */
int dce_driver_init(struct tegra_dce *d)
{
	int ret = 0;

	/**
	 * Set dce boot satus to false
	 */
	dce_set_boot_complete(d, false);

	ret = dce_boot_interface_init(d);
	if (ret) {
		dce_err(d, "dce boot interface init failed");
		goto err_boot_interface_init;
	}

	ret = dce_admin_init(d);
	if (ret) {
		dce_err(d, "dce admin interface init failed");
		goto err_admin_interface_init;
	}

	ret = dce_client_init(d);
	if (ret) {
		dce_err(d, "dce client workqueue init failed");
		goto err_client_init;
	}

	ret = dce_work_cond_sw_resource_init(d);
	if (ret) {
		dce_err(d, "dce sw resource init failed");
		goto err_sw_init;
	}

	ret = dce_fsm_init(d);
	if (ret) {
		dce_err(d, "dce worker thread init failed");
		goto err_fsm_init;
	}

	return ret;

err_fsm_init:
	dce_work_cond_sw_resource_deinit(d);
err_sw_init:
	dce_client_deinit(d);
err_client_init:
	dce_admin_deinit(d);
err_admin_interface_init:
	dce_boot_interface_deinit(d);
err_boot_interface_init:
	d->boot_status |= DCE_STATUS_FAILED;
	return ret;
}

/**
 * dce_driver_deinit - Release various sw resources
 *					associated with dce.
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : Void
 */

void dce_driver_deinit(struct tegra_dce *d)
{
	/*  TODO : Reset DCE ? */
	dce_fsm_deinit(d);

	dce_work_cond_sw_resource_deinit(d);

	dce_client_deinit(d);

	dce_admin_deinit(d);

	dce_boot_interface_deinit(d);

	dce_release_fw(d, d->fw_data);
}
