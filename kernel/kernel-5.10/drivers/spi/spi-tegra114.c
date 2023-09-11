// SPDX-License-Identifier: GPL-2.0-only
/*
 * SPI driver for NVIDIA's Tegra114 SPI Controller.
 *
 * Copyright (c) 2013-2023, NVIDIA CORPORATION.  All rights reserved.
 */

#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/reset.h>
#include <linux/spi/spi.h>
#include <linux/tegra_prod.h>

#define SPI_COMMAND1				0x000
#define SPI_BIT_LENGTH(x)			(((x) & 0x1f) << 0)
#define SPI_PACKED				(1 << 5)
#define SPI_TX_EN				(1 << 11)
#define SPI_RX_EN				(1 << 12)
#define SPI_BOTH_EN_BYTE			(1 << 13)
#define SPI_BOTH_EN_BIT				(1 << 14)
#define SPI_LSBYTE_FE				(1 << 15)
#define SPI_LSBIT_FE				(1 << 16)
#define SPI_BIDIROE				(1 << 17)
#define SPI_IDLE_SDA_DRIVE_LOW			(0 << 18)
#define SPI_IDLE_SDA_DRIVE_HIGH			(1 << 18)
#define SPI_IDLE_SDA_PULL_LOW			(2 << 18)
#define SPI_IDLE_SDA_PULL_HIGH			(3 << 18)
#define SPI_IDLE_SDA_MASK			(3 << 18)
#define SPI_CS_SW_VAL				(1 << 20)
#define SPI_CS_SW_HW				(1 << 21)
#define SPI_CMD1_GR_MASK			0x7FFFA000
/* SPI_CS_POL_INACTIVE bits are default high */
						/* n from 0 to 3 */
#define SPI_CS_POL_INACTIVE(n)			(1 << (22 + (n)))
#define SPI_CS_POL_INACTIVE_MASK		(0xF << 22)

#define SPI_CS_SEL_0				(0 << 26)
#define SPI_CS_SEL_1				(1 << 26)
#define SPI_CS_SEL_2				(2 << 26)
#define SPI_CS_SEL_3				(3 << 26)
#define SPI_CS_SEL_MASK				(3 << 26)
#define SPI_CS_SEL(x)				(((x) & 0x3) << 26)
#define SPI_CONTROL_MODE_0			(0 << 28)
#define SPI_CONTROL_MODE_1			(1 << 28)
#define SPI_CONTROL_MODE_2			(2 << 28)
#define SPI_CONTROL_MODE_3			(3 << 28)
#define SPI_CONTROL_MODE_MASK			(3 << 28)
#define SPI_MODE_SEL(x)				(((x) & 0x3) << 28)
#define SPI_MODE_VAL(x)				(((x) >> 28) & 0x3)
#define SPI_M_S					(1 << 30)
#define SPI_PIO					(1 << 31)

#define SPI_COMMAND2				0x004
#define SPI_TX_TAP_DELAY(x)			(((x) & 0x3F) << 6)
#define SPI_RX_TAP_DELAY(x)			(((x) & 0x3F) << 0)

#define SPI_CS_TIMING1				0x008
#define SPI_SETUP_HOLD(setup, hold)		((setup << 4) | hold)
#define SPI_CS_SETUP_HOLD(reg, cs, val)			\
		((((val) & 0xFFu) << ((cs) * 8)) |	\
		((reg) & ~(0xFFu << ((cs) * 8))))

#define SPI_CS_TIMING2				0x00C
#define CYCLES_BETWEEN_PACKETS_0(x)		(((x) & 0x1F) << 0)
#define CS_ACTIVE_BETWEEN_PACKETS_0		(1 << 5)
#define CYCLES_BETWEEN_PACKETS_1(x)		(((x) & 0x1F) << 8)
#define CS_ACTIVE_BETWEEN_PACKETS_1		(1 << 13)
#define CYCLES_BETWEEN_PACKETS_2(x)		(((x) & 0x1F) << 16)
#define CS_ACTIVE_BETWEEN_PACKETS_2		(1 << 21)
#define CYCLES_BETWEEN_PACKETS_3(x)		(((x) & 0x1F) << 24)
#define CS_ACTIVE_BETWEEN_PACKETS_3		(1 << 29)
#define SPI_SET_CS_ACTIVE_BETWEEN_PACKETS(reg, cs, val)		\
		(reg = (((val) & 0x1) << ((cs) * 8 + 5)) |	\
			((reg) & ~(1 << ((cs) * 8 + 5))))
#define SPI_SET_CYCLES_BETWEEN_PACKETS(reg, cs, val)		\
		(reg = (((val) & 0x1F) << ((cs) * 8)) |		\
			((reg) & ~(0x1F << ((cs) * 8))))

#define SPI_TRANS_STATUS			0x010
#define SPI_BLK_CNT(val)			(((val) >> 0) & 0xFFFF)
#define SPI_SLV_IDLE_COUNT(val)			(((val) >> 16) & 0xFF)
#define SPI_RDY					(1 << 30)

#define SPI_FIFO_STATUS				0x014
#define SPI_RX_FIFO_EMPTY			(1 << 0)
#define SPI_RX_FIFO_FULL			(1 << 1)
#define SPI_TX_FIFO_EMPTY			(1 << 2)
#define SPI_TX_FIFO_FULL			(1 << 3)
#define SPI_RX_FIFO_UNF				(1 << 4)
#define SPI_RX_FIFO_OVF				(1 << 5)
#define SPI_TX_FIFO_UNF				(1 << 6)
#define SPI_TX_FIFO_OVF				(1 << 7)
#define SPI_ERR					(1 << 8)
#define SPI_TX_FIFO_FLUSH			(1 << 14)
#define SPI_RX_FIFO_FLUSH			(1 << 15)
#define SPI_TX_FIFO_EMPTY_COUNT(val)		(((val) >> 16) & 0x7F)
#define SPI_RX_FIFO_FULL_COUNT(val)		(((val) >> 23) & 0x7F)
#define SPI_FRAME_END				(1 << 30)
#define SPI_CS_INACTIVE				(1 << 31)

#define SPI_FIFO_ERROR				(SPI_RX_FIFO_UNF | \
			SPI_RX_FIFO_OVF | SPI_TX_FIFO_UNF | SPI_TX_FIFO_OVF)
#define SPI_FIFO_EMPTY			(SPI_RX_FIFO_EMPTY | SPI_TX_FIFO_EMPTY)

#define SPI_TX_DATA				0x018
#define SPI_RX_DATA				0x01C

#define SPI_DMA_CTL				0x020
#define SPI_TX_TRIG_1				(0 << 15)
#define SPI_TX_TRIG_4				(1 << 15)
#define SPI_TX_TRIG_8				(2 << 15)
#define SPI_TX_TRIG_16				(3 << 15)
#define SPI_TX_TRIG_MASK			(3 << 15)
#define SPI_RX_TRIG_1				(0 << 19)
#define SPI_RX_TRIG_4				(1 << 19)
#define SPI_RX_TRIG_8				(2 << 19)
#define SPI_RX_TRIG_16				(3 << 19)
#define SPI_RX_TRIG_MASK			(3 << 19)
#define SPI_IE_TX				(1 << 28)
#define SPI_IE_RX				(1 << 29)
#define SPI_CONT				(1 << 30)
#define SPI_DMA					(1 << 31)
#define SPI_DMA_EN				SPI_DMA

#define SPI_DMA_BLK				0x024
#define SPI_DMA_BLK_SET(x)			(((x) & 0xFFFF) << 0)

#define SPI_TX_FIFO				0x108
#define SPI_RX_FIFO				0x188
#define SPI_INTR_MASK				0x18c
#define SPI_INTR_RX_FIFO_UNF_MASK		BIT(25)
#define SPI_INTR_RX_FIFO_OVF_MASK		BIT(26)
#define SPI_INTR_TX_FIFO_UNF_MASK		BIT(27)
#define SPI_INTR_TX_FIFO_OVF_MASK		BIT(28)
#define SPI_INTR_RDY_MASK			BIT(29)
#define SPI_INTR_ALL_MASK			(0x1fUL << 25)
#define MAX_CHIP_SELECT				4
#define SPI_FIFO_DEPTH				64
#define DATA_DIR_TX				(1 << 0)
#define DATA_DIR_RX				(1 << 1)

#define SPI_DMA_TIMEOUT				(msecs_to_jiffies(10000))
#define DEFAULT_SPI_DMA_BUF_LEN			(16*1024)
#define TX_FIFO_EMPTY_COUNT_MAX			SPI_TX_FIFO_EMPTY_COUNT(0x40)
#define RX_FIFO_FULL_COUNT_ZERO			SPI_RX_FIFO_FULL_COUNT(0)
#define MAX_HOLD_CYCLES				16
#define SPI_DEFAULT_SPEED			25000000
#define SPI_SPEED_TAP_DELAY_MARGIN		35000000
#define SPI_POLL_TIMEOUT			10000
#define SPI_DEFAULT_RX_TAP_DELAY		10
#define SPI_DEFAULT_TX_TAP_DELAY		0
#define SPI_FIFO_FLUSH_MAX_DELAY		2000


#define SPI_FATAL_INTR_EN_0				0x198
#define SPI_RX_FIFO_UNF_FATAL_INTR_EN		BIT(25)
#define SPI_RX_FIFO_OVF_FATAL_INTR_EN		BIT(26)
#define SPI_TX_FIFO_UNF_FATAL_INTR_EN		BIT(27)
#define SPI_TX_FIFO_OVF_FATAL_INTR_EN		BIT(28)
#define SPI_FATAL_INTR_ALL_EN_0			(0x1fUL << 25)

struct tegra_spi_soc_data {
	bool has_intr_mask_reg;
	bool set_rx_tap_delay;
	bool has_fatal_intr_en_reg;
};

static bool prefer_last_used_cs;
module_param_named(prefer_last_used_cs, prefer_last_used_cs, bool, 0644);
MODULE_PARM_DESC(prefer_last_used_cs,
		 "Skip default CS command update at end of each transaction");

struct tegra_spi_client_ctl_state {
	bool cs_gpio_valid;
};

struct tegra_spi_client_data {
	bool is_hw_based_cs;
	int cs_setup_clk_count;
	int cs_hold_clk_count;
	int tx_clk_tap_delay;
	int rx_clk_tap_delay;
	int cs_inactive_cycles;
	int clk_delay_between_packets;
};

