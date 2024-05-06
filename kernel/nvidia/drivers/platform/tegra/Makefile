# Copyright (c) 2014-2023, NVIDIA CORPORATION. All rights reserved.

KBUILD_CFLAGS += -I$(srctree)/arch/arm/mach-tegra \
		 -I$(srctree)/arch/arm/mach-tegra/include/ \
		 -I$(srctree)/drivers/platform/tegra/ \
		 -I$(srctree.nvidia)/drivers/platform/tegra/rtcpu \
		 -I$(srctree.nvidia)/drivers/video/tegra/dc/ \
		 -I$(srctree.nvidia)/drivers/video/tegra/dc/panel/

ccflags-y += -Werror
obj-$(CONFIG_TEGRA_MCE) += mce/
obj-$(CONFIG_NV_TEGRA_MC) += mc/
obj-$(CONFIG_TEGRA_CBB_NOC) += cbb/
obj-$(CONFIG_TEGRA_DCE) += dce/
obj-$(CONFIG_TEGRA_SPE) += aon/
obj-$(CONFIG_TEGRA_PSC) += psc/
ifeq ($(NV_BUILD_CONFIGURATION_IS_SAFETY),1)
obj-$(CONFIG_TEGRA_SOC_HWPM) += tegra_hwpm.o
else
obj-$(CONFIG_TEGRA_SOC_HWPM) += hwpm/
endif
obj-$(CONFIG_ARCH_TEGRA_18x_SOC) += tegra18_speedo.o
obj-$(CONFIG_TEGRA_DC) += panel-simulator.o
obj-$(CONFIG_ARCH_TEGRA_18x_SOC) += tegra-roc.o
obj-$(CONFIG_TEGRA_PMC_AO_WAKE) += tegra186-aowake.o
obj-$(CONFIG_ARCH_TEGRA_18x_SOC) += tegra186-system-config.o
obj-$(CONFIG_TEGRA_CAMERA_RTCPU) += tegra-camera-rtcpu.o
obj-$(CONFIG_TEGRA_CAMERA_RTCPU) += rtcpu/
obj-$(CONFIG_TEGRA_PM_IRQ) += pm-irq-t18x.o
obj-$(CONFIG_TEGRA_WAKEUP) += wakeups-t18x.o
obj-$(CONFIG_TEGRA_CENTRAL_ACTMON) += central_actmon/
obj-y += powergate/
obj-$(CONFIG_TEGRA_AON) += tegra-aon.o
obj-$(CONFIG_TEGRA_AON) += tegra-aon-ivc-echo.o
obj-$(CONFIG_ARCH_TEGRA_18x_SOC) += tegra18_cpu_debugfs.o
obj-$(CONFIG_TEGRA_PM_DEBUG) += pm.o
obj-$(CONFIG_TEGRA_PM_DEBUG) += pm_debugfs.o
ifneq ($(filter y,$(CONFIG_ARCH_TEGRA_18x_SOC) $(CONFIG_ARCH_TEGRA_186_SOC)),)
obj-y += reboot-t18x.o
endif
ifneq ($(filter y,$(CONFIG_ARCH_TEGRA_18x_SOC) $(CONFIG_ARCH_TEGRA_186_SOC)),)
obj-y += pm-tegra186.o
endif
obj-$(CONFIG_TEGRA_AON) += tegra186_aondbg.o
obj-$(CONFIG_TEGRA_ARI_MCA) += ari_mca.o
obj-$(CONFIG_TEGRA_18X_SERROR) += tegra18_serr.o
obj-$(CONFIG_TEGRA_BRIDGE_MCA) += bridge_mca.o
obj-$(CONFIG_TEGRA_18X_SERROR) += tegra18_a57_serr.o
obj-$(CONFIG_TEGRA_CENTRAL_ACTMON) += cactmon.o
obj-$(CONFIG_TEGRA_CENTRAL_ACTMON) += cactmon_t21x.o
ifeq ($(CONFIG_HW_PERF_EVENTS), y)
obj-$(CONFIG_ARCH_TEGRA_18x_SOC) += tegra18_perf_uncore.o
endif
obj-$(CONFIG_TEGRA_PTP_NOTIFIER) += ptp-notifier.o
ifeq ($(CONFIG_DEBUG_FS), y)
obj-$(CONFIG_DENVER_CPU) += denver-knobs.o
endif
obj-$(CONFIG_DENVER_MCA) += denver_mca.o
obj-$(CONFIG_TEGRA_HV_XHCI_DEBUG) += tegra-hv-xhci-debug.o
obj-$(CONFIG_TEGRA_SAFETY) += tegra-safety-ivc.o tegra-safety-dev.o
obj-$(CONFIG_TEGRA_SAFETY) += tegra_l1ss_kernel_interface.o tegra_l1ss_cmd_resp_l2_interface.o ArLib.o tegra_l1ss.o

