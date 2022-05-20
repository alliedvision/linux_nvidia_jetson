/*
 * SPI driver for NVIDIA's Tegra SPI slave continuous mode Controller.
 *
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/err.h>
#include <linux/of_gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>
#include <linux/clk/tegra.h>
#include <linux/circ_buf.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

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
#define SPI_CS_SS_VAL				(1 << 20)
#define SPI_CS_SW_HW				(1 << 21)
#define SPI_CS(x)				(((x) >> 26) & 0x3)
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
#define SPI_SETUP_HOLD(setup, hold)		(((setup - 1) << 4) |	\
						(hold - 1))
#define SPI_CS_SETUP_HOLD(reg, cs, val)			\
		((((val) & 0xFFu) << ((cs) * 8)) |	\
		((reg) & ~(0xFFu << ((cs) * 8))))

		#define SPI_TRANS_STATUS			0x010
#define SPI_BLK_CNT(val)			(((val) >> 0) & 0xFFFF)
#define SPI_SLV_IDLE_COUNT(val)			(((val) >> 16) & 0xFF)
#define SPI_RDY					(1 << 30)

#define SPI_CS_TIMING2				0x00C

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
#define SPI_CS_BOUNDARY_TIMEOUT_INTR		(1 << 9)
#define SPI_TX_FIFO_FLUSH			(1 << 14)
#define SPI_RX_FIFO_FLUSH			(1 << 15)
#define SPI_TX_FIFO_EMPTY_COUNT(val)		(((val) >> 16) & 0x7F)
#define SPI_RX_FIFO_FULL_COUNT(val)		(((val) >> 23) & 0x7F)
#define SPI_FRAME_END				(1 << 30)
#define SPI_CS_INACTIVE				(1 << 31)

#define SPI_SLAVE_INTR				(SPI_CS_INACTIVE | \
						SPI_FRAME_END)
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
#define SPI_TX_TRIG(val)			(((val) >> 15) & 0x3)
#define SPI_RX_TRIG_1				(0 << 19)
#define SPI_RX_TRIG_4				(1 << 19)
#define SPI_RX_TRIG_8				(2 << 19)
#define SPI_RX_TRIG_16				(3 << 19)
#define SPI_RX_TRIG_MASK			(3 << 19)
#define SPI_RX_TRIG(val)			(((val) >> 19) & 0x3)
#define SPI_PAUSE				(1 << 29)
#define SPI_CONT				(1 << 30)
#define SPI_DMA					(1 << 31)
#define SPI_DMA_EN				SPI_DMA

#define SPI_DMA_BLK				0x024
#define SPI_DMA_BLK_SET(x)			(((x) & 0xFFFF) << 0)

#define SPI_TX_FIFO				0x108
#define SPI_RX_FIFO				0x188

#define SPI_INTR_MASK				0x18c
#define SPI_INTR_CS_BOUNDARY_TIMEOUT_INTR_MASK	BIT(9)
#define SPI_INTR_RX_FIFO_UNF_MASK		BIT(25)
#define SPI_INTR_RX_FIFO_OVF_MASK		BIT(26)
#define SPI_INTR_TX_FIFO_UNF_MASK		BIT(27)
#define SPI_INTR_TX_FIFO_OVF_MASK		BIT(28)
#define SPI_INTR_RDY_MASK			BIT(29)
#define SPI_INTR_FRAME_END_INTR_MASK		BIT(30)
#define SPI_INTR_CS_INTR_MASK			BIT(31)
#define SPI_INTR_ALL_MASK			(0x7fUL << 25)

#define SPI_MISC				0x194
#define SPI_MISC_CLKEN_OVERRIDE			BIT(31)
#define SPI_MISC_EXT_CLK_EN			BIT(30)

#define SPI_FATAL_INTR_EN			0x198
#define SPI_CS_BOUNDARY_TIMEOUT			0x19c

#define SPI_TIMEOUT_BOUNDARY_STATUS		0x1a0
#define SPI_TIMEOUT_NUM_OF_PACKETS(val)		((val) & 0xFFFFFF)
#define SPI_TIMEOUT_PADDED_BYTES(val)		(((val) >> 28) & 0xF)

#define SPI_TIMEOUT_BOUNDARY_FIFO_STATUS	0x1a4
#define SPI_TIMEOUT_FIFO_FULL_COUNT(val)	(((val) >> 16) & 0x7F)
#define SPI_TIMEOUT_FIFO_OVF			BIT(3)
#define SPI_TIMEOUT_FIFO_UNF			BIT(2)
#define SPI_TIMEOUT_FIFO_FULL			BIT(1)
#define SPI_TIMEOUT_FIFO_EMPTY			BIT(0)

#define SPI_DEBUG_REGISTER			0x288

#define MAX_CHIP_SELECT				4
#define SPI_FIFO_DEPTH				64
#define DATA_DIR_TX				(1 << 0)
#define DATA_DIR_RX				(1 << 1)

#define SPI_DMA_TIMEOUT				(msecs_to_jiffies(10000))
#define DEFAULT_SPI_DMA_BUF_LEN			(256*1024)
#define DEFAULT_SPI_DMA_PERIOD_LEN		256
#define TX_FIFO_EMPTY_COUNT_MAX			SPI_TX_FIFO_EMPTY_COUNT(0x40)
#define RX_FIFO_FULL_COUNT_ZERO			SPI_RX_FIFO_FULL_COUNT(0)
#define MAX_HOLD_CYCLES				16
#define SPI_DEFAULT_SPEED			25000000
#define SPI_SPEED_TAP_DELAY_MARGIN		35000000
#define SPI_POLL_TIMEOUT			10000
#define SPI_DEFAULT_RX_TAP_DELAY		10
#define SPI_DEFAULT_TX_TAP_DELAY		0
#define SPI_FIFO_FLUSH_MAX_DELAY		2000

struct tegra_spi_device_controller_data {
		bool is_hw_based_cs;
		bool variable_length_transfer;
		int cs_setup_clk_count;
		int cs_hold_clk_count;
		int rx_clk_tap_delay;
		int tx_clk_tap_delay;
		int cs_inactive_cycles;
		int clk_delay_between_packets;
		int cs_gpio;
};

struct tspi_circ_buf {
	unsigned int *buf;
	int head;
	int tail;
	int size;
};

struct tegra_spi_cnt_chip_data {
	bool boundary_reg;
};

struct tegra_spi_cnt_data {
	struct device			*dev;
	struct spi_controller		*master;
	spinlock_t				lock;

	struct clk				*clk;
	struct reset_control	*rstc;
	void __iomem			*base;
	phys_addr_t				phys;
	unsigned int			irq;
	bool					clock_always_on;
	bool					raw_data;
	struct tspi_circ_buf	*tspi_queue;
	struct dentry			*debugfs;

	u32						cur_speed;
	unsigned int			min_div;

	struct spi_device		*cur_spi;
	struct spi_device		*cs_control;
	unsigned int			cur_pad_pos;
	unsigned int			words_per_32bit;
	unsigned int			bytes_per_word;
	unsigned int			curr_dma_words;
	unsigned int			cur_direction;

	unsigned int			cur_rx_pos;
	unsigned int			cur_tx_pos;
	unsigned int			cur_dma_pos;
	unsigned int			produced_data;
	unsigned int			consumed_data;
	unsigned int			next_pad;
	unsigned int			next_pad_count;
	unsigned int			dma_buf_size;
	unsigned int			dma_period_size;

	struct completion		dma_complete;

	u32						tx_status;
	u32						rx_status;
	u32						status_reg;
	u32						timeout_reg;

	u32						command1_reg;
	u32						command2_reg;
	u32						dma_control_reg;
	u32						def_command1_reg;
	u8						chip_select;

	struct completion		xfer_completion;
	struct spi_transfer		*curr_xfer;

	struct dma_chan			*dma_chan;
	u32						*dma_buf;
	dma_addr_t				dma_phys;
	struct dma_async_tx_descriptor		*dma_desc;

	const struct tegra_spi_cnt_chip_data	*chip_data;
	struct tegra_prod			*prod_list;
	struct work_struct			transfer_work;
	struct spi_device			*test_device;
};

static inline unsigned long tegra_spi_cnt_readl(struct tegra_spi_cnt_data *tspi,
		unsigned long reg)
{
	return readl(tspi->base + reg);
}

static inline void tegra_spi_cnt_writel(struct tegra_spi_cnt_data *tspi,
		unsigned long val, unsigned long reg)
{
	writel(val, tspi->base + reg);
}

static void tegra_spi_cnt_dump_regs(struct tegra_spi_cnt_data *tspi)
{
	u32 command1_reg, command2_reg;
	u32 fifo_status_reg;
	u32 dma_ctrl_reg, blk_size_reg;
	u32 trans_sts_reg;
	u32 timing1_reg, timing2_reg;
	u32 intr_mask_reg, misc_reg;
	u32 fatal_mask_reg;
	u32 br_tout_reg = 0, tout_fifo_reg = 0, debug_reg = 0;

	command1_reg	= tegra_spi_cnt_readl(tspi, SPI_COMMAND1);
	command2_reg	= tegra_spi_cnt_readl(tspi, SPI_COMMAND2);
	timing1_reg	= tegra_spi_cnt_readl(tspi, SPI_CS_TIMING1);
	timing2_reg	= tegra_spi_cnt_readl(tspi, SPI_CS_TIMING2);
	trans_sts_reg	= tegra_spi_cnt_readl(tspi, SPI_TRANS_STATUS);
	fifo_status_reg	= tegra_spi_cnt_readl(tspi, SPI_FIFO_STATUS);
	dma_ctrl_reg	= tegra_spi_cnt_readl(tspi, SPI_DMA_CTL);
	blk_size_reg	= tegra_spi_cnt_readl(tspi, SPI_DMA_BLK);
	intr_mask_reg	= tegra_spi_cnt_readl(tspi, SPI_INTR_MASK);
	misc_reg	= tegra_spi_cnt_readl(tspi, SPI_MISC);
	fatal_mask_reg	= tegra_spi_cnt_readl(tspi, SPI_FATAL_INTR_EN);
	if (tspi->chip_data->boundary_reg) {
		br_tout_reg	= tegra_spi_cnt_readl(tspi, SPI_CS_BOUNDARY_TIMEOUT);
		tout_fifo_reg	= tegra_spi_cnt_readl(tspi, SPI_TIMEOUT_BOUNDARY_FIFO_STATUS);
		debug_reg	= tegra_spi_cnt_readl(tspi, SPI_DEBUG_REGISTER);
	}
	dev_err(tspi->dev,
		"CMD_0: 0x%08x, FIFO_STS: 0x%08x\n",
		command1_reg, fifo_status_reg);
	dev_err(tspi->dev,
		"DMA_CTL: 0x%08x, TRANS_STS: 0x%08x\n",
		dma_ctrl_reg, trans_sts_reg);
	dev_err(tspi->dev,
		"CMD2: 0x%08x, BLK_SIZE: 0x%08x\n",
		command2_reg, blk_size_reg);
	dev_err(tspi->dev,
		"TMG1: 0x%08x, TMG2: 0x%08x\n",
		timing1_reg, timing2_reg);
	dev_err(tspi->dev,
		"INTRM: 0x%08x, MISC: 0x%08x\n",
		intr_mask_reg, misc_reg);
	dev_err(tspi->dev,
		"FATALM: 0x%08x, DEBUG: 0x%08x\n",
		fatal_mask_reg, debug_reg);
	dev_err(tspi->dev,
		"BR_TOUT: 0x%08x, TOUT_FIFO: 0x%08x\n",
		br_tout_reg, tout_fifo_reg);
}

static void tegra_spi_cnt_clear_status(struct tegra_spi_cnt_data *tspi)
{
/* TODO: Include all interrupts status */
	u32 val;

	/* Write 1 to clear status register */
	val = tegra_spi_cnt_readl(tspi, SPI_TRANS_STATUS);
	tegra_spi_cnt_writel(tspi, val, SPI_TRANS_STATUS);

	/* Clear fifo status error if any */
	val = tegra_spi_cnt_readl(tspi, SPI_FIFO_STATUS);
	tegra_spi_cnt_writel(tspi, val, SPI_FIFO_STATUS);
}

