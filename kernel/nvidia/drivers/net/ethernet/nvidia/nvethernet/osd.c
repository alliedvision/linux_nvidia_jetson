/*
 * Copyright (c) 2018-2023, NVIDIA CORPORATION.  All rights reserved.
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

#include "ether_linux.h"

/**
 * @brief ether_get_free_tx_ts_node - get free node for pending SKB
 *
 * Algorithm:
 *  - Find index of statically allocayted free memory for pending SKB
 *
 * @param[in] pdata: OSD private data structure.
 *
 * @retval index number
 */
static inline unsigned int ether_get_free_tx_ts_node(struct ether_priv_data *pdata)
{
	unsigned int i;

	for (i = 0; i < ETHER_MAX_PENDING_SKB_CNT; i++) {
		if (pdata->tx_ts_skb[i].in_use == OSI_NONE) {
			break;
		}
	}

	return i;
}

static inline void add_skb_node(struct ether_priv_data *pdata, struct sk_buff *skb,
				unsigned int pktid) {
	struct list_head *head_node, *temp_head_node;
	struct ether_tx_ts_skb_list *pnode = NULL;
	unsigned int idx;
	unsigned long flags;
	unsigned long now_jiffies = jiffies;

	if (list_empty(&pdata->tx_ts_skb_head)) {
		goto empty;
	}

	raw_spin_lock_irqsave(&pdata->txts_lock, flags);
	list_for_each_safe(head_node, temp_head_node,
			   &pdata->tx_ts_skb_head) {
		pnode = list_entry(head_node,
				   struct ether_tx_ts_skb_list,
				   list_head);

		if ((jiffies_to_msecs(now_jiffies) - jiffies_to_msecs(pnode->pkt_jiffies))
		     >= ETHER_SECTOMSEC) {
			dev_dbg(pdata->dev, "%s() skb %p deleting for pktid = %x time=%lu\n",
				__func__, pnode->skb, pnode->pktid, pnode->pkt_jiffies);
			if (pnode->skb != NULL) {
				dev_consume_skb_any(pnode->skb);
			}
			list_del(head_node);
			pnode->in_use = OSI_DISABLE;
		}
	}
	raw_spin_unlock_irqrestore(&pdata->txts_lock, flags);
empty:
	raw_spin_lock_irqsave(&pdata->txts_lock, flags);
	idx = ether_get_free_tx_ts_node(pdata);
	if (idx == ETHER_MAX_PENDING_SKB_CNT) {
		dev_dbg(pdata->dev,
			"No free node to store pending SKB\n");
		dev_consume_skb_any(skb);
		raw_spin_unlock_irqrestore(&pdata->txts_lock, flags);
		return;
	}

	pdata->tx_ts_skb[idx].in_use = OSI_ENABLE;
	pnode = &pdata->tx_ts_skb[idx];
	pnode->skb = skb;
	pnode->pktid = pktid;
	pnode->pkt_jiffies = now_jiffies;

	dev_dbg(pdata->dev, "%s() SKB %p added for pktid = %x time=%lu\n",
		__func__, skb, pktid, pnode->pkt_jiffies);
	list_add_tail(&pnode->list_head,
		      &pdata->tx_ts_skb_head);
	raw_spin_unlock_irqrestore(&pdata->txts_lock, flags);
}

/**
 * @brief Adds delay in micro seconds.
 *
 * Algorithm: Invokes OSD delay function for adding delay
 *
 * @param[in] usec: Delay number in micro seconds.
 */
static void osd_udelay(unsigned long usec)
{
	udelay(usec);
}

/**
 * @brief Adds sleep in micro seconds
 *
 * Algorithm: Invokes OSD function to add sleep.
 *
 * @param[in] umin: Minimum sleep required in micro seconds.
 * @param[in] umax: Maximum sleep required in micro seconds.
 */
static void osd_usleep_range(unsigned long umin, unsigned long umax)
{
	usleep_range(umin, umax);
}

/**
 * @brief Adds sleep in milli seconds.
 *
 * Algorithm: Invokes OSD function to add sleep.
 *
 * @param[in] msec:  Minimum sleep required in milli seconds.
 */
static void osd_msleep(unsigned int msec)
{
	msleep(msec);
}

/**
 * @brief osd_log - OSD logging function
 *
 * @param[in] priv: OSD private data
 * @param[in] func: function name
 * @param[in] line: line number
 * @param[in] level: log level
 * @param[in] type: error type
 * @param[in] err:  error string
 * @param[in] loga: error additional information
 *
 */
