/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef __NVGPU_IOCTL_H__
#define __NVGPU_IOCTL_H__

#include <linux/cdev.h>

#include <nvgpu/types.h>
#include <nvgpu/list.h>
#include <nvgpu/mig.h>

struct device;
struct class;

struct nvgpu_class {
	struct class *class;
	struct nvgpu_list_node list_entry;

	struct nvgpu_cdev_class_priv_data *priv_data;

	enum nvgpu_mig_gpu_instance_type instance_type;
	bool power_node;
};

static inline struct class *nvgpu_class_get_class(struct nvgpu_class *class)
{
	return class->class;
}

struct nvgpu_cdev {
	struct cdev cdev;
	struct device *node;
	struct nvgpu_class *class;
	struct nvgpu_list_node list_entry;
};

static inline struct nvgpu_cdev *
nvgpu_cdev_from_list_entry(struct nvgpu_list_node *node)
{
	return (struct nvgpu_cdev *)
		((uintptr_t)node - offsetof(struct nvgpu_cdev, list_entry));
};

struct nvgpu_cdev_class_priv_data {
	char class_name[64];
	u32 local_instance_id;
	u32 major_instance_id;
	u32 minor_instance_id;
	bool pci;
};

static inline struct nvgpu_class *
nvgpu_class_from_list_entry(struct nvgpu_list_node *node)
{
	return (struct nvgpu_class *)
		((uintptr_t)node - offsetof(struct nvgpu_class, list_entry));
};

int gk20a_user_nodes_init(struct device *dev);
int gk20a_power_node_init(struct device *dev);
void gk20a_user_nodes_deinit(struct device *dev);
void gk20a_power_node_deinit(struct device *dev);

unsigned int nvgpu_allocate_cdev_minor(struct gk20a *g);
struct gk20a *nvgpu_get_gk20a_from_cdev(struct nvgpu_cdev *cdev);
u32 nvgpu_get_gpu_instance_id_from_cdev(struct gk20a *g, struct nvgpu_cdev *cdev);

int nvgpu_create_device(
	struct device *dev, int devno,
	const char *cdev_name,
	struct cdev *cdev, struct device **out,
	struct nvgpu_class *class);
struct nvgpu_class *nvgpu_get_v2_user_class(struct gk20a *g);

#endif