struct tegra_spi_data {
	struct device				*dev;
	struct spi_controller			*ctrl;
	spinlock_t				lock;

	struct clk				*clk;
	struct reset_control			*rst;
	void __iomem				*base;
	phys_addr_t				phys;
	unsigned				irq;
	bool					clock_always_on;
	bool					polling_mode;
	u32					cur_speed;
	unsigned				min_div;

	struct spi_device			*cur_spi;
	struct spi_device			*cs_control;
	unsigned				cur_pos;
	unsigned				words_per_32bit;
	unsigned				bytes_per_word;
	unsigned				curr_dma_words;
	unsigned				cur_direction;

	unsigned				cur_rx_pos;
	unsigned				cur_tx_pos;

	unsigned				dma_buf_size;
	unsigned				max_buf_size;
	bool					is_hw_based_cs;
	bool					is_curr_dma_xfer;

	struct completion			rx_dma_complete;
	struct completion			tx_dma_complete;

	u32					tx_status;
	u32					rx_status;
	u32					status_reg;
	bool					is_packed;

	u32					command1_reg;
	u32					command2_reg;
	u32					dma_control_reg;
	u32					def_command1_reg;
	u32					spi_cs_timing;
	u32					spi_cs_timing2;
	u32					spi_cs_timing1;
	u8					last_used_cs;
	u8					def_chip_select;

	struct completion			xfer_completion;
	struct spi_transfer			*curr_xfer;
	struct dma_chan				*rx_dma_chan;
	u32					*rx_dma_buf;
	dma_addr_t				rx_dma_phys;
	struct dma_async_tx_descriptor		*rx_dma_desc;

	struct dma_chan				*tx_dma_chan;
	u32					*tx_dma_buf;
	dma_addr_t				tx_dma_phys;
	struct dma_async_tx_descriptor		*tx_dma_desc;
	const struct tegra_spi_soc_data		*soc_data;
	struct tegra_prod			*prod_list;
};

static int tegra_spi_runtime_suspend(struct device *dev);
static int tegra_spi_runtime_resume(struct device *dev);
static int tegra_spi_status_poll(struct tegra_spi_data *tspi);

static inline u32 tegra_spi_readl(struct tegra_spi_data *tspi,
		unsigned long reg)
{
	return readl(tspi->base + reg);
}

static inline void tegra_spi_writel(struct tegra_spi_data *tspi,
		u32 val, unsigned long reg)
{
	/* Read back register to make sure that register writes completed */
	if ((reg == SPI_COMMAND1) && (val & SPI_PIO))
		readl(tspi->base + SPI_COMMAND1);

	writel(val, tspi->base + reg);
}

static void tegra_spi_set_intr_mask(struct tegra_spi_data *tspi)
{
	unsigned long intr_mask;

	/* Interrupts are disabled by default and need not be cleared
	 * in polling mode. Still writing to registers to be robust
	 * This step occurs only in case of system reset or
	 * resume or error case and not in data path affecting perf.
	 */

	if (tspi->soc_data->has_intr_mask_reg) {
		intr_mask = tegra_spi_readl(tspi, SPI_INTR_MASK);
		if (tspi->polling_mode)
			intr_mask |= SPI_INTR_ALL_MASK;
		else
			intr_mask &= ~(SPI_INTR_ALL_MASK);
		tegra_spi_writel(tspi, intr_mask, SPI_INTR_MASK);
	} else {
		intr_mask = tegra_spi_readl(tspi, SPI_DMA_CTL);
		if (tspi->polling_mode)
			intr_mask |= SPI_IE_TX | SPI_IE_RX;
		else
			intr_mask &= ~(SPI_IE_TX | SPI_IE_RX);
		tegra_spi_writel(tspi, intr_mask, SPI_DMA_CTL);
	}
}

/* Enable fatal interrupt. This interrupt only indicates
 * existing interrupts are fatal and does not add any new
 * intr flags. This interrupt gets asserted when corresponding
 * fatal_intr_en is set in SPI_FATAL_INTR_EN_0 register and the
 * error occurs.
 */
static void tegra_spi_set_fatal_intr_en(struct tegra_spi_data *tspi)
{
	unsigned long intr_enable;

	if (tspi->soc_data->has_fatal_intr_en_reg) {
		intr_enable = tegra_spi_readl(tspi, SPI_FATAL_INTR_EN_0);
		if (tspi->polling_mode)
			intr_enable &= ~(SPI_FATAL_INTR_ALL_EN_0);
		else
			intr_enable |= SPI_FATAL_INTR_ALL_EN_0;
		tegra_spi_writel(tspi, intr_enable, SPI_FATAL_INTR_EN_0);
	}
}

static void tegra_spi_clear_status(struct tegra_spi_data *tspi)
{
	u32 val;

	/* Write 1 to clear status register */
	val = tegra_spi_readl(tspi, SPI_TRANS_STATUS);
	tegra_spi_writel(tspi, val, SPI_TRANS_STATUS);

	/* Clear fifo status error if any */
	if (tspi->status_reg & SPI_ERR) {
		tegra_spi_writel(tspi, SPI_ERR | SPI_FIFO_ERROR,
				 SPI_FIFO_STATUS);
		tspi->status_reg = tegra_spi_readl(tspi, SPI_FIFO_STATUS);
	}
}

static unsigned tegra_spi_calculate_curr_xfer_param(
	struct spi_device *spi, struct tegra_spi_data *tspi,
	struct spi_transfer *t)
{
	unsigned remain_len = t->len - tspi->cur_pos;
	unsigned max_word;
	unsigned bits_per_word = t->bits_per_word;
	unsigned max_len;
	unsigned total_fifo_words;

	tspi->bytes_per_word = DIV_ROUND_UP(bits_per_word, 8);

	if ((bits_per_word == 8 || bits_per_word == 16 ||
	     bits_per_word == 32) && t->len > 3) {
		tspi->is_packed = true;
		tspi->words_per_32bit = 32/bits_per_word;
	} else {
		tspi->is_packed = false;
		tspi->words_per_32bit = 1;
	}

	if (tspi->is_packed) {
		max_len = min(remain_len, tspi->max_buf_size);
		tspi->curr_dma_words = max_len/tspi->bytes_per_word;
		total_fifo_words = (max_len + 3) / 4;
	} else {
		max_word = (remain_len - 1) / tspi->bytes_per_word + 1;
		max_word = min(max_word, tspi->max_buf_size/4);
		tspi->curr_dma_words = max_word;
		total_fifo_words = max_word;
	}
	return total_fifo_words;
}

static unsigned tegra_spi_fill_tx_fifo_from_client_txbuf(
	struct tegra_spi_data *tspi, struct spi_transfer *t)
{
	unsigned nbytes;
	unsigned tx_empty_count;
	u32 fifo_status;
	unsigned max_n_32bit;
	unsigned i, count;
	unsigned int written_words;
	unsigned fifo_words_left;
	u8 *tx_buf = (u8 *)t->tx_buf + tspi->cur_tx_pos;

	fifo_status = tspi->status_reg;
	tx_empty_count = SPI_TX_FIFO_EMPTY_COUNT(fifo_status);

	if (tspi->is_packed) {
		fifo_words_left = tx_empty_count * tspi->words_per_32bit;
		written_words = min(fifo_words_left, tspi->curr_dma_words);
		nbytes = written_words * tspi->bytes_per_word;
		max_n_32bit = DIV_ROUND_UP(nbytes, 4);
		for (count = 0; count < max_n_32bit; count++) {
			u32 x = 0;

			for (i = 0; (i < 4) && nbytes; i++, nbytes--)
				x |= (u32)(*tx_buf++) << (i * 8);
			tegra_spi_writel(tspi, x, SPI_TX_FIFO);
		}

		tspi->cur_tx_pos += written_words * tspi->bytes_per_word;
	} else {
		unsigned int write_bytes;
		max_n_32bit = min(tspi->curr_dma_words,  tx_empty_count);
		written_words = max_n_32bit;
		nbytes = written_words * tspi->bytes_per_word;
		if (nbytes > t->len - tspi->cur_pos)
			nbytes = t->len - tspi->cur_pos;
		write_bytes = nbytes;
		for (count = 0; count < max_n_32bit; count++) {
			u32 x = 0;

			for (i = 0; nbytes && (i < tspi->bytes_per_word);
							i++, nbytes--)
				x |= (u32)(*tx_buf++) << (i * 8);
			tegra_spi_writel(tspi, x, SPI_TX_FIFO);
		}

		tspi->cur_tx_pos += write_bytes;
	}

	return written_words;
}

static unsigned int tegra_spi_read_rx_fifo_to_client_rxbuf(
		struct tegra_spi_data *tspi, struct spi_transfer *t)
{
	unsigned rx_full_count;
	u32 fifo_status;
	unsigned i, count;
	unsigned int read_words = 0;
	unsigned len;
	u8 *rx_buf = (u8 *)t->rx_buf + tspi->cur_rx_pos;

	fifo_status = tspi->status_reg;
	rx_full_count = SPI_RX_FIFO_FULL_COUNT(fifo_status);
	if (tspi->is_packed) {
		len = tspi->curr_dma_words * tspi->bytes_per_word;
		for (count = 0; count < rx_full_count; count++) {
			u32 x = tegra_spi_readl(tspi, SPI_RX_FIFO);

			for (i = 0; len && (i < 4); i++, len--)
				*rx_buf++ = (x >> i*8) & 0xFF;
		}
		read_words += tspi->curr_dma_words;
		tspi->cur_rx_pos += tspi->curr_dma_words * tspi->bytes_per_word;
	} else {
		u32 rx_mask = ((u32)1 << t->bits_per_word) - 1;
		u8 bytes_per_word = tspi->bytes_per_word;
		unsigned int read_bytes;

		len = rx_full_count * bytes_per_word;
		if (len > t->len - tspi->cur_pos)
			len = t->len - tspi->cur_pos;
		read_bytes = len;
		for (count = 0; count < rx_full_count; count++) {
			u32 x = tegra_spi_readl(tspi, SPI_RX_FIFO) & rx_mask;

			for (i = 0; len && (i < bytes_per_word); i++, len--)
				*rx_buf++ = (x >> (i*8)) & 0xFF;
		}
		read_words += rx_full_count;
		tspi->cur_rx_pos += read_bytes;
	}

