old-dtb := $(dtb-y)
old-dtbo := $(dtbo-y)
dtb-y :=
dtbo-y :=
makefile-path := platform/t19x/mccoy/kernel-dts

dts-include += platform/t19x/

BUILD_ENABLE=n
ifneq ($(filter y,$(CONFIG_ARCH_TEGRA_19x_SOC) $(CONFIG_ARCH_TEGRA_194_SOC)),)
BUILD_ENABLE=y
endif

dtb-$(BUILD_ENABLE) += tegra194-p2888-0004-e3900-0000.dtb
dtbo-$(BUILD_ENABLE) += tegra194-p2888-0004-e3900-0000-hdr40.dtbo
dtbo-$(BUILD_ENABLE) += tegra194-p2888-0004-e3900-0000-adafruit-sph0645lm4h.dtbo
dtbo-$(BUILD_ENABLE) += tegra194-p2888-0004-e3900-0000-adafruit-uda1334a.dtbo
dtbo-$(BUILD_ENABLE) += tegra194-p2888-0004-e3900-0000-fe-pi-audio.dtbo
dtbo-$(BUILD_ENABLE) += tegra194-p2888-0004-e3900-0000-respeaker-4-mic-array.dtbo
dtbo-$(BUILD_ENABLE) += tegra194-p2888-0004-e3900-0000-respeaker-4-mic-lin-array.dtbo
dtbo-$(BUILD_ENABLE) += tegra194-p2888-0004-e3900-0000-csi.dtbo
dtbo-$(BUILD_ENABLE) += tegra194-p2888-0004-e3900-0000-dual-imx274.dtbo
dtbo-$(BUILD_ENABLE) += tegra194-p2888-0004-e3900-0000-imx274.dtbo

ifneq ($(dtb-y),)
dtb-y := $(addprefix $(makefile-path)/,$(dtb-y))
endif
ifneq ($(dtbo-y),)
dtbo-y := $(addprefix $(makefile-path)/,$(dtbo-y))
endif

dtb-y += $(old-dtb)
dtbo-y += $(old-dtbo)
