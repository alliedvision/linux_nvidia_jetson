/*
 *
 * If you received this File from Marvell, you may opt to use, redistribute and/or
 * modify this File in accordance with the terms and conditions of the General
 * Public License Version 2, June 1991 (the "GPL License"), a copy of which is
 * available along with the File in the license.txt file or by writing to the Free
 * Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or
 * on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
 * DISCLAIMED. The GPL License provides additional details about this warranty
 * disclaimer.
 *
 */
#ifndef H_OAK_UNIMAC
#define H_OAK_UNIMAC

#include "linux/etherdevice.h" /* Include for relation to classifier linux/etherdevice */
#include "linux/pci.h" /* Include for relation to classifier linux/pci */
#include "oak_gicu.h" /* Include for relation to classifier oak_gicu */
#include "oak_channel_stat.h" /* Include for relation to classifier oak_channel_stat */
#include "oak_unimac_stat.h" /* Include for relation to classifier oak_unimac_stat */
#include "oak_unimac_desc.h" /* Include for relation to classifier oak_unimac_desc */
#include "oak_ioc_reg.h" /* Include for relation to classifier oak_ioc_reg */
#include "oak_ioc_lgen.h" /* Include for relation to classifier oak_ioc_lgen */
#include "oak_ioc_stat.h" /* Include for relation to classifier oak_ioc_stat */
#include "oak_ioc_set.h" /* Include for relation to classifier oak_ioc_set */
#include "oak_ioc_flow.h" /* Include for relation to classifier oak_ioc_flow */
#include "oak_irq.h" /* Include for relation to classifier oak_irq */

#define OAK_REVISION_B0 1

#define OAK_PCIE_REGOFF_UNIMAC 0x00050000

#define OAK_UNI_DMA_RING_BASE (OAK_PCIE_REGOFF_UNIMAC + 0x00000000)

#define OAK_UNI_DMA_TXCH_BASE (OAK_PCIE_REGOFF_UNIMAC + 0x00010000)

#define OAK_UNI_DMA_RXCH_BASE (OAK_PCIE_REGOFF_UNIMAC + 0x00011000)

#define OAK_UNI_DMA_GLOB_BASE (OAK_PCIE_REGOFF_UNIMAC + 0x00012000)

#define OAK_UNI_GLOBAL(o) (OAK_UNI_DMA_GLOB_BASE + (o))

#define OAK_UNI_DMA_TXCH_OFFS(o) (OAK_UNI_DMA_TXCH_BASE + (o))

#define OAK_UNI_DMA_RXCH_OFFS(o) (OAK_UNI_DMA_RXCH_BASE + (o))

#define OAK_UNI_DMA_RING_TX(r, o) (OAK_UNI_DMA_RING_BASE + 0x0000 + 0x1000 * (r) + (o))

#define OAK_UNI_DMA_RING_RX(r, o) (OAK_UNI_DMA_RING_BASE + 0x0800 + 0x1000 * (r) + (o))

#define OAK_UNI_CFG_0 OAK_UNI_GLOBAL(0x00)

#define OAK_UNI_CFG_1 OAK_UNI_GLOBAL(0x04)

#define OAK_UNI_CTRL OAK_UNI_GLOBAL(0x10)

#define OAK_UNI_STAT OAK_UNI_GLOBAL(0x14)

#define OAK_UNI_INTR OAK_UNI_GLOBAL(0x18)

#define OAK_UNI_IMSK OAK_UNI_GLOBAL(0x1C)

#define OAK_UNI_RXEN OAK_UNI_GLOBAL(0x40)

#define OAK_UNI_TXEN OAK_UNI_GLOBAL(0x44)

#define OAK_UNI_STAT_TX_WD_HISTORY (1 << 7)

#define OAK_UNI_STAT_TX_WD_EVENT (1 << 6)

#define OAK_UNI_STAT_RX_WD_HISTORY (1 << 5)