	return read_words;
}

static void tegra_spi_copy_client_txbuf_to_spi_txbuf(
		struct tegra_spi_data *tspi, struct spi_transfer *t)
{
	/* Make the dma buffer to read by cpu */
	dma_sync_single_for_cpu(tspi->dev, tspi->tx_dma_phys,
				tspi->dma_buf_size, DMA_TO_DEVICE);

	if (tspi->is_packed) {
		unsigned len = tspi->curr_dma_words * tspi->bytes_per_word;

		memcpy(tspi->tx_dma_buf, t->tx_buf + tspi->cur_pos, len);
		tspi->cur_tx_pos += tspi->curr_dma_words * tspi->bytes_per_word;
	} else {
		unsigned int i;
		unsigned int count;
		u8 *tx_buf = (u8 *)t->tx_buf + tspi->cur_tx_pos;
		unsigned consume = tspi->curr_dma_words * tspi->bytes_per_word;
		unsigned int write_bytes;

		if (consume > t->len - tspi->cur_pos)
			consume = t->len - tspi->cur_pos;
		write_bytes = consume;
		for (count = 0; count < tspi->curr_dma_words; count++) {
			u32 x = 0;

			for (i = 0; consume && (i < tspi->bytes_per_word);
							i++, consume--)
				x |= (u32)(*tx_buf++) << (i * 8);
			tspi->tx_dma_buf[count] = x;
		}

		tspi->cur_tx_pos += write_bytes;
	}

	/* Make the dma buffer to read by dma */
	dma_sync_single_for_device(tspi->dev, tspi->tx_dma_phys,
				tspi->dma_buf_size, DMA_TO_DEVICE);
}

static void tegra_spi_copy_spi_rxbuf_to_client_rxbuf(
		struct tegra_spi_data *tspi, struct spi_transfer *t)
{
	/* Make the dma buffer to read by cpu */
	dma_sync_single_for_cpu(tspi->dev, tspi->rx_dma_phys,
		tspi->dma_buf_size, DMA_FROM_DEVICE);

	if (tspi->is_packed) {
		unsigned len = tspi->curr_dma_words * tspi->bytes_per_word;

		memcpy(t->rx_buf + tspi->cur_rx_pos, tspi->rx_dma_buf, len);
		tspi->cur_rx_pos += tspi->curr_dma_words * tspi->bytes_per_word;
	} else {
		unsigned int i;
		unsigned int count;
		unsigned char *rx_buf = t->rx_buf + tspi->cur_rx_pos;
		u32 rx_mask = ((u32)1 << t->bits_per_word) - 1;
		unsigned consume = tspi->curr_dma_words * tspi->bytes_per_word;
		unsigned int read_bytes;

		if (consume > t->len - tspi->cur_pos)
			consume = t->len - tspi->cur_pos;
		read_bytes = consume;
		for (count = 0; count < tspi->curr_dma_words; count++) {
			u32 x = tspi->rx_dma_buf[count] & rx_mask;

			for (i = 0; consume && (i < tspi->bytes_per_word);
							i++, consume--)
				*rx_buf++ = (x >> (i*8)) & 0xFF;
		}

		tspi->cur_rx_pos += read_bytes;
	}

	/* Make the dma buffer to read by dma */
	dma_sync_single_for_device(tspi->dev, tspi->rx_dma_phys,
		tspi->dma_buf_size, DMA_FROM_DEVICE);
}

static void tegra_spi_dma_complete(void *args)
{
	struct completion *dma_complete = args;

	complete(dma_complete);
}

static int tegra_spi_start_tx_dma(struct tegra_spi_data *tspi, int len)
{
	reinit_completion(&tspi->tx_dma_complete);
	tspi->tx_dma_desc = dmaengine_prep_slave_single(tspi->tx_dma_chan,
				tspi->tx_dma_phys, len, DMA_MEM_TO_DEV,
				DMA_PREP_INTERRUPT |  DMA_CTRL_ACK);
	if (!tspi->tx_dma_desc) {
		dev_err(tspi->dev, "Not able to get desc for Tx\n");
		return -EIO;
	}

	tspi->tx_dma_desc->callback = tegra_spi_dma_complete;
	tspi->tx_dma_desc->callback_param = &tspi->tx_dma_complete;

	dmaengine_submit(tspi->tx_dma_desc);
	dma_async_issue_pending(tspi->tx_dma_chan);
	return 0;
}

static int tegra_spi_start_rx_dma(struct tegra_spi_data *tspi, int len)
{
	reinit_completion(&tspi->rx_dma_complete);
	tspi->rx_dma_desc = dmaengine_prep_slave_single(tspi->rx_dma_chan,
				tspi->rx_dma_phys, len, DMA_DEV_TO_MEM,
				DMA_PREP_INTERRUPT |  DMA_CTRL_ACK);
	if (!tspi->rx_dma_desc) {
		dev_err(tspi->dev, "Not able to get desc for Rx\n");
		return -EIO;
	}

	tspi->rx_dma_desc->callback = tegra_spi_dma_complete;
	tspi->rx_dma_desc->callback_param = &tspi->rx_dma_complete;

	dmaengine_submit(tspi->rx_dma_desc);
	dma_async_issue_pending(tspi->rx_dma_chan);
	return 0;
}

static int tegra_spi_clear_fifo(struct tegra_spi_data *tspi)
{
	unsigned long status;
	int cnt = SPI_FIFO_FLUSH_MAX_DELAY;

	/* Make sure that Rx and Tx fifo are empty */
	status = tspi->status_reg;
	if ((status & SPI_FIFO_EMPTY) != SPI_FIFO_EMPTY) {
		/* flush the fifo */
		status |= (SPI_RX_FIFO_FLUSH | SPI_TX_FIFO_FLUSH);
		tegra_spi_writel(tspi, status, SPI_FIFO_STATUS);
		do {
			status = tegra_spi_readl(tspi, SPI_FIFO_STATUS);
			if ((status & SPI_FIFO_EMPTY) == SPI_FIFO_EMPTY) {
				tspi->status_reg = status;
				return 0;
			}
			udelay(1);
		} while (cnt--);
		dev_err(tspi->dev,
			"Rx/Tx fifo are not empty status 0x%08lx\n", status);
		return -EIO;
	}
	return 0;
}

static int tegra_spi_start_dma_based_transfer(
		struct tegra_spi_data *tspi, struct spi_transfer *t)
{
	u32 val;
	unsigned int len;
	int ret = 0;
	u8 dma_burst;
	struct dma_slave_config dma_sconfig = {0};

	/* Make sure that Rx and Tx fifo are empty */
	ret = tegra_spi_clear_fifo(tspi);
	if (ret != 0)
		return ret;

	val = SPI_DMA_BLK_SET(tspi->curr_dma_words - 1);
	tegra_spi_writel(tspi, val, SPI_DMA_BLK);

	if (tspi->is_packed)
		len = DIV_ROUND_UP(tspi->curr_dma_words * tspi->bytes_per_word,
					4) * 4;
	else
		len = tspi->curr_dma_words * 4;

	/* Set attention level based on length of transfer */
	if (len & 0xF) {
		val |= SPI_TX_TRIG_1 | SPI_RX_TRIG_1;
		dma_burst = 1;
	} else if (((len) >> 4) & 0x1) {
		val |= SPI_TX_TRIG_4 | SPI_RX_TRIG_4;
		dma_burst = 4;
	} else if (((len) >> 5) & 0x1) {
		val |= SPI_TX_TRIG_8 | SPI_RX_TRIG_8;
		dma_burst = 8;
	} else {
		val |= SPI_TX_TRIG_16 | SPI_RX_TRIG_16;
		dma_burst = 16;
	}

	if (!tspi->soc_data->has_intr_mask_reg &&
	    !tspi->polling_mode) {
		if (tspi->cur_direction & DATA_DIR_TX)
			val |= SPI_IE_TX;

		if (tspi->cur_direction & DATA_DIR_RX)
			val |= SPI_IE_RX;
	}

	tegra_spi_writel(tspi, val, SPI_DMA_CTL);
	tspi->dma_control_reg = val;

	dma_sconfig.device_fc = true;
	if (tspi->cur_direction & DATA_DIR_TX) {
		dma_sconfig.dst_addr = tspi->phys + SPI_TX_FIFO;
		dma_sconfig.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		dma_sconfig.dst_maxburst = dma_burst;
		ret = dmaengine_slave_config(tspi->tx_dma_chan, &dma_sconfig);
		if (ret < 0) {
			dev_err(tspi->dev,
				"DMA slave config failed: %d\n", ret);
			return ret;
		}

		tegra_spi_copy_client_txbuf_to_spi_txbuf(tspi, t);
		ret = tegra_spi_start_tx_dma(tspi, len);
		if (ret < 0) {
			dev_err(tspi->dev,
				"Starting tx dma failed, err %d\n", ret);
			return ret;
		}
	}

	if (tspi->cur_direction & DATA_DIR_RX) {
		dma_sconfig.src_addr = tspi->phys + SPI_RX_FIFO;
		dma_sconfig.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		dma_sconfig.src_maxburst = dma_burst;
		ret = dmaengine_slave_config(tspi->rx_dma_chan, &dma_sconfig);
		if (ret < 0) {
			dev_err(tspi->dev,
				"DMA slave config failed: %d\n", ret);
			return ret;
		}

		/* Make the dma buffer to read by dma */
		dma_sync_single_for_device(tspi->dev, tspi->rx_dma_phys,
				tspi->dma_buf_size, DMA_FROM_DEVICE);

		ret = tegra_spi_start_rx_dma(tspi, len);
		if (ret < 0) {
			dev_err(tspi->dev,
				"Starting rx dma failed, err %d\n", ret);
			if (tspi->cur_direction & DATA_DIR_TX)
				dmaengine_terminate_all(tspi->tx_dma_chan);
			return ret;
		}
	}
	tspi->is_curr_dma_xfer = true;
	tspi->dma_control_reg = val;

	val |= SPI_DMA_EN;
	tegra_spi_writel(tspi, val, SPI_DMA_CTL);
	return ret;
}

static int tegra_spi_start_cpu_based_transfer(
		struct tegra_spi_data *tspi, struct spi_transfer *t)
{
	u32 val;
	unsigned cur_words;
	int ret;

