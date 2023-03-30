# SPDX-License-Identifier: GPL-2.0

ccflags-y += -Werror
ccflags-y += -I$(srctree)/drivers/pinctrl
ccflags-y += -I$(srctree)/drivers/pinctrl/tegra

ifdef CONFIG_ARCH_TEGRA_23x_SOC
obj-$(CONFIG_PINCTRL_TEGRA234)		+= pinctrl-tegra234.o
endif
