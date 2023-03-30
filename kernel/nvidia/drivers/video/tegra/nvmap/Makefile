GCOV_PROFILE := y

subdir-ccflags-y := -Werror

include $(srctree.nvidia)/drivers/video/tegra/nvmap/Makefile.memory.configs

ccflags-y += -I$(srctree.nvidia)/include/

ifeq ($(NVMAP_CONFIG), y)
# Build NvMap only when NVMAP_CONFIG is set to y
nvmap-y := nvmap_core.o \
	nvmap_alloc.o \
	nvmap_cache.o \
	nvmap_dev.o \
	nvmap_dmabuf.o \
	nvmap_fault.o \
	nvmap_handle.o \
	nvmap_heap.o \
	nvmap_ioctl.o \
	nvmap_init.o \
	nvmap_tag.o \
	nvmap_mm.o \
	nvmap_stats.o \
	nvmap_carveout.o \
	nvmap_kasan_wrapper.o
nvmap-$(NVMAP_CONFIG_HANDLE_AS_ID) += nvmap_id_array.o

nvmap-$(NVMAP_CONFIG_SCIIPC) += nvmap_sci_ipc.o

ifeq ($(NVMAP_CONFIG_PAGE_POOLS), y)
nvmap-y += nvmap_pp.o
endif #NVMAP_CONFIG_PAGE_POOLS

ifneq ($(filter y,$(CONFIG_ARCH_TEGRA_19x_SOC) $(CONFIG_ARCH_TEGRA_194_SOC)),)
nvmap-y += nvmap_init_t19x.o
nvmap-y += nvmap_dmabuf_t19x.o
nvmap-y += nvmap_cache_t19x.o
endif #CONFIG_ARCH_TEGRA_19x_SOC or CONFIG_ARCH_TEGRA_194_SOC

KASAN_SANITIZE_nvmap_kasan_wrapper.o := n

ifeq ($(NVMAP_CONFIG_LOADABLE_MODULE), y)
nvmap-y += nvmap_cache_maint.o
obj-m += nvmap.o
else
# Not a loadable module
obj-y += nvmap.o
endif #NVMAP_CONFIG_LOADABLE_MODULE
endif #NVMAP_CONFIG
