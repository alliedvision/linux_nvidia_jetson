// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/delay.h>
#include "tegra_i2s_regs.h"
#include "tegra_i2s.h"
#include <asm-generic/delay.h>

static inline struct i2s_dev *i2s_inst(unsigned int id)
{
	static struct i2s_dev *i2s;

	if (!i2s)
		i2s = safety_i2s_get_priv();

	BUG_ON(id >= NUM_SAFETY_I2S_INST);

	return &i2s[id];
}
#define I2S_BASE(id) ((i2s_inst(id))->base)

#ifdef SAFETY_I2S_DEBUG
void i2s_dump_all_regs(unsigned int id)
{
	int i;

	pr_alert("RX Registers:\n");
	for (i = 0; i <= 0x2c; i += 4)
		pr_alert("0x%08x = 0x%08x\n", 0x02450000 + id * 0x10000 + i,
						readl(I2S_BASE(id) + i));

	pr_alert("TX Registers:\n");
	for (i = 0x80; i <= 0xb0; i += 4)
		pr_alert("0x%08x = 0x%08x\n", 0x02450000 + id * 0x10000 + i,
						readl(I2S_BASE(id) + i));

	pr_alert("Common Registers:\n");
	for (i = 0x100; i <= 0x120; i += 4)
		pr_alert("0x%08x = 0x%08x\n", 0x02450000 + id * 0x10000 + i,
						readl(I2S_BASE(id) + i));

}
#endif

static unsigned int is_i2s_enabled(unsigned int id)
{
	unsigned int val;

	val = readl(I2S_BASE(id) + T234_I2S_ENABLE);

	return (val & T234_I2S_EN_MASK);
}

unsigned int i2s_enable(unsigned int id)
{
	unsigned int val;

	val = readl(I2S_BASE(id) + T234_I2S_ENABLE);
	val |= T234_I2S_EN;
	writel(val, I2S_BASE(id) + T234_I2S_ENABLE);

	return 0;
}

unsigned int i2s_disable(unsigned int id)
{
	unsigned int val;

	val = readl(I2S_BASE(id) + T234_I2S_ENABLE);
	val &= ~T234_I2S_EN;
	writel(val, I2S_BASE(id) + T234_I2S_ENABLE);

	return 0;
}

static unsigned int is_i2s_tx_enabled(unsigned int id)
{
	unsigned int val;

	val = readl(I2S_BASE(id) + T234_I2S_TX_ENABLE);

	return (val & T234_I2S_EN_MASK);
}

unsigned int i2s_enable_tx(unsigned int id)
{
	unsigned int val;

	val = readl(I2S_BASE(id) + T234_I2S_TX_ENABLE);
	val |= T234_I2S_TX_EN;
	writel(val, I2S_BASE(id) + T234_I2S_TX_ENABLE);

	return 0;
}

static unsigned int is_i2s_rx_enabled(unsigned int id)
{
	unsigned int val;

	val = readl(I2S_BASE(id) + T234_I2S_RX_ENABLE);

	return (val & T234_I2S_EN_MASK);
}

unsigned int i2s_enable_rx(unsigned int id)
{
	unsigned int val;

	val = readl(I2S_BASE(id) + T234_I2S_RX_ENABLE);
	val |= T234_I2S_RX_EN;
	writel(val, I2S_BASE(id) + T234_I2S_RX_ENABLE);

	return 0;
}

static unsigned int is_i2s_loopback_enabled(unsigned int id)
{
	unsigned int val;

	val = readl(I2S_BASE(id) + T234_I2S_CTRL);

	return !!(val & T234_I2S_CTRL_LPBK_MASK);
}

unsigned int i2s_set_loopback(unsigned int id, unsigned int loopback_enable)
{
	unsigned int val;
	unsigned int enable;
	unsigned int rx_enable;
	unsigned int tx_enable;

	if (loopback_enable == is_i2s_loopback_enabled(id)) {
		pr_info("I2S%d already has loopback in %s state\n",
			(id + 1), loopback_enable ? "enabled" : "disabled");
		return 0;
	}

	/* I2S needs to be disabled before enabling Loopback */
	enable = is_i2s_enabled(id);

	if (enable) {
		tx_enable = is_i2s_tx_enabled(id);
		if (tx_enable)
			i2s_disable_tx(id);

		rx_enable = is_i2s_rx_enabled(id);
		if (rx_enable)
			i2s_disable_rx(id);

		i2s_disable(id);
	}

	val = readl(I2S_BASE(id) + T234_I2S_CTRL);
	val = loopback_enable ? (val | T234_I2S_CTRL_LPBK_EN) :
				(val & ~T234_I2S_CTRL_LPBK_MASK);
	writel(val, I2S_BASE(id) + T234_I2S_CTRL);

	if (enable) {
		i2s_enable(id);

		if (rx_enable)
			i2s_enable_rx(id);

		if (tx_enable)
			i2s_enable_tx(id);
	}

	pr_info("I2S%d loopback set to %s state\n",
			(id + 1), loopback_enable ? "enabled" : "disabled");
	return 0;
}

