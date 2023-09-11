// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2010 Google, Inc.
 * Copyright (c) 2012-2023, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/iopoll.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/platform/tegra/emc_bwmgr.h>
#include <linux/reset.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/slot-gpio.h>
#include <linux/mmc/sdhci-tegra-notify.h>
#include <linux/gpio/consumer.h>
#include <linux/ktime.h>
#include <linux/tegra_prod.h>
#include <linux/debugfs.h>
#include <linux/stat.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/iommu.h>

#include <linux/uaccess.h>
#include <linux/fs.h>
#include <soc/tegra/fuse.h>
#include <soc/tegra/padctrl.h>
#include <linux/pm_runtime.h>

#include "sdhci-pltfm.h"
#include "cqhci.h"

/* Tegra SDHOST controller vendor register definitions */
#define SDHCI_TEGRA_VENDOR_CLOCK_CTRL			0x100
#define SDHCI_CLOCK_CTRL_TAP_MASK			0x00ff0000
#define SDHCI_CLOCK_CTRL_TAP_SHIFT			16
#define SDHCI_CLOCK_CTRL_TRIM_MASK			0x1f000000
#define SDHCI_CLOCK_CTRL_TRIM_SHIFT			24
#define SDHCI_CLOCK_CTRL_LEGACY_CLKEN_OVERRIDE		BIT(6)
#define SDHCI_CLOCK_CTRL_SDR50_TUNING_OVERRIDE		BIT(5)
#define SDHCI_CLOCK_CTRL_PADPIPE_CLKEN_OVERRIDE		BIT(3)
#define SDHCI_CLOCK_CTRL_SPI_MODE_CLKEN_OVERRIDE	BIT(2)
#define SDHCI_CLOCK_CTRL_SDMMC_CLK			BIT(0)

#define SDHCI_TEGRA_VENDOR_SYS_SW_CTRL			0x104
#define SDHCI_TEGRA_SYS_SW_CTRL_ENHANCED_STROBE		BIT(31)

#define SDHCI_TEGRA_VENDOR_ERR_INTR_STATUS		0x108

#define SDHCI_TEGRA_VENDOR_CAP_OVERRIDES		0x10c
#define SDHCI_TEGRA_CAP_OVERRIDES_DQS_TRIM_MASK		0x00003f00
#define SDHCI_TEGRA_CAP_OVERRIDES_DQS_TRIM_SHIFT	8

#define SDHCI_TEGRA_VENDOR_MISC_CTRL			0x120
#define SDHCI_MISC_CTRL_ERASE_TIMEOUT_LIMIT		BIT(0)
#define SDHCI_MISC_CTRL_ENABLE_SDR104			0x8
#define SDHCI_MISC_CTRL_ENABLE_SDR50			0x10
#define SDHCI_MISC_CTRL_ENABLE_SDHCI_SPEC_300		0x20
#define SDHCI_MISC_CTRL_ENABLE_DDR50			0x200
#define SDHCI_MISC_CTRL_SDMMC_SPARE_0_MASK	0xFFFE

#define SDHCI_TEGRA_VENDOR_MISC_CTRL_1		0x124

#define SDHCI_TEGRA_VENDOR_MISC_CTRL_2		0x128
#define SDHCI_MISC_CTRL_2_CLK_OVR_ON		0x40000000


#define SDHCI_TEGRA_VENDOR_IO_TRIM_CTRL_0		0x1AC
#define SDHCI_TEGRA_IO_TRIM_CTRL_0_SEL_VREG_MASK	0x4

#define SDHCI_TEGRA_VENDOR_DLLCAL_CFG			0x1b0
#define SDHCI_TEGRA_DLLCAL_CALIBRATE			BIT(31)

#define SDHCI_TEGRA_VENDOR_DLLCAL_STA			0x1bc
#define SDHCI_TEGRA_DLLCAL_STA_ACTIVE			BIT(31)

#define SDHCI_VNDR_TUN_CTRL0_0				0x1c0
#define SDHCI_VNDR_TUN_CTRL0_TUN_HW_TAP			0x20000
#define SDHCI_VNDR_TUN_CTRL0_START_TAP_VAL_MASK		0x03fc0000
#define SDHCI_VNDR_TUN_CTRL0_START_TAP_VAL_SHIFT	18
#define SDHCI_VNDR_TUN_CTRL0_MUL_M_MASK			0x00001fc0
#define SDHCI_VNDR_TUN_CTRL0_MUL_M_SHIFT		6
#define SDHCI_VNDR_TUN_CTRL0_TUN_ITER_MASK		0x000e000
#define SDHCI_VNDR_TUN_CTRL0_TUN_ITER_SHIFT		13
#define TRIES_128					2
#define TRIES_256					4
#define SDHCI_VNDR_TUN_CTRL0_TUN_WORD_SEL_MASK		0x7

#define SDHCI_TEGRA_VNDR_TUN_CTRL1_0			0x1c4
#define SDHCI_TEGRA_VNDR_TUN_CTRL1_DQ_OFF_MASK          0xc0000000
#define SDHCI_TEGRA_VNDR_TUN_CTRL1_DQ_OFF_SHIFT         30
#define SDHCI_TEGRA_VNDR_TUN_STATUS0			0x1C8
#define SDHCI_TEGRA_VNDR_TUN_STATUS1			0x1CC
#define SDHCI_TEGRA_VNDR_TUN_STATUS1_TAP_MASK		0xFF
#define SDHCI_TEGRA_VNDR_TUN_STATUS1_END_TAP_SHIFT	0x8
#define TUNING_WORD_BIT_SIZE				32

#define SDHCI_TEGRA_VNDR_TUNING_STATUS0		0x1C8

#define SDHCI_TEGRA_VNDR_TUNING_STATUS1		0x1CC
#define SDHCI_TEGRA_VNDR_TUNING_STATUS1_TAP_MASK	0xFF
#define SDHCI_TEGRA_VNDR_TUNING_STATUS1_END_TAP_SHIFT	8

#define SDHCI_TEGRA_AUTO_CAL_CONFIG			0x1e4
#define SDHCI_AUTO_CAL_START				BIT(31)
#define SDHCI_AUTO_CAL_ENABLE				BIT(29)
#define SDHCI_AUTO_CAL_PDPU_OFFSET_MASK			0x0000ffff

#define SDHCI_TEGRA_SDMEM_COMP_PADCTRL			0x1e0
#define SDHCI_TEGRA_SDMEM_COMP_PADCTRL_VREF_SEL_MASK	0x0000000f
#define SDHCI_TEGRA_SDMEM_COMP_PADCTRL_VREF_SEL_VAL	0x7
#define SDHCI_TEGRA_SDMEM_COMP_PADCTRL_E_INPUT_E_PWRD	BIT(31)
#define SDHCI_COMP_PADCTRL_DRVUPDN_OFFSET_MASK		0x07FFF000

#define SDHCI_TEGRA_AUTO_CAL_STATUS			0x1ec
#define SDHCI_TEGRA_AUTO_CAL_ACTIVE			BIT(31)

#define SDHCI_TEGRA_CIF2AXI_CTRL_0			0x1fc

#define NVQUIRK_FORCE_SDHCI_SPEC_200			BIT(0)
#define NVQUIRK_ENABLE_BLOCK_GAP_DET			BIT(1)
#define NVQUIRK_ENABLE_SDHCI_SPEC_300			BIT(2)
#define NVQUIRK_ENABLE_SDR50				BIT(3)
#define NVQUIRK_ENABLE_SDR104				BIT(4)
#define NVQUIRK_ENABLE_DDR50				BIT(5)
/*
 * HAS_PADCALIB NVQUIRK is for SoC's supporting auto calibration of pads
 * drive strength.
 */
#define NVQUIRK_HAS_PADCALIB				BIT(6)
/*
 * NEEDS_PAD_CONTROL NVQUIRK is for SoC's having separate 3V3 and 1V8 pads.
 * 3V3/1V8 pad selection happens through pinctrl state selection depending
 * on the signaling mode.
 */
#define NVQUIRK_NEEDS_PAD_CONTROL			BIT(7)
#define NVQUIRK_DIS_CARD_CLK_CONFIG_TAP			BIT(8)
#define NVQUIRK_CQHCI_DCMD_R1B_CMD_TIMING		BIT(9)
#define NVQUIRK_HW_TAP_CONFIG				BIT(10)
#define NVQUIRK_SDMMC_CLK_OVERRIDE			BIT(11)
#define NVQUIRK_UPDATE_PIN_CNTRL_REG			BIT(12)
#define NVQUIRK_CONTROL_TRIMMER_SUPPLY			BIT(13)
/*
 * NVQUIRK_HAS_TMCLK is for SoC's having separate timeout clock for Tegra
 * SDMMC hardware data timeout.
 */
#define NVQUIRK_HAS_TMCLK				BIT(14)
#define NVQUIRK_ENABLE_PERIODIC_CALIB			BIT(15)
#define NVQUIRK_ENABLE_TUNING_DQ_OFFSET			BIT(16)
#define NVQUIRK_PROGRAM_MC_STREAMID			BIT(17)

#define SDHCI_TEGRA_FALLBACK_CLK_HZ			400000

#define MAX_TAP_VALUE		256

/* Set min identification clock of 400 KHz */
#define SDMMC_TIMEOUT_CLK_FREQ_MHZ	12

/* uhs mask can be used to mask any of the UHS modes support */
#define MMC_UHS_MASK_SDR12	0x1
#define MMC_UHS_MASK_SDR25	0x2
#define MMC_UHS_MASK_SDR50	0x4
#define MMC_UHS_MASK_DDR50	0x8
#define MMC_UHS_MASK_SDR104	0x10
#define MMC_MASK_HS200		0x20
#define MMC_MASK_HS400		0x40
#define MMC_MASK_SD_HS		0x80

static char prod_device_states[MMC_TIMING_COUNTER][20] = {
	"prod_c_ds", /* MMC_TIMING_LEGACY */
	"prod_c_hs", /* MMC_TIMING_MMC_HS */
	"prod_c_hs", /* MMC_TIMING_SD_HS */
	"prod_c_sdr12", /* MMC_TIMING_UHS_SDR12 */
	"prod_c_sdr25", /* MMC_TIMING_UHS_SDR25 */
	"prod_c_sdr50", /* MMC_TIMING_UHS_SDR50 */
	"prod_c_sdr104", /* MMC_TIMING_UHS_SDR104 */
	"prod_c_ddr52", /* MMC_TIMING_UHS_DDR50 */
	"prod_c_ddr52", /* MMC_TIMING_MMC_DDR52 */
	"prod_c_hs200", /* MMC_TIMING_MMC_HS200 */
	"prod_c_hs400", /* MMC_TIMING_MMC_HS400 */
};

#define SDHCI_TEGRA_FALLBACK_CLK_HZ			400000
#define SDHCI_TEGRA_RTPM_TIMEOUT_MS			10
#define SDMMC_EMC_MAX_FREQ      			150000000

/* SDMMC CQE Base Address for Tegra Host Ver 4.1 and Higher */
#define SDHCI_TEGRA_CQE_BASE_ADDR			0xF000

#define SDHCI_TEGRA_CQE_TRNS_MODE	(SDHCI_TRNS_MULTI | \
					 SDHCI_TRNS_BLK_CNT_EN | \
					 SDHCI_TRNS_DMA)

static unsigned int sdmmc_emc_client_id[] = {
	TEGRA_BWMGR_CLIENT_SDMMC1,
	TEGRA_BWMGR_CLIENT_SDMMC2,
	TEGRA_BWMGR_CLIENT_SDMMC3,
	TEGRA_BWMGR_CLIENT_SDMMC4
};

struct sdhci_tegra_soc_data {
	const struct sdhci_pltfm_data *pdata;
	u64 dma_mask;
	u32 nvquirks;
	u8 min_tap_delay;
	u8 max_tap_delay;
	unsigned int min_host_clk;
	bool use_bwmgr;
};

/* Magic pull up and pull down pad calibration offsets */
struct sdhci_tegra_autocal_offsets {
	u32 pull_up_3v3;
	u32 pull_down_3v3;
	u32 pull_up_3v3_timeout;
	u32 pull_down_3v3_timeout;
	u32 pull_up_1v8;
	u32 pull_down_1v8;
	u32 pull_up_1v8_timeout;
	u32 pull_down_1v8_timeout;
	u32 pull_up_sdr104;
	u32 pull_down_sdr104;
	u32 pull_up_hs400;
	u32 pull_down_hs400;
};

static void tegra_sdhci_set_dqs_trim(struct sdhci_host *host, u8 trim);
static int unregister_notifier_to_sd(struct sdhci_host *host);
static int tegra_sdhci_pre_sd_exp_card_init(struct sdhci_host *host,
					    int val, unsigned int mask);
static int notifier_from_sd_call_chain(struct sdhci_host *host, int value);

struct sdhci_tegra {
	const struct sdhci_tegra_soc_data *soc_data;
	struct gpio_desc *power_gpio;
	struct clk *tmclk;
	bool ddr_signaling;
	bool pad_calib_required;
	bool pad_control_available;
	struct dentry *sdhcid;
	struct reset_control *rst;
	struct pinctrl *pinctrl_sdmmc;
	struct pinctrl_state *pinctrl_state_3v3;
	struct pinctrl_state *pinctrl_state_1v8;
	struct pinctrl_state *pinctrl_state_3v3_drv;
	struct pinctrl_state *pinctrl_state_1v8_drv;
	struct pinctrl_state *pinctrl_state_sdexp_disable;
	struct pinctrl_state *pinctrl_state_sdexp_enable;
	bool slcg_status;
	unsigned int tuning_status;
	#define TUNING_STATUS_DONE	1
	#define TUNING_STATUS_RETUNE	2
	struct sdhci_tegra_autocal_offsets autocal_offsets;
	ktime_t last_calib;
	struct tegra_bwmgr_client *emc_clk;
	u32 default_tap;
	u32 default_trim;
	u32 dqs_trim;
	bool enable_hwcq;
	unsigned long curr_clk_rate;
	u8 tuned_tap_delay;
	struct tegra_prod *prods;
	struct pinctrl_state *schmitt_enable[2];
	struct pinctrl_state *schmitt_disable[2];
	u8 uhs_mask;
	bool force_non_rem_rescan;
	int volt_switch_gpio;
	unsigned int cd_irq;
	int cd_gpio;
	bool cd_wakeup_capable;
	bool is_rail_enabled;
	bool en_periodic_cflush;
	bool disable_rtpm;
	struct sdhci_host *host;
	struct delayed_work detect_delay;
	u32 boot_detect_delay;
	unsigned long max_clk_limit;
	unsigned long max_ddr_clk_limit;
	unsigned int instance;
	bool skip_clk_rst;
	int mux_sel_gpio;
	struct blocking_notifier_head notifier_from_sd;
	struct blocking_notifier_head notifier_to_sd;
	struct notifier_block notifier;
	bool sd_exp_support;
	bool is_probe_done;
	bool defer_calib;
	bool wake_enable_failed;
	bool enable_cqic;
	u32 streamid;
};

static void sdhci_tegra_debugfs_init(struct sdhci_host *host);

/* Module params */
static unsigned int en_boot_part_access;

static void tegra_sdhci_update_sdmmc_pinctrl_register(struct sdhci_host *sdhci,
               bool set);
static bool tegra_sdhci_skip_retuning(struct sdhci_host *host);

static u16 tegra_sdhci_readw(struct sdhci_host *host, int reg)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;

	if (tegra_platform_is_vsp() && (reg > SDHCI_HOST_VERSION))
		return 0;

	if (unlikely((soc_data->nvquirks & NVQUIRK_FORCE_SDHCI_SPEC_200) &&
			(reg == SDHCI_HOST_VERSION))) {
		/* Erratum: Version register is invalid in HW. */
		return SDHCI_SPEC_200;
	}

	return readw(host->ioaddr + reg);
}

static void tegra_sdhci_writew(struct sdhci_host *host, u16 val, int reg)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);

	if (tegra_platform_is_vsp() && (reg > SDHCI_HOST_VERSION))
		return;

	switch (reg) {
	case SDHCI_TRANSFER_MODE:
		/*
		 * Postpone this write, we must do it together with a
		 * command write that is down below.
		 */
		pltfm_host->xfer_mode_shadow = val;
		return;
	case SDHCI_COMMAND:
		writel((val << 16) | pltfm_host->xfer_mode_shadow,
			host->ioaddr + SDHCI_TRANSFER_MODE);
		return;
	}

	writew(val, host->ioaddr + reg);
}

static void tegra_sdhci_writel(struct sdhci_host *host, u32 val, int reg)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;

	if (tegra_platform_is_vsp() && (reg > SDHCI_HOST_VERSION))
		return;
	/* Seems like we're getting spurious timeout and crc errors, so
	 * disable signalling of them. In case of real errors software
	 * timers should take care of eventually detecting them.
	 */
	if (unlikely(reg == SDHCI_SIGNAL_ENABLE))
		val &= ~(SDHCI_INT_TIMEOUT|SDHCI_INT_CRC);

	writel(val, host->ioaddr + reg);

	if (unlikely((soc_data->nvquirks & NVQUIRK_ENABLE_BLOCK_GAP_DET) &&
			(reg == SDHCI_INT_ENABLE))) {
		/* Erratum: Must enable block gap interrupt detection */
		u8 gap_ctrl = readb(host->ioaddr + SDHCI_BLOCK_GAP_CONTROL);
		if (val & SDHCI_INT_CARD_INT)
			gap_ctrl |= 0x8;
		else
			gap_ctrl &= ~0x8;
		writeb(gap_ctrl, host->ioaddr + SDHCI_BLOCK_GAP_CONTROL);
	}
}

