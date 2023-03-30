# Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
# Uncore PMU driver code.

GCOV_PROFILE := y

ccflags-y += -I$(srctree)/drivers/platform/tegra/include \
							-I$(srctree.nvidia)/drivers/platform/tegra/

ccflags-y += -Werror

obj-$(CONFIG_ARCH_TEGRA_23x_SOC) += tegra23x_perf_uncore.o
