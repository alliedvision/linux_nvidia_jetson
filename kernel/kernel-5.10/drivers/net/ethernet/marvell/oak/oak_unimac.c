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
#include "oak_unimac.h"

extern int debug;
extern int mhdr;
extern int oak_net_rbr_refill(oak_t* np, uint32_t ring);

/* private function prototypes */
static void oak_unimac_set_channel_dma(oak_t* np, int rto, int rxs, int txs, int chan);
static uint32_t oak_unimac_ena_ring(oak_t* np, uint32_t addr, uint32_t enable);
static void oak_unimac_set_sched_arbit_value(oak_t* np, uint32_t ring, uint32_t weight, int reg);
static void oak_unimac_set_dma_addr(oak_t* np, dma_addr_t phys, uint32_t reg_lo, uint32_t reg_hi);

/* Name      : set_arbit_priority_based
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t prio = prio,  uint32_t reg = reg
 *  */
void oak_unimac_set_arbit_priority_based(oak_t* np, uint32_t ring, uint32_t prio, uint32_t reg)
{
    uint32_t val; /* start of activity code */

    if (ring >= 0 && ring <= 9)
    {
        val = oak_unimac_io_read_32(np, reg);
        /* UserCode{FAE544BB-DEE5-4573-8D1A-52E0432B6DC6}:M5Nw408xN4 */
        val |= (1 << 11);
        /* UserCode{FAE544BB-DEE5-4573-8D1A-52E0432B6DC6} */

        oak_unimac_io_write_32(np, reg, val);

        if (reg == OAK_UNI_DMA_RX_CH_CFG)
        {
            oak_unimac_set_sched_arbit_value(np, ring, prio, OAK_UNI_DMA_RX_CH_ARBIT_B0_LO);
        }
        else
        {
            oak_unimac_set_sched_arbit_value(np, ring, prio, OAK_UNI_DMA_TX_CH_ARBIT_B0_LO);
        }
    }
    else
    {
    }
    return;
}

/* Name      : set_arbit_round_robin
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t weight = weight,  int reg = reg
 *  */
void oak_unimac_set_arbit_round_robin(oak_t* np, uint32_t ring, uint32_t weight, int reg)
{
    uint32_t val; /* start of activity code */

    if (ring >= 0 && ring <= 9)
    {
        val = oak_unimac_io_read_32(np, reg);
        /* UserCode{EC50D223-3D4B-4a57-AA01-757F752048DC}:GVaum8tlpY */
        val &= ~(1 << 11);
        /* UserCode{EC50D223-3D4B-4a57-AA01-757F752048DC} */

        oak_unimac_io_write_32(np, reg, val);

        if (reg == OAK_UNI_DMA_RX_CH_CFG)
        {
            oak_unimac_set_sched_arbit_value(np, ring, weight, OAK_UNI_DMA_RX_CH_ARBIT_B0_LO);
        }
        else
        {
            oak_unimac_set_sched_arbit_value(np, ring, weight, OAK_UNI_DMA_TX_CH_ARBIT_B0_LO);
        }
    }
    else
    {
    }
    return;
}

/* Name      : disable_and_get_tx_irq_reason
 * Returns   : uint32_t
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t* dma_ptr = &tidx
 *  */
uint32_t oak_unimac_disable_and_get_tx_irq_reason(oak_t* np, uint32_t ring, uint32_t* dma_ptr)
{
    oak_tx_chan_t* txc = &np->tx_channel[ring];
    uint32_t reason; /* automatically added for object flow handling */
    uint32_t rc_7 = 0; /* start of activity code */
    oak_unimac_ena_rx_ring_irq(np, ring, 0);
/* UserCode{73DCD239-1452-4f42-8C95-773E68511D95}:1MmoMWI0Y8 */
#if 1
    reason = le32_to_cpup(&txc->mbox->intr_cause);
#else
    reason = oak_unimac_io_read_32(np, OAK_UNI_TX_RING_INT_CAUSE(ring));
#endif
    /* UserCode{73DCD239-1452-4f42-8C95-773E68511D95} */

    oak_unimac_io_write_32(np, OAK_UNI_TX_RING_INT_CAUSE(ring), (OAK_MBOX_TX_COMP | OAK_MBOX_TX_LATE_TS | OAK_MBOX_TX_ERR_HCRED));
/* UserCode{D9313548-A793-4faa-9A2F-834623768A38}:1JcNGbiru6 */
#if 1
    *dma_ptr = le32_to_cpup(&txc->mbox->dma_ptr_rel);
#else
    *dma_ptr = oak_unimac_io_read_32(np, OAK_UNI_TX_RING_DMA_PTR(ring));
    *dma_ptr >>= 16;
#endif
    /* UserCode{D9313548-A793-4faa-9A2F-834623768A38} */

    rc_7 = reason;
    return rc_7;
}

/* Name      : alloc_channels
 * Returns   : int
 * Parameters:  oak_t * np = np,  int rxs = rxs,  int txs = txs,  int chan = chan,  int rto = rto
 *  */
