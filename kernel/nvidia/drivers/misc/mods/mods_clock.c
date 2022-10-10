// SPDX-License-Identifier: GPL-2.0
/*
 * This file is part of NVIDIA MODS kernel driver.
 *
 * Copyright (c) 2011-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA MODS kernel driver is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * NVIDIA MODS kernel driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NVIDIA MODS kernel driver.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "mods_internal.h"
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/reset.h>

#define ARBITRARY_MAX_CLK_FREQ  3500000000

static struct list_head mods_clock_handles;
static spinlock_t mods_clock_lock;
static u32 last_handle;

struct clock_entry {
	struct clk *pclk;
	u32 handle;
	struct list_head list;
};

static LIST_HEAD(reset_handles);

struct reset_data {
	char name[MAX_DT_SIZE];
	struct reset_control *rst;
};

struct reset_entry {
	struct list_head list;
	struct reset_data rst_data;
	u32 handle;
};

static struct device_node *find_clocks_node(const char *name)
{
	const char *node_name = "mods-simple-bus";
	struct device_node *pp = NULL, *np = NULL;

	pp = of_find_node_by_name(NULL, node_name);

	if (!pp) {
		mods_error_printk("'mods-simple-bus' node not found in device tree\n");
		return pp;
	}

	np = of_get_child_by_name(pp, name);
	return np;
}

void mods_init_clock_api(void)
{
	const char *okay_value = "okay";
	struct device_node *mods_np = NULL;
	struct property *pp = NULL;
	int size_value = 0;

	mods_np = find_clocks_node("mods-clocks");
	if (!mods_np) {
		mods_error_printk("'mods-clocks' node not found in device tree\n");
		goto err;
	}

	pp = of_find_property(mods_np, "status", NULL);
	if (IS_ERR(pp)) {
		mods_error_printk("'status' prop not found in 'mods-clocks' node.");
		goto err;
	}

	/* if status is 'okay', then skip updating property */
	if (of_device_is_available(mods_np))
		goto err;

	size_value = strlen(okay_value) + 1;
	pp->value = kmalloc(size_value, GFP_KERNEL);
	if (unlikely(!pp->value)) {
		pp->length = 0;
		goto err;
	}
	strncpy(pp->value, okay_value, size_value);
	pp->length = size_value;

err:
	of_node_put(mods_np);

	spin_lock_init(&mods_clock_lock);
	INIT_LIST_HEAD(&mods_clock_handles);
	last_handle = 0;
}

void mods_shutdown_clock_api(void)
{
	struct list_head   *head       = &mods_clock_handles;
	struct list_head   *reset_head = &reset_handles;
	struct reset_entry *entry      = NULL;
	struct list_head   *iter       = NULL;
	struct list_head   *tmp        = NULL;

	spin_lock(&mods_clock_lock);

	list_for_each_safe(iter, tmp, head) {
		struct clock_entry *entry
			= list_entry(iter, struct clock_entry, list);
		list_del(iter);
		kfree(entry);
	}

	list_for_each_safe(iter, tmp, reset_head) {
		entry = list_entry(iter, struct reset_entry, list);
		list_del(iter);
		kfree(entry);
	}

	spin_unlock(&mods_clock_lock);
}

static u32 mods_get_clock_handle(struct clk *pclk)
{
	struct list_head *head = &mods_clock_handles;
	struct list_head *iter;
	struct clock_entry *entry = NULL;
	u32 handle = 0;

	spin_lock(&mods_clock_lock);

	list_for_each(iter, head) {
		struct clock_entry *cur
			= list_entry(iter, struct clock_entry, list);
		if (cur->pclk == pclk) {
			entry = cur;
			handle = cur->handle;
			break;
		}
	}

	if (!entry) {
		entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
		if (!unlikely(!entry)) {
			entry->pclk = pclk;
			entry->handle = ++last_handle;
			handle = entry->handle;
			list_add(&entry->list, &mods_clock_handles);
		}
	}

	spin_unlock(&mods_clock_lock);

	return handle;
}

