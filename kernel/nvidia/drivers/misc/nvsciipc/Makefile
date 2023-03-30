#
# Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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
# drivers/misc/nvsciipc/Makefile
#

ccflags-y += -Werror
ccflags-y += -I$(srctree.nvidia)/include/


# Make as module for OOT module build
ifeq ($(CONFIG_TEGRA_OOT_MODULE),m)
obj-m := nvsciipc.o
else
obj-$(CONFIG_NVSCIIPC) := nvsciipc.o
endif