int oak_unimac_alloc_channels(oak_t* np, int rxs, int txs, int chan, int rto)
{
    int err = 0;
    oak_rx_chan_t* rxc;
    oak_tx_chan_t* txc;
    int i;
    int max_rx_size;
    int max_tx_size;
    int max_channel; /* automatically added for object flow handling */
    int rc_7 = -ENOMEM; /* start of activity code */

    if (rxs < 16 || rxs > 2048)
    {
        /* UserCode{5877BB77-4DA6-4fe2-9737-0AD579BADAAA}:KGJLNWHPCl */
        rxs = 0;
        /* UserCode{5877BB77-4DA6-4fe2-9737-0AD579BADAAA} */
    }
    else
    {
        /* UserCode{9425D6AC-8943-4c11-A902-67B555C2E91B}:177tMd5fec */
        rxs = ilog2(rxs) - 4;
        /* UserCode{9425D6AC-8943-4c11-A902-67B555C2E91B} */
    }

    if (txs < 16 || txs > 2048)
    {
        /* UserCode{2D01AD19-30AB-42db-B2BD-4586D2086F0E}:Ky61Apxqon */
        txs = 0;
        /* UserCode{2D01AD19-30AB-42db-B2BD-4586D2086F0E} */
    }
    else
    {
        /* UserCode{A4B41DB7-A886-439d-81F9-6DA7E54E608B}:IrRUtoAgQS */
        txs = ilog2(txs) - 4;
        /* UserCode{A4B41DB7-A886-439d-81F9-6DA7E54E608B} */
    }
    /* UserCode{A941F519-C208-434d-A4E0-E86462E41997}:uVFl5hUjmg */
    max_rx_size = XBR_RING_SIZE(rxs);
    max_tx_size = XBR_RING_SIZE(txs);
    /* UserCode{A941F519-C208-434d-A4E0-E86462E41997} */

    if (chan >= MIN_NUM_OF_CHANNELS && chan <= MAX_NUM_OF_CHANNELS)
    {
        /* UserCode{332868DD-CF4C-44ff-9069-2C14C507DAC9}:jGLhpcwTqR */
        max_channel = chan;
        /* UserCode{332868DD-CF4C-44ff-9069-2C14C507DAC9} */
    }
    else
    {
        /* UserCode{18C71CAC-52B8-447f-8543-973F876A3C47}:10Ik6K1OJx */
        err = -EFAULT;
        max_channel = 0;
        /* UserCode{18C71CAC-52B8-447f-8543-973F876A3C47} */
    }
    /* UserCode{29F1A547-8441-4e17-A4A8-FB0F6099FF89}:m0fhNHc1xv */
    np->num_rx_chan = 0;
    np->num_tx_chan = 0;
    i = 0;
    /* UserCode{29F1A547-8441-4e17-A4A8-FB0F6099FF89} */

    while (err == 0 && i < max_channel)
    {
        /* UserCode{4DF35383-AF6A-403a-900B-7875988832F8}:1VYodBsxFe */
        rxc = &np->rx_channel[i];
        rxc->oak = np; /* set backpointer */
        rxc->flags = 0; /* reset flags */
#if 1
        atomic_set(&rxc->rbr_pend, 0);
#else
        rxc->rbr_pend = 0; /* number of pending buffers ready 2b processed by hardware */
#endif
        rxc->rbr_widx = 0; /* software write buffer index */
        rxc->rbr_ridx = 0; /* software read buffer index */
        rxc->skb = NULL;
        rxc->rbr_bsize = OAK_RX_BUFFER_SIZE;
        rxc->rbr_bpage = (PAGE_SIZE / rxc->rbr_bsize);
        /* UserCode{4DF35383-AF6A-403a-900B-7875988832F8} */

        if (rxc->rbr_bpage < 1)
        {
            /* UserCode{A252B176-5B15-45b1-AA37-FF88D5497B8D}:e88et8mSQf */
            err = -EFAULT;
            /* UserCode{A252B176-5B15-45b1-AA37-FF88D5497B8D} */
        }
        else
        {
        }

        if (err == 0 && rxc->rbr == NULL)
        {
            /* UserCode{D43949BE-8643-4a04-B732-FD1CE8F37806}:Nh69hbzYHv */
            rxc->rbr_size = max_rx_size;
            rxc->rbr = dma_alloc_coherent(np->device, rxc->rbr_size * sizeof(oak_rxd_t), &rxc->rbr_dma, GFP_KERNEL);
            /* UserCode{D43949BE-8643-4a04-B732-FD1CE8F37806} */

            if ((rxc->rbr_dma & 7) != 0)
            {
                /* UserCode{988E314E-B9F5-452d-BF91-C563F8300901}:e88et8mSQf */
                err = -EFAULT;
                /* UserCode{988E314E-B9F5-452d-BF91-C563F8300901} */
            }
            else
            {
            }
        }
        else
        {
        }

        if (err == 0 && rxc->rsr == NULL)
        {
            /* UserCode{7BB7904A-E0AA-4011-B757-B18BF9866C2C}:ICHXPx7GFb */
            rxc->rsr_size = max_rx_size;
            rxc->rsr = dma_alloc_coherent(np->device, rxc->rsr_size * sizeof(oak_rxs_t), &rxc->rsr_dma, GFP_KERNEL);

            /* UserCode{7BB7904A-E0AA-4011-B757-B18BF9866C2C} */

            if ((rxc->rsr_dma & 15) != 0)
            {
                /* UserCode{F5A0B9F5-3FF2-4ee4-933B-0EB699D24BE3}:e88et8mSQf */
                err = -EFAULT;
                /* UserCode{F5A0B9F5-3FF2-4ee4-933B-0EB699D24BE3} */
            }
            else
            {
            }
        }
        else
        {
        }

        if (err == 0 && rxc->mbox == NULL)
        {
            /* UserCode{3358AC45-1864-4257-912C-4F33732C3A31}:VuHS15gGNz */
            rxc->mbox_size = 1;
            rxc->mbox = dma_alloc_coherent(np->device, sizeof(oak_mbox_t), &rxc->mbox_dma, GFP_KERNEL);
            /* UserCode{3358AC45-1864-4257-912C-4F33732C3A31} */

            if ((rxc->mbox_dma & 7) != 0)
            {
                /* UserCode{3C73B9E7-94E2-4ba3-BC5B-106CBA9C2947}:e88et8mSQf */
                err = -EFAULT;
                /* UserCode{3C73B9E7-94E2-4ba3-BC5B-106CBA9C2947} */
            }
            else
            {
            }
        }
        else
        {
        }

        if (err == 0 && rxc->rba == NULL)
        {
            /* UserCode{5E6B69C7-125E-4003-906A-D70D9DEF1F70}:9YNOX6VLtT */
            rxc->rba = kzalloc(rxc->rbr_size * sizeof(oak_rxa_t), GFP_KERNEL);
            /* UserCode{5E6B69C7-125E-4003-906A-D70D9DEF1F70} */
        }
        else
        {
        }

        if ((err == 0) && (rxc->rbr == NULL || rxc->rsr == NULL || rxc->mbox == NULL || rxc->rba == NULL))
        {
            /* UserCode{6F67D2E4-4724-4c92-94C0-972807D31D0E}:17P14WrLMp */
            err = -ENOMEM;
            /* UserCode{6F67D2E4-4724-4c92-94C0-972807D31D0E} */
        }
        else
        {
        }
        /* UserCode{BBC51DEF-18A5-4bdc-A03E-1A78C8541B51}:1DZQgxiQOH */
        ++np->num_rx_chan;
        ++i;
        /* UserCode{BBC51DEF-18A5-4bdc-A03E-1A78C8541B51} */
    }

    /* UserCode{8F289C0E-A3EE-4e07-8965-D07943CE68FB}:7233eLMZuK */
    i = 0;
    /* UserCode{8F289C0E-A3EE-4e07-8965-D07943CE68FB} */

    while (err == 0 && i < max_channel)
    {
        /* UserCode{D35A14B0-1F6C-48e8-AA9F-A3AA4A01DCEE}:4ndUnGq8tC */
        txc = &np->tx_channel[i];
        txc->oak = np; /* set backpointer */
        txc->flags = 0; /* reset flags */
        txc->tbr_count = 0; /* number of pending buffers ready 2b processed by hardware */
#if 1
        atomic_set(&txc->tbr_pend, 0);
#else
        txc->tbr_pend = 0; /* number of pending buffers ready 2b processed by hardware */
#endif
        txc->tbr_widx = 0; /* software write buffer index */
        txc->tbr_ridx = 0; /* software read buffer index */

        /* UserCode{D35A14B0-1F6C-48e8-AA9F-A3AA4A01DCEE} */

        if (txc->tbr == NULL)
        {
            /* UserCode{2F53A4C5-80A8-4159-BAC9-DBE8420FE242}:1XBjS08AVX */
            txc->tbr_size = max_tx_size;
            txc->tbr = dma_alloc_coherent(np->device, txc->tbr_size * sizeof(oak_txd_t), &txc->tbr_dma, GFP_KERNEL);

            /* UserCode{2F53A4C5-80A8-4159-BAC9-DBE8420FE242} */

            if ((txc->tbr_dma & 15) != 0)
            {
                /* UserCode{2E08EDF6-FE14-4e67-A21C-9C9446F8405D}:e88et8mSQf */
                err = -EFAULT;
                /* UserCode{2E08EDF6-FE14-4e67-A21C-9C9446F8405D} */
            }
            else
            {
            }
        }
        else
        {
        }

        if (err == 0 && txc->mbox == NULL)
        {
            /* UserCode{2C7A7BA0-DB86-46aa-A552-2CFE07177B74}:1TKUYZCVc9 */
            txc->mbox_size = 1;
            txc->mbox = dma_alloc_coherent(np->device, sizeof(oak_mbox_t), &txc->mbox_dma, GFP_KERNEL);
            /* UserCode{2C7A7BA0-DB86-46aa-A552-2CFE07177B74} */

            if ((txc->mbox_dma & 7) != 0)
            {
                /* UserCode{AB394F66-B414-4160-BE28-02E4829E9DDA}:e88et8mSQf */
                err = -EFAULT;
                /* UserCode{AB394F66-B414-4160-BE28-02E4829E9DDA} */
            }
            else
            {
            }
        }
        else
        {
        }

        if (err == 0 && txc->tbi == NULL)
        {
            /* UserCode{A01F1999-04E4-4291-B3D7-89B36FC16036}:qDOELeht0Q */
            txc->tbi = kzalloc(txc->tbr_size * sizeof(oak_txi_t), GFP_KERNEL);
            /* UserCode{A01F1999-04E4-4291-B3D7-89B36FC16036} */
        }
        else
        {
        }

        if ((err == 0) && (txc->tbr == NULL || txc->mbox == NULL || txc->tbi == NULL))
        {
            /* UserCode{9B52E988-7138-443a-BDE3-61347CBBD703}:17P14WrLMp */
            err = -ENOMEM;
            /* UserCode{9B52E988-7138-443a-BDE3-61347CBBD703} */
        }
        else
        {
        }
        /* UserCode{78722989-451B-4f55-8982-129D72E27E8C}:1EaJ3uHQoZ */
        ++np->num_tx_chan;
        ++i;
        /* UserCode{78722989-451B-4f55-8982-129D72E27E8C} */
    }

    if (err == 0)
    {
        oak_unimac_set_channel_dma(np, rto, rxs, txs, chan);
    }
    else
    {
        oak_unimac_free_channels(np);
    }
    rc_7 = err;
    return rc_7;
}

/* Name      : free_channels
 * Returns   : void
 * Parameters:  oak_t * np = np
 *  */