static void tegra_spi_cnt_set_intr_mask(struct tegra_spi_cnt_data *tspi)
{
	unsigned long intr_mask;

	/* Interrupts are disabled by default and need not be cleared
	 * in polling mode. Still writing to registers to be robust
	 * This step occurs only in case of system reset or
	 * resume or error case and not in data path affecting perf.
	 */
	intr_mask = tegra_spi_cnt_readl(tspi, SPI_INTR_MASK);
	intr_mask &= ~(SPI_INTR_ALL_MASK);
	tegra_spi_cnt_writel(tspi, intr_mask, SPI_INTR_MASK);
}

static int tegra_spi_cnt_clear_fifo(struct tegra_spi_cnt_data *tspi)
{
	unsigned long status;
	int cnt = SPI_FIFO_FLUSH_MAX_DELAY;

	/* Make sure that Rx and Tx fifo are empty */
	status = tspi->status_reg;
	if ((status & SPI_FIFO_EMPTY) != SPI_FIFO_EMPTY) {
		/* flush the fifo */
		status |= (SPI_RX_FIFO_FLUSH | SPI_TX_FIFO_FLUSH);
		tegra_spi_cnt_writel(tspi, status, SPI_FIFO_STATUS);
		do {
			status = tegra_spi_cnt_readl(tspi, SPI_FIFO_STATUS);
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

static void tegra_spi_reset_controller(struct tegra_spi_cnt_data *tspi)
{
	dmaengine_terminate_all(tspi->dma_chan);
	wmb(); /* barrier for dma terminate to happen */
	reset_control_reset(tspi->rstc);
	tegra_spi_cnt_writel(tspi, tspi->def_command1_reg, SPI_COMMAND1);
	tegra_spi_cnt_clear_status(tspi);
	tegra_spi_cnt_writel(tspi, tspi->dma_control_reg, SPI_DMA_CTL);
	tegra_spi_cnt_clear_fifo(tspi);
	tegra_spi_cnt_set_intr_mask(tspi);
}

static void dump_circ_buff(struct tspi_circ_buf *cq)
{
	pr_err("Buffer head=%d, tail=%d, size=%d\n", cq->head, cq->tail,
	       cq->size);
	pr_err("count = %d\tcount to end = %d\n",
	       CIRC_CNT(cq->head, cq->tail, cq->size),
	       CIRC_CNT_TO_END(cq->head, cq->tail, cq->size));
	pr_err("Space = %d\tSpace to end = %d\n",
	       CIRC_SPACE(cq->head, cq->tail, cq->size),
	       CIRC_SPACE_TO_END(cq->head, cq->tail, cq->size));
}

static int copy_from_circ_buffer(struct tspi_circ_buf *cq, void *buf, int len)
{
	uint32_t read_index, cnt_to_end;

	if (len > CIRC_CNT(cq->head, cq->tail, cq->size)) {
		pr_err("%s: invalid args\n", __func__);
		dump_circ_buff(cq);
		return -EINVAL;
	}

	read_index = cq->tail;
	cnt_to_end = CIRC_CNT_TO_END(cq->head, read_index, cq->size);
	if (cnt_to_end < len) {
		memcpy((u8 *)buf, (u8 *)cq->buf + read_index, cnt_to_end);
		memcpy((u8 *)buf, (u8 *)cq->buf, (len - cnt_to_end));
		cq->tail = cq->tail + len - cnt_to_end;
	} else {
		memcpy(buf, (u8 *)cq->buf + read_index, len);
		cq->tail = cq->tail + len;
	}

	return 0;
}

static int copy_to_circ_buffer(struct tspi_circ_buf *cq, const void *buf,
			       int len)
{
	uint32_t write_index, space_to_end;

	if (len > CIRC_SPACE(cq->head, cq->tail, cq->size)) {
		pr_err("%s: invalid args\n", __func__);
		dump_circ_buff(cq);
		return -EINVAL;
	}

	write_index = cq->head;
	space_to_end = CIRC_SPACE_TO_END(write_index, cq->tail, cq->size);
	if (space_to_end < len) {
		memcpy((u8 *)cq->buf + write_index, (u8 *)buf, space_to_end);
		memcpy((u8 *)cq->buf, (u8 *)buf, (len - space_to_end));
		cq->head = len - space_to_end;
	} else {
		memcpy((u8 *)cq->buf + write_index, (u8 *)buf, len);
		cq->head = cq->head + len;
	}

	return 0;
}

/* copy functions for circular buffer queue */
static int tegra_spi_cnt_copy_from_client(struct tegra_spi_cnt_data *tspi,
				      struct spi_transfer *t)
{
	return copy_to_circ_buffer(tspi->tspi_queue, t->tx_buf, t->len);
}

static int tegra_spi_cnt_copy_to_client(struct tegra_spi_cnt_data *tspi,
				    struct spi_transfer *t)
{
	int ret;

	ret = copy_from_circ_buffer(tspi->tspi_queue, t->rx_buf, t->len);
	dump_circ_buff(tspi->tspi_queue);

	return ret;
}

static void tegra_spi_cnt_copy_to_dmabuf(struct tegra_spi_cnt_data *tspi)
{
	copy_from_circ_buffer(tspi->tspi_queue, (u8 *)tspi->dma_buf + tspi->cur_dma_pos,
			    tspi->dma_period_size);
	tspi->cur_dma_pos += tspi->dma_period_size;
	if (tspi->cur_dma_pos == tspi->dma_buf_size)
		tspi->cur_dma_pos = 0;
}

static void tegra_spi_cnt_copy_from_dmabuf(struct tegra_spi_cnt_data *tspi, u32 len)
{
	u32 count, rem_len;

	rem_len = len;
	if (rem_len > 0) {
		while (rem_len > 0) {
			count = min(rem_len, (tspi->dma_buf_size - tspi->cur_dma_pos));
			copy_to_circ_buffer(tspi->tspi_queue,
				(u8 *)tspi->dma_buf + tspi->cur_dma_pos, count);
			tspi->cur_dma_pos += count;
			if (tspi->cur_dma_pos > tspi->dma_buf_size)
				tspi->cur_dma_pos %= tspi->dma_buf_size;
			rem_len = rem_len - count;
		}
		dump_circ_buff(tspi->tspi_queue);
	}
}

static u32 get_timeout_fifo_count(struct tegra_spi_cnt_data *tspi)
{
	u32 fifo_count, fifo_status;

	fifo_status = tegra_spi_cnt_readl(tspi, SPI_TIMEOUT_BOUNDARY_FIFO_STATUS);
	fifo_count = SPI_TIMEOUT_FIFO_FULL_COUNT(fifo_status);
	return fifo_count;
}

static void tegra_spi_cnt_update_pad(struct tegra_spi_cnt_data *tspi)
{
	unsigned int intr_mask;

	if (tspi->next_pad)
		return;
	if (get_timeout_fifo_count(tspi)) {
		intr_mask = tegra_spi_cnt_readl(tspi, SPI_INTR_MASK);
		intr_mask |= SPI_INTR_CS_BOUNDARY_TIMEOUT_INTR_MASK;
		tegra_spi_cnt_writel(tspi, intr_mask, SPI_INTR_MASK);
	} else {
		//clear cs_boundary_timeout interrupt
		intr_mask = tegra_spi_cnt_readl(tspi, SPI_INTR_MASK);
		intr_mask &= ~(SPI_INTR_CS_BOUNDARY_TIMEOUT_INTR_MASK);
		tegra_spi_cnt_writel(tspi, intr_mask, SPI_INTR_MASK);
		return;
	}
}

static void tegra_spi_cnt_handle_padding(struct tegra_spi_cnt_data *tspi)
{
	unsigned int rem_len;
	unsigned int target_len;
	u32 len;

	target_len = tspi->consumed_data + tspi->dma_period_size;
	if (tspi->next_pad > target_len) {
		tegra_spi_cnt_copy_from_dmabuf(tspi, tspi->dma_period_size);
		tspi->consumed_data += tspi->dma_period_size;
		return;
	}

	rem_len = tspi->dma_period_size;
	if (rem_len > 0) {
		while (rem_len > 0) {
			if (!tspi->next_pad)
				len = rem_len;
			else if (tspi->next_pad > target_len)
				len = rem_len;
			else
				len = rem_len;

			tegra_spi_cnt_copy_from_dmabuf(tspi, len);
			rem_len = rem_len - len;
			tspi->cur_dma_pos += tspi->next_pad_count;
			if (tspi->cur_dma_pos > tspi->dma_buf_size)
				tspi->cur_dma_pos = tspi->cur_dma_pos - tspi->dma_buf_size;
			tspi->next_pad = 0;
			tspi->next_pad_count = 0;
			tegra_spi_cnt_update_pad(tspi);
		}
		tspi->consumed_data += tspi->dma_period_size;
	}
}

static void tegra_spi_cnt_dma_complete(void *args)
{
	struct tegra_spi_cnt_data *tspi = args;

	if (!tspi->chip_data->boundary_reg) {
		tegra_spi_cnt_copy_from_dmabuf(tspi, tspi->dma_period_size);
		tspi->consumed_data += tspi->dma_period_size;
	} else {
		if (tspi->cur_direction & DATA_DIR_RX)
			tegra_spi_cnt_handle_padding(tspi);
	}
}

static int tegra_spi_cnt_start_dma(struct tegra_spi_cnt_data *tspi, int len)
{
	unsigned int direction;

	if (tspi->cur_direction & DATA_DIR_RX)
		direction = DMA_DEV_TO_MEM;
	else
		direction = DMA_MEM_TO_DEV;

	reinit_completion(&tspi->dma_complete);
	dev_dbg(tspi->dev, "%s DMA buffer length =%d period=%d\n", __func__,
			len, tspi->dma_period_size);
	tspi->dma_desc = dmaengine_prep_dma_cyclic(tspi->dma_chan,
				tspi->dma_phys, len, tspi->dma_period_size,
				direction, DMA_PREP_INTERRUPT);
	if (!tspi->dma_desc) {
		dev_err(tspi->dev, "Not able to get dma desc\n");
		return -EIO;
	}

	tspi->dma_desc->callback = tegra_spi_cnt_dma_complete;
	tspi->dma_desc->callback_param = tspi;

	dmaengine_submit(tspi->dma_desc);
	dma_async_issue_pending(tspi->dma_chan);

	return 0;
}

static int tegra_spi_cnt_start_dma_based_transfer(struct tegra_spi_cnt_data *tspi)
{
	unsigned long val;
	int ret = 0;
	struct dma_slave_config dma_sconfig;

	val = SPI_RX_TRIG_16 | SPI_TX_TRIG_16;
	val |= (SPI_CONT | SPI_PAUSE);
	tegra_spi_cnt_writel(tspi, val, SPI_DMA_CTL);
	tspi->dma_control_reg = val;

	if (tspi->cur_direction & DATA_DIR_TX) {
		dma_sconfig.dst_addr = tspi->phys + SPI_TX_FIFO;
		dma_sconfig.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		dma_sconfig.dst_maxburst = 16;
		dmaengine_slave_config(tspi->dma_chan, &dma_sconfig);

		tegra_spi_cnt_copy_to_dmabuf(tspi);
		ret = tegra_spi_cnt_start_dma(tspi, tspi->dma_buf_size);
		if (ret < 0) {
			dev_err(tspi->dev,
				"Starting tx dma failed, err %d\n", ret);
			return ret;
		}
	}

	if (tspi->cur_direction & DATA_DIR_RX) {
		/* Make the dma buffer to read by dma */
		dma_sync_single_for_device(tspi->dev, tspi->dma_phys,
				tspi->dma_buf_size, DMA_FROM_DEVICE);
		dma_sconfig.src_addr = tspi->phys + SPI_RX_FIFO;
		dma_sconfig.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		dma_sconfig.src_maxburst = 16;
		dmaengine_slave_config(tspi->dma_chan, &dma_sconfig);

		ret = tegra_spi_cnt_start_dma(tspi, tspi->dma_buf_size);
		if (ret < 0) {
			dev_err(tspi->dev,
				"Starting rx dma failed, err %d\n", ret);
			if (tspi->cur_direction & DATA_DIR_RX)
				dmaengine_terminate_all(tspi->dma_chan);
			return ret;
		}
	}

	val |= SPI_DMA_EN;
	tegra_spi_cnt_writel(tspi, val, SPI_DMA_CTL);

	val = tegra_spi_cnt_readl(tspi, SPI_MISC);
	val |= SPI_MISC_EXT_CLK_EN;
	tegra_spi_cnt_writel(tspi, val, SPI_MISC);

	return ret;
}

static void tegra_spi_cnt_stop_dma(struct tegra_spi_cnt_data *tspi)
{
	u32 val;

	val = tspi->dma_control_reg;
	val &= ~SPI_DMA_EN;
	tegra_spi_cnt_writel(tspi, val, SPI_DMA_CTL);
	tegra_spi_reset_controller(tspi);
}

static void tegra_spi_cnt_deinit_dma_param(struct tegra_spi_cnt_data *tspi)
{
	if (!tspi->dma_chan)
		return;
	tspi->dma_chan = NULL;
	tspi->dma_buf = NULL;

	dma_free_coherent(tspi->dev, tspi->dma_buf_size, tspi->dma_buf, tspi->dma_phys);
	dma_release_channel(tspi->dma_chan);
}

static int tegra_spi_cnt_init_dma_param(struct tegra_spi_cnt_data *tspi)
{
	struct dma_chan *dma_chan;
	u32 *dma_buf;
	dma_addr_t dma_phys;
	int ret;
	struct dma_slave_config dma_sconfig;

	dma_chan = dma_request_chan(tspi->dev, "rx");
	if (IS_ERR(dma_chan)) {
		ret = PTR_ERR(dma_chan);
		if (ret != -EPROBE_DEFER)
			dev_err(tspi->dev,
				"Dma channel is not available: %d\n", ret);
		return ret;
	}

	dma_buf = dma_alloc_coherent(tspi->dev, tspi->dma_buf_size,
				&dma_phys, GFP_KERNEL);
	if (!dma_buf) {
		dev_err(tspi->dev, " Not able to allocate the dma buffer\n");
		dma_release_channel(dma_chan);
		return -ENOMEM;
	}

	if (tspi->cur_direction) {
		dma_sconfig.src_addr = tspi->phys + SPI_RX_FIFO;
		dma_sconfig.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		dma_sconfig.src_maxburst = 0;
	} else {
		dma_sconfig.dst_addr = tspi->phys + SPI_TX_FIFO;
		dma_sconfig.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		dma_sconfig.dst_maxburst = 0;
	}

	ret = dmaengine_slave_config(dma_chan, &dma_sconfig);
	if (ret)
		goto scrub;
	tspi->dma_chan = dma_chan;
	tspi->dma_buf = dma_buf;
	tspi->dma_phys = dma_phys;

	return 0;

scrub:
	dma_free_coherent(tspi->dev, tspi->dma_buf_size, dma_buf, dma_phys);
	dma_release_channel(dma_chan);
	return ret;
}

static void set_best_clk_source(struct tegra_spi_cnt_data *tspi,
				unsigned long rate)
{
	long new_rate;
	unsigned long err_rate, crate, prate;
	unsigned int cdiv, fin_err = rate;
	int ret;
	struct clk *pclk, *fpclk = NULL;
	const char *pclk_name, *fpclk_name;
	struct device_node *node;
	struct property *prop;

	node = tspi->master->dev.of_node;
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
			crate = tspi->master->max_speed_hz;
			cdiv = DIV_ROUND_UP(prate, crate);
			if (cdiv > tspi->min_div)
				tspi->min_div = cdiv;
		}
	}

	pclk = clk_get_parent(tspi->clk);
	crate = clk_get_rate(tspi->clk);
	if (!crate)
		return;
	prate = clk_get_rate(pclk);
	cdiv = DIV_ROUND_UP(prate, crate);
	if (cdiv < tspi->min_div) {
		crate = DIV_ROUND_UP(prate, tspi->min_div);
		clk_set_rate(tspi->clk, crate);
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
		dev_err(tspi->dev, "Setting clk_src %s\n",
			fpclk_name);
		clk_set_parent(tspi->clk, fpclk);
	}
}

static int tegra_spi_cnt_set_clock_rate(struct tegra_spi_cnt_data *tspi, u32 speed)
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

static int tegra_spi_cnt_setup_transfer_one(struct spi_device *spi,
					struct spi_transfer *t)
{
	struct tegra_spi_cnt_data *tspi = spi_master_get_devdata(spi->master);
	u32 speed = t->speed_hz;
	u8 bits_per_word = t->bits_per_word;
	u32 command1;
	int req_mode;
	int ret;

	ret = tegra_spi_cnt_set_clock_rate(tspi, speed);
	if (ret < 0)
		return ret;

	tspi->cur_spi = spi;
	tspi->cur_dma_pos = 0;
	tspi->cur_pad_pos = 0;
	tspi->tx_status = 0;
	tspi->rx_status = 0;
	tspi->curr_xfer = t;

	tspi->status_reg = tegra_spi_cnt_readl(tspi, SPI_FIFO_STATUS);
	tegra_spi_cnt_clear_status(tspi);

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

	if ((tspi->cur_direction & DATA_DIR_TX) &&
	    ((req_mode == SPI_MODE_0) || (req_mode == SPI_MODE_2)))
		return -EINVAL;

	if (spi->mode & SPI_LSB_FIRST)
		command1 |= SPI_LSBYTE_FE;
	else
		command1 &= ~SPI_LSBYTE_FE;

	if (spi->mode & SPI_LSB_FIRST)
		command1 |= SPI_LSBIT_FE;
	else
		command1 &= ~SPI_LSBIT_FE;

	command1 &= ~SPI_BIDIROE;
	command1 &= ~SPI_CS_SW_HW;
	command1 &= ~SPI_CS_SS_VAL;

	tegra_spi_cnt_writel(tspi, command1, SPI_COMMAND1);

	if (bits_per_word == 8 || bits_per_word == 16 ||
	    bits_per_word == 32)
		command1 |= SPI_PACKED;
	else
		return -EINVAL;

	command1 &= ~(SPI_CS_SEL_MASK | SPI_TX_EN | SPI_RX_EN);
	if (tspi->cur_direction == DATA_DIR_RX)
		command1 |= SPI_RX_EN;
	else if (tspi->cur_direction == DATA_DIR_TX)
		command1 |= SPI_TX_EN;
	else
		return -EINVAL;

	command1 |= SPI_CS_SEL(spi->chip_select);
	tegra_spi_cnt_writel(tspi, command1, SPI_COMMAND1);
	tspi->command1_reg = command1;

	if (tspi->chip_data->boundary_reg) {
		if (tspi->raw_data)
			tegra_spi_cnt_writel(tspi, 0xFFFFFFFF,
					     SPI_CS_BOUNDARY_TIMEOUT);
		else
			tegra_spi_cnt_writel(tspi, 0x3FF,
					     SPI_CS_BOUNDARY_TIMEOUT);
	}

	dev_dbg(tspi->dev, "The def 0x%x and written 0x%x\n",
		tspi->def_command1_reg, (unsigned int)command1);

	return 0;
}

static void tegra_spi_cnt_slave_transfer(struct work_struct *work)
{
	struct tegra_spi_cnt_data *tspi = container_of(work, struct tegra_spi_cnt_data,
						   transfer_work);

	tegra_spi_cnt_start_dma_based_transfer(tspi);
}

static int tegra_spi_cnt_slave_start_controller(struct spi_device *spi, struct spi_transfer *t)
{
	struct tegra_spi_cnt_data *tspi = spi_master_get_devdata(spi->master);

	tspi->next_pad = 0;
	tspi->next_pad_count = 0;
	tspi->consumed_data = 0;
	tspi->produced_data = 0;

	tegra_spi_cnt_setup_transfer_one(spi, t);
	schedule_work(&tspi->transfer_work);

	return 0;
}

static void tegra_spi_cnt_slave_stop_controller(struct spi_device *spi)
{
	struct tegra_spi_cnt_data *tspi = spi_master_get_devdata(spi->master);

	tegra_spi_cnt_stop_dma(tspi);
	tspi->tspi_queue->head = 0;
	tspi->tspi_queue->tail = 0;
}

static int tegra_spi_cnt_slave_write_request(struct spi_device *spi, struct spi_transfer *t)
{
	struct tegra_spi_cnt_data *tspi = spi_master_get_devdata(spi->master);

	return tegra_spi_cnt_copy_from_client(tspi, t);
}

static int tegra_spi_cnt_slave_read_request(struct spi_device *spi, struct spi_transfer *t)
{
	struct tegra_spi_cnt_data *tspi = spi_master_get_devdata(spi->master);

	return tegra_spi_cnt_copy_to_client(tspi, t);
}

static int tegra_spi_cnt_transfer_one_message(struct spi_controller *master,
			struct spi_message *msg)
{
	struct tegra_spi_cnt_data *tspi = spi_master_get_devdata(master);
	struct spi_transfer *xfer;
	struct spi_device *spi = msg->spi;
	int ret = -EIO;

	list_for_each_entry(xfer, &msg->transfers, transfer_list) {
		if (tspi->cur_direction & DATA_DIR_TX)
			ret = tegra_spi_cnt_slave_write_request(spi, xfer);
		if (tspi->cur_direction & DATA_DIR_RX)
			ret = tegra_spi_cnt_slave_read_request(spi, xfer);
		if (!ret)
			msg->actual_length += xfer->len;
		else
			dev_err(tspi->dev, "msg xfer failed %d", ret);
	}
	msg->status = ret;
	spi_finalize_current_message(master);

	return 0;
}

static irqreturn_t tegra_spi_cnt_isr_thread(int irq, void *context_data);
static irqreturn_t tegra_spi_cnt_isr(int irq, void *context_data);


static struct tegra_spi_cnt_chip_data tegra234_spi_cnt_chip_data = {
	.boundary_reg = true,
};

static struct tegra_spi_cnt_chip_data tegra194_spi_cnt_chip_data = {
	.boundary_reg = true,
};

static struct tegra_spi_cnt_chip_data tegra186_spi_cnt_chip_data = {
	.boundary_reg = false,
};

static const struct of_device_id tegra_spi_cnt_of_match[] = {
	{
		.compatible = "nvidia,tegra194-spi-slave-cnt-mode",
		.data       = &tegra194_spi_cnt_chip_data,
	}, {
		.compatible = "nvidia,tegra186-spi-slave-cnt-mode",
		.data       = &tegra186_spi_cnt_chip_data,
	}, {
		.compatible = "nvidia,tegra234-spi-slave-cnt-mode",
		.data       = &tegra234_spi_cnt_chip_data,
	},
	{}
};
MODULE_DEVICE_TABLE(of, tegra_spi_cnt_of_match);

static struct tegra_spi_device_controller_data
	*tegra_spi_cnt_get_cdata_dt(struct spi_device *spi)
{
	struct tegra_spi_device_controller_data *cdata;
	struct device_node *slave_np, *data_np;
	int ret;

	slave_np = spi->dev.of_node;
	if (!slave_np) {
		dev_err(&spi->dev, "device node not found\n");
		return NULL;
	}

	data_np = of_get_child_by_name(slave_np, "controller-data");
	if (!data_np) {
		dev_err(&spi->dev, "child node 'controller-data' not found\n");
		return NULL;
	}

	cdata = kzalloc(sizeof(*cdata), GFP_KERNEL);
	if (!cdata) {
		of_node_put(data_np);
		return NULL;
	}

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

	of_node_put(data_np);

	return cdata;
}

static void tegra_spi_cnt_parse_dt(struct tegra_spi_cnt_data *tspi)
{
	struct device_node *np = tspi->dev->of_node;
	int ret;

	ret = of_property_read_bool(np, "nvidia,raw_data");
	if (ret)
		tspi->raw_data = 0;

	if (of_find_property(np, "nvidia,clock-always-on", NULL))
		tspi->clock_always_on = true;

	if (of_find_property(np, "nvidia,tx-mode", NULL))
		tspi->cur_direction |= DATA_DIR_TX;
	else
		tspi->cur_direction |= DATA_DIR_RX;

	if (of_property_read_u32(np, "spi-max-frequency",
				 &tspi->master->max_speed_hz))
		tspi->master->max_speed_hz = 25000000; /* 25MHz */

	if (of_property_read_u32(np, "nvidia,maximum-dma-buffer-size",
				 &tspi->dma_buf_size))
		tspi->dma_buf_size = DEFAULT_SPI_DMA_BUF_LEN;
	if (of_property_read_u32(np, "nvidia,dma-period-size",
				 &tspi->dma_period_size))
		tspi->dma_period_size = DEFAULT_SPI_DMA_PERIOD_LEN;
}

static int tegra_spi_cnt_setup(struct spi_device *spi)
{
	struct tegra_spi_cnt_data *tspi = spi_master_get_devdata(spi->master);
	struct tegra_spi_device_controller_data *cdata = spi->controller_data;
	u32 val;
	unsigned long flags;
	int ret;

	dev_dbg(&spi->dev, "setup %d bpw, %scpol, %scpha, %dHz\n",
		spi->bits_per_word,
		spi->mode & SPI_CPOL ? "" : "~",
		spi->mode & SPI_CPHA ? "" : "~",
		spi->max_speed_hz);

	if (!cdata) {
		cdata = tegra_spi_cnt_get_cdata_dt(spi);
		spi->controller_data = cdata;
	}

	ret = pm_runtime_get_sync(tspi->dev);
	if (ret < 0) {
		dev_err(tspi->dev, "pm runtime failed, e = %d\n", ret);
		return ret;
	}
	spin_lock_irqsave(&tspi->lock, flags);
	val = tspi->def_command1_reg;
	if (spi->mode & SPI_CS_HIGH)
		val &= ~SPI_CS_POL_INACTIVE(spi->chip_select);
	else
		val |= SPI_CS_POL_INACTIVE(spi->chip_select);
	val |= SPI_MODE_SEL(spi->mode & 0x3);
	tspi->def_command1_reg = val;
	spin_unlock_irqrestore(&tspi->lock, flags);

	tspi->test_device = spi;

	pm_runtime_put(tspi->dev);
	return 0;
}

static void tegra_spi_cnt_set_slcg(struct tegra_spi_cnt_data *tspi)
{
	int reg;

	reg = tegra_spi_cnt_readl(tspi, SPI_MISC);
	reg &= ~SPI_MISC_CLKEN_OVERRIDE;
	tegra_spi_cnt_writel(tspi, reg, SPI_MISC);
}

static int tegra_spi_cnt_runtime_suspend(struct device *dev);
static int tegra_spi_cnt_runtime_resume(struct device *dev);

static int tegra_spi_cnt_probe(struct platform_device *pdev)
{
	struct spi_controller	*master;
	struct tegra_spi_cnt_data	*tspi;
	struct resource		*r;
	int ret, spi_irq;
	int bus_num;

	master = spi_alloc_master(&pdev->dev, sizeof(*tspi));
	if (!master) {
		dev_err(&pdev->dev, "master allocation failed\n");
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, master);
	tspi = spi_master_get_devdata(master);

	/* the spi->mode bits understood by this driver: */
	master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH | SPI_LSB_FIRST;
	master->bits_per_word_mask = SPI_BPW_MASK(32) | SPI_BPW_MASK(16) |
				     SPI_BPW_MASK(8);
	master->setup = tegra_spi_cnt_setup;
	master->transfer_one_message = tegra_spi_cnt_transfer_one_message;
	master->stop_controller = tegra_spi_cnt_slave_stop_controller;
	master->start_controller = tegra_spi_cnt_slave_start_controller;
	master->num_chipselect = MAX_CHIP_SELECT;
	bus_num = of_alias_get_id(pdev->dev.of_node, "spi");
	if (bus_num >= 0)
		master->bus_num = bus_num;
	master->auto_runtime_pm = true;

	tspi->master = master;
	tspi->dev = &pdev->dev;

	spin_lock_init(&tspi->lock);
	tspi->chip_data = of_device_get_match_data(&pdev->dev);
	if (!tspi->chip_data) {
		dev_err(&pdev->dev, "Unsupported chip. Exiting\n");
		ret = -ENODEV;
		goto exit_free_master;
	}

	tegra_spi_cnt_parse_dt(tspi);

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	tspi->base = devm_ioremap_resource(&pdev->dev, r);
	if (IS_ERR(tspi->base)) {
		ret = PTR_ERR(tspi->base);
		goto exit_free_master;
	}
	tspi->phys = r->start;

	spi_irq = platform_get_irq(pdev, 0);
	tspi->irq = spi_irq;

	tspi->clk = devm_clk_get(&pdev->dev, "spi");
	if (IS_ERR(tspi->clk)) {
		dev_err(&pdev->dev, "can not get clock\n");
		ret = PTR_ERR(tspi->clk);
		goto exit_free_master;
	}

	tspi->rstc = devm_reset_control_get(&pdev->dev, "spi");
	if (IS_ERR(tspi->rstc)) {
		dev_err(&pdev->dev, "can not get reset\n");
		ret = PTR_ERR(tspi->rstc);
		goto exit_free_master;
	}

	tspi->min_div = 0;

	ret = tegra_spi_cnt_init_dma_param(tspi);
	if (ret < 0)
		goto exit_free_master;

	init_completion(&tspi->dma_complete);
	init_completion(&tspi->xfer_completion);
	INIT_WORK(&tspi->transfer_work, tegra_spi_cnt_slave_transfer);

	tspi->tspi_queue = kzalloc(sizeof(*tspi->tspi_queue), GFP_KERNEL);
	tspi->tspi_queue->buf = kzalloc(tspi->dma_buf_size, GFP_KERNEL);
	if (!tspi->tspi_queue->buf) {
		dev_err(tspi->dev, "circular buffer allocation failed");
		goto exit_free_master;
	}
	tspi->tspi_queue->size = tspi->dma_buf_size;
	tspi->tspi_queue->head = 0;
	tspi->tspi_queue->tail = 0;

	/* slcg supported on chips supporting continuous pause mode */
	tspi->clock_always_on = true;

	if (tspi->clock_always_on) {
		ret = clk_prepare_enable(tspi->clk);
		if (ret < 0) {
			dev_err(tspi->dev, "clk_prepare failed: %d\n", ret);
			goto exit_dma_free;
		}
	}

	pm_runtime_enable(&pdev->dev);
	if (!pm_runtime_enabled(&pdev->dev)) {
		ret = tegra_spi_cnt_runtime_resume(&pdev->dev);
		if (ret)
			goto exit_pm_disable;
	}

	ret = pm_runtime_get_sync(&pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "pm runtime get failed, e = %d\n", ret);
		goto exit_pm_disable;
	}

	reset_control_reset(tspi->rstc);

	tspi->def_command1_reg  = SPI_LSBYTE_FE;
	tspi->def_command1_reg |= SPI_CS_SEL(0);
	tegra_spi_cnt_writel(tspi, tspi->def_command1_reg, SPI_COMMAND1);
	tspi->command2_reg = tegra_spi_cnt_readl(tspi, SPI_COMMAND2);
	tegra_spi_cnt_set_slcg(tspi);
	pm_runtime_put(&pdev->dev);

	ret = request_threaded_irq(tspi->irq, tegra_spi_cnt_isr,
				   tegra_spi_cnt_isr_thread, IRQF_ONESHOT,
				   dev_name(&pdev->dev), tspi);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register ISR for IRQ %d\n",
			tspi->irq);
		goto exit_pm_disable;
	}

	master->dev.of_node = pdev->dev.of_node;
	ret = devm_spi_register_master(&pdev->dev, master);
	if (ret < 0) {
		dev_err(&pdev->dev, "can not register to master err %d\n", ret);
		goto exit_free_irq;
	}

	return ret;

