GCOV_PROFILE := y

# Set config to build as module for OOT build
ifeq ($(CONFIG_TEGRA_OOT_MODULE),m)
CONFIG_TEGRA_GRHOST_NVDLA := m
CONFIG_TEGRA_HOST1X_NEXT := m
CONFIG_TEGRA_FUSE_UPSTREAM := y
endif

ifdef CONFIG_TEGRA_GRHOST
ccflags-y += -I$(srctree.nvidia)/drivers/video/tegra/host
ccflags-y += -DCONFIG_TEGRA_NVDLA_CHANNEL
endif

ccflags-y += -I$(srctree.nvidia)/include
ccflags-y += -I$(srctree.nvidia)/include/uapi/linux
ccflags-y += -I$(srctree.nvidia)/drivers/video/tegra/host/nvdla
ccflags-y += -Werror

# When using the upstream host1x driver, the Makefile must define the
# srctree.host1x path in order to find the necessary header files for
# the upstream host1x driver.
ifdef CONFIG_TEGRA_HOST1X_NEXT
ccflags-y += -I$(srctree.host1x)/include
ccflags-y += -DCONFIG_TEGRA_HOST1X
endif

ifeq ($(CONFIG_TEGRA_FUSE_UPSTREAM),y)
ccflags-y += -DCONFIG_TEGRA_FUSE_UPSTREAM
endif

nvhost-nvdla-objs  = \
		nvdla.o \
		nvdla_buffer.o \
		nvdla_ioctl.o \
		dla_queue.o \
		nvdla_queue.o \
		nvdla_debug.o

ifdef CONFIG_TEGRA_GRHOST
nvhost-nvdla-objs += dla_channel.o
endif

obj-$(CONFIG_TEGRA_GRHOST_NVDLA) += nvhost-nvdla.o
