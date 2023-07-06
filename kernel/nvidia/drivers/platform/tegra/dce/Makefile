# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2022-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
# Display Controller Engine code.
#
GCOV_PROFILE := y

ccflags-y += -I$(srctree.nvidia)/drivers/platform/tegra/dce/include
ccflags-y += -Wno-multichar
ccflags-y += -Werror
ccflags-y += -Wno-error=cpp
ifeq ($(VERSION),4)
ccflags-y += -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
endif

# Set config when build as OOT module.
ifeq ($(CONFIG_TEGRA_OOT_MODULE),m)
CONFIG_TEGRA_DCE := m
ccflags-y += -I$(srctree.nvidia)/include
endif

obj-$(CONFIG_TEGRA_DCE) += tegra-dce.o
tegra-dce-$(CONFIG_TEGRA_DCE) += \
	dce-ast.o \
	dce-reset.o \
	dce-hsp-smb.o \
	dce-hsp-ss.o \
	dce-worker.o \
	dce-fsm.o \
	dce-init-deinit.o \
	dce-mailbox.o \
	dce-bootstrap.o \
	dce-admin.o \
	dce-ipc.o \
	dce-ipc-signal.o \
	dce-client-ipc.o \
	dce-module.o \
	dce-pm.o \
	dce-util-common.o

ifeq ($(CONFIG_DEBUG_FS),y)
tegra-dce-$(CONFIG_TEGRA_DCE) += \
	dce-debug.o
endif

