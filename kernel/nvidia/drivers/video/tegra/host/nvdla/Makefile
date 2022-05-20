GCOV_PROFILE := y
ccflags-y += -I$(srctree.nvidia)/drivers/video/tegra/host
ccflags-y += -I$(srctree.nvidia)/include
ccflags-y += -I$(srctree.nvidia)/include/uapi/linux
ccflags-y += -I$(srctree.nvidia)/drivers/video/tegra/host/nvdla
ccflags-y += -Werror

nvhost-nvdla-objs  = \
		nvdla.o \
		nvdla_buffer.o \
		nvdla_ioctl.o \
		dla_queue.o \
		nvdla_queue.o \
		nvdla_debug.o

obj-$(CONFIG_TEGRA_GRHOST_NVDLA) += nvhost-nvdla.o
