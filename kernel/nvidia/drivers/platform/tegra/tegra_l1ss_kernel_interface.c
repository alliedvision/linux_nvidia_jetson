/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/tegra_l1ss_kernel_interface.h>
#include "tegra_l1ss.h"

DEFINE_MUTEX(l1ss_client_lock);
LIST_HEAD(l1ss_client_list_head);
atomic_t received_ivc_ready;

static const struct of_device_id sce_match[] = {
	{ .compatible = "nvidia,tegra194-safety-ivc", },
	{},
};

void l1ss_set_ivc_ready(void)
{
	atomic_set(&received_ivc_ready, 1);
}

int l1ss_register_client(client_param_t *p)
{
	struct device_node *np = NULL;
	struct l1ss_client_param_node *temp;

	np = of_find_matching_node(NULL, sce_match);
	if (!np || !of_device_is_available(np))
		return -ENODEV;

	temp = kmalloc(sizeof(struct l1ss_client_param_node),
			GFP_KERNEL);
	if (temp == NULL)
		return -ENOMEM;
	temp->p = p;
	INIT_LIST_HEAD(&temp->cli_list);

	mutex_lock(&l1ss_client_lock);
	list_add_tail(&temp->cli_list, &l1ss_client_list_head);
	mutex_unlock(&l1ss_client_lock);

	if (atomic_read(&received_ivc_ready) == 1)
		return L1SS_READY;
	else
		return L1SS_NOT_READY;

}
EXPORT_SYMBOL_GPL(l1ss_register_client);

int l1ss_deregister_client(nv_guard_client_id_t id)
{
	struct l1ss_client_param_node *temp;
	bool found = false;
	int ret = 0;

	mutex_lock(&l1ss_client_lock);
	list_for_each_entry(temp, &l1ss_client_list_head, cli_list) {
		if (temp->p->id == id) {
			found = true;
			list_del(&temp->cli_list);
			kfree(temp);
		}
	}
	mutex_unlock(&l1ss_client_lock);
	if (found == false)
		ret = -EINVAL;

	return ret;
}
EXPORT_SYMBOL_GPL(l1ss_deregister_client);

int l1ss_notify_client(l1ss_cli_callback_param val)
{
	struct l1ss_client_param_node *temp;

	mutex_lock(&l1ss_client_lock);
	list_for_each_entry(temp, &l1ss_client_list_head, cli_list) {
		temp->p->cli_callback(val, temp->p->data);
	}
	mutex_unlock(&l1ss_client_lock);

	return 0;
}