static void osd_log(void *priv,
		    const char *func,
		    unsigned int line,
		    unsigned int level,
		    unsigned int type,
		    const char *err,
		    unsigned long long loga)
{
	if (priv) {
		switch (level) {
		case OSI_LOG_INFO:
			dev_info(((struct ether_priv_data *)priv)->dev,
				 "[%s][%d][type:0x%x][loga-0x%llx] %s",
				 func, line, type, loga, err);
			break;
		case OSI_LOG_WARN:
			dev_warn(((struct ether_priv_data *)priv)->dev,
				 "[%s][%d][type:0x%x][loga-0x%llx] %s",
				 func, line, type, loga, err);
			break;
		case OSI_LOG_ERR:
			dev_err(((struct ether_priv_data *)priv)->dev,
				"[%s][%d][type:0x%x][loga-0x%llx] %s",
				func, line, type, loga, err);
			break;
		default:
			break;
		}

	} else {
		switch (level) {
		case OSI_LOG_INFO:
			pr_info("[%s][%d][type:0x%x][loga-0x%llx] %s",
				func, line, type, loga, err);
			break;
		case OSI_LOG_WARN:
			pr_warn("[%s][%d][type:0x%x][loga-0x%llx] %s",
				func, line, type, loga, err);
			break;
		case OSI_LOG_ERR:
			pr_err("[%s][%d][type:0x%x][loga-0x%llx] %s",
			       func, line, type, loga, err);
			break;
		default:
			break;
		}
	}
}

/**
 * @brief Allocate and DMA map Rx buffer.
 *
 * Algorithm: Allocate network buffer (skb) and map skb->data to
 * DMA mappable address.
 *
 * @param[in] pdata: OSD private data structure.
 * @param[in] rx_swcx: Rx ring software context.
 * @param[in] dma_rx_buf_len: DMA Rx buffer length.
 *
 * @retval 0 on Sucess
 * @retval  ENOMEM on failure.
 */
static inline int ether_alloc_skb(struct ether_priv_data *pdata,
				  struct osi_rx_swcx *rx_swcx,
				  unsigned int dma_rx_buf_len,
				  unsigned int chan)
{
#ifndef ETHER_PAGE_POOL
	struct sk_buff *skb = NULL;
	dma_addr_t dma_addr;
#endif
	unsigned long val;

	if (((rx_swcx->flags & OSI_RX_SWCX_REUSE) == OSI_RX_SWCX_REUSE) &&
	    (rx_swcx->buf_virt_addr != pdata->osi_dma->resv_buf_virt_addr)) {
		/* Skip buffer allocation and DMA mapping since
		 * PTP software context will have valid buffer and
		 * DMA addresses so use them as is.
		 */
		rx_swcx->flags |= OSI_RX_SWCX_BUF_VALID;
		return 0;
	}

#ifndef ETHER_PAGE_POOL
	skb = netdev_alloc_skb_ip_align(pdata->ndev, dma_rx_buf_len);

	if (unlikely(skb == NULL)) {
		dev_err(pdata->dev, "RX skb allocation failed, using reserved buffer\n");
		rx_swcx->buf_virt_addr = pdata->osi_dma->resv_buf_virt_addr;
		rx_swcx->buf_phy_addr = pdata->osi_dma->resv_buf_phy_addr;
		rx_swcx->flags |= OSI_RX_SWCX_BUF_VALID;
		val = pdata->xstats.re_alloc_rxbuf_failed[chan];
		pdata->xstats.re_alloc_rxbuf_failed[chan] =
			osi_update_stats_counter(val, 1UL);
		return 0;
	}

	dma_addr = dma_map_single(pdata->dev, skb->data, dma_rx_buf_len,
				  DMA_FROM_DEVICE);
	if (unlikely(dma_mapping_error(pdata->dev, dma_addr) != 0)) {
		dev_err(pdata->dev, "RX skb dma map failed\n");
		dev_kfree_skb_any(skb);
		return -ENOMEM;
	}

#else
	rx_swcx->buf_virt_addr = page_pool_dev_alloc_pages(pdata->page_pool);
	if (!rx_swcx->buf_virt_addr) {
		dev_err(pdata->dev,
			"page pool allocation failed using resv_buf\n");
		rx_swcx->buf_virt_addr = pdata->osi_dma->resv_buf_virt_addr;
		rx_swcx->buf_phy_addr = pdata->osi_dma->resv_buf_phy_addr;
		rx_swcx->flags |= OSI_RX_SWCX_BUF_VALID;
		val = pdata->xstats.re_alloc_rxbuf_failed[chan];
		pdata->xstats.re_alloc_rxbuf_failed[chan] =
			osi_update_stats_counter(val, 1UL);
		return 0;
	}

	rx_swcx->buf_phy_addr = page_pool_get_dma_addr(rx_swcx->buf_virt_addr);
#endif
#ifndef ETHER_PAGE_POOL
	rx_swcx->buf_virt_addr = skb;
	rx_swcx->buf_phy_addr = dma_addr;
#endif
	rx_swcx->flags |= OSI_RX_SWCX_BUF_VALID;

	return 0;
}

