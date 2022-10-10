GCOV_PROFILE := y

ccflags-y += -I$(srctree.nvidia)/drivers/video/tegra/host
ccflags-y += -I$(srctree.nvidia)/include
ccflags-y += -Idrivers/video/tegra/host
ccflags-y += -Idrivers/devfreq
ccflags-y += -Wno-multichar

ifeq ($(CONFIG_TEGRA_T239_GRHOST),y)
	ccflags-y += -I$(srctree.nvidia-t239)/include
	ccflags-y += -I$(srctree.nvidia-t239)/drivers/video/tegra/host
endif

ccflags-y += -Werror -Wno-vla

nvhost-y += \
	nvhost_acm.o \
	nvhost_syncpt.o \
	nvhost_cdma.o \
	nvhost_intr.o \
	nvhost_channel.o \
	nvhost_job.o \
	dev.o \
	debug.o \
	bus_client.o \
	chip_support.o \
	nvhost_vm.o \
	syncpt_fd.o \
	nvhost_syncpt_dmabuf.o \
	interrupt_syncpt.o \
	iommu_context_dev.o \
	nvhost_syncpt_unit_interface.o \
	host1x/host1x.o \
	flcn/flcn.o \
	riscv/riscv.o \
	t210/t210.o \
	t194/t194.o \
	t23x/t23x.o

nvhost-$(CONFIG_TEGRA_T239_GRHOST) += \
	t239/t239.o

nvhost-$(CONFIG_TEGRA_GRHOST_OFA) += ofa/ofa.o
nvhost-$(CONFIG_TEGRA_GRHOST_NVDEC) += nvdec/nvdec.o nvdec/nvdec_t23x.o
nvhost-$(CONFIG_TEGRA_GRHOST_TSEC) += tsec/tsec.o tsec/tsec_t23x.o

ifeq ($(CONFIG_TEGRA_T239_GRHOST),y)
nvhost-$(CONFIG_TEGRA_GRHOST_VIC) += vic/vic_t239.o
endif

nvhost-$(CONFIG_TEGRA_GRHOST_LEGACY_PD) += nvhost_pd.o
ifeq ($(CONFIG_SYNC),y)
nvhost-$(CONFIG_TEGRA_GRHOST_SYNC) += nvhost_sync.o
else
nvhost-$(CONFIG_TEGRA_GRHOST_SYNC) += nvhost_sync_v2.o nvhost_sync_compat.o
endif

nvhost-$(CONFIG_TEGRA_GRHOST_SCALE) += scale_emc.o nvhost_scale.o
nvhost-$(CONFIG_TEGRA_GRHOST_VHOST) += vhost/vhost.o vhost/vhost_client.o

obj-$(CONFIG_TEGRA_GRHOST) += nvhost.o

# Not built into module

obj-$(CONFIG_TEGRA_GRHOST_ISP) += isp/
obj-$(CONFIG_TEGRA_GRHOST_NVCSI) += nvcsi/
ifneq ($(filter y,$(CONFIG_ARCH_TEGRA_18x_SOC) $(CONFIG_ARCH_TEGRA_186_SOC)),)
obj-y += vi/
endif

obj-$(CONFIG_TEGRA_GRHOST_CAPTURE_SUPPORT) += capture/
obj-$(CONFIG_TEGRA_GRHOST_PVA) += pva/
nvhost-$(CONFIG_TEGRA_GRHOST_PVA) += nvhost_queue.o nvhost_buffer.o
obj-$(CONFIG_TEGRA_GRHOST_NVDLA) += nvdla/
nvhost-$(CONFIG_TEGRA_GRHOST_NVDLA) += nvhost_queue.o nvhost_buffer.o
obj-$(CONFIG_TEGRA_GRHOST_SLVSEC) += slvsec/

ifdef CONFIG_EVENTLIB

$(obj)/bus_client.o: $(obj)/nvhost_events_json.h

$(obj)/nvhost_events_json.h: $(src)/nvhost_events.json
	xxd -i $< $@
	sed -i 's/\(.*\) \(.*\)nvhost_events_json\(.*\)/\1 nvhost_events_json\3/g' $@

clean-files := nvhost_events_json.h

endif
