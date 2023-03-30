# SPDX-License-Identifier: GPL-2.0

ccflags-y += -Werror
ccflags-y += -DDYNAMIC_DEBUG_MODULE
ccflags-y += -I$(srctree.nvidia)/drivers/misc/nvscic2c-pcie

obj-$(CONFIG_NVSCIC2C_PCIE) := nvscic2c-pcie-epc.o nvscic2c-pcie-epf.o
nvscic2c-pcie-epc-y := comm-channel.o dt.o endpoint.o epc/module.o iova-mngr.o pci-client.o stream-extensions.o vmap.o vmap-pin.o
nvscic2c-pcie-epf-y := comm-channel.o dt.o endpoint.o epf/module.o iova-mngr.o pci-client.o stream-extensions.o vmap.o vmap-pin.o