	ret = tegra_spi_clear_fifo(tspi);
	if (ret != 0)
		return ret;

	if (tspi->cur_direction & DATA_DIR_TX)
		cur_words = tegra_spi_fill_tx_fifo_from_client_txbuf(tspi, t);
	else
		cur_words = tspi->curr_dma_words;

	val = SPI_DMA_BLK_SET(cur_words - 1);
	tegra_spi_writel(tspi, val, SPI_DMA_BLK);

	val = 0;
	if (!tspi->soc_data->has_intr_mask_reg &&
	    !tspi->polling_mode) {
		if (tspi->cur_direction & DATA_DIR_TX)
			val |= SPI_IE_TX;
		if (tspi->cur_direction & DATA_DIR_RX)
			val |= SPI_IE_RX;
		tegra_spi_writel(tspi, val, SPI_DMA_CTL);
	}
	tspi->dma_control_reg = val;

	tspi->is_curr_dma_xfer = false;

	val = tspi->command1_reg;
	val |= SPI_PIO;
	tegra_spi_writel(tspi, val, SPI_COMMAND1);
	return 0;
}

static int tegra_spi_init_dma_param(struct tegra_spi_data *tspi,
			bool dma_to_memory)
{
	struct dma_chan *dma_chan;
	u32 *dma_buf;
	dma_addr_t dma_phys;

	dma_chan = dma_request_chan(tspi->dev, dma_to_memory ? "rx" : "tx");
	if (IS_ERR(dma_chan))
		return dev_err_probe(tspi->dev, PTR_ERR(dma_chan),
				     "Dma channel is not available\n");

	dma_buf = dma_alloc_coherent(tspi->dev, tspi->dma_buf_size,
				&dma_phys, GFP_KERNEL);
	if (!dma_buf) {
		dev_err(tspi->dev, " Not able to allocate the dma buffer\n");
		dma_release_channel(dma_chan);
		return -ENOMEM;
	}

	if (dma_to_memory) {
		tspi->rx_dma_chan = dma_chan;
		tspi->rx_dma_buf = dma_buf;
		tspi->rx_dma_phys = dma_phys;
	} else {
		tspi->tx_dma_chan = dma_chan;
		tspi->tx_dma_buf = dma_buf;
		tspi->tx_dma_phys = dma_phys;
	}
	return 0;
}

static void tegra_spi_deinit_dma_param(struct tegra_spi_data *tspi,
	bool dma_to_memory)
{
	u32 *dma_buf;
	dma_addr_t dma_phys;
	struct dma_chan *dma_chan;

	if (dma_to_memory) {
		dma_buf = tspi->rx_dma_buf;
		dma_chan = tspi->rx_dma_chan;
		dma_phys = tspi->rx_dma_phys;
		tspi->rx_dma_chan = NULL;
		tspi->rx_dma_buf = NULL;
	} else {
		dma_buf = tspi->tx_dma_buf;
		dma_chan = tspi->tx_dma_chan;
		dma_phys = tspi->tx_dma_phys;
		tspi->tx_dma_buf = NULL;
		tspi->tx_dma_chan = NULL;
	}
	if (!dma_chan)
		return;

	dma_free_coherent(tspi->dev, tspi->dma_buf_size, dma_buf, dma_phys);
	dma_release_channel(dma_chan);
}

static void tegra_spi_set_prod(struct tegra_spi_data *tspi, int cs)
{
	int ret;
	char prod_name[15];

	/* Avoid write to register for transfers to last used device */
	if (tspi->last_used_cs == cs)
		return;

	ret = tegra_prod_set_by_name(&tspi->base, "prod", tspi->prod_list);
	sprintf(prod_name, "prod_c_cs%d", cs);
	ret = tegra_prod_set_by_name(&tspi->base, prod_name, tspi->prod_list);
	if (ret)
		dev_dbg(tspi->dev, "prod settings failed with error %d", ret);
	tspi->last_used_cs = cs;
}

static void tegra_spi_set_cmd2(struct spi_device *spi, u32 speed)
{
	struct tegra_spi_data *tspi = spi_controller_get_devdata(spi->master);
	struct tegra_spi_client_data *cdata = spi->controller_data;
	u32 command2_reg = 0;
	u32 tx_tap = 0;
	u32 rx_tap = 0;

	/* Avoid write to register for transfers to last used device */
	if (tspi->last_used_cs == spi->chip_select)
		return;

	if (!cdata || tspi->prod_list)
		return;

	if (cdata && cdata->rx_clk_tap_delay)
		rx_tap = cdata->rx_clk_tap_delay;
	else if (speed > SPI_SPEED_TAP_DELAY_MARGIN)
		rx_tap = SPI_DEFAULT_RX_TAP_DELAY;

	if (cdata && cdata->tx_clk_tap_delay)
		tx_tap = cdata->tx_clk_tap_delay;
	else
		tx_tap = SPI_DEFAULT_TX_TAP_DELAY;

	command2_reg = SPI_TX_TAP_DELAY(tx_tap) |
		       SPI_RX_TAP_DELAY(rx_tap);

	if (tspi->soc_data->set_rx_tap_delay)
		if (command2_reg != tspi->command2_reg)
			tegra_spi_writel(tspi, command2_reg,
					 SPI_COMMAND2);
	tspi->last_used_cs = spi->chip_select;
}

static void tegra_spi_set_timing1(struct spi_device *spi)
{
	struct tegra_spi_data *tspi = spi_controller_get_devdata(spi->master);
	struct tegra_spi_client_data *cdata = spi->controller_data;
	u32 set_count;
	u32 hold_count;
	u32 spi_cs_timing;
	u32 spi_cs_setup;

	if (!cdata || tspi->prod_list)
		return;
	set_count = min(cdata->cs_setup_clk_count, 16);
	if (set_count)
		set_count--;

	hold_count = min(cdata->cs_hold_clk_count, 16);
	if (hold_count)
		hold_count--;

	spi_cs_setup = SPI_SETUP_HOLD(set_count, hold_count);
	spi_cs_timing = SPI_CS_SETUP_HOLD(tspi->spi_cs_timing,
					  spi->chip_select,
					  spi_cs_setup);
	if (tspi->spi_cs_timing != spi_cs_timing) {
		tspi->spi_cs_timing = spi_cs_timing;
		tegra_spi_writel(tspi, spi_cs_timing, SPI_CS_TIMING1);
	}
}

static void tegra_spi_set_timing2(struct spi_device *spi)
{
	struct tegra_spi_data *tspi = spi_controller_get_devdata(spi->master);
	struct tegra_spi_client_data *cdata = spi->controller_data;
	u32 spi_cs_timing2 = 0;

	if (!cdata || tspi->prod_list)
		return;
	if (!cdata->clk_delay_between_packets)
		return;
	if (cdata->cs_inactive_cycles) {
		u32 inactive_cycles;

		SPI_SET_CS_ACTIVE_BETWEEN_PACKETS(spi_cs_timing2,
						  spi->chip_select,
						  0);
		inactive_cycles = min(cdata->cs_inactive_cycles, 32);
		SPI_SET_CYCLES_BETWEEN_PACKETS(spi_cs_timing2,
					       spi->chip_select,
					       inactive_cycles);
		if (tspi->spi_cs_timing2 != spi_cs_timing2) {
			tspi->spi_cs_timing2 = spi_cs_timing2;
			tegra_spi_writel(tspi, spi_cs_timing2,
					 SPI_CS_TIMING2);
		}
		tspi->is_hw_based_cs = true;
	} else {
		SPI_SET_CS_ACTIVE_BETWEEN_PACKETS(spi_cs_timing2,
						  spi->chip_select, 1);
		SPI_SET_CYCLES_BETWEEN_PACKETS(spi_cs_timing2,
					       spi->chip_select, 0);
		if (tspi->spi_cs_timing2 != spi_cs_timing2) {
			tspi->spi_cs_timing2 = spi_cs_timing2;
			tegra_spi_writel(tspi, spi_cs_timing2,
					 SPI_CS_TIMING2);
		}
	}
}

static void set_best_clk_source(struct tegra_spi_data *tspi,
				unsigned long rate)
{
	long new_rate;
	unsigned long err_rate, crate, prate;
	unsigned int cdiv, fin_err = rate;
	int ret;
	struct clk *pclk, *fpclk = NULL;
	const char *pclk_name, *fpclk_name = NULL;
	struct device_node *node;
	struct property *prop;

	node = tspi->ctrl->dev.of_node;
	if (!of_property_count_strings(node, "nvidia,clk-parents"))
		return;

	/* when parent of a clk changes divider is not changed
	 * set a min div with which clk will not cross max rate
	 */
	if (!tspi->min_div) {
		of_property_for_each_string(node, "nvidia,clk-parents",
					    prop, pclk_name) {
			pclk = clk_get(tspi->dev, pclk_name);
			if (IS_ERR(pclk))
				continue;
			prate = clk_get_rate(pclk);
			crate = tspi->ctrl->max_speed_hz;
			cdiv = DIV_ROUND_UP(prate, crate);
			if (cdiv > tspi->min_div)
				tspi->min_div = cdiv;
		}
	}

	pclk = clk_get_parent(tspi->clk);
	crate = clk_get_rate(tspi->clk);
	prate = clk_get_rate(pclk);
	if (crate) {
		cdiv = DIV_ROUND_UP(prate, crate);
		if (cdiv < tspi->min_div) {
			crate = DIV_ROUND_UP(prate, tspi->min_div);
			clk_set_rate(tspi->clk, crate);
		}
	}

	of_property_for_each_string(node, "nvidia,clk-parents",
				    prop, pclk_name) {
		pclk = clk_get(tspi->dev, pclk_name);
		if (IS_ERR(pclk))
			continue;

		ret = clk_set_parent(tspi->clk, pclk);
		if (ret < 0)
			continue;

		new_rate = clk_round_rate(tspi->clk, rate);
		if (new_rate < 0)
			continue;

		err_rate = abs(new_rate - rate);
		if (err_rate < fin_err) {
			fpclk = pclk;
			fin_err = err_rate;
			fpclk_name = pclk_name;
		}
	}

	if (fpclk) {
		dev_dbg(tspi->dev, "Setting clk_src %s\n",
			fpclk_name);
		clk_set_parent(tspi->clk, fpclk);
	}
}