static struct clk *mods_get_clock(u32 handle)
{
	struct list_head *head = &mods_clock_handles;
	struct list_head *iter = NULL;
	struct clk *pclk = NULL;

	spin_lock(&mods_clock_lock);

	list_for_each(iter, head) {
		struct clock_entry *entry
			= list_entry(iter, struct clock_entry, list);
		if (entry->handle == handle) {
			pclk = entry->pclk;
			break;
		}
	}

	spin_unlock(&mods_clock_lock);

	return pclk;
}

static struct reset_data find_reset_data(u32 handle)
{
	struct list_head   *entry     = NULL;
	struct reset_entry *rst_entry = NULL;
	struct reset_data  reset_data = {"", NULL};

	spin_lock(&mods_clock_lock);

	list_for_each(entry, &reset_handles) {
		rst_entry = list_entry(entry, struct reset_entry, list);
		if (handle == rst_entry->handle) {
			reset_data = rst_entry->rst_data;
			break;
		}
	}

	spin_unlock(&mods_clock_lock);

	return reset_data;
}

static int get_reset_handle(struct reset_data reset_data)
{
	int    handle                 = -1;
	struct list_head *entry       = NULL;
	struct reset_entry *rst_entry = NULL;

	spin_lock(&mods_clock_lock);

	/* If entry has no rst structure, we are past last cached entry */
	list_for_each(entry, &reset_handles) {
		rst_entry = list_entry(entry, struct reset_entry, list);
		handle = rst_entry->handle;
		if (strcmp(rst_entry->rst_data.name,
			   reset_data.name) == 0) {
			goto failed;
		}
	}

	/* If reset not already in array, then we must add it */
	rst_entry = kzalloc(sizeof(struct reset_entry), GFP_ATOMIC);
	if (unlikely(!rst_entry)) {
		handle = -1;
		goto failed;
	}
	rst_entry->handle = ++handle;
	rst_entry->rst_data = reset_data;
	INIT_LIST_HEAD(&rst_entry->list);
	list_add_tail(&rst_entry->list, &reset_handles);

failed:
	spin_unlock(&mods_clock_lock);
	return handle;
}

int esc_mods_get_clock_handle(struct mods_client *client,
			      struct MODS_GET_CLOCK_HANDLE *p)
{
	struct clk *pclk = NULL;
	int ret = -EINVAL;

	struct device_node *mods_np = NULL;
	struct property *pp = NULL;

	LOG_ENT();

	mods_np = find_clocks_node("mods-clocks");
	if (!mods_np || !of_device_is_available(mods_np)) {
		cl_error("'mods-clocks' node not found in device tree\n");
		goto err;
	}
	pp = of_find_property(mods_np, "clock-names", NULL);
	if (IS_ERR(pp)) {
		cl_error(
		    "No 'clock-names' prop in 'mods-clocks' node for dev %s\n",
				  p->controller_name);
		goto err;
	}

	pclk = of_clk_get_by_name(mods_np, p->controller_name);

	if (IS_ERR(pclk))
		cl_error("clk (%s) not found\n", p->controller_name);
	else {
		p->clock_handle = mods_get_clock_handle(pclk);
		ret = OK;
	}
err:
	of_node_put(mods_np);
	LOG_EXT();
	return ret;
}

int esc_mods_get_rst_handle(struct mods_client *client,
			    struct MODS_GET_RESET_HANDLE *p)
{
	struct reset_data reset_data = {{0}, NULL};
	struct reset_control *p_reset_ctrl = NULL;
	int ret = -EINVAL;

	struct device_node *mods_np = NULL;
	struct property *pp = NULL;

	LOG_ENT();