#define OAK_UNI_STAT_RX_WD_EVENT (1 << 4)

#define OAK_UNI_STAT_RX_FIFO_EMPTY (1 << 3)

#define OAK_UNI_STAT_RX_FIFO_FULL (1 << 2)

#define OAK_UNI_STAT_TX_FIFO_EMPTY (1 << 1)

#define OAK_UNI_STAT_TX_FIFO_FULL (1 << 0)

#define OAK_UNI_INTR_RX_STAT_MEM_UCE (1 << 27)

#define OAK_UNI_INTR_RX_DESC_MEM_UCE (1 << 26)

#define OAK_UNI_INTR_TX_DESC_MEM_UCE (1 << 25)

#define OAK_UNI_INTR_AXI_WR_MEM_UCE (1 << 24)

#define OAK_UNI_INTR_TX_DATA_MEM_UCE (1 << 23)

#define OAK_UNI_INTR_TX_FIFO_MEM_UCS (1 << 22)

#define OAK_UNI_INTR_RX_FIFO_MEM_UCS (1 << 21)

#define OAK_UNI_INTR_HCS_MEM_UCE (1 << 20)

#define OAK_UNI_INTR_RX_STAT_MEM_CE (1 << 19)

#define OAK_UNI_INTR_RX_DESC_MEM_CE (1 << 18)

#define OAK_UNI_INTR_TX_DESC_MEM_CE (1 << 17)

#define OAK_UNI_INTR_AXI_WR_MEM_CE (1 << 16)

#define OAK_UNI_INTR_TX_DATA_MEM_CE (1 << 15)

#define OAK_UNI_INTR_TX_FIFO_MEM_CE (1 << 14)

#define OAK_UNI_INTR_RX_FIFO_MEM_CE (1 << 13)

#define OAK_UNI_INTR_HCS_MEM_CHECK (1 << 12)

#define OAK_UNI_INTR_TX_MTU_ERR (1 << 11)

#define OAK_UNI_INTR_RX_BERR_WR (1 << 10)

#define OAK_UNI_INTR_RX_BERR_RD (1 << 9)

#define OAK_UNI_INTR_TX_BERR_WR (1 << 8)

#define OAK_UNI_INTR_TX_BERR_RD (1 << 7)

#define OAK_UNI_INTR_BERR_HIC_A (1 << 6)

#define OAK_UNI_INTR_BERR_HIC_B (1 << 5)

#define OAK_UNI_INTR_COUNT_WRAP (1 << 4)

#define OAK_UNI_INTR_RX_WATCHDOG (1 << 3)

#define OAK_UNI_INTR_TX_WATCHDOG (1 << 2)

#define OAK_UNI_INTR_RX_STALL_FIFO (1 << 1)

#define OAK_UNI_INTR_TX_STALL_FIFO (1 << 0)

#define OAK_UNI_INTR_SEVERE_ERRORS (\
OAK_UNI_INTR_RX_STAT_MEM_UCE | \
OAK_UNI_INTR_RX_DESC_MEM_UCE | \
OAK_UNI_INTR_TX_DESC_MEM_UCE | \
OAK_UNI_INTR_AXI_WR_MEM_UCE | \
OAK_UNI_INTR_TX_DATA_MEM_UCE | \
OAK_UNI_INTR_TX_FIFO_MEM_UCS | \
OAK_UNI_INTR_RX_FIFO_MEM_UCS | \
OAK_UNI_INTR_HCS_MEM_UCE | \
OAK_UNI_INTR_RX_STAT_MEM_CE | \
OAK_UNI_INTR_RX_DESC_MEM_CE | \
OAK_UNI_INTR_TX_DESC_MEM_CE | \
OAK_UNI_INTR_AXI_WR_MEM_CE | \
OAK_UNI_INTR_TX_DATA_MEM_CE | \
OAK_UNI_INTR_TX_FIFO_MEM_CE | \
OAK_UNI_INTR_RX_FIFO_MEM_CE | \
OAK_UNI_INTR_HCS_MEM_CHECK | \
OAK_UNI_INTR_TX_MTU_ERR | \
OAK_UNI_INTR_RX_BERR_WR | \
OAK_UNI_INTR_RX_BERR_RD | \
OAK_UNI_INTR_TX_BERR_WR | \
OAK_UNI_INTR_TX_BERR_RD\
)