static int tegra_spi_set_clock_rate(struct tegra_spi_data *tspi, u32 speed)
{
	int ret;

	if (speed == tspi->cur_speed)
		return 0;
	set_best_clk_source(tspi, speed);
	ret = clk_set_rate(tspi->clk, speed);
	if (ret) {
		dev_err(tspi->dev, "Failed to set clk freq %d\n", ret);
		return -EINVAL;
	}
	tspi->cur_speed = speed;

	return 0;
}

static u32 tegra_spi_setup_transfer_one(struct spi_device *spi,
					struct spi_transfer *t,
					bool is_first_of_msg,
					bool is_single_xfer)
{
	struct tegra_spi_data *tspi = spi_controller_get_devdata(spi->master);
	struct tegra_spi_client_data *cdata = spi->controller_data;
	struct tegra_spi_client_ctl_state *cstate = spi->controller_state;
	u32 speed = t->speed_hz;
	u8 bits_per_word = t->bits_per_word;
	u32 command1;
	int req_mode;
	int ret;

	ret = tegra_spi_set_clock_rate(tspi, speed);
	if (ret < 0)
		return ret;

	tspi->cur_spi = spi;
	tspi->cur_pos = 0;
	tspi->cur_rx_pos = 0;
	tspi->cur_tx_pos = 0;
	tspi->curr_xfer = t;

	if (is_first_of_msg) {
		tspi->status_reg = tegra_spi_readl(tspi, SPI_FIFO_STATUS);
		tegra_spi_clear_status(tspi);

		command1 = tspi->def_command1_reg;
		command1 |= SPI_BIT_LENGTH(bits_per_word - 1);

		command1 &= ~SPI_CONTROL_MODE_MASK;
		req_mode = spi->mode & 0x3;
		if (req_mode == SPI_MODE_0)
			command1 |= SPI_CONTROL_MODE_0;
		else if (req_mode == SPI_MODE_1)
			command1 |= SPI_CONTROL_MODE_1;
		else if (req_mode == SPI_MODE_2)
			command1 |= SPI_CONTROL_MODE_2;
		else if (req_mode == SPI_MODE_3)
			command1 |= SPI_CONTROL_MODE_3;

		if (spi->mode & SPI_LSB_FIRST)
			command1 |= SPI_LSBIT_FE;
		else
			command1 &= ~SPI_LSBIT_FE;

		if (spi->mode & SPI_3WIRE)
			command1 |= SPI_BIDIROE;
		else
			command1 &= ~SPI_BIDIROE;

		if (tspi->cs_control) {
			if (tspi->cs_control != spi)
				tegra_spi_writel(tspi, command1, SPI_COMMAND1);
			tspi->cs_control = NULL;
		} else
			if (SPI_MODE_VAL(command1) !=
			    SPI_MODE_VAL(tspi->def_command1_reg))
				tegra_spi_writel(tspi, command1, SPI_COMMAND1);

		tspi->is_hw_based_cs = false;
		if (cdata && cdata->is_hw_based_cs && is_single_xfer &&
		    ((tspi->curr_dma_words * tspi->bytes_per_word) ==
		     (t->len - tspi->cur_pos))) {
			tegra_spi_set_timing1(spi);
			tspi->is_hw_based_cs = true;
		}

		tegra_spi_set_timing2(spi);

		if (!tspi->is_hw_based_cs) {
			command1 |= SPI_CS_SW_HW;
			if (spi->mode & SPI_CS_HIGH)
				command1 |= SPI_CS_SW_VAL;
			else
				command1 &= ~SPI_CS_SW_VAL;
		} else {
			command1 &= ~SPI_CS_SW_HW;
			command1 &= ~SPI_CS_SW_VAL;
		}

		if (cstate && cstate->cs_gpio_valid) {
			int gval = 0;

			if (spi->mode & SPI_CS_HIGH)
				gval = 1;
			gpio_set_value(spi->cs_gpio, gval);
		}

		if (!tspi->prod_list)
			tegra_spi_set_cmd2(spi, speed);
		else
			tegra_spi_set_prod(tspi, spi->chip_select);
	} else {
		command1 = tspi->command1_reg;
		command1 &= ~SPI_BIT_LENGTH(~0);
		command1 |= SPI_BIT_LENGTH(bits_per_word - 1);
	}

	return command1;
}

static int tegra_spi_start_transfer_one(struct spi_device *spi,
		struct spi_transfer *t, u32 command1)
{
	struct tegra_spi_data *tspi = spi_controller_get_devdata(spi->master);
	unsigned total_fifo_words;
	int ret;

	total_fifo_words = tegra_spi_calculate_curr_xfer_param(spi, tspi, t);

	if (t->rx_nbits == SPI_NBITS_DUAL || t->tx_nbits == SPI_NBITS_DUAL)
		command1 |= SPI_BOTH_EN_BIT;
	else
		command1 &= ~SPI_BOTH_EN_BIT;

	if (tspi->is_packed)
		command1 |= SPI_PACKED;
	else
		command1 &= ~SPI_PACKED;

	command1 &= ~(SPI_CS_SEL_MASK | SPI_TX_EN | SPI_RX_EN);
	tspi->cur_direction = 0;
	if (t->rx_buf) {
		command1 |= SPI_RX_EN;
		tspi->cur_direction |= DATA_DIR_RX;
	}
	if (t->tx_buf) {
		command1 |= SPI_TX_EN;
		tspi->cur_direction |= DATA_DIR_TX;
	}
	command1 |= SPI_CS_SEL(spi->chip_select);
	tegra_spi_writel(tspi, command1, SPI_COMMAND1);
	tspi->command1_reg = command1;

	dev_dbg(tspi->dev, "The def 0x%x and written 0x%x\n",
		tspi->def_command1_reg, (unsigned)command1);

	if (total_fifo_words > SPI_FIFO_DEPTH)
		ret = tegra_spi_start_dma_based_transfer(tspi, t);
	else
		ret = tegra_spi_start_cpu_based_transfer(tspi, t);
	return ret;
}

static struct tegra_spi_client_data
	*tegra_spi_parse_cdata_dt(struct spi_device *spi)
{
	struct tegra_spi_client_data *cdata;
	struct device_node *slave_np, *data_np;
	int ret;

	slave_np = spi->dev.of_node;
	if (!slave_np) {
		dev_dbg(&spi->dev, "device node not found\n");
		return NULL;
	}

	data_np = of_get_child_by_name(slave_np, "controller-data");
	if (!data_np) {
		dev_dbg(&spi->dev, "child node 'controller-data' not found\n");
		return NULL;
	}

	cdata = kzalloc(sizeof(*cdata), GFP_KERNEL);
	if (!cdata) {
		of_node_put(data_np);
		return NULL;
	}

	of_property_read_u32(slave_np, "nvidia,tx-clk-tap-delay",
			     &cdata->tx_clk_tap_delay);
	of_property_read_u32(slave_np, "nvidia,rx-clk-tap-delay",
			     &cdata->rx_clk_tap_delay);

	ret = of_property_read_bool(data_np, "nvidia,enable-hw-based-cs");
	if (ret)
		cdata->is_hw_based_cs = 1;

	of_property_read_u32(data_np, "nvidia,cs-setup-clk-count",
			     &cdata->cs_setup_clk_count);
	of_property_read_u32(data_np, "nvidia,cs-hold-clk-count",
			     &cdata->cs_hold_clk_count);
	of_property_read_u32(data_np, "nvidia,rx-clk-tap-delay",
			     &cdata->rx_clk_tap_delay);
	of_property_read_u32(data_np, "nvidia,tx-clk-tap-delay",
			     &cdata->tx_clk_tap_delay);
	of_property_read_u32(data_np, "nvidia,cs-inactive-cycles",
			     &cdata->cs_inactive_cycles);
	of_property_read_u32(data_np, "nvidia,clk-delay-between-packets",
			     &cdata->clk_delay_between_packets);

	of_node_put(data_np);

	return cdata;
}

static void tegra_spi_cleanup(struct spi_device *spi)
{
	struct tegra_spi_client_data *cdata = spi->controller_data;
	struct tegra_spi_client_ctl_state *cstate = spi->controller_state;

	if (cdata && cdata->clk_delay_between_packets)
		cdata->cs_inactive_cycles = 0;
	spi->controller_state = NULL;
	if (cstate && cstate->cs_gpio_valid)
		gpio_free(spi->cs_gpio);
	kfree(cstate);
	spi->controller_data = NULL;
	if (spi->dev.of_node)
		kfree(cdata);
}

