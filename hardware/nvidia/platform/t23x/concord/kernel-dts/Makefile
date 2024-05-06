old-dtb := $(dtb-y)
old-dtbo := $(dtbo-y)
dtb-y :=
dtbo-y :=
makefile-path := platform/t23x/concord/kernel-dts

BUILD_ENABLE=n
ifneq ($(filter y,$(CONFIG_ARCH_TEGRA_23x_SOC)),)
BUILD_ENABLE=y
endif

dtb-$(BUILD_ENABLE) += tegra234-p3701-0000-p3737-0000.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3701-0000-p3737-0000-avt-14375-2cam.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3701-0000-as-p3701-0004-p3737-0000.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3701-0000-as-p3767-0004-p3737-0000.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3701-0000-as-p3767-0003-p3737-0000.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3701-0000-as-p3767-0003-p3737-0000-dsi-innolux-a1-cphy.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3701-0000-as-p3767-0003-p3737-0000-dsi-innolux-c1-dphy.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3701-0000-as-p3767-0003-p3737-0000-dsi-innolux-c1-dsc-dphy.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3701-0000-as-p3767-0003-p3737-0000-dsi-tianma-a-dphy.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3701-0000-as-p3767-0003-p3737-0000-dsi-tianma-b-dphy.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3701-0000-as-p3767-0000-p3737-0000.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3701-0000-as-p3767-0001-p3737-0000.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3701-0000-as-pxxxx-p3737-0000.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3701-0000-p3737-0000-kexec.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3701-0004-p3737-0000.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3701-0004-p3737-0000-avt-14375-2cam.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3701-0005-p3737-0000.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3701-0005-p3737-0000-avt-14375-2cam.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3701-0008-p3737-0000.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3701-0008-p3737-0000-avt-14375-2cam.dtb
dtbo-$(BUILD_ENABLE) += tegra234-p3737-overlay.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3737-audio-codec-rt5658.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3737-audio-codec-rt5640.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3737-audio-codec-rt5658-40pin.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3737-0000-p3701-0000-adafruit-sph0645lm4h.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3737-0000-p3701-0000-adafruit-uda1334a.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3737-0000-p3701-0000-csi.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3737-0000-p3701-0000-fe-pi-audio.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3737-0000-p3701-0000-hdr40.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3737-0000-p3701-0000-m2ke.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3737-0000-p3701-0000-respeaker-4-mic-array.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3737-0000-p3701-0000-respeaker-4-mic-lin-array.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3737-camera-dual-imx274-overlay.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3737-camera-e3331-overlay.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3737-camera-e3333-overlay.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3737-camera-imx185-overlay.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3737-camera-imx390-overlay.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3737-camera-hawk-owl-overlay.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3701-overlay.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3737-camera-dual-hawk-ar0234-e3653-overlay.dtbo

ifneq ($(dtb-y),)
dtb-y := $(addprefix $(makefile-path)/,$(dtb-y))
endif
ifneq ($(dtbo-y),)
dtbo-y := $(addprefix $(makefile-path)/,$(dtbo-y))
endif

dtb-y += $(old-dtb)
dtbo-y += $(old-dtbo)
