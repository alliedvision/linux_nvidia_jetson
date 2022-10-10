#
# Platform Security Controller Driver code
#

GCOV_PROFILE := y

ccflags-y += -I$(srctree.nvidia)/drivers/platform/tegra/include \
	     -I$(srctree)/drivers/mailbox

ccflags-y += -Werror

obj-$(CONFIG_TEGRA_PSC) += tegra23x_psc.o
tegra23x_psc-objs := tegra23x_psc_mailbox.o

tegra23x_psc-$(CONFIG_DEBUG_FS) += psc_debug.o
