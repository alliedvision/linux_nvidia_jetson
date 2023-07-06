# Copyright (c) 2018-2023, NVIDIA CORPORATION.  All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms and conditions of the GNU General Public License,
# version 2, as published by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

ifneq ($(CONFIG_TEGRA_LINUX_SAFETY),y)

OSI_COMMON := nvethernetrm/osi/common
OSI_CORE := nvethernetrm/osi/core
OSI_DMA := nvethernetrm/osi/dma

obj-$(CONFIG_NVETHERNET) += nvethernet.o

# These CFLAGS must not be shared/used in OSI. These are local to Linux
ccflags-y +=  -DLINUX_OS -DNET30 -DNVPKCS_MACSEC -DLINUX_IVC -DUPDATED_PAD_CAL \
              -I$(srctree.nvidia)/drivers/net/ethernet/nvidia/nvethernet/nvethernetrm/include \
	     -I$(srctree.nvidia)/drivers/net/ethernet/nvidia/nvethernet/nvethernetrm/osi/common/include

nvethernet-objs:= ether_linux.o \
		  osd.o \
		  ethtool.o \
		  ether_tc.o \
		  sysfs.o \
		  ioctl.o \
		  ptp.o \
		  macsec.o \
		  $(OSI_CORE)/osi_core.o \
		  $(OSI_CORE)/osi_hal.o \
		  $(OSI_CORE)/macsec.o \
		  $(OSI_COMMON)/osi_common.o \
		  $(OSI_COMMON)/eqos_common.o \
		  $(OSI_COMMON)/mgbe_common.o \
		  $(OSI_DMA)/osi_dma.o \
		  $(OSI_DMA)/osi_dma_txrx.o \
		  $(OSI_CORE)/eqos_core.o \
		  $(OSI_CORE)/ivc_core.o \
		  $(OSI_CORE)/mgbe_core.o \
		  $(OSI_CORE)/core_common.o \
		  $(OSI_CORE)/xpcs.o \
		  $(OSI_DMA)/mgbe_dma.o \
		  $(OSI_CORE)/eqos_mmc.o \
		  $(OSI_DMA)/eqos_dma.o \
		  $(OSI_DMA)/eqos_desc.o \
		  $(OSI_DMA)/mgbe_desc.o \
		  $(OSI_DMA)/debug.o \
		  $(OSI_CORE)/mgbe_mmc.o \
		  $(OSI_CORE)/frp.o \
		  $(OSI_CORE)/vlan_filter.o \
		  $(OSI_CORE)/debug.o

include $(srctree.nvidia)/drivers/net/ethernet/nvidia/nvethernet/nvethernetrm/include/config.tmk

nvethernet-$(CONFIG_NVETHERNET_SELFTESTS) += selftests.o

endif