	mods_np = find_clocks_node("mods-clocks");
	if (!mods_np || !of_device_is_available(mods_np)) {
		cl_error("'mods-clocks' node not found in device tree\n");
		goto err;
	}
	pp = of_find_property(mods_np, "reset-names", NULL);
	if (IS_ERR(pp)) {
		cl_error(
		"No 'reset-names' prop in 'mods-clocks' node for dev %s\n",
			p->reset_name);
		goto err;
	}

	p_reset_ctrl = of_reset_control_get(mods_np, p->reset_name);

	if (IS_ERR(p_reset_ctrl))
		cl_error("reset (%s) not found\n", p->reset_name);
	else {
		strncpy(reset_data.name, p->reset_name,
			sizeof(reset_data.name) - 1);
		if (reset_data.name[sizeof(reset_data.name) - 1] != '\0') {
			cl_error(
			"reset name %sis too large to store in reset array\n",
			reset_data.name);
			goto err;
		}
		reset_data.rst = p_reset_ctrl;
		p->reset_handle = get_reset_handle(reset_data);
		if (p->reset_handle == -1) {
			cl_error("no valid reset handle was acquired");
			goto err;
		}
		ret = OK;
	}
err:
	of_node_put(mods_np);
	LOG_EXT();
	return ret;
}

int esc_mods_set_clock_rate(struct mods_client *client,
			    struct MODS_CLOCK_RATE *p)
{
	struct clk *pclk = NULL;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		cl_error("unrecognized clock handle: 0x%x\n",
				  p->clock_handle);
	} else {
		ret = clk_set_rate(pclk, p->clock_rate_hz);
		if (ret) {
			cl_error(
				"unable to set rate %lluHz on clock 0x%x\n",
				p->clock_rate_hz, p->clock_handle);
		} else {
			cl_debug(DEBUG_CLOCK,
				 "successfuly set rate %lluHz on clock 0x%x\n",
				 p->clock_rate_hz, p->clock_handle);
		}
	}

	LOG_EXT();
	return ret;
}

int esc_mods_get_clock_rate(struct mods_client *client,
			    struct MODS_CLOCK_RATE *p)
{
	struct clk *pclk = NULL;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		cl_error("unrecognized clock handle: 0x%x\n",
				  p->clock_handle);
	} else {
		p->clock_rate_hz = clk_get_rate(pclk);
		cl_debug(DEBUG_CLOCK, "clock 0x%x has rate %lluHz\n",
			 p->clock_handle, p->clock_rate_hz);
		ret = OK;
	}

	LOG_EXT();
	return ret;
}

int esc_mods_get_clock_max_rate(struct mods_client *client,
				struct MODS_CLOCK_RATE *p)
{
	struct clk *pclk = NULL;
	int ret = -EINVAL;

	LOG_ENT();
	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		cl_error("unrecognized clock handle: 0x%x\n", p->clock_handle);
	} else {
		long rate = clk_round_rate(pclk, ARBITRARY_MAX_CLK_FREQ);

		p->clock_rate_hz = rate < 0 ? ARBITRARY_MAX_CLK_FREQ
			: (unsigned long)rate;
		cl_debug(DEBUG_CLOCK,
			 "clock 0x%x has max rate %lluHz\n",
			 p->clock_handle, p->clock_rate_hz);
		ret = OK;
	}

	LOG_EXT();
	return ret;
}

int esc_mods_set_clock_max_rate(struct mods_client *client,
				struct MODS_CLOCK_RATE *p)
{
	struct clk *pclk = NULL;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		cl_error("unrecognized clock handle: 0x%x\n", p->clock_handle);
	} else {
#if defined(CONFIG_TEGRA_CLOCK_DEBUG_FUNC)
		ret = tegra_clk_set_max(pclk, p->clock_rate_hz);
		if (ret) {
			cl_error(
				"unable to override max clock rate %lluHz on clock 0x%x\n",
				p->clock_rate_hz, p->clock_handle);
		} else {
			cl_debug(DEBUG_CLOCK,
				 "successfuly set max rate %lluHz on clock 0x%x\n",
				 p->clock_rate_hz, p->clock_handle);
		}
#else
		cl_error("unable to override max clock rate\n");
		cl_error(
			"reconfigure kernel with CONFIG_TEGRA_CLOCK_DEBUG_FUNC=y\n");
		ret = -EINVAL;
#endif
	}

	LOG_EXT();
	return ret;
}

