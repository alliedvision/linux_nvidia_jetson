# SPDX-License-Identifier: GPL-2.0
#
# Tegra CVNAS Driver.
#
# Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
#

ccflags-y += -I$(srctree.nvidia)/include
ccflags-y += -Werror

# Set config when build as OOT module.
ifeq ($(CONFIG_TEGRA_OOT_MODULE),m)
CONFIG_TEGRA_FUSE_UPSTREAM := y
CONFIG_TEGRA_CVNAS := m
endif

ifeq ($(CONFIG_TEGRA_FUSE_UPSTREAM),y)
ccflags-y += -DCONFIG_TEGRA_FUSE_UPSTREAM
endif

obj-$(CONFIG_TEGRA_CVNAS) += cvnas.o
ifeq ($(CONFIG_TEGRA_CVNAS),m)
ccflags-y += -DCVNAS_MODULE
endif
