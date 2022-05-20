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
#include "oak_net.h"
#include "oak_ethtool.h"
#include "oak_chksum.h"

extern int debug;
extern int rxs;
extern int txs;
extern int chan;
extern int rto;
extern int mhdr;
extern int port_speed;

/* private function prototypes */
static void oak_net_esu_ena_mrvl_hdr(oak_t* np);
static struct sk_buff* oak_net_tx_prepend_mrvl_hdr(struct sk_buff* skb);
static int oak_net_tx_packet(oak_t* np, struct sk_buff* skb, uint16_t txq);
static int oak_net_tx_work(ldg_t* ldg, uint32_t ring, int budget);
static int oak_net_rx_work(ldg_t* ldg, uint32_t ring, int budget);
static int oak_net_process_rx_pkt(oak_rx_chan_t* rxc, uint32_t desc_num, struct sk_buff** target);
static int oak_net_process_channel(ldg_t* ldg, uint32_t ring, uint32_t reason, int budget);
static int oak_net_poll(struct napi_struct* napi, int budget);
static int oak_net_poll_core(oak_t* np, ldg_t* ldg, int budget);
static int oak_net_stop_tx_queue(oak_t* np, uint32_t nfrags, uint16_t txq);

/* Name      : esu_ena_mrvl_hdr
 * Returns   : void
 * Parameters:  oak_t * np = np
 *  */
static void oak_net_esu_ena_mrvl_hdr(oak_t* np)
{
    uint32_t offs = 0x10000 | (4 << 2) | (0xB << 7);
    uint32_t data = 0x007f; /* start of activity code */

    if (mhdr != 0)
    {
        /* UserCode{BFDF3BD4-AC01-4e83-8CB4-5EA477D0F86C}:1dWJf0ZV5o */
        data |= 0x0800;
        /* UserCode{BFDF3BD4-AC01-4e83-8CB4-5EA477D0F86C} */
    }
    else
    {
    }
    /* UserCode{90097D08-D674-41d2-8D7B-7C141E8E4BF7}:1S1brboJR3 */
    oakdbg(debug, PROBE, "PCI class revision: 0x%x\n", np->pci_class_revision);
    /* UserCode{90097D08-D674-41d2-8D7B-7C141E8E4BF7} */

    /* UserCode{C2E6865D-83D4-445a-B079-585E5E2C978D}:19WRvjU3yH */
    sw32(np, offs, data);
    /* UserCode{C2E6865D-83D4-445a-B079-585E5E2C978D} */

    if ((mhdr != 0) && (np->pci_class_revision >= 1))
    {
        /* UserCode{D4A77A32-F3CA-49ab-9C26-3BB142E01686}:pLjAEuiHnY */
        oakdbg(debug, PROBE, "No MRVL header generation in SW");
        /* UserCode{D4A77A32-F3CA-49ab-9C26-3BB142E01686} */

        /* UserCode{225E05B7-E73A-423a-AA1A-F8E250E725FF}:j7aNbPV5ra */
        mhdr = 0;
        /* UserCode{225E05B7-E73A-423a-AA1A-F8E250E725FF} */
    }
    else
    {
    }
    return;
}

/* Name      : esu_set_mtu
 * Returns   : int
 * Parameters: struct net_device * net_dev = net_dev,  int new_mtu = new_mtu
 * Description: This function set the MTU size of the Ethernet interface.
 *  */
int oak_net_esu_set_mtu(struct net_device* net_dev, int new_mtu)
{
    oak_t* np = netdev_priv(net_dev);
    uint32_t offs = 0x10000 | (8 << 2) | (0xB << 7);
    uint32_t fs = new_mtu + (ETH_HLEN + ETH_FCS_LEN);
    uint32_t data; /* automatically added for object flow handling */
    int rc_1 = 0; /* start of activity code */
    /* UserCode{59AB9360-4F36-41fc-A82C-84E028BB2E45}:tDUl6BNW0Y */
    data = sr32(np, offs);
    data &= ~(3 << 12);
    /* UserCode{59AB9360-4F36-41fc-A82C-84E028BB2E45} */

    if (fs > 1522)
    {

        if (fs <= 2048)
        {
            /* UserCode{7262AD93-8E88-4b6f-8BAC-873F46D9BC35}:1JHxzimhdX */
            data |= (1 << 12);
            /* UserCode{7262AD93-8E88-4b6f-8BAC-873F46D9BC35} */
        }
        else
        {
            /* UserCode{E7B005BD-EB3B-44d1-A30D-C779F6E09A78}:1DERkBQy2Q */
            data |= (2 << 12);
            /* UserCode{E7B005BD-EB3B-44d1-A30D-C779F6E09A78} */
        }
    }
    else
    {
    }
    /* UserCode{1A9E69CF-33F3-4e73-A6B2-C7392F68FC32}:1f1SN5lR3J */
    oakdbg(debug, PROBE, "MTU %d/%d data=0x%x", new_mtu, fs, data);
    /* UserCode{1A9E69CF-33F3-4e73-A6B2-C7392F68FC32} */

    /* UserCode{D4989027-5836-4adb-A865-9058BC925696}:ya0FogcAyq */
    net_dev->mtu = new_mtu;
    /* UserCode{D4989027-5836-4adb-A865-9058BC925696} */

    /* UserCode{A8EF72C3-CFD9-445e-AE49-DB55FAC645EA}:19WRvjU3yH */
    sw32(np, offs, data);
    /* UserCode{A8EF72C3-CFD9-445e-AE49-DB55FAC645EA} */

    rc_1 = 0;
    return rc_1;
}

/* Name      : esu_ena_speed
 * Returns   : void
 * Parameters:  int gbit = gbit,  oak_t * np = np
 *  */
void oak_net_esu_ena_speed(int gbit, oak_t* np)
{
    uint32_t data;
    uint32_t offs = 0x10000 | (1 << 2) | (0xB << 7); /* start of activity code */

    gbit = oak_ethtool_cap_cur_speed(np, gbit);
    np->speed = gbit;
    pr_info("oak: device=0x%x speed=%dGbps\n",np->pdev->device,gbit);

    if (gbit == 10)
    {
        /* UserCode{E4E5AE71-151B-4d11-97F7-E05A759CD8F8}:maWeCmTlzd */
        data = 0x201f;
        /* UserCode{E4E5AE71-151B-4d11-97F7-E05A759CD8F8} */
    }
    else
    {

        if (gbit == 5)
        {
            /* UserCode{5E051748-ED51-4f4c-B8CB-63479B8A2FF6}:sqMZzhgjrM */
            data = 0x301f;
            /* UserCode{5E051748-ED51-4f4c-B8CB-63479B8A2FF6} */
        }
        else
        {
            /* UserCode{61972CC7-69E1-4a33-99ED-F9A567D3F2E2}:8jn5Iqhtx5 */
            data = 0x1013;
            /* UserCode{61972CC7-69E1-4a33-99ED-F9A567D3F2E2} */
        }
    }
    /* UserCode{2C5C84F7-A827-4ba8-9863-AF9D9834C69F}:1GLbcCEP6r */
    sw32(np, offs, data);
    msleep(10);
    /* UserCode{2C5C84F7-A827-4ba8-9863-AF9D9834C69F} */

    if (gbit == 10)
    {
        /* UserCode{F3CE7461-607E-44f8-B3C7-543417468B94}:Z69e99JPOh */
        data = 0x203f;
        /* UserCode{F3CE7461-607E-44f8-B3C7-543417468B94} */
    }
    else
    {

        if (gbit == 5)
        {
            /* UserCode{76D384AF-0208-4e77-A5F0-918A30D1CEBA}:SnBHjPzkOs */
            data = 0x303f;
            /* UserCode{76D384AF-0208-4e77-A5F0-918A30D1CEBA} */
        }
        else
        {
            /* UserCode{9123ACFA-4923-4046-93CD-58B37742ADE8}:WXLFFo9gYE */
            data = 0x1033;
            /* UserCode{9123ACFA-4923-4046-93CD-58B37742ADE8} */
        }
    }
    /* UserCode{F04E8CB6-D4FC-432e-B477-4264CAB52B98}:1GLbcCEP6r */
    sw32(np, offs, data);
    msleep(10);
    /* UserCode{F04E8CB6-D4FC-432e-B477-4264CAB52B98} */

    /* UserCode{B535658C-4966-4fbb-81A6-AEBF088CA987}:BRQ6lfjiK2 */
    oakdbg(debug, PROBE, "Unimac %d Gbit speed enabled", gbit == 1 ? 1 : 10);
    /* UserCode{B535658C-4966-4fbb-81A6-AEBF088CA987} */

    return;
}

/* Name      : tx_prepend_mrvl_hdr
 * Returns   : struct sk_buff *
 * Parameters:  struct sk_buff * skb = skb
 *  */
static struct sk_buff* oak_net_tx_prepend_mrvl_hdr(struct sk_buff* skb)
{
    struct sk_buff* nskb;
    char* hdr; /* automatically added for object flow handling */
    struct sk_buff* ret_1 = skb; /* start of activity code */

    if (unlikely(skb_headroom(skb) < 2))
    {
        /* UserCode{755249AF-E544-40af-B186-6EE38C143F8C}:eYldSFGzBU */
        nskb = skb_realloc_headroom(skb, 2);
        /* UserCode{755249AF-E544-40af-B186-6EE38C143F8C} */

        /* UserCode{573D59D0-0EE1-4e40-A523-BB8B0AEB171C}:1ZhlrL8IAO */
        dev_kfree_skb(skb);
        skb = nskb;
        /* UserCode{573D59D0-0EE1-4e40-A523-BB8B0AEB171C} */
    }
    else
    {
    }

    if (skb != NULL)
    {
        /* UserCode{7488D48C-2AEC-48af-80EE-073FF4A24A52}:TSlkPUByf6 */
        hdr = (char*)skb_push(skb, 2);
        memset(hdr, 0, 2);
        /* UserCode{7488D48C-2AEC-48af-80EE-073FF4A24A52} */

        ret_1 = skb;
    }
    else
    {
    }
    return ret_1;
}

/* Name      : rbr_refill
 * Returns   : int
 * Parameters:  oak_t * np = np,  uint32_t ring = ring
 *  */