/**
 * @brief Re-fill DMA channel Rx ring.
 *
 * Algorithm: Re-fill Rx DMA channel ring until dirty rx index is equal
 * to current rx index.
 * 1) Invokes OSD layer to allocate the buffer and map the buffer to DMA
 * mappable address.
 * 2) Fill Rx descriptors with required data.
 *
 * @param[in] pdata: OSD private data structure.
 * @param[in] rx_ring: DMA channel Rx ring instance.
 * @param[in] chan: DMA Rx channel number.
 */
static void ether_realloc_rx_skb(struct ether_priv_data *pdata,
				 struct osi_rx_ring *rx_ring,
				 unsigned int chan)
{
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	struct osi_rx_swcx *rx_swcx = NULL;
	struct osi_rx_desc *rx_desc = NULL;
	unsigned int local_refill_idx = rx_ring->refill_idx;
	int ret = 0;

	while (local_refill_idx != rx_ring->cur_rx_idx &&
	       local_refill_idx < osi_dma->rx_ring_sz) {
		rx_swcx = rx_ring->rx_swcx + local_refill_idx;
		rx_desc = rx_ring->rx_desc + local_refill_idx;

		ret = ether_alloc_skb(pdata, rx_swcx, osi_dma->rx_buf_len,
				      chan);
		if (ret < 0) {
			break;
		}
		INCR_RX_DESC_INDEX(local_refill_idx, osi_dma->rx_ring_sz);
	}

	ret = osi_rx_dma_desc_init(osi_dma, rx_ring, chan);
	if (ret < 0) {
		dev_err(pdata->dev, "Failed to refill Rx ring %u\n", chan);
	}
}

/**
 * @brief osd_realloc_buf - Allocate RX sk_buffer
 *
 * Algorithm: call ether_realloc_rx_skb for re-allocation
 *
 * @param[in] priv: OSD private data structure.
 * @param[in] rx_ring: Pointer to DMA channel Rx ring.
 * @param[in] chan: DMA Rx channel number.
 *
 */
static void osd_realloc_buf(void *priv, struct osi_rx_ring *rx_ring,
			    unsigned int chan)
{
	struct ether_priv_data *pdata = (struct ether_priv_data *)priv;

	ether_realloc_rx_skb(pdata, rx_ring, chan);
}

#ifdef ETHER_NVGRO
/**
 * @brief ether_gro_merge_complete - Merging the packets with GRO layer
 *
 * @param[in] nvgro_q: NVGRO packet sequence queue.
 * @param[in] napi: Driver NAPI instance.
 */
static inline void ether_gro_merge_complete(struct sk_buff_head *nvgro_q,
					    struct napi_struct *napi)
{
	struct list_head h;
	struct sk_buff *f_skb, *p, *pp;

	f_skb = __skb_peek(nvgro_q);

	INIT_LIST_HEAD(&h);

	skb_queue_walk_safe(nvgro_q, p, pp) {
		__skb_unlink(p, nvgro_q);

		NAPI_GRO_CB(p)->data_offset = 0;
		NAPI_GRO_CB(p)->frag0 = NULL;
		NAPI_GRO_CB(p)->frag0_len = 0;
		NAPI_GRO_CB(p)->same_flow = 1;
		NAPI_GRO_CB(p)->flush_id = 0;
		NAPI_GRO_CB(p)->count = 0;
		NAPI_GRO_CB(p)->flush = skb_is_gso(p);
		NAPI_GRO_CB(p)->free = 0;
		NAPI_GRO_CB(p)->encap_mark = 0;
		NAPI_GRO_CB(p)->recursion_counter = 0;
		NAPI_GRO_CB(p)->is_fou = 0;
		NAPI_GRO_CB(p)->is_atomic = 1;
		NAPI_GRO_CB(p)->gro_remcsum_start = 0;
		NAPI_GRO_CB(p)->csum_cnt = p->csum_level + 1;
		NAPI_GRO_CB(p)->csum_valid = 0;

		inet_gro_receive(&h, p);

		if (p == f_skb) {
			list_add(&p->list, &h);
			NAPI_GRO_CB(p)->age = jiffies;
			NAPI_GRO_CB(p)->last = p;
			skb_shinfo(p)->gso_size = skb_gro_len(p);
		}

		NAPI_GRO_CB(f_skb)->count++;
	}

	skb_list_del_init(f_skb);
	napi_gro_complete(napi, f_skb);
}

/**
 * @brief ether_update_fq_with_fs - Populates final queue with TTL = 1 packet
 *
 * @param[in] pdata: Ethernet driver private data
 * @param[in] skb: Socket buffer.
 */
