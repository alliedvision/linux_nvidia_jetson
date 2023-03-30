subdir-ccflags-y			+= -Werror
KBUILD_CFLAGS += -I$(srctree)/drivers/firmware/tegra

obj-$(CONFIG_NV_TEGRA_BPMP)	:= bpmp.o
bpmp-objs := \
	nv_bpmp.o \
	bpmp_tty.o \
	mail.o \
	mail_t186.o \
	mail_t210.o \
	mailman.o
bpmp-$(CONFIG_DEBUG_FS)		+= bpmp_t210.o
bpmp-$(CONFIG_DEBUG_FS)		+= fwdebug.o

obj-$(CONFIG_ARCH_TEGRA_186_SOC)	+= bpmp-tegra186-hv.o
obj-$(CONFIG_ARCH_TEGRA_194_SOC)	+= bpmp-tegra186-hv.o

GCOV_PROFILE				:= y