static int tegra_spi_setup(struct spi_device *spi)
{
	struct tegra_spi_data *tspi = spi_controller_get_devdata(spi->master);
	struct tegra_spi_client_data *cdata = spi->controller_data;
	struct tegra_spi_client_ctl_state *cstate = spi->controller_state;
	u32 val;
	unsigned long flags;
	int ret;

	dev_dbg(&spi->dev, "setup %d bpw, %scpol, %scpha, %dHz\n",
		spi->bits_per_word,
		spi->mode & SPI_CPOL ? "" : "~",
		spi->mode & SPI_CPHA ? "" : "~",
		spi->max_speed_hz);

	if (!cstate) {
		cstate = kzalloc(sizeof(*cstate), GFP_KERNEL);
		if (!cstate)
			return -ENOMEM;
		spi->controller_state = cstate;
	}
	if (!cdata) {
		cdata = tegra_spi_parse_cdata_dt(spi);
		spi->controller_data = cdata;
	}

	if (spi->master->cs_gpios && gpio_is_valid(spi->cs_gpio)) {
		if (!cstate->cs_gpio_valid) {
			int gpio_flag = GPIOF_OUT_INIT_HIGH;

			if (spi->mode & SPI_CS_HIGH)
				gpio_flag = GPIOF_OUT_INIT_LOW;

			ret = gpio_request_one(spi->cs_gpio, gpio_flag,
					       "cs_gpio");
			if (ret < 0) {
				dev_err(&spi->dev,
					"GPIO request failed: %d\n", ret);
				tegra_spi_cleanup(spi);
				return ret;
			}
			cstate->cs_gpio_valid = true;
		} else {
			int val = (spi->mode & SPI_CS_HIGH) ? 0 : 1;

			gpio_set_value(spi->cs_gpio, val);
		}
	}

	if (cdata && cdata->clk_delay_between_packets) {
		if (cdata->cs_inactive_cycles || !cstate->cs_gpio_valid) {
			dev_err(&spi->dev,
				"Invalid cs packet delay config\n");
			tegra_spi_cleanup(spi);
			return -EINVAL;
		}
		cdata->cs_inactive_cycles = cdata->clk_delay_between_packets;
	}

	ret = pm_runtime_get_sync(tspi->dev);
	if (ret < 0) {
		pm_runtime_put_noidle(tspi->dev);
		dev_err(tspi->dev, "pm runtime failed, e = %d\n", ret);
		if (cdata)
			tegra_spi_cleanup(spi);
		return ret;
	}

	if (tspi->soc_data->has_intr_mask_reg) {
		val = tegra_spi_readl(tspi, SPI_INTR_MASK);
		val &= ~SPI_INTR_ALL_MASK;
		tegra_spi_writel(tspi, val, SPI_INTR_MASK);
	}

	if (tspi->soc_data->has_fatal_intr_en_reg) {
		val = tegra_spi_readl(tspi, SPI_FATAL_INTR_EN_0);
		val |= SPI_FATAL_INTR_ALL_EN_0;
		tegra_spi_writel(tspi, val, SPI_FATAL_INTR_EN_0);
	}

	spin_lock_irqsave(&tspi->lock, flags);
	/* GPIO based chip select control */
	if (spi->cs_gpiod)
		gpiod_set_value(spi->cs_gpiod, 0);

	val = tspi->def_command1_reg;
	if (spi->mode & SPI_CS_HIGH)
		val &= ~SPI_CS_POL_INACTIVE(spi->chip_select);
	else
		val |= SPI_CS_POL_INACTIVE(spi->chip_select);
	if (tspi->def_chip_select == spi->chip_select)
		val |= SPI_MODE_SEL(spi->mode & 0x3);
	tspi->def_command1_reg = val;
	tegra_spi_writel(tspi, tspi->def_command1_reg, SPI_COMMAND1);
	if (tspi->def_chip_select == spi->chip_select)
		tegra_spi_set_cmd2(spi, spi->max_speed_hz);
	spin_unlock_irqrestore(&tspi->lock, flags);

	pm_runtime_put(tspi->dev);
	return 0;
}

static void tegra_spi_dump_regs(struct tegra_spi_data *tspi)
{
	dev_dbg(tspi->dev, "============ SPI REGISTER DUMP ============\n");
	dev_dbg(tspi->dev, "Command1:    0x%08x | Command2:    0x%08x\n",
		tegra_spi_readl(tspi, SPI_COMMAND1),
		tegra_spi_readl(tspi, SPI_COMMAND2));
	dev_dbg(tspi->dev, "DMA_CTL:     0x%08x | DMA_BLK:     0x%08x\n",
		tegra_spi_readl(tspi, SPI_DMA_CTL),
		tegra_spi_readl(tspi, SPI_DMA_BLK));
	dev_dbg(tspi->dev, "TRANS_STAT:  0x%08x | FIFO_STATUS: 0x%08x\n",
		tegra_spi_readl(tspi, SPI_TRANS_STATUS),
		tegra_spi_readl(tspi, SPI_FIFO_STATUS));
}

static void tegra_spi_transfer_delay(int delay)
{
	if (!delay)
		return;

	if (delay >= 1000)
		mdelay(delay / 1000);

	udelay(delay % 1000);
}

static  int tegra_spi_cs_low(struct spi_device *spi, bool state)
{
	struct tegra_spi_data *tspi = spi_controller_get_devdata(spi->master);
	struct tegra_spi_client_ctl_state *cstate = spi->controller_state;
	int ret;
	unsigned long val;
	unsigned long flags;

	ret = pm_runtime_get_sync(tspi->dev);
	if (ret < 0) {
		dev_err(tspi->dev, "pm runtime failed, e = %d\n", ret);
		return ret;
	}

	if (cstate && cstate->cs_gpio_valid)
		gpio_set_value(spi->cs_gpio, 0);

	spin_lock_irqsave(&tspi->lock, flags);
	if (!(spi->mode & SPI_CS_HIGH)) {
		val = tegra_spi_readl(tspi, SPI_COMMAND1);
		if (state)
			val &= ~SPI_CS_POL_INACTIVE(spi->chip_select);
		else
			val |= SPI_CS_POL_INACTIVE(spi->chip_select);
		tegra_spi_writel(tspi, val, SPI_COMMAND1);
	}

	spin_unlock_irqrestore(&tspi->lock, flags);
	pm_runtime_put(tspi->dev);
	return 0;
}

static int tegra_spi_transfer_one_message(struct spi_controller *ctrl,
					  struct spi_message *msg)
{
	bool is_first_msg = true;
	struct tegra_spi_data *tspi = spi_controller_get_devdata(ctrl);
	struct spi_transfer *xfer;
	struct spi_device *spi = msg->spi;
	struct tegra_spi_client_ctl_state *cstate = spi->controller_state;
	int ret;
	int gval = 1;
	bool skip = false;
	int single_xfer;
	u32 cmd1 = 0, dma_ctl = 0;

	msg->status = 0;
	msg->actual_length = 0;

	if (spi->mode & SPI_CS_HIGH)
		gval = 0;

	single_xfer = list_is_singular(&msg->transfers);
	list_for_each_entry(xfer, &msg->transfers, transfer_list) {

		reinit_completion(&tspi->xfer_completion);

		cmd1 = tegra_spi_setup_transfer_one(spi, xfer, is_first_msg,
						    single_xfer);

		if (!xfer->len) {
			ret = 0;
			skip = true;
			goto complete_xfer;
		}

		ret = tegra_spi_start_transfer_one(spi, xfer, cmd1);
		if (ret < 0) {
			dev_err(tspi->dev,
				"spi can not start transfer, err %d\n", ret);
			goto complete_xfer;
		}

		is_first_msg = false;
		if (tspi->polling_mode)
			ret = tegra_spi_status_poll(tspi);
		else
			ret = wait_for_completion_timeout(
					&tspi->xfer_completion,
					SPI_DMA_TIMEOUT);
		if (WARN_ON(ret == 0)) {
			dev_err(tspi->dev,
				"spi transfer timeout, err %d\n", ret);
			if (tspi->is_curr_dma_xfer &&
			    (tspi->cur_direction & DATA_DIR_TX))
				dmaengine_terminate_all(tspi->tx_dma_chan);
			if (tspi->is_curr_dma_xfer &&
			    (tspi->cur_direction & DATA_DIR_RX))
				dmaengine_terminate_all(tspi->rx_dma_chan);
			ret = -EIO;
			tegra_spi_dump_regs(tspi);
			/* Abort transfer by resetting pio/dma bit */
			if (!tspi->is_curr_dma_xfer) {
				cmd1 = tegra_spi_readl(tspi, SPI_COMMAND1);
				cmd1 &= ~SPI_PIO;
				tegra_spi_writel(tspi, cmd1, SPI_COMMAND1);
			} else {
				dma_ctl = tegra_spi_readl(tspi, SPI_DMA_CTL);
				dma_ctl &= ~SPI_DMA_EN;
				tegra_spi_writel(tspi, dma_ctl, SPI_DMA_CTL);
			}
			reset_control_assert(tspi->rst);
			udelay(2);
			reset_control_deassert(tspi->rst);
			tspi->last_used_cs = ctrl->num_chipselect + 1;
			tegra_spi_set_intr_mask(tspi);
			tegra_spi_set_fatal_intr_en(tspi);
			goto complete_xfer;
		}

		if (tspi->tx_status ||  tspi->rx_status) {
			dev_err(tspi->dev, "Error in Transfer\n");
			ret = -EIO;
			tegra_spi_dump_regs(tspi);
			goto complete_xfer;
		}
		msg->actual_length += xfer->len;

complete_xfer:
		if (prefer_last_used_cs)
			cmd1 = tspi->command1_reg;
		else
			cmd1 = tspi->def_command1_reg;
		if (ret < 0 || skip) {
			if (cstate && cstate->cs_gpio_valid)
				gpio_set_value(spi->cs_gpio, gval);
			tegra_spi_writel(tspi, cmd1, SPI_COMMAND1);
			tegra_spi_transfer_delay(xfer->delay_usecs);
			goto exit;
		} else if (list_is_last(&xfer->transfer_list,
					&msg->transfers)) {
			if (xfer->cs_change)
				tspi->cs_control = spi;
			else {
				if (cstate && cstate->cs_gpio_valid)
					gpio_set_value(spi->cs_gpio, gval);
				tegra_spi_writel(tspi, cmd1, SPI_COMMAND1);
				tegra_spi_transfer_delay(xfer->delay_usecs);
			}
		} else if (xfer->cs_change) {
			/* CS should de-asserted
			 * at the end of current transfer
			 */
			if (cstate && cstate->cs_gpio_valid)
				gpio_set_value(spi->cs_gpio, gval);
			if (!tspi->is_hw_based_cs) {
				u32 cmd1_ncs = (cmd1 & SPI_CS_SW_VAL)
						? cmd1 & ~SPI_CS_SW_VAL
						: cmd1 |  SPI_CS_SW_VAL;
				tegra_spi_writel(tspi, cmd1_ncs, SPI_COMMAND1);
			}
			tegra_spi_transfer_delay(xfer->delay_usecs);
			/* CS should asserted again for the next transfer */
			tegra_spi_writel(tspi, cmd1, SPI_COMMAND1);
			if (cstate && cstate->cs_gpio_valid)
				gpio_set_value(spi->cs_gpio, !gval);
		}

	}
	ret = 0;
exit:
	if (prefer_last_used_cs)
		cmd1 = SPI_CMD1_GR_MASK & tspi->command1_reg;
	else
		cmd1 = tegra_spi_readl(tspi, SPI_COMMAND1);
	/* CS de-assert is required before clock
	 * goes to it's default state.
	 */
	if (!tspi->is_hw_based_cs) {
		if (spi->mode & SPI_CS_HIGH) {
			/* Active high. Reset the value to make it deactive */
			cmd1 &= ~SPI_CS_SW_VAL;
		} else {
			/* Active low. Set the value to make it deactive */
			cmd1 |= SPI_CS_SW_VAL;
		}
	}

	tegra_spi_writel(tspi, cmd1, SPI_COMMAND1);
	if (!prefer_last_used_cs)
		tegra_spi_writel(tspi, tspi->def_command1_reg, SPI_COMMAND1);