int oak_net_rbr_refill(oak_t* np, uint32_t ring)
{
    uint32_t count;
    uint32_t widx;
    uint32_t num;
    uint32_t sum = 0;
    struct page* page;
    dma_addr_t dma;
    dma_addr_t offs;
    oak_rx_chan_t* rxc = &np->rx_channel[ring];
    int rc = 0;
    uint32_t loop_cnt; /* automatically added for object flow handling */
    int rc_11 = rc; /* start of activity code */
    /* UserCode{64CD2E69-0678-40e4-899F-5B81662988B5}:1IkUl6Yljg */
    num = atomic_read(&rxc->rbr_pend);
    count = rxc->rbr_size - 1;

    /* UserCode{64CD2E69-0678-40e4-899F-5B81662988B5} */

    if (num >= count)
    {
        /* UserCode{0CF925AE-9420-4324-BEC6-99AB7FB76C17}:17zylXIV9A */
        rc = -ENOMEM;
        /* UserCode{0CF925AE-9420-4324-BEC6-99AB7FB76C17} */
    }
    else
    {
        /* UserCode{74ABC6C6-7843-4128-9AAC-2F3E63C15AA6}:tRDHJ89VcS */
        count = (count - num) & ~1;
        widx = rxc->rbr_widx;
        num = 0;

        /* UserCode{74ABC6C6-7843-4128-9AAC-2F3E63C15AA6} */

        /* UserCode{82931B10-BA22-4734-8101-D8341435A414}:uXRrzu9Ncd */
        oakdbg(debug, PKTDATA, "rbr_size=%d rbr_pend=%d refill count=%d widx=%d ridx=%d", rxc->rbr_size, num, count, rxc->rbr_widx, rxc->rbr_ridx);
        /* UserCode{82931B10-BA22-4734-8101-D8341435A414} */

        while ((count > 0) && (rc == 0))
        {
            page = oak_net_alloc_page(np, &dma, DMA_FROM_DEVICE);

            if (page != NULL)
            {
                /* UserCode{87C3B23C-8A97-4440-8ED0-3A862AB11205}:NxsZ75CSjy */
                offs = dma;
                loop_cnt = 0;
                /* UserCode{87C3B23C-8A97-4440-8ED0-3A862AB11205} */

                while ((count > 0) && (loop_cnt < rxc->rbr_bpage))
                {
                    /* UserCode{53F7FEB0-EAEA-4c70-AB4B-D0128212A015}:voPeZU5lwa */
                    oak_rxa_t* rba = &rxc->rba[widx];
                    oak_rxd_t* rbr = &rxc->rbr[widx];

                    rba->page_virt = page;

                    /* UserCode{53F7FEB0-EAEA-4c70-AB4B-D0128212A015} */

                    if (loop_cnt == (rxc->rbr_bpage - 1))
                    {
                        /* UserCode{631BED2F-40D7-47fe-A65D-B5A9FD3B44FE}:4kQ3NDxxVu */
                        rba->page_phys = dma;
                        /* UserCode{631BED2F-40D7-47fe-A65D-B5A9FD3B44FE} */
                    }
                    else
                    {
                        /* UserCode{B6904BE0-54FC-455e-B24B-BE8402DFCC32}:1Fgj1373Nb */
                        rba->page_phys = 0;
                        /* UserCode{B6904BE0-54FC-455e-B24B-BE8402DFCC32} */
                    }
                    /* UserCode{2A333CC3-1108-41c3-9627-213DB4408CC2}:1729UUeRr4 */
                    rba->page_offs = loop_cnt * rxc->rbr_bsize;
                    rbr->buf_ptr_lo = (offs & 0xFFFFFFFF);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
                    rbr->buf_ptr_hi = ((offs >> 32) & 0xFFFFFFFF); /* High 32 bit */
#else
                    rbr->buf_ptr_hi = 0;
#endif

                    /* UserCode{2A333CC3-1108-41c3-9627-213DB4408CC2} */

                    /* UserCode{E45BFF66-D888-4562-BA0E-7A602E02E5BC}:futJvUmgPC */
                    /* move to next write position */
                    widx = NEXT_IDX(widx, rxc->rbr_size);
                    --count;
                    ++num;
                    ++loop_cnt;
                    /* offset to 2nd buffer in page */
                    offs += rxc->rbr_bsize;

                    /* UserCode{E45BFF66-D888-4562-BA0E-7A602E02E5BC} */
                }

                /* UserCode{AE69A799-7245-4f33-A09D-61E7EE91AA5D}:G0MsR5rQMX */
                ++sum;
                ++rxc->stat.rx_alloc_pages;
                /* UserCode{AE69A799-7245-4f33-A09D-61E7EE91AA5D} */
            }
            else
            {
                /* UserCode{48823135-50CF-40a9-9315-444AE1FD657C}:17zylXIV9A */
                rc = -ENOMEM;
                /* UserCode{48823135-50CF-40a9-9315-444AE1FD657C} */

                /* UserCode{4A8F6AE1-132B-4da0-BD6A-A12BE708B164}:16sxc2aYH8 */
                ++rxc->stat.rx_alloc_error;
                /* UserCode{4A8F6AE1-132B-4da0-BD6A-A12BE708B164} */
            }
        }

        /* UserCode{EC588C92-48B8-4836-8152-AF60587AB3B1}:uVT5smx2mC */
        atomic_add(num, &rxc->rbr_pend);
        /* UserCode{EC588C92-48B8-4836-8152-AF60587AB3B1} */

        /* UserCode{FF436EC8-C2B1-46f9-B211-798E6F9304FD}:1X6WHwKtLL */
        oakdbg(debug, PKTDATA, "%d pages allocated, widx=%d/%d, rc=%d", sum, widx, rxc->rbr_widx, rc);
        /* UserCode{FF436EC8-C2B1-46f9-B211-798E6F9304FD} */

        if ((rc == 0) && (num > 0))
        {
            /* UserCode{951C0BAA-10D8-4d68-B7C3-B882E01ACA0C}:4FWMn4S9TN */
            wmb();
            /* UserCode{951C0BAA-10D8-4d68-B7C3-B882E01ACA0C} */

            /* UserCode{98953556-E128-4316-B222-111D06F1257C}:YVp0JXv1eT */
            rxc->rbr_widx = widx;
            /* UserCode{98953556-E128-4316-B222-111D06F1257C} */

            oak_unimac_io_write_32(np, OAK_UNI_RX_RING_CPU_PTR(ring), widx & 0x7ff);
            oak_unimac_io_write_32(np, OAK_UNI_RX_RING_INT_CAUSE(ring), OAK_MBOX_RX_RES_LOW);
        }
        else
        {
        }
    }
    rc_11 = rc;
    return rc_11;
}

/* Name      : open
 * Returns   : int
 * Parameters:  struct net_device * net_dev
 *  */
int oak_net_open(struct net_device* net_dev)
{
    oak_t* np = netdev_priv(net_dev);
    int err = -ENODEV;
    int rc;
    uint16_t qnum; /* automatically added for object flow handling */
    int err_24; /* start of activity code */
    /* UserCode{F5434684-76BF-4b80-ADFB-413BDEED6AB4}:SdRCZGJsEF */
    rc = try_module_get(THIS_MODULE);
    /* UserCode{F5434684-76BF-4b80-ADFB-413BDEED6AB4} */

    if (rc != 0)
    {
        err = oak_unimac_reset(np);

        if (err == 0 && np->level == 40)
        {
            err = oak_unimac_alloc_channels(np, rxs, txs, chan, rto);

            if (err == 0)
            {
                /* UserCode{CD8A67C4-75DD-4270-AE1A-31A859FA62EF}:1B1SDKHcBX */
                np->level = 41;
                /* UserCode{CD8A67C4-75DD-4270-AE1A-31A859FA62EF} */

                err = oak_irq_request_ivec(np);

                if (err == 0)
                {
                    /* UserCode{4BCBBB8C-2AC3-43df-98FC-B64DC4C09348}:Y3nXwOVMWZ */
                    np->level = 42;
                    /* UserCode{4BCBBB8C-2AC3-43df-98FC-B64DC4C09348} */

                    err = oak_irq_enable_groups(np);

                    if (err == 0)
                    {
                        /* UserCode{54DB09B7-8953-4562-98D2-3153B2070758}:16AhYFjHCo */
                        np->level = 43;
                        /* UserCode{54DB09B7-8953-4562-98D2-3153B2070758} */

                        oak_net_esu_ena_mrvl_hdr(np);
                        err = oak_net_esu_set_mtu(np->netdev, np->netdev->mtu);

                        if (err == 0)
                        {
                            err = oak_net_start_all(np);
                        }
                        else
                        {
                        }

                        if (err == 0)
                        {
                            /* UserCode{76E43C82-39B0-4ca9-8FAD-EEE5728B5F64}:cjJiWasMoL */
                            np->level = 44;
                            /* UserCode{76E43C82-39B0-4ca9-8FAD-EEE5728B5F64} */

                            oak_net_esu_ena_speed(port_speed, np);
                            /* UserCode{99152F65-5E5F-49e6-A67E-206215DE0386}:2FZKJpC8Q3 */
                            netif_carrier_on(net_dev);
                            for (qnum = 0; qnum < np->num_tx_chan; qnum++)
                            {
                                netif_start_subqueue(np->netdev, qnum);
                            }

                            /* UserCode{99152F65-5E5F-49e6-A67E-206215DE0386} */
                        }
                        else
                        {
                        }
                    }
                    else
                    {
                    }
                }
                else
                {
                }
            }
            else
            {
            }
        }
        else
        {
        }
    }
    else
    {
    }
    err_24 = err;

    if (err == 0)
    {
    }
    else
    {
        /* UserCode{037A4C02-712C-4fc0-BA21-4CE066B99239}:xMYQ6BFq36 */
        oak_net_close(net_dev);
        /* UserCode{037A4C02-712C-4fc0-BA21-4CE066B99239} */
    }
    /* UserCode{51B24D49-C399-428f-A7A0-13B954979D3D}:dLxLCNxvk1 */
    oakdbg(debug, PROBE, "ndev=%p err=%d", net_dev, err);
    /* UserCode{51B24D49-C399-428f-A7A0-13B954979D3D} */

    return err_24;
}

/* Name      : close
 * Returns   : int
 * Parameters:  struct net_device * net_dev
 *  */
int oak_net_close(struct net_device* net_dev)
{
    oak_t* np = netdev_priv(net_dev); /* automatically added for object flow handling */
    int err_4; /* start of activity code */
    /* UserCode{2BD19806-3251-43e3-B578-9CCF2EFAAE46}:10NCOLarBG */
    netif_carrier_off(net_dev);

    /* UserCode{2BD19806-3251-43e3-B578-9CCF2EFAAE46} */

    if (np->level >= 44)
    {
        oak_net_stop_all(np);
    }
    else
    {
    }

    if (np->level >= 43)
    {
        oak_irq_disable_groups(np);
    }
    else
    {
    }

    if (np->level >= 42)
    {
        oak_irq_release_ivec(np);
    }
    else
    {
    }

    if (np->level >= 41)
    {
        oak_unimac_free_channels(np);
        /* UserCode{B50FBDDC-4805-41c8-AAB2-4A6FA09E990F}:1Ug3cpZD5a */
        np->level = 40;
        /* UserCode{B50FBDDC-4805-41c8-AAB2-4A6FA09E990F} */

        /* UserCode{2CB5C7DC-5A08-4ee8-A0C6-D0A9BA4B8DD8}:O2qIuvLm96 */
        module_put(THIS_MODULE);
        /* UserCode{2CB5C7DC-5A08-4ee8-A0C6-D0A9BA4B8DD8} */
    }
    else
    {
    }
    err_4 = 0;
    /* UserCode{04285A4D-5221-4e0a-BBCC-085C0F1052DE}:Sin6g37H0t */
    oakdbg(debug, PROBE, "ndev=%p", net_dev);
    /* UserCode{04285A4D-5221-4e0a-BBCC-085C0F1052DE} */

    return err_4;
}

/* Name      : ioctl
 * Returns   : int
 * Parameters:  struct net_device * net_dev,  struct ifreq * ifr,  int cmd
 *  */
