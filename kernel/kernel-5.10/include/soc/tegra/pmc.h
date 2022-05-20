/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2010 Google, Inc
 * Copyright (c) 2014-2020, NVIDIA CORPORATION. All rights reserved.
 *
 * Author:
 *	Colin Cross <ccross@google.com>
 */

#ifndef __SOC_TEGRA_PMC_H__
#define __SOC_TEGRA_PMC_H__

#include <linux/reboot.h>
#include <linux/usb/ch9.h>
#include <linux/pinctrl/pinctrl.h>

#include <soc/tegra/pm.h>

struct clk;
struct reset_control;

bool tegra_pmc_cpu_is_powered(unsigned int cpuid);
int tegra_pmc_cpu_power_on(unsigned int cpuid);
int tegra_pmc_cpu_remove_clamping(unsigned int cpuid);

/*
 * powergate and I/O rail APIs
 */

#define TEGRA_POWERGATE_CPU	0
#define TEGRA_POWERGATE_3D	1
#define TEGRA_POWERGATE_VENC	2
#define TEGRA_POWERGATE_PCIE	3
#define TEGRA_POWERGATE_VDEC	4
#define TEGRA_POWERGATE_L2	5
#define TEGRA_POWERGATE_MPE	6
#define TEGRA_POWERGATE_HEG	7
#define TEGRA_POWERGATE_SATA	8
#define TEGRA_POWERGATE_CPU1	9
#define TEGRA_POWERGATE_CPU2	10
#define TEGRA_POWERGATE_CPU3	11
#define TEGRA_POWERGATE_CELP	12
#define TEGRA_POWERGATE_3D1	13
#define TEGRA_POWERGATE_CPU0	14
#define TEGRA_POWERGATE_C0NC	15
#define TEGRA_POWERGATE_C1NC	16
#define TEGRA_POWERGATE_SOR	17
#define TEGRA_POWERGATE_DIS	18
#define TEGRA_POWERGATE_DISB	19
#define TEGRA_POWERGATE_XUSBA	20
#define TEGRA_POWERGATE_XUSBB	21
#define TEGRA_POWERGATE_XUSBC	22
#define TEGRA_POWERGATE_VIC	23
#define TEGRA_POWERGATE_IRAM	24
#define TEGRA_POWERGATE_NVDEC	25
#define TEGRA_POWERGATE_NVJPG	26
#define TEGRA_POWERGATE_AUD	27
#define TEGRA_POWERGATE_DFD	28
#define TEGRA_POWERGATE_VE2	29
#define TEGRA_POWERGATE_MAX	TEGRA_POWERGATE_VE2

#define TEGRA_POWERGATE_3D0	TEGRA_POWERGATE_3D

/**
 * enum tegra_io_pad - I/O pad group identifier
 *
 * I/O pins on Tegra SoCs are grouped into so-called I/O pads. Each such pad
 * can be used to control the common voltage signal level and power state of
 * the pins of the given pad.
 */
