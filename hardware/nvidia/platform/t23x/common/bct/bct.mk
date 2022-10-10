 #
 # Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
 #
 # This program is free software; you can redistribute it and/or modify
 # it under the terms of the GNU General Public License as published by
 # the Free Software Foundation; either version 2 of the License, or
 # (at your option) any later version.
 #
 # This program is distributed in the hope that it will be useful, but WITHOUT
 # ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 # FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 # more details.

BCT_FILES_PATH=hardware/nvidia/platform/t23x/common/bct

PRODUCT_COPY_FILES += \
    $(BCT_FILES_PATH)/device/tegra234-mb1-bct-device-common.dtsi:tegra234-mb1-bct-device-common.dtsi \
    $(BCT_FILES_PATH)/diag-boot/tegra234-br-bct-diag-boot.dts:tegra234-br-bct-diag-boot.dts \
    $(BCT_FILES_PATH)/firewall/tegra234-chip.dtsi:tegra234-chip.dtsi \
    $(BCT_FILES_PATH)/firewall/tegra234-firewall-config-base.dtsi:tegra234-firewall-config-base.dtsi \
    $(BCT_FILES_PATH)/firewall/tegra234-mb2-bct-firewall-common-mods.dts:tegra234-mb2-bct-firewall-common-mods.dts \
    $(BCT_FILES_PATH)/firewall/tegra234-mb2-bct-firewall-common.dts:tegra234-mb2-bct-firewall-common.dts \
    $(BCT_FILES_PATH)/misc/tegra234-mb1-bct-carveout-auto.dtsi:tegra234-mb1-bct-carveout-auto.dtsi \
    $(BCT_FILES_PATH)/misc/tegra234-mb1-bct-carveout-common.dtsi:tegra234-mb1-bct-carveout-common.dtsi \
    $(BCT_FILES_PATH)/misc/tegra234-mb1-bct-carveout-l4t.dtsi:tegra234-mb1-bct-carveout-l4t.dtsi \
    $(BCT_FILES_PATH)/misc/tegra234-mb1-bct-misc-auto-common.dtsi:tegra234-mb1-bct-misc-auto-common.dtsi \
    $(BCT_FILES_PATH)/misc/tegra234-mb1-bct-misc-common.dtsi:tegra234-mb1-bct-misc-common.dtsi \
    $(BCT_FILES_PATH)/misc/tegra234-mb2-bct-auto-common.dtsi:tegra234-mb2-bct-auto-common.dtsi \
    $(BCT_FILES_PATH)/misc/tegra234-mb2-bct-common.dtsi:tegra234-mb2-bct-common.dtsi \
    $(BCT_FILES_PATH)/misc/tegra234-mb2-bct-mods-common.dtsi:tegra234-mb2-bct-mods-common.dtsi \
    $(BCT_FILES_PATH)/misc/tegra234-mb2-bct-mods-common.dtsi:tegra234-mb2-bct-mods-common.dtsi \
    $(BCT_FILES_PATH)/sdram/tegra234-mem-bct-sw-override-carveout-auto.dtsi:tegra234-mem-bct-sw-override-carveout-auto.dtsi \
    $(BCT_FILES_PATH)/sdram/tegra234-mem-bct-sw-override-carveout-common.dtsi:tegra234-mem-bct-sw-override-carveout-common.dtsi \
    $(BCT_FILES_PATH)/sdram/tegra234-mem-bct-sw-override-carveout-l4t.dtsi:tegra234-mem-bct-sw-override-carveout-l4t.dtsi \
    $(BCT_FILES_PATH)/sdram/tegra234-mem-bct-sw-override-non-carveout-common.dtsi:tegra234-mem-bct-sw-override-non-carveout-common.dtsi \
    $(BCT_FILES_PATH)/sdram/tegra234-mem-bct-sw-override-non-carveout-mods.dtsi:tegra234-mem-bct-sw-override-non-carveout-mods.dtsi \
    $(BCT_FILES_PATH)/uphy-lanes/tegra234-mb1-bct-uphylane-si.dtsi:tegra234-mb1-bct-uphylane-si.dtsi