	msg->status = ret;
	spi_finalize_current_message(ctrl);
	return ret;
}

static irqreturn_t handle_cpu_based_xfer(struct tegra_spi_data *tspi)
{
	struct spi_transfer *t = tspi->curr_xfer;
	unsigned long flags;

	spin_lock_irqsave(&tspi->lock, flags);
	if (tspi->tx_status ||  tspi->rx_status) {
		dev_err(tspi->dev, "CpuXfer ERROR bit set 0x%x\n",
			tspi->status_reg);
		dev_err(tspi->dev, "CpuXfer 0x%08x:0x%08x\n",
			tspi->command1_reg, tspi->dma_control_reg);
		tegra_spi_dump_regs(tspi);
		complete(&tspi->xfer_completion);
		spin_unlock_irqrestore(&tspi->lock, flags);
		reset_control_assert(tspi->rst);
		udelay(2);
		reset_control_deassert(tspi->rst);
		tegra_spi_set_intr_mask(tspi);
		tegra_spi_set_fatal_intr_en(tspi);
		return IRQ_HANDLED;
	}

	if (tspi->cur_direction & DATA_DIR_RX)
		tegra_spi_read_rx_fifo_to_client_rxbuf(tspi, t);

	if (tspi->cur_direction & DATA_DIR_TX)
		tspi->cur_pos = tspi->cur_tx_pos;
	else
		tspi->cur_pos = tspi->cur_rx_pos;

	if (tspi->cur_pos == t->len) {
		complete(&tspi->xfer_completion);
		goto exit;
	}

	tegra_spi_calculate_curr_xfer_param(tspi->cur_spi, tspi, t);
	tegra_spi_start_cpu_based_transfer(tspi, t);
exit:
	spin_unlock_irqrestore(&tspi->lock, flags);
	return IRQ_HANDLED;
}

static irqreturn_t handle_dma_based_xfer(struct tegra_spi_data *tspi)
{
	struct spi_transfer *t = tspi->curr_xfer;
	long wait_status;
	int err = 0;
	unsigned total_fifo_words;
	unsigned long flags;

	/* Abort dmas if any error */
	if (tspi->cur_direction & DATA_DIR_TX) {
		if (tspi->tx_status) {
			dmaengine_terminate_all(tspi->tx_dma_chan);
			err += 1;
		} else {
			wait_status = wait_for_completion_interruptible_timeout(
				&tspi->tx_dma_complete, SPI_DMA_TIMEOUT);
			if (wait_status <= 0) {
				dmaengine_terminate_all(tspi->tx_dma_chan);
				dev_err(tspi->dev, "TxDma Xfer failed\n");
				err += 1;
			}
		}
	}

	if (tspi->cur_direction & DATA_DIR_RX) {
		if (tspi->rx_status) {
			dmaengine_terminate_all(tspi->rx_dma_chan);
			err += 2;
		} else {
			wait_status = wait_for_completion_interruptible_timeout(
				&tspi->rx_dma_complete, SPI_DMA_TIMEOUT);
			if (wait_status <= 0) {
				dmaengine_terminate_all(tspi->rx_dma_chan);
				dev_err(tspi->dev, "RxDma Xfer failed\n");
				err += 2;
			}
		}
	}

	spin_lock_irqsave(&tspi->lock, flags);
	if (err) {
		dev_err(tspi->dev, "DmaXfer: ERROR bit set 0x%x\n",
			tspi->status_reg);
		dev_err(tspi->dev, "DmaXfer 0x%08x:0x%08x\n",
			tspi->command1_reg, tspi->dma_control_reg);
		tegra_spi_dump_regs(tspi);
		complete(&tspi->xfer_completion);
		spin_unlock_irqrestore(&tspi->lock, flags);
		reset_control_assert(tspi->rst);
		udelay(2);
		reset_control_deassert(tspi->rst);
		tegra_spi_set_intr_mask(tspi);
		tegra_spi_set_fatal_intr_en(tspi);
		return IRQ_HANDLED;
	}

	if (tspi->cur_direction & DATA_DIR_RX)
		tegra_spi_copy_spi_rxbuf_to_client_rxbuf(tspi, t);

	if (tspi->cur_direction & DATA_DIR_TX)
		tspi->cur_pos = tspi->cur_tx_pos;
	else
		tspi->cur_pos = tspi->cur_rx_pos;

	if (tspi->cur_pos == t->len) {
		complete(&tspi->xfer_completion);
		goto exit;
	}

	/* Continue transfer in current message */
	total_fifo_words = tegra_spi_calculate_curr_xfer_param(tspi->cur_spi,
							tspi, t);
	if (total_fifo_words > SPI_FIFO_DEPTH)
		err = tegra_spi_start_dma_based_transfer(tspi, t);
	else
		err = tegra_spi_start_cpu_based_transfer(tspi, t);

exit:
	spin_unlock_irqrestore(&tspi->lock, flags);
	return IRQ_HANDLED;
}

static int tegra_spi_status_poll(struct tegra_spi_data *tspi)
{
	unsigned int status;
	unsigned long timeout;

	timeout = SPI_POLL_TIMEOUT;
	/*
	 * Read register would take between 1~3us and 1us delay added in loop
	 * Calculate timeout taking this into consideration
	 */
	do {
		status = tegra_spi_readl(tspi, SPI_TRANS_STATUS);
		if (status & SPI_RDY)
			break;
		timeout--;
		udelay(1);
	} while (timeout);

	if (!timeout) {
		dev_err(tspi->dev, "transfer timeout (polling)\n");
		return 0;
	}

	tspi->status_reg = tegra_spi_readl(tspi, SPI_FIFO_STATUS);
	if (tspi->cur_direction & DATA_DIR_TX)
		tspi->tx_status = tspi->status_reg &
					(SPI_TX_FIFO_UNF | SPI_TX_FIFO_OVF);

	if (tspi->cur_direction & DATA_DIR_RX)
		tspi->rx_status = tspi->status_reg &
					(SPI_RX_FIFO_OVF | SPI_RX_FIFO_UNF);

	tegra_spi_clear_status(tspi);

	if (!tspi->is_curr_dma_xfer)
		handle_cpu_based_xfer(tspi);
	else
		handle_dma_based_xfer(tspi);

	return timeout;
}

static irqreturn_t tegra_spi_isr_thread(int irq, void *context_data)
{
	struct tegra_spi_data *tspi = context_data;

	if (!tspi->is_curr_dma_xfer)
		return handle_cpu_based_xfer(tspi);
	return handle_dma_based_xfer(tspi);
}

static irqreturn_t tegra_spi_isr(int irq, void *context_data)
{
	struct tegra_spi_data *tspi = context_data;

	if (tspi->polling_mode)
		dev_warn(tspi->dev, "interrupt raised in polling mode\n");

	tspi->status_reg = tegra_spi_readl(tspi, SPI_FIFO_STATUS);
	if (tspi->cur_direction & DATA_DIR_TX)
		tspi->tx_status = tspi->status_reg &
					(SPI_TX_FIFO_UNF | SPI_TX_FIFO_OVF);

	if (tspi->cur_direction & DATA_DIR_RX)
		tspi->rx_status = tspi->status_reg &
					(SPI_RX_FIFO_OVF | SPI_RX_FIFO_UNF);
	tegra_spi_clear_status(tspi);

	return IRQ_WAKE_THREAD;
}

static void tegra_spi_parse_dt(struct tegra_spi_data *tspi)
{
	const __be32 *prop;
	struct device_node *np = tspi->dev->of_node;
	struct device_node *nc = NULL;
	struct device_node *found_nc = NULL;
	int len;
	int ret;

	if (of_find_property(np, "nvidia,clock-always-on", NULL))
		tspi->clock_always_on = true;

	if (of_find_property(np, "nvidia,polling-mode", NULL))
		tspi->polling_mode = true;

	if (of_property_read_u32(np, "spi-max-frequency",
				 &tspi->ctrl->max_speed_hz))
		tspi->ctrl->max_speed_hz = 25000000; /* 25MHz */

	if (of_property_read_u32(np, "nvidia,maximum-dma-buffer-size",
				 &tspi->dma_buf_size))
		tspi->dma_buf_size = DEFAULT_SPI_DMA_BUF_LEN;

	/*
	 * Last child node or first node which has property as default-cs will
	 * become the default. When no client is defined, default chipselect
	 * is zero.
	 */
	tspi->def_chip_select = 0;

	for_each_available_child_of_node(np, nc) {
		if (!strcmp(nc->name, "prod-settings"))
			continue;
		found_nc = nc;
		ret = of_property_read_bool(nc, "nvidia,default-chipselect");
		if (ret)
			break;
	}
	if (found_nc) {
		prop = of_get_property(found_nc, "reg", &len);
		if (!prop || len < sizeof(*prop))
			dev_err(tspi->dev, "%s has no reg property\n",
				found_nc->full_name);
		else
			tspi->def_chip_select = be32_to_cpup(prop);
	}
}

static struct tegra_spi_soc_data tegra114_spi_soc_data = {
	.has_intr_mask_reg = false,
	.set_rx_tap_delay = false,
	.has_fatal_intr_en_reg = false,
};

static struct tegra_spi_soc_data tegra124_spi_soc_data = {
	.has_intr_mask_reg = false,
	.set_rx_tap_delay = true,
	.has_fatal_intr_en_reg = false,
};

static struct tegra_spi_soc_data tegra210_spi_soc_data = {
	.has_intr_mask_reg = true,
	.set_rx_tap_delay = false,
	.has_fatal_intr_en_reg = false,
};

static struct tegra_spi_soc_data tegra186_spi_soc_data = {
	.has_intr_mask_reg = true,
	.set_rx_tap_delay = false,
	.has_fatal_intr_en_reg = false,
};

static struct tegra_spi_soc_data tegra234_spi_soc_data = {
	.has_intr_mask_reg = true,
	.set_rx_tap_delay = false,
	.has_fatal_intr_en_reg = true,
};

