old-dtb := $(dtb-y)
old-dtbo := $(dtbo-y)
dtb-y :=
dtbo-y :=
makefile-path := platform/t23x/prometheus/kernel-dts

BUILD_ENABLE=n
ifneq ($(filter y,$(CONFIG_ARCH_TEGRA_23x_SOC)),)
BUILD_ENABLE=y
endif

dtb-$(BUILD_ENABLE) += tegra234-p3701-0000-p3740-0000.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3701-0002-p3740-0002.dtb
dtb-$(BUILD_ENABLE) += tegra234-p3701-0002-p3740-0002-b01.dtb
dtbo-$(BUILD_ENABLE) += tegra234-p3740-overlay-pcie.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3740-camera-hawk-owl-overlay.dtbo
dtbo-$(BUILD_ENABLE) += tegra234-p3740-camera-p3785-overlay.dtbo
dtb-$(BUILD_ENABLE) += tegra234-p3701-0002-p3740-0002-safety.dtb

ifneq ($(dtb-y),)
dtb-y := $(addprefix $(makefile-path)/,$(dtb-y))
endif
ifneq ($(dtbo-y),)
dtbo-y := $(addprefix $(makefile-path)/,$(dtbo-y))
endif

dtb-y += $(old-dtb)
dtbo-y += $(old-dtbo)