static int i2s_sw_reset(unsigned int id, int direction, int timeout)
{
	unsigned int sw_reset_reg, sw_reset_mask;
	unsigned int  sw_reset_en, sw_reset_default;
	unsigned int tx_fifo_ctrl, rx_fifo_ctrl, tx_ctrl, rx_ctrl, ctrl, val;
	int wait = timeout;

	tx_fifo_ctrl = readl(I2S_BASE(id) + T234_I2S_TX_FIFO_CTRL);
	rx_fifo_ctrl = readl(I2S_BASE(id) + T234_I2S_RX_FIFO_CTRL);
	tx_ctrl = readl(I2S_BASE(id) + T234_I2S_TX_CTRL);
	rx_ctrl = readl(I2S_BASE(id) + T234_I2S_RX_CTRL);
	ctrl = readl(I2S_BASE(id) + T234_I2S_CTRL);

	if (direction == PCM_STREAM_CAPTURE) {
		sw_reset_reg = T234_I2S_RX_SOFT_RESET;
		sw_reset_mask = T234_I2S_RX_SOFT_RESET_MASK;
		sw_reset_en = T234_I2S_RX_SOFT_RESET_EN;
		sw_reset_default = T234_I2S_RX_SOFT_RESET_DEFAULT;
	} else {
		sw_reset_reg = T234_I2S_TX_SOFT_RESET;
		sw_reset_mask = T234_I2S_TX_SOFT_RESET_MASK;
		sw_reset_en = T234_I2S_TX_SOFT_RESET_EN;
		sw_reset_default = T234_I2S_TX_SOFT_RESET_DEFAULT;
	}

	updatel(I2S_BASE(id) + sw_reset_reg, sw_reset_mask, sw_reset_en);

	do {
		val = readl(I2S_BASE(id) + sw_reset_reg);
		wait--;
		udelay(10);

		if (!wait) {
			pr_err("RESET bit not cleared yet\n");
			return -1;
		}
	} while (val & sw_reset_mask);

	updatel(I2S_BASE(id) + sw_reset_reg, sw_reset_mask, sw_reset_default);

	writel(tx_fifo_ctrl, I2S_BASE(id) + T234_I2S_TX_FIFO_CTRL);
	writel(rx_fifo_ctrl, I2S_BASE(id) + T234_I2S_RX_FIFO_CTRL);
	writel(tx_ctrl, I2S_BASE(id) + T234_I2S_TX_CTRL);
	writel(rx_ctrl, I2S_BASE(id) + T234_I2S_RX_CTRL);
	writel(ctrl, I2S_BASE(id) + T234_I2S_CTRL);

	return 0;
}

static int i2s_get_status(unsigned int id, int direction)
{
	unsigned int status_reg;

	status_reg = (direction == PCM_STREAM_CAPTURE) ?
		T234_I2S_RX_STATUS :
		T234_I2S_TX_STATUS;

	return readl(I2S_BASE(id) + status_reg);
}

/* Should be called after disabling RX */
static int i2s_rx_stop(unsigned int id)
{
	int dcnt = 10, ret;
	int status;

	/* wait until I2S RX ENABLE bit is cleared
	 * and FIFO becomes empty as the DMA is still on
	 */
	while (((status = i2s_get_status(id, PCM_STREAM_CAPTURE)) &
				T234_I2S_RX_STATUS_ENABLED) && dcnt--)
		udelay(10);

	if (dcnt < 0 || !(status & T234_I2S_RX_STATUS_FIFO_EMPTY)) {
		/* HW needs sw reset to make sure previous trans be clean */
		ret = i2s_sw_reset(id, PCM_STREAM_CAPTURE, 0xffff);
		if (ret) {
			pr_err("Failed at I2S%d_RX sw reset\n", id + 7);
			return ret;
		}
	}
	return 0;
}

unsigned int i2s_disable_rx(unsigned int id)
{
	unsigned int val;

	val = readl(I2S_BASE(id) + T234_I2S_RX_ENABLE);
	val &= ~T234_I2S_RX_EN;
	writel(val, I2S_BASE(id) + T234_I2S_RX_ENABLE);

	i2s_rx_stop(id);

	return 0;
}