static void tegra_sdhci_dump_vendor_regs(struct sdhci_host *host)
{
	int reg, tuning_status;
	u32 tap_delay;
	u32 trim_delay;
	u8 i;

	pr_debug("======= %s: Tuning windows =======\n",
		mmc_hostname(host->mmc));
	reg = sdhci_readl(host, SDHCI_VNDR_TUN_CTRL0_0);
	for (i = 0; i <= SDHCI_VNDR_TUN_CTRL0_TUN_WORD_SEL_MASK; i++) {
		reg &= ~SDHCI_VNDR_TUN_CTRL0_TUN_WORD_SEL_MASK;
		reg |= i;
		sdhci_writel(host, reg, SDHCI_VNDR_TUN_CTRL0_0);
		tuning_status = sdhci_readl(host,
					SDHCI_TEGRA_VNDR_TUNING_STATUS0);
		pr_debug("%s: tuning window[%d]: %#x\n",
			mmc_hostname(host->mmc), i, tuning_status);
	}
	reg = sdhci_readl(host, SDHCI_TEGRA_VENDOR_CLOCK_CTRL);
	tap_delay = reg & SDHCI_CLOCK_CTRL_TAP_MASK;
	tap_delay >>= SDHCI_CLOCK_CTRL_TAP_SHIFT;
	trim_delay = reg & SDHCI_CLOCK_CTRL_TRIM_MASK;
	trim_delay >>= SDHCI_CLOCK_CTRL_TRIM_SHIFT;
	pr_debug("sdhci: Tap value: %u | Trim value: %u\n", tap_delay,
			trim_delay);
	pr_debug("==================================\n");

	pr_debug("Vendor clock ctrl: %#x\n",
		sdhci_readl(host, SDHCI_TEGRA_VENDOR_CLOCK_CTRL));
	pr_debug("Vendor SysSW ctrl: %#x\n",
		sdhci_readl(host, SDHCI_TEGRA_VENDOR_SYS_SW_CTRL));
	pr_debug("Vendor Err interrupt status : %#x\n",
		sdhci_readl(host, SDHCI_TEGRA_VENDOR_ERR_INTR_STATUS));
	pr_debug("Vendor Cap overrides: %#x\n",
		sdhci_readl(host, SDHCI_TEGRA_VENDOR_CAP_OVERRIDES));
	pr_debug("Vendor Misc ctrl: %#x\n",
		sdhci_readl(host, SDHCI_TEGRA_VENDOR_MISC_CTRL));
	pr_debug("Vendor Misc ctrl_1: %#x\n",
		sdhci_readl(host, SDHCI_TEGRA_VENDOR_MISC_CTRL_1));
	pr_debug("Vendor Misc ctrl_2: %#x\n",
		sdhci_readl(host, SDHCI_TEGRA_VENDOR_MISC_CTRL_2));
	pr_debug("Vendor IO trim ctrl: %#x\n",
		sdhci_readl(host, SDHCI_TEGRA_VENDOR_IO_TRIM_CTRL_0));
	pr_debug("Vendor Tuning ctrl: %#x\n",
		sdhci_readl(host, SDHCI_VNDR_TUN_CTRL0_0));
	pr_debug("SDMEM comp padctrl: %#x\n",
		sdhci_readl(host, SDHCI_TEGRA_SDMEM_COMP_PADCTRL));
	pr_debug("Autocal config: %#x\n",
		sdhci_readl(host, SDHCI_TEGRA_AUTO_CAL_CONFIG));
	pr_debug("Autocal status: %#x\n",
		sdhci_readl(host, SDHCI_TEGRA_AUTO_CAL_STATUS));
}

static bool tegra_sdhci_configure_card_clk(struct sdhci_host *host, bool enable)
{
	bool status;
	u32 reg;

	reg = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	status = !!(reg & SDHCI_CLOCK_CARD_EN);

	if (status == enable)
		return status;

	if (enable)
		reg |= SDHCI_CLOCK_CARD_EN;
	else
		reg &= ~SDHCI_CLOCK_CARD_EN;

	sdhci_writew(host, reg, SDHCI_CLOCK_CONTROL);

	return status;
}

static void tegra210_sdhci_writew(struct sdhci_host *host, u16 val, int reg)
{
	bool is_tuning_cmd = 0;
	bool clk_enabled;
	u8 cmd;

	if (reg == SDHCI_COMMAND) {
		cmd = SDHCI_GET_CMD(val);
		is_tuning_cmd = cmd == MMC_SEND_TUNING_BLOCK ||
				cmd == MMC_SEND_TUNING_BLOCK_HS200;
	}

	if (is_tuning_cmd)
		clk_enabled = tegra_sdhci_configure_card_clk(host, 0);

	writew(val, host->ioaddr + reg);

	if (is_tuning_cmd) {
		udelay(1);
		sdhci_reset(host, SDHCI_RESET_CMD | SDHCI_RESET_DATA);
		tegra_sdhci_configure_card_clk(host, clk_enabled);
	}
}

static unsigned int tegra_sdhci_get_ro(struct sdhci_host *host)
{
	/*
	 * Write-enable shall be assumed if GPIO is missing in a board's
	 * device-tree because SDHCI's WRITE_PROTECT bit doesn't work on
	 * Tegra.
	 */
	return mmc_gpio_get_ro(host->mmc);
}

static bool tegra_sdhci_is_pad_and_regulator_valid(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	int has_1v8, has_3v3;

	/*
	 * The SoCs which have NVQUIRK_NEEDS_PAD_CONTROL require software pad
	 * voltage configuration in order to perform voltage switching. This
	 * means that valid pinctrl info is required on SDHCI instances capable
	 * of performing voltage switching. Whether or not an SDHCI instance is
	 * capable of voltage switching is determined based on the regulator.
	 */

	if (!(tegra_host->soc_data->nvquirks & NVQUIRK_NEEDS_PAD_CONTROL))
		return true;

	if (IS_ERR_OR_NULL(host->mmc->supply.vqmmc))
		return false;

	has_1v8 = regulator_is_supported_voltage(host->mmc->supply.vqmmc,
						 1700000, 1950000);

	has_3v3 = regulator_is_supported_voltage(host->mmc->supply.vqmmc,
						 2700000, 3600000);

	if (has_1v8 == 1 && has_3v3 == 1)
		return tegra_host->pad_control_available;

	/* Fixed voltage, no pad control required. */
	return true;
}

static void tegra_sdhci_set_tap(struct sdhci_host *host, unsigned int tap)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;
	bool card_clk_enabled = false;
	u32 reg;

	if (tap > MAX_TAP_VALUE) {
		dev_err(mmc_dev(host->mmc), "Invalid tap value %d\n", tap);
		return;
	}

	/*
	 * Touching the tap values is a bit tricky on some SoC generations.
	 * The quirk enables a workaround for a glitch that sometimes occurs if
	 * the tap values are changed.
	 */

	if (soc_data->nvquirks & NVQUIRK_DIS_CARD_CLK_CONFIG_TAP)
		card_clk_enabled = tegra_sdhci_configure_card_clk(host, false);

	/* Disable HW tap delay config */
	if (soc_data->nvquirks & NVQUIRK_HW_TAP_CONFIG) {
		reg = sdhci_readl(host, SDHCI_VNDR_TUN_CTRL0_0);
		reg &= ~SDHCI_VNDR_TUN_CTRL0_TUN_HW_TAP;
		sdhci_writel(host, reg, SDHCI_VNDR_TUN_CTRL0_0);
	}

	reg = sdhci_readl(host, SDHCI_TEGRA_VENDOR_CLOCK_CTRL);
	reg &= ~SDHCI_CLOCK_CTRL_TAP_MASK;
	reg |= tap << SDHCI_CLOCK_CTRL_TAP_SHIFT;
	sdhci_writel(host, reg, SDHCI_TEGRA_VENDOR_CLOCK_CTRL);

	if (soc_data->nvquirks & NVQUIRK_DIS_CARD_CLK_CONFIG_TAP &&
	    card_clk_enabled) {
		udelay(1);
		sdhci_reset(host, SDHCI_RESET_CMD | SDHCI_RESET_DATA);
		tegra_sdhci_configure_card_clk(host, card_clk_enabled);
	}
}

static void tegra_sdhci_apply_tuning_correction(struct sdhci_host *host,
		u16 tun_iter, u8 upthres, u8 lowthres, u8 fixed_tap)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	u32 reg, mask = 0x00000001;
	u8 i, j, edge1;
	unsigned int tun_word;
	bool start_fail_def = false;
	bool start_pass_def = false;
	bool end_fail_def = false;
	bool end_pass_def = false;
	bool first_pass_def = false;
	bool first_fail_def = false;
	u8 start_fail = 0;
	u8 end_fail = 0;
	u8 start_pass = 0;
	u8 end_pass = 0;
	u8 first_fail = 0;
	u8 first_pass = 0;

	/*Select the first valid window with starting and ending edges defined*/
	for (i = 0; i <= SDHCI_VNDR_TUN_CTRL0_TUN_WORD_SEL_MASK; i++) {
		if (i == (tun_iter/TUNING_WORD_BIT_SIZE))
			break;
		j = 0;
		reg = sdhci_readl(host, SDHCI_VNDR_TUN_CTRL0_0);
		reg &= ~SDHCI_VNDR_TUN_CTRL0_TUN_WORD_SEL_MASK;
		reg |= i;
		sdhci_writel(host, reg, SDHCI_VNDR_TUN_CTRL0_0);
		tun_word = sdhci_readl(host, SDHCI_TEGRA_VNDR_TUNING_STATUS0);
		while (j <= (TUNING_WORD_BIT_SIZE - 1)) {
			if (!(tun_word & (mask << j)) && !start_fail_def) {
				start_fail = i*TUNING_WORD_BIT_SIZE + j;
				start_fail_def = true;
				if (!first_fail_def) {
					first_fail = start_fail;
					first_fail_def = true;
				}
			} else if ((tun_word & (mask << j)) && !start_pass_def
					&& start_fail_def) {
				start_pass = i*TUNING_WORD_BIT_SIZE + j;
				start_pass_def = true;
				if (!first_pass_def) {
					first_pass = start_pass;
					first_pass_def = true;
				}
			} else if (!(tun_word & (mask << j)) && start_fail_def
					&& start_pass_def && !end_pass_def) {
				end_pass = i*TUNING_WORD_BIT_SIZE + j - 1;
				end_pass_def = true;
			} else if ((tun_word & (mask << j)) && start_pass_def
				&& start_fail_def && end_pass_def) {
				end_fail  = i*TUNING_WORD_BIT_SIZE + j - 1;
				end_fail_def = true;
				if ((end_pass - start_pass) >= upthres) {
					start_fail = end_pass + 1;
					start_pass = end_fail + 1;
					end_pass_def = false;
					end_fail_def = false;
					j++;
					continue;
				} else if ((end_pass - start_pass) < lowthres) {
					start_pass = end_fail + 1;
					end_pass_def = false;
					end_fail_def = false;
					j++;
					continue;
				}
				break;
			}
			j++;
			if ((i*TUNING_WORD_BIT_SIZE + j) == tun_iter - 1)
				break;
		}
		if (start_pass_def && end_pass_def && start_fail_def
				&& end_fail_def) {
			tegra_host->tuned_tap_delay = start_pass +
				(end_pass - start_pass)/2;
			return;
		}
	}
	/*
	 * If no edge found, retain tap set by HW tuning
	 */
	if (!first_fail_def) {
		WARN_ON("No edge detected\n");
		reg = sdhci_readl(host, SDHCI_TEGRA_VENDOR_CLOCK_CTRL);
		tegra_host->tuned_tap_delay =
			((reg & SDHCI_CLOCK_CTRL_TAP_MASK) >>
				SDHCI_CLOCK_CTRL_TAP_SHIFT);
	}
	/*
	 * Set tap based on fixed value relative to first edge
	 * if no valid windows found
	 */
	if (!end_fail_def && first_fail_def && first_pass_def) {
		edge1 = first_fail + (first_pass - first_fail)/2;
		if ((edge1 - 1) > fixed_tap)
			tegra_host->tuned_tap_delay = edge1 - fixed_tap;
		else
			tegra_host->tuned_tap_delay = edge1 + fixed_tap;
	}

}

static void tegra_sdhci_post_tuning(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;
	u32 reg;
	u8 start_tap, end_tap, window_width, upperthreshold, lowerthreshold;
	u8 fixed_tap;
	u16 num_tun_iter;
	u32 clk_rate_mhz, period, bestcase, worstcase;
	u32 avg_tap_delay, min_tap_delay, max_tap_delay;

	min_tap_delay = soc_data->min_tap_delay;
	max_tap_delay = soc_data->max_tap_delay;
	if (!min_tap_delay || !max_tap_delay) {
		pr_info("%s: Tuning correction cannot be applied",
				mmc_hostname(host->mmc));
		goto retain_hw_tun_tap;
	}

	clk_rate_mhz = tegra_host->curr_clk_rate/1000000;
	period = 1000000/clk_rate_mhz;
	bestcase = period/min_tap_delay;
	worstcase = period/max_tap_delay;
	avg_tap_delay = (period*2)/(min_tap_delay + max_tap_delay);
	upperthreshold = ((2*worstcase) + bestcase)/2;
	lowerthreshold = worstcase/4;
	fixed_tap = avg_tap_delay/2;

	reg = sdhci_readl(host, SDHCI_TEGRA_VNDR_TUNING_STATUS1);
	start_tap = reg & SDHCI_TEGRA_VNDR_TUNING_STATUS1_TAP_MASK;
	end_tap = (reg >> SDHCI_TEGRA_VNDR_TUNING_STATUS1_END_TAP_SHIFT) &
			SDHCI_TEGRA_VNDR_TUNING_STATUS1_TAP_MASK;
	window_width = end_tap - start_tap;

	num_tun_iter = (sdhci_readl(host, SDHCI_VNDR_TUN_CTRL0_0) &
				SDHCI_VNDR_TUN_CTRL0_TUN_ITER_MASK) >>
				SDHCI_VNDR_TUN_CTRL0_TUN_ITER_SHIFT;

	switch (num_tun_iter) {
	case 0:
		num_tun_iter = 40;
		break;
	case 1:
		num_tun_iter = 64;
		break;
	case 2:
		num_tun_iter = 128;
		break;
	case 3:
		num_tun_iter = 196;
		break;
	case 4:
		num_tun_iter = 256;
		break;
	default:
		WARN_ON("Invalid value of number of tuning iterations");
	}
	/*
	 * Apply tuning correction if partial window is selected by HW tuning
	 * or window merge is detected
	 */
	if ((start_tap == 0) || (end_tap == 254) ||
	   ((end_tap == 126) && (num_tun_iter == 128)) ||
	   (end_tap == (num_tun_iter - 1)) ||
	   (window_width >= upperthreshold)) {
		tegra_sdhci_dump_vendor_regs(host);
		pr_info("%s: Applying tuning correction\n",
				mmc_hostname(host->mmc));
		tegra_sdhci_apply_tuning_correction(host, num_tun_iter,
				upperthreshold, lowerthreshold, fixed_tap);
		pr_info("%s: Tap value after applying correction %u\n",
			mmc_hostname(host->mmc), tegra_host->tuned_tap_delay);
	} else {
retain_hw_tun_tap:
		reg = sdhci_readl(host, SDHCI_TEGRA_VENDOR_CLOCK_CTRL);
		tegra_host->tuned_tap_delay =
			((reg & SDHCI_CLOCK_CTRL_TAP_MASK) >>
				SDHCI_CLOCK_CTRL_TAP_SHIFT);
	}
	tegra_sdhci_set_tap(host, tegra_host->tuned_tap_delay);
	tegra_host->tuning_status = TUNING_STATUS_DONE;

	pr_debug("%s: hw tuning done ...\n", mmc_hostname(host->mmc));
	tegra_sdhci_dump_vendor_regs(host);
}

static void tegra_sdhci_mask_host_caps(struct sdhci_host *host, u8 uhs_mask)
{
	/* Mask any bus speed modes if set in platform data */
	if (uhs_mask & MMC_UHS_MASK_SDR12)
		host->mmc->caps &= ~MMC_CAP_UHS_SDR12;

	if (uhs_mask & MMC_UHS_MASK_SDR25)
		host->mmc->caps &= ~MMC_CAP_UHS_SDR25;

	if (uhs_mask & MMC_UHS_MASK_SDR50)
		host->mmc->caps &= ~MMC_CAP_UHS_SDR50;

	if (uhs_mask & MMC_UHS_MASK_SDR104)
		host->mmc->caps &= ~MMC_CAP_UHS_SDR104;

	if (uhs_mask & MMC_UHS_MASK_DDR50) {
		host->mmc->caps &= ~MMC_CAP_UHS_DDR50;
		host->mmc->caps &= ~MMC_CAP_1_8V_DDR;
	}

	if (uhs_mask & MMC_MASK_HS200) {
		host->mmc->caps2 &= ~MMC_CAP2_HS200;
		host->mmc->caps2 &= ~MMC_CAP2_HS400;
		host->mmc->caps2 &= ~MMC_CAP2_HS400_ES;
	}

	if (uhs_mask & MMC_MASK_HS400) {
		host->mmc->caps2 &= ~MMC_CAP2_HS400;
		host->mmc->caps2 &= ~MMC_CAP2_HS400_ES;
	}
	if (uhs_mask & MMC_MASK_SD_HS)
		host->mmc->caps &= ~MMC_CAP_SD_HIGHSPEED;
}

