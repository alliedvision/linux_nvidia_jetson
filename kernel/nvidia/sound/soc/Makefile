ifeq ($(shell expr $(VERSION) \< 5), 1)
obj-$(CONFIG_SND_SOC) += tegra-alt/
else
obj-$(CONFIG_SND_SOC) += tegra-virt-alt/
endif