int esc_mods_set_clock_parent(struct mods_client *client,
			      struct MODS_CLOCK_PARENT *p)
{
	struct clk *pclk = NULL;
	struct clk *pparent = NULL;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);
	pparent = mods_get_clock(p->clock_parent_handle);

	if (!pclk) {
		cl_error("unrecognized clock handle: 0x%x\n", p->clock_handle);
	} else if (!pparent) {
		cl_error("unrecognized parent clock handle: 0x%x\n",
			 p->clock_parent_handle);
	} else {
		ret = clk_set_parent(pclk, pparent);
		if (ret) {
			cl_error(
				"unable to make clock 0x%x parent of clock 0x%x\n",
				p->clock_parent_handle, p->clock_handle);
		} else {
			cl_debug(DEBUG_CLOCK,
				 "successfuly made clock 0x%x parent of clock 0x%x\n",
				 p->clock_parent_handle, p->clock_handle);
		}
	}

	LOG_EXT();
	return ret;
}

int esc_mods_get_clock_parent(struct mods_client *client,
			      struct MODS_CLOCK_PARENT *p)
{
	struct clk *pclk = NULL;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		cl_error("unrecognized clock handle: 0x%x\n", p->clock_handle);
	} else {
		struct clk *pparent = clk_get_parent(pclk);

		p->clock_parent_handle = mods_get_clock_handle(pparent);
		cl_debug(DEBUG_CLOCK,
			 "clock 0x%x is parent of clock 0x%x\n",
			 p->clock_parent_handle, p->clock_handle);
		ret = OK;
	}

	LOG_EXT();
	return ret;
}

int esc_mods_enable_clock(struct mods_client *client,
			  struct MODS_CLOCK_HANDLE *p)
{
	struct clk *pclk = NULL;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		cl_error("unrecognized clock handle: 0x%x\n", p->clock_handle);
	} else {
		ret = clk_prepare(pclk);
		if (ret) {
			cl_error(
				"unable to prepare clock 0x%x before enabling\n",
				p->clock_handle);
		}
		ret = clk_enable(pclk);
		if (ret) {
			cl_error("failed to enable clock 0x%x\n",
				 p->clock_handle);
		} else {
			cl_debug(DEBUG_CLOCK, "clock 0x%x enabled\n",
				 p->clock_handle);
		}
	}

	LOG_EXT();
	return ret;
}

int esc_mods_disable_clock(struct mods_client *client,
			   struct MODS_CLOCK_HANDLE *p)
{
	struct clk *pclk = NULL;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		cl_error("unrecognized clock handle: 0x%x\n", p->clock_handle);
	} else {
		clk_disable(pclk);
		clk_unprepare(pclk);
		cl_debug(DEBUG_CLOCK, "clock 0x%x disabled\n",
			 p->clock_handle);
		ret = OK;
	}

	LOG_EXT();
	return ret;
}

int esc_mods_is_clock_enabled(struct mods_client *client,
			      struct MODS_CLOCK_ENABLED *p)
{
	struct clk *pclk = NULL;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		cl_error("unrecognized clock handle: 0x%x\n", p->clock_handle);
	} else {
		p->enable_count = (u32)__clk_is_enabled(pclk);
		cl_debug(DEBUG_CLOCK, "clock 0x%x enable count is %u\n",
			 p->clock_handle, p->enable_count);
		ret = OK;
	}

	LOG_EXT();
	return ret;
}

int esc_mods_reset_assert(struct mods_client *client,
			  struct MODS_RESET_HANDLE *p)
{
	int err = -EINVAL;
	const struct reset_data reset_data = find_reset_data(p->handle);
	struct device_node *mods_np = NULL;