static inline void ether_update_fq_with_fs(struct ether_priv_data *pdata,
					   struct sk_buff *skb)
{
	if (!skb_queue_empty(&pdata->fq)) {
		pdata->nvgro_dropped += pdata->fq.qlen;
		__skb_queue_purge(&pdata->fq);
	}

	/* queue skb to fq which has TTL = 1 */
	__skb_queue_tail(&pdata->fq, skb);

	pdata->expected_ip_id = NAPI_GRO_CB(skb)->flush_id + 1;
}

/**
 * @brief ether_get_skb_from_ip_id - Get SKB from MQ based on IPID.
 *
 * @param[in] mq: NVGRO packet out of order queue.
 * @param[in] ip_id: IPv4 packet ID.
 *
 * @retval skb on Success
 * @retval NULL on failure.
 */
static inline struct sk_buff *ether_get_skb_from_ip_id(struct sk_buff_head *mq,
						       u16 ip_id)
{
	struct sk_buff *p, *pp;

	skb_queue_walk_safe(mq, p, pp) {
		if ((NAPI_GRO_CB(p)->flush_id) == ip_id) {
			__skb_unlink(p, mq);
			return p;
		}
	}

	return NULL;
}

/**
 * @brief ether_gro - Perform NVGRO packet merging.
 *
 * @param[in] fq: NVGRO packets sequence queue.
 * @param[in] mq: NVGRO packet out of order queue.
 * @param[in] napi: Driver NAPI instance.
 */
static inline void ether_gro(struct sk_buff_head *fq, struct sk_buff_head *mq,
			     struct napi_struct *napi)
{
	struct sk_buff *f_skb, *p;
	u32 s_ip_id;

	if (skb_queue_empty(fq))
		return;

	f_skb = skb_peek_tail(fq);

	s_ip_id = NAPI_GRO_CB(f_skb)->flush_id;

	do {
		s_ip_id++;
		p = ether_get_skb_from_ip_id(mq, s_ip_id);
		if (!p)
			return;

		__skb_queue_tail(fq, p);

		if (NAPI_GRO_CB(p)->free == 2)
			break;
	} while (1);

	ether_gro_merge_complete(fq, napi);
}

/**
 * @brief ether_purge_q - Purge master queue based on packet age.
 *
 * @param[in] pdata: Ethernet private data.
 */
static inline void ether_purge_q(struct ether_priv_data *pdata)
{
	struct sk_buff *p, *pp;

	skb_queue_walk_safe(&pdata->mq, p, pp) {
		if ((jiffies - NAPI_GRO_CB(p)->age) >
		    msecs_to_jiffies(pdata->pkt_age_msec)) {
			__skb_unlink(p, &pdata->mq);
			dev_consume_skb_any(p);
			pdata->nvgro_dropped++;
		} else {
			return;
		}
	}
}

/**
 * @brief ether_nvgro_purge_timer - NVGRO purge timer handler.
 *
 * @param[in] t: Pointer to the timer.
 */
void ether_nvgro_purge_timer(struct timer_list *t)
{
	struct ether_priv_data *pdata = from_timer(pdata, t, nvgro_timer);
	struct sk_buff *f_skb;

	if (atomic_read(&pdata->rx_state) == OSI_ENABLE)
		return;

	atomic_set(&pdata->timer_state, OSI_ENABLE);

	ether_purge_q(pdata);

	f_skb = skb_peek(&pdata->fq);
	if (!f_skb)
		goto exit;

	if ((jiffies - NAPI_GRO_CB(f_skb)->age) >
	    msecs_to_jiffies(pdata->pkt_age_msec)) {
		pdata->nvgro_dropped += pdata->fq.qlen;
		__skb_queue_purge(&pdata->fq);
	}

exit:
	atomic_set(&pdata->timer_state, OSI_DISABLE);

	mod_timer(&pdata->nvgro_timer,
		  jiffies + msecs_to_jiffies(pdata->nvgro_timer_intrvl));
}

/**
 * @brief ether_do_nvgro - Perform NVGRO processing.
 *
 * @param[in] pdata: Ethernet private data.
 * @param[in] napi: Ethernet driver NAPI instance.
 * @param[in] skb: socket buffer
 *
 * @retval true on Success
 * @retval false on failure.
 */
static bool ether_do_nvgro(struct ether_priv_data *pdata,
			   struct napi_struct *napi,
			   struct sk_buff *skb)
{
	struct udphdr *uh = (struct udphdr *)(skb->data + sizeof(struct iphdr));
	struct iphdr *iph = (struct iphdr *)skb->data;
	struct sk_buff_head *mq = &pdata->mq;
	struct ethhdr *ethh = eth_hdr(skb);
	struct sock *sk = NULL;

	if (ethh->h_proto != htons(ETH_P_IP))
		return false;

	if (iph->protocol != IPPROTO_UDP)
		return false;

