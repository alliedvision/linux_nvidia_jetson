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
obj-$(CONFIG_TEGRA_DCE) += tegra-dce.o

tegra-dce-y += \
	dce-ast.o \
	dce-reset.o \
	dce-hsp-smb.o \
	dce-hsp-ss.o \
	dce-worker.o \
	dce-init-deinit.o \
	dce-mailbox.o \
	dce-bootstrap.o \
	dce-admin.o \
	dce-ipc.o \
	dce-ipc-signal.o \
	dce-client-ipc.o \
	dce-module.o \
	dce-util-common.o

tegra-dce-$(CONFIG_DEBUG_FS) += \
	dce-debug.o

