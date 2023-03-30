# SPDX-License-Identifier: GPL-2.0
#
# Tegra Host1x-Nvhost Driver.
#
# Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
#

ccflags-y += -I$(srctree.nvidia)/include
ccflags-y += -I$(srctree.nvidia)/include/uapi/linux
ccflags-y += -I$(srctree.nvidia)/drivers/gpu/host1x/include
ccflags-y += -DCONFIG_TEGRA_HOST1X
ccflags-y += -Werror

host1x-nvhost-objs = nvhost.o falcon.o

obj-m += host1x-nvhost.o