	/* TODO: Hash based based queues selection */
	/* Socket look up with IPv4/UDP source/destination */
	sk = __udp4_lib_lookup(dev_net(skb->dev), iph->saddr, uh->source,
			       iph->daddr, uh->dest, inet_iif(skb),
			       inet_sdif(skb), &udp_table, NULL);
	if (!sk)
		return false;

	/* Socket found but GRO not enabled on the socket - We don't care */
	if (!udp_sk(sk)->gro_enabled)
		return false;

	/* Store IPID, TTL and age of skb inside per skb control block */
	NAPI_GRO_CB(skb)->flush_id = ntohs(iph->id);
	NAPI_GRO_CB(skb)->free = (iph->ttl & (BIT(6) | BIT(7))) >> 6;
	NAPI_GRO_CB(skb)->age = jiffies;

	while (atomic_read(&pdata->timer_state) == OSI_ENABLE) {
		/* busyloop */
	};

	atomic_set(&pdata->rx_state, OSI_ENABLE);

	if (NAPI_GRO_CB(skb)->free == 1) {
		/* Update final queue with first segment */
		ether_update_fq_with_fs(pdata, skb);
		goto exit;
	} else {
		if (pdata->expected_ip_id == NAPI_GRO_CB(skb)->flush_id) {
			__skb_queue_tail(&pdata->fq, skb);
			pdata->expected_ip_id = NAPI_GRO_CB(skb)->flush_id + 1;

			if (NAPI_GRO_CB(skb)->free == 2)
				ether_gro_merge_complete(&pdata->fq, napi);

			goto exit;
		}
	}

	/* Add skb to the queue */
	__skb_queue_tail(mq, skb);

	/* Queue the packets until last segment received */
	if (NAPI_GRO_CB(skb)->free != 2)
		goto exit;

	ether_gro(&pdata->fq, &pdata->mq, napi);

exit:
	atomic_set(&pdata->rx_state, OSI_DISABLE);
	return true;
}
#endif

/**
 * @brief Handover received packet to network stack.
 *
 * Algorithm:
 * 1) Unmap the DMA buffer address.
 * 2) Updates socket buffer with len and ether type and handover to
 * Linux network stack.
 * 3) Refill the Rx ring based on threshold.
 *
 * @param[in] priv: OSD private data structure.
 * @param[in] rx_ring: Pointer to DMA channel Rx ring.
 * @param[in] chan: DMA Rx channel number.
 * @param[in] dma_buf_len: Rx DMA buffer length.
 * @param[in] rx_pkt_cx: Received packet context.
 * @param[in] rx_swcx: Received packet sw context.
 *
 * @note Rx completion need to make sure that Rx descriptors processed properly.
 */
static void osd_receive_packet(void *priv, struct osi_rx_ring *rx_ring,
			       unsigned int chan, unsigned int dma_buf_len,
			       const struct osi_rx_pkt_cx *rx_pkt_cx,
			       struct osi_rx_swcx *rx_swcx)
{
	struct ether_priv_data *pdata = (struct ether_priv_data *)priv;
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct ether_rx_napi *rx_napi = pdata->rx_napi[chan];
#ifdef ETHER_PAGE_POOL
	struct page *page = (struct page *)rx_swcx->buf_virt_addr;
	struct sk_buff *skb = NULL;
#else
	struct sk_buff *skb = (struct sk_buff *)rx_swcx->buf_virt_addr;
#endif
	dma_addr_t dma_addr = (dma_addr_t)rx_swcx->buf_phy_addr;
	struct net_device *ndev = pdata->ndev;
	struct osi_pkt_err_stats *pkt_err_stat = &pdata->osi_dma->pkt_err_stats;
	struct skb_shared_hwtstamps *shhwtstamp;
	unsigned long val;

#ifndef ETHER_PAGE_POOL
	dma_unmap_single(pdata->dev, dma_addr, dma_buf_len, DMA_FROM_DEVICE);
#endif
	/* Process only the Valid packets */
	if (likely((rx_pkt_cx->flags & OSI_PKT_CX_VALID) ==
		   OSI_PKT_CX_VALID)) {
#ifdef ETHER_PAGE_POOL
		skb = netdev_alloc_skb_ip_align(pdata->ndev,
						rx_pkt_cx->pkt_len);
		if (unlikely(!skb)) {
			pdata->ndev->stats.rx_dropped++;
			dev_err(pdata->dev,
				"%s(): Error in allocating the skb\n",
			        __func__);
			page_pool_recycle_direct(pdata->page_pool, page);
			return;
		}

		dma_sync_single_for_cpu(pdata->dev, dma_addr,
					rx_pkt_cx->pkt_len, DMA_FROM_DEVICE);
		skb_copy_to_linear_data(skb, page_address(page),
					rx_pkt_cx->pkt_len);
		skb_put(skb, rx_pkt_cx->pkt_len);
		page_pool_recycle_direct(pdata->page_pool, page);
#else
		skb_put(skb, rx_pkt_cx->pkt_len);
#endif
		if (likely((rx_pkt_cx->rxcsum & OSI_CHECKSUM_UNNECESSARY) ==
			 OSI_CHECKSUM_UNNECESSARY)) {
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		} else {
			skb->ip_summed = CHECKSUM_NONE;
		}

		if ((rx_pkt_cx->flags & OSI_PKT_CX_RSS) == OSI_PKT_CX_RSS) {
			skb_set_hash(skb, rx_pkt_cx->rx_hash,
				     rx_pkt_cx->rx_hash_type);
		}

		if ((rx_pkt_cx->flags & OSI_PKT_CX_VLAN) == OSI_PKT_CX_VLAN) {
			val = pdata->osi_dma->dstats.rx_vlan_pkt_n;
			pdata->osi_dma->dstats.rx_vlan_pkt_n =
				osi_update_stats_counter(val, 1UL);
			__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q),
					       rx_pkt_cx->vlan_tag);
		}

		/* Handle time stamp */
		if ((rx_pkt_cx->flags & OSI_PKT_CX_PTP) == OSI_PKT_CX_PTP) {
			shhwtstamp = skb_hwtstamps(skb);
			memset(shhwtstamp, 0,
			       sizeof(struct skb_shared_hwtstamps));
			shhwtstamp->hwtstamp = ns_to_ktime(rx_pkt_cx->ns);
		}

		skb_record_rx_queue(skb, chan);
		skb->dev = ndev;
		skb->protocol = eth_type_trans(skb, ndev);
		ndev->stats.rx_bytes += skb->len;