static void tegra_sdhci_reset(struct sdhci_host *host, u8 mask)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;
	u32 misc_ctrl, clk_ctrl, pad_ctrl, misc_ctrl_2;
	int err;
	bool clear_ddr_signalling = true;

	sdhci_reset(host, mask);

	if (!(mask & SDHCI_RESET_ALL))
		return;

	if (tegra_platform_is_silicon()) {
		err = tegra_prod_set_by_name(&host->ioaddr, "prod",
			tegra_host->prods);
		if (err)
			dev_err(mmc_dev(host->mmc),
				"Failed to set prod-reset settings %d\n", err);
	}

	tegra_sdhci_set_tap(host, tegra_host->default_tap);
	tegra_sdhci_set_dqs_trim(host, tegra_host->dqs_trim);

	misc_ctrl = sdhci_readl(host, SDHCI_TEGRA_VENDOR_MISC_CTRL);
	clk_ctrl = sdhci_readl(host, SDHCI_TEGRA_VENDOR_CLOCK_CTRL);

	misc_ctrl &= ~(SDHCI_MISC_CTRL_ENABLE_SDHCI_SPEC_300 |
		       SDHCI_MISC_CTRL_ENABLE_SDR50 |
		       SDHCI_MISC_CTRL_ENABLE_DDR50 |
		       SDHCI_MISC_CTRL_ENABLE_SDR104);

	clk_ctrl &= ~(SDHCI_CLOCK_CTRL_TRIM_MASK |
		      SDHCI_CLOCK_CTRL_SPI_MODE_CLKEN_OVERRIDE);

	if (tegra_sdhci_is_pad_and_regulator_valid(host)) {
		/* Erratum: Enable SDHCI spec v3.00 support */
		if (soc_data->nvquirks & NVQUIRK_ENABLE_SDHCI_SPEC_300)
			misc_ctrl |= SDHCI_MISC_CTRL_ENABLE_SDHCI_SPEC_300;
		/* Advertise UHS modes as supported by host */
		if (soc_data->nvquirks & NVQUIRK_ENABLE_SDR50)
			misc_ctrl |= SDHCI_MISC_CTRL_ENABLE_SDR50;
		if ((soc_data->nvquirks & NVQUIRK_ENABLE_DDR50) &&
			!(tegra_host->uhs_mask & MMC_UHS_MASK_DDR50))
			misc_ctrl |= SDHCI_MISC_CTRL_ENABLE_DDR50;
		if (soc_data->nvquirks & NVQUIRK_ENABLE_SDR104)
			misc_ctrl |= SDHCI_MISC_CTRL_ENABLE_SDR104;
		if (soc_data->nvquirks & NVQUIRK_ENABLE_SDR50)
			clk_ctrl |= SDHCI_CLOCK_CTRL_SDR50_TUNING_OVERRIDE;
	}

	clk_ctrl |= tegra_host->default_trim << SDHCI_CLOCK_CTRL_TRIM_SHIFT;

	if (soc_data->nvquirks & NVQUIRK_SDMMC_CLK_OVERRIDE) {
		misc_ctrl_2 = sdhci_readl(host, SDHCI_TEGRA_VENDOR_MISC_CTRL_2);
		tegra_host->slcg_status = !(misc_ctrl_2 &
						SDHCI_MISC_CTRL_2_CLK_OVR_ON);
	} else
		tegra_host->slcg_status = !(clk_ctrl &
					SDHCI_CLOCK_CTRL_LEGACY_CLKEN_OVERRIDE);
	sdhci_writel(host, misc_ctrl, SDHCI_TEGRA_VENDOR_MISC_CTRL);
	sdhci_writel(host, clk_ctrl, SDHCI_TEGRA_VENDOR_CLOCK_CTRL);

	if (soc_data->nvquirks & NVQUIRK_HAS_PADCALIB) {
		pad_ctrl = sdhci_readl(host, SDHCI_TEGRA_SDMEM_COMP_PADCTRL);
		pad_ctrl &= ~SDHCI_TEGRA_SDMEM_COMP_PADCTRL_VREF_SEL_MASK;
		pad_ctrl |= SDHCI_TEGRA_SDMEM_COMP_PADCTRL_VREF_SEL_VAL;
		sdhci_writel(host, pad_ctrl, SDHCI_TEGRA_SDMEM_COMP_PADCTRL);

		tegra_host->pad_calib_required = true;
	}

	/* ddr signalling post resume */
	if ((host->mmc->pm_flags & MMC_PM_KEEP_POWER) &&
		((host->mmc->ios.timing == MMC_TIMING_MMC_DDR52) ||
		(host->mmc->ios.timing == MMC_TIMING_UHS_DDR50)))
		clear_ddr_signalling = false;

	if (clear_ddr_signalling)
		tegra_host->ddr_signaling = false;
	tegra_sdhci_mask_host_caps(host, tegra_host->uhs_mask);
}

static void tegra_sdhci_configure_cal_pad(struct sdhci_host *host, bool enable)
{
	u32 val;

	/*
	 * Enable or disable the additional I/O pad used by the drive strength
	 * calibration process.
	 */
	val = sdhci_readl(host, SDHCI_TEGRA_SDMEM_COMP_PADCTRL);

	if (enable)
		val |= SDHCI_TEGRA_SDMEM_COMP_PADCTRL_E_INPUT_E_PWRD;
	else
		val &= ~SDHCI_TEGRA_SDMEM_COMP_PADCTRL_E_INPUT_E_PWRD;

	sdhci_writel(host, val, SDHCI_TEGRA_SDMEM_COMP_PADCTRL);

	if (enable)
		udelay(2);
}

static void tegra_sdhci_set_pad_autocal_offset(struct sdhci_host *host,
					       u16 pdpu)
{
	u32 reg;

	reg = sdhci_readl(host, SDHCI_TEGRA_AUTO_CAL_CONFIG);
	reg &= ~SDHCI_AUTO_CAL_PDPU_OFFSET_MASK;
	reg |= pdpu;
	sdhci_writel(host, reg, SDHCI_TEGRA_AUTO_CAL_CONFIG);
}

static int tegra_sdhci_set_padctrl(struct sdhci_host *host, int voltage,
				   bool state_drvupdn)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	struct sdhci_tegra_autocal_offsets *offsets =
						&tegra_host->autocal_offsets;
	struct pinctrl_state *pinctrl_drvupdn = NULL;
	int ret = 0;
	u8 drvup = 0, drvdn = 0;
	u32 reg;

	if (!state_drvupdn) {
		/* PADS Drive Strength */
		if (voltage == MMC_SIGNAL_VOLTAGE_180) {
			if (tegra_host->pinctrl_state_1v8_drv) {
				pinctrl_drvupdn =
					tegra_host->pinctrl_state_1v8_drv;
			} else {
				drvup = offsets->pull_up_1v8_timeout;
				drvdn = offsets->pull_down_1v8_timeout;
			}
		} else {
			if (tegra_host->pinctrl_state_3v3_drv) {
				pinctrl_drvupdn =
					tegra_host->pinctrl_state_3v3_drv;
			} else {
				drvup = offsets->pull_up_3v3_timeout;
				drvdn = offsets->pull_down_3v3_timeout;
			}
		}

		if (pinctrl_drvupdn != NULL) {
			ret = pinctrl_select_state(tegra_host->pinctrl_sdmmc,
							pinctrl_drvupdn);
			if (ret < 0)
				dev_err(mmc_dev(host->mmc),
					"failed pads drvupdn, ret: %d\n", ret);
		} else if ((drvup) || (drvdn)) {
			reg = sdhci_readl(host,
					SDHCI_TEGRA_SDMEM_COMP_PADCTRL);
			reg &= ~SDHCI_COMP_PADCTRL_DRVUPDN_OFFSET_MASK;
			reg |= (drvup << 20) | (drvdn << 12);
			sdhci_writel(host, reg,
					SDHCI_TEGRA_SDMEM_COMP_PADCTRL);
		}
	} else {
		/* Toggle power gpio for switching voltage on FPGA */
		if (gpio_is_valid(tegra_host->volt_switch_gpio)) {
			if (voltage == MMC_SIGNAL_VOLTAGE_330) {
				gpio_set_value(tegra_host->volt_switch_gpio, 1);
				dev_info(mmc_dev(host->mmc),
					 "3.3V set by voltage switch gpio\n");
			} else {
				gpio_set_value(tegra_host->volt_switch_gpio, 0);
				dev_info(mmc_dev(host->mmc),
					 "1.8V set by voltage switch gpio\n");
			}
			return 0;
		}
		/* Dual Voltage PADS Voltage selection */
		if (!tegra_host->pad_control_available)
			return 0;

		if (voltage == MMC_SIGNAL_VOLTAGE_180) {
			ret = pinctrl_select_state(tegra_host->pinctrl_sdmmc,
						tegra_host->pinctrl_state_1v8);
			if (ret < 0)
				dev_err(mmc_dev(host->mmc),
					"setting 1.8V failed, ret: %d\n", ret);
		} else {
			ret = pinctrl_select_state(tegra_host->pinctrl_sdmmc,
						tegra_host->pinctrl_state_3v3);
			if (ret < 0)
				dev_err(mmc_dev(host->mmc),
					"setting 3.3V failed, ret: %d\n", ret);
		}
	}

	return ret;
}

static void tegra_sdhci_card_event(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	int err = 0;

	if (!host->mmc->rem_card_present) {
		if (host->mmc->is_card_sd_express) {
			err = notifier_from_sd_call_chain(host, CARD_REMOVED);
			if (err != NOTIFY_OK) {
				pr_err("%s: SD express card removal failed\n",
				       mmc_hostname(host->mmc));
			}
			err = tegra_sdhci_pre_sd_exp_card_init(host, CARD_REMOVED, 0);
			if (err) {
				WARN_ON("Switch to default SD mode failed\r\n");
			} else {
				err = unregister_notifier_to_sd(host);
				if (!err)
					pr_info("%s: SD Express card removed successfully\n",
						mmc_hostname(host->mmc));
			}
		}
		if (tegra_host->sd_exp_support)
			host->mmc->caps2 |= MMC_CAP2_SD_EXPRESS_SUPPORT;
	}
}

static void tegra_sdhci_pad_autocalib(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	struct sdhci_tegra_autocal_offsets offsets =
			tegra_host->autocal_offsets;
	struct mmc_ios *ios = &host->mmc->ios;
	bool card_clk_enabled;
	u16 pdpu;
	u32 reg;
	int ret;

	if (tegra_platform_is_vsp() || tegra_host->defer_calib)
		return;

	switch (ios->timing) {
	case MMC_TIMING_UHS_SDR104:
		pdpu = offsets.pull_down_sdr104 << 8 | offsets.pull_up_sdr104;
		break;
	case MMC_TIMING_MMC_HS400:
		pdpu = offsets.pull_down_hs400 << 8 | offsets.pull_up_hs400;
		break;
	default:
		if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_180)
			pdpu = offsets.pull_down_1v8 << 8 | offsets.pull_up_1v8;
		else
			pdpu = offsets.pull_down_3v3 << 8 | offsets.pull_up_3v3;
	}

	/* Set initial offset before auto-calibration */
	tegra_sdhci_set_pad_autocal_offset(host, pdpu);

	card_clk_enabled = tegra_sdhci_configure_card_clk(host, false);

	tegra_sdhci_configure_cal_pad(host, true);

	reg = sdhci_readl(host, SDHCI_TEGRA_AUTO_CAL_CONFIG);
	reg |= SDHCI_AUTO_CAL_ENABLE | SDHCI_AUTO_CAL_START;
	sdhci_writel(host, reg, SDHCI_TEGRA_AUTO_CAL_CONFIG);

	udelay(2);
	/* 10 ms timeout */
	ret = readl_poll_timeout(host->ioaddr + SDHCI_TEGRA_AUTO_CAL_STATUS,
				 reg, !(reg & SDHCI_TEGRA_AUTO_CAL_ACTIVE),
				 1000, 10000);

	tegra_sdhci_configure_cal_pad(host, false);

	tegra_sdhci_configure_card_clk(host, card_clk_enabled);

	if (ret) {
		dev_err(mmc_dev(host->mmc), "Pad autocal timed out\n");

		/* Disable automatic cal and use fixed Drive Strengths */
		reg = sdhci_readl(host, SDHCI_TEGRA_AUTO_CAL_CONFIG);
		reg &= ~SDHCI_AUTO_CAL_ENABLE;
		sdhci_writel(host, reg, SDHCI_TEGRA_AUTO_CAL_CONFIG);

		ret = tegra_sdhci_set_padctrl(host, ios->signal_voltage, false);
		if (ret < 0)
			dev_err(mmc_dev(host->mmc),
				"Setting drive strengths failed: %d\n", ret);
	}
}

static void tegra_sdhci_parse_pad_autocal_dt(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	struct sdhci_tegra_autocal_offsets *autocal =
			&tegra_host->autocal_offsets;
	int err;

	err = device_property_read_u32(host->mmc->parent,
			"nvidia,pad-autocal-pull-up-offset-3v3",
			&autocal->pull_up_3v3);
	if (err)
		autocal->pull_up_3v3 = 0;

	err = device_property_read_u32(host->mmc->parent,
			"nvidia,pad-autocal-pull-down-offset-3v3",
			&autocal->pull_down_3v3);
	if (err)
		autocal->pull_down_3v3 = 0;

	err = device_property_read_u32(host->mmc->parent,
			"nvidia,pad-autocal-pull-up-offset-1v8",
			&autocal->pull_up_1v8);
	if (err)
		autocal->pull_up_1v8 = 0;

	err = device_property_read_u32(host->mmc->parent,
			"nvidia,pad-autocal-pull-down-offset-1v8",
			&autocal->pull_down_1v8);
	if (err)
		autocal->pull_down_1v8 = 0;

	err = device_property_read_u32(host->mmc->parent,
			"nvidia,pad-autocal-pull-up-offset-sdr104",
			&autocal->pull_up_sdr104);
	if (err)
		autocal->pull_up_sdr104 = autocal->pull_up_1v8;

	err = device_property_read_u32(host->mmc->parent,
			"nvidia,pad-autocal-pull-down-offset-sdr104",
			&autocal->pull_down_sdr104);
	if (err)
		autocal->pull_down_sdr104 = autocal->pull_down_1v8;

	err = device_property_read_u32(host->mmc->parent,
			"nvidia,pad-autocal-pull-up-offset-hs400",
			&autocal->pull_up_hs400);
	if (err)
		autocal->pull_up_hs400 = autocal->pull_up_1v8;

	err = device_property_read_u32(host->mmc->parent,
			"nvidia,pad-autocal-pull-down-offset-hs400",
			&autocal->pull_down_hs400);
	if (err)
		autocal->pull_down_hs400 = autocal->pull_down_1v8;

	/*
	 * Different fail-safe drive strength values based on the signaling
	 * voltage are applicable for SoCs supporting 3V3 and 1V8 pad controls.
	 * So, avoid reading below device tree properties for SoCs that don't
	 * have NVQUIRK_NEEDS_PAD_CONTROL.
	 */
	if (!(tegra_host->soc_data->nvquirks & NVQUIRK_NEEDS_PAD_CONTROL))
		return;

	err = device_property_read_u32(host->mmc->parent,
			"nvidia,pad-autocal-pull-up-offset-3v3-timeout",
			&autocal->pull_up_3v3_timeout);
	if (err) {
		if (!IS_ERR(tegra_host->pinctrl_state_3v3) &&
			(tegra_host->pinctrl_state_3v3_drv == NULL))
			pr_warn("%s: Missing autocal timeout 3v3-pad drvs\n",
				mmc_hostname(host->mmc));
		autocal->pull_up_3v3_timeout = 0;
	}

	err = device_property_read_u32(host->mmc->parent,
			"nvidia,pad-autocal-pull-down-offset-3v3-timeout",
			&autocal->pull_down_3v3_timeout);
	if (err) {
		if (!IS_ERR(tegra_host->pinctrl_state_3v3) &&
			(tegra_host->pinctrl_state_3v3_drv == NULL))
			pr_warn("%s: Missing autocal timeout 3v3-pad drvs\n",
				mmc_hostname(host->mmc));
		autocal->pull_down_3v3_timeout = 0;
	}

	err = device_property_read_u32(host->mmc->parent,
			"nvidia,pad-autocal-pull-up-offset-1v8-timeout",
			&autocal->pull_up_1v8_timeout);
	if (err) {
		if (!IS_ERR(tegra_host->pinctrl_state_1v8) &&
			(tegra_host->pinctrl_state_1v8_drv == NULL))
			pr_warn("%s: Missing autocal timeout 1v8-pad drvs\n",
				mmc_hostname(host->mmc));
		autocal->pull_up_1v8_timeout = 0;
	}

	err = device_property_read_u32(host->mmc->parent,
			"nvidia,pad-autocal-pull-down-offset-1v8-timeout",
			&autocal->pull_down_1v8_timeout);
	if (err) {
		if (!IS_ERR(tegra_host->pinctrl_state_1v8) &&
			(tegra_host->pinctrl_state_1v8_drv == NULL))
			pr_warn("%s: Missing autocal timeout 1v8-pad drvs\n",
				mmc_hostname(host->mmc));
		autocal->pull_down_1v8_timeout = 0;
	}
}

static void tegra_sdhci_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;
	ktime_t since_calib = ktime_sub(ktime_get(), tegra_host->last_calib);

	/* 100 ms calibration interval is specified in the TRM */
	if ((soc_data->nvquirks & NVQUIRK_ENABLE_PERIODIC_CALIB) &&
			(ktime_to_ms(since_calib) > 100)) {
		tegra_sdhci_pad_autocalib(host);
		tegra_host->last_calib = ktime_get();
	}

	sdhci_request(mmc, mrq);
}

