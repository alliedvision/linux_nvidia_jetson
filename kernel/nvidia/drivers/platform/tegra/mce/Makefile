#
# MCE Driver code.
#

GCOV_PROFILE := y

ccflags-y += -I$(srctree)/drivers/platform/tegra/include

ccflags-y += -Werror

obj-$(CONFIG_TEGRA_MCE) += tegra-mce.o

ifneq (,$(filter y,$(CONFIG_ARCH_TEGRA_18x_SOC) $(CONFIG_ARCH_TEGRA_186_SOC)))
obj-y += tegra18x-mce.o
endif
ifneq (,$(filter y,$(CONFIG_ARCH_TEGRA_19x_SOC) $(CONFIG_ARCH_TEGRA_194_SOC) $(CONFIG_ARCH_TEGRA_23x_SOC)))
obj-y += tegra19x-mce.o
endif
obj-$(CONFIG_ARCH_TEGRA_23x_SOC) += tegra23x-mce.o
