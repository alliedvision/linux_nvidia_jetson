GCOV_PROFILE := y

subdir-ccflags-y := -Werror
ccflags-y += -I$(overlay)/$(src)/../tegra-virt-alt/include/
ccflags-y += -I$(overlay)/$(src)/../codecs
ccflags-y += -I$(srctree.nvidia)/drivers/platform/tegra/nvaudio_ivc/

# Tegra platform Support

snd-soc-tegra210-virt-alt-admaif-objs := tegra210_virt_alt_admaif.o \
					tegra_asoc_xbar_virt_alt.o \
					tegra_asoc_util_virt_alt.o \
					tegra_asoc_machine_virt_alt.o \
					tegra_pcm_virt_alt.o

snd-soc-tegra-virt-t210ref-pcm-objs := tegra_virt_ref_alt.o
obj-$(CONFIG_SND_SOC_TEGRA_VIRT_T210REF_PCM) += snd-soc-tegra210-virt-alt-admaif.o
obj-$(CONFIG_SND_SOC_TEGRA_VIRT_T210REF_PCM) += snd-soc-tegra-virt-t210ref-pcm.o

snd-soc-tegra210-virt-alt-adsp-objs := tegra210_adsp_virt_alt.o
obj-$(CONFIG_SND_SOC_TEGRA210_ADSP_VIRT_ALT) += snd-soc-tegra210-virt-alt-adsp.o