static void tegra_sdhci_parse_tap_and_trim(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	int err;

	err = device_property_read_u32(host->mmc->parent, "nvidia,default-tap",
				       &tegra_host->default_tap);
	if (err)
		tegra_host->default_tap = 0;

	err = device_property_read_u32(host->mmc->parent, "nvidia,default-trim",
				       &tegra_host->default_trim);
	if (err)
		tegra_host->default_trim = 0;

	err = device_property_read_u32(host->mmc->parent, "nvidia,dqs-trim",
				       &tegra_host->dqs_trim);
	if (err)
		tegra_host->dqs_trim = 0x11;
}

static void tegra_sdhci_set_bg_trimmer_supply(struct sdhci_host *host,
					      bool enable)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;
	unsigned int misc_ctrl;

	if (!(soc_data->nvquirks & NVQUIRK_CONTROL_TRIMMER_SUPPLY))
		return;

	misc_ctrl = sdhci_readl(host, SDHCI_TEGRA_VENDOR_IO_TRIM_CTRL_0);
	if (enable) {
		misc_ctrl &= ~(SDHCI_TEGRA_IO_TRIM_CTRL_0_SEL_VREG_MASK);
		sdhci_writel(host, misc_ctrl, SDHCI_TEGRA_VENDOR_IO_TRIM_CTRL_0);
		udelay(3);
		sdhci_reset(host, SDHCI_RESET_CMD | SDHCI_RESET_DATA);
	} else {
		misc_ctrl |= (SDHCI_TEGRA_IO_TRIM_CTRL_0_SEL_VREG_MASK);
		sdhci_writel(host, misc_ctrl, SDHCI_TEGRA_VENDOR_IO_TRIM_CTRL_0);
		udelay(1);
	}
}

static void tegra_sdhci_parse_dt(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	int val = 0;

	if (device_property_read_bool(host->mmc->parent, "supports-cqe"))
		tegra_host->enable_hwcq = true;
	else
		tegra_host->enable_hwcq = false;

	if (tegra_host->enable_hwcq) {
		if (device_property_read_bool(host->mmc->parent,
						"nvidia,enable-cqic"))
			tegra_host->enable_cqic = true;
		else
			tegra_host->enable_cqic = false;
	}

	if (device_property_read_bool(host->mmc->parent, "nvidia,disable-rtpm"))
		tegra_host->disable_rtpm = true;
	else
		tegra_host->disable_rtpm = false;


	tegra_sdhci_parse_pad_autocal_dt(host);
	tegra_sdhci_parse_tap_and_trim(host);

	device_property_read_u32(host->mmc->parent, "uhs-mask",
				(u32 *)&tegra_host->uhs_mask);

	tegra_host->force_non_rem_rescan =
		device_property_read_bool(host->mmc->parent,
		"force-non-removable-rescan");
	tegra_host->cd_wakeup_capable = 
			device_property_read_bool(host->mmc->parent,
					"nvidia,cd-wakeup-capable");
	host->mmc->cd_cap_invert = device_property_read_bool(host->mmc->parent,
								"cd-inverted");

	tegra_host->en_periodic_cflush = device_property_read_bool(host->mmc->parent,
				"nvidia,en-periodic-cflush");
	if (tegra_host->en_periodic_cflush) {
		device_property_read_u32(host->mmc->parent,
				"nvidia,periodic-cflush-to", &val);
		host->mmc->flush_timeout = val;
		if (val == 0) {
			tegra_host->en_periodic_cflush = false;
		}
	}
	device_property_read_u32(host->mmc->parent, "nvidia,boot-detect-delay",
					&tegra_host->boot_detect_delay);
	device_property_read_u32(host->mmc->parent, "max-clk-limit",
		(u32 *)&tegra_host->max_clk_limit);
	device_property_read_u32(host->mmc->parent, "ddr-clk-limit",
		(u32 *)&tegra_host->max_ddr_clk_limit);

	tegra_host->skip_clk_rst = device_property_read_bool(host->mmc->parent,
							"nvidia,skip-clk-rst");
}

static unsigned long tegra_sdhci_apply_clk_limits(struct sdhci_host *host,
	unsigned int clock)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	unsigned long host_clk;

	if (tegra_host->ddr_signaling)
		host_clk = (tegra_host->max_ddr_clk_limit) ?
			tegra_host->max_ddr_clk_limit * 2 : clock * 2;
	else
		host_clk = (clock > tegra_host->max_clk_limit) ?
			tegra_host->max_clk_limit : clock;

	dev_dbg(mmc_dev(host->mmc), "Setting clk limit %lu\n", host_clk);
	return host_clk;
}

static void tegra_sdhci_set_clock(struct sdhci_host *host, unsigned int clock)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	unsigned long host_clk;

	if (host->mmc->skip_host_clkgate) {
		sdhci_set_card_clock(host, clock ? true : false);
		return;
	}

	if (!clock)
		return sdhci_set_clock(host, clock);

	/*
	 * In DDR50/52 modes the Tegra SDHCI controllers require the SDHCI
	 * divider to be configured to divided the host clock by two. The SDHCI
	 * clock divider is calculated as part of sdhci_set_clock() by
	 * sdhci_calc_clk(). The divider is calculated from host->max_clk and
	 * the requested clock rate.
	 *
	 * By setting the host->max_clk to clock * 2 the divider calculation
	 * will always result in the correct value for DDR50/52 modes,
	 * regardless of clock rate rounding, which may happen if the value
	 * from clk_get_rate() is used.
	 */
	if (!tegra_host->skip_clk_rst) {
		host_clk = tegra_sdhci_apply_clk_limits(host, clock);
		if (host_clk < tegra_host->soc_data->min_host_clk)
			host_clk = tegra_host->soc_data->min_host_clk;
		clk_set_rate(pltfm_host->clk, host_clk);
		tegra_host->curr_clk_rate = clk_get_rate(pltfm_host->clk);
		if (tegra_host->ddr_signaling)
			host->max_clk = host_clk;
		else
			host->max_clk = clk_get_rate(pltfm_host->clk);
	}
	sdhci_set_clock(host, clock);

	if (tegra_host->pad_calib_required) {
		tegra_sdhci_pad_autocalib(host);
		tegra_host->pad_calib_required = false;
	}
}

static void tegra_sdhci_hs400_enhanced_strobe(struct sdhci_host *host,
					      bool enable)
{
	u32 val;

	val = sdhci_readl(host, SDHCI_TEGRA_VENDOR_SYS_SW_CTRL);

	if (enable) {
		val |= SDHCI_TEGRA_SYS_SW_CTRL_ENHANCED_STROBE;
		/*
		 * When CMD13 is sent from mmc_select_hs400es() after
		 * switching to HS400ES mode, the bus is operating at
		 * either MMC_HIGH_26_MAX_DTR or MMC_HIGH_52_MAX_DTR.
		 * To meet Tegra SDHCI requirement at HS400ES mode, force SDHCI
		 * interface clock to MMC_HS200_MAX_DTR (200 MHz) so that host
		 * controller CAR clock and the interface clock are rate matched.
		 */
		tegra_sdhci_set_clock(host, MMC_HS200_MAX_DTR);
	} else {
		val &= ~SDHCI_TEGRA_SYS_SW_CTRL_ENHANCED_STROBE;
	}

	sdhci_writel(host, val, SDHCI_TEGRA_VENDOR_SYS_SW_CTRL);

}

static int tegra_sdhci_set_host_clock(struct sdhci_host *host, bool enable)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	u8 vndr_ctrl;
	int err;

	if (tegra_host->skip_clk_rst)
		return 0;

	if (!enable) {
		dev_dbg(mmc_dev(host->mmc), "Disabling clk\n");

		/*
		 * Power down BG trimmer supply(VREG).
		 * Ensure SDMMC host internal clocks are
		 * turned off before calling this function.
		 */
		tegra_sdhci_set_bg_trimmer_supply(host, false);

		/* Update SDMMC host CAR clock status */
		vndr_ctrl = sdhci_readb(host, SDHCI_TEGRA_VENDOR_CLOCK_CTRL);
		vndr_ctrl &= ~SDHCI_CLOCK_CTRL_SDMMC_CLK;
		sdhci_writeb(host, vndr_ctrl, SDHCI_TEGRA_VENDOR_CLOCK_CTRL);

		/* Disable SDMMC host CAR clock */
		clk_disable_unprepare(pltfm_host->clk);
	} else {
		dev_dbg(mmc_dev(host->mmc), "Enabling clk\n");

		/* Enable SDMMC host CAR clock */
		err = clk_prepare_enable(pltfm_host->clk);
		if (err) {
			dev_err(mmc_dev(host->mmc),
				"clk enable failed %d\n", err);
			return err;
		}

		/* Reset SDMMC host CAR clock status */
		vndr_ctrl = sdhci_readb(host, SDHCI_TEGRA_VENDOR_CLOCK_CTRL);
		vndr_ctrl |= SDHCI_CLOCK_CTRL_SDMMC_CLK;
		sdhci_writeb(host, vndr_ctrl, SDHCI_TEGRA_VENDOR_CLOCK_CTRL);

		/*
		 * Power up BG trimmer supply(VREG).
		 * Ensure SDMMC host internal clocks are
		 * turned off before calling this function.
		 */
		tegra_sdhci_set_bg_trimmer_supply(host, true);
	}

	return 0;
}

static unsigned int tegra_sdhci_get_max_clock(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);

	return clk_round_rate(pltfm_host->clk, UINT_MAX);
}

static void tegra_sdhci_set_dqs_trim(struct sdhci_host *host, u8 trim)
{
	u32 val;

	val = sdhci_readl(host, SDHCI_TEGRA_VENDOR_CAP_OVERRIDES);
	val &= ~SDHCI_TEGRA_CAP_OVERRIDES_DQS_TRIM_MASK;
	val |= trim << SDHCI_TEGRA_CAP_OVERRIDES_DQS_TRIM_SHIFT;
	sdhci_writel(host, val, SDHCI_TEGRA_VENDOR_CAP_OVERRIDES);
}

static void tegra_sdhci_hs400_dll_cal(struct sdhci_host *host)
{
	u32 reg;
	int timeout=5;

	reg = sdhci_readl(host, SDHCI_TEGRA_VENDOR_DLLCAL_CFG);
	reg |= SDHCI_TEGRA_DLLCAL_CALIBRATE;
	sdhci_writel(host, reg, SDHCI_TEGRA_VENDOR_DLLCAL_CFG);

	mdelay(1);

	/*
	 * Wait for calibrate_en bit to clear before checking
	 * calibration status
	 */
	while (sdhci_readl(host, SDHCI_TEGRA_VENDOR_DLLCAL_CFG) &
			SDHCI_TEGRA_DLLCAL_CALIBRATE)
		;

	/* Wait until DLL calibration is done */
	/* 1 ms sleep, 5 ms timeout */
	do {
		if (!(sdhci_readl(host, SDHCI_TEGRA_VENDOR_DLLCAL_STA) &
			SDHCI_TEGRA_DLLCAL_STA_ACTIVE))
			break;
		mdelay(1);
		timeout--;
	} while (timeout);

	if (!timeout)
		dev_err(mmc_dev(host->mmc),
			"HS400 delay line calibration timed out\n");
}

static void tegra_sdhci_dll_calib(struct sdhci_host *host)
{
	struct mmc_host *mmc = host->mmc;

	if ((mmc->ios.timing == MMC_TIMING_MMC_DDR52) ||
		(mmc->ios.timing == MMC_TIMING_UHS_DDR50)) {
		/*
		 * Tegra SDMMC controllers support DDR mode with only clock
		 * divisor 1. Set the clock frequency here again to ensure
		 * host and device clocks are correctly configured.
		 */
		tegra_sdhci_set_clock(host, host->max_clk);
	} else if (mmc->ios.timing == MMC_TIMING_MMC_HS400) {
		tegra_sdhci_hs400_dll_cal(host);
	}
}


static int tegra_sdhci_execute_hw_tuning(struct mmc_host *mmc, u32 opcode)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;
	int err;
	u32 val;

	if (tegra_sdhci_skip_retuning(host))
		return 0;

	if (soc_data->nvquirks & NVQUIRK_ENABLE_TUNING_DQ_OFFSET) {
		/* Configure DQ_OFFSET=1 before tuning */
		val = sdhci_readl(host, SDHCI_TEGRA_VNDR_TUN_CTRL1_0);
		val &= ~SDHCI_TEGRA_VNDR_TUN_CTRL1_DQ_OFF_MASK;
		val |= (1U << SDHCI_TEGRA_VNDR_TUN_CTRL1_DQ_OFF_SHIFT);
		sdhci_writel(host, val, SDHCI_TEGRA_VNDR_TUN_CTRL1_0);
	}
	err = sdhci_execute_tuning(mmc, opcode);

	if (soc_data->nvquirks & NVQUIRK_ENABLE_TUNING_DQ_OFFSET) {
		/* Reset DQ_OFFSET=0 after tuning */
		val = sdhci_readl(host, SDHCI_TEGRA_VNDR_TUN_CTRL1_0);
		val &= ~SDHCI_TEGRA_VNDR_TUN_CTRL1_DQ_OFF_MASK;
		sdhci_writel(host, val, SDHCI_TEGRA_VNDR_TUN_CTRL1_0);
	}

	if (!err && !host->tuning_err)
		tegra_sdhci_post_tuning(host);

	return err;
}

static void tegra_sdhci_set_uhs_signaling(struct sdhci_host *host,
					  unsigned timing)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	int ret;
	bool tuning_mode = false;
	bool set_num_tun_iter = false;
	bool set_trim_delay = false;
	bool set_padpipe_clk_override = false;
	bool set_sdmmc_spare_0 = false;
	bool do_hs400_dll_cal = false;

	tegra_host->ddr_signaling = false;

	sdhci_set_uhs_signaling(host, timing);

	switch (timing) {
	case MMC_TIMING_UHS_SDR50:
	case MMC_TIMING_UHS_SDR104:
		tuning_mode = true;
		break;
	case MMC_TIMING_MMC_DDR52:
		set_sdmmc_spare_0 = true;
		set_trim_delay = true;
		tegra_host->ddr_signaling = true;
		break;
	case MMC_TIMING_UHS_DDR50:
		tegra_host->ddr_signaling = true;
		set_trim_delay = true;
		break;
	case MMC_TIMING_MMC_HS200:
		set_trim_delay = true;
		tuning_mode = true;
		set_num_tun_iter = true;
		set_padpipe_clk_override = true;
		break;
	case MMC_TIMING_MMC_HS400:
		tuning_mode = true;
		set_num_tun_iter = true;
		set_padpipe_clk_override = true;
		do_hs400_dll_cal = true;
		break;
	default:
		break;
	}

	/* Set Tap delay */
	if ((tegra_host->tuning_status == TUNING_STATUS_DONE) &&
		tuning_mode)
		tegra_sdhci_set_tap(host, tegra_host->tuned_tap_delay);
	else
		tegra_sdhci_set_tap(host, tegra_host->default_tap);

	if (!tegra_platform_is_silicon() && do_hs400_dll_cal)
		return tegra_sdhci_dll_calib(host);

	/* Set trim delay */
	if (set_trim_delay) {
		ret = tegra_prod_set_by_name_partially(&host->ioaddr,
				prod_device_states[timing], tegra_host->prods,
				0, SDHCI_TEGRA_VENDOR_CLOCK_CTRL,
				SDHCI_CLOCK_CTRL_TRIM_MASK);
		if (ret < 0)
			dev_err(mmc_dev(host->mmc),
				"Failed to set trim value for timing %d, %d\n",
				timing, ret);
	}

	/*set padpipe_clk_override*/
	if (set_padpipe_clk_override) {
		ret = tegra_prod_set_by_name_partially(&host->ioaddr,
				prod_device_states[timing], tegra_host->prods,
				0, SDHCI_TEGRA_VENDOR_CLOCK_CTRL,
				SDHCI_CLOCK_CTRL_PADPIPE_CLKEN_OVERRIDE);
		if (ret < 0)
			dev_err(mmc_dev(host->mmc),
				"Failed to set padpipe clk override value for timing %d, %d\n",
				timing, ret);
	}
	/* Set number of tuning iterations */
	if (set_num_tun_iter) {
		ret = tegra_prod_set_by_name_partially(&host->ioaddr,
				prod_device_states[timing], tegra_host->prods,
				0, SDHCI_VNDR_TUN_CTRL0_0,
				SDHCI_VNDR_TUN_CTRL0_TUN_ITER_MASK);
		if (ret < 0)
			dev_err(mmc_dev(host->mmc),
				"Failed to set number of iterations for timing %d, %d\n",
				timing, ret);
	}
	/* Set SDMMC_SPARE_0*/
	if (set_sdmmc_spare_0) {
		ret = tegra_prod_set_by_name_partially(&host->ioaddr,
				prod_device_states[timing], tegra_host->prods,
				0, SDHCI_TEGRA_VENDOR_MISC_CTRL,
				 SDHCI_MISC_CTRL_SDMMC_SPARE_0_MASK);
		if (ret < 0)
			dev_err(mmc_dev(host->mmc),
				"Failed to set spare0 field for timing %d, %d\n",
				timing, ret);
	}

	if (do_hs400_dll_cal)
		tegra_sdhci_dll_calib(host);
}

static unsigned int tegra_sdhci_get_sw_timeout_value(struct sdhci_host *host)
{
	/*
	 * With SDMMC timeout clock set to 12MHZ, host controller waits for
	 * 11.18 seconds before triggering data timeout error interrupt.
	 * Increase SW timer timeout value to 12 seconds to avoid SW timer
	 * getting triggered before data timeout error interrupt.
	 */
	return 12 * HZ;
}

