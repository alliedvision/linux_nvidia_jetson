subdir-ccflags-y += -Werror

ifeq ($(CONFIG_NV_TEGRA_BPMP),y)
obj-y += tegra-bpmp-dvfs.o
obj-$(CONFIG_DEBUG_FS) += tegra-bpmp-ec.o
endif
obj-$(CONFIG_ARCH_TEGRA_21x_SOC)	+= pm-tegra210.o
obj-$(CONFIG_TEGRA_PROC_POWER_MODEL)	+= tegra-ppm.o

obj-$(CONFIG_TEGRA_DVFS)       += tegra-dvfs.o
obj-$(CONFIG_TEGRA_210_DVFS)   += tegra210-dvfs.o
