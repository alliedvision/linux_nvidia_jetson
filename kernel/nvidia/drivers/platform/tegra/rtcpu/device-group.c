/*
 * Copyright (c) 2017-2022, NVIDIA Corporation.  All rights reserved.
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

#include "device-group.h"

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include "drivers/video/tegra/host/nvhost_acm.h"

struct camrtc_device_group {
	struct device *dev;
	char const *names_name;
	int ndevices;
	struct platform_device *devices[];
};

static int get_grouped_device(struct camrtc_device_group *grp,
			struct device *dev, char const *name, int index)
{
	struct device_node *np;
	struct platform_device *pdev;

	np = of_parse_phandle(dev->of_node, name, index);
	if (np == NULL)
		return 0;

	if (!of_device_is_available(np)) {
		dev_info(dev, "%s[%u] is disabled\n", name, index);
		of_node_put(np);
		return 0;
	}

	pdev = of_find_device_by_node(np);
	of_node_put(np);

	if (pdev == NULL) {
		dev_warn(dev, "%s[%u] node has no device\n", name, index);
		return 0;
	}

	grp->devices[index] = pdev;

	return 0;
}

static void camrtc_device_group_release(struct device *dev, void *res)
{
	const struct camrtc_device_group *grp = res;
	int i;

	put_device(grp->dev);

	for (i = 0; i < grp->ndevices; i++)
		platform_device_put(grp->devices[i]);
}

struct camrtc_device_group *camrtc_device_group_get(
	struct device *dev,
	char const *property_name,
	char const *names_property_name)
{
	int index, err;
	struct camrtc_device_group *grp;
	int ndevices;

	if (!dev || !dev->of_node)
		return ERR_PTR(-EINVAL);

	ndevices = of_count_phandle_with_args(dev->of_node,
			property_name, NULL);
	if (ndevices <= 0)
		return ERR_PTR(-ENOENT);

	grp = devres_alloc(camrtc_device_group_release,
			offsetof(struct camrtc_device_group, devices[ndevices]),
			GFP_KERNEL | __GFP_ZERO);
	if (!grp)
		return ERR_PTR(-ENOMEM);

	grp->dev = get_device(dev);
	grp->ndevices = ndevices;
	grp->names_name = names_property_name;

	for (index = 0; index < grp->ndevices; index++) {
		err = get_grouped_device(grp, dev, property_name, index);
		if (err) {
			devres_free(grp);
			return ERR_PTR(err);
		}
	}

	devres_add(dev, grp);
	return grp;
}
EXPORT_SYMBOL(camrtc_device_group_get);

static inline struct platform_device *platform_device_get(
	struct platform_device *pdev)
{
	if (pdev != NULL)
		get_device(&pdev->dev);
	return pdev;
}

struct platform_device *camrtc_device_get_byname(
	struct camrtc_device_group *grp,
	const char *device_name)
{
	int index;

	if (grp == NULL)
		return ERR_PTR(-EINVAL);
	if (grp->names_name == NULL)
		return ERR_PTR(-ENOENT);

	index = of_property_match_string(grp->dev->of_node, grp->names_name,
			device_name);
	if (index < 0)
		return ERR_PTR(-ENODEV);
	if (index >= grp->ndevices)
		return ERR_PTR(-ENODEV);

	return platform_device_get(grp->devices[index]);
}