#ifdef ETHER_NVGRO
		if ((ndev->features & NETIF_F_GRO) &&
		    ether_do_nvgro(pdata, &rx_napi->napi, skb))
			goto done;
#endif
		if (likely(ndev->features & NETIF_F_GRO)) {
			napi_gro_receive(&rx_napi->napi, skb);
		} else {
			netif_receive_skb(skb);
		}
	} else {
		ndev->stats.rx_crc_errors = pkt_err_stat->rx_crc_error;
		ndev->stats.rx_frame_errors = pkt_err_stat->rx_frame_error;
		ndev->stats.rx_fifo_errors = osi_core->mmc.mmc_rx_fifo_overflow;
		ndev->stats.rx_errors++;
#ifdef ETHER_PAGE_POOL
		page_pool_recycle_direct(pdata->page_pool, page);
#endif
		dev_kfree_skb_any(skb);
	}

#ifdef ETHER_NVGRO
done:
#endif
	ndev->stats.rx_packets++;
	rx_swcx->buf_virt_addr = NULL;
	rx_swcx->buf_phy_addr = 0;
	/* mark packet is processed */
	rx_swcx->flags |= OSI_RX_SWCX_PROCESSED;

	if (osi_get_refill_rx_desc_cnt(pdata->osi_dma, chan) >= 16U)
		ether_realloc_rx_skb(pdata, rx_ring, chan);
}

/**
 * @brief osd_transmit_complete - Transmit completion routine.
 *
 * Algorithm:
 * 1) Updates stats for linux network stack.
 * 2) unmap and free the buffer DMA address and buffer.
 * 3) Time stamp will be update to stack if available.
 *
 * @param[in] priv: OSD private data structure.
 * @param[in] swcx: Pointer to swcx
 * @param[in] txdone_pkt_cx: Pointer to struct which has tx done status info.
 * This struct has flags to indicate tx error, whether DMA address
 * is mapped from paged/linear buffer.
 *
 * @note Tx completion need to make sure that Tx descriptors processed properly.
 */