static unsigned int tegra_sdhci_get_timeout_clock(struct sdhci_host *host)
{
	/*
	* Tegra SDMMC controller advertises 12MHz timeout clock. Controller
	* models in simulator might not advertise the timeout clock frequency.
	* To avoid errors, return 12MHz clock for supporting timeout clock
	* on simulators.
	*/
	return SDMMC_TIMEOUT_CLK_FREQ_MHZ * 1000;
}

static int tegra_sdhci_execute_tuning(struct sdhci_host *host, u32 opcode)
{
	unsigned int min, max;

	/*
	 * Start search for minimum tap value at 10, as smaller values are
	 * may wrongly be reported as working but fail at higher speeds,
	 * according to the TRM.
	 */
	min = 10;
	while (min < 255) {
		tegra_sdhci_set_tap(host, min);
		if (!mmc_send_tuning(host->mmc, opcode, NULL))
			break;
		min++;
	}

	/* Find the maximum tap value that still passes. */
	max = min + 1;
	while (max < 255) {
		tegra_sdhci_set_tap(host, max);
		if (mmc_send_tuning(host->mmc, opcode, NULL)) {
			max--;
			break;
		}
		max++;
	}

	/* The TRM states the ideal tap value is at 75% in the passing range. */
	tegra_sdhci_set_tap(host, min + ((max - min) * 3 / 4));

	return mmc_send_tuning(host->mmc, opcode, NULL);
}

static int tegra_sdhci_get_max_tuning_loop_counter(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	int err = 0;

	if (!tegra_platform_is_silicon())
		return 257;

	err = tegra_prod_set_by_name_partially(&host->ioaddr,
			prod_device_states[host->mmc->ios.timing],
			tegra_host->prods, 0, SDHCI_VNDR_TUN_CTRL0_0,
			SDHCI_VNDR_TUN_CTRL0_TUN_ITER_MASK);
	if (err)
		dev_err(mmc_dev(host->mmc),
			"%s: error %d in tuning iteration update\n",
				__func__, err);

	return 257;
}

static int sdhci_tegra_start_signal_voltage_switch(struct mmc_host *mmc,
						   struct mmc_ios *ios)
{
	struct sdhci_host *host = mmc_priv(mmc);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	int ret = 0;

	if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_330) {
		ret = tegra_sdhci_set_padctrl(host, ios->signal_voltage, true);
		if (ret < 0)
			return ret;
		tegra_sdhci_update_sdmmc_pinctrl_register(host, false);
		ret = sdhci_start_signal_voltage_switch(mmc, ios);
	} else if (ios->signal_voltage == MMC_SIGNAL_VOLTAGE_180) {
		ret = sdhci_start_signal_voltage_switch(mmc, ios);
		if (ret < 0)
			return ret;
		ret = tegra_sdhci_set_padctrl(host, ios->signal_voltage, true);
		tegra_sdhci_update_sdmmc_pinctrl_register(host, true);
	}

	if (tegra_host->pad_calib_required)
		tegra_sdhci_pad_autocalib(host);

	return ret;
}

static bool tegra_sdhci_skip_retuning(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);

	if (tegra_host->tuning_status == TUNING_STATUS_DONE) {
		dev_dbg(mmc_dev(host->mmc),
			"Tuning done, restoring the best tap value : %u\n",
				tegra_host->tuned_tap_delay);
		tegra_sdhci_set_tap(host, tegra_host->tuned_tap_delay);
		return true;
	}

	return false;
}

static void tegra_sdhci_init_sdexp_pinctrl_info(struct sdhci_tegra *tegra_host)
{
	if (IS_ERR_OR_NULL(tegra_host->pinctrl_sdmmc)) {
		pr_debug("No pinctrl info for SD express selection\n");
			return;
	}

	tegra_host->pinctrl_state_sdexp_disable = pinctrl_lookup_state(
				tegra_host->pinctrl_sdmmc, "sdmmc-sdexp-disable");
	if (IS_ERR(tegra_host->pinctrl_state_sdexp_disable)) {
		if (PTR_ERR(tegra_host->pinctrl_state_sdexp_disable) == -ENODEV)
			tegra_host->pinctrl_state_sdexp_disable = NULL;
	}

	tegra_host->pinctrl_state_sdexp_enable = pinctrl_lookup_state(
				tegra_host->pinctrl_sdmmc, "sdmmc-sdexp-enable");
	if (IS_ERR(tegra_host->pinctrl_state_sdexp_enable)) {
		if (PTR_ERR(tegra_host->pinctrl_state_sdexp_enable) == -ENODEV)
			tegra_host->pinctrl_state_sdexp_enable = NULL;
	}
}
static int tegra_sdhci_init_pinctrl_info(struct device *dev,
					 struct sdhci_tegra *tegra_host)
{
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;
	int i, ret;

	if (!tegra_platform_is_silicon())
		return 0;

	tegra_host->prods = devm_tegra_prod_get(dev);
	if (IS_ERR_OR_NULL(tegra_host->prods)) {
		dev_err(dev, "Prod-setting not available\n");
		tegra_host->prods = NULL;
	}

	tegra_host->pinctrl_sdmmc = devm_pinctrl_get(dev);
	if (IS_ERR(tegra_host->pinctrl_sdmmc)) {
		dev_dbg(dev, "No pinctrl info, err: %ld\n",
			PTR_ERR(tegra_host->pinctrl_sdmmc));
		return -1;
	}

	tegra_host->pinctrl_state_1v8_drv = pinctrl_lookup_state(
				tegra_host->pinctrl_sdmmc, "sdmmc-1v8-drv");
	if (IS_ERR(tegra_host->pinctrl_state_1v8_drv)) {
		if (PTR_ERR(tegra_host->pinctrl_state_1v8_drv) == -ENODEV)
			tegra_host->pinctrl_state_1v8_drv = NULL;
	}

	tegra_host->pinctrl_state_3v3_drv = pinctrl_lookup_state(
				tegra_host->pinctrl_sdmmc, "sdmmc-3v3-drv");
	if (IS_ERR(tegra_host->pinctrl_state_3v3_drv)) {
		if (PTR_ERR(tegra_host->pinctrl_state_3v3_drv) == -ENODEV)
			tegra_host->pinctrl_state_3v3_drv = NULL;
	}

	tegra_host->pinctrl_state_3v3 =
		pinctrl_lookup_state(tegra_host->pinctrl_sdmmc, "sdmmc-3v3");
	if (IS_ERR(tegra_host->pinctrl_state_3v3)) {
		dev_warn(dev, "Missing 3.3V pad state, err: %ld\n",
			 PTR_ERR(tegra_host->pinctrl_state_3v3));
		return -1;
	}

	tegra_host->pinctrl_state_1v8 =
		pinctrl_lookup_state(tegra_host->pinctrl_sdmmc, "sdmmc-1v8");
	if (IS_ERR(tegra_host->pinctrl_state_1v8)) {
		dev_warn(dev, "Missing 1.8V pad state, err: %ld\n",
			 PTR_ERR(tegra_host->pinctrl_state_1v8));
		return -1;
	}

	tegra_host->pad_control_available = true;

	if (soc_data->nvquirks & NVQUIRK_UPDATE_PIN_CNTRL_REG) {
		tegra_host->schmitt_enable[0] =
			pinctrl_lookup_state(tegra_host->pinctrl_sdmmc,
			"sdmmc_schmitt_enable");
		if (IS_ERR_OR_NULL(tegra_host->schmitt_enable[0]))
			dev_err(dev, "Missing schmitt enable state\n");

		tegra_host->schmitt_enable[1] =
			pinctrl_lookup_state(tegra_host->pinctrl_sdmmc,
			"sdmmc_clk_schmitt_enable");
		if (IS_ERR_OR_NULL(tegra_host->schmitt_enable[1]))
			dev_err(dev, "Missing clk schmitt enable state\n");

		tegra_host->schmitt_disable[0] =
			pinctrl_lookup_state(tegra_host->pinctrl_sdmmc,
			"sdmmc_schmitt_disable");
		if (IS_ERR_OR_NULL(tegra_host->schmitt_disable[0]))
			dev_err(dev, "Missing schmitt disable state\n");

		tegra_host->schmitt_disable[1] =
			pinctrl_lookup_state(tegra_host->pinctrl_sdmmc,
			"sdmmc_clk_schmitt_disable");
		if (IS_ERR_OR_NULL(tegra_host->schmitt_disable[1]))
			dev_err(dev, "Missing clk schmitt disable state\n");

		for (i = 0; i < 2; i++) {
			if (!IS_ERR_OR_NULL(tegra_host->schmitt_disable[i])) {
				ret = pinctrl_select_state(tegra_host->pinctrl_sdmmc,
					tegra_host->schmitt_disable[i]);
				if (ret < 0)
					dev_warn(dev, "setting schmitt state failed\n");
			}
		}
	}

	return 0;
}

static void tegra_sdhci_update_sdmmc_pinctrl_register(struct sdhci_host *sdhci,
       bool set)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(sdhci);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;
	struct pinctrl_state *set_schmitt[2];
	int ret;
	int i;

	if (!(soc_data->nvquirks & NVQUIRK_UPDATE_PIN_CNTRL_REG))
		return;

	if (set) {
		set_schmitt[0] = tegra_host->schmitt_enable[0];
		set_schmitt[1] = tegra_host->schmitt_enable[1];
	} else {
		set_schmitt[0] = tegra_host->schmitt_disable[0];
		set_schmitt[1] = tegra_host->schmitt_disable[1];
	}

	for (i = 0; i < 2; i++) {
		if (IS_ERR_OR_NULL(set_schmitt[i]))
			continue;
		ret = pinctrl_select_state(tegra_host->pinctrl_sdmmc,
			set_schmitt[i]);
		if (ret < 0)
			dev_warn(mmc_dev(sdhci->mmc),
				"setting schmitt state failed\n");
	}
}


static void tegra_sdhci_voltage_switch(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;

	if (soc_data->nvquirks & NVQUIRK_HAS_PADCALIB)
		tegra_host->pad_calib_required = true;
}

static void tegra_cqhci_writel(struct cqhci_host *cq_host, u32 val, int reg)
{
	struct mmc_host *mmc = cq_host->mmc;
	struct sdhci_host *host = mmc_priv(mmc);
	u8 ctrl;
	ktime_t timeout;
	bool timed_out;

	/*
	 * During CQE resume/unhalt, CQHCI driver unhalts CQE prior to
	 * cqhci_host_ops enable where SDHCI DMA and BLOCK_SIZE registers need
	 * to be re-configured.
	 * Tegra CQHCI/SDHCI prevents write access to block size register when
	 * CQE is unhalted. So handling CQE resume sequence here to configure
	 * SDHCI block registers prior to exiting CQE halt state.
	 */
	if (reg == CQHCI_CTL && !(val & CQHCI_HALT) &&
	    cqhci_readl(cq_host, CQHCI_CTL) & CQHCI_HALT) {
		sdhci_writew(host, SDHCI_TEGRA_CQE_TRNS_MODE, SDHCI_TRANSFER_MODE);
		sdhci_cqe_enable(mmc);
		writel(val, cq_host->mmio + reg);
		timeout = ktime_add_us(ktime_get(), 50);
		while (1) {
			timed_out = ktime_compare(ktime_get(), timeout) > 0;
			ctrl = cqhci_readl(cq_host, CQHCI_CTL);
			if (!(ctrl & CQHCI_HALT) || timed_out)
				break;
		}
		/*
		 * CQE usually resumes very quick, but incase if Tegra CQE
		 * doesn't resume retry unhalt.
		 */
		if (timed_out)
			writel(val, cq_host->mmio + reg);
	} else {
		writel(val, cq_host->mmio + reg);
	}
}

static void sdhci_tegra_update_dcmd_desc(struct mmc_host *mmc,
					 struct mmc_request *mrq, u64 *data)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(mmc_priv(mmc));
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;

	if (soc_data->nvquirks & NVQUIRK_CQHCI_DCMD_R1B_CMD_TIMING &&
	    mrq->cmd->flags & MMC_RSP_R1B)
		*data |= CQHCI_CMD_TIMING(1);
}

static void sdhci_tegra_cqe_enable(struct mmc_host *mmc)
{
	struct cqhci_host *cq_host = mmc->cqe_private;
	struct sdhci_host *host = mmc_priv(mmc);
	u32 val;

	/*
	 * Tegra CQHCI/SDMMC design prevents write access to sdhci block size
	 * register when CQE is enabled and unhalted.
	 * CQHCI driver enables CQE prior to activation, so disable CQE before
	 * programming block size in sdhci controller and enable it back.
	 */
	if (!cq_host->activated) {
		val = cqhci_readl(cq_host, CQHCI_CFG);
		if (val & CQHCI_ENABLE)
			cqhci_writel(cq_host, (val & ~CQHCI_ENABLE),
				     CQHCI_CFG);
		sdhci_writew(host, SDHCI_TEGRA_CQE_TRNS_MODE, SDHCI_TRANSFER_MODE);
		sdhci_cqe_enable(mmc);
		if (val & CQHCI_ENABLE)
			cqhci_writel(cq_host, val, CQHCI_CFG);
	}

	/*
	 * CMD CRC errors are seen sometimes with some eMMC devices when status
	 * command is sent during transfer of last data block which is the
	 * default case as send status command block counter (CBC) is 1.
	 * Recommended fix to set CBC to 0 allowing send status command only
	 * when data lines are idle.
	 */
	val = cqhci_readl(cq_host, CQHCI_SSC1);
	val &= ~CQHCI_SSC1_CBC_MASK;
	cqhci_writel(cq_host, val, CQHCI_SSC1);
}

static void sdhci_tegra_cqe_pre_enable(struct mmc_host *mmc)
{
	struct cqhci_host *cq_host = mmc->cqe_private;
	u32 reg;

	reg = cqhci_readl(cq_host, CQHCI_CFG);
	reg |= CQHCI_ENABLE;
	cqhci_writel(cq_host, reg, CQHCI_CFG);
}

static void sdhci_tegra_cqe_post_disable(struct mmc_host *mmc)
{
	struct cqhci_host *cq_host = mmc->cqe_private;
	struct sdhci_host *host = mmc_priv(mmc);
	u32 reg;

	reg = cqhci_readl(cq_host, CQHCI_CFG);
	reg &= ~CQHCI_ENABLE;
	cqhci_writel(cq_host, reg, CQHCI_CFG);
	sdhci_writew(host, 0x0, SDHCI_TRANSFER_MODE);
}

static void sdhci_tegra_dumpregs(struct mmc_host *mmc)
{
	sdhci_dumpregs(mmc_priv(mmc));
}

static u32 sdhci_tegra_cqhci_irq(struct sdhci_host *host, u32 intmask)
{
	int cmd_error = 0;
	int data_error = 0;

	if (!sdhci_cqe_irq(host, intmask, &cmd_error, &data_error))
		return intmask;

	cqhci_irq(host->mmc, intmask, cmd_error, data_error);

	return 0;
}

/* Configure voltage switch specific requirements */
static void tegra_sdhci_voltage_switch_req(struct sdhci_host *host, bool req)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	const struct sdhci_tegra_soc_data *soc_data = tegra_host->soc_data;
	u32 clk_ctrl;

	if (!req) {
		/* Disable SLCG */
		clk_ctrl = sdhci_readl(host, SDHCI_TEGRA_VENDOR_CLOCK_CTRL);
		clk_ctrl = clk_ctrl | SDHCI_CLOCK_CTRL_LEGACY_CLKEN_OVERRIDE;
		sdhci_writel(host, clk_ctrl, SDHCI_TEGRA_VENDOR_CLOCK_CTRL);

		if (soc_data->nvquirks & NVQUIRK_SDMMC_CLK_OVERRIDE) {
			clk_ctrl = sdhci_readl(host,
						SDHCI_TEGRA_VENDOR_MISC_CTRL_2);
			clk_ctrl = clk_ctrl | SDHCI_MISC_CTRL_2_CLK_OVR_ON;
			sdhci_writel(host, clk_ctrl,
						SDHCI_TEGRA_VENDOR_MISC_CTRL_2);
		}
	} else  {
		/* Restore SLCG */
		if (tegra_host->slcg_status) {
			clk_ctrl = sdhci_readl(host,
						SDHCI_TEGRA_VENDOR_CLOCK_CTRL);
			clk_ctrl = clk_ctrl &
					~SDHCI_CLOCK_CTRL_LEGACY_CLKEN_OVERRIDE;
			sdhci_writel(host, clk_ctrl,
						SDHCI_TEGRA_VENDOR_CLOCK_CTRL);
			if (soc_data->nvquirks &
					NVQUIRK_SDMMC_CLK_OVERRIDE) {
				clk_ctrl = sdhci_readl(host,
						SDHCI_TEGRA_VENDOR_MISC_CTRL_2);
				clk_ctrl = clk_ctrl &
						~SDHCI_MISC_CTRL_2_CLK_OVR_ON;
				sdhci_writel(host, clk_ctrl,
						SDHCI_TEGRA_VENDOR_MISC_CTRL_2);
			}
		}
	}

}

static void tegra_sdhci_set_timeout(struct sdhci_host *host,
				    struct mmc_command *cmd)
{
	u32 val;

	/*
	 * HW busy detection timeout is based on programmed data timeout
	 * counter and maximum supported timeout is 11s which may not be
	 * enough for long operations like cache flush, sleep awake, erase.
	 *
	 * ERASE_TIMEOUT_LIMIT bit of VENDOR_MISC_CTRL register allows
	 * host controller to wait for busy state until the card is busy
	 * without HW timeout.
	 *
	 * So, use infinite busy wait mode for operations that may take
	 * more than maximum HW busy timeout of 11s otherwise use finite
	 * busy wait mode.
	 */
	val = sdhci_readl(host, SDHCI_TEGRA_VENDOR_MISC_CTRL);
	if (cmd && cmd->busy_timeout >= 11 * MSEC_PER_SEC)
		val |= SDHCI_MISC_CTRL_ERASE_TIMEOUT_LIMIT;
	else
		val &= ~SDHCI_MISC_CTRL_ERASE_TIMEOUT_LIMIT;
	sdhci_writel(host, val, SDHCI_TEGRA_VENDOR_MISC_CTRL);