obj-$(CONFIG_TEGRA_NVADSP) += nvadsp/

obj-$(CONFIG_TEGRA_VIRT_AUDIO_IVC) += nvaudio_ivc/

ifeq ($(CONFIG_TEGRA_VPR),y)
obj-y += tegra_vpr_resize.o
obj-y += smc-calls.o
else
obj-y += tegra_vpr.o
endif

ifeq ($(CONFIG_CPU_FREQ),y)
obj-$(CONFIG_ARCH_TEGRA_21x_SOC) += cpu_emc.o
endif

ifdef CONFIG_ARCH_TEGRA_194_SOC
obj-y += cvnas/
endif

obj-$(CONFIG_TEGRA_CACHE) += cache/
ifdef CONFIG_ARCH_TEGRA_19x_SOC
obj-y += cvnas/
obj-$(CONFIG_TEGRA_19X_RAS) += carmel_ras.o
ifeq ($(CONFIG_HW_PERF_EVENTS), y)
obj-$(CONFIG_ARCH_TEGRA_19x_SOC) += tegra19_perf_uncore.o
endif

endif

ifdef CONFIG_ARCH_TEGRA_23x_SOC
obj-$(CONFIG_HW_PERF_EVENTS) += uncore_pmu/
endif

obj-$(CONFIG_ARCH_TEGRA_210_SOC) += mselect.o
obj-$(CONFIG_TEGRA_HSP) += tegra-hsp.o
obj-$(CONFIG_TEGRA_HSP) += tegra186-hsp.o
obj-$(CONFIG_TEGRA_WAKEUP) += tegra-wakeups.o

obj-$(CONFIG_TEGRA_FIRMWARES_CLASS)    += firmwares.o
obj-$(CONFIG_TEGRA_FIRMWARES_INVENTORY)    += firmwares-all.o
obj-$(CONFIG_NV_TEGRA_IVC)		+= tegra-ivc.o
obj-$(CONFIG_TEGRA_FIQ_DEBUGGER)        += tegra_fiq_debugger.o

obj-$(CONFIG_TEGRA_BOOTLOADER_DEBUG)    += tegra_bootloader_debug.o
obj-$(CONFIG_TEGRA_BOOTLOADER_DEBUG_INIT) += tegra_bootloader_debug_init.o
obj-$(CONFIG_TEGRA_BOOTLOADER_BOOT_CFG)	+= bcp.o

obj-$(CONFIG_TEGRA_WDT_RECOVERY)        += wdt-recovery.o
obj-y					+= tegra_prod.o
obj-m 					+= bad.o
obj-$(CONFIG_TEGRA_CLOCKS_CONFIGURE)	+= clocks-config.o
obj-$(CONFIG_TEGRA_FSICOM)		+= tegra-fsicom.o
obj-$(CONFIG_TEGRA_EPL)			+= tegra-epl.o
obj-$(CONFIG_TEGRA_USS_IO_PROXY)	+= tegra-uss-io-proxy.o
obj-$(CONFIG_TEGRA_HSIERRRPTINJ)	+= tegra-hsierrrptinj.o
