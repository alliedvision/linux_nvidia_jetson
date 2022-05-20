ccflags-y := -Werror
ccflags-y += -I$(srctree)/include
ccflags-y += -I$(srctree)
ccflags-y += -I$(srctree.nvidia)
ccflags-y += -I$(srctree.nvidia)/drivers/platform/tegra/rtcpu
ccflags-y += -I$(srctree.nvidia)/drivers/platform/tegra

obj-y += camera-diagnostics.o
obj-y += debug.o
obj-y += ivc-bus.o
obj-y += tegra-sysfs-mbox.o
obj-y += vi-notify.o
ifeq ($(CONFIG_TEGRA_CAMERA_HSP_MBOX_CLIENT), y)
obj-y += hsp-mailbox-client.o
else
obj-y += hsp-combo.o
endif
obj-y += tegra-rtcpu-trace.o
obj-y += camchar.o
obj-y += clk-group.o
obj-y += device-group.o
obj-y += reset-group.o
obj-y += rtcpu-monitor.o
obj-y += tegra-ivc-rpc.o tegra-ivc-rpc-test.o
obj-y += capture-ivc.o
obj-y += syncpt-display-channel.o
obj-y += tegra-rtcpu-coverage.o
obj-m += i2c_util.o
