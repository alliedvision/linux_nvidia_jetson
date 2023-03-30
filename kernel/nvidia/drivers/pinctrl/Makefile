# SPDX-License-Identifier: GPL-2.0
GCOV_PROFILE_pinctrl-tegra186-padctl.o := y

ccflags-y += -Werror
ccflags-y += -I$(srctree)/drivers/pinctrl
ccflags-y += -I$(srctree)/drivers/pinctrl/tegra
ccflags-y += -I$(srctree)/drivers/video/tegra/host
ccflags-y += -I$(srctree.nvidia)/drivers/video/tegra/host
ccflags-y += -I$(srctree.nvidia)/include

ifdef CONFIG_ARCH_TEGRA_18x_SOC

obj-$(CONFIG_PINCTRL_TEGRA186)  += pinctrl-tegra186.o

endif
obj-$(CONFIG_PINCTRL_TEGRA186_PADCTL) += pinctrl-tegra186-padctl.o
obj-$(CONFIG_PINCTRL_TEGRA186_DPAUX)  += pinctrl-tegra186-dpaux.o

ifdef CONFIG_ARCH_TEGRA_19x_SOC

GCOV_PROFILE_pinctrl-tegra194-padctl.o := y
obj-$(CONFIG_PINCTRL_TEGRA194)  += pinctrl-tegra194.o
obj-$(CONFIG_PINCTRL_TEGRA194_PEXCLK_PADCTL) += pinctrl-tegra194-pexclk-padctrl.o

endif

ifdef CONFIG_ARCH_TEGRA_23x_SOC

obj-$(CONFIG_PINCTRL_TEGRA234_DPAUX)  += pinctrl-tegra234-dpaux.o

endif