enum tegra_io_pad {
	TEGRA_IO_PAD_AUDIO,
	TEGRA_IO_PAD_AUDIO_HV,
	TEGRA_IO_PAD_BB,
	TEGRA_IO_PAD_CAM,
	TEGRA_IO_PAD_COMP,
	TEGRA_IO_PAD_CONN,
	TEGRA_IO_PAD_CSIA,
	TEGRA_IO_PAD_CSIB,
	TEGRA_IO_PAD_CSIC,
	TEGRA_IO_PAD_CSID,
	TEGRA_IO_PAD_CSIE,
	TEGRA_IO_PAD_CSIF,
	TEGRA_IO_PAD_CSIG,
	TEGRA_IO_PAD_CSIH,
	TEGRA_IO_PAD_DAP3,
	TEGRA_IO_PAD_DAP5,
	TEGRA_IO_PAD_DBG,
	TEGRA_IO_PAD_DEBUG_NONAO,
	TEGRA_IO_PAD_DMIC,
	TEGRA_IO_PAD_DMIC_HV,
	TEGRA_IO_PAD_DP,
	TEGRA_IO_PAD_DSI,
	TEGRA_IO_PAD_DSIB,
	TEGRA_IO_PAD_DSIC,
	TEGRA_IO_PAD_DSID,
	TEGRA_IO_PAD_EDP,
	TEGRA_IO_PAD_EMMC,
	TEGRA_IO_PAD_EMMC2,
	TEGRA_IO_PAD_EQOS,
	TEGRA_IO_PAD_GPIO,
	TEGRA_IO_PAD_GP_PWM2,
	TEGRA_IO_PAD_GP_PWM3,
	TEGRA_IO_PAD_HDMI,
	TEGRA_IO_PAD_HDMI_DP0,
	TEGRA_IO_PAD_HDMI_DP1,
	TEGRA_IO_PAD_HDMI_DP2,
	TEGRA_IO_PAD_HDMI_DP3,
	TEGRA_IO_PAD_HSIC,
	TEGRA_IO_PAD_HV,
	TEGRA_IO_PAD_LVDS,
	TEGRA_IO_PAD_MIPI_BIAS,
	TEGRA_IO_PAD_NAND,
	TEGRA_IO_PAD_PEX_BIAS,
	TEGRA_IO_PAD_PEX_CLK_BIAS,
	TEGRA_IO_PAD_PEX_CLK1,
	TEGRA_IO_PAD_PEX_CLK2,
	TEGRA_IO_PAD_PEX_CLK3,
	TEGRA_IO_PAD_PEX_CLK_2_BIAS,
	TEGRA_IO_PAD_PEX_CLK_2,
	TEGRA_IO_PAD_PEX_CNTRL,
	TEGRA_IO_PAD_PEX_CTL2,
	TEGRA_IO_PAD_PEX_L0_RST_N,
	TEGRA_IO_PAD_PEX_L1_RST_N,
	TEGRA_IO_PAD_PEX_L5_RST_N,
	TEGRA_IO_PAD_PWR_CTL,
	TEGRA_IO_PAD_SDMMC1,
	TEGRA_IO_PAD_SDMMC1_HV,
	TEGRA_IO_PAD_SDMMC2,
	TEGRA_IO_PAD_SDMMC2_HV,
	TEGRA_IO_PAD_SDMMC3,
	TEGRA_IO_PAD_SDMMC3_HV,
	TEGRA_IO_PAD_SDMMC4,
	TEGRA_IO_PAD_SOC_GPIO10,
	TEGRA_IO_PAD_SOC_GPIO12,
	TEGRA_IO_PAD_SOC_GPIO13,
	TEGRA_IO_PAD_SOC_GPIO53,
	TEGRA_IO_PAD_SPI,
	TEGRA_IO_PAD_SPI_HV,
	TEGRA_IO_PAD_SYS_DDC,
	TEGRA_IO_PAD_UART,
	TEGRA_IO_PAD_UART4,
	TEGRA_IO_PAD_UART5,
	TEGRA_IO_PAD_UFS,
	TEGRA_IO_PAD_USB0,
	TEGRA_IO_PAD_USB1,
	TEGRA_IO_PAD_USB2,
	TEGRA_IO_PAD_USB3,
	TEGRA_IO_PAD_USB_BIAS,
	TEGRA_IO_PAD_AO_HV,
};

/* Define reboot-reset mode */
#define RECOVERY_MODE           BIT(31)
#define BOOTLOADER_MODE         BIT(30)
#define UPDATE_MODE             BIT(29)
#define FORCED_RECOVERY_MODE    BIT(1)

/* deprecated, use TEGRA_IO_PAD_{HDMI,LVDS} instead */
#define TEGRA_IO_RAIL_HDMI	TEGRA_IO_PAD_HDMI
#define TEGRA_IO_RAIL_LVDS	TEGRA_IO_PAD_LVDS

#ifdef CONFIG_SOC_TEGRA_PMC
int tegra_powergate_power_on(unsigned int id);
int tegra_powergate_power_off(unsigned int id);
int tegra_powergate_remove_clamping(unsigned int id);

/* Must be called with clk disabled, and returns with clk enabled */
int tegra_powergate_sequence_power_up(unsigned int id, struct clk *clk,
				      struct reset_control *rst);

int tegra_io_pad_power_enable(enum tegra_io_pad id);
int tegra_io_pad_power_disable(enum tegra_io_pad id);

/* deprecated, use tegra_io_pad_power_{enable,disable}() instead */
int tegra_io_rail_power_on(unsigned int id);
int tegra_io_rail_power_off(unsigned int id);

void tegra_pmc_set_suspend_mode(enum tegra_suspend_mode mode);
void tegra_pmc_enter_suspend_mode(enum tegra_suspend_mode mode);

/* T210 USB2 SLEEPWALK APIs */
struct tegra_utmi_pad_config {
	u32 tctrl;
	u32 pctrl;
	u32 rpd_ctrl;
};
int tegra_pmc_utmi_phy_enable_sleepwalk(int port, enum usb_device_speed speed,
					struct tegra_utmi_pad_config *config);