int oak_net_ioctl(struct net_device* net_dev, struct ifreq* ifr, int cmd)
{
    oak_t* np = netdev_priv(net_dev); /* automatically added for object flow handling */
    int rc_16 = -EOPNOTSUPP; /* start of activity code */

    if (cmd == OAK_IOCTL_REG_MAC_REQ || cmd == OAK_IOCTL_REG_ESU_REQ)
    {
        rc_16 = oak_ctl_direct_register_access(np, ifr, cmd);
    }
    else
    {
    }

    if (cmd == OAK_IOCTL_STAT)
    {
        rc_16 = oak_ctl_channel_status_access(np, ifr, cmd);
    }
    else
    {
    }

    if (cmd == OAK_IOCTL_SET_MAC_RATE_A)
    {
        rc_16 = oak_ctl_set_mac_rate(np, ifr, cmd);
    }
    else
    {
    }

    if (cmd == OAK_IOCTL_SET_MAC_RATE_B)
    {
        rc_16 = oak_ctl_set_mac_rate(np, ifr, cmd);
    }
    else
    {
    }

    if (cmd == OAK_IOCTL_RXFLOW)
    {
        rc_16 = oak_ctl_set_rx_flow(np, ifr, cmd);
    }
    else
    {
    }
    /* UserCode{B94E8AC7-423C-4319-97DA-2857A888AED9}:SmZE92F29v */
    oakdbg(debug, DRV, "np=%p cmd=0x%x", np, cmd);
    /* UserCode{B94E8AC7-423C-4319-97DA-2857A888AED9} */

    return rc_16;
}

/* Name      : add_napi
 * Returns   : void
 * Parameters:  struct net_device * netdev
 *  */
void oak_net_add_napi(struct net_device* netdev)
{
    oak_t* np = netdev_priv(netdev);
    ldg_t* ldg = np->gicu.ldg;
    uint32_t num_ldg = np->gicu.num_ldg; /* start of activity code */
    while (num_ldg > 0)
    {
        /* UserCode{E8FBEFF6-4B95-4c9c-AC0D-EAFD759A6CF6}:11VKombJis */
        netif_napi_add(netdev, &ldg->napi, oak_net_poll, 64);
        napi_enable(&ldg->napi);
        ++ldg;
        --num_ldg;
        /* UserCode{E8FBEFF6-4B95-4c9c-AC0D-EAFD759A6CF6} */
    }

    /* UserCode{539E6666-CB14-4d44-9355-FC333E0292F9}:hHg36ol1mn */
    oakdbg(debug, PROBE, "%d napi IF added", np->gicu.num_ldg);
    /* UserCode{539E6666-CB14-4d44-9355-FC333E0292F9} */

    return;
}

/* Name      : del_napi
 * Returns   : void
 * Parameters:  struct net_device * netdev
 *  */
void oak_net_del_napi(struct net_device* netdev)
{
    oak_t* np = netdev_priv(netdev);
    ldg_t* ldg = np->gicu.ldg;
    uint32_t num_ldg = np->gicu.num_ldg; /* start of activity code */
    while (num_ldg > 0)
    {
        /* UserCode{36074E5D-28D7-479f-B2E4-B5F29A80AE0A}:1NNLcNvLOT */
        napi_disable(&ldg->napi);
        netif_napi_del(&ldg->napi);
        ++ldg;
        --num_ldg;
        /* UserCode{36074E5D-28D7-479f-B2E4-B5F29A80AE0A} */
    }

    /* UserCode{F7FC3970-2C5D-4ae3-8674-99FC8CDF6123}:fn2QGNToDH */
    oakdbg(debug, PROBE, "%d napi IF deleted", np->gicu.num_ldg);
    /* UserCode{F7FC3970-2C5D-4ae3-8674-99FC8CDF6123} */

    return;
}

/* Name      : set_mac_addr
 * Returns   : int
 * Parameters:  struct net_device * dev = dev,  void * p_addr = addr
 *  */
int oak_net_set_mac_addr(struct net_device* dev, void* p_addr)
{
    struct sockaddr* addr = p_addr;
    int rc; /* automatically added for object flow handling */
    int rc_5 = 0; /* start of activity code */
    /* UserCode{F1BF70DD-8AC8-414d-80D2-48036958B5CB}:1Tac9pwSTt */
    rc = is_valid_ether_addr(addr->sa_data);
    /* UserCode{F1BF70DD-8AC8-414d-80D2-48036958B5CB} */

    if (rc == 0)
    {
        /* UserCode{079109E2-2C39-4874-BEF6-A7281506E0CB}:FubMmJTDKN */
        rc = -EINVAL;
        /* UserCode{079109E2-2C39-4874-BEF6-A7281506E0CB} */
    }
    else
    {
        /* UserCode{DAA1BDA8-0040-4cad-AE48-E1C23FD97194}:168DpFlyID */
        memcpy(dev->dev_addr, addr->sa_data, ETH_ALEN);
        /* UserCode{DAA1BDA8-0040-4cad-AE48-E1C23FD97194} */

        /* UserCode{AAE718E4-036C-42d7-80F8-C10F2F6FC3A8}:1vlGps3D54 */
        rc = netif_running(dev);
        /* UserCode{AAE718E4-036C-42d7-80F8-C10F2F6FC3A8} */

        /* UserCode{65B334FB-7563-408c-9714-8200141DC3C3}:r8srRGlzXT */
        rc = 0;
        /* UserCode{65B334FB-7563-408c-9714-8200141DC3C3} */
    }
    rc_5 = rc;
    /* UserCode{AB2EB8C5-351C-4c07-8774-80E6D2C1BA89}:7hXZ86VX4h */
    oakdbg(debug, DRV, "addr=0x%02x%02x%02x%02x%02x%02x rc=%d",
           dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2],
           dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5], rc);
    /* UserCode{AB2EB8C5-351C-4c07-8774-80E6D2C1BA89} */

    return rc_5;
}

/* Name      : alloc_page
 * Returns   : struct page *
 * Parameters:  oak_t * np = np,  dma_addr_t * dma = dma,  int direction = direction
 *  */
struct page* oak_net_alloc_page(oak_t* np, dma_addr_t* dma, int direction)
{
    /* automatically added for object flow handling */
    struct page* page_2; /* start of activity code */
    struct page* page = NULL;
    /* UserCode{E1E2D5C4-F2A1-4fdb-B927-E1BF52A80902}:BD5Ns24foD */
    np->page_order = 0; /* 0: 4K */
    np->page_size = (PAGE_SIZE << np->page_order);

    /* UserCode{E1E2D5C4-F2A1-4fdb-B927-E1BF52A80902} */

    /* UserCode{750E4269-1EE4-4828-8C11-7A21A70FA943}:oWLN25unPo */
    page = alloc_page(GFP_ATOMIC /* | __GFP_COLD */ | __GFP_COMP);
    /* UserCode{750E4269-1EE4-4828-8C11-7A21A70FA943} */

    if (page == NULL)
    {
        /* UserCode{874857C7-65DC-4990-95F5-CE63E83FDF48}:ElbKo8KFDR */
        *dma = 0;
        /* UserCode{874857C7-65DC-4990-95F5-CE63E83FDF48} */
    }
    else
    {
        /* UserCode{E5F8F57C-C7DB-4972-A6AA-1BBBBFA9FAD8}:6Ttk2LRFIA */
        *dma = dma_map_page(np->device, page, 0, np->page_size, direction);
        /* UserCode{E5F8F57C-C7DB-4972-A6AA-1BBBBFA9FAD8} */

        if (dma_mapping_error(np->device, *dma) != 0)
        {
            /* UserCode{A31BED45-973F-4edc-8720-B9C200D37BF2}:7EjViBJqCL */
            __free_page(page);
            /* UserCode{A31BED45-973F-4edc-8720-B9C200D37BF2} */

            /* UserCode{980016B7-E1DE-4519-924A-BE7F7AD2FA26}:1KnxJriMVc */
            *dma = 0;
            page = NULL;

            /* UserCode{980016B7-E1DE-4519-924A-BE7F7AD2FA26} */
        }
        else
        {
        }
    }
    page_2 = page;
    return page_2;
}

/* Name      : select_queue
 * Returns   : uint16_t
 * Parameters:  struct net_device * dev = dev,  struct sk_buff * skb = skb,  struct net_device * sb_dev = sb_dev
 *  */
uint16_t oak_net_select_queue(struct net_device* dev, struct sk_buff* skb, struct net_device* sb_dev)
{
    oak_t* np = netdev_priv(dev);
    uint32_t txq = 0;
    bool rec; /* automatically added for object flow handling */
    uint16_t rc_1 = 0; /* start of activity code */
    /* UserCode{2C789BB6-8130-4b2f-8096-0795F5B08BD4}:1Vcdwi2tgS */
    rec = skb_rx_queue_recorded(skb);
    /* UserCode{2C789BB6-8130-4b2f-8096-0795F5B08BD4} */

    if (rec == false)
    {
        /* UserCode{4EDA2AEC-9FA1-4cc2-9926-D315E27ED512}:1YbVmPlUYl */
        txq = smp_processor_id();
        /* UserCode{4EDA2AEC-9FA1-4cc2-9926-D315E27ED512} */
    }
    else
    {
        /* UserCode{20613BE9-50A8-4c73-8F5A-B5FF6280101E}:qRJFH8txRB */
        txq = skb_get_rx_queue(skb);
        /* UserCode{20613BE9-50A8-4c73-8F5A-B5FF6280101E} */
    }

    if (txq >= np->num_tx_chan)
    {
        /* UserCode{342C2B25-397B-4bae-A8FE-B76EA7826173}:xeuANJpY9w */
        txq %= np->num_tx_chan;
        /* UserCode{342C2B25-397B-4bae-A8FE-B76EA7826173} */
    }
    else
    {
    }
    rc_1 = (uint16_t)txq;
    /* UserCode{FF639960-35C6-4c3f-887F-4612C9D1359D}:14e0ZNhVRP */
    oakdbg(debug, DRV, "queue=%d of %d", txq, dev->real_num_tx_queues);
    /* UserCode{FF639960-35C6-4c3f-887F-4612C9D1359D} */

    return rc_1;
}

/* Name      : xmit_frame
 * Returns   : int
 * Parameters:  struct sk_buff * skb,  struct net_device * net_dev
 *  */
int oak_net_xmit_frame(struct sk_buff* skb, struct net_device* net_dev)
{
    oak_t* np = netdev_priv(net_dev);
    uint16_t txq;
    int rc;
    uint16_t nfrags; /* automatically added for object flow handling */
    int err_5; /* start of activity code */
    /* UserCode{296A772A-F7AD-4a85-BAC7-AB00A5906DB4}:PKEfscKlLX */
    txq = skb->queue_mapping;
    /* UserCode{296A772A-F7AD-4a85-BAC7-AB00A5906DB4} */

    /* UserCode{E2FB0A2B-4BF6-416c-8E1F-992F6CF0A167}:6IUBxWM0ee */
    nfrags = skb_shinfo(skb)->nr_frags + 1;
    /* UserCode{E2FB0A2B-4BF6-416c-8E1F-992F6CF0A167} */

    /* UserCode{805F12D4-04F6-4154-A4B6-532C953DA08A}:1UB0OTfDts */
    rc = oak_net_stop_tx_queue(np, nfrags, txq);
    /* UserCode{805F12D4-04F6-4154-A4B6-532C953DA08A} */

    if (rc == 0)
    {
        rc = oak_net_tx_packet(np, skb, txq);
    }
    else
    {
    }
    err_5 = rc;
    /* UserCode{ABF3CA07-86DA-4796-BDC7-F520E20C622B}:TqhuHACkUy */
    oakdbg(debug, TX_DONE, "nfrags=%d txq=%d rc=%d", nfrags, txq, rc);

    /* UserCode{ABF3CA07-86DA-4796-BDC7-F520E20C622B} */

    return err_5;
}

/* Name      : process_tx_pkt
 * Returns   : int
 * Parameters:  oak_tx_chan_t * txc = txc,  int desc_num = desc_num
 *  */
