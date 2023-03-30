#
# Tegra Cache Driver.
#

GCOV_PROFILE := y

ccflags-y += -I$(srctree)/drivers/platform/tegra/include

ccflags-y += -Werror

obj-y += tegra-cache.o

ifneq ($(filter y,$(CONFIG_ARCH_TEGRA_18x_SOC) $(CONFIG_ARCH_TEGRA_186_SOC)),)
obj-y += tegra18x-cache.o
endif

ifneq ($(filter y,$(CONFIG_ARCH_TEGRA_19x_SOC) $(CONFIG_ARCH_TEGRA_194_SOC) $(CONFIG_ARCH_TEGRA_23x_SOC)),)
obj-y += tegra19x-cache.o
endif
