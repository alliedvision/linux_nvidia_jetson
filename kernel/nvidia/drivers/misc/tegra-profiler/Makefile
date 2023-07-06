#
# drivers/misc/tegra-profiler/Makefile
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU General Public License,
# version 2, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# Copyright (c) 2015-2023, NVIDIA CORPORATION.  All rights reserved.
#

KBUILD_CFLAGS += -I$(srctree.nvidia)/drivers/platform/tegra

ccflags-y += -Werror

obj-$(CONFIG_TEGRA_PROFILER) := tegra-profiler.o

tegra-profiler-y := \
	main.o \
	hrt.o \
	comm.o \
	mmap.o \
	backtrace.o \
	ma.o \
	power_clk.o \
	auth.o \
	quadd_proc.o \
	eh_unwind.o \
	dwarf_unwind.o \
	disassembler.o \
	uncore_events.o

tegra-profiler-$(CONFIG_ARM) += armv7_pmu.o
tegra-profiler-$(CONFIG_ARM64) += armv8_pmu.o

ifneq (,$(filter y,$(CONFIG_ARCH_TEGRA_19x_SOC) $(CONFIG_ARCH_TEGRA_194_SOC)))
tegra-profiler-y += carmel_pmu.o
endif

ifneq (,$(filter y,$(CONFIG_ARCH_TEGRA_23x_SOC) $(CONFIG_ARCH_TEGRA_234_SOC)))
tegra-profiler-y += tegra23x_pmu_scf.o
tegra-profiler-$(CONFIG_ARM_DSU_PMU) += tegra23x_pmu_dsu.o
endif
