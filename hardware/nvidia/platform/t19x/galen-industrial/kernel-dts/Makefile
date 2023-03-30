old-dtb := $(dtb-y)
old-dtbo := $(dtbo-y)
dtb-y :=
dtbo-y :=
makefile-path := platform/t19x/galen-industrial/kernel-dts

BUILD_19x_ENABLE=n
BUILD_18x_ENABLE=n
ifneq ($(filter y,$(CONFIG_ARCH_TEGRA_19x_SOC) $(CONFIG_ARCH_TEGRA_194_SOC)),)
BUILD_19x_ENABLE=y
endif
ifneq ($(filter y,$(CONFIG_ARCH_TEGRA_18x_SOC) $(CONFIG_ARCH_TEGRA_186_SOC)),)
BUILD_18x_ENABLE=y
endif

dtb-$(BUILD_19x_ENABLE) += tegra194-p2888-0008-e3366-1199.dtb
dtb-$(BUILD_19x_ENABLE) += tegra194-p2888-0008-p2822-0000.dtb
dtb-$(BUILD_19x_ENABLE) += tegra194-p2888-0008-p2822-0000-maxn.dtb
dtb-$(BUILD_19x_ENABLE) += tegra194-p2888-0008-p2822-0000-noecc.dtb
dtb-$(BUILD_19x_ENABLE) += tegra194-p2888-0008-p2822-0000-safety.dtb
dtbo-$(BUILD_19x_ENABLE) += tegra194-p2888-0008-p2822-0000-overlay.dtbo

ifneq ($(dtb-y),)
dtb-y := $(addprefix $(makefile-path)/,$(dtb-y))
endif
ifneq ($(dtbo-y),)
dtbo-y := $(addprefix $(makefile-path)/,$(dtbo-y))
endif

dtb-y += $(old-dtb)
dtbo-y += $(old-dtbo)