int oak_net_process_tx_pkt(oak_tx_chan_t* txc, int desc_num)
{
    oak_t* np = txc->oak;
    int work_done = 0;
    oak_txi_t* tbi; /* automatically added for object flow handling */
    int rc_1; /* start of activity code */
    while (desc_num > 0)
    {
        /* UserCode{836AE84D-47A0-4e9a-892E-6CD3A410CADA}:NNyqdUkYCx */
        tbi = &txc->tbi[txc->tbr_ridx];
        txc->tbr_ridx = NEXT_IDX(txc->tbr_ridx, txc->tbr_size);
        /* UserCode{836AE84D-47A0-4e9a-892E-6CD3A410CADA} */

        if (tbi->mapping != 0)
        {

            if ((tbi->flags & TX_BUFF_INFO_ADR_MAPS) == TX_BUFF_INFO_ADR_MAPS)
            {
                /* UserCode{98016E3A-34C4-42d7-8AE6-F4F4D2FD15F1}:tzYMx8Mwkl */
                dma_unmap_single(np->device, tbi->mapping, tbi->mapsize, DMA_TO_DEVICE);
                /* UserCode{98016E3A-34C4-42d7-8AE6-F4F4D2FD15F1} */
            }
            else
            {

                if ((tbi->flags & TX_BUFF_INFO_ADR_MAPP) == TX_BUFF_INFO_ADR_MAPP)
                {
                    /* UserCode{B5EDD001-DE6A-4819-8B53-0B7895ED9823}:rGSps83TOM */
                    dma_unmap_page(np->device, tbi->mapping, tbi->mapsize, DMA_TO_DEVICE);
                    /* UserCode{B5EDD001-DE6A-4819-8B53-0B7895ED9823} */
                }
                else
                {
                }
            }
            /* UserCode{84298E5D-7B33-49bc-823F-1510C7040CA6}:uoPgT2i62K */
            tbi->mapping = 0;
            tbi->mapsize = 0;

            /* UserCode{84298E5D-7B33-49bc-823F-1510C7040CA6} */
        }
        else
        {
        }

        if ((tbi->flags & TX_BUFF_INFO_EOP) == TX_BUFF_INFO_EOP)
        {

            if (tbi->skb != NULL)
            {
                /* UserCode{F32A9B7A-A91B-4b7f-AAA7-EF14FFD6CBD5}:9uQPYLVSiN */
                dev_kfree_skb(tbi->skb);
                /* UserCode{F32A9B7A-A91B-4b7f-AAA7-EF14FFD6CBD5} */
            }
            else
            {
            }

            if (tbi->page != NULL)
            {
                /* UserCode{DB8099F9-13C6-45c2-A2F5-3E04878D8F84}:1KgcoTrWvJ */
                __free_page(tbi->page);
                /* UserCode{DB8099F9-13C6-45c2-A2F5-3E04878D8F84} */
            }
            else
            {
            }
            /* UserCode{DF6D2244-ED2C-40c7-8EA1-AFC098A1A36A}:1PRIbjotqh */
            ++txc->stat.tx_frame_compl;
            /* UserCode{DF6D2244-ED2C-40c7-8EA1-AFC098A1A36A} */
        }
        else
        {
        }
        /* UserCode{1BDAC176-A765-4ce3-B9F6-B86A3BF15D7E}:PEuqJRVWO5 */
        tbi->flags = 0;
        tbi->skb = NULL;
        tbi->page = NULL;
        --desc_num;
        atomic_dec(&txc->tbr_pend);
        ++work_done;

        /* UserCode{1BDAC176-A765-4ce3-B9F6-B86A3BF15D7E} */
    }

    /* UserCode{F7BC6C6C-010B-4ce9-AD63-21958386338A}:ZgH52OzWtd */
    oakdbg(debug, TX_DONE, "work done=%d", work_done);
    /* UserCode{F7BC6C6C-010B-4ce9-AD63-21958386338A} */

    rc_1 = work_done;
    return rc_1;
}

/* Name      : start_all
 * Returns   : int
 * Parameters:  oak_t * np = np
 *  */
int oak_net_start_all(oak_t* np)
{
    uint32_t i = 0;
    int rc = 0; /* automatically added for object flow handling */
    int rc_13 = 0; /* start of activity code */
    while (i < np->num_rx_chan)
    {
        oak_net_rbr_refill(np, i);
        /* UserCode{E2F21CF3-82DB-4453-A5E7-90D138688E9C}:pqeK47KwaC */
        ++i;
        /* UserCode{E2F21CF3-82DB-4453-A5E7-90D138688E9C} */
    }

    rc = oak_unimac_start_all_txq(np, 1);

    if (rc == 0)
    {
        rc = oak_unimac_start_all_rxq(np, 1);
    }
    else
    {
    }

    if (rc == 0)
    {
        oak_irq_ena_general(np, 1);
    }
    else
    {
    }
    rc_13 = rc;
    /* UserCode{C25724F3-BF70-41cb-A49C-BE27F535064D}:qAvnC5adyf */
    oakdbg(debug, IFDOWN, " ok");
    /* UserCode{C25724F3-BF70-41cb-A49C-BE27F535064D} */

    return rc_13;
}

/* Name      : stop_all
 * Returns   : void
 * Parameters:  oak_t * np = np
 *  */
void oak_net_stop_all(oak_t* np)
{
    int i = 0; /* start of activity code */
    oak_unimac_start_all_rxq(np, 0);
    oak_unimac_start_all_txq(np, 0);
    while (i < np->num_rx_chan)
    {
        oak_net_rbr_free(&np->rx_channel[i]);
        /* UserCode{772B4327-E86A-47fb-8B23-25F5378EE7AB}:pqeK47KwaC */
        ++i;
        /* UserCode{772B4327-E86A-47fb-8B23-25F5378EE7AB} */
    }

    /* UserCode{819F6408-27A0-42ac-981A-7A779E076B26}:7233eLMZuK */
    i = 0;
    /* UserCode{819F6408-27A0-42ac-981A-7A779E076B26} */

    while (i < np->num_tx_chan)
    {
        oak_net_tbr_free(&np->tx_channel[i]);
        /* UserCode{70329741-8B58-4655-9830-A87A93FADB98}:pqeK47KwaC */
        ++i;
        /* UserCode{70329741-8B58-4655-9830-A87A93FADB98} */
    }

    oak_irq_ena_general(np, 0);
    /* UserCode{1BA4AC6B-239F-4234-A999-357D161C572E}:qAvnC5adyf */
    oakdbg(debug, IFDOWN, " ok");
    /* UserCode{1BA4AC6B-239F-4234-A999-357D161C572E} */

    return;
}

/* Name      : tx_stats
 * Returns   : void
 * Parameters:  oak_tx_chan_t * txc = txc,  int len = len
 *  */
void oak_net_tx_stats(oak_tx_chan_t* txc, int len)
{
    if (len <= 64)
        ++txc->stat.tx_64;
    else
        if (len <= 128)
            ++txc->stat.tx_128;
        else
            if (len <= 256)
                ++txc->stat.tx_256;
            else
                if (len <= 512)
                    ++txc->stat.tx_512;
                else
                    if (len <= 1024)
                        ++txc->stat.tx_1024;
                    else
                        ++txc->stat.tx_2048;
}

/* Name      : rx_stats
 * Returns   : void
 * Parameters:  oak_rx_chan_t * rxc = rxc,  int len = len
 *  */
void oak_net_rx_stats(oak_rx_chan_t* rxc, int len)
{
    if (len <= 64)
        ++rxc->stat.rx_64;
    else
        if (len <= 128)
            ++rxc->stat.rx_128;
        else
            if (len <= 256)
                ++rxc->stat.rx_256;
            else
                if (len <= 512)
                    ++rxc->stat.rx_512;
                else
                    if (len <= 1024)
                        ++rxc->stat.rx_1024;
                    else
                        ++rxc->stat.rx_2048;
}

/* Name      : tbr_free
 * Returns   : void
 * Parameters:  oak_tx_chan_t * txp = txp
 *  */
void oak_net_tbr_free(oak_tx_chan_t* txp)
{
    uint32_t cnt; /* start of activity code */
    /* UserCode{3B8863F7-210B-4505-A4C7-32D93062BA41}:p8PLz3nV8O */
    cnt = atomic_read(&txp->tbr_pend);
    /* UserCode{3B8863F7-210B-4505-A4C7-32D93062BA41} */

    oak_net_process_tx_pkt(txp, cnt);
    /* UserCode{BAE5E4B3-C224-4bb0-B710-F7EDF7E1C32D}:1JanEFNDrO */
    atomic_set(&txp->tbr_pend, 0);
    txp->tbr_widx = 0; /* write buffer index */
    txp->tbr_ridx = 0;
    /* UserCode{BAE5E4B3-C224-4bb0-B710-F7EDF7E1C32D} */

    return;
}

/* Name      : rbr_free
 * Returns   : void
 * Parameters:  oak_rx_chan_t * rxp = rxp
 *  */
void oak_net_rbr_free(oak_rx_chan_t* rxp)
{
    oak_t* np = rxp->oak;
    uint32_t sum = 0;
    struct page* page;
    dma_addr_t dma; /* start of activity code */
    while (rxp->rbr_ridx != rxp->rbr_widx)
    {
        /* UserCode{E13A2124-0026-4818-9172-4F18DB971F52}:1MLrb9OPdC */
        page = rxp->rba[rxp->rbr_ridx].page_virt;
        /* UserCode{E13A2124-0026-4818-9172-4F18DB971F52} */

        if (page != NULL)
        {
            /* UserCode{FD3EFB10-A787-4500-99FE-9A21EBCDCAD9}:7LR11KzLEd */
            dma = rxp->rba[rxp->rbr_ridx].page_phys;
            ++sum;

            /* UserCode{FD3EFB10-A787-4500-99FE-9A21EBCDCAD9} */

            if (dma != 0)
            {
                /* UserCode{314182EA-2D24-4373-AFBC-0944E2A2026A}:1EIoW34f7w */
                dma_unmap_page(np->device, dma, np->page_size, DMA_FROM_DEVICE);
                ++rxp->stat.rx_unmap_pages;
                rxp->rba[rxp->rbr_ridx].page_phys = 0;
                page->index = 0;
                page->mapping = NULL;
                __free_page(page);

                /* UserCode{314182EA-2D24-4373-AFBC-0944E2A2026A} */
            }
            else
            {
            }
        }
        else
        {
        }
        /* UserCode{284D2E57-1DE4-44b5-ACAB-58780BD0F12F}:JNW8KiiyaF */
        rxp->rba[rxp->rbr_ridx].page_virt = NULL;
        rxp->rbr[rxp->rbr_ridx].buf_ptr_hi = 0;
        rxp->rbr[rxp->rbr_ridx].buf_ptr_lo = 0;
        /* UserCode{284D2E57-1DE4-44b5-ACAB-58780BD0F12F} */

        /* UserCode{F8EC2B66-B02B-4896-B501-026F0BB847BF}:1IBsYR2Ips */
        rxp->rbr_ridx = NEXT_IDX(rxp->rbr_ridx, rxp->rbr_size);
        /* UserCode{F8EC2B66-B02B-4896-B501-026F0BB847BF} */
    }

    /* UserCode{1282CF7B-D2F5-48b9-B7FA-1EA454F0FFBE}:W7jM12fo78 */
    oakdbg(debug, IFDOWN, "totally freed ring buffer size %d kByte (ring entries: %d)", sum, rxp->rbr_size);
/* UserCode{1282CF7B-D2F5-48b9-B7FA-1EA454F0FFBE} */

/* UserCode{9B4C5CA8-21E4-4fbc-91E1-2EDFFD2C0D50}:10Co0H6o9l */
#if 1
    atomic_set(&rxp->rbr_pend, 0);
#else
    rxp->rbr_pend = 0; /* number of pending ring buffers */
#endif
    rxp->rbr_widx = 0; /* write buffer index */
    rxp->rbr_ridx = 0; /* read buffer index */
    rxp->rbr_len = 0;

    /* UserCode{9B4C5CA8-21E4-4fbc-91E1-2EDFFD2C0D50} */

    return;
}

