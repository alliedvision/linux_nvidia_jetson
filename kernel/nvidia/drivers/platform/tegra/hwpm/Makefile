#
# Tegra SOC HWPM
#

GCOV_PROFILE := y

ccflags-y += -I$(srctree.nvidia)/drivers/platform/tegra/hwpm
ccflags-y += -I$(srctree.nvidia)/drivers/platform/tegra/hwpm/include
ccflags-y += -I$(srctree.nvidia)/include

obj-$(CONFIG_DEBUG_FS) += os/linux/tegra_hwpm_debugfs.o
obj-y += os/linux/tegra_hwpm_linux.o
obj-y += os/linux/tegra_hwpm_io.o
obj-y += os/linux/tegra_hwpm_ip.o
obj-y += os/linux/tegra_hwpm_ioctl.o
obj-y += os/linux/tegra_hwpm_log.o

obj-y += common/tegra_hwpm_alist_utils.o
obj-y += common/tegra_hwpm_aperture_utils.o
obj-y += common/tegra_hwpm_ip_utils.o
obj-y += common/tegra_hwpm_mem_buf_utils.o
obj-y += common/tegra_hwpm_regops_utils.o
obj-y += common/tegra_hwpm_resource_utils.o
obj-y += common/tegra_hwpm_init.o

ifeq ($(CONFIG_TEGRA_T234_HWPM),y)
obj-y += hal/t234/t234_hwpm_alist_utils.o
obj-y += hal/t234/t234_hwpm_aperture_utils.o
obj-y += hal/t234/t234_hwpm_interface_utils.o
obj-y += hal/t234/t234_hwpm_ip_utils.o
obj-y += hal/t234/t234_hwpm_mem_buf_utils.o
obj-y += hal/t234/t234_hwpm_regops_allowlist.o
obj-y += hal/t234/t234_hwpm_resource_utils.o

obj-y += hal/t234/ip/pma/t234_hwpm_ip_pma.o
obj-y += hal/t234/ip/rtr/t234_hwpm_ip_rtr.o

#
# Control IP config
# To disable an IP config in compilation, add condition for both
# IP config flag and IP specific .o file.
#

#
# Define a Minimal IP config flag
# Enable only MSS_Channel, NVDLA and PVA IPs
# When CONFIG_TEGRA_HWPM_MINIMAL_IP_ENABLE is set to y.
#
CONFIG_TEGRA_HWPM_MINIMAL_IP_ENABLE=y

ifeq ($(CONFIG_TEGRA_HWPM_MINIMAL_IP_ENABLE),y)
ccflags-y += -DCONFIG_HWPM_ALLOW_FORCE_ENABLE
endif

ifeq ($(CONFIG_TEGRA_GRHOST_NVDLA),y)
ccflags-y += -DCONFIG_SOC_HWPM_IP_NVDLA
obj-y += hal/t234/ip/nvdla/t234_hwpm_ip_nvdla.o
endif

ifeq ($(CONFIG_TEGRA_GRHOST_PVA),y)
ccflags-y += -DCONFIG_SOC_HWPM_IP_PVA
obj-y += hal/t234/ip/pva/t234_hwpm_ip_pva.o
endif

ifeq ($(CONFIG_NV_TEGRA_MC),y)
ccflags-y += -DCONFIG_SOC_HWPM_IP_MSS_CHANNEL
obj-y += hal/t234/ip/mss_channel/t234_hwpm_ip_mss_channel.o
endif

ifneq ($(CONFIG_TEGRA_HWPM_MINIMAL_IP_ENABLE),y)
ccflags-y += -DCONFIG_SOC_HWPM_IP_DISPLAY
obj-y += hal/t234/ip/display/t234_hwpm_ip_display.o

ifeq ($(CONFIG_TEGRA_GRHOST_ISP),y)
ccflags-y += -DCONFIG_SOC_HWPM_IP_ISP
obj-y += hal/t234/ip/isp/t234_hwpm_ip_isp.o
endif

ifeq ($(CONFIG_NVETHERNET),y)
ccflags-y += -DCONFIG_SOC_HWPM_IP_MGBE
obj-y += hal/t234/ip/mgbe/t234_hwpm_ip_mgbe.o
endif

ifeq ($(CONFIG_NV_TEGRA_MC),y)
ccflags-y += -DCONFIG_SOC_HWPM_IP_MSS_ISO_NISO_HUBS
obj-y += hal/t234/ip/mss_iso_niso_hubs/t234_hwpm_ip_mss_iso_niso_hubs.o

ccflags-y += -DCONFIG_SOC_HWPM_IP_MSS_MCF
obj-y += hal/t234/ip/mss_mcf/t234_hwpm_ip_mss_mcf.o
endif

ccflags-y += -DCONFIG_SOC_HWPM_IP_MSS_GPU_HUB
obj-y += hal/t234/ip/mss_gpu_hub/t234_hwpm_ip_mss_gpu_hub.o

ifeq ($(CONFIG_TEGRA_GRHOST_NVDEC),y)
ccflags-y += -DCONFIG_SOC_HWPM_IP_NVDEC
obj-y += hal/t234/ip/nvdec/t234_hwpm_ip_nvdec.o
endif

ifeq ($(CONFIG_TEGRA_GRHOST_NVENC),y)
ccflags-y += -DCONFIG_SOC_HWPM_IP_NVENC
obj-y += hal/t234/ip/nvenc/t234_hwpm_ip_nvenc.o
endif

ifeq ($(CONFIG_TEGRA_GRHOST_OFA),y)
ccflags-y += -DCONFIG_SOC_HWPM_IP_OFA
obj-y += hal/t234/ip/ofa/t234_hwpm_ip_ofa.o
endif

ifeq ($(CONFIG_PCIE_TEGRA194),y)
ccflags-y += -DCONFIG_SOC_HWPM_IP_PCIE
obj-y += hal/t234/ip/pcie/t234_hwpm_ip_pcie.o
endif

ccflags-y += -DCONFIG_SOC_HWPM_IP_SCF
obj-y += hal/t234/ip/scf/t234_hwpm_ip_scf.o

ifeq ($(CONFIG_VIDEO_TEGRA_VI),y)
ccflags-y += -DCONFIG_SOC_HWPM_IP_VI
obj-y += hal/t234/ip/vi/t234_hwpm_ip_vi.o
endif

ifeq ($(CONFIG_TEGRA_GRHOST_VIC),y)
ccflags-y += -DCONFIG_SOC_HWPM_IP_VIC
obj-y += hal/t234/ip/vic/t234_hwpm_ip_vic.o
endif

endif

endif