#define OAK_UNI_INTR_NORMAL_ERRORS (OAK_UNI_INTR_BERR_HIC_A | OAK_UNI_INTR_BERR_HIC_B | OAK_UNI_INTR_COUNT_WRAP | OAK_UNI_INTR_RX_WATCHDOG | OAK_UNI_INTR_TX_WATCHDOG | OAK_UNI_INTR_RX_STALL_FIFO | OAK_UNI_INTR_TX_STALL_FIFO)

#define OAK_UNI_TXRATE_B OAK_UNI_GLOBAL(0x80)

#define OAK_UNI_TXRATE_A OAK_UNI_GLOBAL(0x84)

#define OAK_UNI_STAT_RX_GOOD_FRAMES OAK_UNI_GLOBAL(0x100)

#define OAK_UNI_STAT_RX_BAD_FRAMES OAK_UNI_GLOBAL(0x104)

#define OAK_UNI_STAT_TX_PAUSE OAK_UNI_GLOBAL(0x108)

#define OAK_UNI_STAT_TX_STALL_FIFO OAK_UNI_GLOBAL(0x10C)

#define OAK_UNI_STAT_RX_STALL_DESC OAK_UNI_GLOBAL(0x110)

#define OAK_UNI_STAT_RX_STALL_FIFO OAK_UNI_GLOBAL(0x114)

#define OAK_UNI_STAT_RX_DISC_DESC OAK_UNI_GLOBAL(0x118)

#define OAK_UNI_PTP_HW_TIME OAK_UNI_GLOBAL(0x17C)

#define OAK_UNI_ECC_ERR_CFG OAK_UNI_GLOBAL(0x1E4)

#define OAK_UNI_ECC_ERR_STAT_0 OAK_UNI_GLOBAL(0x1E8)

#define OAK_UNI_ECC_ERR_STAT_1 OAK_UNI_GLOBAL(0x1EC)

#define OAK_UNI_ECC_ERR_CNT_0 OAK_UNI_GLOBAL(0x1F0)

#define OAK_UNI_ECC_ERR_CNT_1 OAK_UNI_GLOBAL(0x1F4)

#define OAK_UNI_DMA_TX_CH_CFG OAK_UNI_DMA_TXCH_OFFS(0x00)

#define OAK_UNI_DMA_TX_CH_ARBIT_B0_LO OAK_UNI_DMA_TXCH_OFFS(0x04)

#define OAK_UNI_DMA_TX_CH_ARBIT_B0_HI OAK_UNI_DMA_TXCH_OFFS(0x08)

#define OAK_UNI_DMA_TX_CH_SCHED_B0_LO OAK_UNI_DMA_TXCH_OFFS(0x0C)

#define OAK_UNI_DMA_TX_CH_SCHED_B0_HI OAK_UNI_DMA_TXCH_OFFS(0x10)

#define OAK_UNI_DMA_TX_CH_SCHED_LO OAK_UNI_DMA_TXCH_OFFS(0x04)

#define OAK_UNI_DMA_TX_CH_SCHED_HI OAK_UNI_DMA_TXCH_OFFS(0x08)

#define OAK_UNI_DMA_RX_CH_CFG OAK_UNI_DMA_RXCH_OFFS(0x00)

#define OAK_UNI_DMA_RX_CH_ARBIT_B0_LO OAK_UNI_DMA_RXCH_OFFS(0x04)

#define OAK_UNI_DMA_RX_CH_ARBIT_B0_HI OAK_UNI_DMA_RXCH_OFFS(0x08)