/* Name      : tx_packet
 * Returns   : int
 * Parameters:  oak_t * np = np,  struct sk_buff * skb = skb,  uint16_t txq = txq
 *  */
static int oak_net_tx_packet(oak_t* np, struct sk_buff* skb, uint16_t txq)
{
    oak_tx_chan_t* txc = &np->tx_channel[txq];
    uint32_t num = 0;
    uint32_t frag_idx = 0;
    int flags = 0;
    uint16_t len = 0;
    uint32_t cs_g3 = 0;
    uint32_t cs_g4 = 0;
    skb_frag_t* frag;
    uint32_t nfrags;
    dma_addr_t mapping;
    int rc = 0; /* start of activity code */

    if (mhdr != 0)
    {
        skb = oak_net_tx_prepend_mrvl_hdr(skb);
    }
    else
    {
    }

    if (skb == NULL)
    {
    }
    else
    {
        /* OAK HW does not need padding in the data, */
        /* only limitation is zero length packet.    */
        /* So zero byte transfer should not be programmed */
        /* in the descriptor. */
        if (skb->len < OAK_ONEBYTE)
        {

            if (skb_padto(skb, OAK_ONEBYTE) == 0)
            {
                /* UserCode{DDD9258D-55D2-4101-AF1D-E41B482FC7B3}:XgB9vMEEDs */
                len = OAK_ONEBYTE;
                /* UserCode{DDD9258D-55D2-4101-AF1D-E41B482FC7B3} */
            }
        }
        else
        {
            /* UserCode{06D8BC61-3695-4058-8E70-ADF712EDB505}:16AoGfL1CY */
            skb_orphan(skb);
            len = skb_headlen(skb);
            /* UserCode{06D8BC61-3695-4058-8E70-ADF712EDB505} */
        }
        /* UserCode{56A3A634-5921-410c-B4ED-9B713FFD4F85}:1Zjk2qPQ5A */
        nfrags = skb_shinfo(skb)->nr_frags;
        /* UserCode{56A3A634-5921-410c-B4ED-9B713FFD4F85} */

        if (len > 0)
        {
            /* UserCode{976AE98B-C833-4549-973A-729D82E2FA3D}:IN9h5n21GL */
            mapping = dma_map_single(np->device, skb->data, len, DMA_TO_DEVICE);
            flags = TX_BUFF_INFO_ADR_MAPS;
            ++num;

            /* UserCode{976AE98B-C833-4549-973A-729D82E2FA3D} */
        }
        else
        {

            if (nfrags > 0)
            {
                /* UserCode{43DFC20D-D464-4d5d-90B9-19BCA9C989E7}:1aNJhkrdJp */
                frag = &skb_shinfo(skb)->frags[frag_idx];
                len = skb_frag_size(frag);
                mapping = dma_map_page(np->device, skb_frag_page(frag), skb_frag_off(frag), len, DMA_TO_DEVICE);
                flags = TX_BUFF_INFO_ADR_MAPP;
                ++num;
                ++frag_idx;

                /* UserCode{43DFC20D-D464-4d5d-90B9-19BCA9C989E7} */
            }
        }

        if (num > 0)
        {
            rc = oak_chksum_get_tx_config(skb, &cs_g3, &cs_g4);
            /* For error case set checksum to zero */
            if (rc)
            {
                cs_g3 = 0;
                cs_g4 = 0;
            }

            oak_net_set_txd_first(txc, len, cs_g3, cs_g4, mapping, len, flags);
            while (frag_idx < nfrags)
            {
                /* UserCode{183B8E19-132A-4254-BF8D-E499A9F7306D}:EELkNsgATV */
                txc->tbr_widx = NEXT_IDX(txc->tbr_widx, txc->tbr_size);
                frag = &skb_shinfo(skb)->frags[frag_idx];
                len = skb_frag_size(frag);

                /* UserCode{183B8E19-132A-4254-BF8D-E499A9F7306D} */

                /* UserCode{25537965-6577-4f39-A397-4A8A32EE61E7}:9X918578yq */
                mapping = dma_map_page(np->device, skb_frag_page(frag), skb_frag_off(frag), len, DMA_TO_DEVICE);
                /* UserCode{25537965-6577-4f39-A397-4A8A32EE61E7} */

                oak_net_set_txd_page(txc, len, mapping, len, TX_BUFF_INFO_ADR_MAPP);
                /* UserCode{EA930BDE-4693-469e-AE61-38F3FFDA92D3}:bKcwul097s */
                ++num;
                ++frag_idx;

                /* UserCode{EA930BDE-4693-469e-AE61-38F3FFDA92D3} */
            }

            oak_net_set_txd_last(txc, skb, NULL);
            /* UserCode{68071F66-3C2F-4e2f-803E-55B6B487ED12}:Ug5CsRaySF */
            txc->tbr_widx = NEXT_IDX(txc->tbr_widx, txc->tbr_size);
            atomic_add(num, &txc->tbr_pend);
            /* UserCode{68071F66-3C2F-4e2f-803E-55B6B487ED12} */

            /* UserCode{831197C1-78EF-4659-BFAF-05317D0EFF1C}:4FWMn4S9TN */
            wmb();
            /* UserCode{831197C1-78EF-4659-BFAF-05317D0EFF1C} */

            /* UserCode{E113C6AA-C328-41d2-8AC3-782199731797}:wFHWC4oS0Z */
            ++txc->stat.tx_frame_count;
            txc->stat.tx_byte_count += skb->len;
            /* UserCode{E113C6AA-C328-41d2-8AC3-782199731797} */

	    /* Static Counter: Increment tx stats counter and bytes for ifconfig */
	    np->netdev->stats.tx_packets++;
	    np->netdev->stats.tx_bytes += skb->len;

            oak_net_tx_stats(txc, skb->len);
            oak_unimac_io_write_32(np, OAK_UNI_TX_RING_CPU_PTR(txq), txc->tbr_widx & 0x7ff);
        }
        else
        {
            /* UserCode{ADD85A20-BD63-4e7e-BD6C-878BAA0883AA}:1RETKCQPjB */
            ++txc->stat.tx_drop;
            /* UserCode{ADD85A20-BD63-4e7e-BD6C-878BAA0883AA} */
        }
    }
    rc = 0;
    return rc;
}

/* Name      : skb_tx_protocol_type
 * Returns   : int
 * Parameters:  struct sk_buff *skb
 * Description: This function returns the transmit frames protocol type 
 *              for deciding the checksum offload configuration.
 *  */
int oak_net_skb_tx_protocol_type(struct sk_buff* skb)
{
    uint8_t ip_prot = 0;
    int rc = NO_CHKSUM;
    __be16 prot = skb->protocol;

    if (prot == htons(ETH_P_8021Q))
        prot = vlan_eth_hdr(skb)->h_vlan_encapsulated_proto;

    if (prot == htons(ETH_P_IP))
    {
        ip_prot = ip_hdr(skb)->protocol;
        rc = L3_CHKSUM;
    }
    else if (prot == htons(ETH_P_IPV6))
    {
        ip_prot = ipv6_hdr(skb)->nexthdr;
        rc = L3_CHKSUM;
    }

    if (ip_prot == IPPROTO_TCP || ip_prot == IPPROTO_UDP)
        rc = L3_L4_CHKSUM;

    return rc;
}

/* Name      : tx_work
 * Returns   : int
 * Parameters:  ldg_t * ldg = ldg,  uint32_t ring = ring,  int budget = budget
 *  */
static int oak_net_tx_work(ldg_t* ldg, uint32_t ring, int budget)
{
    oak_tx_chan_t* txc;
    oak_t* np = ldg->device;
    uint32_t reason = 0;
    uint32_t tidx;
    int todo;
    int work_done = 0; /* automatically added for object flow handling */
    int rc_10; /* start of activity code */
    /* UserCode{C23A40D7-CC9F-4ce5-BF09-8C1330147817}:wZNMr276Oc */
    txc = &np->tx_channel[ring];
    /* UserCode{C23A40D7-CC9F-4ce5-BF09-8C1330147817} */

    /* UserCode{44972A34-0288-4f99-872A-A8A550D15AFC}:MTKjGC49AZ */
    smp_mb();
    /* UserCode{44972A34-0288-4f99-872A-A8A550D15AFC} */

    if (txc->tbr_len == 0)
    {
        /* UserCode{1EE9681E-EEF4-4962-88E4-CA6B885ACFDE}:11Y10Aq94b */
        ++txc->stat.tx_interrupts;
        /* UserCode{1EE9681E-EEF4-4962-88E4-CA6B885ACFDE} */

        reason = oak_unimac_disable_and_get_tx_irq_reason(np, ring, &tidx);
/* UserCode{0DC6EF08-D642-4571-902D-D7DDE07E74C5}:cR92LBeUkf */
#if 1
        oakdbg(debug, TX_DONE, "MB ring=%d reason=0x%x tidx=%d", ring, reason, tidx);
#else
        oakdbg(debug, TX_DONE, "IO ring=%d reason=0x%x tidx=%d", ring, reason, tidx);
#endif
        /* UserCode{0DC6EF08-D642-4571-902D-D7DDE07E74C5} */

        if ((reason & OAK_MBOX_TX_COMP) != 0)
        {

            if (tidx < txc->tbr_ridx)
            {
                /* UserCode{9243E911-F157-4346-84A0-C5DEC8968AA9}:vtVyaj1jIq */
                txc->tbr_len = txc->tbr_size - txc->tbr_ridx + tidx;
                /* UserCode{9243E911-F157-4346-84A0-C5DEC8968AA9} */
            }
            else
            {
                /* UserCode{BE946420-D7EF-4b2d-A91B-C90DEC42E278}:1MHx4Fs9iA */
                txc->tbr_len = tidx - txc->tbr_ridx;
                /* UserCode{BE946420-D7EF-4b2d-A91B-C90DEC42E278} */
            }
        }
        else
        {
        }
    }
    else
    {
    }

    if (txc->tbr_len > 0)
    {
        /* UserCode{3517E816-3F35-4bb2-B3E7-CE30571A145F}:15zCCGhCuY */
        todo = txc->tbr_len;
        todo = min(budget, todo);

        /* UserCode{3517E816-3F35-4bb2-B3E7-CE30571A145F} */

        work_done = oak_net_process_tx_pkt(txc, todo);
        /* UserCode{85806D83-122C-4906-88C1-F60D1DB5682E}:1IkAnVQ2nl */
        txc->tbr_len -= work_done;
        /* UserCode{85806D83-122C-4906-88C1-F60D1DB5682E} */
    }
    else
    {
    }

    if (txc->tbr_len == 0)
    {
        oak_unimac_ena_tx_ring_irq(np, ring, 1);
    }
    else
    {
    }
    rc_10 = work_done;
    return rc_10;
}

/* Name      : rx_work
 * Returns   : int
 * Parameters:  ldg_t * ldg = ldg,  uint32_t ring = ring,  int budget = budget
 *  */