void oak_unimac_free_channels(oak_t* np)
{
    /* start of activity code */
    /* UserCode{F0D14852-5229-43b3-8574-D5BFFF3E41A3}:aadSV18VXL */
    oakdbg(debug, IFDOWN, "np=%p num_rx_chan=%d num_tx_chan=%d", np, np->num_rx_chan, np->num_tx_chan);
    /* UserCode{F0D14852-5229-43b3-8574-D5BFFF3E41A3} */

    while (np->num_rx_chan > 0)
    {
        /* UserCode{EEE2EE67-A2E6-4b4a-8D69-45DE2FE29AAD}:gZTFZDkukN */
        oak_rx_chan_t* chan = &np->rx_channel[np->num_rx_chan - 1];
        /* UserCode{EEE2EE67-A2E6-4b4a-8D69-45DE2FE29AAD} */

        if (chan->rbr != NULL)
        {
            /* UserCode{C3C2AC3D-7526-42f7-A69B-80964E8B274C}:iv1atq25fZ */
            dma_free_coherent(np->device, chan->rbr_size * sizeof(oak_rxd_t), chan->rbr, chan->rbr_dma);
            chan->rbr = NULL;
            /* UserCode{C3C2AC3D-7526-42f7-A69B-80964E8B274C} */
        }
        else
        {
        }

        if (chan->rsr != NULL)
        {
            /* UserCode{4B89C8D7-F668-45c3-AF7A-BE9A3733A9B2}:1CEmhVtX7y */
            dma_free_coherent(np->device, chan->rsr_size * sizeof(oak_rxs_t), chan->rsr, chan->rsr_dma);
            chan->rsr = NULL;

            /* UserCode{4B89C8D7-F668-45c3-AF7A-BE9A3733A9B2} */
        }
        else
        {
        }

        if (chan->mbox != NULL)
        {
            /* UserCode{A87D5F8A-38A3-487c-A005-EF3819140D69}:2oetepWPu4 */
            dma_free_coherent(np->device, sizeof(oak_mbox_t), chan->mbox, chan->mbox_dma);
            chan->mbox = NULL;
            /* UserCode{A87D5F8A-38A3-487c-A005-EF3819140D69} */
        }
        else
        {
        }

        if (chan->rba != NULL)
        {
            /* UserCode{9ADFB1A3-6BC7-4a42-8E0A-1E4B1EB70374}:fdviNZ5r2P */
            kfree(chan->rba);
            chan->rba = NULL;

            /* UserCode{9ADFB1A3-6BC7-4a42-8E0A-1E4B1EB70374} */
        }
        else
        {
        }
        /* UserCode{5892623B-2F50-4474-AFDF-FAA44DDF4F58}:17BkhbgNNn */
        --np->num_rx_chan;
        /* UserCode{5892623B-2F50-4474-AFDF-FAA44DDF4F58} */
    }

    while (np->num_tx_chan > 0)
    {
        /* UserCode{C1AA6E5A-57F0-4c83-AA41-6E15BECE1C14}:yXZRSUF56i */
        oak_tx_chan_t* chan = &np->tx_channel[np->num_tx_chan - 1];
        /* UserCode{C1AA6E5A-57F0-4c83-AA41-6E15BECE1C14} */

        if (chan->tbr != NULL)
        {
            /* UserCode{566B2DDF-6C31-42b1-9FD9-3ADBC08815AB}:T0xx80tjwj */
            dma_free_coherent(np->device, chan->tbr_size * sizeof(oak_txd_t), chan->tbr, chan->tbr_dma);
            chan->tbr = NULL;

            /* UserCode{566B2DDF-6C31-42b1-9FD9-3ADBC08815AB} */
        }
        else
        {
        }

        if (chan->mbox != NULL)
        {
            /* UserCode{9CFBF545-741A-425d-B6A8-8954173A86DD}:2oetepWPu4 */
            dma_free_coherent(np->device, sizeof(oak_mbox_t), chan->mbox, chan->mbox_dma);
            chan->mbox = NULL;
            /* UserCode{9CFBF545-741A-425d-B6A8-8954173A86DD} */
        }
        else
        {
        }

        if (chan->tbi != NULL)
        {
            /* UserCode{EDDC1206-4386-473c-8448-D3920030E55D}:y1TXmCfFPW */
            kfree(chan->tbi);
            chan->tbi = NULL;

            /* UserCode{EDDC1206-4386-473c-8448-D3920030E55D} */
        }
        else
        {
        }
        /* UserCode{9378A469-4A2A-41b9-9A82-97CF51052233}:18d7VRt6Rd */
        --np->num_tx_chan;
        /* UserCode{9378A469-4A2A-41b9-9A82-97CF51052233} */
    }

    return;
}

/* Name      : reset
 * Returns   : int
 * Parameters:  oak_t * np = np
 *  */
int oak_unimac_reset(oak_t* np)
{
    uint32_t val = (1 << 31);
    uint32_t cnt = 1000; /* automatically added for object flow handling */
    int rc_9 = 0; /* start of activity code */
    oak_unimac_io_write_32(np, OAK_UNI_CTRL, val);
    while ((cnt > 0) && (val & (1 << 31)))
    {
        val = oak_unimac_io_read_32(np, OAK_UNI_CTRL);
        /* UserCode{9BE3A678-A679-43b5-A09F-ED6426136CB5}:1NbgwaCgZl */
        --cnt;
        /* UserCode{9BE3A678-A679-43b5-A09F-ED6426136CB5} */
    }

    if (cnt > 0)
    {
        oak_unimac_reset_statistics(np);
    }
    else
    {
        rc_9 = -EFAULT;
    }
    return rc_9;
}

/* Name      : reset_statistics
 * Returns   : void
 * Parameters:  oak_t * np
 *  */
void oak_unimac_reset_statistics(oak_t* np)
{
    uint32_t i = 0; /* start of activity code */
    oak_unimac_io_write_32(np, OAK_UNI_STAT_RX_GOOD_FRAMES, 0);
    oak_unimac_io_write_32(np, OAK_UNI_STAT_RX_BAD_FRAMES, 0);
    oak_unimac_io_write_32(np, OAK_UNI_STAT_RX_STALL_DESC, 0);
    oak_unimac_io_write_32(np, OAK_UNI_STAT_RX_STALL_FIFO, 0);
    oak_unimac_io_write_32(np, OAK_UNI_STAT_RX_DISC_DESC, 0);
    oak_unimac_io_write_32(np, OAK_UNI_STAT_TX_STALL_FIFO, 0);
    oak_unimac_io_write_32(np, OAK_UNI_STAT_TX_PAUSE, 0);
    while (i < np->num_rx_chan)
    {
        /* UserCode{4A8348C0-9493-45bb-96FD-3EE4FC41730F}:sIgiDhDFOT */
        memset(&np->rx_channel[i], 0, sizeof(oak_rx_chan_t));
        /* UserCode{4A8348C0-9493-45bb-96FD-3EE4FC41730F} */

        /* UserCode{A9684064-B692-454a-9829-F74B8E7CA703}:pqeK47KwaC */
        ++i;
        /* UserCode{A9684064-B692-454a-9829-F74B8E7CA703} */
    }

    /* UserCode{7CDA796A-E389-4da6-89BE-9866F7BE2381}:7233eLMZuK */
    i = 0;
    /* UserCode{7CDA796A-E389-4da6-89BE-9866F7BE2381} */

    while (i < np->num_tx_chan)
    {
        /* UserCode{723B9DEF-919B-4f5e-8FD3-E57B9EB68697}:1Z5IXfiQlY */
        memset(&np->tx_channel[i], 0, sizeof(oak_tx_chan_t));
        /* UserCode{723B9DEF-919B-4f5e-8FD3-E57B9EB68697} */

        /* UserCode{55078C1E-94B1-4155-A5C1-1AB94B6B1A04}:pqeK47KwaC */
        ++i;
        /* UserCode{55078C1E-94B1-4155-A5C1-1AB94B6B1A04} */
    }

    return;
}

/* Name      : crt_bit_mask
 * Returns   : uint32_t
 * Parameters:  uint32_t off = off,  uint32_t len = len,  uint32_t val = val,  uint32_t bit_mask = bit_mask
 *  */
uint32_t oak_unimac_crt_bit_mask(uint32_t off, uint32_t len, uint32_t val, uint32_t bit_mask)
{
    /* automatically added for object flow handling */
    uint32_t ret_7 = 0; /* start of activity code */
    uint32_t mask = 0;
    uint32_t sz = sizeof(val) * 8;

    if ((len + off) >= sz)
    {
        len = sz - off;
    }
    else
    {
    }
    mask = ((1 << len) - 1) << off;
    val = val & ~mask;
    val = val | ((bit_mask << off) & mask);
    ret_7 = val;
    return ret_7;
}

/* Name      : io_read_32
 * Returns   : uint32_t
 * Parameters:  oak_t * np = np,  uint32_t addr = addr
 *  */
uint32_t oak_unimac_io_read_32(oak_t* np, uint32_t addr)
{
    /* automatically added for object flow handling */
    uint32_t ret_2 = 0; /* start of activity code */
    uint32_t val = readl(np->um_base + addr);
    ret_2 = val;
    return ret_2;
}

/* Name      : io_write_32
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t addr = addr,  uint32_t val = val
 *  */
