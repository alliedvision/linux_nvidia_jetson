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
#include "oak_ctl.h"

extern int debug;

/* private function prototypes */
static uint32_t oak_ctl_esu_rd32(oak_t* np, oak_ioc_reg* req);
static void oak_ctl_esu_wr32(oak_t* np, oak_ioc_reg* req);

/* Name      : channel_status_access
 * Returns   : int
 * Parameters:  oak_t * np = np,  struct ifreq * ifr = ifr,  int cmd = cmd
 *  */
int oak_ctl_channel_status_access(oak_t* np, struct ifreq* ifr, int cmd)
{
    oak_ioc_stat req;
    int rc; /* automatically added for object flow handling */
    int rc_1; /* start of activity code */
    /* UserCode{F34AA6EC-27B5-452a-BEE5-FA043690008D}:l6ARB3E6qd */
    rc = copy_from_user(&req, ifr->ifr_data, sizeof(req));
    /* UserCode{F34AA6EC-27B5-452a-BEE5-FA043690008D} */

    if (rc == 0)
    {

        if (req.cmd == OAK_IOCTL_STAT_GET_TXS)
        {

            if (req.idx < np->num_tx_chan)
            {
                /* UserCode{35DF9CCE-A639-4d25-9630-2CF684350DF3}:SQ2S65WXZj */
                oak_chan_info* tar = (oak_chan_info*)req.data;
                oak_tx_chan_t* src = &np->tx_channel[req.idx];
                tar->flags = src->flags;
                tar->r_size = src->tbr_size; /* RX/TX ring size */
#if 0
tar->r_pend = src->tbr_pend ; /* number of pending ring buffers */
#else
                tar->r_pend = atomic_read(&src->tbr_pend);
#endif
                tar->r_widx = src->tbr_widx; /* write buffer index */
                tar->r_ridx = src->tbr_ridx; /* read buffer index */

                /* UserCode{35DF9CCE-A639-4d25-9630-2CF684350DF3} */
            }
            else
            {
                /* UserCode{9854F59E-6023-42e0-8743-8957960F4DCF}:17zylXIV9A */
                rc = -ENOMEM;
                /* UserCode{9854F59E-6023-42e0-8743-8957960F4DCF} */
            }
        }
        else
        {

            if (req.cmd == OAK_IOCTL_STAT_GET_RXS)
            {

                if (req.idx < np->num_rx_chan)
                {
                    /* UserCode{A067A39E-44A4-40bb-834D-B3DDF25E7120}:bxwlZFt4bc */
                    oak_chan_info* tar = (oak_chan_info*)req.data;
                    oak_rx_chan_t* src = &np->rx_channel[req.idx];
                    tar->flags = src->flags;
                    tar->r_size = src->rbr_size; /* RX/TX ring size */
#if 0
tar->r_pend = src->rbr_pend ; /* number of pending ring buffers */
#else
                    tar->r_pend = atomic_read(&src->rbr_pend);
#endif
                    tar->r_widx = src->rbr_widx; /* write buffer index */
                    tar->r_ridx = src->rbr_ridx; /* read buffer index */
                    /* UserCode{A067A39E-44A4-40bb-834D-B3DDF25E7120} */
                }
                else
                {
                    /* UserCode{A7DC593F-DABE-422f-92AD-5E39950B207E}:17zylXIV9A */
                    rc = -ENOMEM;
                    /* UserCode{A7DC593F-DABE-422f-92AD-5E39950B207E} */
                }
            }
            else
            {

                if (req.cmd == OAK_IOCTL_STAT_GET_TXC)
                {

                    if ((req.idx < np->num_tx_chan) && (req.offs < np->tx_channel[req.idx].tbr_size))
                    {
                        /* UserCode{2C832C29-F277-4133-824E-6E8B8063FC7E}:3CHMZzeJvW */
                        memcpy(req.data, &np->tx_channel[req.idx].tbr[req.offs], sizeof(oak_txd_t));
                        /* UserCode{2C832C29-F277-4133-824E-6E8B8063FC7E} */
                    }
                    else
                    {
                        /* UserCode{CD29AD8F-2424-4e9e-973D-023A04391A44}:17zylXIV9A */
                        rc = -ENOMEM;
                        /* UserCode{CD29AD8F-2424-4e9e-973D-023A04391A44} */
                    }
                }
                else
                {

                    if (req.cmd == OAK_IOCTL_STAT_GET_RXC)
                    {

                        if ((req.idx < np->num_rx_chan) && (req.offs < np->rx_channel[req.idx].rbr_size))
                        {
                            /* UserCode{7BF86B7A-DFAF-44b4-8586-6E503F672048}:1JX09XBH5A */
                            memcpy(req.data, &np->rx_channel[req.idx].rsr[req.offs], sizeof(oak_rxs_t));

                            /* UserCode{7BF86B7A-DFAF-44b4-8586-6E503F672048} */
                        }
                        else
                        {
                            /* UserCode{21549931-9E1E-4569-A756-1D2245006629}:17zylXIV9A */
                            rc = -ENOMEM;
                            /* UserCode{21549931-9E1E-4569-A756-1D2245006629} */
                        }
                    }
                    else
                    {

                        if (req.cmd == OAK_IOCTL_STAT_GET_RXB)
                        {

                            if ((req.idx < np->num_rx_chan) && (req.offs < np->rx_channel[req.idx].rbr_size))
                            {
                                /* UserCode{4EE8FD73-DE16-44d1-9CF6-063AB836097D}:1UWCCUh5z6 */
                                memcpy(req.data, &np->rx_channel[req.idx].rbr[req.offs], sizeof(oak_rxd_t));
                                /* UserCode{4EE8FD73-DE16-44d1-9CF6-063AB836097D} */
                            }
                            else
                            {
                                /* UserCode{492C3BEE-6AF5-48b7-BC93-75D7C7562CB7}:17zylXIV9A */
                                rc = -ENOMEM;
                                /* UserCode{492C3BEE-6AF5-48b7-BC93-75D7C7562CB7} */
                            }
                        }
                        else
                        {

                            if (req.cmd == OAK_IOCTL_STAT_GET_LDG)
                            {

                                if ((req.idx < np->gicu.num_ldg) && (sizeof(req.data) >= (8 * sizeof(uint64_t))))
                                {
                                    /* UserCode{28E9CA47-5BCD-47af-AB66-E88D7DF1D33F}:MK1slGJH7a */
                                    uint64_t* tar = (uint64_t*)req.data;
                                    tar[0] = np->gicu.num_ldg;
                                    if (np->num_rx_chan < np->num_tx_chan)
                                    {
                                        tar[1] = np->num_tx_chan;
                                    }
                                    else
                                    {
                                        tar[1] = np->num_rx_chan;
                                    }
                                    tar[2] = np->gicu.ldg[req.idx].msi_tx;
                                    tar[3] = np->gicu.ldg[req.idx].msi_te;
                                    tar[4] = np->gicu.ldg[req.idx].msi_rx;
                                    tar[5] = np->gicu.ldg[req.idx].msi_re;
                                    tar[6] = np->gicu.ldg[req.idx].msi_ge;
                                    tar[7] = np->gicu.msi_vec[req.idx].vector;
                                    /* UserCode{28E9CA47-5BCD-47af-AB66-E88D7DF1D33F} */
                                }
                                else
                                {
                                    /* UserCode{5B10FB38-0325-4d6c-BD08-9E6B43C1A58E}:17zylXIV9A */
                                    rc = -ENOMEM;
                                    /* UserCode{5B10FB38-0325-4d6c-BD08-9E6B43C1A58E} */
                                }
                            }
                            else
                            {
                                /* UserCode{7FE403F6-CEC8-4240-8FAD-C5CA0D176702}:FubMmJTDKN */
                                rc = -EINVAL;
                                /* UserCode{7FE403F6-CEC8-4240-8FAD-C5CA0D176702} */
                            }
                        }
                    }
                }
            }
        }
        /* UserCode{5B3C2789-2206-4e48-AB74-7298EC30177D}:UkatlF1Wx3 */
        req.error = rc;
        rc = copy_to_user(ifr->ifr_data, &req, sizeof(req));
        /* UserCode{5B3C2789-2206-4e48-AB74-7298EC30177D} */
    }
    else
    {
    }
    rc_1 = rc;
    /* UserCode{81560891-39B6-4d6c-9F43-52D70FF6F3FD}:T7ELqq2Eec */
    oakdbg(debug, DRV, "np-level=%d cmd=0x%x req=0x%x rc=%d", np->level, cmd, req.cmd, rc);
    /* UserCode{81560891-39B6-4d6c-9F43-52D70FF6F3FD} */

    return rc_1;
}

