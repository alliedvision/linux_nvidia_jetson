/*
 * Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
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

#include <linux/hashtable.h>

#include <nvgpu/kmem.h>
#include <nvgpu/bug.h>
#include <nvgpu/hashtable.h>

#include "os_linux.h"

#include "gk20a/gk20a.h"

#include "platform_gk20a.h"
#include "platform_gk20a_tegra.h"
#include "platform_gp10b.h"
#include "platform_gp10b_tegra.h"
#include "platform_ecc_sysfs.h"

static u32 gen_ecc_hash_key(char *str)
{
	int i = 0;
	u32 hash_key = 0x811c9dc5;

	while (str[i]) {
		hash_key *= 0x1000193;
		hash_key ^= (u32)(str[i]);
		i++;
	};

	return hash_key;
}

static ssize_t ecc_stat_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	const char *ecc_stat_full_name = attr->attr.name;
	const char *ecc_stat_base_name;
	unsigned int hw_unit;
	unsigned int subunit;
	struct gk20a_ecc_stat *ecc_stat;
	u32 hash_key;
	struct gk20a *g = get_gk20a(dev);
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);

	if (sscanf(ecc_stat_full_name, "ltc%u_lts%u", &hw_unit,
							&subunit) == 2) {
		ecc_stat_base_name = &(ecc_stat_full_name[strlen("ltc0_lts0_")]);
		hw_unit = g->gr.slices_per_ltc * hw_unit + subunit;
	} else if (sscanf(ecc_stat_full_name, "ltc%u", &hw_unit) == 1) {
		ecc_stat_base_name = &(ecc_stat_full_name[strlen("ltc0_")]);
	} else if (sscanf(ecc_stat_full_name, "gpc0_tpc%u", &hw_unit) == 1) {
		ecc_stat_base_name = &(ecc_stat_full_name[strlen("gpc0_tpc0_")]);
	} else if (sscanf(ecc_stat_full_name, "gpc%u", &hw_unit) == 1) {
		ecc_stat_base_name = &(ecc_stat_full_name[strlen("gpc0_")]);
	} else if (sscanf(ecc_stat_full_name, "eng%u", &hw_unit) == 1) {
		ecc_stat_base_name = &(ecc_stat_full_name[strlen("eng0_")]);
	} else {
		return snprintf(buf,
				PAGE_SIZE,
				"Error: Invalid ECC stat name!\n");
	}

	hash_key = gen_ecc_hash_key((char *)ecc_stat_base_name);

	hash_for_each_possible(l->ecc_sysfs_stats_htable,
				ecc_stat,
				hash_node,
				hash_key) {
		if (hw_unit >= ecc_stat->count)
			continue;
		if (!strcmp(ecc_stat_full_name, ecc_stat->names[hw_unit]))
			return snprintf(buf, PAGE_SIZE, "%u\n", ecc_stat->counters[hw_unit]);
	}

	return snprintf(buf, PAGE_SIZE, "Error: No ECC stat found!\n");
}

int nvgpu_gr_ecc_stat_create(struct device *dev,
			     int is_l2, char *ecc_stat_name,
			     struct gk20a_ecc_stat *ecc_stat)
{
	struct gk20a *g = get_gk20a(dev);
	char *ltc_unit_name = "ltc";
	char *gr_unit_name = "gpc0_tpc";
	char *lts_unit_name = "lts";
	int num_hw_units = 0;
	int num_subunits = 0;

	if (is_l2 == 1)
		num_hw_units = g->ltc_count;
	else if (is_l2 == 2) {
		num_hw_units = g->ltc_count;
		num_subunits = g->gr.slices_per_ltc;
	} else
		num_hw_units = g->gr.tpc_count;


	return nvgpu_ecc_stat_create(dev, num_hw_units, num_subunits,
				is_l2 ? ltc_unit_name : gr_unit_name,
				num_subunits ? lts_unit_name: NULL,
				ecc_stat_name,
				ecc_stat);
}

int nvgpu_ecc_stat_create(struct device *dev,
			  int num_hw_units, int num_subunits,
			  char *ecc_unit_name, char *ecc_subunit_name,
			  char *ecc_stat_name,
			  struct gk20a_ecc_stat *ecc_stat)
{
	int error = 0;
	struct gk20a *g = get_gk20a(dev);
	struct nvgpu_os_linux *l = nvgpu_os_linux_from_gk20a(g);
	int hw_unit = 0;
	int subunit = 0;
	int element = 0;
	u32 hash_key = 0;
	struct device_attribute *dev_attr_array;

	int num_elements = num_subunits ? num_subunits * num_hw_units :
		num_hw_units;

	/* Allocate arrays */
	dev_attr_array = nvgpu_kzalloc(g, sizeof(struct device_attribute) *
				       num_elements);
	ecc_stat->counters = nvgpu_kzalloc(g, sizeof(u32) * num_elements);
	ecc_stat->names = nvgpu_kzalloc(g, sizeof(char *) * num_elements);

	for (hw_unit = 0; hw_unit < num_elements; hw_unit++) {
		ecc_stat->names[hw_unit] = nvgpu_kzalloc(g, sizeof(char) *
						ECC_STAT_NAME_MAX_SIZE);
	}
	ecc_stat->count = num_elements;
	if (num_subunits) {
		for (hw_unit = 0; hw_unit < num_hw_units; hw_unit++) {
			for (subunit = 0; subunit < num_subunits; subunit++) {
				element = hw_unit*num_subunits + subunit;

				snprintf(ecc_stat->names[element],
					ECC_STAT_NAME_MAX_SIZE,
					"%s%d_%s%d_%s",
					ecc_unit_name,
					hw_unit,
					ecc_subunit_name,
					subunit,
					ecc_stat_name);

				sysfs_attr_init(&dev_attr_array[element].attr);
				dev_attr_array[element].attr.name =
					ecc_stat->names[element];
				dev_attr_array[element].attr.mode =
					VERIFY_OCTAL_PERMISSIONS(S_IRUGO);
				dev_attr_array[element].show = ecc_stat_show;
				dev_attr_array[element].store = NULL;

				/* Create sysfs file */
				error |= device_create_file(dev,
						&dev_attr_array[element]);

			}
		}
	} else {
		for (hw_unit = 0; hw_unit < num_hw_units; hw_unit++) {

			/* Fill in struct device_attribute members */
			snprintf(ecc_stat->names[hw_unit],
				ECC_STAT_NAME_MAX_SIZE,
				"%s%d_%s",
				ecc_unit_name,
				hw_unit,
				ecc_stat_name);

			sysfs_attr_init(&dev_attr_array[hw_unit].attr);
			dev_attr_array[hw_unit].attr.name =
						ecc_stat->names[hw_unit];
			dev_attr_array[hw_unit].attr.mode =
					VERIFY_OCTAL_PERMISSIONS(S_IRUGO);
			dev_attr_array[hw_unit].show = ecc_stat_show;
			dev_attr_array[hw_unit].store = NULL;

			/* Create sysfs file */
			error |= device_create_file(dev,
					&dev_attr_array[hw_unit]);
		}
	}

	/* Add hash table entry */
	hash_key = gen_ecc_hash_key(ecc_stat_name);
	hash_add(l->ecc_sysfs_stats_htable,
		&ecc_stat->hash_node,
		hash_key);

	ecc_stat->attr_array = dev_attr_array;

	return error;
}

