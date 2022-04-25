old-dtb := $(dtb-y)
old-dtbo := $(dtbo-y)
dtb-y :=
dtbo-y :=
makefile-path := platform/t18x/lanai/kernel-dts

dtb-$(CONFIG_ARCH_TEGRA_18x_SOC) += tegra186-p3636-0001-p3509-0000-a01.dtb
dtb-$(CONFIG_ARCH_TEGRA_18x_SOC) += tegra186-p3636-0002-p3509-0000-a01.dtb
dtb-$(CONFIG_ARCH_TEGRA_18x_SOC) += tegra186-p3636-0002-lanai.dtb
dtbo-$(CONFIG_ARCH_TEGRA_18x_SOC) += tegra186-p3636-0001-p3509-0000-a01-adafruit-sph0645lm4h.dtbo
dtbo-$(CONFIG_ARCH_TEGRA_18x_SOC) += tegra186-p3636-0001-p3509-0000-a01-adafruit-uda1334a.dtbo
dtbo-$(CONFIG_ARCH_TEGRA_18x_SOC) += tegra186-p3636-0001-p3509-0000-a01-fe-pi-audio.dtbo
dtbo-$(CONFIG_ARCH_TEGRA_18x_SOC) += tegra186-p3636-0001-p3509-0000-a01-hdr40.dtbo
dtbo-$(CONFIG_ARCH_TEGRA_18x_SOC) += tegra186-p3636-0001-p3509-0000-a01-m2ke.dtbo
dtbo-$(CONFIG_ARCH_TEGRA_18x_SOC) += tegra186-p3636-0001-p3509-0000-a01-respeaker-4-mic-array.dtbo
dtbo-$(CONFIG_ARCH_TEGRA_18x_SOC) += tegra186-p3636-0001-p3509-0000-a01-respeaker-4-mic-lin-array.dtbo

ifneq ($(dtb-y),)
dtb-y := $(addprefix $(makefile-path)/,$(dtb-y))
dts-include += $(makefile-path)
endif
ifneq ($(dtbo-y),)
dtbo-y := $(addprefix $(makefile-path)/,$(dtbo-y))
endif

dtb-y += $(old-dtb)
dtbo-y += $(old-dtbo)
