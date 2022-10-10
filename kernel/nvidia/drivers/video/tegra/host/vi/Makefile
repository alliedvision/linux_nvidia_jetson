GCOV_PROFILE := y

ccflags-y += -I$(srctree.nvidia)/drivers/video/tegra/host
ccflags-y += -I$(srctree.nvidia)/drivers/media/platform/tegra
ccflags-y += -I$(srctree.nvidia)/include

obj-$(CONFIG_VIDEO_TEGRA_VI) += host_vi_notify.o
obj-$(CONFIG_VIDEO_TEGRA_VI) += vi_notify.o

ccflags-y += -Werror

nvhost-vi5-objs = vi5.o

obj-$(CONFIG_VIDEO_TEGRA_VI) += nvhost-vi5.o