	__sdhci_set_timeout(host, cmd);
}

static const struct cqhci_host_ops sdhci_tegra_cqhci_ops = {
	.write_l    = tegra_cqhci_writel,
	.enable	= sdhci_tegra_cqe_enable,
	.disable = sdhci_cqe_disable,
	.dumpregs = sdhci_tegra_dumpregs,
	.update_dcmd_desc = sdhci_tegra_update_dcmd_desc,
	.pre_enable = sdhci_tegra_cqe_pre_enable,
	.post_disable = sdhci_tegra_cqe_post_disable,
};

static int tegra_sdhci_set_dma_mask(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *platform = sdhci_priv(host);
	struct sdhci_tegra *tegra = sdhci_pltfm_priv(platform);
	const struct sdhci_tegra_soc_data *soc = tegra->soc_data;
	struct device *dev = mmc_dev(host->mmc);

	if (host->quirks2 & SDHCI_QUIRK2_BROKEN_64_BIT_DMA) {
		host->flags &= ~SDHCI_USE_64_BIT_DMA;
		return dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
	}

	if (soc->dma_mask)
		return dma_set_mask_and_coherent(dev, soc->dma_mask);

	return 0;
}

static void tegra_sdhci_skip_host_clkgate(struct sdhci_host *host, bool req)
{
	if (req)
		host->mmc->skip_host_clkgate = true;
	else
		host->mmc->skip_host_clkgate = false;
}

static void sdhci_tegra_sd_express_mode_select(struct sdhci_host *host, bool req)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	int ret;

	if (req) {
		if (!IS_ERR_OR_NULL(tegra_host->pinctrl_state_sdexp_enable)) {
			ret = pinctrl_select_state(tegra_host->pinctrl_sdmmc,
						tegra_host->pinctrl_state_sdexp_enable);
			if (ret < 0) {
				pr_err("%s: Dynamic switch to SD express mode failed\n",
						mmc_hostname(host->mmc));
			}
		}
	} else {
		if (!IS_ERR_OR_NULL(tegra_host->pinctrl_state_sdexp_disable)) {
			ret = pinctrl_select_state(tegra_host->pinctrl_sdmmc,
						tegra_host->pinctrl_state_sdexp_disable);
			if (ret < 0) {
				pr_err("%s: Dynamic switch to SD mode operation failed\n",
						mmc_hostname(host->mmc));
			}
		}
	}

	if (gpio_is_valid(tegra_host->mux_sel_gpio)) {
		if (req == false) {
			gpio_set_value_cansleep(tegra_host->mux_sel_gpio, 0);
			dev_info(mmc_dev(host->mmc),
				 "SD mode set by mux selection gpio\n");
		} else {
			gpio_set_value_cansleep(tegra_host->mux_sel_gpio, 1);
			dev_info(mmc_dev(host->mmc),
				 "SD express mode set by mux selection gpio\n");
		}
	} else {
		tegra_misc_sd_exp_mux_select(req);
	}
}

int register_notifier_from_sd(struct device *dev,
			      struct notifier_block *nb)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host;
	struct sdhci_tegra *tegra_host;

	if (host == NULL)
		return -EPROBE_DEFER;

	pltfm_host = sdhci_priv(host);
	tegra_host = sdhci_pltfm_priv(pltfm_host);

	if (!tegra_host->is_probe_done)
		return -EPROBE_DEFER;

	return blocking_notifier_chain_register(&tegra_host->notifier_from_sd, nb);
}
EXPORT_SYMBOL_GPL(register_notifier_from_sd);

int unregister_notifier_from_sd(struct device *dev,
				struct notifier_block *nb)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);

	return blocking_notifier_chain_unregister(&tegra_host->notifier_from_sd, nb);
}

struct device *get_sdhci_device_handle(struct device *dev)
{
	struct device_node *sd_node = NULL;
	struct platform_device *sd_pltfm_device = NULL;

	sd_node = of_parse_phandle(dev->of_node, "nvidia,sdmmc-instance", 0);
	if (!sd_node) {
		dev_dbg(dev, "Looking up %s property in node %pOF failed\n",
			"sdmmc-instance", dev->of_node);
		return NULL;
	}

	sd_pltfm_device = of_find_device_by_node(sd_node);
	if (!sd_pltfm_device) {
		dev_dbg(dev, "Finding platform device in node %pOF failed\n",
			sd_node);
	}
	return &sd_pltfm_device->dev;
}
EXPORT_SYMBOL_GPL(get_sdhci_device_handle);

static int notifier_from_sd_call_chain(struct sdhci_host *host, int value)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);

	return blocking_notifier_call_chain(&tegra_host->notifier_from_sd,
					    value, NULL);
}

int sdhci_tegra_notifier_handle(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct sdhci_tegra *tegra_host =
		container_of(self, struct sdhci_tegra, notifier);
	struct sdhci_host *host = tegra_host->host;
	int err = NOTIFY_OK;

	switch (event) {
	case CARD_IS_SD_ONLY:
		/* Handle SD card only event only for unexpected PCIe link failure */
		if (!host->mmc->rem_card_present) {
			err = NOTIFY_OK;
			goto out;
		}
		err = tegra_sdhci_pre_sd_exp_card_init(host, CARD_IS_SD_ONLY, 0);
		if (err)
			err = NOTIFY_BAD;
		else
			err = NOTIFY_OK;

		host->mmc->caps2 &= ~MMC_CAP2_SD_EXPRESS_SUPPORT;
		mmc_detect_change(host->mmc, 0);
		err = unregister_notifier_to_sd(host);
		break;
	default:
		err = NOTIFY_BAD;
		break;
	}
out:
	return err;
}

static int register_notifier_to_sd(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);

	tegra_host->notifier.notifier_call = &sdhci_tegra_notifier_handle;
	return blocking_notifier_chain_register(&tegra_host->notifier_to_sd,
							&tegra_host->notifier);
}

static int unregister_notifier_to_sd(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);

	return blocking_notifier_chain_unregister(&tegra_host->notifier_to_sd,
							&tegra_host->notifier);
}

int notifier_to_sd_call_chain(struct device *dev, int value)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);

	return blocking_notifier_call_chain(&tegra_host->notifier_to_sd,
						value, NULL);
}
EXPORT_SYMBOL_GPL(notifier_to_sd_call_chain);

static int tegra_sdhci_pre_sd_exp_card_init(struct sdhci_host *host,
					    int val, unsigned int mask)
{
	int err = 0;

	switch (val) {
	case CARD_INSERTED:
		err = notifier_from_sd_call_chain(host, val);
		if (err == NOTIFY_OK)
			err = 0;
		else
			err = -EIO;
		break;
	case CARD_IS_SD_EXPRESS:
		/* Turn off card clock */
		sdhci_set_card_clock(host, false);
		/* Set pinmux to PCIe */
		sdhci_tegra_sd_express_mode_select(host, true);
		/* Notify PCIe layer */
		if ((mask & SD_EXP_1V2_MASK) != 0U) {
			pr_info("%s: Trying link setup with VDD3\n",
				mmc_hostname(host->mmc));
			/* Enable VDD3 regulator */
			if (!IS_ERR(host->mmc->supply.vdd3)) {
				err = regulator_enable(host->mmc->supply.vdd3);
				if (err) {
					pr_err("%s: Failed to enable VDD3 regulator: %d\n",
					       mmc_hostname(host->mmc), err);
					host->mmc->supply.vdd3 =
						ERR_PTR(-EINVAL);
					err = 0;
					goto enable_vdd2;
				}
			}
			err = notifier_from_sd_call_chain(host, val);
			if (err != NOTIFY_OK) {
				pr_info("%s: Link setup fail with VDD3 err=%d\n",
					mmc_hostname(host->mmc), err);
				/* Disable VDD3 regulator */
				if (!IS_ERR(host->mmc->supply.vdd3))
					regulator_disable(host->mmc->supply.vdd3);
				err = 0;
			}
		}
enable_vdd2:
		if ((err == 0) && ((mask & SD_EXP_1V8_MASK) != 0U)) {
			pr_info("%s: Trying link setup with VDD2\n",
				mmc_hostname(host->mmc));
			/* Enable VDD2 regulator */
			if (!IS_ERR(host->mmc->supply.vdd2)) {
				err = regulator_enable(host->mmc->supply.vdd2);
				if (err) {
					pr_err("%s: Failed to enable vdd2 regulator: %d\n",
					       mmc_hostname(host->mmc), err);
					host->mmc->supply.vdd2 =
						ERR_PTR(-EINVAL);
					err = -EIO;
				} else {
					err = notifier_from_sd_call_chain(host,
									  val);
					if (err != NOTIFY_OK) {
						pr_err("%s: Link setup failed with VDD2 err=%d\n",
						       mmc_hostname(host->mmc),
							err);
						err = -EIO;
					}
				}
			}
		}
		if (err == NOTIFY_OK) {
			pr_info("%s: PCIe Link setup success\n",
				mmc_hostname(host->mmc));
			err = register_notifier_to_sd(host);
		}
		break;
	case CARD_REMOVED:
	case CARD_IS_SD_ONLY:
		/* Turn off VDD2/VDD3 */
		if (!IS_ERR(host->mmc->supply.vdd2) &&
		    regulator_is_enabled(host->mmc->supply.vdd2))
			regulator_disable(host->mmc->supply.vdd2);
		if (!IS_ERR(host->mmc->supply.vdd3) &&
		    regulator_is_enabled(host->mmc->supply.vdd3))
			regulator_disable(host->mmc->supply.vdd3);
		/* Set pinmux to SD */
		sdhci_tegra_sd_express_mode_select(host, false);
		/* Turn on card clock */
		sdhci_set_card_clock(host, true);
		err = 0;
		break;
	default:
		err = -EINVAL;
		break;
	}
	return err;
}

static const struct sdhci_ops tegra_sdhci_ops = {
	.get_ro     = tegra_sdhci_get_ro,
	.read_w     = tegra_sdhci_readw,
	.write_l    = tegra_sdhci_writel,
	.set_clock  = tegra_sdhci_set_clock,
	.set_dma_mask = tegra_sdhci_set_dma_mask,
	.set_bus_width = sdhci_set_bus_width,
	.reset      = tegra_sdhci_reset,
	.set_uhs_signaling = tegra_sdhci_set_uhs_signaling,
	.voltage_switch = tegra_sdhci_voltage_switch,
	.get_max_clock = tegra_sdhci_get_max_clock,
	.get_timeout_clock = tegra_sdhci_get_timeout_clock,
	.get_max_tuning_loop_counter = tegra_sdhci_get_max_tuning_loop_counter,
	.hs400_enhanced_strobe = tegra_sdhci_hs400_enhanced_strobe,
	.dump_vendor_regs = tegra_sdhci_dump_vendor_regs,
	.irq = sdhci_tegra_cqhci_irq,
	.get_sw_timeout = tegra_sdhci_get_sw_timeout_value,
	.voltage_switch_req	= tegra_sdhci_voltage_switch_req,
	.skip_host_clkgate	= tegra_sdhci_skip_host_clkgate,
	.pre_card_init = tegra_sdhci_pre_sd_exp_card_init,
	.card_event = tegra_sdhci_card_event,
};

static const struct sdhci_pltfm_data sdhci_tegra20_pdata = {
	.quirks = SDHCI_QUIRK_BROKEN_TIMEOUT_VAL |
		  SDHCI_QUIRK_SINGLE_POWER_WRITE |
		  SDHCI_QUIRK_NO_HISPD_BIT |
		  SDHCI_QUIRK_BROKEN_ADMA_ZEROLEN_DESC |
		  SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN,
	.ops  = &tegra_sdhci_ops,
};

static const struct sdhci_tegra_soc_data soc_data_tegra20 = {
	.pdata = &sdhci_tegra20_pdata,
	.dma_mask = DMA_BIT_MASK(32),
	.nvquirks = NVQUIRK_FORCE_SDHCI_SPEC_200 |
		    NVQUIRK_ENABLE_BLOCK_GAP_DET,
	.use_bwmgr = false,
};

static const struct sdhci_pltfm_data sdhci_tegra30_pdata = {
	.quirks = SDHCI_QUIRK_BROKEN_TIMEOUT_VAL |
		  SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK |
		  SDHCI_QUIRK_SINGLE_POWER_WRITE |
		  SDHCI_QUIRK_NO_HISPD_BIT |
		  SDHCI_QUIRK_BROKEN_ADMA_ZEROLEN_DESC |
		  SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN |
		   SDHCI_QUIRK2_BROKEN_HS200 |
		   /*
		    * Auto-CMD23 leads to "Got command interrupt 0x00010000 even
		    * though no command operation was in progress."
		    *
		    * The exact reason is unknown, as the same hardware seems
		    * to support Auto CMD23 on a downstream 3.1 kernel.
		    */
		   SDHCI_QUIRK2_ACMD23_BROKEN,
	.ops  = &tegra_sdhci_ops,
};

static const struct sdhci_tegra_soc_data soc_data_tegra30 = {
	.pdata = &sdhci_tegra30_pdata,
	.dma_mask = DMA_BIT_MASK(32),
	.nvquirks = NVQUIRK_ENABLE_SDHCI_SPEC_300 |
		    NVQUIRK_ENABLE_SDR50 |
		    NVQUIRK_ENABLE_SDR104 |
		    NVQUIRK_HAS_PADCALIB,
	.use_bwmgr = false,
};

static const struct sdhci_ops tegra114_sdhci_ops = {
	.get_ro     = tegra_sdhci_get_ro,
	.read_w     = tegra_sdhci_readw,
	.write_w    = tegra_sdhci_writew,
	.write_l    = tegra_sdhci_writel,
	.set_clock  = tegra_sdhci_set_clock,
	.set_dma_mask = tegra_sdhci_set_dma_mask,
	.set_bus_width = sdhci_set_bus_width,
	.reset      = tegra_sdhci_reset,
	.platform_execute_tuning = tegra_sdhci_execute_tuning,
	.set_uhs_signaling = tegra_sdhci_set_uhs_signaling,
	.voltage_switch = tegra_sdhci_voltage_switch,
	.get_max_clock = tegra_sdhci_get_max_clock,
};

static const struct sdhci_pltfm_data sdhci_tegra114_pdata = {
	.quirks = SDHCI_QUIRK_BROKEN_TIMEOUT_VAL |
		  SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK |
		  SDHCI_QUIRK_SINGLE_POWER_WRITE |
		  SDHCI_QUIRK_NO_HISPD_BIT |
		  SDHCI_QUIRK_BROKEN_ADMA_ZEROLEN_DESC |
		  SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN,
	.ops  = &tegra114_sdhci_ops,
};

static const struct sdhci_tegra_soc_data soc_data_tegra114 = {
	.pdata = &sdhci_tegra114_pdata,
	.dma_mask = DMA_BIT_MASK(32),
	.use_bwmgr = false,
};

static const struct sdhci_pltfm_data sdhci_tegra124_pdata = {
	.quirks = SDHCI_QUIRK_BROKEN_TIMEOUT_VAL |
		  SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK |
		  SDHCI_QUIRK_SINGLE_POWER_WRITE |
		  SDHCI_QUIRK_NO_HISPD_BIT |
		  SDHCI_QUIRK_BROKEN_ADMA_ZEROLEN_DESC |
		  SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN,
	.ops  = &tegra114_sdhci_ops,
};

static const struct sdhci_tegra_soc_data soc_data_tegra124 = {
	.pdata = &sdhci_tegra124_pdata,
	.dma_mask = DMA_BIT_MASK(34),
	.use_bwmgr = false,
};

static const struct sdhci_ops tegra210_sdhci_ops = {
	.get_ro     = tegra_sdhci_get_ro,
	.read_w     = tegra_sdhci_readw,
	.write_w    = tegra210_sdhci_writew,
	.write_l    = tegra_sdhci_writel,
	.set_clock  = tegra_sdhci_set_clock,
	.set_dma_mask = tegra_sdhci_set_dma_mask,
	.set_bus_width = sdhci_set_bus_width,
	.reset      = tegra_sdhci_reset,
	.set_uhs_signaling = tegra_sdhci_set_uhs_signaling,
	.voltage_switch = tegra_sdhci_voltage_switch,
	.get_max_clock = tegra_sdhci_get_max_clock,
	.set_timeout = tegra_sdhci_set_timeout,
};

static const struct sdhci_pltfm_data sdhci_tegra210_pdata = {
	.quirks = SDHCI_QUIRK_BROKEN_TIMEOUT_VAL |
		  SDHCI_QUIRK_SINGLE_POWER_WRITE |
		  SDHCI_QUIRK_NO_HISPD_BIT |
		  SDHCI_QUIRK_BROKEN_ADMA_ZEROLEN_DESC |
		  SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK |
		  SDHCI_QUIRK_BROKEN_CARD_DETECTION |
		  SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC |
		  SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN |
		   SDHCI_QUIRK2_ISSUE_CMD_DAT_RESET_TOGETHER |
		   SDHCI_QUIRK2_SEL_SDR104_UHS_MODE_IN_SDR50 |
		   SDHCI_QUIRK2_NON_STD_TUN_CARD_CLOCK,
	.ops  = &tegra210_sdhci_ops,
};