static void osd_transmit_complete(void *priv, const struct osi_tx_swcx *swcx,
				  const struct osi_txdone_pkt_cx
				  *txdone_pkt_cx)
{
	struct ether_priv_data *pdata = (struct ether_priv_data *)priv;
	struct osi_dma_priv_data *osi_dma = pdata->osi_dma;
	struct sk_buff *skb = (struct sk_buff *)swcx->buf_virt_addr;
	unsigned long dmaaddr = swcx->buf_phy_addr;
	struct skb_shared_hwtstamps shhwtstamp;
	struct net_device *ndev = pdata->ndev;
	struct osi_tx_ring *tx_ring;
	struct netdev_queue *txq;
	unsigned int chan, qinx;
	unsigned int len = swcx->len;

	ndev->stats.tx_bytes += len;

	if ((txdone_pkt_cx->flags & OSI_TXDONE_CX_TS) == OSI_TXDONE_CX_TS) {
		memset(&shhwtstamp, 0, sizeof(struct skb_shared_hwtstamps));
		shhwtstamp.hwtstamp = ns_to_ktime(txdone_pkt_cx->ns);
		/* pass tstamp to stack */
		skb_tstamp_tx(skb, &shhwtstamp);
	}

	if (dmaaddr != 0UL) {
		if ((txdone_pkt_cx->flags & OSI_TXDONE_CX_PAGED_BUF) ==
		    OSI_TXDONE_CX_PAGED_BUF) {
			dma_unmap_page(pdata->dev, dmaaddr,
				       len, DMA_TO_DEVICE);
		} else {
			dma_unmap_single(pdata->dev, dmaaddr,
					 len, DMA_TO_DEVICE);
		}
	}

	if (skb) {
		/* index in array, netdev_get_tx_queue use index to get
		 * network queue.
		 */
		qinx = skb_get_queue_mapping(skb);
		chan = osi_dma->dma_chans[qinx];
		tx_ring = osi_dma->tx_ring[chan];
		txq = netdev_get_tx_queue(ndev, qinx);

		if (netif_tx_queue_stopped(txq) &&
		    (ether_avail_txdesc_cnt(osi_dma, tx_ring) >
		    ETHER_TX_DESC_THRESHOLD)) {
			netif_tx_wake_queue(txq);
			netdev_dbg(ndev, "Tx ring[%d] - waking Txq\n", chan);
		}

		ndev->stats.tx_packets++;
		if ((txdone_pkt_cx->flags & OSI_TXDONE_CX_TS_DELAYED) ==
		    OSI_TXDONE_CX_TS_DELAYED) {
			add_skb_node(pdata, skb, txdone_pkt_cx->pktid);
			/* Consume the timestamp immediately if already available */
			if (ether_get_tx_ts(pdata) < 0)
				schedule_delayed_work(&pdata->tx_ts_work,
						      msecs_to_jiffies(ETHER_TS_MS_TIMER));

		} else {
			dev_consume_skb_any(skb);
		}
	}

}

#ifdef OSI_DEBUG
/**
 * @brief Dumps the data to trace buffer
 *
 * @param[in] osi_dma: OSI DMA private data.
 * @param[in] type: Type of data to be dump.
 * @param[in] fmt: Data format.
 */
static void osd_printf(struct osi_dma_priv_data *osi_dma,
		       unsigned int type,
		       const char *fmt, ...)
{
	char buf[512];
	va_list args;

	va_start(args, fmt);
	vsprintf(buf, fmt, args);

	switch (type) {
	case OSI_DEBUG_TYPE_DESC:
#if 0
		/**
		 * TODO: trace_printk resulted in kernel warning GVS failure.
		 * Add support for writing to a file
		 */
		trace_printk("%s", buf);
#endif
		pr_err("%s", buf);
		break;
	case OSI_DEBUG_TYPE_REG:
	case OSI_DEBUG_TYPE_STRUCTS:
		pr_err("%s", buf);
		break;
	default:
		pr_err("Unsupported debug type\n");
		break;
	}
	va_end(args);
}

/**
 * @brief Dumps the data
 *
 * @param[in] osi_core: OSI core private data.
 * @param[in] type: Type of data to be dump.
 * @param[in] fmt: Data format.
 */
static void osd_core_printf(struct osi_core_priv_data *osi_core,
			    unsigned int type,
			    const char *fmt, ...)
{
	char buf[512];
	va_list args;

	va_start(args, fmt);
	vsprintf(buf, fmt, args);

	switch (type) {
	case OSI_DEBUG_TYPE_REG:
	case OSI_DEBUG_TYPE_STRUCTS:
		pr_err("%s", buf);
		break;
	default:
		pr_err("Unsupported debug type\n");
		break;
	}
	va_end(args);
}
#endif

void ether_restart_lane_bringup_task(struct tasklet_struct *t)
{
	struct ether_priv_data *pdata = from_tasklet(pdata, t, lane_restart_task);

	if (pdata->tx_start_stop == OSI_DISABLE) {
		netif_tx_lock(pdata->ndev);
		netif_carrier_off(pdata->ndev);
		netif_tx_stop_all_queues(pdata->ndev);
		netif_tx_unlock(pdata->ndev);
		schedule_delayed_work(&pdata->set_speed_work, msecs_to_jiffies(500));
		if (netif_msg_drv(pdata)) {
			netdev_info(pdata->ndev, "Disable network Tx Queue\n");
		}
	} else if (pdata->tx_start_stop == OSI_ENABLE) {
		netif_tx_lock(pdata->ndev);
		netif_tx_start_all_queues(pdata->ndev);
		netif_tx_unlock(pdata->ndev);
		if (netif_msg_drv(pdata)) {
			netdev_info(pdata->ndev, "Enable network Tx Queue\n");
		}
	}
}