void oak_unimac_io_write_32(oak_t* np, uint32_t addr, uint32_t val)
{
    /* start of activity code */
    /* UserCode{BDA52639-E8EF-4a7e-BC08-29B54F12981F}:C5CTU7x1VO */
    writel((val), np->um_base + (addr));
    /* UserCode{BDA52639-E8EF-4a7e-BC08-29B54F12981F} */

    return;
}

/* Name      : set_bit_num
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t reg = req,  uint32_t bit_num = bit_num,  int enable = enable
 *  */
void oak_unimac_set_bit_num(oak_t* np, uint32_t reg, uint32_t bit_num, int enable)
{
    /* start of activity code */
    uint32_t val = oak_unimac_io_read_32(np, reg);

    if (enable != 0)
    {
        /* UserCode{0515E7E1-E2AA-4833-A2CA-ADD15C79F336}:4PmzelkHZu */
        val |= (1 << bit_num);
        /* UserCode{0515E7E1-E2AA-4833-A2CA-ADD15C79F336} */
    }
    else
    {
        /* UserCode{EE57420F-3B82-4520-9651-5131A1BDED03}:1c6fIMu0fp */
        val &= ~(1 << bit_num);
        /* UserCode{EE57420F-3B82-4520-9651-5131A1BDED03} */
    }
    oak_unimac_io_write_32(np, reg, val);
    return;
}

/* Name      : set_rx_none
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring
 *  */
void oak_unimac_set_rx_none(oak_t* np, uint32_t ring)
{
    /* start of activity code */
    oak_unimac_io_write_32(np, OAK_UNI_RX_RING_MAP(ring), 0);
    /* UserCode{3347870D-6C91-41bc-85D2-7505A20E4F6B}:1X6PgBdMXD */
    oakdbg(debug, DRV, "clear np=%p chan=%d", np, ring);
    /* UserCode{3347870D-6C91-41bc-85D2-7505A20E4F6B} */

    return;
}

/* Name      : set_rx_8021Q_et
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint16_t etype = etype,  uint16_t pcp_vid = pcp_vid,  int enable = enable
 *  */
void oak_unimac_set_rx_8021Q_et(oak_t* np, uint32_t ring, uint16_t etype, uint16_t pcp_vid, int enable)
{
    /* automatically added for object flow handling */
    uint32_t val_1; /* start of activity code */

    if (enable != 0)
    {
        val_1 = (etype << 16) | pcp_vid;
        oak_unimac_io_write_32(np, OAK_UNI_RX_RING_ETYPE(ring), val_1);
    }
    else
    {
    }
    oak_unimac_set_bit_num(np, OAK_UNI_RX_RING_MAP(ring), 19, enable);
    /* UserCode{54732AF2-D7DC-4eef-9C14-1BD8E233E209}:1KZgJYihU2 */
    oakdbg(debug, DRV, "np=%p chan=%d etype=0x%x vid=0x%x enable=%d", np, ring, etype, pcp_vid, enable);

    /* UserCode{54732AF2-D7DC-4eef-9C14-1BD8E233E209} */

    return;
}

/* Name      : set_rx_8021Q_fid
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t fid = fid,  int enable = enable
 *  */
void oak_unimac_set_rx_8021Q_fid(oak_t* np, uint32_t ring, uint32_t fid, int enable)
{
    /* start of activity code */
    uint32_t val = oak_unimac_io_read_32(np, OAK_UNI_RX_RING_MAP(ring));
    val = oak_unimac_crt_bit_mask(21, 3, val, fid);

    if (enable != 0)
    {
        /* UserCode{CC81A5F7-4EC8-463f-86C1-B11C8CCAD407}:yOqKxWWCev */
        val |= (1 << 20);
        /* UserCode{CC81A5F7-4EC8-463f-86C1-B11C8CCAD407} */
    }
    else
    {
        /* UserCode{14162241-405B-4aea-B020-65B4A5324400}:crYfnQlhEO */
        val &= ~(1 << 20);
        /* UserCode{14162241-405B-4aea-B020-65B4A5324400} */
    }
    oak_unimac_io_write_32(np, OAK_UNI_RX_RING_MAP(ring), val);
    /* UserCode{38E74690-0B90-4afb-84E6-F2EB45107BEC}:C48zuwZJtc */
    oakdbg(debug, DRV, "np=%p chan=%d fid=0x%x enable=%d", np, ring, fid, enable);
    /* UserCode{38E74690-0B90-4afb-84E6-F2EB45107BEC} */

    return;
}

/* Name      : set_rx_8021Q_flow
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t flow_id = flow_id,  int enable = enable
 *  */
void oak_unimac_set_rx_8021Q_flow(oak_t* np, uint32_t ring, uint32_t flow_id, int enable)
{
    /* start of activity code */
    uint32_t val = oak_unimac_io_read_32(np, OAK_UNI_RX_RING_MAP(ring));
    val = oak_unimac_crt_bit_mask(14, 4, val, flow_id);

    if (enable != 0)
    {
        /* UserCode{2EB09F57-C5CF-4ef7-A525-9CAA3345203C}:xnmXhQCMWG */
        val |= (1 << 12);
        /* UserCode{2EB09F57-C5CF-4ef7-A525-9CAA3345203C} */
    }
    else
    {
        /* UserCode{19D6CC4C-0F90-4b5f-B9DB-E00FBD02D2EA}:z4TGUR240C */
        val &= ~(1 << 12);
        /* UserCode{19D6CC4C-0F90-4b5f-B9DB-E00FBD02D2EA} */
    }
    oak_unimac_io_write_32(np, OAK_UNI_RX_RING_MAP(ring), val);
    /* UserCode{4C3C4670-2975-48aa-84EA-E2F358837771}:1BRhY5ahDb */
    oakdbg(debug, DRV, "np=%p chan=%d flow_id=%d enable=%d", np, ring, flow_id, enable);
    /* UserCode{4C3C4670-2975-48aa-84EA-E2F358837771} */

    return;
}

/* Name      : set_rx_8021Q_qpri
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t qpri = qpri,  int enable = enable
 *  */
void oak_unimac_set_rx_8021Q_qpri(oak_t* np, uint32_t ring, uint32_t qpri, int enable)
{
    /* start of activity code */
    uint32_t val = oak_unimac_io_read_32(np, OAK_UNI_RX_RING_MAP(ring));
    val = oak_unimac_crt_bit_mask(4, 3, val, qpri);

    if (enable != 0)
    {
        /* UserCode{FF9DFF99-2246-4cc7-8BF0-235D08890309}:zUFuNcbrkJ */
        val |= (1 << 3);
        /* UserCode{FF9DFF99-2246-4cc7-8BF0-235D08890309} */
    }
    else
    {
        /* UserCode{F33ACC94-5FF8-48e7-94B4-65E1FD3236FB}:JCQ3NsexJP */
        val &= ~(1 << 3);
        /* UserCode{F33ACC94-5FF8-48e7-94B4-65E1FD3236FB} */
    }
    oak_unimac_io_write_32(np, OAK_UNI_RX_RING_MAP(ring), val);
    /* UserCode{28A3543D-C933-4c4f-97AD-184D06D878FF}:eMbDjinOmt */
    oakdbg(debug, DRV, "np=%p chan=%d qpri=%d enable=%d", np, ring, qpri, enable);
    /* UserCode{28A3543D-C933-4c4f-97AD-184D06D878FF} */

    return;
}

/* Name      : set_rx_8021Q_spid
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t spid = spid,  int enable = enable
 *  */
void oak_unimac_set_rx_8021Q_spid(oak_t* np, uint32_t ring, uint32_t spid, int enable)
{
    /* start of activity code */
    uint32_t val = oak_unimac_io_read_32(np, OAK_UNI_RX_RING_MAP(ring));
    val = oak_unimac_crt_bit_mask(8, 4, val, spid);

    if (enable != 0)
    {
        /* UserCode{BCBAB931-BCF8-414f-A321-D016EF6C9D31}:13uKPUkxtA */
        val |= (1 << 7);
        /* UserCode{BCBAB931-BCF8-414f-A321-D016EF6C9D31} */
    }
    else
    {
        /* UserCode{B362FD60-23EB-4eb7-8DBF-32BE2A0AD7B6}:1HEerxzX48 */
        val &= ~(1 << 7);
        /* UserCode{B362FD60-23EB-4eb7-8DBF-32BE2A0AD7B6} */
    }
    oak_unimac_io_write_32(np, OAK_UNI_RX_RING_MAP(ring), val);
    /* UserCode{9CE53B0E-9793-4632-AAEA-553223DE6FDD}:EHJfuqerKM */
    oakdbg(debug, DRV, "np=%p chan=%d spid=0x%x enable=%d", np, ring, spid, enable);
    /* UserCode{9CE53B0E-9793-4632-AAEA-553223DE6FDD} */

    return;
}

