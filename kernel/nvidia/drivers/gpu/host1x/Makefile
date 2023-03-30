# SPDX-License-Identifier: GPL-2.0

srctree.host1x := $(shell dirname $(abspath $(lastword $(MAKEFILE_LIST))))

ccflags-y := -I$(srctree.host1x)/include

host1x-next-y = \
	bus.o \
	syncpt.o \
	dev.o \
	intr.o \
	cdma.o \
	channel.o \
	job.o \
	debug.o \
	mipi.o \
	fence.o \
	hw/host1x01.o \
	hw/host1x02.o \
	hw/host1x04.o \
	hw/host1x05.o \
	hw/host1x06.o \
	hw/host1x07.o \
	hw/host1x08.o

host1x-next-$(CONFIG_IOMMU_API) += \
	context.o

obj-m := host1x-next.o