static int oak_net_rx_work(ldg_t* ldg, uint32_t ring, int budget)
{
    oak_t* np = ldg->device;
    oak_rx_chan_t* rxc = &np->rx_channel[ring];
    int work_done = 0;
    struct sk_buff* skb;
    uint32_t reason;
    int todo;
    uint32_t ridx;
    int compl; /* automatically added for object flow handling */
    int rc_11; /* start of activity code */

    if (rxc->rbr_len == 0)
    {
/* UserCode{82986339-62BB-4e3f-A217-15B565C530E7}:56OFfV6b68 */
#if 1
        smp_mb();
        reason = le32_to_cpup(&rxc->mbox->intr_cause);
#else
        reason = oak_unimac_io_read_32(np, OAK_UNI_RX_RING_INT_CAUSE(ring));
#endif

        /* UserCode{82986339-62BB-4e3f-A217-15B565C530E7} */

        /* UserCode{E7226067-D597-4c0c-8106-4B15B6262F31}:z0EfbceAdj */
        ++rxc->stat.rx_interrupts;
        /* UserCode{E7226067-D597-4c0c-8106-4B15B6262F31} */

        oak_unimac_io_write_32(np, OAK_UNI_RX_RING_INT_CAUSE(ring), OAK_MBOX_RX_COMP);
/* UserCode{16314771-E1E1-4091-98B7-2CFE72B6C620}:yDFj0r5wf2 */
#if 1
        ridx = le32_to_cpup(&rxc->mbox->dma_ptr_rel);
#else
        ridx = oak_unimac_io_read_32(np, OAK_UNI_RX_RING_DMA_PTR(ring));
        ridx >>= 16;
#endif
        /* UserCode{16314771-E1E1-4091-98B7-2CFE72B6C620} */

        /* UserCode{4BE18A99-A709-45a8-B225-4C1EA1338FF0}:FWZpSLR4uu */
        reason &= (OAK_MBOX_RX_COMP | OAK_MBOX_RX_RES_LOW);
        /* UserCode{4BE18A99-A709-45a8-B225-4C1EA1338FF0} */

        if ((reason & OAK_MBOX_RX_COMP) != 0)
        {

            if (ridx < rxc->rbr_ridx)
            {
                /* UserCode{06C750AB-B3CB-478e-AE8E-EB4D6C471AE2}:bxnlWqXT24 */
                rxc->rbr_len = rxc->rbr_size - rxc->rbr_ridx + ridx;
                /* UserCode{06C750AB-B3CB-478e-AE8E-EB4D6C471AE2} */
            }
            else
            {
                /* UserCode{498F707A-A245-4240-8387-EEFD317EC07F}:E8hookst8s */
                rxc->rbr_len = ridx - rxc->rbr_ridx;
                /* UserCode{498F707A-A245-4240-8387-EEFD317EC07F} */
            }
        }
        else
        {
        }
    }
    else
    {
        /* UserCode{7388C643-C65B-45b3-8AAC-9F8813820C38}:BNiLKE3Gb7 */
        reason = 0;
        /* UserCode{7388C643-C65B-45b3-8AAC-9F8813820C38} */
    }
    /* UserCode{A92EA017-72C2-4d7a-AA7E-C04162FA9377}:3dhnRBuh36 */
    todo = rxc->rbr_len;
    todo = min(budget, todo);

    /* UserCode{A92EA017-72C2-4d7a-AA7E-C04162FA9377} */

    while ((todo > 0) && (rxc->rbr_len > 0))
    {
        /* UserCode{A2960C77-125F-4c6f-A430-103C347013BD}:PTea8jWXnO */
        skb = NULL;
        /* UserCode{A2960C77-125F-4c6f-A430-103C347013BD} */

        compl = oak_net_process_rx_pkt(rxc, rxc->rbr_len, &skb);

        if (skb != NULL)
        {

	    /* Static Counter: Increment rx stats counter and bytes for ifconfig */
	    np->netdev->stats.rx_packets++;
	    np->netdev->stats.rx_bytes += skb->len;

            /* UserCode{4A2ABB1B-70A0-4590-B495-B89B98EF2262}:bOp9jO7pRj */
            rxc->stat.rx_byte_count += skb->len;
            skb->protocol = eth_type_trans(skb, np->netdev);
            skb_record_rx_queue(skb, ldg->msi_grp);
            napi_gro_receive(&ldg->napi, skb);
            /* UserCode{4A2ABB1B-70A0-4590-B495-B89B98EF2262} */
        }
        else
        {
        }
        /* UserCode{9F35465F-015A-4daa-BD0B-AA0FD85A0B34}:qceTZkFgQQ */
        rxc->rbr_len -= compl;
        ++work_done;
        --todo;

        /* UserCode{9F35465F-015A-4daa-BD0B-AA0FD85A0B34} */
    }

    if (rxc->rbr_len == 0)
    {
        /* UserCode{B3D5E86D-1CD7-45b9-BA68-4879E112A26F}:1eM9cItc1i */
        oakdbg(debug, RX_STATUS, "irq enabled");
        /* UserCode{B3D5E86D-1CD7-45b9-BA68-4879E112A26F} */

        oak_unimac_io_write_32(np, OAK_UNI_RX_RING_INT_MASK(ring), OAK_MBOX_RX_COMP);
    }
    else
    {
    }
    rc_11 = work_done;
    return rc_11;
}

/* Name      : process_rx_pkt
 * Returns   : int
 * Parameters:  oak_rx_chan_t * rxc = rxc,  uint32_t desc_num = desc_num,  struct sk_buff ** target = skb
 *  */
static int oak_net_process_rx_pkt(oak_rx_chan_t* rxc, uint32_t desc_num, struct sk_buff** target)
{
    oak_t* np = rxc->oak;
    int work_done = 0;
    int comp_frame = 0;
    int tlen = 0;
    int good_frame;
    uint32_t blen;
    struct page* page;
    uint32_t offs = 0; /* automatically added for object flow handling */
    int rc_6; /* start of activity code */

    if (rxc->skb == NULL)
    {
        /* UserCode{463A4C3E-7C56-4ad6-BD8D-EBB63E8BBD88}:lkJpNPIZTf */
        rxc->skb = netdev_alloc_skb(np->netdev, OAK_RX_SKB_ALLOC_SIZE);
        rxc->skb->ip_summed = CHECKSUM_NONE; /* Default */
        good_frame = 0;
        /* UserCode{463A4C3E-7C56-4ad6-BD8D-EBB63E8BBD88} */
    }
    else
    {
        /* UserCode{EC8E2892-71C1-4cc6-8F71-D3CB8723B285}:jgXHSVoUTg */
        good_frame = 1; /* continue last good frame == 1 */
        ++rxc->stat.rx_fragments;
        tlen = rxc->skb->len;
        /* UserCode{EC8E2892-71C1-4cc6-8F71-D3CB8723B285} */
    }
    /* UserCode{A378C075-C0FD-4312-A174-49FB923F32F6}:AlW3f2IIZB */
    *target = NULL;
    /* UserCode{A378C075-C0FD-4312-A174-49FB923F32F6} */

    if (rxc->skb != NULL)
    {
        while ((desc_num > 0) && (comp_frame == 0))
        {
            /* UserCode{EC8339F2-3A98-465e-A421-FBEDC1D0545A}:fqxbNpM3V5 */
            oak_rxs_t* rsr = &rxc->rsr[rxc->rbr_ridx]; /* Rx status information */
            oak_rxa_t* rba = &rxc->rba[rxc->rbr_ridx]; /* Rx status information */

            blen = rsr->bc; /* Receive buffer length */
            tlen += blen;
            page = rba->page_virt;
            /* UserCode{EC8339F2-3A98-465e-A421-FBEDC1D0545A} */

            if (page != NULL)
            {

                if (rsr->first_last == 3)
                {

                    if (good_frame == 1)
                    {
                        /* UserCode{B3B507C8-5545-4c8c-A211-54A655F668E5}:1LiugKXqu6 */
                        ++rxc->stat.rx_no_eof;
                        good_frame = 0;

                        /* UserCode{B3B507C8-5545-4c8c-A211-54A655F668E5} */
                    }
                    else
                    {
                        /* UserCode{38A9E6B7-7924-465c-AF42-BAA3E91E48C7}:1alIFcbmM3 */
                        good_frame = 1;
                        /* UserCode{38A9E6B7-7924-465c-AF42-BAA3E91E48C7} */
                    }
                    /* UserCode{8DA4A0E0-B112-42b9-8B7C-93F748990607}:1z4Xa11Lwr */
                    comp_frame = 1;
                    /* UserCode{8DA4A0E0-B112-42b9-8B7C-93F748990607} */
                }
                else
                {

                    if (rsr->first_last == 2)
                    {

                        if (good_frame == 1)
                        {
                            /* UserCode{C13FCDF6-2986-4620-8F68-315C0DE655AE}:9q3jolCY8C */
                            ++rxc->stat.rx_no_eof;
                            good_frame = 0;
                            comp_frame = 1;

                            /* UserCode{C13FCDF6-2986-4620-8F68-315C0DE655AE} */
                        }
                        else
                        {
                            /* UserCode{228E7B01-3DBF-462e-B183-32B8F7E2E405}:1alIFcbmM3 */
                            good_frame = 1;
                            /* UserCode{228E7B01-3DBF-462e-B183-32B8F7E2E405} */
                        }
                    }
                    else
                    {

                        if (rsr->first_last == 1)
                        {

                            if (good_frame == 0)
                            {
                                /* UserCode{61873A84-7AAE-4ac0-BCD0-DE7D1AE715BA}:1GkcPbjpvV */
                                ++rxc->stat.rx_no_sof;
                                /* UserCode{61873A84-7AAE-4ac0-BCD0-DE7D1AE715BA} */
                            }
                            else
                            {
                            }
                            /* UserCode{CD25F994-5F04-4120-AA78-35381A3CAC98}:1z4Xa11Lwr */
                            comp_frame = 1;
                            /* UserCode{CD25F994-5F04-4120-AA78-35381A3CAC98} */
                        }
                        else
                        {

                            if (good_frame == 0)
                            {
                                /* UserCode{EEE249E6-B9F5-4827-BE95-540FFEAD1516}:1Fj87egmWo */
                                ++rxc->stat.rx_no_sof;
                                comp_frame = 1;
                                /* UserCode{EEE249E6-B9F5-4827-BE95-540FFEAD1516} */
                            }
                            else
                            {
                            }
                        }
                    }
                }

                if (good_frame == 1)
                {

                    if (mhdr != 0)
                    {

                        if ((rsr->first_last & 2) == 2)
                        {
                            /* UserCode{DB8DB6E5-5E17-4c1b-AAD5-6BE38A1A7999}:1b83mfTNiH */
                            blen -= 2;
                            tlen -= 2;
                            offs = 2;

                            /* UserCode{DB8DB6E5-5E17-4c1b-AAD5-6BE38A1A7999} */
                        }
                        else
                        {
                            /* UserCode{17E35181-CCD2-4e41-98B2-4F3E5EEA77AD}:ZRcCudxfG4 */
                            offs = 0;
                            /* UserCode{17E35181-CCD2-4e41-98B2-4F3E5EEA77AD} */
                        }
                    }
                    else
                    {
                    }
                    /* UserCode{9085B08F-763D-4581-A6A3-1ECCCD97F834}:H89MgRaUmi */
                    skb_fill_page_desc(rxc->skb, skb_shinfo(rxc->skb)->nr_frags, page, rba->page_offs + offs, blen);
                    rxc->skb->len += blen;
                    rxc->skb->data_len += blen;
                    rxc->skb->truesize += blen;

                    /* UserCode{9085B08F-763D-4581-A6A3-1ECCCD97F834} */
                }
                else
                {
                }

                if (rba->page_phys != 0)
                {
                    /* UserCode{863F73AF-3227-4d33-8C0F-2C9F5D90D514}:12PtoDOzFc */
                    dma_unmap_page(np->device, rba->page_phys, np->page_size, DMA_FROM_DEVICE);
                    ++rxc->stat.rx_unmap_pages;

                    /* UserCode{863F73AF-3227-4d33-8C0F-2C9F5D90D514} */

                    /* UserCode{077C0DF3-1BDF-4343-9CB2-6649102683E4}:1c0p5EanLJ */
                    oakdbg(debug, RX_STATUS, " free page=0x%p dma=0x%llx ", rba->page_virt, rba->page_phys);
                    /* UserCode{077C0DF3-1BDF-4343-9CB2-6649102683E4} */

                    /* UserCode{FE775913-0B95-4a62-95C8-6215891333DE}:XevOGArxua */
                    rba->page_phys = 0;
                    page->index = 0;
                    page->mapping = NULL;

                    /* UserCode{FE775913-0B95-4a62-95C8-6215891333DE} */

                    if (good_frame == 0)
                    {
                        /* UserCode{1DA09262-CA06-4639-9C54-4247B9662E70}:7EjViBJqCL */
                        __free_page(page);
                        /* UserCode{1DA09262-CA06-4639-9C54-4247B9662E70} */
                    }
                    else
                    {
                    }
                }
                else
                {

                    if (good_frame == 1)
                    {
                        /* UserCode{7B6426B0-3252-460f-95D9-731B4B42F0D2}:1tB6tufsII */
                        get_page(page);
                        /* UserCode{7B6426B0-3252-460f-95D9-731B4B42F0D2} */
                    }
                    else
                    {
                    }
                }
                /* UserCode{48338223-BD1F-4bd3-B44F-875143DF93C4}:ytXJgAnNOr */
                rba->page_virt = NULL;
                /* UserCode{48338223-BD1F-4bd3-B44F-875143DF93C4} */
            }
            else
            {
                /* UserCode{80FF3F13-9D16-4e6e-8869-CEF8FB2BF79E}:FSsYQIvlQS */
                good_frame = 0; /* Page lookup failure */

                /* UserCode{80FF3F13-9D16-4e6e-8869-CEF8FB2BF79E} */
            }

            if (comp_frame == 1)
            {

                if (good_frame == 1)
                {

                    if (rsr->es == 0)
                    {
                        rxc->skb->ip_summed = oak_chksum_get_rx_config(rxc, rsr);
                    }
                    else
                    {

                        if (rsr->ec == 0)
                        {
                            /* UserCode{19979735-CA07-4bd2-BAC8-C0904D2C2D14}:wUSqSDYlMj */
                            good_frame = 0;
                            ++rxc->stat.rx_badcrc;
                            /* UserCode{19979735-CA07-4bd2-BAC8-C0904D2C2D14} */
                        }
                        else
                        {

                            if (rsr->ec == 1)
                            {
                                /* UserCode{E6503C62-7BCE-4c3c-A21E-283190FD06CA}:1GEx7uWMlm */
                                ++rxc->stat.rx_badcsum;
                                /* UserCode{E6503C62-7BCE-4c3c-A21E-283190FD06CA} */
                            }
                            else
                            {

                                if (rsr->ec == 3)
                                {
                                    /* UserCode{D9D72763-83CB-419a-B9E2-D432DEAAD59E}:rb5ZR5R53f */
                                    ++rxc->stat.rx_nores;
                                    good_frame = 0;

                                    /* UserCode{D9D72763-83CB-419a-B9E2-D432DEAAD59E} */
                                }
                            }
                        }
                    }
                }
                else
                {
                }

                if (good_frame == 1)
                {
                    /* UserCode{C3BDEB80-B48B-4599-9EC2-87F4DF878FD6}:1bDa6o91Cs */
                    skb_reserve(rxc->skb, NET_IP_ALIGN);
                    if(!__pskb_pull_tail(rxc->skb, min(tlen, ETH_HLEN)))
                    {
                        /* Free the skb and stop processing this frame. */
                        dev_kfree_skb_any(rxc->skb);
                    }
                    else
                    {
                        ++rxc->stat.rx_goodframe;
                        *target = rxc->skb;
                    }
                    /* UserCode{C3BDEB80-B48B-4599-9EC2-87F4DF878FD6} */

                    oak_net_rx_stats(rxc, rxc->skb->len);
                    /* UserCode{D7C58606-6BA9-435b-A4DE-D891063D2CE6}:qvH0BVM8E7 */
                    rxc->skb = NULL;
                    /* UserCode{D7C58606-6BA9-435b-A4DE-D891063D2CE6} */
                }
                else
                {
                    /* UserCode{353FB2FB-55C0-4852-978C-DC9707A239D1}:1EfT17AKxx */
                    ++rxc->stat.rx_badframe;
                    dev_kfree_skb(rxc->skb);
                    rxc->skb = NULL;

                    /* UserCode{353FB2FB-55C0-4852-978C-DC9707A239D1} */
                }
                /* UserCode{EC660B6C-278F-4682-A60D-A5734D5107C9}:EStVhApUVS */
                oakdbg(debug, RX_STATUS, " page=0x%p good-frame=%d comp_frame-frame=%d ridx=%d tlen=%d", page, good_frame, comp_frame, rxc->rbr_ridx, tlen);
                /* UserCode{EC660B6C-278F-4682-A60D-A5734D5107C9} */
            }
            else
            {
            }
            /* UserCode{34A75361-2AF2-4bd5-9300-BD671327C6BE}:En0cFs1ufr */
            rxc->rbr_ridx = NEXT_IDX(rxc->rbr_ridx, rxc->rbr_size);
            --desc_num;
            atomic_dec(&rxc->rbr_pend);
            ++work_done;

            /* UserCode{34A75361-2AF2-4bd5-9300-BD671327C6BE} */
        }
    }
    else
    {
    }
    rc_6 = work_done;
    /* UserCode{F7527ED7-5F78-4c31-AF0A-3AD6943F8DBE}:w9Js65Q1os */
    oakdbg(debug, RX_STATUS, " work_done=%d skb=0x%p %s", work_done, *target, rxc->skb == NULL ? "" : "(continued)");
    /* UserCode{F7527ED7-5F78-4c31-AF0A-3AD6943F8DBE} */

    return rc_6;
}

