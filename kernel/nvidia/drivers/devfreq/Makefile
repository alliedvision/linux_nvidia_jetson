ccflags-y += -I$(srctree)/drivers/devfreq

obj-$(CONFIG_DEVFREQ_GOV_WMARK_SIMPLE)  += governor_wmark_simple.o
obj-$(CONFIG_DEVFREQ_GOV_WMARK_ACTIVE)  += governor_wmark_active.o

ifeq ($(CONFIG_TEGRA_OOT_MODULE),m)
	ccflags-y += -I$(srctree.nvidia)/include
	ccflags-y += -DGOVERNOR_POD_SCALING_V2_MODULE
	obj-m   += governor_pod_scaling_v2.o
else
	obj-$(CONFIG_DEVFREQ_GOV_POD_SCALING)   += governor_pod_scaling.o
	obj-$(CONFIG_DEVFREQ_GOV_POD_SCALING_V2)   += governor_pod_scaling_v2.o
endif