exit_free_irq:
	free_irq(spi_irq, tspi);
exit_pm_disable:
	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra_spi_cnt_runtime_suspend(&pdev->dev);
	if (tspi->clock_always_on)
		clk_disable_unprepare(tspi->clk);
exit_dma_free:
	kfree(tspi->tspi_queue->buf);
	tegra_spi_cnt_deinit_dma_param(tspi);
exit_free_master:
	spi_master_put(master);
	return ret;
}

static int tegra_spi_cnt_remove(struct platform_device *pdev)
{
	struct spi_controller *master = platform_get_drvdata(pdev);
	struct tegra_spi_cnt_data	*tspi = spi_master_get_devdata(master);

	free_irq(tspi->irq, tspi);

	spi_unregister_master(master);

	if (tspi->dma_chan)
		tegra_spi_cnt_deinit_dma_param(tspi);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra_spi_cnt_runtime_suspend(&pdev->dev);

	if (tspi->clock_always_on)
		clk_disable_unprepare(tspi->clk);

	return 0;
}

static irqreturn_t handle_dma_based_xfer(struct tegra_spi_cnt_data *tspi)
{
	int err = 0;
	unsigned long flags;

	dev_err(tspi->dev, "IN %s[%d]", __func__, __LINE__);
	/* Abort dmas if any error */
	if (tspi->cur_direction & DATA_DIR_TX) {
		if (tspi->tx_status) {
			dmaengine_terminate_all(tspi->dma_chan);
			err += 1;
		}
	}

	if (tspi->cur_direction & DATA_DIR_RX) {
		if (tspi->rx_status) {
			dmaengine_terminate_all(tspi->dma_chan);
			err += 2;
		}
	}

	spin_lock_irqsave(&tspi->lock, flags);
	if (err) {
		dev_err(tspi->dev, "DmaXfer: ERROR bit set 0x%x\n",
			tspi->status_reg);
		dev_err(tspi->dev, "DmaXfer 0x%08x:0x%08x\n",
			tspi->command1_reg, tspi->dma_control_reg);
		complete(&tspi->xfer_completion);
		spin_unlock_irqrestore(&tspi->lock, flags);
		tegra_spi_cnt_dump_regs(tspi);
		tegra_spi_reset_controller(tspi);
		return IRQ_HANDLED;
	}

	spin_unlock_irqrestore(&tspi->lock, flags);
	return IRQ_HANDLED;
}

