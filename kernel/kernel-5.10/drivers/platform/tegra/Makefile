# Copyright (c) 2012-2020, NVIDIA CORPORATION.  All rights reserved.
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
#
# Makefile for linux/drivers/platform/tegra
# Tegra Platform-Specific Drivers
#

GCOV_PROFILE := y
subdir-ccflags-y += -Werror
subdir-ccflags-y += -I$(srctree)/drivers/platform/tegra/include

obj-y                                   += tegra-safety-build.o
obj-y                                   += tegra-hv-mode.o