#define OAK_UNI_DMA_RX_CH_SCHED_LO OAK_UNI_DMA_RXCH_OFFS(0x04)

#define OAK_UNI_DMA_RX_CH_SCHED_HI OAK_UNI_DMA_RXCH_OFFS(0x08)

#define OAK_UNI_TX_RING_CFG(r) OAK_UNI_DMA_RING_TX(r, 0x00)

#define OAK_UNI_TX_RING_PREF_THR(r) OAK_UNI_DMA_RING_TX(r, 0x04)

#define OAK_UNI_TX_RING_MBOX_THR(r) OAK_UNI_DMA_RING_TX(r, 0x08)

#define OAK_UNI_TX_RING_DMA_PTR(r) OAK_UNI_DMA_RING_TX(r, 0x0C)

#define OAK_UNI_TX_RING_CPU_PTR(r) OAK_UNI_DMA_RING_TX(r, 0x10)

#define OAK_UNI_TX_RING_EN(r) OAK_UNI_DMA_RING_TX(r, 0x14)

#define OAK_UNI_TX_RING_INT_CAUSE(r) OAK_UNI_DMA_RING_TX(r, 0x18)

#define OAK_UNI_TX_RING_INT_MASK(r) OAK_UNI_DMA_RING_TX(r, 0x1C)

#define OAK_UNI_TX_RING_DBASE_LO(r) OAK_UNI_DMA_RING_TX(r, 0x20)

#define OAK_UNI_TX_RING_DBASE_HI(r) OAK_UNI_DMA_RING_TX(r, 0x24)

#define OAK_UNI_TX_RING_MBASE_LO(r) OAK_UNI_DMA_RING_TX(r, 0x28)

#define OAK_UNI_TX_RING_MBASE_HI(r) OAK_UNI_DMA_RING_TX(r, 0x2C)

#define OAK_UNI_TX_RING_TIMEOUT(r) OAK_UNI_DMA_RING_TX(r, 0x30)

#define OAK_UNI_TX_RING_RATECTRL(r) OAK_UNI_DMA_RING_TX(r, 0x34)

#define OAK_UNI_TX_RING_MAXDTIME(r) OAK_UNI_DMA_RING_TX(r, 0x38)

#define OAK_UNI_RING_ENABLE_REQ (1 << 0)

#define OAK_UNI_RING_ENABLE_DONE (1 << 1)

#define OAK_UNI_RX_RING_CFG(r) OAK_UNI_DMA_RING_RX(r, 0x00)

#define OAK_UNI_RX_RING_PREF_THR(r) OAK_UNI_DMA_RING_RX(r, 0x04)

#define OAK_UNI_RX_RING_MBOX_THR(r) OAK_UNI_DMA_RING_RX(r, 0x08)

#define OAK_UNI_RX_RING_DMA_PTR(r) OAK_UNI_DMA_RING_RX(r, 0x0C)

#define OAK_UNI_RX_RING_CPU_PTR(r) OAK_UNI_DMA_RING_RX(r, 0x10)

#define OAK_UNI_RX_RING_WATERMARK(r) OAK_UNI_DMA_RING_RX(r, 0x14)

#define OAK_UNI_RX_RING_EN(r) OAK_UNI_DMA_RING_RX(r, 0x18)

#define OAK_UNI_RX_RING_INT_CAUSE(r) OAK_UNI_DMA_RING_RX(r, 0x1C)

#define OAK_UNI_RX_RING_INT_MASK(r) OAK_UNI_DMA_RING_RX(r, 0x20)

#define OAK_UNI_RX_RING_DBASE_LO(r) OAK_UNI_DMA_RING_RX(r, 0x24)

#define OAK_UNI_RX_RING_DBASE_HI(r) OAK_UNI_DMA_RING_RX(r, 0x28)

#define OAK_RING_TOUT_USEC(us) (OAK_CLOCK_FREQ_MHZ * 1 * (us))