static const struct sdhci_tegra_soc_data soc_data_tegra210 = {
	.pdata = &sdhci_tegra210_pdata,
	.dma_mask = DMA_BIT_MASK(34),
	.nvquirks = NVQUIRK_NEEDS_PAD_CONTROL |
		    NVQUIRK_HAS_PADCALIB |
		    NVQUIRK_DIS_CARD_CLK_CONFIG_TAP |
		    NVQUIRK_ENABLE_SDR50 |
		    NVQUIRK_UPDATE_PIN_CNTRL_REG |
		    NVQUIRK_ENABLE_SDR104 |
		    NVQUIRK_CONTROL_TRIMMER_SUPPLY |
		    NVQUIRK_ENABLE_PERIODIC_CALIB |
		    NVQUIRK_HAS_TMCLK,
	.min_tap_delay = 106,
	.max_tap_delay = 185,
	.use_bwmgr = true,
};

static const struct sdhci_ops tegra186_sdhci_ops = {
	.get_ro     = tegra_sdhci_get_ro,
	.read_w     = tegra_sdhci_readw,
	.write_l    = tegra_sdhci_writel,
	.set_clock  = tegra_sdhci_set_clock,
	.set_dma_mask = tegra_sdhci_set_dma_mask,
	.set_bus_width = sdhci_set_bus_width,
	.reset      = tegra_sdhci_reset,
	.set_uhs_signaling = tegra_sdhci_set_uhs_signaling,
	.voltage_switch = tegra_sdhci_voltage_switch,
	.get_max_clock = tegra_sdhci_get_max_clock,
	.irq = sdhci_tegra_cqhci_irq,
	.set_timeout = tegra_sdhci_set_timeout,
};

static const struct sdhci_pltfm_data sdhci_tegra186_pdata = {
	.quirks = SDHCI_QUIRK_BROKEN_TIMEOUT_VAL |
		  SDHCI_QUIRK_SINGLE_POWER_WRITE |
		  SDHCI_QUIRK_NO_HISPD_BIT |
		  SDHCI_QUIRK_BROKEN_ADMA_ZEROLEN_DESC |
		  SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC,
	.quirks2 = SDHCI_QUIRK2_PRESET_VALUE_BROKEN |
		   SDHCI_QUIRK2_HOST_OFF_CARD_ON |
		   SDHCI_QUIRK2_ISSUE_CMD_DAT_RESET_TOGETHER |
		   SDHCI_QUIRK2_SEL_SDR104_UHS_MODE_IN_SDR50,
	.ops  = &tegra_sdhci_ops,
};

static const struct sdhci_tegra_soc_data soc_data_tegra186 = {
	.pdata = &sdhci_tegra186_pdata,
	.dma_mask = DMA_BIT_MASK(40),
	.nvquirks = NVQUIRK_NEEDS_PAD_CONTROL |
		    NVQUIRK_HAS_PADCALIB |
		    NVQUIRK_DIS_CARD_CLK_CONFIG_TAP |
		    NVQUIRK_ENABLE_SDR50 |
		    NVQUIRK_ENABLE_SDR104 |
		    NVQUIRK_SDMMC_CLK_OVERRIDE |
		    NVQUIRK_HAS_TMCLK |
		    NVQUIRK_CONTROL_TRIMMER_SUPPLY |
		    NVQUIRK_ENABLE_PERIODIC_CALIB |
		    NVQUIRK_CQHCI_DCMD_R1B_CMD_TIMING,
	.min_tap_delay = 84,
	.max_tap_delay = 136,
	.use_bwmgr = true,
};

static const struct sdhci_tegra_soc_data soc_data_tegra194 = {
	.pdata = &sdhci_tegra186_pdata,
	.dma_mask = DMA_BIT_MASK(39),
	.nvquirks = NVQUIRK_NEEDS_PAD_CONTROL |
		    NVQUIRK_HAS_PADCALIB |
		    NVQUIRK_DIS_CARD_CLK_CONFIG_TAP |
		    NVQUIRK_CONTROL_TRIMMER_SUPPLY |
		    NVQUIRK_ENABLE_SDR50 |
		    NVQUIRK_SDMMC_CLK_OVERRIDE |
		    NVQUIRK_ENABLE_SDR104 |
		    NVQUIRK_HAS_TMCLK,
	.min_tap_delay = 96,
	.max_tap_delay = 139,
	.use_bwmgr = true,
};

static const struct sdhci_tegra_soc_data soc_data_tegra234 = {
	.pdata = &sdhci_tegra186_pdata,
	.dma_mask = DMA_BIT_MASK(39),
	.nvquirks = NVQUIRK_NEEDS_PAD_CONTROL |
		    NVQUIRK_HAS_PADCALIB |
		    NVQUIRK_DIS_CARD_CLK_CONFIG_TAP |
		    NVQUIRK_CONTROL_TRIMMER_SUPPLY |
		    NVQUIRK_ENABLE_SDR50 |
		    NVQUIRK_SDMMC_CLK_OVERRIDE |
		    NVQUIRK_PROGRAM_MC_STREAMID |
		    NVQUIRK_ENABLE_SDR104 |
		    NVQUIRK_HAS_TMCLK,
	.min_tap_delay = 95,
	.max_tap_delay = 111,
	.min_host_clk = 20000000,
	.use_bwmgr = false,
};

static const struct sdhci_tegra_soc_data soc_data_tegra239 = {
	.pdata = &sdhci_tegra186_pdata,
	.dma_mask = DMA_BIT_MASK(39),
	.nvquirks = NVQUIRK_NEEDS_PAD_CONTROL |
		    NVQUIRK_HAS_PADCALIB |
		    NVQUIRK_DIS_CARD_CLK_CONFIG_TAP |
		    NVQUIRK_CONTROL_TRIMMER_SUPPLY |
		    NVQUIRK_ENABLE_SDR50 |
		    NVQUIRK_SDMMC_CLK_OVERRIDE |
		    NVQUIRK_ENABLE_SDR104 |
		    NVQUIRK_ENABLE_TUNING_DQ_OFFSET |
		    NVQUIRK_HAS_TMCLK,
	.use_bwmgr = false,
};


static const struct of_device_id sdhci_tegra_dt_match[] = {
	{ .compatible = "nvidia,tegra239-sdhci", .data = &soc_data_tegra239 },
	{ .compatible = "nvidia,tegra234-sdhci", .data = &soc_data_tegra234 },
	{ .compatible = "nvidia,tegra194-sdhci", .data = &soc_data_tegra194 },
	{ .compatible = "nvidia,tegra186-sdhci", .data = &soc_data_tegra186 },
	{ .compatible = "nvidia,tegra210-sdhci", .data = &soc_data_tegra210 },
	{ .compatible = "nvidia,tegra124-sdhci", .data = &soc_data_tegra124 },
	{ .compatible = "nvidia,tegra114-sdhci", .data = &soc_data_tegra114 },
	{ .compatible = "nvidia,tegra30-sdhci", .data = &soc_data_tegra30 },
	{ .compatible = "nvidia,tegra20-sdhci", .data = &soc_data_tegra20 },
	{}
};
MODULE_DEVICE_TABLE(of, sdhci_tegra_dt_match);

static int sdhci_tegra_add_host(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	struct cqhci_host *cq_host;
	bool dma64;
	int ret;

	if (!tegra_host->enable_hwcq)
		return sdhci_add_host(host);

	sdhci_enable_v4_mode(host);

	ret = sdhci_setup_host(host);
	if (ret)
		return ret;

	host->mmc->caps2 |= MMC_CAP2_CQE | MMC_CAP2_CQE_DCMD;

	cq_host = devm_kzalloc(host->mmc->parent,
				sizeof(*cq_host), GFP_KERNEL);
	if (!cq_host) {
		ret = -ENOMEM;
		goto cleanup;
	}

	cq_host->mmio = host->ioaddr + SDHCI_TEGRA_CQE_BASE_ADDR;
	cq_host->ops = &sdhci_tegra_cqhci_ops;

	dma64 = host->flags & SDHCI_USE_64_BIT_DMA;
	if (dma64)
		cq_host->caps |= CQHCI_TASK_DESC_SZ_128;

	ret = cqhci_init(cq_host, host->mmc, dma64);
	if (ret)
		goto cleanup;

	ret = __sdhci_add_host(host);
	if (ret)
		goto cleanup;

	return 0;

cleanup:
	sdhci_cleanup_host(host);
	return ret;
}

static void sdhci_delayed_detect(struct work_struct *work)
{
	struct sdhci_tegra *tegra_host;
	struct sdhci_host *host;
	struct sdhci_pltfm_host *pltfm_host;

	tegra_host = container_of(work, struct sdhci_tegra, detect_delay.work);
	host = tegra_host->host;
	pltfm_host = sdhci_priv(host);

	if (sdhci_tegra_add_host(host))
		goto err_add_host;

	/* Initialize debugfs */
	sdhci_tegra_debugfs_init(host);

	if (!tegra_host->skip_clk_rst && !tegra_host->disable_rtpm) {
		pm_runtime_set_active(mmc_dev(host->mmc));
		pm_runtime_set_autosuspend_delay(mmc_dev(host->mmc), SDHCI_TEGRA_RTPM_TIMEOUT_MS);
		pm_runtime_use_autosuspend(mmc_dev(host->mmc));
		pm_suspend_ignore_children(mmc_dev(host->mmc), true);
		pm_runtime_enable(mmc_dev(host->mmc));
	}
	return;

err_add_host:
	if (!tegra_host->skip_clk_rst) {
		clk_disable_unprepare(tegra_host->tmclk);
		reset_control_assert(tegra_host->rst);
		clk_disable_unprepare(pltfm_host->clk);
	}
}

static int sdhci_tegra_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *match;
	const struct sdhci_tegra_soc_data *soc_data;
	struct sdhci_host *host;
	struct sdhci_pltfm_host *pltfm_host;
	struct sdhci_tegra *tegra_host;
	struct clk *clk;
	struct iommu_fwspec *fwspec;
	int rc;

	match = of_match_device(sdhci_tegra_dt_match, &pdev->dev);
	if (!match)
		return -EINVAL;
	soc_data = match->data;

	host = sdhci_pltfm_init(pdev, soc_data->pdata, sizeof(*tegra_host));
	if (IS_ERR(host))
		return PTR_ERR(host);
	pltfm_host = sdhci_priv(host);

	tegra_host = sdhci_pltfm_priv(pltfm_host);
	tegra_host->ddr_signaling = false;
	tegra_host->pad_calib_required = false;
	tegra_host->pad_control_available = false;
	tegra_host->is_probe_done = false;
	tegra_host->soc_data = soc_data;
	tegra_host->host = host;

	INIT_DELAYED_WORK(&tegra_host->detect_delay, sdhci_delayed_detect);

	if (soc_data->nvquirks & NVQUIRK_NEEDS_PAD_CONTROL) {
		rc = tegra_sdhci_init_pinctrl_info(&pdev->dev, tegra_host);
		if (rc == 0)
			host->mmc_host_ops.start_signal_voltage_switch =
				sdhci_tegra_start_signal_voltage_switch;
	}

	/* Hook to periodically rerun pad calibration */
	if (soc_data->nvquirks & NVQUIRK_HAS_PADCALIB) {
		host->mmc_host_ops.request = tegra_sdhci_request;
		tegra_host->defer_calib = false;
	}

	if (!host->ops->platform_execute_tuning)
		host->mmc_host_ops.execute_tuning =
				tegra_sdhci_execute_hw_tuning;

	rc = mmc_of_parse(host->mmc);
	if (rc)
		goto err_parse_dt;

	tegra_host->instance = of_alias_get_id(pdev->dev.of_node, "sdhci");

	host->mmc->caps |= MMC_CAP_WAIT_WHILE_BUSY;

	if (tegra_host->soc_data->nvquirks & NVQUIRK_ENABLE_DDR50)
		host->mmc->caps |= MMC_CAP_1_8V_DDR;

	/* HW busy detection is supported, but R1B responses are required. */
	host->mmc->caps |= MMC_CAP_WAIT_WHILE_BUSY | MMC_CAP_NEED_RSP_BUSY;

	/*
	 * Set host ocr for populating support for 3.3V and 1.8V in case
	 * VMMC regulator is not populated. The value gets overwritten by the regulator
	 * calls if a valid VMMC regulator is populated.
	 */
	host->ocr_mask = MMC_VDD_27_36 | MMC_VDD_165_195;

	tegra_sdhci_parse_dt(host);

	tegra_host->power_gpio = devm_gpiod_get_optional(&pdev->dev, "power",
							 GPIOD_OUT_HIGH);
	if (IS_ERR(tegra_host->power_gpio)) {
		rc = PTR_ERR(tegra_host->power_gpio);
		goto err_power_req;
	}

	if (!tegra_host->skip_clk_rst && tegra_host->soc_data->use_bwmgr) {
		tegra_host->emc_clk =
			tegra_bwmgr_register(sdmmc_emc_client_id[tegra_host->instance]);

		if (tegra_host->emc_clk == ERR_PTR(-EAGAIN)) {
			rc = -EPROBE_DEFER;
			goto err_power_req;
		}
		if (IS_ERR_OR_NULL(tegra_host->emc_clk))
			dev_err(mmc_dev(host->mmc),
				"BWMGR client registration for eMC failed\n");
		else
			dev_info(mmc_dev(host->mmc),
				"BWMGR client registration for eMC Successful\n");
	}
	/*
	 * Tegra210 has a separate SDMMC_LEGACY_TM clock used for host
	 * timeout clock and SW can choose TMCLK or SDCLK for hardware
	 * data timeout through the bit USE_TMCLK_FOR_DATA_TIMEOUT of
	 * the register SDHCI_TEGRA_VENDOR_SYS_SW_CTRL.
	 *
	 * USE_TMCLK_FOR_DATA_TIMEOUT bit default is set to 1 and SDMMC uses
	 * 12Mhz TMCLK which is advertised in host capability register.
	 * With TMCLK of 12Mhz provides maximum data timeout period that can
	 * be achieved is 11s better than using SDCLK for data timeout.
	 *
	 * So, TMCLK is set to 12Mhz and kept enabled all the time on SoC's
	 * supporting separate TMCLK.
	 */

	if (soc_data->nvquirks & NVQUIRK_HAS_TMCLK && !tegra_host->skip_clk_rst) {
		clk = devm_clk_get(&pdev->dev, "tmclk");
		if (IS_ERR(clk)) {
			rc = PTR_ERR(clk);
			if (rc == -EPROBE_DEFER)
				goto err_power_req;

			dev_warn(&pdev->dev, "failed to get tmclk: %d\n", rc);
			clk = NULL;
		}

		clk_set_rate(clk, 12000000);
		rc = clk_prepare_enable(clk);
		if (rc) {
			dev_err(&pdev->dev,
				"failed to enable tmclk: %d\n", rc);
			goto err_power_req;
		}

		tegra_host->tmclk = clk;
	}

	if (!tegra_host->skip_clk_rst) {
		clk = devm_clk_get(mmc_dev(host->mmc), NULL);
		if (IS_ERR(clk)) {
			rc = dev_err_probe(&pdev->dev, PTR_ERR(clk),
					   "failed to get clock\n");
			goto err_clk_get;
		}
		clk_prepare_enable(clk);
		pltfm_host->clk = clk;

		tegra_host->rst = devm_reset_control_get_exclusive(&pdev->dev,
							   "sdhci");
		if (IS_ERR(tegra_host->rst)) {
			rc = PTR_ERR(tegra_host->rst);
			dev_err(&pdev->dev, "failed to get reset control: %d\n", rc);
			goto err_rst_get;
		}

		rc = reset_control_assert(tegra_host->rst);
		if (rc)
			goto err_rst_get;

		usleep_range(2000, 4000);

		rc = reset_control_deassert(tegra_host->rst);
		if (rc)
			goto err_rst_get;

		usleep_range(2000, 4000);
	}
	if (tegra_host->force_non_rem_rescan)
		host->mmc->caps2 |= MMC_CAP2_FORCE_RESCAN;

	if (!en_boot_part_access)
		host->mmc->caps2 |= MMC_CAP2_BOOTPART_NOACC;

	if (tegra_host->en_periodic_cflush)
		host->mmc->caps2 |= MMC_CAP2_PERIODIC_CACHE_FLUSH;

	tegra_host->volt_switch_gpio = of_get_named_gpio(np,
			"nvidia,voltage-switch-gpio", 0);
	tegra_host->mux_sel_gpio = of_get_named_gpio(np,
						     "nvidia,sdexp-sel-gpio",
						     0);
	if (gpio_is_valid(tegra_host->volt_switch_gpio)) {
		rc = gpio_request(tegra_host->volt_switch_gpio, "sdhci_power");
		if (rc)
			dev_err(mmc_dev(host->mmc),
				"failed to allocate gpio for voltage switch, "
				"err: %d\n", rc);
		gpio_direction_output(tegra_host->volt_switch_gpio, 1);
		gpio_set_value(tegra_host->volt_switch_gpio, 1);
		dev_info(mmc_dev(host->mmc),
				"3.3V set initially by voltage switch gpio\n");
	}

	tegra_host->cd_gpio = of_get_named_gpio(np, "cd-gpios", 0);
	if (gpio_is_valid(tegra_host->cd_gpio) &&
			tegra_host->cd_wakeup_capable) {
		tegra_host->cd_irq = gpio_to_irq(tegra_host->cd_gpio);
		if (tegra_host->cd_irq <= 0) {
			dev_err(mmc_dev(host->mmc),
				"failed to get gpio irq %d\n",
				tegra_host->cd_irq);
			tegra_host->cd_irq = 0;
		} else {
			device_init_wakeup(&pdev->dev, 1);
			dev_info(mmc_dev(host->mmc),
				"wakeup init done, cdirq %d\n",
				tegra_host->cd_irq);
		}
	}

	if (host->mmc->caps2 & MMC_CAP2_SD_EXPRESS_SUPPORT) {
		BLOCKING_INIT_NOTIFIER_HEAD(&tegra_host->notifier_from_sd);
		BLOCKING_INIT_NOTIFIER_HEAD(&tegra_host->notifier_to_sd);
		tegra_sdhci_init_sdexp_pinctrl_info(tegra_host);
		sdhci_tegra_sd_express_mode_select(host, false);
		tegra_host->sd_exp_support = true;
	}

	if (tegra_platform_is_vsp()) {
		host->quirks2 |= SDHCI_QUIRK2_BROKEN_64_BIT_DMA;
		host->mmc->caps2 |= MMC_CAP2_BROKEN_CARD_BUSY_DETECT;
	}

	/*
	 * If there is no card detect gpio, assume that the
	 * card is always present.
	 */
	if (!gpio_is_valid(tegra_host->cd_gpio)) {
		host->mmc->rem_card_present = 1;
	} else {
		if (!host->mmc->cd_cap_invert)
			host->mmc->rem_card_present =
				(mmc_gpio_get_cd(host->mmc) == 0);
		else
			host->mmc->rem_card_present =
				mmc_gpio_get_cd(host->mmc);
	}

	rc = mmc_regulator_get_supply(host->mmc);
	if (rc < 0 ) {
		if (rc != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Parsing regulators failed: %d\n", rc);
		goto err_rst_get;
	}

	if (gpio_is_valid(tegra_host->mux_sel_gpio)) {
		rc = gpio_request(tegra_host->mux_sel_gpio, "sdexp_select");
		if (rc) {
			dev_err(mmc_dev(host->mmc),
				"failed to allocate gpio for mux selection err: %d\n",
				rc);
			host->mmc->caps2 &= ~MMC_CAP2_SD_EXPRESS_SUPPORT;
		} else {
			gpio_direction_output(tegra_host->mux_sel_gpio, 1);
			gpio_set_value_cansleep(tegra_host->mux_sel_gpio, 0);
			dev_info(mmc_dev(host->mmc),
				 "SD mode initially set by mux selection GPIO\n");
		}
	}

	/* Program MC streamID for DMA transfers */
	if (soc_data->nvquirks & NVQUIRK_PROGRAM_MC_STREAMID) {
		fwspec = dev_iommu_fwspec_get(&pdev->dev);
		if (fwspec == NULL) {
			rc = -ENODEV;
			dev_err(mmc_dev(host->mmc),
				"failed to get MC streamid: %d\n",
				rc);
			goto err_rst_get;
		} else {
			tegra_host->streamid = fwspec->ids[0] & 0xffff;
			tegra_sdhci_writel(host, tegra_host->streamid |
						(tegra_host->streamid << 8),
						SDHCI_TEGRA_CIF2AXI_CTRL_0);
		}
	}

	tegra_host->is_probe_done = true;

	schedule_delayed_work(&tegra_host->detect_delay,
			      msecs_to_jiffies(tegra_host->boot_detect_delay));

	return 0;

err_rst_get:
	if (!tegra_host->skip_clk_rst)
		clk_disable_unprepare(pltfm_host->clk);
err_clk_get:
	if (!tegra_host->skip_clk_rst)
		clk_disable_unprepare(tegra_host->tmclk);
err_power_req:
err_parse_dt:
	sdhci_pltfm_free(pdev);
	return rc;
}

