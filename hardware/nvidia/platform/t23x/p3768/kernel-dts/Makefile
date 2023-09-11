old-dtb := $(dtb-y)
old-dtbo := $(dtbo-y)
dtb-y :=
dtbo-y :=
makefile-path := platform/t23x/p3768/kernel-dts

BUILD_ENABLE=n
ifneq ($(filter y,$(CONFIG_ARCH_TEGRA_23x_SOC)),)
BUILD_ENABLE=y
endif

dtb-$(BUILD_ENABLE) += tegra234-p3767-0000-p3509-a02.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3767-0000-as-p3767-0001-p3509-a02.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3767-0000-as-p3767-0003-p3509-a02.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3767-0000-as-p3767-0004-p3509-a02.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3767-0001-p3509-a02.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3767-0003-p3509-a02.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3767-0004-p3509-a02.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3767-0000-p3768-0000-a0.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3767-0000-as-p3767-0001-p3768-0000-a0.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3767-0000-as-p3767-0003-p3768-0000-a0.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3767-0000-as-p3767-0004-p3768-0000-a0.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3767-0001-p3768-0000-a0.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3767-0003-p3768-0000-a0.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3767-0004-p3768-0000-a0.dtb
dtbo-$(BUILD_ENABLE) += tegra234-p3767-0000-p3509-a02-adafruit-sph0645lm4h.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3767-0000-p3509-a02-adafruit-uda1334a.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3767-0000-p3509-a02-csi.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3767-0000-p3509-a02-fe-pi-audio.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3767-0000-p3509-a02-hdr40.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3767-0000-p3509-a02-m2ke.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3767-0000-p3509-a02-respeaker-4-mic-array.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3767-0000-p3509-a02-respeaker-4-mic-lin-array.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3767-0000-p3768-0000-csi.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3767-camera-imx477-imx219.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3767-camera-imx477-dual.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3767-camera-imx219-dual.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3767-camera-p3768-imx477-imx219.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3767-camera-p3768-imx219-dual.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3767-camera-p3768-imx477-dual.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3767-camera-p3768-imx477-dual-4lane.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3767-overlay.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3768-overlay.dtbo

ifneq ($(dtb-y),)
dtb-y := $(addprefix $(makefile-path)/,$(dtb-y))
endif
ifneq ($(dtbo-y),)
dtbo-y := $(addprefix $(makefile-path)/,$(dtbo-y))
endif

dtb-y += $(old-dtb)
dtbo-y += $(old-dtbo)