	LOG_ENT();
	mods_np = find_clocks_node("mods-clocks");
	if (!mods_np || !of_device_is_available(mods_np)) {
		cl_error("'mods-clocks' node not found in DTB\n");
		goto error;
	}

	if (!reset_data.rst) {
		cl_error("No reset corresponding to requested handle!\n");
		goto error;
	}

	if (p->assert)
		err = reset_control_assert(reset_data.rst);
	else
		err = reset_control_deassert(reset_data.rst);
	if (err) {
		cl_error("failed to %s reset on '%s'\n",
			 (p->assert ? "asserted" : "deasserted"),
			 reset_data.name);
	} else {
		cl_debug(DEBUG_CLOCK, "%s reset on '%s'",
			 (p->assert ? "asserted" : "desasserted"),
			 reset_data.name);
	}
error:
	of_node_put(mods_np);

	LOG_EXT();
	return err;
}

int esc_mods_clock_reset_assert(struct mods_client *client,
				struct MODS_CLOCK_HANDLE *p)
{
	struct clk *pclk = NULL;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		cl_error("unrecognized clock handle: 0x%x\n", p->clock_handle);
	} else {
		const char *clk_name = NULL;
		struct reset_control *prst = NULL;
		struct device_node *mods_np = NULL;
		struct property *pp = NULL;

		mods_np = find_clocks_node("mods-clocks");
		if (!mods_np || !of_device_is_available(mods_np)) {
			cl_error("'mods-clocks' node not found in DTB\n");
			goto err;
		}
		pp = of_find_property(mods_np, "reset-names", NULL);
		if (IS_ERR(pp)) {
			cl_error(
				"No 'reset-names' prop in 'mods-clocks' node for dev %s\n",
				__clk_get_name(pclk));
			goto err;
		}

		clk_name = __clk_get_name(pclk);

		prst = of_reset_control_get(mods_np, clk_name);
		if (IS_ERR(prst)) {
			cl_error("reset device %s not found\n", clk_name);
			goto err;
		}
		ret = reset_control_assert(prst);
		if (ret) {
			cl_error("failed to assert reset on '%s'\n", clk_name);
		} else {
			cl_debug(DEBUG_CLOCK, "asserted reset on '%s'",
				 clk_name);
		}

err:
		of_node_put(mods_np);
	}
	LOG_EXT();
	return ret;
}

int esc_mods_clock_reset_deassert(struct mods_client *client,
				  struct MODS_CLOCK_HANDLE *p)
{
	struct clk *pclk = NULL;
	int ret = -EINVAL;

	LOG_ENT();

	pclk = mods_get_clock(p->clock_handle);

	if (!pclk) {
		cl_error("unrecognized clock handle: 0x%x\n", p->clock_handle);
	} else {
		const char *clk_name = NULL;
		struct reset_control *prst = NULL;
		struct device_node *mods_np = NULL;
		struct property *pp = NULL;

		mods_np = find_clocks_node("mods-clocks");
		if (!mods_np || !of_device_is_available(mods_np)) {
			cl_error("'mods-clocks' node not found in DTB\n");
			goto err;
		}
		pp = of_find_property(mods_np, "reset-names", NULL);
		if (IS_ERR(pp)) {
			cl_error(
				"No 'reset-names' prop in 'mods-clocks' node for dev %s\n",
				__clk_get_name(pclk));
			goto err;
		}

		clk_name = __clk_get_name(pclk);

		prst = of_reset_control_get(mods_np, clk_name);
		if (IS_ERR(prst)) {
			cl_error("reset device %s not found\n", clk_name);
			goto err;
		}
		ret = reset_control_deassert(prst);
		if (ret) {
			cl_error("failed to assert reset on '%s'\n", clk_name);
		} else {
			cl_debug(DEBUG_CLOCK, "deasserted reset on '%s'",
				 clk_name);
		}

err:
		of_node_put(mods_np);
	}

	LOG_EXT();
	return ret;
}
