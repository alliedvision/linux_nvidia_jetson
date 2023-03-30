ifeq ($(CONFIG_TEGRA_OOT_MODULE),m)
NVPVA_OOT = y
NVPVA_OBJ = m
NVPVA_T23X = y
else
NVPVA_OBJ = $(CONFIG_TEGRA_GRHOST_PVA)
NVPVA_T23X = $(CONFIG_TEGRA_T23X_GRHOST)
endif

GCOV_PROFILE := y
ccflags-y += -I$(srctree.nvidia)/drivers/video/tegra/host/pva
ccflags-y += -I$(srctree.nvidia)/drivers/video/tegra/host/pva/fw_include
ccflags-y += -I$(srctree.nvidia)/include
ccflags-y += -I$(srctree.nvidia)/include/linux
ccflags-y += -Werror

# When using the upstream host1x driver, the Makefile must define the
# srctree.host1x path in order to find the necessary header files for
# the upstream host1x driver.
ccflags-$(NVPVA_OOT) += -I$(srctree.host1x)/include
ccflags-$(NVPVA_OOT) += -DCONFIG_TEGRA_HOST1X
ccflags-$(NVPVA_OOT) += -DCONFIG_TEGRA_FUSE_UPSTREAM
ccflags-$(NVPVA_OOT) += -DCONFIG_TEGRA_T23X_GRHOST
ccflags-$(NVPVA_OOT) += -DTEGRA_OOT_MODULE

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
		nvpva_elf_parser.o \
		pva_vpu_exe.o \
		nvpva_client.o \
		nvpva_queue.o \
		pva_dma.o \
		nvpva_buffer.o \
		pva_vpu_ocd.o \
		pva_sha256.o \
		pva_system_allow_list.o \
		pva_vpu_app_auth.o \
		pva_iommu_context_dev.o \
		nvpva_syncpt.o

ifdef NVPVA_T23X
nvhost-pva-objs  += \
		pva_isr_t23x.o \
		pva_mailbox_t23x.o \
		pva_interface_regs_t23x.o \
		pva_version_config_t23x.o \
		pva_ccq_t23x.o
endif

obj-$(NVPVA_OBJ) += nvhost-pva.o