static const struct of_device_id tegra_spi_of_match[] = {
	{
		.compatible = "nvidia,tegra114-spi",
		.data	    = &tegra114_spi_soc_data,
	}, {
		.compatible = "nvidia,tegra124-spi",
		.data	    = &tegra124_spi_soc_data,
	}, {
		.compatible = "nvidia,tegra210-spi",
		.data	    = &tegra210_spi_soc_data,
	}, {
		.compatible = "nvidia,tegra186-spi",
		.data       = &tegra186_spi_soc_data,
	}, {
		.compatible = "nvidia,tegra234-spi",
		.data       = &tegra234_spi_soc_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, tegra_spi_of_match);

static int tegra_spi_probe(struct platform_device *pdev)
{
	struct spi_controller	*ctrl;
	struct tegra_spi_data	*tspi;
	struct resource		*r;
	int ret, spi_irq;
	int bus_num;

	ctrl = devm_spi_alloc_master(&pdev->dev, sizeof(*tspi));
	if (!ctrl) {
		dev_err(&pdev->dev, "ctrl allocation failed\n");
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, ctrl);
	tspi = spi_controller_get_devdata(ctrl);

	/* the spi->mode bits understood by this driver: */
	ctrl->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH | SPI_LSB_FIRST |
			    SPI_TX_DUAL | SPI_RX_DUAL | SPI_3WIRE;
	ctrl->bits_per_word_mask = SPI_BPW_RANGE_MASK(4, 32);
	ctrl->setup = tegra_spi_setup;
	ctrl->cleanup = tegra_spi_cleanup;
	ctrl->transfer_one_message = tegra_spi_transfer_one_message;
	ctrl->num_chipselect = MAX_CHIP_SELECT;
	ctrl->auto_runtime_pm = true;
	bus_num = of_alias_get_id(pdev->dev.of_node, "spi");
	if (bus_num >= 0)
		ctrl->bus_num = bus_num;
	ctrl->spi_cs_low = tegra_spi_cs_low;

	tspi->ctrl = ctrl;
	tspi->dev = &pdev->dev;

	tspi->prod_list = devm_tegra_prod_get(tspi->dev);
	if (IS_ERR(tspi->prod_list)) {
		dev_dbg(&pdev->dev, "Prod settings list not initialized\n");
		tspi->prod_list = NULL;
	}

	spin_lock_init(&tspi->lock);

	tspi->soc_data = of_device_get_match_data(&pdev->dev);
	if (!tspi->soc_data) {
		dev_err(&pdev->dev, "unsupported tegra\n");
		ret = -ENODEV;
		goto exit_free_ctrl;
	}

	tegra_spi_parse_dt(tspi);

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	tspi->base = devm_ioremap_resource(&pdev->dev, r);
	if (IS_ERR(tspi->base)) {
		ret = PTR_ERR(tspi->base);
		goto exit_free_ctrl;
	}
	tspi->phys = r->start;

	spi_irq = platform_get_irq(pdev, 0);
	if (spi_irq < 0) {
		ret = spi_irq;
		goto exit_free_ctrl;
	}
	tspi->irq = spi_irq;

	tspi->clk = devm_clk_get(&pdev->dev, "spi");
	if (IS_ERR(tspi->clk)) {
		dev_err(&pdev->dev, "can not get clock\n");
		ret = PTR_ERR(tspi->clk);
		goto exit_free_ctrl;
	}

	tspi->rst = devm_reset_control_get_exclusive(&pdev->dev, "spi");
	if (IS_ERR(tspi->rst)) {
		dev_err(&pdev->dev, "can not get reset\n");
		ret = PTR_ERR(tspi->rst);
		goto exit_free_ctrl;
	}

	tspi->max_buf_size = SPI_FIFO_DEPTH << 2;
	tspi->min_div = 0;

	ret = tegra_spi_init_dma_param(tspi, true);
	if (ret < 0)
		goto exit_free_ctrl;
	ret = tegra_spi_init_dma_param(tspi, false);
	if (ret < 0)
		goto exit_rx_dma_free;
	tspi->max_buf_size = tspi->dma_buf_size;
	init_completion(&tspi->tx_dma_complete);
	init_completion(&tspi->rx_dma_complete);

	init_completion(&tspi->xfer_completion);

	if (tspi->clock_always_on) {
		ret = clk_prepare_enable(tspi->clk);
		if (ret < 0) {
			dev_err(tspi->dev, "clk_prepare failed: %d\n", ret);
			goto exit_tx_dma_free;
		}
	}

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = tegra_spi_runtime_resume(&pdev->dev);
		if (ret)
			goto exit_pm_disable;
	}

	ret = pm_runtime_get_sync(&pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "pm runtime get failed, e = %d\n", ret);
		pm_runtime_put_noidle(&pdev->dev);
		goto exit_pm_disable;
	}

	reset_control_assert(tspi->rst);
	udelay(2);
	reset_control_deassert(tspi->rst);

	tspi->last_used_cs = ctrl->num_chipselect + 1;
	tegra_spi_set_prod(tspi, tspi->def_chip_select);
	tspi->def_command1_reg  = tegra_spi_readl(tspi, SPI_COMMAND1);
	tspi->def_command1_reg |= SPI_CS_SEL(tspi->def_chip_select);
	tegra_spi_writel(tspi, tspi->def_command1_reg, SPI_COMMAND1);
	tspi->def_command1_reg  = SPI_M_S | SPI_LSBYTE_FE;
	tegra_spi_writel(tspi, tspi->def_command1_reg, SPI_COMMAND1);
	tspi->spi_cs_timing1 = tegra_spi_readl(tspi, SPI_CS_TIMING1);
	tspi->spi_cs_timing2 = tegra_spi_readl(tspi, SPI_CS_TIMING2);
	tspi->command2_reg = tegra_spi_readl(tspi, SPI_COMMAND2);
	pm_runtime_put(&pdev->dev);
	ret = request_threaded_irq(tspi->irq, tegra_spi_isr,
				   tegra_spi_isr_thread, IRQF_ONESHOT,
				   dev_name(&pdev->dev), tspi);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register ISR for IRQ %d\n",
			tspi->irq);
		goto exit_pm_disable;
	}

	ctrl->dev.of_node = pdev->dev.of_node;
	ret = devm_spi_register_controller(&pdev->dev, ctrl);
	if (ret < 0) {
		dev_err(&pdev->dev, "can not register to ctrl err %d\n", ret);
		goto exit_free_irq;
	}

	return ret;

exit_free_irq:
	free_irq(spi_irq, tspi);
exit_pm_disable:
	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra_spi_runtime_suspend(&pdev->dev);
	if (tspi->clock_always_on)
		clk_disable_unprepare(tspi->clk);
exit_tx_dma_free:
	tegra_spi_deinit_dma_param(tspi, false);
exit_rx_dma_free:
	tegra_spi_deinit_dma_param(tspi, true);
exit_free_ctrl:
	return ret;
}

static int tegra_spi_remove(struct platform_device *pdev)
{
	struct spi_controller *ctrl = platform_get_drvdata(pdev);
	struct tegra_spi_data	*tspi = spi_controller_get_devdata(ctrl);

	free_irq(tspi->irq, tspi);

	if (tspi->tx_dma_chan)
		tegra_spi_deinit_dma_param(tspi, false);

	if (tspi->rx_dma_chan)
		tegra_spi_deinit_dma_param(tspi, true);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra_spi_runtime_suspend(&pdev->dev);

	if (tspi->clock_always_on)
		clk_disable_unprepare(tspi->clk);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tegra_spi_suspend(struct device *dev)
{
	struct spi_controller *ctrl = dev_get_drvdata(dev);
	struct tegra_spi_data *tspi = spi_controller_get_devdata(ctrl);
	int ret;

	ret = spi_controller_suspend(ctrl);

	if (tspi->clock_always_on)
		clk_disable_unprepare(tspi->clk);

	return ret;
}

static int tegra_spi_resume(struct device *dev)
{
	struct spi_controller *ctrl = dev_get_drvdata(dev);
	struct tegra_spi_data *tspi = spi_controller_get_devdata(ctrl);
	int ret;

	if (tspi->clock_always_on) {
		ret = clk_prepare_enable(tspi->clk);
		if (ret < 0) {
			dev_err(tspi->dev, "clk_prepare failed: %d\n", ret);
			return ret;
		}
	}

	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		pm_runtime_put_noidle(dev);
		dev_err(dev, "pm runtime failed, e = %d\n", ret);
		return ret;
	}
	tegra_spi_writel(tspi, tspi->command1_reg, SPI_COMMAND1);
	tegra_spi_writel(tspi, tspi->command2_reg, SPI_COMMAND2);
	tspi->last_used_cs = ctrl->num_chipselect + 1;
	tegra_spi_set_intr_mask(tspi);
	tegra_spi_set_fatal_intr_en(tspi);
	pm_runtime_put(dev);

	return spi_controller_resume(ctrl);
}
#endif

static int tegra_spi_runtime_suspend(struct device *dev)
{
	struct spi_controller *ctrl = dev_get_drvdata(dev);
	struct tegra_spi_data *tspi = spi_controller_get_devdata(ctrl);

	/* Flush all write which are in PPSB queue by reading back */
	tegra_spi_readl(tspi, SPI_COMMAND1);

	if (!tspi->clock_always_on)
		clk_disable_unprepare(tspi->clk);
	return 0;
}

static int tegra_spi_runtime_resume(struct device *dev)
{
	struct spi_controller *ctrl = dev_get_drvdata(dev);
	struct tegra_spi_data *tspi = spi_controller_get_devdata(ctrl);
	int ret;

	if (!tspi->clock_always_on) {
		ret = clk_prepare_enable(tspi->clk);
		if (ret < 0) {
			dev_err(tspi->dev, "clk_prepare failed: %d\n", ret);
			return ret;
		}
	}
	return 0;
}

static const struct dev_pm_ops tegra_spi_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra_spi_runtime_suspend,
		tegra_spi_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(tegra_spi_suspend, tegra_spi_resume)
};
static struct platform_driver tegra_spi_driver = {
	.driver = {
		.name		= "spi-tegra114",
		.pm		= &tegra_spi_pm_ops,
		.of_match_table	= tegra_spi_of_match,
	},
	.probe =	tegra_spi_probe,
	.remove =	tegra_spi_remove,
};
module_platform_driver(tegra_spi_driver);

MODULE_ALIAS("platform:spi-tegra114");
MODULE_DESCRIPTION("NVIDIA Tegra114 SPI Controller Driver");
MODULE_AUTHOR("Laxman Dewangan <ldewangan@nvidia.com>");
MODULE_LICENSE("GPL v2");