/* Name      : set_rx_da
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  unsigned char * addr = addr,  int enable = enable
 *  */
void oak_unimac_set_rx_da(oak_t* np, uint32_t ring, unsigned char* addr, int enable)
{
    /* automatically added for object flow handling */
    uint32_t val_1; /* automatically added for object flow handling */
    uint32_t val_4; /* start of activity code */

    if (enable != 0)
    {
        val_4 = (addr[2] << 0) | (addr[3] << 8) | (addr[4] << 16) | (addr[5] << 24);
        oak_unimac_io_write_32(np, OAK_UNI_RX_RING_DADDR_HI(ring), val_4);
        val_1 = (addr[0] << 0) | (addr[1] << 8);
        oak_unimac_io_write_32(np, OAK_UNI_RX_RING_DADDR_LO(ring), val_1);
    }
    else
    {
    }
    oak_unimac_set_bit_num(np, OAK_UNI_RX_RING_MAP(ring), 18, enable);
    /* UserCode{E9EF5738-9640-4060-B2D0-2671351CC3A7}:138D3eqSdw */
    oakdbg(debug, DRV, "np=%p chan=%d addr=0x%02x%02x%02x%02x%02x%02x enable=%d", np, ring, addr[0] & 0xFF, addr[1] & 0xFF, addr[2] & 0xFF, addr[3] & 0xFF, addr[4] & 0xFF, addr[5] & 0xFF, enable);
    /* UserCode{E9EF5738-9640-4060-B2D0-2671351CC3A7} */

    return;
}

/* Name      : set_rx_da_mask
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  unsigned char * addr = addr,  int enable = enable
 *  */
void oak_unimac_set_rx_da_mask(oak_t* np, uint32_t ring, unsigned char* addr, int enable)
{
    /* automatically added for object flow handling */
    uint32_t val_1; /* automatically added for object flow handling */
    uint32_t val_4; /* start of activity code */

    if (enable != 0)
    {
        val_1 = (addr[2] << 0) | (addr[3] << 8) | (addr[4] << 16) | (addr[5] << 24);
        oak_unimac_io_write_32(np, OAK_UNI_RX_RING_DADDR_MASK_HI(ring), val_1);
        val_4 = (addr[0] << 0) | (addr[1] << 8);
        oak_unimac_io_write_32(np, OAK_UNI_RX_RING_DADDR_MASK_LO(ring), val_4);
    }
    else
    {
        oak_unimac_io_write_32(np, OAK_UNI_RX_RING_DADDR_MASK_HI(ring), 0);
        oak_unimac_io_write_32(np, OAK_UNI_RX_RING_DADDR_MASK_LO(ring), 0);
    }
    /* UserCode{79B95407-1C1C-48d9-A0C9-A32850D63736}:138D3eqSdw */
    oakdbg(debug, DRV, "np=%p chan=%d addr=0x%02x%02x%02x%02x%02x%02x enable=%d", np, ring, addr[0] & 0xFF, addr[1] & 0xFF, addr[2] & 0xFF, addr[3] & 0xFF, addr[4] & 0xFF, addr[5] & 0xFF, enable);
    /* UserCode{79B95407-1C1C-48d9-A0C9-A32850D63736} */

    return;
}

/* Name      : set_rx_mgmt
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t val = val,  int enable = enable
 *  */
void oak_unimac_set_rx_mgmt(oak_t* np, uint32_t ring, uint32_t val, int enable)
{
    /* start of activity code */
    oak_unimac_set_bit_num(np, OAK_UNI_RX_RING_MAP(ring), 1, val);
    oak_unimac_set_bit_num(np, OAK_UNI_RX_RING_MAP(ring), 0, enable);
    /* UserCode{6C2934C1-3DFF-49d7-A6CB-BD849AE19207}:1b6eLtVT3q */
    oakdbg(debug, DRV, "np=%p chan=%d enable=%d", np, ring, enable);
    /* UserCode{6C2934C1-3DFF-49d7-A6CB-BD849AE19207} */

    return;
}

/* Name      : process_status
 * Returns   : void
 * Parameters:  ldg_t * ldg = ldg
 *  */
void oak_unimac_process_status(ldg_t* ldg)
{
    uint32_t irq_reason;
    uint32_t uni_status; /* start of activity code */
    irq_reason = oak_unimac_io_read_32(ldg->device, OAK_UNI_INTR);

    if ((irq_reason & OAK_UNI_INTR_SEVERE_ERRORS) != 0)
    {
        /* UserCode{D76D6F9D-BDC6-46f1-BA55-E0027EAF57F1}:aYRw8bhPY0 */
        oakdbg(debug, INTR, "SEVERE unimac irq reason: 0x%x", irq_reason & OAK_UNI_INTR_SEVERE_ERRORS);
        /* UserCode{D76D6F9D-BDC6-46f1-BA55-E0027EAF57F1} */
    }
    else
    {
    }

    if ((irq_reason & OAK_UNI_INTR_NORMAL_ERRORS) != 0)
    {
        /* UserCode{F289E260-B75D-4ae0-8E41-3A20AC0F4C1B}:HsbaYZJxr9 */
        oakdbg(debug, INTR, "NORMAL unimac irq reason: 0x%x", irq_reason & OAK_UNI_INTR_NORMAL_ERRORS);
        /* UserCode{F289E260-B75D-4ae0-8E41-3A20AC0F4C1B} */
    }
    else
    {
    }
    uni_status = oak_unimac_io_read_32(ldg->device, OAK_UNI_STAT);
    /* UserCode{0E7CDEC8-A920-4023-9E93-68EDA6AC239E}:1YOr1AthfM */
    oakdbg(debug, INTR, "unimac status: 0x%x", uni_status);
    /* UserCode{0E7CDEC8-A920-4023-9E93-68EDA6AC239E} */

    oak_unimac_io_write_32(ldg->device, OAK_UNI_INTR, irq_reason);
    return;
}

/* Name      : rx_error
 * Returns   : void
 * Parameters:  ldg_t * ldg = ldg,  uint32_t ring = ring
 *  */
void oak_unimac_rx_error(ldg_t* ldg, uint32_t ring)
{
    oak_t* np = ldg->device;
    oak_rx_chan_t* rxc;
    uint32_t reason; /* start of activity code */
    /* UserCode{8EB17680-5F9A-4414-A4F5-85D3434D28D6}:6CHCiyJgyD */
    rxc = &np->rx_channel[ring];
    /* UserCode{8EB17680-5F9A-4414-A4F5-85D3434D28D6} */

    /* UserCode{41D91B3C-E79D-4f1c-B6E7-E8069411A1A3}:1VHN4d4noG */
    reason = le32_to_cpup(&rxc->mbox->intr_cause);
    /* UserCode{41D91B3C-E79D-4f1c-B6E7-E8069411A1A3} */

    if ((reason & OAK_MBOX_RX_RES_LOW) != 0)
    {
        oak_net_rbr_refill(np, ring);
    }
    else
    {
        /* UserCode{B66B7D1B-B09C-4c5d-B04B-237B7C98FA8E}:1NoKR8WJIE */
        ++rxc->stat.rx_errors;
        /* UserCode{B66B7D1B-B09C-4c5d-B04B-237B7C98FA8E} */

        /* UserCode{296CD3C8-B419-4ea3-9202-238C7934D15F}:12dFpwVBkU */
        oakdbg(debug, RX_ERR, "reason=0x%x", reason);
        /* UserCode{296CD3C8-B419-4ea3-9202-238C7934D15F} */
    }
    return;
}

/* Name      : tx_error
 * Returns   : void
 * Parameters:  ldg_t * ldg = ldg,  uint32_t ring = ring
 *  */
void oak_unimac_tx_error(ldg_t* ldg, uint32_t ring)
{
    oak_t* np = ldg->device;
    oak_tx_chan_t* txc;
    uint32_t reason; /* start of activity code */
    /* UserCode{1838F067-C979-4a98-9868-8DECEA231BE4}:wZNMr276Oc */
    txc = &np->tx_channel[ring];
    /* UserCode{1838F067-C979-4a98-9868-8DECEA231BE4} */

    oak_unimac_io_write_32(np, OAK_UNI_TX_RING_INT_CAUSE(ring), (OAK_MBOX_TX_LATE_TS | OAK_MBOX_TX_ERR_HCRED));
    /* UserCode{B4A96019-63C2-4fd1-93DD-BA09FDC67F0A}:IErYYV5YzY */
    reason = le32_to_cpup(&txc->mbox->intr_cause);
    /* UserCode{B4A96019-63C2-4fd1-93DD-BA09FDC67F0A} */

    /* UserCode{094D907E-047B-4b13-AFAD-EFB43B7BD542}:1FBsSJ88YK */
    ++txc->stat.tx_errors;
    /* UserCode{094D907E-047B-4b13-AFAD-EFB43B7BD542} */

    /* UserCode{731AE2AB-4D18-4539-B30F-985390838F87}:12hOdS3uRR */
    oakdbg(debug, TX_ERR, "reason=0x%x", reason);
    /* UserCode{731AE2AB-4D18-4539-B30F-985390838F87} */

    return;
}

