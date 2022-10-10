ifdef CONFIG_ARCH_TEGRA_21x_SOC
obj-$(CONFIG_I2C_TEGRA_VI)      += i2c-tegra-vi.o
endif

#CFLAGS_i2c-bpmp-tegra.o                 = -Werror
ccflags-y += -I$(srctree.nvidia)/include
obj-$(CONFIG_I2C_BPMP_TEGRA)     += i2c-bpmp-tegra.o
obj-$(CONFIG_I2C_TEGRA_CAMRTC) += i2c-ivc-single.o i2c-ivc-multi.o i2c-rtcpu.o i2c-rtcpu-clk-config.o
obj-$(CONFIG_I2C_TEGRA_HV)	+= i2c-tegra-hv-common.o i2c-tegra-hv.o
obj-$(CONFIG_I2C_TEGRA_SLAVE)	+= i2c-tegra-slave.o
obj-$(CONFIG_I2C_NVVRS11)	+= i2c-nvvrs11.o

ifdef CONFIG_ARCH_TEGRA_19x_SOC

GCOV_PROFILE := y
subdir-ccflags-y := -Werror

obj-$(CONFIG_I2C_TEGRA194_SLAVE) += i2c-tegra194-slave.o

endif