/* Name      : esu_rd32
 * Returns   : uint32_t
 * Parameters:  oak_t * np = np,  oak_ioc_reg * req = req
 *  */
static uint32_t oak_ctl_esu_rd32(oak_t* np, oak_ioc_reg* req)
{
    /* automatically added for object flow handling */
    uint32_t rc_1 = 0; /* start of activity code */
    /* UserCode{FECF5077-0413-48ff-BB0E-66C26AA161CC}:1LCij4OtQQ */
    uint32_t offs = req->offs;
    uint32_t reg = (offs & 0x1F);
    uint32_t val;

    offs &= ~0x0000001f;
    offs |= (req->dev_no << 7) | (reg << 2);

    /* UserCode{FECF5077-0413-48ff-BB0E-66C26AA161CC} */

    /* UserCode{FDCB5F73-5538-4f83-9E49-A0F50CD90EBF}:r0Rynu0uqE */
    val = sr32(np, offs);
    /* UserCode{FDCB5F73-5538-4f83-9E49-A0F50CD90EBF} */

    /* UserCode{7265991E-427A-4c00-B555-C8687444FA28}:119qb1dGuY */
    oakdbg(debug, DRV, "ESU RD at offset: 0x%x device: %d data=0x%x", offs, req->dev_no, val);
    /* UserCode{7265991E-427A-4c00-B555-C8687444FA28} */

    rc_1 = val;
    return rc_1;
}