/* Name      : ena_rx_ring_irq
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t enable = enable
 *  */
void oak_unimac_ena_rx_ring_irq(oak_t* np, uint32_t ring, uint32_t enable)
{
    /* start of activity code */

    if (enable != 0)
    {
        /* UserCode{19E6E8BB-F457-4559-839E-0470F35A1145}:d1mRe6tH4T */
        enable = OAK_MBOX_RX_COMP | OAK_MBOX_RX_RES_LOW;

        /* UserCode{19E6E8BB-F457-4559-839E-0470F35A1145} */
    }
    else
    {
    }
    oak_unimac_io_write_32(np, OAK_UNI_RX_RING_INT_MASK(ring), enable);
    return;
}

/* Name      : ena_tx_ring_irq
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t enable = enable
 *  */
void oak_unimac_ena_tx_ring_irq(oak_t* np, uint32_t ring, uint32_t enable)
{
    /* start of activity code */

    if (enable != 0)
    {
        /* UserCode{41C47FBA-29DF-4115-978E-769F649B8494}:ahdmOpmOf1 */
        enable = OAK_MBOX_TX_COMP;
        /* UserCode{41C47FBA-29DF-4115-978E-769F649B8494} */

        if (ring >= 2)
        {
            /* UserCode{DF3344BF-8EE7-4a06-A3D7-496707E958A0}:144KHqzaOZ */
            enable |= (OAK_MBOX_TX_LATE_TS | OAK_MBOX_TX_ERR_HCRED);
            /* UserCode{DF3344BF-8EE7-4a06-A3D7-496707E958A0} */
        }
        else
        {
        }
    }
    else
    {
    }
    oak_unimac_io_write_32(np, OAK_UNI_TX_RING_INT_MASK(ring), enable);
    return;
}

/* Name      : set_tx_ring_rate
 * Returns   : int
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t sr_class = sr_class,  uint32_t hi_credit = hi_credit,  uint32_t r_kbps = r_kbps
 *  */
int oak_unimac_set_tx_ring_rate(oak_t* np, uint32_t ring, uint32_t sr_class, uint32_t hi_credit, uint32_t r_kbps)
{
    int rc = -EFAULT;
    uint32_t val; /* automatically added for object flow handling */
    int rc_4; /* start of activity code */

    if (ring >= 2 && ring <= 9)
    {

        if (hi_credit <= OAK_MAX_TX_HI_CREDIT_BYTES)
        {
            /* UserCode{04A7E3B9-5E57-4512-9DA0-76250D53999C}:LGYvLzxYOE */
            val = ((sr_class & 1) << 31);
            val |= ((hi_credit & OAK_MAX_TX_HI_CREDIT_BYTES) << 17);
            val |= (r_kbps & 0x1FFFF);

            /* UserCode{04A7E3B9-5E57-4512-9DA0-76250D53999C} */

            oak_unimac_io_write_32(np, OAK_UNI_TX_RING_RATECTRL(ring), val);
            /* UserCode{B1049F01-5835-43e7-86CC-B498F62071BF}:r8srRGlzXT */
            rc = 0;
            /* UserCode{B1049F01-5835-43e7-86CC-B498F62071BF} */
        }
        else
        {
        }
    }
    else
    {
    }
    rc_4 = rc;
    /* UserCode{A7E2F660-F5CD-49d1-8A25-373E8CD16342}:1R3uCGYjP5 */
    oakdbg(debug, DRV, " np=%p ring=%d sr_class=%d hi_credit=%d kbps=%d rc=%d", np, ring, sr_class, hi_credit, r_kbps, rc);

    /* UserCode{A7E2F660-F5CD-49d1-8A25-373E8CD16342} */

    return rc_4;
}

/* Name      : clr_tx_ring_rate
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring
 *  */
void oak_unimac_clr_tx_ring_rate(oak_t* np, uint32_t ring)
{
    /* start of activity code */

    if (ring >= 2 && ring <= 9)
    {
        uint32_t val = oak_unimac_io_read_32(np, OAK_UNI_TX_RING_RATECTRL(ring));
        /* UserCode{626B4AB2-8073-4d7e-AA2F-9E902D4762E9}:12GRnqvVtD */
        val &= 0x7FFF0000;

        /* UserCode{626B4AB2-8073-4d7e-AA2F-9E902D4762E9} */

        oak_unimac_io_write_32(np, OAK_UNI_TX_RING_RATECTRL(ring), val);
    }
    else
    {
    }
    return;
}

/* Name      : set_tx_mac_rate
 * Returns   : int
 * Parameters:  oak_t * np = np,  uint32_t sr_class = sr_class,  uint32_t hi_credit = hi_credit,  uint32_t r_kbps = r_kbps
 *  */
int oak_unimac_set_tx_mac_rate(oak_t* np, uint32_t sr_class, uint32_t hi_credit, uint32_t r_kbps)
{
    int rc = -EFAULT;
    uint32_t val; /* automatically added for object flow handling */
    int rc_7 = -EFAULT; /* start of activity code */

    if (hi_credit <= OAK_MAX_TX_HI_CREDIT_BYTES)
    {
        /* UserCode{2A3E8E3E-6DA1-4815-90DA-815B841751E5}:Hsuq3YPwnM */
        val = ((hi_credit & OAK_MAX_TX_HI_CREDIT_BYTES) << 17);
        val |= (r_kbps & 0x1FFFF);

        /* UserCode{2A3E8E3E-6DA1-4815-90DA-815B841751E5} */

        if (sr_class == OAK_MIN_TX_RATE_CLASS_A)
        {
            oak_unimac_io_write_32(np, OAK_UNI_TXRATE_A, val);
        }
        else
        {
            oak_unimac_io_write_32(np, OAK_UNI_TXRATE_B, val);
        }
        /* UserCode{AE6D40F1-A6D7-458c-9A58-6F8862844221}:r8srRGlzXT */
        rc = 0;
        /* UserCode{AE6D40F1-A6D7-458c-9A58-6F8862844221} */
    }
    else
    {
    }
    rc_7 = rc;
    return rc_7;
}

/* Name      : set_sched_round_robin
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t weight = weight,  int reg = rx_flag
 *  */
void oak_unimac_set_sched_round_robin(oak_t* np, uint32_t ring, uint32_t weight, int reg)
{
    uint32_t val; /* start of activity code */

    if (ring >= 0 && ring <= 9)
    {

        if (np->pci_class_revision >= 1)
        {

            if (reg == OAK_UNI_DMA_TX_CH_CFG)
            {
                val = oak_unimac_io_read_32(np, reg);
                /* UserCode{9C60C359-8CED-4574-95DB-284E9AD9C709}:1RxfOTRwHE */
                val &= ~(1 << 10);
                /* UserCode{9C60C359-8CED-4574-95DB-284E9AD9C709} */

                oak_unimac_io_write_32(np, reg, val);
                oak_unimac_set_sched_arbit_value(np, ring, weight, OAK_UNI_DMA_TX_CH_SCHED_B0_LO);
            }
            else
            {
            }
        }
        else
        {
            val = oak_unimac_io_read_32(np, reg);
            /* UserCode{0530414A-173D-4377-A82A-50187BA31499}:1RxfOTRwHE */
            val &= ~(1 << 10);
            /* UserCode{0530414A-173D-4377-A82A-50187BA31499} */

            oak_unimac_io_write_32(np, reg, val);

            if (reg == OAK_UNI_DMA_RX_CH_CFG)
            {
                oak_unimac_set_sched_arbit_value(np, ring, weight, OAK_UNI_DMA_RX_CH_SCHED_LO);
            }
            else
            {
                oak_unimac_set_sched_arbit_value(np, ring, weight, OAK_UNI_DMA_TX_CH_SCHED_LO);
            }
        }
    }
    else
    {
    }
    return;
}

/* Name      : set_sched_priority_based
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t prio = prio,  uint32_t reg = rx_flag
 *  */
