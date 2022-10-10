# SPDX-License-Identifier: GPL-2.0

DTC_CPP_FLAGS += $(EXTRA_CPP_FLAGS)

DTC_CPP_FLAGS += -DLINUX_VERSION=504 -DTEGRA_HOST1X_DT_VERSION=1

DTC_INCLUDE += $(tegra-dtstree)/soc/tegra/kernel-include
DTC_INCLUDE += $(tegra-dtstree)/platform/tegra/common/kernel-dts

ifeq ($(CONFIG_ARCH_TEGRA_210_SOC),y)
DTC_INCLUDE += $(tegra-dtstree)/soc/t210/kernel-dts
DTC_INCLUDE += $(tegra-dtstree)/platform/t210/common/kernel-dts
DTC_INCLUDE += $(tegra-dtstree)/platform/t210b01/common/kernel-dts
endif

ifeq ($(CONFIG_ARCH_TEGRA_186_SOC),y)
DTC_INCLUDE += $(tegra-dtstree)/soc/t18x/kernel-include
DTC_INCLUDE += $(tegra-dtstree)/soc/t18x/kernel-dts
DTC_INCLUDE += $(tegra-dtstree)/platform/t18x/common/kernel-dts
endif

ifeq ($(CONFIG_ARCH_TEGRA_194_SOC),y)
DTC_INCLUDE += $(tegra-dtstree)/soc/t19x/kernel-include
DTC_INCLUDE += $(tegra-dtstree)/soc/t19x/kernel-dts
DTC_INCLUDE += $(tegra-dtstree)/platform/t19x/common/kernel-dts
DTC_INCLUDE += $(tegra-dtstree)/platform/t19x/automotive/kernel-dts/common
endif

ifeq ($(CONFIG_ARCH_TEGRA_23x_SOC),y)
DTC_INCLUDE += $(tegra-dtstree)/soc/t23x/kernel-include
DTC_INCLUDE += $(tegra-dtstree)/soc/t23x/kernel-dts
DTC_INCLUDE += $(tegra-dtstree)/platform/t23x/common/kernel-dts
DTC_INCLUDE += $(tegra-dtstree)/platform/t23x/automotive/kernel-dts/common/linux/
endif

ifeq ($(CONFIG_ARCH_TEGRA_239_SOC),y)
DTC_INCLUDE += $(tegra-dtstree)/soc/t239/kernel-include
DTC_INCLUDE += $(tegra-dtstree)/soc/t239/kernel-dts
DTC_INCLUDE += $(tegra-dtstree)/platform/t239/common/kernel-dts
endif

DTB_LIST := $(dtb-y)
DTBO_LIST := $(dtbo-y)
dtb-y :=
dts_makefile=$(foreach d,$(wildcard $1*), $(call dts_makefile,$(d)/,$(2)) $(if $(findstring Makefile,$(d)),$(d)))
dts_mfiles = $(call dts_makefile, $(tegra-dtstree)/platform/, Makefile)
ifneq ($(dts_mfiles),)
dts-include :=
include $(dts_mfiles)
dtb-y := $(addprefix $(tegra-rel-dtstree)/hardware/nvidia/,$(dtb-y))
dtbo-y := $(addprefix $(tegra-rel-dtstree)/hardware/nvidia/,$(dtbo-y))
ifneq ($(dts-include),)
DTC_INCLUDE += $(addprefix $(tegra-dtstree)/,$(dts-include))
endif
endif

DTB_LIST += $(dtb-y)
DTB_NEW_RULE_LIST := $(dtb-y)
DTBO_LIST += $(dtbo-y)
DTBO_NEW_RULE_LIST := $(dtbo-y)

# Now save DTB_LIST to dtb-y
dtb-y := $(DTB_LIST)
dtbo-y := $(DTBO_LIST)

DTB_OBJS := $(addprefix $(obj)/,$(DTB_LIST))
DTBO_OBJS := $(addprefix $(obj)/,$(DTBO_LIST))

define _define_dtb_rule
$(obj)/$(call replace_ddot,$(1)): $(src)/$(patsubst %.dtb,%.dts,$(1)) FORCE
endef

$(foreach _dtb, $(DTB_NEW_RULE_LIST), $(eval $(call _define_dtb_rule,$(_dtb))))

DTB_OBJS := $(call replace_ddot,$(DTB_OBJS))
DTB_NEW_RULE_LIST := $(addprefix $(obj)/,$(DTB_NEW_RULE_LIST))
DTB_NEW_RULE_LIST := $(call replace_ddot,$(DTB_NEW_RULE_LIST))

$(DTB_NEW_RULE_LIST):
	$(call if_changed_dep,dtc)

define _define_dtbo_rule
$(obj)/$(call replace_ddot,$(1)): $(src)/$(patsubst %.dtbo,%.dts,$(1)) FORCE
endef

$(foreach _dtbo, $(DTBO_NEW_RULE_LIST), $(eval $(call _define_dtbo_rule,$(_dtbo))))

DTBO_OBJS := $(call replace_ddot,$(DTBO_OBJS))
DTBO_NEW_RULE_LIST := $(addprefix $(obj)/,$(DTBO_NEW_RULE_LIST))
DTBO_NEW_RULE_LIST := $(call replace_ddot,$(DTBO_NEW_RULE_LIST))

$(DTBO_NEW_RULE_LIST):
	$(call if_changed_dep,dtc)

dtbs: $(DTB_OBJS) $(DTBO_OBJS) FORCE
	if [ ! -d arch/arm64/boot/dts/nvidia/ ] ; then \
		mkdir arch/arm64/boot/dts/nvidia/ ; \
	fi
	cp -u $(DTB_OBJS) $(DTBO_OBJS) arch/arm64/boot/dts/nvidia/
	if [ -d arch/arm64/boot/dts/_ddot_/ ] ; then \
		rm -rf arch/arm64/boot/dts/_ddot_/ ; \
	fi

clean-files := *.dtb *.dtbo *.tmp