int tegra_pmc_utmi_phy_disable_sleepwalk(int port);
int tegra_pmc_hsic_phy_enable_sleepwalk(int port);
int tegra_pmc_hsic_phy_disable_sleepwalk(int port);

int tegra_pmc_set_reboot_reason(u32 reboot_reason);
int tegra_pmc_clear_reboot_reason(u32 reboot_reason);

u32 tegra_pmc_gpu_clamp_enable(void);
u32 tegra_pmc_gpu_clamp_disable(void);

#else
static inline int tegra_powergate_power_on(unsigned int id)
{
	return -ENOSYS;
}

static inline int tegra_powergate_power_off(unsigned int id)
{
	return -ENOSYS;
}

static inline int tegra_powergate_remove_clamping(unsigned int id)
{
	return -ENOSYS;
}

static inline int tegra_powergate_sequence_power_up(unsigned int id,
						    struct clk *clk,
						    struct reset_control *rst)
{
	return -ENOSYS;
}

static inline int tegra_io_pad_power_enable(enum tegra_io_pad id)
{
	return -ENOSYS;
}

static inline int tegra_io_pad_power_disable(enum tegra_io_pad id)
{
	return -ENOSYS;
}

static inline int tegra_io_pad_get_voltage(enum tegra_io_pad id)
{
	return -ENOSYS;
}

static inline int tegra_io_rail_power_on(unsigned int id)
{
	return -ENOSYS;
}

static inline int tegra_io_rail_power_off(unsigned int id)
{
	return -ENOSYS;
}

static inline void tegra_pmc_set_suspend_mode(enum tegra_suspend_mode mode)
{
}

static inline void tegra_pmc_enter_suspend_mode(enum tegra_suspend_mode mode)
{
}

static inline int tegra_pmc_set_reboot_reason(u32 reboot_reason)
{
	return -ENOTSUPP;
}

static inline int tegra_pmc_clear_reboot_reason(u32 reboot_reason)
{
	return -ENOTSUPP;
}

static inline u32 tegra_pmc_gpu_clamp_enable(void)
{
	return 0;
}
static inline u32 tegra_pmc_gpu_clamp_disable(void)
{
	return 0;
}

#endif /* CONFIG_SOC_TEGRA_PMC */

void tegra_pmc_fuse_control_ps18_latch_set(void);
void tegra_pmc_fuse_control_ps18_latch_clear(void);

void tegra_pmc_fuse_disable_mirroring(void);
void tegra_pmc_fuse_enable_mirroring(void);
#if defined(CONFIG_SOC_TEGRA_PMC) && defined(CONFIG_PM_SLEEP)
enum tegra_suspend_mode tegra_pmc_get_suspend_mode(void);
#else
static inline enum tegra_suspend_mode tegra_pmc_get_suspend_mode(void)
{
	return TEGRA_SUSPEND_NONE;
}
#endif

void tegra_io_pad_pinconf_dbg_show(struct pinctrl_dev *pctldev,
				struct seq_file *s, unsigned int pin);
int tegra_pmc_nvcsi_brick_getstatus(const char *pad_name);
int tegra_pmc_nvcsi_ab_brick_dpd_enable(void);
int tegra_pmc_nvcsi_cdef_brick_dpd_enable(void);
int tegra_pmc_nvcsi_ab_brick_dpd_disable(void);
int tegra_pmc_nvcsi_cdef_brick_dpd_disable(void);

int tegra_pmc_save_se_context_buffer_address(u32 add);
u32 tegra_pmc_get_se_context_buffer_address(void);
bool tegra_pmc_is_halt_in_fiq(void);

void tegra_pmc_sata_pwrgt_update(unsigned long mask, unsigned long val);
unsigned long tegra_pmc_sata_pwrgt_get(void);

void tegra_pmc_write_bootrom_command(u32 command_offset, unsigned long val);
void tegra_pmc_reset_system(void);

int tegra_pmc_pwm_blink_enable(void);
int tegra_pmc_pwm_blink_disable(void);
int tegra_pmc_pwm_blink_config(int duty_ns, int period_ns);

int tegra_pmc_soft_led_blink_enable(void);
int tegra_pmc_soft_led_blink_disable(void);
int tegra_pmc_soft_led_blink_configure(int duty_cycle_ns, int ll_period_ns,
				int ramp_time_ns);
int tegra_pmc_soft_led_blink_set_ramptime(int ramp_time_ns);
int tegra_pmc_soft_led_blink_set_short_period(int short_low_period_ns);

u32 tegra_pmc_aotag_readl(unsigned long offset);
void tegra_pmc_aotag_writel(u32 value, unsigned long offset);
#endif /* __SOC_TEGRA_PMC_H__ */
