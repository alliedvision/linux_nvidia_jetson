ifeq ($(CONFIG_TEGRA_PVA_V1),y)
V2 := n
BRINGUP := $(CONFIG_TEGRA_GRHOST_PVA)
else
V2 := $(CONFIG_TEGRA_GRHOST_PVA)
BRINGUP := n
endif

obj-$(BRINGUP) = bringup/

GCOV_PROFILE := y
ccflags-$(V2) += -I$(srctree.nvidia)/drivers/video/tegra/host/pva
ccflags-$(V2) += -I$(srctree.nvidia)/drivers/video/tegra/host
ccflags-$(V2) += -I$(srctree.nvidia)/drivers/video/tegra/host/pva/fw_include
ccflags-$(V2) += -I$(srctree.nvidia)/include
ccflags-$(V2) += -Werror


nvhost-pva-objs  = \
		pva.o \
		pva_ioctl.o \
		pva_mailbox.o \
		pva_interface_regs_t19x.o \
		pva_version_config_t19x.o \
		pva_mailbox_t19x.o \
		pva_isr.o \
		pva_queue.o \
		pva_debug.o \
		pva_trace.o \
		pva_abort.o \
		pva_ccq_t19x.o \
		pva_selftest.o \
		nvpva_elf_parser.o \
		pva_vpu_exe.o \
		nvpva_client.o \
		nvpva_queue.o \
		pva_dma.o

obj-$(V2) += nvhost-pva.o

ifdef CONFIG_TEGRA_T23X_GRHOST
nvhost-pva-t23x-objs  = \
		pva_isr_t23x.o \
		pva_mailbox_t23x.o \
		pva_interface_regs_t23x.o \
		pva_version_config_t23x.o \
		pva_ccq_t23x.o


obj-$(V2) += nvhost-pva-t23x.o

endif
