// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 */

#ifndef _TEGRA_I2S_H_
#define _TEGRA_I2S_H_

#define PCM_STREAM_PLAYBACK     0
#define PCM_STREAM_CAPTURE      1

#define NUM_SAFETY_I2S_INST 2
#define I2S_DT_NODE "i2s%u"
#define I2S_NODE_START_INDEX 7

enum clock_mode {
	I2S_SLAVE = 0,
	I2S_MASTER
};

enum edge_ctrl {
	I2S_CLK_POS_EDGE = 0,
	I2S_CLK_NEG_EDGE
};

enum clock_polarity {
	LRCK_LOW = 0,
	LRCK_HIGH
};

enum i2s_mode {
	I2S_FRAME_FORMAT_I2S = 0,
	I2S_FRAME_FORMAT_TDM
};

enum i2s_clocks {
	CLK_PLLA_OUT0,
	CLK_I2S,
	CLK_I2S_SOURCE,
	CLK_I2S_SYNC,
	CLK_AUDIO_SYNC,
	CLK_AUDIO_INPUT_SYNC,
	CLK_NUM_ENTRIES = CLK_AUDIO_INPUT_SYNC
};

struct i2s_config {
	unsigned int mode;
	unsigned int clock_mode;
	unsigned int clock_polarity;
	unsigned int edge_ctrl;
	unsigned int total_slots;
	unsigned int bclk;
	unsigned int bit_size;
	unsigned int channels;
	unsigned int offset;
	unsigned int tx_mask;
	unsigned int rx_mask;
	unsigned int srate;
	unsigned int bclk_ratio;
	unsigned int fsync_width;
	unsigned int pcm_mask_bits;
	unsigned int highz_ctrl;
	unsigned int clock_trim;
};

struct dma_data {
	const char *dma_chan_name;
	unsigned long addr;
	unsigned int size;
	unsigned int width;
	unsigned int req_sel;
	unsigned int triggered;
};

struct i2s_dev {
	volatile void __iomem *base;
	struct dma_data capture_data;
	struct dma_data playback_data;
	struct clk *clk_i2s;
	struct clk *clk_i2s_src;
	struct clk *audio_sync;
	struct clk *i2s_sync;
	struct clk *audio_sync_input;
	struct reset_control *reset;
	struct i2s_config config;

};

struct i2s_dev *safety_i2s_get_priv(void);

#ifdef SAFETY_I2S_DEBUG
void i2s_dump_all_regs(unsigned int id);
#endif

int i2s_configure(unsigned int id, struct i2s_config *config);

/* Enable I2S controller */
unsigned int i2s_enable(unsigned int id);

/* Disable I2S controller */
unsigned int i2s_disable(unsigned int id);

/* Enable the Tx engine on I2S controller IO */
unsigned int i2s_enable_tx(unsigned int id);

/* Disable the Tx engine on I2S controller IO */
unsigned int i2s_disable_tx(unsigned int id);

/* Enable the Rx engine on I2S controller IO */
unsigned int i2s_enable_rx(unsigned int id);

/* Disable the Rx engine on I2S controller IO */
unsigned int i2s_disable_rx(unsigned int id);

static inline void
updatel(volatile void __iomem *addr, unsigned int mask, unsigned int val)
{
	unsigned int prev_val, new_val;

	prev_val = readl(addr);
	new_val = (prev_val & (~mask)) | (val & mask);

	writel(new_val, addr);
}

/* Enable/Disable digital loopback with I2S controller */
unsigned int i2s_set_loopback(unsigned int id, unsigned int enable);

#endif /* _TEGRA_I2S_H_ */