/* Should be called after disabling TX */
static int i2s_tx_stop(unsigned int id)
{
	int dcnt = 10, ret;
	int status;

	/* wait until  I2S TX ENABLE bit is cleared */
	while ((status = i2s_get_status(id, PCM_STREAM_PLAYBACK) &
				T234_I2S_TX_STATUS_ENABLED) && dcnt--)
		udelay(10);

	if (dcnt < 0 || !(status & T234_I2S_TX_STATUS_FIFO_EMPTY)) {
		/* HW needs sw reset to make sure previous trans be clean */
		ret = i2s_sw_reset(id, PCM_STREAM_PLAYBACK, 0xffff);
		if (ret) {
			pr_err("Failed at I2S%d_TX sw reset\n", id + 7);
			return ret;
		}
	}
	return 0;
}

unsigned int i2s_disable_tx(unsigned int id)
{
	unsigned int val;

	val = readl(I2S_BASE(id) + T234_I2S_TX_ENABLE);
	val &= ~T234_I2S_TX_EN;
	writel(val, I2S_BASE(id) + T234_I2S_TX_ENABLE);

	i2s_tx_stop(id);

	return 0;
}

int i2s_configure(unsigned int id, struct i2s_config *config)
{
	unsigned int ctrl = 0, timing = 0, rxctrl = 0, txctrl = 0;
	unsigned int slotctrl = 0, tx_slotctrl = 0, rx_slotctrl = 0;
	unsigned int i2sclock, bitcnt;
	unsigned int clock_trim = config->clock_trim;
	unsigned int fifo_ctrl = 0, threshold = 0;
	volatile void __iomem *i2s_base = I2S_BASE(id);

	switch (config->mode) {
	case I2S_FRAME_FORMAT_I2S:
		if (config->clock_mode == I2S_MASTER)
			ctrl |= T234_I2S_CTRL_MASTER_EN;

		ctrl |= (config->fsync_width <<
				T234_I2S_CTRL_FSYNC_WIDTH_SHIFT);
		ctrl &= ~T234_I2S_CTRL_EDGE_CTRL_MASK;
		if (config->edge_ctrl == I2S_CLK_NEG_EDGE)
			ctrl |= T234_I2S_CTRL_EDGE_CTRL_NEG_EDGE;

		/* LRCK_MODE */
		ctrl &= ~T234_I2S_CTRL_FRAME_FORMAT_MASK;

		ctrl &= ~T234_I2S_CTRL_LRCK_POLARITY_MASK;
		if (config->clock_polarity == LRCK_HIGH)
			ctrl |= T234_I2S_CTRL_LRCK_POLARITY_HIGH;

		i2sclock = config->srate * config->bit_size * config->channels;

		if (config->bclk_ratio != 0)
			i2sclock *= config->bclk_ratio;

		bitcnt = (i2sclock / config->srate) - 1;

		if (i2sclock % (2 * config->srate))
			timing |= T234_I2S_TIMING_NON_SYM_EN;

		timing |= ((bitcnt >> 1) <<
				T234_I2S_TIMING_CHANNEL_BIT_CNT_SHIFT);

		ctrl |= (((config->bit_size >> 2) - 1) <<
				T234_I2S_CTRL_BIT_SIZE_SHIFT);
		rxctrl |= (config->offset <<
				T234_I2S_RX_CTRL_DATA_OFFSET_SHIFT) &
			T234_I2S_RX_CTRL_DATA_OFFSET_MASK;
		txctrl |= (config->offset <<
				T234_I2S_TX_CTRL_DATA_OFFSET_SHIFT) &
			T234_I2S_TX_CTRL_DATA_OFFSET_MASK;

		if (config->pcm_mask_bits) {
			rxctrl |= ((config->pcm_mask_bits) <<
					T234_I2S_RX_CTRL_MASK_BITS_SHIFT);
			txctrl |= ((config->pcm_mask_bits) <<
					T234_I2S_TX_CTRL_MASK_BITS_SHIFT);
		}

		txctrl &= ~T234_I2S_TX_CTRL_HIGHZ_CTRL_MASK;

		if (config->highz_ctrl == 1)
			txctrl |= T234_I2S_TX_CTRL_HIGHZ_CTRL_HIGHZ;
		else if (config->highz_ctrl == 2)
			txctrl |=
			T234_I2S_TX_CTRL_HIGHZ_CTRL_HIGHZ_ON_HALF_BIT_CLK;

		writel(timing, i2s_base + T234_I2S_TIMING);

		break;

	case I2S_FRAME_FORMAT_TDM:
		if (config->clock_mode == I2S_MASTER)
			ctrl |= T234_I2S_CTRL_MASTER_EN;
		ctrl |= (config->fsync_width <<
				T234_I2S_CTRL_FSYNC_WIDTH_SHIFT);

		ctrl &= ~T234_I2S_CTRL_EDGE_CTRL_MASK;
		if (config->edge_ctrl == I2S_CLK_NEG_EDGE)
			ctrl |= T234_I2S_CTRL_EDGE_CTRL_NEG_EDGE;

		ctrl &= ~T234_I2S_CTRL_FRAME_FORMAT_MASK;
		ctrl |= T234_I2S_CTRL_FRAME_FORMAT_FSYNC_MODE;

		ctrl &= ~T234_I2S_CTRL_LRCK_POLARITY_MASK;
		if (config->clock_polarity == LRCK_HIGH)
			ctrl |= T234_I2S_CTRL_LRCK_POLARITY_HIGH;

		ctrl |= (((config->bit_size >> 2) - 1) <<
					T234_I2S_CTRL_BIT_SIZE_SHIFT);

		i2sclock = config->srate * config->bit_size * config->channels;

		if (config->bclk_ratio != 0)
			i2sclock *= config->bclk_ratio;

		bitcnt = (i2sclock / config->srate) - 1;

		if (i2sclock % (2 * config->srate))
			timing |= T234_I2S_TIMING_NON_SYM_EN;

		timing |= (bitcnt << T234_I2S_TIMING_CHANNEL_BIT_CNT_SHIFT);

		rxctrl |= (config->offset <<
				T234_I2S_RX_CTRL_DATA_OFFSET_SHIFT) &
					T234_I2S_RX_CTRL_DATA_OFFSET_MASK;
		txctrl |= (config->offset <<
				T234_I2S_TX_CTRL_DATA_OFFSET_SHIFT) &
					T234_I2S_TX_CTRL_DATA_OFFSET_MASK;

		if (config->pcm_mask_bits) {
			rxctrl |= ((config->pcm_mask_bits) <<
					T234_I2S_RX_CTRL_MASK_BITS_SHIFT);
			txctrl |= ((config->pcm_mask_bits) <<
					T234_I2S_TX_CTRL_MASK_BITS_SHIFT);
		}

		txctrl &= ~T234_I2S_TX_CTRL_HIGHZ_CTRL_MASK;

		if (config->highz_ctrl == 1)
			txctrl |= T234_I2S_TX_CTRL_HIGHZ_CTRL_HIGHZ;
		else if (config->highz_ctrl == 2)
			txctrl |=
			T234_I2S_TX_CTRL_HIGHZ_CTRL_HIGHZ_ON_HALF_BIT_CLK;

		slotctrl |= ((config->total_slots - 1) <<
				T234_I2S_SLOT_CTRL_TOTAL_SLOTS_SHIFT);

		tx_slotctrl |= (config->tx_mask <<
				T234_I2S_TX_SLOT_CTRL_SLOT_ENABLES_SHIFT);
		rx_slotctrl |= (config->rx_mask <<
				T234_I2S_RX_SLOT_CTRL_SLOT_ENABLES_SHIFT);

		writel(timing, i2s_base + T234_I2S_TIMING);
		break;

	default:
		return -1;
	}

	if (clock_trim  > T234_I2S_SCLK_TRIM_SEL_MASK) {
		pr_alert("Clock trim invalid\n");
		return -1;
	}
	clock_trim = ((clock_trim & T234_I2S_SCLK_TRIM_SEL_MASK) <<
						T234_I2S_SCLK_TRIM_SEL_SHIFT);

	//TODO: Add proper register offset macros and masks etc.
	fifo_ctrl = (((config->channels - 1) << 4) & (0xf << 4)) |
			(((config->channels) << 16) & (0x7f << 16)) |
			(1 << 24);

	threshold = 0;

	/* Overwrite the timing register only in the master mode.
	 * Setting it to 0x0 causes noise during slave mode playback.
	 * Setting the I2S control registers
	 */
	writel(ctrl, i2s_base + T234_I2S_CTRL);
	writel(rxctrl, i2s_base + T234_I2S_RX_CTRL);
	writel(txctrl, i2s_base + T234_I2S_TX_CTRL);
	writel(slotctrl, i2s_base + T234_I2S_SLOT_CTRL);
	writel(tx_slotctrl, i2s_base + T234_I2S_TX_SLOT_CTRL);
	writel(rx_slotctrl, i2s_base + T234_I2S_RX_SLOT_CTRL);
	writel(clock_trim, i2s_base + T234_I2S_CLK_TRIM);
	writel(fifo_ctrl, i2s_base + T234_I2S_RX_FIFO_CTRL);
	writel(fifo_ctrl, i2s_base + T234_I2S_TX_FIFO_CTRL);
	writel(threshold, i2s_base + T234_I2S_TX_START_THRESHOLD);

	return 0;
}

