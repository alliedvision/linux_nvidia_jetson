#
# Tegra SOC HWPM
#

GCOV_PROFILE := y

ccflags-y += -I$(srctree.nvidia)/drivers/platform/tegra/hwpm
ccflags-y += -I$(srctree.nvidia)/include

obj-y += tegra-soc-hwpm.o
obj-y += tegra-soc-hwpm-io.o
obj-y += tegra-soc-hwpm-ioctl.o
obj-y += tegra-soc-hwpm-log.o
obj-y += tegra-soc-hwpm-ip.o
obj-y += hal/tegra_soc_hwpm_init.o
obj-y += hal/t234/t234_soc_hwpm_init.o
obj-y += hal/t234/t234_soc_hwpm_mem_buf_utils.o
obj-y += hal/t234/t234_soc_hwpm_resource_utils.o
obj-$(CONFIG_DEBUG_FS) += tegra-soc-hwpm-debugfs.o