void oak_unimac_set_sched_priority_based(oak_t* np, uint32_t ring, uint32_t prio, uint32_t reg)
{
    uint32_t val; /* start of activity code */

    if (ring >= 0 && ring <= 9)
    {

        if (np->pci_class_revision >= 1)
        {

            if (reg == OAK_UNI_DMA_TX_CH_CFG)
            {

                if (np->rrs >= 16)
                {
                    val = oak_unimac_io_read_32(np, reg);
                    /* UserCode{6378AF32-5902-41eb-AD8C-40ACC33CB42E}:t9Ns8TMrWt */
                    val &= ~0x7F; /* Max burst size */

                    val |= ((np->rrs / 8) - 1) & 0x7F;
                    /* UserCode{6378AF32-5902-41eb-AD8C-40ACC33CB42E} */

                    /* UserCode{D2B1B1E2-A6C9-4c32-8DD6-B567F7C0EF26}:NC5kd3Yxed */
                    val |= (1 << 10); /* RR: bit-10 == 1 */

                    /* UserCode{D2B1B1E2-A6C9-4c32-8DD6-B567F7C0EF26} */

                    oak_unimac_io_write_32(np, reg, val);
                    /* UserCode{FC855315-00BD-43b2-B4AB-390C23863731}:1d6xMkzvo0 */
                    oakdbg(debug, DRV, "TX max burst size: %d, val=0x%x", np->rrs, val);
                    /* UserCode{FC855315-00BD-43b2-B4AB-390C23863731} */
                }
                else
                {
                }
                oak_unimac_set_sched_arbit_value(np, ring, prio, OAK_UNI_DMA_TX_CH_SCHED_B0_LO);
            }
            else
            {
            }
        }
        else
        {
            val = oak_unimac_io_read_32(np, reg);
            /* UserCode{FBB9B9FC-60D6-4e8e-A675-D1FDA782CCF2}:1TfUTI6jrp */
            val |= (1 << 10);
            /* UserCode{FBB9B9FC-60D6-4e8e-A675-D1FDA782CCF2} */

            oak_unimac_io_write_32(np, reg, val);

            if (reg == OAK_UNI_DMA_RX_CH_CFG)
            {
                oak_unimac_set_sched_arbit_value(np, ring, prio, OAK_UNI_DMA_RX_CH_SCHED_LO);
            }
            else
            {
                oak_unimac_set_sched_arbit_value(np, ring, prio, OAK_UNI_DMA_TX_CH_SCHED_LO);
            }
        }
    }
    else
    {
    }
    return;
}

/* Name      : start_all_txq
 * Returns   : int
 * Parameters:  oak_t * np = np,  uint32_t enable = enable
 *  */
int oak_unimac_start_all_txq(oak_t* np, uint32_t enable)
{
    int rc = 0;
    uint32_t i = 0; /* automatically added for object flow handling */
    int rc_1 = 0; /* start of activity code */
    while (i < np->num_tx_chan)
    {
        /* UserCode{16675A47-62F2-4385-B802-F03BBCA2CA1E}:1IVEixMeH5 */
        rc = oak_unimac_start_tx_ring(np, i, enable);
        /* UserCode{16675A47-62F2-4385-B802-F03BBCA2CA1E} */

        if (rc == 0)
        {
            /* UserCode{09F3F655-7387-4586-A50D-C4353557197C}:WyHqPMY9K4 */
            rc = -EFAULT;
            break;
            /* UserCode{09F3F655-7387-4586-A50D-C4353557197C} */
        }
        else
        {
            /* UserCode{39A51EBA-3AF2-491b-9C01-215C103CAAAA}:r8srRGlzXT */
            rc = 0;
            /* UserCode{39A51EBA-3AF2-491b-9C01-215C103CAAAA} */
        }
        /* UserCode{3A378773-C13A-40dc-BD13-1B3CA3C9B844}:pqeK47KwaC */
        ++i;
        /* UserCode{3A378773-C13A-40dc-BD13-1B3CA3C9B844} */
    }

    rc_1 = rc;
    /* UserCode{60DFD38A-6C76-4a54-9B29-693C32818F77}:1bMYUtxg8s */
    oakdbg(debug, IFUP, " rc: %d", rc);
    /* UserCode{60DFD38A-6C76-4a54-9B29-693C32818F77} */

    return rc_1;
}

/* Name      : start_all_rxq
 * Returns   : int
 * Parameters:  oak_t * np = np,  uint32_t enable = enable
 *  */
int oak_unimac_start_all_rxq(oak_t* np, uint32_t enable)
{
    int rc = 0;
    uint32_t i = 0; /* automatically added for object flow handling */
    int rc_1 = 0; /* start of activity code */
    while (i < np->num_rx_chan)
    {
        /* UserCode{7730FE97-CE99-455b-861F-5AF4D458874F}:oCxCTkVbJ6 */
        rc = oak_unimac_start_rx_ring(np, i, enable);
        /* UserCode{7730FE97-CE99-455b-861F-5AF4D458874F} */

        if (rc == 0)
        {
            /* UserCode{F67242AF-0EBC-442e-9E9B-02E214D19B77}:WyHqPMY9K4 */
            rc = -EFAULT;
            break;
            /* UserCode{F67242AF-0EBC-442e-9E9B-02E214D19B77} */
        }
        else
        {
            /* UserCode{EA42FC77-93F0-4ece-B931-FBEBECAC2C72}:r8srRGlzXT */
            rc = 0;
            /* UserCode{EA42FC77-93F0-4ece-B931-FBEBECAC2C72} */
        }
        /* UserCode{5F2DE187-634F-4420-B6DB-B9988A80CD3E}:pqeK47KwaC */
        ++i;
        /* UserCode{5F2DE187-634F-4420-B6DB-B9988A80CD3E} */
    }

    rc_1 = rc;
    /* UserCode{514DFD82-B1D3-48b7-9F20-66C180B9B515}:1bMYUtxg8s */
    oakdbg(debug, IFUP, " rc: %d", rc);
    /* UserCode{514DFD82-B1D3-48b7-9F20-66C180B9B515} */

    return rc_1;
}

/* Name      : set_channel_dma
 * Returns   : void
 * Parameters:  oak_t * np = np,  int rto,  int rxs = rxs,  int txs = txd,  int chan = chan
 *  */
static void oak_unimac_set_channel_dma(oak_t* np, int rto, int rxs, int txs, int chan)
{
    int i = 0;
    uint32_t val; /* start of activity code */
    while (i < np->num_rx_chan)
    {
        /* UserCode{8DA7B369-5AF2-41ae-8B8F-C1D1BA40DDED}:DKVcjyHrMa */
        oak_unimac_set_dma_addr(np, np->rx_channel[i].rbr_dma, OAK_UNI_RX_RING_DBASE_LO(i), OAK_UNI_RX_RING_DBASE_HI(i));
        oak_unimac_set_dma_addr(np, np->rx_channel[i].rsr_dma, OAK_UNI_RX_RING_SBASE_LO(i), OAK_UNI_RX_RING_SBASE_HI(i));
        oak_unimac_set_dma_addr(np, np->rx_channel[i].mbox_dma, OAK_UNI_RX_RING_MBASE_LO(i), OAK_UNI_RX_RING_MBASE_HI(i));

        /* UserCode{8DA7B369-5AF2-41ae-8B8F-C1D1BA40DDED} */

        /* UserCode{DB560743-2406-4a6c-A84D-D17CC2EFAF21}:8GTxmc108K */
        val = (np->rx_channel[i].rbr_bsize & 0xFFF8) << 16;
        val |= rxs;

        /* UserCode{DB560743-2406-4a6c-A84D-D17CC2EFAF21} */

        oak_unimac_io_write_32(np, OAK_UNI_RX_RING_CFG(i), val);
        oak_unimac_io_write_32(np, OAK_UNI_RX_RING_PREF_THR(i), RX_DESC_PREFETCH_TH);
        /* UserCode{E4BAAE68-33FC-490d-9175-115F9FEEFEB9}:1SZxc33SNN */
        val = (np->rx_channel[i].rbr_size / 4);
        val <<= 16;

        /* UserCode{E4BAAE68-33FC-490d-9175-115F9FEEFEB9} */

        oak_unimac_io_write_32(np, OAK_UNI_RX_RING_WATERMARK(i), val);
        /* UserCode{408ED877-B742-4a67-8700-345621A8DF75}:1DIBglwMvF */
        val = (np->rx_channel[i].rbr_size / 8);
        val = ilog2(val);
        if (val > 32)
        {
            val = 5;
        }

        /* UserCode{408ED877-B742-4a67-8700-345621A8DF75} */

        oak_unimac_io_write_32(np, OAK_UNI_RX_RING_MBOX_THR(i), val);
        oak_unimac_io_write_32(np, OAK_UNI_RX_RING_TIMEOUT(i), OAK_RING_TOUT_USEC(rto));

        if (np->pci_class_revision >= 1)
        {
            oak_unimac_set_arbit_priority_based(np, i, 0, OAK_UNI_DMA_RX_CH_CFG);
        }
        else
        {
            oak_unimac_set_sched_priority_based(np, i, 0, OAK_UNI_DMA_RX_CH_CFG);
        }
        /* UserCode{EB651B65-1D45-4b2d-B369-9CD7E90797F5}:pqeK47KwaC */
        ++i;
        /* UserCode{EB651B65-1D45-4b2d-B369-9CD7E90797F5} */
    }

    /* UserCode{BF2398FA-A034-4a17-BD26-0A983386DA2F}:7233eLMZuK */
    i = 0;
    /* UserCode{BF2398FA-A034-4a17-BD26-0A983386DA2F} */

    while (i < np->num_tx_chan)
    {
        /* UserCode{80F57145-1F0D-407b-B345-889DB3F57BF7}:XmXtQqe9RN */
        oak_unimac_set_dma_addr(np, np->tx_channel[i].tbr_dma, OAK_UNI_TX_RING_DBASE_LO(i), OAK_UNI_TX_RING_DBASE_HI(i));
        oak_unimac_set_dma_addr(np, np->tx_channel[i].mbox_dma, OAK_UNI_TX_RING_MBASE_LO(i), OAK_UNI_TX_RING_MBASE_HI(i));

        /* UserCode{80F57145-1F0D-407b-B345-889DB3F57BF7} */

        oak_unimac_io_write_32(np, OAK_UNI_TX_RING_CFG(i), txs);
        oak_unimac_io_write_32(np, OAK_UNI_TX_RING_PREF_THR(i), TX_DESC_PREFETCH_TH);
        oak_unimac_io_write_32(np, OAK_UNI_TX_RING_MBOX_THR(i), TX_MBOX_WRITE_TH);
        oak_unimac_clr_tx_ring_rate(np, i);
        oak_unimac_io_write_32(np, OAK_UNI_TX_RING_TIMEOUT(i), OAK_RING_TOUT_MSEC(10));

        if (np->pci_class_revision >= 1)
        {
            oak_unimac_set_arbit_priority_based(np, i, 0, OAK_UNI_DMA_TX_CH_CFG);
        }
        else
        {
        }
        oak_unimac_set_sched_priority_based(np, i, 0, OAK_UNI_DMA_TX_CH_CFG);
        /* UserCode{F74F4F7A-F126-446a-9308-E0B1ADDE71C8}:pqeK47KwaC */
        ++i;
        /* UserCode{F74F4F7A-F126-446a-9308-E0B1ADDE71C8} */
    }

    /* UserCode{32E9601B-3684-4eea-9652-1B7DC3A53882}:QO9kauWoM6 */
    netif_set_real_num_tx_queues(np->netdev, np->num_tx_chan);
    netif_set_real_num_rx_queues(np->netdev, np->num_rx_chan);
    /* UserCode{32E9601B-3684-4eea-9652-1B7DC3A53882} */

    return;
}

