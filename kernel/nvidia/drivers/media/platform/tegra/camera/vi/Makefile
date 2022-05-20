GCOV_PROFILE := y
ccflags-y += -I$(srctree.nvidia)/drivers/video/tegra/host
ccflags-y += -I$(srctree)/drivers/video/tegra/host
ccflags-y += -I$(srctree.nvidia)/drivers/video/tegra/camera
ccflags-y += -I$(srctree.nvidia)/drivers/media/platform/tegra
ccflags-y += -I$(srctree.nvidia)/include
ccflags-y += -I$(srctree.nvidia)/drivers/video/tegra/host/
ccflags-y += -I$(srctree.nvidia)/include
ccflags-y += -Werror

ifneq ($(filter y,$(CONFIG_ARCH_TEGRA_18x_SOC) $(CONFIG_ARCH_TEGRA_186_SOC)),)
obj-y += mc_common.o core.o channel.o graph.o vi2_fops.o vi4_fops.o
endif

ifeq ($(CONFIG_ARCH_TEGRA_210_SOC),y)
ccflags-y += -I$(srctree.nvidia)/drivers/video/tegra/host/
obj-y += mc_common.o core.o channel.o graph.o vi2_fops.o
endif

ifneq ($(filter y,$(CONFIG_ARCH_TEGRA_19x_SOC) $(CONFIG_ARCH_TEGRA_194_SOC)),)

ccflags-y += -I$(srctree.nvidia)/drivers/video/tegra/camera
ccflags-y += -I$(srctree.nvidia)/drivers/media/platform/tegra
ccflags-y += -I$(srctree.nvidia)/drivers/video/tegra/host
ccflags-y += -I$(srctree.nvidia)/include

obj-y += vi5_fops.o
endif