static irqreturn_t tegra_spi_cnt_isr_thread(int irq, void *context_data)
{
	struct tegra_spi_cnt_data *tspi = context_data;

	return handle_dma_based_xfer(tspi);
}

static irqreturn_t tegra_spi_cnt_isr(int irq, void *context_data)
{
	struct tegra_spi_cnt_data *tspi = context_data;

	tspi->status_reg = tegra_spi_cnt_readl(tspi, SPI_FIFO_STATUS);
	if (tspi->chip_data->boundary_reg) {
		if (tspi->status_reg & SPI_CS_BOUNDARY_TIMEOUT_INTR)
			tegra_spi_cnt_update_pad(tspi);

		tspi->timeout_reg = tegra_spi_cnt_readl(tspi,
				    SPI_TIMEOUT_BOUNDARY_FIFO_STATUS);
	}
	if (tspi->cur_direction & DATA_DIR_TX)
		tspi->tx_status = tspi->status_reg & (SPI_TX_FIFO_UNF |
						      SPI_TX_FIFO_OVF |
						      SPI_FRAME_END);

	if (tspi->cur_direction & DATA_DIR_RX)
		tspi->rx_status = tspi->status_reg & (SPI_RX_FIFO_OVF |
						      SPI_RX_FIFO_UNF |
						      SPI_FRAME_END);
	tegra_spi_cnt_clear_status(tspi);

	return IRQ_WAKE_THREAD;
}

