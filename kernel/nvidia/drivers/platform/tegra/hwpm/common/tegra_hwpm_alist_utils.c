/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <soc/tegra/fuse.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/of_address.h>
#include <linux/dma-buf.h>

#include <uapi/linux/tegra-soc-hwpm-uapi.h>

#include <tegra_hwpm.h>
#include <tegra_hwpm_log.h>
#include <tegra_hwpm_common.h>
#include <tegra_hwpm_static_analysis.h>


int tegra_hwpm_get_allowlist_size(struct tegra_soc_hwpm *hwpm)
{
	int ret = 0;

	hwpm->full_alist_size = 0ULL;

	tegra_hwpm_fn(hwpm, " ");

	ret = tegra_hwpm_func_all_ip(hwpm, NULL, TEGRA_HWPM_GET_ALIST_SIZE);
	if (ret != 0) {
		tegra_hwpm_err(hwpm, "get_alist_size failed");
		return ret;
	}

	return 0;
}

static int tegra_hwpm_combine_alist(struct tegra_soc_hwpm *hwpm, u64 *alist)
{
	struct tegra_hwpm_func_args func_args;
	int err = 0;

	tegra_hwpm_fn(hwpm, " ");

	func_args.alist = alist;
	func_args.full_alist_idx = 0ULL;

	err = tegra_hwpm_func_all_ip(hwpm, &func_args,
		TEGRA_HWPM_COMBINE_ALIST);
	if (err != 0) {
		tegra_hwpm_err(hwpm, "combine alist failed");
		return err;
	}

	/* Check size of full alist with hwpm->full_alist_size*/
	if (func_args.full_alist_idx != hwpm->full_alist_size) {
		tegra_hwpm_err(hwpm, "full_alist_size 0x%llx doesn't match "
			"max full_alist_idx 0x%llx",
			hwpm->full_alist_size, func_args.full_alist_idx);
		err = -EINVAL;
	}

	return err;
}

int tegra_hwpm_update_allowlist(struct tegra_soc_hwpm *hwpm,
	void *ioctl_struct)
{
	int err = 0;
	u64 pinned_pages = 0;
	u64 page_idx = 0;
	u64 alist_buf_size = 0;
	u64 num_pages = 0;
	u64 *full_alist_u64 = NULL;
	void *full_alist = NULL;
	struct page **pages = NULL;
	struct tegra_soc_hwpm_query_allowlist *query_allowlist =
			(struct tegra_soc_hwpm_query_allowlist *)ioctl_struct;
	unsigned long user_va = (unsigned long)(query_allowlist->allowlist);
	unsigned long offset = user_va & ~PAGE_MASK;

	tegra_hwpm_fn(hwpm, " ");

	if (hwpm->full_alist_size == 0ULL) {
		tegra_hwpm_err(hwpm, "Invalid allowlist size");
		return -EINVAL;
	}

	alist_buf_size = tegra_hwpm_safe_mult_u64(hwpm->full_alist_size,
		hwpm->active_chip->get_alist_buf_size(hwpm));

	tegra_hwpm_dbg(hwpm, hwpm_info | hwpm_dbg_allowlist,
		"alist_buf_size 0x%llx", alist_buf_size);

	/* Memory map user buffer into kernel address space */
	alist_buf_size = tegra_hwpm_safe_add_u64(offset, alist_buf_size);

	/* Round-up and Divide */
	alist_buf_size = tegra_hwpm_safe_sub_u64(
		tegra_hwpm_safe_add_u64(alist_buf_size, PAGE_SIZE), 1ULL);
	num_pages = alist_buf_size / PAGE_SIZE;

	pages = (struct page **)kzalloc(sizeof(*pages) * num_pages, GFP_KERNEL);
	if (!pages) {
		tegra_hwpm_err(hwpm,
			"Couldn't allocate memory for pages array");
		err = -ENOMEM;
		goto alist_unmap;
	}

	pinned_pages = get_user_pages(user_va & PAGE_MASK, num_pages, 0,
				pages, NULL);
	if (pinned_pages != num_pages) {
		tegra_hwpm_err(hwpm, "Requested %llu pages / Got %ld pages",
				num_pages, pinned_pages);
		err = -ENOMEM;
		goto alist_unmap;
	}

	full_alist = vmap(pages, num_pages, VM_MAP, PAGE_KERNEL);
	if (!full_alist) {
		tegra_hwpm_err(hwpm, "Couldn't map allowlist buffer into"
				   " kernel address space");
		err = -ENOMEM;
		goto alist_unmap;
	}
	full_alist_u64 = (u64 *)(full_alist + offset);

	err = tegra_hwpm_combine_alist(hwpm, full_alist_u64);
	if (err != 0) {
		goto alist_unmap;
	}

	query_allowlist->allowlist_size = hwpm->full_alist_size;
	return 0;

alist_unmap:
	if (full_alist)
		vunmap(full_alist);
	if (pinned_pages > 0) {
		for (page_idx = 0ULL; page_idx < pinned_pages; page_idx++) {
			set_page_dirty(pages[page_idx]);
			put_page(pages[page_idx]);
		}
	}
	if (pages) {
		kfree(pages);
	}

	return err;
}