static void osd_restart_lane_bringup(void *priv, unsigned int en_disable)
{
	struct ether_priv_data *pdata = (struct ether_priv_data *)priv;

	pdata->tx_start_stop = en_disable;
	tasklet_hi_schedule(&pdata->lane_restart_task);
}

void ether_assign_osd_ops(struct osi_core_priv_data *osi_core,
			  struct osi_dma_priv_data *osi_dma)
{
	osi_core->osd_ops.ops_log = osd_log;
	osi_core->osd_ops.udelay = osd_udelay;
	osi_core->osd_ops.usleep_range = osd_usleep_range;
	osi_core->osd_ops.msleep = osd_msleep;
	osi_core->osd_ops.padctrl_mii_rx_pins = ether_padctrl_mii_rx_pins;
#ifdef OSI_DEBUG
	osi_core->osd_ops.printf = osd_core_printf;
#endif
	osi_core->osd_ops.restart_lane_bringup = osd_restart_lane_bringup;
	osi_dma->osd_ops.transmit_complete = osd_transmit_complete;
	osi_dma->osd_ops.receive_packet = osd_receive_packet;
	osi_dma->osd_ops.realloc_buf = osd_realloc_buf;
	osi_dma->osd_ops.ops_log = osd_log;
	osi_dma->osd_ops.udelay = osd_udelay;
#ifdef OSI_DEBUG
	osi_dma->osd_ops.printf = osd_printf;
#endif
}

/**
 * @brief osd_send_cmd - OSD ivc send cmd
 *
 * @param[in] priv: OSD private data
 * @param[in] ivc_buf: ivc_msg_common structure
 * @param[in] len: length of data
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 */
int osd_ivc_send_cmd(void *priv, ivc_msg_common_t *ivc_buf, unsigned int len)
{
	int ret = -1;
	static int cnt  = 0;
	struct osi_core_priv_data *core = (struct osi_core_priv_data *)priv;
	struct ether_priv_data *pdata = (struct ether_priv_data *)core->osd;
	struct ether_ivc_ctxt *ictxt = &pdata->ictxt;
	struct tegra_hv_ivc_cookie *ivck =
				  (struct tegra_hv_ivc_cookie *) ictxt->ivck;
	int status = -1;
	unsigned long flags = 0;

	if (len > ETHER_MAX_IVC_BUF) {
		dev_err(pdata->dev, "Invalid IVC len\n");
		return -1;
	}
	ivc_buf->status = -1;
	ivc_buf->count = cnt++;

	raw_spin_lock_irqsave(&ictxt->ivck_lock, flags);

	/* Waiting for the channel to be ready */
	ret = readx_poll_timeout_atomic(tegra_hv_ivc_channel_notified, ivck,
					status, status == 0, 10, IVC_WAIT_TIMEOUT_CNT);
	if (ret == -ETIMEDOUT) {
		dev_err(pdata->dev, "IVC channel timeout\n");
		goto fail;
	}

	/* Write the current message for the ethernet server */
	ret = tegra_hv_ivc_write(ivck, ivc_buf, len);
	if (ret != len) {
		dev_err(pdata->dev, "IVC write with len %d ret %d cmd %d ioctlcmd %d failed\n",
			len, ret, ivc_buf->cmd, ivc_buf->data.ioctl_data.cmd);
		goto fail;
	}

	ret = readx_poll_timeout_atomic(tegra_hv_ivc_can_read, ictxt->ivck,
					status, status, 10, IVC_WAIT_TIMEOUT_CNT);
	if (ret == -ETIMEDOUT) {
		dev_err(pdata->dev, "IVC read timeout status %d\n", status);
		goto fail;
	}

	ret = tegra_hv_ivc_read(ivck, ivc_buf, len);
	if (ret < 0) {
		dev_err(pdata->dev, "IVC read failed: %d cmd %d ioctlcmd %d\n",
			ret, ivc_buf->cmd, ivc_buf->data.ioctl_data.cmd);
	}
	ret = ivc_buf->status;
fail:
	raw_spin_unlock_irqrestore(&ictxt->ivck_lock, flags);
	return ret;
}

int ether_padctrl_mii_rx_pins(void *priv, unsigned int enable)
{
	struct ether_priv_data *pdata = (struct ether_priv_data *)priv;
	int ret = 0;

	if (enable == OSI_ENABLE) {
		ret = pinctrl_select_state(pdata->pin,
					   pdata->mii_rx_enable_state);
		if (ret < 0) {
			dev_err(pdata->dev, "pinctrl enable state failed %d\n",
				ret);
		}
	} else if (enable == OSI_DISABLE) {
		ret = pinctrl_select_state(pdata->pin,
					   pdata->mii_rx_disable_state);
		if (ret < 0) {
			dev_err(pdata->dev, "pinctrl disable state failed %d\n",
				ret);
		}
	}
	return ret;
}