#ifdef CONFIG_PM_SLEEP
static int tegra_spi_cnt_suspend(struct device *dev)
{
	struct spi_controller *master = dev_get_drvdata(dev);
	struct tegra_spi_cnt_data *tspi = spi_master_get_devdata(master);
	int ret;

	ret = spi_master_suspend(master);

	if (tspi->clock_always_on)
		clk_disable_unprepare(tspi->clk);

	return ret;
}

static int tegra_spi_cnt_resume(struct device *dev)
{
	struct spi_controller *master = dev_get_drvdata(dev);
	struct tegra_spi_cnt_data *tspi = spi_master_get_devdata(master);
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
		dev_err(dev, "pm runtime failed, e = %d\n", ret);
		return ret;
	}
	tegra_spi_cnt_writel(tspi, tspi->command1_reg, SPI_COMMAND1);
	tegra_spi_cnt_set_intr_mask(tspi);
	tegra_spi_cnt_set_slcg(tspi);
	pm_runtime_put(dev);
	ret = spi_master_resume(master);

	return ret;
}
#endif

static int tegra_spi_cnt_runtime_suspend(struct device *dev)
{
	struct spi_controller *master = dev_get_drvdata(dev);
	struct tegra_spi_cnt_data *tspi = spi_master_get_devdata(master);

	/* Flush all write which are in PPSB queue by reading back */
	tegra_spi_cnt_readl(tspi, SPI_COMMAND1);

	if (!tspi->clock_always_on)
		clk_disable_unprepare(tspi->clk);
	return 0;
}