void nvgpu_gr_ecc_stat_remove(struct device *dev,
			      int is_l2, struct gk20a_ecc_stat *ecc_stat)
{
	struct gk20a *g = get_gk20a(dev);
	int num_hw_units = 0;
	int num_subunits = 0;

	if (is_l2 == 1)
		num_hw_units = g->ltc_count;
	else if (is_l2 == 2) {
		num_hw_units = g->ltc_count;
		num_subunits = g->gr.slices_per_ltc;
	} else
		num_hw_units = g->gr.tpc_count;

	nvgpu_ecc_stat_remove(dev, num_hw_units, num_subunits, ecc_stat);
}

void nvgpu_ecc_stat_remove(struct device *dev,
			   int num_hw_units, int num_subunits,
			   struct gk20a_ecc_stat *ecc_stat)
{
	struct gk20a *g = get_gk20a(dev);
	struct device_attribute *dev_attr_array = ecc_stat->attr_array;
	int hw_unit = 0;
	int subunit = 0;
	int element = 0;
	int num_elements = num_subunits ? num_subunits * num_hw_units :
		num_hw_units;

	/* Remove sysfs files */
	if (num_subunits) {
		for (hw_unit = 0; hw_unit < num_hw_units; hw_unit++) {
			for (subunit = 0; subunit < num_subunits; subunit++) {
				element = hw_unit * num_subunits + subunit;

				device_remove_file(dev,
						   &dev_attr_array[element]);
			}
		}
	} else {
		for (hw_unit = 0; hw_unit < num_hw_units; hw_unit++)
			device_remove_file(dev, &dev_attr_array[hw_unit]);
	}

	/* Remove hash table entry */
	hash_del(&ecc_stat->hash_node);

	/* Free arrays */
	nvgpu_kfree(g, ecc_stat->counters);

	for (hw_unit = 0; hw_unit < num_elements; hw_unit++)
		nvgpu_kfree(g, ecc_stat->names[hw_unit]);

	nvgpu_kfree(g, ecc_stat->names);
	nvgpu_kfree(g, dev_attr_array);
}
