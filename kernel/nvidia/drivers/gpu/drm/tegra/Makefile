# SPDX-License-Identifier: GPL-2.0

srctree.tegra-drm := $(shell dirname $(abspath $(lastword $(MAKEFILE_LIST))))
srctree.host1x := $(abspath $(srctree.tegra-drm)/../../host1x)

ccflags-$(CONFIG_DRM_TEGRA_DEBUG) += -DDEBUG
ccflags-y += -I$(srctree.tegra-drm)/include
ccflags-y += -I$(srctree.host1x)/include

tegra-drm-next-y := \
	drm.o \
	uapi.o \
	submit.o \
	firewall.o \
	gem.o \
	fb.o \
	dp.o \
	hub.o \
	plane.o \
	dc.o \
	output.o \
	rgb.o \
	hda.o \
	hdmi.o \
	mipi-phy.o \
	dsi.o \
	sor.o \
	dpaux.o \
	gr2d.o \
	gr3d.o \
	falcon.o \
	vic.o \
	nvdec.o \
	nvenc.o \
	nvjpg.o \
	riscv.o \
	util.o \
	ofa.o

tegra-drm-next-y += trace.o

obj-m := tegra-drm-next.o