static int tegra_spi_cnt_runtime_resume(struct device *dev)
{
	struct spi_controller *master = dev_get_drvdata(dev);
	struct tegra_spi_cnt_data *tspi = spi_master_get_devdata(master);
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

static const struct dev_pm_ops tegra_spi_cnt_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra_spi_cnt_runtime_suspend,
		tegra_spi_cnt_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(tegra_spi_cnt_suspend, tegra_spi_cnt_resume)
};
static struct platform_driver tegra_spi_cnt_driver = {
	.driver = {
		.name		= "spi-tegra194-slave",
		.owner		= THIS_MODULE,
		.pm		= &tegra_spi_cnt_pm_ops,
		.of_match_table	= of_match_ptr(tegra_spi_cnt_of_match),
	},
	.probe =	tegra_spi_cnt_probe,
	.remove =	tegra_spi_cnt_remove,
};
module_platform_driver(tegra_spi_cnt_driver);

MODULE_ALIAS("platform:spi-tegra194");
MODULE_DESCRIPTION("NVIDIA Tegra194 SPI Controller Driver");
MODULE_AUTHOR("Krishna Yarlagadda <kyarlagadda@nvidia.com> Ashutosh Patel <ashutoshp@nvidia.com>");
MODULE_LICENSE("GPL v2");