/* Name      : ena_ring
 * Returns   : uint32_t
 * Parameters:  oak_t * np = np,  uint32_t addr = addr,  uint32_t enable = enable
 *  */
static uint32_t oak_unimac_ena_ring(oak_t* np, uint32_t addr, uint32_t enable)
{
    uint32_t result;
    uint32_t count; /* automatically added for object flow handling */
    uint32_t rc_7; /* start of activity code */

    if (enable != 0)
    {
        /* UserCode{E0CA86B9-C5B6-4463-8A0D-58E5CE8A7D9C}:kRWUhBJs0H */
        enable = OAK_UNI_RING_ENABLE_REQ | OAK_UNI_RING_ENABLE_DONE;
        /* UserCode{E0CA86B9-C5B6-4463-8A0D-58E5CE8A7D9C} */
    }
    else
    {
    }
    oak_unimac_io_write_32(np, addr, enable & OAK_UNI_RING_ENABLE_REQ);
    /* UserCode{9B1514BE-E85C-448d-B871-0A6896D9C349}:1UQ6qp7ODS */
    count = 1000;
    /* UserCode{9B1514BE-E85C-448d-B871-0A6896D9C349} */

    do
    {
        result = oak_unimac_io_read_32(np, addr);
        /* UserCode{D0EB1737-EB91-4312-B2D6-8B0B215C6719}:9wEChOWkbj */
        --count;
        /* UserCode{D0EB1737-EB91-4312-B2D6-8B0B215C6719} */

    } while (((enable & OAK_UNI_RING_ENABLE_DONE) != (result & OAK_UNI_RING_ENABLE_DONE)) && (count > 0));

/* UserCode{0295923C-692D-42d8-8699-89819C76E2B6}:84LgX3l0js */
#ifdef SIMULATION
    count = 1;
#endif
    /* UserCode{0295923C-692D-42d8-8699-89819C76E2B6} */

    rc_7 = count;
    return rc_7;
}

/* Name      : set_sched_arbit_value
 * Returns   : void
 * Parameters:  oak_t * np = np,  uint32_t ring = ring,  uint32_t weight = weight,  int reg = rx_flag
 *  */
static void oak_unimac_set_sched_arbit_value(oak_t* np, uint32_t ring, uint32_t weight, int reg)
{
    uint32_t val; /* start of activity code */

    if (ring <= 7)
    {
        /* UserCode{22BF66B1-169B-4991-AD57-8798D708C419}:1Hdia2nRiP */
        ring = ring * 4;
        /* UserCode{22BF66B1-169B-4991-AD57-8798D708C419} */
    }
    else
    {
        /* UserCode{07528988-1118-4dcf-A604-07DC944ADE08}:1H2sl7G9UQ */
        ring = (ring - 8) * 4;
        reg += 4;
        /* UserCode{07528988-1118-4dcf-A604-07DC944ADE08} */
    }
    val = oak_unimac_io_read_32(np, reg);
    /* UserCode{58691998-1053-49ec-83A0-F732EB52098E}:LNPRp0j7RP */
    val &= ~(0xF << ring);
    val |= (weight << ring);
    /* UserCode{58691998-1053-49ec-83A0-F732EB52098E} */

    oak_unimac_io_write_32(np, reg, val);
    return;
}

/* Name      : start_tx_ring
 * Returns   : uint32_t
 * Parameters:  oak_t * np,  int32_t ring,  uint32_t enable
 *  */
uint32_t oak_unimac_start_tx_ring(oak_t* np, int32_t ring, uint32_t enable)
{
    /* UserCode{D55622D0-04A6-45ec-BDF3-2AD137C55AB4}:1ZvLiAOlLN */
    if (enable != 0)
    {
        oak_unimac_io_write_32(np, OAK_UNI_TX_RING_INT_CAUSE(ring), (OAK_MBOX_TX_COMP | OAK_MBOX_TX_LATE_TS | OAK_MBOX_TX_ERR_HCRED));
        oak_unimac_io_write_32(np, OAK_UNI_TX_RING_CPU_PTR(ring), 0);
    }
    return (oak_unimac_ena_ring(np, OAK_UNI_TX_RING_EN(ring), enable));
    /* UserCode{D55622D0-04A6-45ec-BDF3-2AD137C55AB4} */
}

/* Name      : start_rx_ring
 * Returns   : uint32_t
 * Parameters:  oak_t * np,  uint32_t ring,  uint32_t enable
 *  */
uint32_t oak_unimac_start_rx_ring(oak_t* np, uint32_t ring, uint32_t enable)
{
    /* UserCode{5787CBB6-FD4E-4b65-821C-4C8F8A440E96}:1OsZS6oyyn */
    if (enable != 0)
    {
        oak_unimac_io_write_32(np, OAK_UNI_RX_RING_INT_CAUSE(ring), (OAK_MBOX_RX_COMP | OAK_MBOX_RX_RES_LOW));
    }
    return (oak_unimac_ena_ring(np, OAK_UNI_RX_RING_EN(ring), enable));
    /* UserCode{5787CBB6-FD4E-4b65-821C-4C8F8A440E96} */
}

/* Name      : set_dma_addr
 * Returns   : void
 * Parameters:  oak_t * np,  dma_addr_t phys,  uint32_t reg_lo,  uint32_t reg_hi
 *  */
static void oak_unimac_set_dma_addr(oak_t* np, dma_addr_t phys, uint32_t reg_lo, uint32_t reg_hi)
{
    /* UserCode{5C63BD81-45AD-47ad-BAE8-51E584157C38}:14Etb7pLfg */
    uint32_t addr;
    addr = (phys & 0xFFFFFFFF); /* Low 32 bit */
    oak_unimac_io_write_32(np, reg_lo, addr);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
    addr = ((phys >> 32) & 0xFFFFFFFF); /* High 32 bit */
#else
    addr = 0;
#endif
    oak_unimac_io_write_32(np, reg_hi, addr);

    /* UserCode{5C63BD81-45AD-47ad-BAE8-51E584157C38} */
}

