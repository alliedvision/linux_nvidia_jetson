#
# Always On Sensor Processing Engine code.
#
GCOV_PROFILE := y

ccflags-y += -I$(srctree.nvidia)/drivers/platform/tegra/aon/include
ccflags-y += -I$(srctree)/include

ccflags-y += -Werror

obj-$(CONFIG_TEGRA_SPE) += tegra234-aon.o

tegra234-aon-y += tegra-aon-reset.o
tegra234-aon-y += tegra-aon-reset.o
tegra234-aon-y += tegra-aon-hsp.o
tegra234-aon-y += tegra-aon-ast.o
ifeq ($(CONFIG_TEGRA_SPE_HSP_MBOX_CLIENT), y)
tegra234-aon-y += aon-hsp-mbox-client.o
else
tegra234-aon-y += aon-hsp-combo.o
endif
tegra234-aon-y += tegra-aon-mail.o
ifeq ($(CONFIG_DEBUG_FS), y)
tegra234-aon-y += tegra-aon-debug.o
endif
tegra234-aon-y += tegra-aon-module.o