/* Name      : process_channel
 * Returns   : int
 * Parameters:  ldg_t * ldg,  uint32_t ring,  uint32_t reason,  int budget
 *  */
static int oak_net_process_channel(ldg_t* ldg, uint32_t ring, uint32_t reason, int budget)
{
    uint16_t qidx = ring;
    int work_done = 0;
    int ret; /* automatically added for object flow handling */
    int work_done_25; /* start of activity code */
    oak_unimac_ena_tx_ring_irq(ldg->device, ring, 0);
    oak_unimac_ena_rx_ring_irq(ldg->device, ring, 0);

    if ((reason & OAK_INTR_MASK_RX_DMA) != 0)
    {
        work_done = oak_net_rx_work(ldg, ring, budget);
    }
    else
    {
    }

    if ((reason & OAK_INTR_MASK_RX_ERR) != 0)
    {
        oak_unimac_rx_error(ldg, ring);
    }
    else
    {
    }

    if ((reason & OAK_INTR_MASK_TX_ERR) != 0)
    {
        oak_unimac_tx_error(ldg, ring);
    }
    else
    {
    }

    if ((reason & OAK_INTR_MASK_TX_DMA) != 0)
    {
        ret = oak_net_tx_work(ldg, ring, budget);
        /* UserCode{2BCC8DB2-86F8-4203-B1F4-5126ADC19979}:6bt3HnddPc */
        work_done += ret;
        /* UserCode{2BCC8DB2-86F8-4203-B1F4-5126ADC19979} */

        if (ldg->device->level < 45 && __netif_subqueue_stopped(ldg->device->netdev, qidx) != 0)
        {
            /* UserCode{90ABEDB3-1905-4197-80FF-38396082F405}:SbSt0SFrCP */
            netif_wake_subqueue(ldg->device->netdev, qidx);
            /* UserCode{90ABEDB3-1905-4197-80FF-38396082F405} */

            /* UserCode{109A6B33-056D-4195-BD30-BE6A64E0C935}:1Ibm1ruKAT */
            oakdbg(debug, TX_QUEUED, "Wake Queue:%d pend=%d", ring, atomic_read(&ldg->device->tx_channel[ring].tbr_pend));
            /* UserCode{109A6B33-056D-4195-BD30-BE6A64E0C935} */
        }
        else
        {
        }
    }
    else
    {
    }
    work_done_25 = work_done;
    /* UserCode{A2429725-4F1A-40f4-8B23-54D64E5B1777}:IrPqjzVDBB */
    oakdbg(debug, PROBE, "chan=%i reason=0x%x work_done=%d", ring, reason, work_done);

    /* UserCode{A2429725-4F1A-40f4-8B23-54D64E5B1777} */

    oak_unimac_ena_tx_ring_irq(ldg->device, ring, 1);
    oak_unimac_ena_rx_ring_irq(ldg->device, ring, 1);
    return work_done_25;
}

/* Name      : poll
 * Returns   : int
 * Parameters:  struct napi_struct * napi,  int budget
 *  */
static int oak_net_poll(struct napi_struct* napi, int budget)
{
    ldg_t* ldg = container_of(napi, ldg_t, napi);
    oak_t* np = ldg->device;
    int work_done; /* automatically added for object flow handling */
    int work_done_11; /* start of activity code */
    work_done = oak_net_poll_core(np, ldg, budget);

    if (work_done < budget)
    {
        /* UserCode{DAFFB1D8-32DE-4100-96F8-84D973E2835A}:lxpjhUlTnu */
        napi_complete(napi);
        /* UserCode{DAFFB1D8-32DE-4100-96F8-84D973E2835A} */

        oak_irq_enable_gicu_64(np, ldg->irq_mask);
        oak_unimac_io_write_32(np, OAK_GICU_INTR_GRP_CLR_MASK, ldg->msi_grp | OAK_GICU_INTR_GRP_MASK_ENABLE);
    }
    else
    {
    }
    work_done_11 = work_done;
    return work_done_11;
}

/* Name      : poll_core
 * Returns   : int
 * Parameters:  oak_t * np,  ldg_t * ldg,  int budget
 *  */