static int sdhci_tegra_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);

	sdhci_remove_host(host, 0);

	if (!tegra_host->skip_clk_rst) {
		reset_control_assert(tegra_host->rst);
		usleep_range(2000, 4000);
		clk_disable_unprepare(pltfm_host->clk);
		clk_disable_unprepare(tegra_host->tmclk);
	}

	if (!tegra_host->disable_rtpm)
		pm_runtime_disable(mmc_dev(host->mmc));

	debugfs_remove_recursive(tegra_host->sdhcid);

	sdhci_pltfm_free(pdev);

	return 0;
}

static int sdhci_tegra_runtime_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	struct mmc_host *mmc = host->mmc;
	int ret, rc;

	if (host->mmc->caps2 & MMC_CAP2_CQE) {
		mmc->cqe_ops->cqe_off(mmc);
	}

	ret = sdhci_runtime_suspend_host(host);
	if (ret < 0)
		return ret;

	if (host->tuning_mode != SDHCI_TUNING_MODE_3)
		mmc_retune_needed(host->mmc);

	/* Disable SDMMC internal clock */
	sdhci_set_clock(host, 0);

	if (tegra_host->soc_data->use_bwmgr && tegra_host->emc_clk &&
		!tegra_host->skip_clk_rst) {
		ret = tegra_bwmgr_set_emc(tegra_host->emc_clk, 0,
						TEGRA_BWMGR_SET_EMC_SHARED_BW);
		if (ret) {
			dev_err(mmc_dev(host->mmc),
				"disabling eMC clock failed, err: %d\n",
				ret);
			rc = sdhci_runtime_resume_host(host, true);
			if (rc) {
				dev_err(mmc_dev(host->mmc),
				"Failed to runtime resume the host err: %d\n",
				rc);
			}
			return ret;
		}
	}

	/* Disable SDMMC host CAR clock and BG trimmer supply */
	return tegra_sdhci_set_host_clock(host, false);
}

static int sdhci_tegra_runtime_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	unsigned int clk;
	int ret = 0;

	/* Clock enable should be invoked with a non-zero freq */
	if (host->clock)
		clk = host->clock;
	else if (host->mmc->ios.clock)
		clk = host->mmc->ios.clock;
	else
		clk = SDHCI_TEGRA_FALLBACK_CLK_HZ;

	/* Enable SDMMC host CAR clock and BG trimmer supply */
	ret = tegra_sdhci_set_host_clock(host, true);

	/* Enable SDMMC internal clocks */
	sdhci_set_clock(host, clk);

	/*
	 * Defer auto-calibration in RTPM context so that it can be run
	 * only once before the incoming request.
	 */
	tegra_host->defer_calib = true;
	ret = sdhci_runtime_resume_host(host, true);
	tegra_host->defer_calib = false;
	if (ret)
		goto disable_car_clk;

	if (host->mmc->caps2 & MMC_CAP2_CQE)
		ret = cqhci_resume(host->mmc);

	if (tegra_host->soc_data->use_bwmgr && tegra_host->emc_clk &&
		!tegra_host->skip_clk_rst) {
		ret = tegra_bwmgr_set_emc(tegra_host->emc_clk, SDMMC_EMC_MAX_FREQ,
					TEGRA_BWMGR_SET_EMC_SHARED_BW);
		if (ret)
			dev_err(mmc_dev(host->mmc),
				"Boosting eMC clock failed, err: %d\n",
				ret);
	}

	tegra_host->tuning_status = TUNING_STATUS_RETUNE;

	return ret;

disable_car_clk:
	return tegra_sdhci_set_host_clock(host, false);
}

#ifdef CONFIG_PM_SLEEP
static int __maybe_unused sdhci_tegra_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	int ret;

	if (pm_runtime_status_suspended(dev)) {
		ret = tegra_sdhci_set_host_clock(host, true);
		if (ret)
			return ret;
	}

	/*
	 * PCIe driver does not have a mechanism to detect card insertion status
	 * and handle sudden card removal events.
	 * Send notification to the PCIe driver with card removal event before
	 * SDHCI proceeds with its suspend sequence. This ensures safe
	 * card removal from the PCIe subsystem and avoids crash in case the
	 * card is removed during resume.
	 * During resume, if the card is kept inserted, the SDHCI driver
	 * retriggers the init sequence and attaches the card in PCIe mode.
	 */
	if (host->mmc->is_card_sd_express) {
		ret = notifier_from_sd_call_chain(host, CARD_REMOVED);
		if (ret != NOTIFY_OK) {
			pr_warn("%s: SD express card removal failed in suspend\n",
				mmc_hostname(host->mmc));
		}
		sdhci_set_power(host, MMC_POWER_OFF, 0);
		ret = tegra_sdhci_pre_sd_exp_card_init(host, CARD_REMOVED, 0);
		if (ret)
			return ret;
		unregister_notifier_to_sd(host);
		host->mmc->is_card_sd_express = false;
	}

	if (host->mmc->caps2 & MMC_CAP2_CQE) {
		ret = cqhci_suspend(host->mmc);
		if (ret)
			return ret;
	}

	ret = sdhci_suspend_host(host);
	if (ret) {
		if (host->mmc->caps2 & MMC_CAP2_CQE)
			cqhci_resume(host->mmc);
		return ret;
	}

	/* Enable wake irq at end of suspend */
	if (device_may_wakeup(dev)) {
		ret = enable_irq_wake(tegra_host->cd_irq);
		if (ret) {
			dev_err(mmc_dev(host->mmc),
				"Failed to enable wake irq %u, err %d\n",
				tegra_host->cd_irq, ret);
			tegra_host->wake_enable_failed = true;
		}
	}

	return tegra_sdhci_set_host_clock(host, false);
}

static int __maybe_unused sdhci_tegra_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	int ret;

	if (device_may_wakeup(dev)) {
		if (!tegra_host->wake_enable_failed) {
			ret = disable_irq_wake(tegra_host->cd_irq);
			if (ret)
				dev_err(mmc_dev(host->mmc),
					"Failed to disable wakeirq %u,err %d\n",
					tegra_host->cd_irq, ret);
		}
	}

	if (gpio_is_valid(tegra_host->cd_gpio)) {
		if (!host->mmc->cd_cap_invert)
			host->mmc->rem_card_present =
				(mmc_gpio_get_cd(host->mmc) == 0);
		else
			host->mmc->rem_card_present =
				mmc_gpio_get_cd(host->mmc);
	} else {
		host->mmc->rem_card_present = true;
	}

	tegra_host->tuning_status = TUNING_STATUS_RETUNE;

	ret = tegra_sdhci_set_host_clock(host, true);
	if (ret)
		return ret;

	/* Re-program MC streamID for DMA transfers */
	if (tegra_host->soc_data->nvquirks & NVQUIRK_PROGRAM_MC_STREAMID) {
		tegra_sdhci_writel(host, tegra_host->streamid |
					(tegra_host->streamid << 8),
					SDHCI_TEGRA_CIF2AXI_CTRL_0);
	}

	ret = sdhci_resume_host(host);
	if (ret)
		goto disable_clk;

	if (host->mmc->caps2 & MMC_CAP2_CQE) {
		ret = cqhci_resume(host->mmc);
		if (ret)
			goto suspend_host;
	}

	/* Detect change in the card state over suspend/resume cycles */
	if (mmc_card_is_removable(host->mmc) ||
		(host->mmc->caps2 & MMC_CAP2_SD_EXPRESS_SUPPORT)) {
		mmc_detect_change(host->mmc, 0);
	}
	return 0;

suspend_host:
	sdhci_suspend_host(host);
disable_clk:
	tegra_sdhci_set_host_clock(host, false);
	return ret;
}
#endif

static int sdhci_tegra_card_detect(struct sdhci_host *host, bool req)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	bool card_present = false;
	int err = 0;

	if (!(host->mmc->caps & MMC_CAP_NONREMOVABLE))
		if (host->mmc->rem_card_present)
			card_present = true;
	/* Check if card is inserted physically before performing */
	if (gpio_is_valid(tegra_host->cd_gpio)) {
		if ((mmc_gpio_get_cd(host->mmc)  == 1) &&
			(!host->mmc->cd_cap_invert)) {
			err = -ENXIO;
			dev_err(mmc_dev(host->mmc),
				"Card not inserted in slot\n");
			goto err_config;
		} else if ((mmc_gpio_get_cd(host->mmc)  == 0) &&
				(host->mmc->cd_cap_invert)) {
			err = -ENXIO;
			dev_err(mmc_dev(host->mmc),
				"Card not inserted in slot\n");
			goto err_config;
		}
	}

	/* Ignore the request if card already in requested state */
	if (card_present == req) {
		dev_info(mmc_dev(host->mmc),
			"Card already in requested state\n");
		goto err_config;
	} else {
		card_present = req;
	}

	if (card_present) {
		/* Virtual card insertion */
		host->mmc->rem_card_present = true;
		host->mmc->rescan_disable = 0;
		/* If vqmmc regulator and no 1.8V signalling,
		 *  then there's no UHS
		 */
		if (!IS_ERR(host->mmc->supply.vqmmc)) {
			err = regulator_enable(host->mmc->supply.vqmmc);
			if (err) {
				pr_warn("%s: Failed to enable vqmmc regulator: %d\n",
					mmc_hostname(host->mmc), err);
				host->mmc->supply.vqmmc = ERR_PTR(-EINVAL);
				goto err_config;
			}
			tegra_host->is_rail_enabled = true;
		}
		/* If vmmc regulator and no 1.8V signalling,
		 * then there's no UHS
		 */
		if (!IS_ERR(host->mmc->supply.vmmc)) {
			err = regulator_enable(host->mmc->supply.vmmc);
			if (err) {
				pr_warn("%s: Failed to enable vmmc regulator; %d\n",
					mmc_hostname(host->mmc), err);
				host->mmc->supply.vmmc = ERR_PTR(-EINVAL);
				goto err_config;
			}
			tegra_host->is_rail_enabled = true;
		}
	} else {
		/* Virtual card removal */
		host->mmc->rem_card_present = false;
		host->mmc->rescan_disable = 0;
		if (tegra_host->is_rail_enabled) {
			if (!IS_ERR(host->mmc->supply.vqmmc))
				regulator_disable(host->mmc->supply.vqmmc);
			if (!IS_ERR(host->mmc->supply.vmmc))
				regulator_disable(host->mmc->supply.vmmc);
			tegra_host->is_rail_enabled = false;
		}
	}
	host->mmc->trigger_card_event = true;
	mmc_detect_change(host->mmc, msecs_to_jiffies(200));

err_config:
	return err;
}

static int get_card_insert(void *data, u64 *val)
{
	struct sdhci_host *host = data;

	*val = host->mmc->rem_card_present;

	return 0;
}

static int set_card_insert(void *data, u64 val)
{
	struct sdhci_host *host = data;
	int err = 0;

	if (val > 1) {
		err = -EINVAL;
		dev_err(mmc_dev(host->mmc),
			"Usage error. Use 0 to remove, 1 to insert %d\n", err);
		goto err_detect;
	}

	if (host->mmc->caps & MMC_CAP_NONREMOVABLE) {
		err = -EINVAL;
		dev_err(mmc_dev(host->mmc),
			"usage error, Supports SDCARD hosts only %d\n", err);
		goto err_detect;
	}

	err = sdhci_tegra_card_detect(host, val);

err_detect:
	return err;
}

DEFINE_SIMPLE_ATTRIBUTE(sdhci_tegra_card_insert_fops, get_card_insert,
	set_card_insert, "%llu\n");

static void sdhci_tegra_debugfs_init(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_tegra *tegra_host = sdhci_pltfm_priv(pltfm_host);
	struct dentry *sdhcidir, *clkdir, *retval;

	sdhcidir = debugfs_create_dir(dev_name(mmc_dev(host->mmc)), NULL);
	if (!sdhcidir) {
		dev_err(mmc_dev(host->mmc), "Failed to create debugfs\n");
		return;
	}

	tegra_host->sdhcid = sdhcidir;

	/* Create clock debugfs dir under sdhci debugfs dir */
	clkdir = debugfs_create_dir("clock_data", sdhcidir);
	if (!clkdir)
		goto err;

	retval = debugfs_create_bool("slcg_status", S_IRUGO, clkdir,
				     &tegra_host->slcg_status);
	if (!retval)
		goto err;

	retval = debugfs_create_ulong("curr_clk_rate", S_IRUGO, clkdir,
		&tegra_host->curr_clk_rate);
	if (!retval)
		goto err;
	
	/* backup original host timing capabilities as
	 * debugfs may override it later
	 */
	host->caps_timing_orig = host->mmc->caps &
				(MMC_CAP_SD_HIGHSPEED | MMC_CAP_UHS_DDR50
				 | MMC_CAP_UHS_SDR12 | MMC_CAP_UHS_SDR25
				 | MMC_CAP_UHS_SDR50 | MMC_CAP_UHS_SDR104);

	retval = debugfs_create_file("card_insert", S_IRUSR | S_IWUSR,
			sdhcidir, host, &sdhci_tegra_card_insert_fops);

	if (!retval)
		goto err;

	return;
err:
	debugfs_remove_recursive(sdhcidir);
	sdhcidir = NULL;
	dev_err(mmc_dev(host->mmc), "%s %s\n"
		, __func__, mmc_hostname(host->mmc));
	return;
}

const struct dev_pm_ops sdhci_tegra_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sdhci_tegra_suspend, sdhci_tegra_resume)
	SET_RUNTIME_PM_OPS(sdhci_tegra_runtime_suspend,
			   sdhci_tegra_runtime_resume, NULL)
};

static struct platform_driver sdhci_tegra_driver = {
	.driver		= {
		.name	= "sdhci-tegra",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table = sdhci_tegra_dt_match,
		.pm	= &sdhci_tegra_dev_pm_ops,
	},
	.probe		= sdhci_tegra_probe,
	.remove		= sdhci_tegra_remove,
};

module_platform_driver(sdhci_tegra_driver);

module_param(en_boot_part_access, uint, 0444);

MODULE_DESCRIPTION("SDHCI driver for Tegra");
MODULE_AUTHOR("Google, Inc.");
MODULE_LICENSE("GPL v2");