/* Name      : esu_wr32
 * Returns   : void
 * Parameters:  oak_t * np = np,  oak_ioc_reg * req = req
 *  */
static void oak_ctl_esu_wr32(oak_t* np, oak_ioc_reg* req)
{
    /* start of activity code */
    /* UserCode{585F4434-507C-4c05-AE8E-E93578B0B1BA}:1YqGmMjpuN */
    uint32_t offs = req->offs;
    uint32_t reg = (offs & 0x1F);

    offs &= ~0x0000001f;
    offs |= (req->dev_no << 7) | (reg << 2);
    /* UserCode{585F4434-507C-4c05-AE8E-E93578B0B1BA} */

    /* UserCode{FC239E06-9ADA-4828-89EB-2E78D6350240}:1efotd5Dsz */
    sw32(np, offs, req->data);
    /* UserCode{FC239E06-9ADA-4828-89EB-2E78D6350240} */

    /* UserCode{214A7204-3051-4abb-9A30-A8F2B21C0B38}:bHeFvC2vGL */
    oakdbg(debug, DRV, "ESU WR at offset: 0x%x device: %d data=0x%x", offs, req->dev_no, req->data);
    /* UserCode{214A7204-3051-4abb-9A30-A8F2B21C0B38} */

    return;
}

/* Name      : set_mac_rate
 * Returns   : int
 * Parameters:  oak_t * np = np,  struct ifreq * ifr = ifr,  int cmd
 *  */