static int oak_net_poll_core(oak_t* np, ldg_t* ldg, int budget)
{
    uint64_t irq_mask = 0;
    int work_done = 0;
    int todo;
    uint32_t ring;
    uint32_t mask_0;
    uint32_t mask_1;
    uint32_t irq_count;
    uint32_t irq_reason;
    uint64_t irq_next; /* automatically added for object flow handling */
    int work_completed_8; /* start of activity code */
    mask_0 = oak_unimac_io_read_32(np, OAK_GICU_INTR_FLAG_0);
    mask_1 = oak_unimac_io_read_32(np, OAK_GICU_INTR_FLAG_1);
    /* UserCode{74BC5184-0880-448a-A561-F79BE631302A}:14W0HFiW2k */
    irq_mask = mask_1;
    irq_mask <<= 32;
    irq_mask |= mask_0;
    irq_mask &= ldg->irq_mask;
    /* UserCode{74BC5184-0880-448a-A561-F79BE631302A} */

    if ((mask_1 & OAK_GICU_HOST_UNIMAC_P11_IRQ) != 0)
    {
        oak_unimac_process_status(ldg);
        /* UserCode{56DF9CAF-0E45-44a1-87DF-FC18D122448E}:8gTx1W4WwK */
        oakdbg(debug, INTR, "UNIMAC  P11 IRQ");
        /* UserCode{56DF9CAF-0E45-44a1-87DF-FC18D122448E} */
    }
    else
    {
    }

    if ((mask_1 & OAK_GICU_HOST_UNIMAC_P11_RESET) != 0)
    {
        /* UserCode{44E37433-CB28-42ad-90B1-765EB2FDF1AF}:11pE3mfKNk */
        oakdbg(debug, INTR, "UNIMAC  P11 RST");
        /* UserCode{44E37433-CB28-42ad-90B1-765EB2FDF1AF} */
    }
    else
    {
    }

    if ((mask_1 & OAK_GICU_HOST_MASK_E) != 0)
    {
        /* UserCode{FCC7FB07-E591-4b75-932D-1A878A932EE8}:hf9dkDUF0v */
        oakdbg(debug, INTR, "OTHER IRQ");
        /* UserCode{FCC7FB07-E591-4b75-932D-1A878A932EE8} */
    }
    else
    {
    }

    if (irq_mask != 0)
    {
        /* UserCode{583DE185-0596-4c00-903B-299B37F472F7}:19qcdliXcQ */
        uint32_t max_bits = sizeof(irq_mask) * 8;

        irq_next = (1ULL << ldg->irq_first);
        irq_count = ldg->irq_count;
        todo = budget;
        /* UserCode{583DE185-0596-4c00-903B-299B37F472F7} */

        while (irq_count > 0 && max_bits > 0)
        {

            if ((irq_mask & irq_next) != 0)
            {
                /* UserCode{DB67A622-1F0C-488e-8F73-2ACDE99D9111}:MlmoGgfzYB */
                ring = ilog2(irq_next);
                ring = ring / 4;
                irq_reason = irq_next >> (ring * 4);

                /* UserCode{DB67A622-1F0C-488e-8F73-2ACDE99D9111} */

                /* UserCode{63541D84-80CD-48df-A067-418F51E842AC}:1E52cc3Ybp */
                work_done += oak_net_process_channel(ldg, ring, irq_reason, todo);
                irq_count -= 1;
                /* UserCode{63541D84-80CD-48df-A067-418F51E842AC} */
            }
            else
            {
            }
            /* UserCode{758F0D06-7452-4a0e-85B9-1F37947B4C99}:p3ghub3lhT */
            irq_next <<= 1;
            max_bits -= 1;
            /* UserCode{758F0D06-7452-4a0e-85B9-1F37947B4C99} */
        }
    }
    else
    {
    }
    work_completed_8 = work_done;
    return work_completed_8;
}

/* Name      : stop_tx_queue
 * Returns   : int
 * Parameters:  oak_t * np,  uint32_t nfrags,  uint16_t txq
 *  */
static int oak_net_stop_tx_queue(oak_t* np, uint32_t nfrags, uint16_t txq)
{
    oak_tx_chan_t* txc = &np->tx_channel[txq];
    uint32_t free_desc; /* automatically added for object flow handling */
    int rc_1; /* start of activity code */
/* UserCode{FADD8790-0B36-4be7-9BC3-561CC1714EB0}:1VUuLljvwt */
#if 1
    free_desc = atomic_read(&txc->tbr_pend);
    free_desc = txc->tbr_size - free_desc;
#else
    free_desc = txc->tbr_size - txc->tbr_pend;
#endif
    /* UserCode{FADD8790-0B36-4be7-9BC3-561CC1714EB0} */

    if (free_desc <= nfrags)
    {
        /* UserCode{D3F5F168-AA73-43a8-98F5-89F5CFC3A7F3}:4mb2Fuu6hL */
        netif_stop_subqueue(np->netdev, txq);
        ++txc->stat.tx_stall_count;
        /* UserCode{D3F5F168-AA73-43a8-98F5-89F5CFC3A7F3} */

        /* UserCode{5B612DFF-0A00-4a6d-B582-894A1DE1EB40}:1U7g5MqoSj */
        oakdbg(debug, TX_QUEUED, "Stop Queue:%d pend=%d", txq, atomic_read(&txc->tbr_pend));
        /* UserCode{5B612DFF-0A00-4a6d-B582-894A1DE1EB40} */

        rc_1 = NETDEV_TX_BUSY;
    }
    else
    {
        rc_1 = 0;
    }
    return rc_1;
}

/* Name      : get_stats
 * Returns   : void
 * Parameters:  oak_t * np,  uint64_t * data
 *  */
void oak_net_get_stats(oak_t* np, uint64_t* data)
{
    /* UserCode{2585A73B-B912-4eb8-B899-FE715C67F273}:1xBgNfjofw */
    uint32_t i;
    np->unimac_stat.rx_good_frames = oak_unimac_io_read_32(np, OAK_UNI_STAT_RX_GOOD_FRAMES);
    np->unimac_stat.rx_bad_frames = oak_unimac_io_read_32(np, OAK_UNI_STAT_RX_BAD_FRAMES);
    np->unimac_stat.rx_stall_desc = oak_unimac_io_read_32(np, OAK_UNI_STAT_RX_STALL_DESC);
    np->unimac_stat.rx_stall_fifo = oak_unimac_io_read_32(np, OAK_UNI_STAT_RX_STALL_FIFO);
    np->unimac_stat.rx_discard_desc = oak_unimac_io_read_32(np, OAK_UNI_STAT_RX_DISC_DESC);

    np->unimac_stat.tx_pause = oak_unimac_io_read_32(np, OAK_UNI_STAT_TX_PAUSE);
    np->unimac_stat.tx_stall_fifo = oak_unimac_io_read_32(np, OAK_UNI_STAT_TX_STALL_FIFO);

    memcpy(data, &np->unimac_stat, sizeof(np->unimac_stat));
    data += sizeof(np->unimac_stat) / sizeof(uint64_t);
    for (i = 0; i < np->num_rx_chan; i++)
    {
        oak_rx_chan_t* rxc = &np->rx_channel[i];
        memcpy(data, &rxc->stat, sizeof(oak_driver_rx_stat));
        *data = i + 1;
        data += (sizeof(oak_driver_rx_stat) / sizeof(uint64_t));
    }

    for (i = 0; i < np->num_tx_chan; i++)
    {
        oak_tx_chan_t* txc = &np->tx_channel[i];
        memcpy(data, &txc->stat, sizeof(oak_driver_tx_stat));
        *data = i + 1;
        data += (sizeof(oak_driver_tx_stat) / sizeof(uint64_t));
    }
    /* UserCode{2585A73B-B912-4eb8-B899-FE715C67F273} */
}

/* Name      : add_txd_length
 * Returns   : void
 * Parameters:  oak_tx_chan_t * txc,  uint16_t len
 *  */
void oak_net_add_txd_length(oak_tx_chan_t* txc, uint16_t len)
{
    /* UserCode{01F72569-2E14-4cda-BE1B-BD96A1315D83}:1FVsFbfuan */
    oak_txd_t* txd;
    txd = &txc->tbr[txc->tbr_widx];
    txd->bc += len;
    /* UserCode{01F72569-2E14-4cda-BE1B-BD96A1315D83} */
}

/* Name      : set_txd_first
 * Returns   : void
 * Parameters:  oak_tx_chan_t * txc,  uint16_t len,  uint32_t g3,  uint32_t g4,  dma_addr_t map,  uint32_t sz,  int flags
 *  */
void oak_net_set_txd_first(oak_tx_chan_t* txc, uint16_t len, uint32_t g3, uint32_t g4, dma_addr_t map, uint32_t sz, int flags)
{
    /* UserCode{CE90F837-07F5-4c32-8572-FCB03D74346D}:1TzGXh7Sy5 */
    oak_txd_t* txd;
    oak_txi_t* tbi;

    txd = &txc->tbr[txc->tbr_widx];
    tbi = &txc->tbi[txc->tbr_widx];

    txd->bc = len;
    txd->res1 = 0;
    txd->last = 0;
    txd->first = 1;
    txd->gl3_chksum = g3;
    txd->gl4_chksum = g4;
    txd->res2 = 0;
    txd->time_valid = 0;
    txd->res3 = 0;
    txd->buf_ptr_lo = (map & 0xFFFFFFFF);

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
    txd->buf_ptr_hi = (map >> 32);
#else
    txd->buf_ptr_hi = 0;
#endif
    tbi->skb = NULL;
    tbi->page = NULL;
    tbi->mapping = map;
    tbi->mapsize = sz;
    /* First fragment my be page or single mapped */
    tbi->flags = flags;
    ++txc->stat.tx_fragm_count;
    /* UserCode{CE90F837-07F5-4c32-8572-FCB03D74346D} */
}

/* Name      : set_txd_page
 * Returns   : void
 * Parameters:  oak_tx_chan_t * txc,  uint16_t len,  dma_addr_t map,  uint32_t sz,  int flags
 *  */
void oak_net_set_txd_page(oak_tx_chan_t* txc, uint16_t len, dma_addr_t map, uint32_t sz, int flags)
{
    /* UserCode{B0E80509-FCC3-4716-91B2-354AFD80700E}:1bZ6yoO8NT */
    oak_txd_t* txd;
    oak_txi_t* tbi;

    /* txc->tbr_widx = ((txc->tbr_widx + 1) % txc->tbr_size) ;
 */
    txd = &txc->tbr[txc->tbr_widx];
    tbi = &txc->tbi[txc->tbr_widx];

    txd->bc = len;
    txd->res1 = 0;
    txd->last = 0;
    txd->first = 0;
    txd->gl3_chksum = 0;
    txd->gl4_chksum = 0;
    txd->res2 = 0;
    txd->time_valid = 0;
    txd->res3 = 0;
    txd->buf_ptr_lo = (map & 0xFFFFFFFF);
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
    txd->buf_ptr_hi = (map >> 32);
#else
    txd->buf_ptr_hi = 0;
#endif
    tbi->skb = NULL;
    tbi->page = NULL;
    tbi->mapping = map;
    tbi->mapsize = sz;
    tbi->flags = flags;
    ++txc->stat.tx_fragm_count;
    /* UserCode{B0E80509-FCC3-4716-91B2-354AFD80700E} */
}

/* Name      : set_txd_last
 * Returns   : void
 * Parameters:  oak_tx_chan_t * txc,  struct sk_buff * skb,  struct page * page
 *  */
void oak_net_set_txd_last(oak_tx_chan_t* txc, struct sk_buff* skb, struct page* page)
{
    /* UserCode{320ECED6-3A2E-4f25-8C99-146B71A6306C}:1Eh3UXW2NZ */
    txc->tbr[txc->tbr_widx].last = 1;
    txc->tbi[txc->tbr_widx].skb = skb;
    txc->tbi[txc->tbr_widx].page = page;
    txc->tbi[txc->tbr_widx].flags |= TX_BUFF_INFO_EOP;
    ++txc->stat.tx_fragm_count;
    /* UserCode{320ECED6-3A2E-4f25-8C99-146B71A6306C} */
}

/* Name      : oak_pcie_get_width_cap
 * Returns   : pcie_link_width
 * Parameters: struct pci_dev * pdev
 * Note      : pcie_get_width_cap() API is not available in few platforms,
 * so added a function for the same.
 *  */
enum pcie_link_width oak_net_pcie_get_width_cap(struct pci_dev *pdev)
{
    u32 lnkcap;
    enum pcie_link_width wdth;

    pcie_capability_read_dword(pdev, PCI_EXP_LNKCAP, &lnkcap);
    if (lnkcap)
        wdth = (lnkcap & PCI_EXP_LNKCAP_MLW) >> 4;
    else
        wdth = PCIE_LNK_WIDTH_UNKNOWN;

    return wdth;
}
