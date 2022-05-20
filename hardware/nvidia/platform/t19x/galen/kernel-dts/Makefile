old-dtb := $(dtb-y)
old-dtbo := $(dtbo-y)
dtb-y :=
dtbo-y :=
makefile-path := platform/t19x/galen/kernel-dts

BUILD_19x_ENABLE=n
BUILD_18x_ENABLE=n
ifneq ($(filter y,$(CONFIG_ARCH_TEGRA_19x_SOC) $(CONFIG_ARCH_TEGRA_194_SOC)),)
BUILD_19x_ENABLE=y
endif
ifneq ($(filter y,$(CONFIG_ARCH_TEGRA_18x_SOC) $(CONFIG_ARCH_TEGRA_186_SOC)),)
BUILD_18x_ENABLE=y
endif

dtb-$(BUILD_19x_ENABLE) += tegra194-p2888-0001-p2822-0000.dtb
dtb-$(BUILD_19x_ENABLE) += tegra194-p2888-0001-p2822-0000-avt-14375-2cam.dtb
dtb-$(BUILD_19x_ENABLE) += tegra194-p2888-0001-p2822-0000-avt-14385-6cam.dtb
dtb-$(BUILD_19x_ENABLE) += tegra194-p2888-0001-p2822-0000-imx274-hdmi.dtb
dtb-$(BUILD_19x_ENABLE) += tegra194-p2888-0001-p2822-0000-imx185_v1.dtb
dtb-$(BUILD_19x_ENABLE) += tegra194-p2888-0001-p2822-0000-maxn.dtb
dtb-$(BUILD_19x_ENABLE) += tegra194-p2888-0001-e3366-1199.dtb
dtb-$(BUILD_19x_ENABLE) += tegra194-p2888-0006-e3366-1199.dtb
dtb-$(BUILD_19x_ENABLE) += tegra194-p2888-as-0006-p2822-0000.dtb
dtb-$(BUILD_19x_ENABLE) += tegra194-p2888-as-p3668-p2822-0000.dtb
dtbo-$(BUILD_19x_ENABLE) += tegra194-p2888-0001-p2822-0000-overlay.dtbo
dtbo-$(BUILD_19x_ENABLE) += tegra194-p2888-0001-p2822-0000-hdr40.dtbo
dtbo-$(BUILD_19x_ENABLE) += tegra194-p2888-0001-p2822-0000-adafruit-sph0645lm4h.dtbo
dtbo-$(BUILD_19x_ENABLE) += tegra194-p2888-0001-p2822-0000-adafruit-uda1334a.dtbo
dtbo-$(BUILD_19x_ENABLE) += tegra194-p2888-0001-p2822-0000-fe-pi-audio.dtbo
dtbo-$(BUILD_19x_ENABLE) += tegra194-p2888-0001-p2822-0000-respeaker-4-mic-array.dtbo
dtbo-$(BUILD_19x_ENABLE) += tegra194-p2888-0001-p2822-0000-respeaker-4-mic-lin-array.dtbo
dtbo-$(BUILD_19x_ENABLE) += tegra194-p2888-0001-p2822-0000-m2ke.dtbo
dtbo-$(BUILD_19x_ENABLE) += tegra194-p2888-0001-p2822-0000-csi.dtbo
dtbo-$(BUILD_19x_ENABLE) += tegra194-p2888-0005-overlay.dtbo
dtbo-$(BUILD_19x_ENABLE) += tegra194-p2822-camera-dual-imx274-overlay.dtbo
dtbo-$(BUILD_19x_ENABLE) += tegra194-p2822-camera-e3331-overlay.dtbo
dtbo-$(BUILD_19x_ENABLE) += tegra194-p2822-camera-e3333-overlay.dtbo
dtbo-$(BUILD_19x_ENABLE) += tegra194-p2822-camera-imx185-overlay.dtbo
dtbo-$(BUILD_19x_ENABLE) += tegra194-p2822-camera-imx390-overlay.dtbo



ifneq ($(dtb-y),)
dtb-y := $(addprefix $(makefile-path)/,$(dtb-y))
endif
ifneq ($(dtbo-y),)
dtbo-y := $(addprefix $(makefile-path)/,$(dtbo-y))
endif

dtb-y += $(old-dtb)
dtbo-y += $(old-dtbo)