int oak_ctl_set_mac_rate(oak_t* np, struct ifreq* ifr, int cmd)
{
    uint32_t cls = OAK_MIN_TX_RATE_CLASS_A;
    oak_ioc_set ioc;
    int rc; /* automatically added for object flow handling */
    int rc_12; /* start of activity code */
    /* UserCode{7B29C9BF-6582-4c9d-99FB-4432B4BC6C25}:lYS0x0eI4j */
    rc = copy_from_user(&ioc, ifr->ifr_data, sizeof(ioc));
    /* UserCode{7B29C9BF-6582-4c9d-99FB-4432B4BC6C25} */

    if (rc == 0)
    {

        if (cmd == OAK_IOCTL_SET_MAC_RATE_B)
        {
            /* UserCode{E6381D62-9848-4aef-BFF6-433921A21704}:4FUg3z82bZ */
            cls = OAK_MIN_TX_RATE_CLASS_B;
            /* UserCode{E6381D62-9848-4aef-BFF6-433921A21704} */
        }
        else
        {
        }

        if (ioc.idx > 0)
        {
            rc = oak_unimac_set_tx_ring_rate(np, ioc.idx - 1, cls, 0x600, ioc.data & 0x1FFFF);
        }
        else
        {
            rc = oak_unimac_set_tx_mac_rate(np, cls, 0x600, ioc.data & 0x1FFFF);
        }
    }
    else
    {
    }
    rc_12 = rc;
    return rc_12;
}

/* Name      : set_rx_flow
 * Returns   : int
 * Parameters:  oak_t * np = np,  struct ifreq * ifr = ifr,  int cmd = cmd
 *  */