#define OAK_UNI_RX_RING_SBASE_LO(r) OAK_UNI_DMA_RING_RX(r, 0x2C)

#define OAK_UNI_RX_RING_SBASE_HI(r) OAK_UNI_DMA_RING_RX(r, 0x30)

#define OAK_UNI_RX_RING_MBASE_LO(r) OAK_UNI_DMA_RING_RX(r, 0x34)

#define OAK_UNI_RX_RING_MBASE_HI(r) OAK_UNI_DMA_RING_RX(r, 0x38)

#define OAK_UNI_RX_RING_TIMEOUT(r) OAK_UNI_DMA_RING_RX(r, 0x3C)

#define OAK_UNI_RX_RING_DADDR_HI(r) OAK_UNI_DMA_RING_RX(r, 0x40)

#define OAK_UNI_RX_RING_DADDR_LO(r) OAK_UNI_DMA_RING_RX(r, 0x44)

#define OAK_UNI_RX_RING_ETYPE(r) OAK_UNI_DMA_RING_RX(r, 0x48)

#define OAK_UNI_RX_RING_MAP(r) OAK_UNI_DMA_RING_RX(r, 0x4C)

#define OAK_CLOCK_FREQ_MHZ (250)

#define OAK_RING_TOUT_MSEC(ms) (OAK_CLOCK_FREQ_MHZ * 1000 * (ms))

#define OAK_MIN_TX_RATE_CLASS_A 0

#define OAK_MIN_TX_RATE_CLASS_B 1

#define OAK_MIN_TX_RATE_IN_KBPS 64

#define OAK_MAX_TX_RATE_IN_KBPS 4194240

#define OAK_DEF_TX_HI_CREDIT_BYTES 1536

#define OAK_MAX_TX_HI_CREDIT_BYTES 0x3fff

#define OAK_UNI_RX_RING_DADDR_MASK_HI(r) OAK_UNI_DMA_RING_RX(r, 0x50)

#define OAK_UNI_RX_RING_DADDR_MASK_LO(r) OAK_UNI_DMA_RING_RX(r, 0x54)

typedef struct oak_mbox_tStruct
{
    uint32_t dma_ptr_rel;
    uint32_t intr_cause;
} oak_mbox_t;

typedef struct oak_rxa_tStruct
{
    struct page* page_virt;
    dma_addr_t page_phys;
    uint32_t page_offs;
} oak_rxa_t;

typedef struct oak_rx_chan_tStruct
{
#define OAK_RX_BUFFER_SIZE 2048
#define OAK_RX_BUFFER_PER_PAGE (PAGE_SIZE / OAK_RX_BUFFER_SIZE)
#define OAK_RX_SKB_ALLOC_SIZE (128 + NET_IP_ALIGN)
#define OAK_RX_LGEN_RX_MODE (1 << 0)
#define OAK_RX_REFILL_REQ (1 << 1)
    struct oak_tStruct* oak;
    uint32_t enabled;
    uint32_t flags;
    uint32_t rbr_count;
    uint32_t rbr_size;
    uint32_t rsr_size;
    uint32_t mbox_size;
    atomic_t rbr_pend;
    uint32_t rbr_widx;
    uint32_t rbr_ridx;
    uint32_t rbr_len;
    uint32_t rbr_bsize;
    uint32_t rbr_bpage;
    dma_addr_t rbr_dma;
    dma_addr_t rsr_dma;
    dma_addr_t mbox_dma;
    oak_rxa_t* rba;
    oak_rxd_t* rbr;
    oak_rxs_t* rsr;
    oak_mbox_t* mbox;
    oak_driver_rx_stat stat;
    struct sk_buff* skb;
} oak_rx_chan_t;

