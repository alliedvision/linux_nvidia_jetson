#
# Control Backbone Driver code.
#

GCOV_PROFILE := y

ccflags-y += -I$(srctree)/drivers/platform/tegra/include

ccflags-y += -Werror

obj-$(CONFIG_TEGRA_CBB_NOC) += tegra_cbb.o

ifneq ($(filter y,$(CONFIG_ARCH_TEGRA_19x_SOC) $(CONFIG_ARCH_TEGRA_194_SOC)),)
obj-y += tegra19x_cbb.o
endif

obj-$(CONFIG_ARCH_TEGRA_23x_SOC)        += tegra23x_cbb.o