int oak_ctl_set_rx_flow(oak_t* np, struct ifreq* ifr, int cmd)
{
    oak_ioc_flow ioc;
    int rc;
    uint16_t v0;
    uint16_t v1; /* automatically added for object flow handling */
    int rc_37; /* start of activity code */
    /* UserCode{A8508237-147F-4653-8B01-BA2D85A883C4}:lYS0x0eI4j */
    rc = copy_from_user(&ioc, ifr->ifr_data, sizeof(ioc));
    /* UserCode{A8508237-147F-4653-8B01-BA2D85A883C4} */

    if (rc == 0)
    {
        /* UserCode{FD4D50DA-9FEE-42c5-9416-A292928822F3}:FubMmJTDKN */
        rc = -EINVAL;
        /* UserCode{FD4D50DA-9FEE-42c5-9416-A292928822F3} */

        if (ioc.idx > 0 && ioc.idx < 10)
        {

            if (ioc.cmd == OAK_IOCTL_RXFLOW_CLEAR)
            {
                oak_unimac_set_rx_none(np, ioc.idx);
                /* UserCode{F6384BE8-8793-4c4b-8B3C-20B2EE7A16F3}:r8srRGlzXT */
                rc = 0;
                /* UserCode{F6384BE8-8793-4c4b-8B3C-20B2EE7A16F3} */
            }
            else
            {

                if (ioc.cmd == OAK_IOCTL_RXFLOW_MGMT)
                {
                    oak_unimac_set_rx_mgmt(np, ioc.idx, ioc.val_lo, ioc.ena);
                    /* UserCode{75B2709C-68A1-418d-B8B7-2859F3EC50AB}:r8srRGlzXT */
                    rc = 0;
                    /* UserCode{75B2709C-68A1-418d-B8B7-2859F3EC50AB} */
                }
                else
                {

                    if (ioc.cmd == OAK_IOCTL_RXFLOW_QPRI)
                    {
                        oak_unimac_set_rx_8021Q_qpri(np, ioc.idx, ioc.val_lo, ioc.ena);
                        /* UserCode{48370F49-1221-456c-A40A-C62A18330668}:r8srRGlzXT */
                        rc = 0;
                        /* UserCode{48370F49-1221-456c-A40A-C62A18330668} */
                    }
                    else
                    {

                        if (ioc.cmd == OAK_IOCTL_RXFLOW_SPID)
                        {
                            oak_unimac_set_rx_8021Q_spid(np, ioc.idx, ioc.val_lo, ioc.ena);
                            /* UserCode{42BDE506-C0D4-4ab2-9C62-361F53779E25}:r8srRGlzXT */
                            rc = 0;
                            /* UserCode{42BDE506-C0D4-4ab2-9C62-361F53779E25} */
                        }
                        else
                        {

                            if (ioc.cmd == OAK_IOCTL_RXFLOW_FLOW)
                            {
                                oak_unimac_set_rx_8021Q_flow(np, ioc.idx, ioc.val_lo, ioc.ena);
                                /* UserCode{9DB5D93A-0FD5-424b-B6CC-AFC33CF07FEC}:r8srRGlzXT */
                                rc = 0;
                                /* UserCode{9DB5D93A-0FD5-424b-B6CC-AFC33CF07FEC} */
                            }
                            else
                            {

                                if (ioc.cmd == OAK_IOCTL_RXFLOW_DA)
                                {
                                    oak_unimac_set_rx_da(np, ioc.idx, ioc.data, ioc.ena);
                                    /* UserCode{5AA1F9D4-63B4-4fc2-A9F0-D144E9E00A25}:r8srRGlzXT */
                                    rc = 0;
                                    /* UserCode{5AA1F9D4-63B4-4fc2-A9F0-D144E9E00A25} */
                                }
                                else
                                {

                                    if (ioc.cmd == OAK_IOCTL_RXFLOW_DA_MASK && np->pci_class_revision >= 1)
                                    {
                                        oak_unimac_set_rx_da_mask(np, ioc.idx, ioc.data, ioc.ena);
                                        /* UserCode{3465B918-A659-4c15-B5AD-9AB77ACE9026}:r8srRGlzXT */
                                        rc = 0;
                                        /* UserCode{3465B918-A659-4c15-B5AD-9AB77ACE9026} */
                                    }
                                    else
                                    {

                                        if (ioc.cmd == OAK_IOCTL_RXFLOW_FID)
                                        {
                                            oak_unimac_set_rx_8021Q_fid(np, ioc.idx, ioc.val_lo, ioc.ena);
                                            /* UserCode{551464D9-DB6B-4a24-A7B5-008C1E7D50B9}:r8srRGlzXT */
                                            rc = 0;
                                            /* UserCode{551464D9-DB6B-4a24-A7B5-008C1E7D50B9} */
                                        }
                                        else
                                        {

                                            if (ioc.cmd == OAK_IOCTL_RXFLOW_ET)
                                            {

                                                if (ioc.ena != 0)
                                                {
                                                    /* UserCode{F4B179CF-794D-4a47-815B-E66249277271}:1QP6h2XPoR */
                                                    v0 = ioc.val_lo & 0xFFFF;
                                                    v1 = ioc.val_hi & 0xFFFF;
                                                    /* UserCode{F4B179CF-794D-4a47-815B-E66249277271} */
                                                }
                                                else
                                                {
                                                    /* UserCode{24C33268-456D-4be5-8C48-FBE9FFB45774}:U8IcixnmKd */
                                                    v0 = 0;
                                                    v1 = 0;
                                                    /* UserCode{24C33268-456D-4be5-8C48-FBE9FFB45774} */
                                                }
                                                oak_unimac_set_rx_8021Q_et(np, ioc.idx, v0, v1, ioc.ena);
                                                /* UserCode{298DA314-4B48-4c9d-A79C-324E6D8E026F}:r8srRGlzXT */
                                                rc = 0;
                                                /* UserCode{298DA314-4B48-4c9d-A79C-324E6D8E026F} */
                                            }
                                            else
                                            {
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        else
        {
        }
        /* UserCode{1DB82AD5-13C8-4ffc-BA22-883CE108688A}:1NPBPpWDbt */
        ioc.error = rc;
        /* UserCode{1DB82AD5-13C8-4ffc-BA22-883CE108688A} */

        /* UserCode{B394B0EF-8932-4b09-B659-4BE2BDC09052}:IOHxKSFByd */
        rc = copy_to_user(ifr->ifr_data, &ioc, sizeof(ioc));
        /* UserCode{B394B0EF-8932-4b09-B659-4BE2BDC09052} */
    }
    else
    {
    }
    /* UserCode{CF832F61-570E-4f40-94AD-DC09FB28D3D1}:hdlNM0yhVZ */
    oakdbg(debug, DRV, "cmd:0x%x ioc=0x%x err=%d", cmd, ioc.cmd, rc);
    /* UserCode{CF832F61-570E-4f40-94AD-DC09FB28D3D1} */

    rc_37 = rc;
    return rc_37;
}

/* Name      : set_txr_rate
 * Returns   : int
 * Parameters:  oak_t * np = np,  struct ifreq * ifr = ifr,  int cmd = cmd
 *  */
int oak_ctl_set_txr_rate(oak_t* np, struct ifreq* ifr, int cmd)
{
    uint32_t reg;
    uint32_t val;
    oak_ioc_set ioc;
    int rc = 0; /* automatically added for object flow handling */
    int rc_7 = rc; /* start of activity code */
    /* UserCode{B4CFD9EA-BB12-4a4e-A1A1-186E31BF61D3}:lYS0x0eI4j */
    rc = copy_from_user(&ioc, ifr->ifr_data, sizeof(ioc));
    /* UserCode{B4CFD9EA-BB12-4a4e-A1A1-186E31BF61D3} */

    if (rc == 0)
    {

        if (cmd == OAK_IOCTL_SET_TXR_RATE)
        {
            /* UserCode{29BAA251-3E50-46a6-99A5-53067C013E2F}:1ERI235gzS */
            reg = OAK_UNI_TX_RING_RATECTRL(ioc.idx);
            /* UserCode{29BAA251-3E50-46a6-99A5-53067C013E2F} */

            val = oak_unimac_io_read_32(np, reg);
            /* UserCode{00FE148C-B47E-4ac2-B31D-8AEF8C79B2B1}:YBZsW0iftP */
            val &= ~0x1FFFF;
            val |= ioc.data & 0x1FFFF;

            /* UserCode{00FE148C-B47E-4ac2-B31D-8AEF8C79B2B1} */

            oak_unimac_io_write_32(np, reg, val);
        }
        else
        {
        }
    }
    else
    {
    }
    rc_7 = rc;
    return rc_7;
}

/* Name      : direct_register_access
 * Returns   : int
 * Parameters:  oak_t * np,  struct ifreq * ifr,  int cmd
 *  */
int oak_ctl_direct_register_access(oak_t* np, struct ifreq* ifr, int cmd)
{
    int reg_timeout = 100;
    int rc;
    uint32_t val;
    oak_ioc_reg req; /* automatically added for object flow handling */
    int rc_27; /* start of activity code */
    /* UserCode{02345228-9903-4361-B05F-71715F7B8115}:l6ARB3E6qd */
    rc = copy_from_user(&req, ifr->ifr_data, sizeof(req));
    /* UserCode{02345228-9903-4361-B05F-71715F7B8115} */

    if (rc == 0)
    {

        if (cmd == OAK_IOCTL_REG_ESU_REQ && req.cmd == OAK_IOCTL_REG_RD)
        {
            req.data = oak_ctl_esu_rd32(np, &req);
            /* UserCode{89984F70-DEC2-4561-9911-3C0F9D08D450}:r8srRGlzXT */
            rc = 0;
            /* UserCode{89984F70-DEC2-4561-9911-3C0F9D08D450} */
        }
        else
        {
        }

        if (cmd == OAK_IOCTL_REG_MAC_REQ && req.cmd == OAK_IOCTL_REG_RD)
        {
            req.data = oak_unimac_io_read_32(np, req.offs);
            /* UserCode{7A8D37F6-D270-4d52-8AD0-2978A51E2609}:r8srRGlzXT */
            rc = 0;
            /* UserCode{7A8D37F6-D270-4d52-8AD0-2978A51E2609} */
        }
        else
        {
        }

        if (cmd == OAK_IOCTL_REG_ESU_REQ && req.cmd == OAK_IOCTL_REG_WR)
        {
            oak_ctl_esu_wr32(np, &req);
            /* UserCode{1E14164F-1ECB-416e-9575-C59AF5B918F9}:r8srRGlzXT */
            rc = 0;
            /* UserCode{1E14164F-1ECB-416e-9575-C59AF5B918F9} */
        }
        else
        {
        }

        if (cmd == OAK_IOCTL_REG_MAC_REQ && req.cmd == OAK_IOCTL_REG_WR)
        {
            oak_unimac_io_write_32(np, req.offs, req.data);
            /* UserCode{192FCA94-44BC-416e-9FC0-53B3E67767D6}:r8srRGlzXT */
            rc = 0;
            /* UserCode{192FCA94-44BC-416e-9FC0-53B3E67767D6} */
        }
        else
        {
        }

        if (req.cmd == OAK_IOCTL_REG_WC)
        {
            do
            {

                if (cmd == OAK_IOCTL_REG_ESU_REQ)
                {
                    val = oak_ctl_esu_rd32(np, &req);
                }
                else
                {
                    val = oak_unimac_io_read_32(np, req.offs);
                }
            } while ((--reg_timeout > 0) && ((val & (1 << req.data)) != 0));

            if (reg_timeout == 0)
            {
                /* UserCode{DD925FEE-BF13-4883-B14D-4FD4D79941CB}:tGbLT2rByH */
                rc = -EFAULT;
                /* UserCode{DD925FEE-BF13-4883-B14D-4FD4D79941CB} */
            }
            else
            {
                /* UserCode{A45BA98D-F2BE-4f62-955A-E5921C5D0AD2}:r8srRGlzXT */
                rc = 0;
                /* UserCode{A45BA98D-F2BE-4f62-955A-E5921C5D0AD2} */
            }
        }
        else
        {
        }

        if (req.cmd == OAK_IOCTL_REG_WS)
        {
            /* UserCode{1C88F2C9-E848-45c9-A475-6EFE6C694C3A}:AY8ytJtEEA */
            reg_timeout = 100;
            /* UserCode{1C88F2C9-E848-45c9-A475-6EFE6C694C3A} */

            do
            {

                if (cmd == OAK_IOCTL_REG_ESU_REQ)
                {
                    val = oak_ctl_esu_rd32(np, &req);
                }
                else
                {
                    val = oak_unimac_io_read_32(np, req.offs);
                }
            } while ((--reg_timeout > 0) && ((val & (1 << req.data)) == 0));

            if (reg_timeout == 0)
            {
                /* UserCode{40C5AD02-DB77-40b4-B266-C0A27943F0EC}:tGbLT2rByH */
                rc = -EFAULT;
                /* UserCode{40C5AD02-DB77-40b4-B266-C0A27943F0EC} */
            }
            else
            {
                /* UserCode{408CB6B1-B0DE-4191-A185-87ADA2950009}:r8srRGlzXT */
                rc = 0;
                /* UserCode{408CB6B1-B0DE-4191-A185-87ADA2950009} */
            }
        }
        else
        {
        }
        /* UserCode{82403923-7F6B-4555-8A0D-044A78B958F9}:gRYJRUVnrn */
        oakdbg(debug, DRV, "MAC RD at offset: 0x%x data=0x%x err=%d", req.offs, req.data, rc);
        /* UserCode{82403923-7F6B-4555-8A0D-044A78B958F9} */

        /* UserCode{DEFC1231-3CCD-4414-8FA4-319C7446ED90}:UkatlF1Wx3 */
        req.error = rc;
        rc = copy_to_user(ifr->ifr_data, &req, sizeof(req));
        /* UserCode{DEFC1231-3CCD-4414-8FA4-319C7446ED90} */
    }
    else
    {
        /* UserCode{F8468CF2-1C9F-42c2-B726-9707E94FA621}:tGbLT2rByH */
        rc = -EFAULT;
        /* UserCode{F8468CF2-1C9F-42c2-B726-9707E94FA621} */
    }
    rc_27 = rc;
    /* UserCode{38A57CE5-C23F-4c06-B42F-9B8B3A6FFA93}:e6dJTa0pdt */
    oakdbg(debug, DRV, "cmd=0x%xreq=0x%x rc=%d", cmd, req.cmd, rc);
    /* UserCode{38A57CE5-C23F-4c06-B42F-9B8B3A6FFA93} */

    return rc_27;
}