typedef struct oak_txi_tStruct
{
#define TX_BUFF_INFO_NONE 0x00000000
#define TX_BUFF_INFO_ADR_MAPS 0x00000001
#define TX_BUFF_INFO_ADR_MAPP 0x00000002
#define TX_BUFF_INFO_EOP 0x00000004
    struct sk_buff* skb;
    struct page* page;
    dma_addr_t mapping;
    uint32_t mapsize;
    uint32_t flags;
} oak_txi_t;

typedef struct oak_tx_chan_tStruct
{
#define OAK_RX_LGEN_TX_MODE (1 << 0)
    struct oak_tStruct* oak;
    uint32_t enabled;
    uint32_t flags;
    uint32_t tbr_count;
    uint32_t tbr_compl;
    uint32_t tbr_size;
    atomic_t tbr_pend;
    uint32_t tbr_ridx;
    uint32_t tbr_widx;
    uint32_t tbr_len;
    dma_addr_t tbr_dma;
    oak_txd_t* tbr;
    oak_txi_t* tbi;
    uint32_t mbox_size;
    dma_addr_t mbox_dma;
    oak_mbox_t* mbox;
    oak_driver_tx_stat stat;
    spinlock_t lock;
} oak_tx_chan_t;

typedef struct oak_tStruct
{
#define MAX_RBR_RING_ENTRIES 3
#define MAX_TBR_RING_ENTRIES 3
#define XBR_RING_SIZE(s) (1 << ((s) + 4))
#define MAX_RBR_RING_SIZE XBR_RING_SIZE(MAX_RBR_RING_ENTRIES)
#define MAX_TBR_RING_SIZE XBR_RING_SIZE(MAX_TBR_RING_ENTRIES)
#define RX_DESC_PREFETCH_TH 4
#define RX_MBOX_WRITE_TH 4
#define TX_DESC_PREFETCH_TH 4
#define TX_MBOX_WRITE_TH 4
#define OAK_MBOX_RX_RES_LOW (1 << 2)
#define OAK_MBOX_RX_COMP (1 << 0)
#define OAK_MBOX_TX_ERR_ABORT (1 << 4)
#define OAK_MBOX_TX_ERR_HCRED (1 << 3)
#define OAK_MBOX_TX_LATE_TS (1 << 2)
#define OAK_MBOX_TX_COMP (1 << 0)
#define OAK_IVEC_UVEC1 (1 << 8)
#define NEXT_IDX(i, sz) (((i) + 1) % (sz))
#define MAX_NUM_OF_CHANNELS 10
#define nw32(np, reg, val) writel((val), np->um_base + (reg))
#define MIN_NUM_OF_CHANNELS 1
#define nr32(np, reg) readl(np->um_base + (reg))
#define sr32(np, reg) readl(np->um_base + (reg))
#define sw32(np, reg, val) writel((val), np->um_base + (reg))
#define oakdbg(debug_var, TYPE, f, a...)     \
    if (((debug_var)&NETIF_MSG_##TYPE) != 0) \
    {                                        \
        printk("%s:" f, __func__, ##a);      \
    \
}
    int level;
    uint32_t pci_class_revision;
    spinlock_t lock;
    struct net_device* netdev;
    struct device* device;
    struct pci_dev* pdev;
    void __iomem* um_base;
    void __iomem* sw_base;
    uint32_t page_order;
    uint32_t page_size;
    oak_gicu gicu;
    uint32_t num_rx_chan;
    oak_rx_chan_t rx_channel[MAX_NUM_OF_CHANNELS];
    uint32_t num_tx_chan;
    oak_tx_chan_t tx_channel[MAX_NUM_OF_CHANNELS];
    oak_unimac_stat unimac_stat;
    uint16_t rrs;
    uint32_t speed;
} oak_t;

extern int debug;
extern int mhdr;
extern int oak_net_rbr_refill(oak_t* np, uint32_t ring);
/* Name      : set_arbit_priority_based
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t prio = prio,  uint32_t reg = reg
 *  */
void oak_unimac_set_arbit_priority_based(oak_t* np, uint32_t ring, uint32_t prio, uint32_t reg);

/* Name      : set_arbit_round_robin
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t weight = weight,  int reg = reg
 *  */
void oak_unimac_set_arbit_round_robin(oak_t* np, uint32_t ring, uint32_t weight, int reg);

/* Name      : disable_and_get_tx_irq_reason
 * Returns   : uint32_t
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t* dma_ptr = &tidx
 *  */
uint32_t oak_unimac_disable_and_get_tx_irq_reason(oak_t* np, uint32_t ring, uint32_t* dma_ptr);

/* Name      : alloc_channels
 * Returns   : int
 * Parameters:  oak_t * np = np,  int rxs = rxs,  int txs = txs,  int chan = chan,  int rto = rto
 *  */
int oak_unimac_alloc_channels(oak_t* np, int rxs, int txs, int chan, int rto);

/* Name      : free_channels
 * Returns   : void
 * Parameters:  oak_t * np = np
 *  */
void oak_unimac_free_channels(oak_t* np);

/* Name      : reset
 * Returns   : int
 * Parameters:  oak_t * np = np
 *  */
int oak_unimac_reset(oak_t* np);

/* Name      : reset_statistics
 * Returns   : void
 * Parameters:  oak_t * np
 *  */
void oak_unimac_reset_statistics(oak_t* np);

/* Name      : crt_bit_mask
 * Returns   : uint32_t
 * Parameters:  uint32_t off = off,  uint32_t len = len,  uint32_t val = val,  uint32_t bit_mask = bit_mask
 *  */
uint32_t oak_unimac_crt_bit_mask(uint32_t off, uint32_t len, uint32_t val, uint32_t bit_mask);

/* Name      : io_read_32
 * Returns   : uint32_t
 * Parameters:  oak_t * np = np,  uint32_t addr = addr
 *  */
uint32_t oak_unimac_io_read_32(oak_t* np, uint32_t addr);

/* Name      : io_write_32
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t addr = addr,  uint32_t val = val
 *  */
void oak_unimac_io_write_32(oak_t* np, uint32_t addr, uint32_t val);

/* Name      : set_bit_num
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t reg = req,  uint32_t bit_num = bit_num,  int enable = enable
 *  */
void oak_unimac_set_bit_num(oak_t* np, uint32_t reg, uint32_t bit_num, int enable);

/* Name      : set_rx_none
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring
 *  */
void oak_unimac_set_rx_none(oak_t* np, uint32_t ring);

/* Name      : set_rx_8021Q_et
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint16_t etype = etype,  uint16_t pcp_vid = pcp_vid,  int enable = enable
 *  */
void oak_unimac_set_rx_8021Q_et(oak_t* np, uint32_t ring, uint16_t etype, uint16_t pcp_vid, int enable);

/* Name      : set_rx_8021Q_fid
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t fid = fid,  int enable = enable
 *  */
void oak_unimac_set_rx_8021Q_fid(oak_t* np, uint32_t ring, uint32_t fid, int enable);

/* Name      : set_rx_8021Q_flow
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t flow_id = flow_id,  int enable = enable
 *  */
void oak_unimac_set_rx_8021Q_flow(oak_t* np, uint32_t ring, uint32_t flow_id, int enable);

/* Name      : set_rx_8021Q_qpri
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t qpri = qpri,  int enable = enable
 *  */
void oak_unimac_set_rx_8021Q_qpri(oak_t* np, uint32_t ring, uint32_t qpri, int enable);

/* Name      : set_rx_8021Q_spid
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t spid = spid,  int enable = enable
 *  */
void oak_unimac_set_rx_8021Q_spid(oak_t* np, uint32_t ring, uint32_t spid, int enable);

/* Name      : set_rx_da
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  unsigned char * addr = addr,  int enable = enable
 *  */
void oak_unimac_set_rx_da(oak_t* np, uint32_t ring, unsigned char* addr, int enable);

/* Name      : set_rx_da_mask
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  unsigned char * addr = addr,  int enable = enable
 *  */
void oak_unimac_set_rx_da_mask(oak_t* np, uint32_t ring, unsigned char* addr, int enable);

/* Name      : set_rx_mgmt
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t val = val,  int enable = enable
 *  */
void oak_unimac_set_rx_mgmt(oak_t* np, uint32_t ring, uint32_t val, int enable);

/* Name      : process_status
 * Returns   : void
 * Parameters:  ldg_t * ldg = ldg
 *  */
void oak_unimac_process_status(ldg_t* ldg);

/* Name      : rx_error
 * Returns   : void
 * Parameters:  ldg_t * ldg = ldg,  uint32_t ring = ring
 *  */
void oak_unimac_rx_error(ldg_t* ldg, uint32_t ring);

/* Name      : tx_error
 * Returns   : void
 * Parameters:  ldg_t * ldg = ldg,  uint32_t ring = ring
 *  */
void oak_unimac_tx_error(ldg_t* ldg, uint32_t ring);

/* Name      : ena_rx_ring_irq
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t enable = enable
 *  */
void oak_unimac_ena_rx_ring_irq(oak_t* np, uint32_t ring, uint32_t enable);

/* Name      : ena_tx_ring_irq
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t enable = enable
 *  */
void oak_unimac_ena_tx_ring_irq(oak_t* np, uint32_t ring, uint32_t enable);

/* Name      : set_tx_ring_rate
 * Returns   : int
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t sr_class = sr_class,  uint32_t hi_credit = hi_credit,  uint32_t r_kbps = r_kbps
 *  */
int oak_unimac_set_tx_ring_rate(oak_t* np, uint32_t ring, uint32_t sr_class, uint32_t hi_credit, uint32_t r_kbps);

/* Name      : clr_tx_ring_rate
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring
 *  */
void oak_unimac_clr_tx_ring_rate(oak_t* np, uint32_t ring);

/* Name      : set_tx_mac_rate
 * Returns   : int
 * Parameters:  oak_t * np = np,  uint32_t sr_class = sr_class,  uint32_t hi_credit = hi_credit,  uint32_t r_kbps = r_kbps
 *  */
int oak_unimac_set_tx_mac_rate(oak_t* np, uint32_t sr_class, uint32_t hi_credit, uint32_t r_kbps);

/* Name      : set_sched_round_robin
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t weight = weight,  int reg = rx_flag
 *  */
void oak_unimac_set_sched_round_robin(oak_t* np, uint32_t ring, uint32_t weight, int reg);

/* Name      : set_sched_priority_based
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t prio = prio,  uint32_t reg = rx_flag
 *  */
void oak_unimac_set_sched_priority_based(oak_t* np, uint32_t ring, uint32_t prio, uint32_t reg);

/* Name      : start_all_txq
 * Returns   : int
 * Parameters:  oak_t * np = np,  uint32_t enable = enable
 *  */
int oak_unimac_start_all_txq(oak_t* np, uint32_t enable);

/* Name      : start_all_rxq
 * Returns   : int
 * Parameters:  oak_t * np = np,  uint32_t enable = enable
 *  */
int oak_unimac_start_all_rxq(oak_t* np, uint32_t enable);

/* Name      : start_tx_ring
 * Returns   : uint32_t
 * Parameters:  oak_t * np,  int32_t ring,  uint32_t enable
 *  */
uint32_t oak_unimac_start_tx_ring(oak_t* np, int32_t ring, uint32_t enable);

/* Name      : start_rx_ring
 * Returns   : uint32_t
 * Parameters:  oak_t * np,  uint32_t ring,  uint32_t enable
 *  */
uint32_t oak_unimac_start_rx_ring(oak_t* np, uint32_t ring, uint32_t enable);

#endif /* #ifndef H_OAK_UNIMAC */

