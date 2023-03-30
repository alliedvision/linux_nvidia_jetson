#
# Memory controller code.
#

GCOV_PROFILE := y

ccflags-y += -I$(srctree)/arch/arm/mach-tegra/include \
             -I$(srctree)/arch/arm/mach-tegra \
             -I$(srctree)/drivers/platform/tegra/include

obj-y                                   += mc.o

obj-y                                   += mcerr.o
obj-y                                   += mcerr-t21.o

obj-y                                   += latency_allowance.o
obj-y                                   += tegra21x_la.o

obj-y                                   += fixed_point.o
obj-$(CONFIG_TEGRA_ISOMGR)              += isomgr.o isomgr-pre_t19x.o isomgr-t19x.o
obj-$(CONFIG_TEGRA_BWMGR)               += emc_bwmgr.o
obj-$(CONFIG_TEGRA_BWMGR)               += emc_bwmgr-t21x.o emc_bwmgr-t18x.o emc_bwmgr-t19x.o

obj-y                                   += tegra-mc-sid.o

ifneq (,$(filter y,$(CONFIG_ARCH_TEGRA_18x_SOC) $(CONFIG_ARCH_TEGRA_186_SOC)))
obj-y += mcerr_ecc_t18x.o
obj-y += mc_addr_translate.o
obj-y += mcerr-t18x.o
obj-y += tegra18_emc.o
obj-y += tegra18x_la.o
obj-y += tegra186-mc-sid.o
endif

obj-y					+= tegra_fd.o

ifneq (,$(filter y,$(CONFIG_ARCH_TEGRA_19x_SOC) $(CONFIG_ARCH_TEGRA_194_SOC)))
ccflags-y += -Werror

obj-y += tegra19x_la_ptsa_core.o tegra19x_la_ptsa.o
obj-y += tegra194-mc-sid.o
obj-y += mcerr-t19x.o

endif

ifeq ($(CONFIG_ARCH_TEGRA_23x_SOC),y)
obj-y += tegra234-mc-sid.o
obj-y += mcerr-t23x.o
obj-y += mc_utils.o
endif

ifeq ($(CONFIG_TEGRA_SOC_HWPM),y)
obj-y += mc-hwpm.o
endif
