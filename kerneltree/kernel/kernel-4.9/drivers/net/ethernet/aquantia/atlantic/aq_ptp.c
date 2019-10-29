/*
 * Aquantia Corporation Network Driver
 * Copyright (C) 2014-2016 Aquantia Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*
 * File aq_ptp.c:
 * Definition of functions for Linux PTP support.
 */

#include "aq_nic.h"
#include "aq_hw.h"
#include "aq_ptp.h"
#include "aq_ring.h"
#include "aq_nic.h"

#include <linux/clocksource.h>

//#define ATLANTIC_PTP_DEBUG

#ifdef ATLANTIC_PTP_DEBUG
#define __pr_info(...)   pr_info(__VA_ARGS__)
#else
#define __pr_info(...)
#endif

struct skb_ring {
	struct sk_buff **buff;
	spinlock_t lock;
	unsigned int size;
	unsigned int head;
	unsigned int tail;
};

struct aq_ptp_s {
	struct aq_nic_s *aq_nic;
	
	struct hwtstamp_config hwtstamp_config;
	
	spinlock_t ptp_lock;
	struct ptp_clock *ptp_clock;
	struct ptp_clock_info ptp_info;
	struct cyclecounter cc;
	struct timecounter tc;

  atomic_t offset_egress;
  atomic_t offset_ingress;
	
	struct aq_ring_param_s ptp_ring_param;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 7, 0)
	unsigned int num_vector;
#endif
	unsigned int idx_vector;
	struct napi_struct napi;
	
	struct aq_ring_s ptp_tx;
	struct aq_ring_s ptp_rx;
	struct aq_ring_s hwts_rx;

	struct skb_ring skb_ring;
};

struct ptp_tm_offset {
	unsigned int mbps;
	int egress;
	int ingress;
};

static const struct ptp_tm_offset ptp_offset[] = {
	{ 0, 0, 0 },
	{ 100, 5150, 5050 },      // 100M
	{ 1000, 1100, 1000 },     // 1G
	{ 2500, 1788, 3326 },     // 2,5G
	{ 5000, 1241, 1727 },     // 5G
	{ 10000, 732, 1607 }     // 10G
};

static inline int aq_ptp_tm_offset_egress_get(struct aq_ptp_s *self)
{
  return atomic_read(&self->offset_egress);
}

static inline int aq_ptp_tm_offset_ingress_get(struct aq_ptp_s *self)
{
  return atomic_read(&self->offset_ingress);
}

void aq_ptp_tm_offset_set(struct aq_nic_s *aq_nic, unsigned int mbps)
{
	struct aq_ptp_s *self = aq_nic->aq_ptp;
	int i, egress, ingress;

	egress = ingress = 0;

	for (i = 0; i < ARRAY_SIZE(ptp_offset); i++) {
		if (mbps == ptp_offset[i].mbps) {
			egress = ptp_offset[i].egress;
			ingress = ptp_offset[i].ingress;
			break;
		}
	}
	
	atomic_set(&self->offset_egress, egress);
	atomic_set(&self->offset_ingress, ingress);

	pr_info("aq_ptp_tm_offset_set: egress: %d; ingress: %d;\n",
			atomic_read(&self->offset_egress), atomic_read(&self->offset_ingress));
}

static inline unsigned int __aq_ptp_skb_ring_next_idx(struct skb_ring *ring, unsigned int idx)
{
	return (++idx >= ring->size) ? 0 : idx;
}

static int __aq_ptp_skb_put(struct skb_ring *ring, struct sk_buff *skb)
{
	unsigned int next_head = __aq_ptp_skb_ring_next_idx(ring, ring->head);
	
	if (next_head == ring->tail)
		return -1;

	ring->buff[ring->head] = skb_get(skb);
	ring->head = next_head;
	
	return 0;
}

static void aq_ptp_skb_put(struct skb_ring *ring, struct sk_buff *skb)
{
	unsigned long flags;
	int ret;
	
	spin_lock_irqsave(&ring->lock, flags);
	ret = __aq_ptp_skb_put(ring, skb);
	spin_unlock_irqrestore(&ring->lock, flags);
	
	if (ret)
		__pr_info("SKB Ring is overflow (%u)!\n", ring->size);
}

static struct sk_buff *__aq_ptp_skb_get(struct skb_ring *ring)
{
	struct sk_buff *skb;
	
	if (ring->tail == ring->head)
		return NULL;
	
	skb = ring->buff[ring->tail];
	ring->tail = __aq_ptp_skb_ring_next_idx(ring, ring->tail);

	return skb;
}

static struct sk_buff *aq_ptp_skb_get(struct skb_ring *ring)
{
	unsigned long flags;
	struct sk_buff *skb;
	
	spin_lock_irqsave(&ring->lock, flags);
	skb = __aq_ptp_skb_get(ring);
	spin_unlock_irqrestore(&ring->lock, flags);

	return skb;
}

#ifdef ATLANTIC_PTP_DEBUG
static unsigned int aq_ptp_skb_buf_len(struct skb_ring *ring)
{
  unsigned long flags;
  unsigned int len;

  spin_lock_irqsave(&ring->lock, flags);
  len = (ring->head >= ring->tail) ?
    ring->head - ring->tail :
    ring->size - ring->tail + ring->head;
  spin_unlock_irqrestore(&ring->lock, flags);

  return len;
}
#endif

static int aq_ptp_skb_ring_init(struct skb_ring *ring, unsigned int size)
{
	struct sk_buff **buff = kmalloc(sizeof(*buff) * size, GFP_KERNEL);
	if (!buff) {
		return -1;
	}
	
	spin_lock_init(&ring->lock);
	
	ring->buff = buff;
	ring->size = size;
	ring->head = ring->tail = 0;
	
	return 0;
}

static void aq_ptp_skb_ring_clean(struct skb_ring *ring)
{
	struct sk_buff *skb;
	while ((skb = aq_ptp_skb_get(ring)) != NULL)
		dev_kfree_skb_any(skb);
}

static void aq_ptp_skb_ring_release(struct skb_ring *ring)
{
	if (ring->buff) {
		aq_ptp_skb_ring_clean(ring);
		kfree(ring->buff);
		ring->buff = NULL;
	}
}

 /**
 * aq_ptp_read - read raw cycle counter (to be used by time counter)
 * @cc: the cyclecounter structure
 *
 * this function reads the cyclecounter registers and is called by the
 * cyclecounter structure used to construct a ns counter from the
 * arbitrary fixed point registers
 */
static u64 aq_ptp_read(const struct cyclecounter *cc)
{
	struct aq_ptp_s *self = container_of(cc, struct aq_ptp_s, cc);
	struct aq_nic_s *aq_nic = self->aq_nic;
	u64 stamp;

	down(&aq_nic->fwreq_sem);
	aq_nic->aq_hw_ops->hw_get_ptp_ts(aq_nic->aq_hw, &stamp);
	up(&aq_nic->fwreq_sem);

	return stamp;
}

 /**
 * aq_ptp_adjfreq
 * @ptp: the ptp clock structure
 * @ppb: parts per billion adjustment from base
 *
 * adjust the frequency of the ptp cycle counter by the
 * indicated ppb from the base frequency.
 */
static int aq_ptp_adjfreq(struct ptp_clock_info *ptp, s32 ppb)
{
	struct aq_ptp_s *self = container_of(ptp, struct aq_ptp_s, ptp_info);
	struct aq_nic_s *aq_nic = self->aq_nic;

  __pr_info("aq_ptp_adjfreq: ppb: %d\n", ppb);
	
	down(&aq_nic->fwreq_sem);
	aq_nic->aq_hw_ops->hw_adj_sys_clock(aq_nic->aq_hw, ppb);
	up(&aq_nic->fwreq_sem);

	return 0;
}

/**
 * aq_ptp_adjtime
 * @ptp: the ptp clock structure
 * @delta: offset to adjust the cycle counter by
 *
 * adjust the timer by resetting the timecounter structure.
 */
static int aq_ptp_adjtime(struct ptp_clock_info *ptp, s64 delta)
{
	struct aq_ptp_s *self = container_of(ptp, struct aq_ptp_s, ptp_info);
	unsigned long flags;

  __pr_info("aq_ptp_adjtime: delta: %lld\n", delta);
	
	spin_lock_irqsave(&self->ptp_lock, flags);
	timecounter_adjtime(&self->tc, delta);
	spin_unlock_irqrestore(&self->ptp_lock, flags);
	
	return 0;
}

/**
 * aq_ptp_gettime
 * @ptp: the ptp clock structure
 * @ts: timespec structure to hold the current time value
 *
 * read the timecounter and return the correct value on ns,
 * after converting it into a struct timespec.
 */
static int aq_ptp_gettime(struct ptp_clock_info *ptp, struct timespec64 *ts)
{
	struct aq_ptp_s *self = container_of(ptp, struct aq_ptp_s, ptp_info);
	unsigned long flags;
	u64 ns;
	
	spin_lock_irqsave(&self->ptp_lock, flags);
	ns = timecounter_read(&self->tc);
	spin_unlock_irqrestore(&self->ptp_lock, flags);
	
	*ts = ns_to_timespec64(ns);

  __pr_info("aq_ptp_gettime: ns: %llu\n", ns);

	return 0;
}

/**
 * aq_ptp_settime
 * @ptp: the ptp clock structure
 * @ts: the timespec containing the new time for the cycle counter
 *
 * reset the timecounter to use a new base value instead of the kernel
 * wall timer value.
 */
static int aq_ptp_settime(struct ptp_clock_info *ptp,
				 const struct timespec64 *ts)
{
	struct aq_ptp_s *self = container_of(ptp, struct aq_ptp_s, ptp_info);
	unsigned long flags;
	u64 ns;
	
	ns = timespec64_to_ns(ts);
	
	/* reset the timecounter */
	spin_lock_irqsave(&self->ptp_lock, flags);
	timecounter_init(&self->tc, &self->cc, ns);
	spin_unlock_irqrestore(&self->ptp_lock, flags);

  __pr_info("aq_ptp_settime: ns: %llu\n", ns);
	
	return 0;
}

static void aq_ptp_convert_to_hwtstamp(struct aq_ptp_s *self, struct skb_shared_hwtstamps *hwtstamp, u64 timestamp)
{
	unsigned long flags;
	u64 ns;
	
	spin_lock_irqsave(&self->ptp_lock, flags);
	ns = timecounter_cyc2time(&self->tc, timestamp);
	spin_unlock_irqrestore(&self->ptp_lock, flags);

	memset(hwtstamp, 0, sizeof(*hwtstamp));	
	hwtstamp->hwtstamp = ns_to_ktime(ns);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0)	
	__pr_info("hwtstamp: %lld\n", hwtstamp->hwtstamp.tv64);
#else
	__pr_info("hwtstamp: %lld\n", hwtstamp->hwtstamp);
#endif
}

/**
 * aq_ptp_feature_enable
 * @ptp: the ptp clock structure
 * @rq: the requested feature to change
 * @on: whether to enable or disable the feature
 */
static int aq_ptp_feature_enable(struct ptp_clock_info *ptp,
					struct ptp_clock_request *rq, int on)
{
	struct ptp_clock_time *t = &rq->perout.period;
	struct aq_ptp_s *self = container_of(ptp, struct aq_ptp_s, ptp_info);
	struct aq_nic_s *aq_nic = self->aq_nic;
	u64 period;
	
	/* we can only support periodic output */
	if (rq->type != PTP_CLK_REQ_PEROUT)
		return -ENOTSUPP;
	
	/* verify the request channel is there */
	if (rq->perout.index >= ptp->n_per_out)
		return -EINVAL;
	
	/*
	 * We cannot enforce start time as there is no
	 * mechanism for that in the hardware, we can only control
	 * the period.
	 */
	
	/* we cannot support periods greater than 4 seconds due to reg limit */
	if (t->sec > 4 || t->sec < 0)
		return -ERANGE;
	
	/* convert to unsigned 64b ns, verify we can put it in a 32b register */
	period = on ? t->sec * 1000000000LL + t->nsec : 0;
	
	/* verify the value is in range supported by hardware */
	if (period > U32_MAX)
		return -ERANGE;
	
	/*
	 * Notify hardware of request to being sending pulses.
	 * If period is ZERO then pulsen is disabled.
	 */
	down(&aq_nic->fwreq_sem);
	aq_nic->aq_hw_ops->hw_gpio_pulse(aq_nic->aq_hw, rq->perout.index, (u32)period);
	up(&aq_nic->fwreq_sem);
	
	return 0;
}

/**
 * aq_ptp_verify
 * @ptp: the ptp clock structure
 * @pin: index of the pin in question
 * @func: the desired function to use
 * @chan: the function channel index to use
 */
 static int aq_ptp_verify(struct ptp_clock_info *ptp, unsigned int pin,
	enum ptp_pin_function func, unsigned int chan)
{
	/* verify the requested pin is there */
	if (!ptp->pin_config || pin >= ptp->n_pins)
		return -EINVAL;

	/* enforce locked channels, no changing them */
	if (chan != ptp->pin_config[pin].chan)
		return -EINVAL;

	/* we want to keep the functions locked as well */
	if (func != ptp->pin_config[pin].func)
		return -EINVAL;

	return 0;
}

/**
 * aq_ptp_tx_hwtstamp - utility function which checks for TX time stamp
 * @adapter: the private adapter struct
 *
 * if the timestamp is valid, we convert it into the timecounter ns
 * value, then store that result into the shhwtstamps structure which
 * is passed up the network stack
 */
void aq_ptp_tx_hwtstamp(struct aq_nic_s *aq_nic, u64 timestamp)
{
	struct aq_ptp_s *self = aq_nic->aq_ptp;
	struct sk_buff *skb = aq_ptp_skb_get(&self->skb_ring);
	struct skb_shared_hwtstamps hwtstamp;

	if (!skb) {
		pr_info("have timestamp but tx_queus empty\n");
		return;
	}

	__pr_info("tx ts (%u): %llu;\n",
			aq_ptp_skb_buf_len(&self->skb_ring), timestamp);

	timestamp += aq_ptp_tm_offset_egress_get(self);
	aq_ptp_convert_to_hwtstamp(self, &hwtstamp, timestamp);
	skb_tstamp_tx(skb, &hwtstamp);
	dev_kfree_skb_any(skb);
}

/**
 * aq_ptp_rx_hwtstamp - utility function which checks for RX time stamp
 * @adapter: pointer to adapter struct
 * @skb: particular skb to send timestamp with
 *
 * if the timestamp is valid, we convert it into the timecounter ns
 * value, then store that result into the shhwtstamps structure which
 * is passed up the network stack
 */
static void aq_ptp_rx_hwtstamp(struct aq_ptp_s *self, struct sk_buff *skb, u64 timestamp)
{
	__pr_info("rx ts: %llu;\n", timestamp);

	timestamp -= aq_ptp_tm_offset_ingress_get(self);
	aq_ptp_convert_to_hwtstamp(self, skb_hwtstamps(skb), timestamp);
}

void aq_ptp_hwtstamp_config_get(struct aq_nic_s *aq_nic, struct hwtstamp_config *config)
{
	*config = aq_nic->aq_ptp->hwtstamp_config;
}

void aq_ptp_hwtstamp_config_set(struct aq_nic_s *aq_nic, struct hwtstamp_config *config)
{
	aq_nic->aq_ptp->hwtstamp_config = *config;
}

static unsigned int aq_ptp_pdata_rx_hook(struct aq_nic_s *aq_nic,
			struct sk_buff *skb, u8 *p, unsigned int len)
{
	struct aq_ptp_s *self = aq_nic->aq_ptp;
  struct ethhdr *eth;
  unsigned int offset;
	u64 timestamp, sec;
	u32 ns;
	u8 *ptr;

  offset = 14;

	if (len <= offset)
		return 0;
	
	/*
	 * The TIMESTAMP in the end of package has following format:
	 *   struct {
	 *     uint64_t sec;        // big-endian
	 *     uint32_t ns;         // big-endian
	 *     uint16_t stream_id;  // big-endian
	 *   };
	 */
	ptr = p + (len - offset);
	memcpy(&sec, ptr, sizeof(sec));
	ptr += sizeof(sec);
	memcpy(&ns, ptr, sizeof(ns));

	sec = be64_to_cpu(sec) & 0xffffffffffffllu;
	ns = be32_to_cpu(ns);
	timestamp = sec * 1000000000llu + ns;
	
	aq_ptp_rx_hwtstamp(self, skb, timestamp);

  eth = (struct ethhdr *)p;
  if (eth->h_proto == htons(ETH_P_1588))
    return 12;
  return 0;
}

static int aq_ptp_poll(struct napi_struct *napi, int budget)
{
	struct aq_ptp_s *self = container_of(napi, struct aq_ptp_s, napi);
	struct aq_nic_s *aq_nic = self->aq_nic;
	bool was_cleaned = false;
	int work_done = 0;
	int err;
	
	/* Processing PTP TX traffic */
	err = aq_nic->aq_hw_ops->hw_ring_tx_head_update(aq_nic->aq_hw, &self->ptp_tx);
	if (err < 0)
		goto err_exit;
	
	if (self->ptp_tx.sw_head != self->ptp_tx.hw_head) {
		aq_ring_tx_clean(&self->ptp_tx);
		
		was_cleaned = true;
	}

	/* Processing HW_TIMESTAMP RX traffic */
	err = aq_nic->aq_hw_ops->hw_ring_hwts_rx_receive(aq_nic->aq_hw, &self->hwts_rx);
	if (err < 0)
		goto err_exit;
	
	if (self->hwts_rx.sw_head != self->hwts_rx.hw_head) {
		aq_ring_hwts_rx_clean(&self->hwts_rx, aq_nic);
		
		err = aq_nic->aq_hw_ops->hw_ring_hwts_rx_fill(aq_nic->aq_hw,
			&self->hwts_rx);

		was_cleaned = true;
	}
	
	/* Processing PTP RX traffic */
	err = aq_nic->aq_hw_ops->hw_ring_rx_receive(aq_nic->aq_hw, &self->ptp_rx);
	if (err < 0)
		goto err_exit;
	
	if (self->ptp_rx.sw_head != self->ptp_rx.hw_head) {
		unsigned int sw_tail_old;
		err = aq_ring_rx_clean(&self->ptp_rx, napi, &work_done, budget, aq_ptp_pdata_rx_hook);
		if (err < 0)
			goto err_exit;
		
		sw_tail_old = self->ptp_rx.sw_tail;
		
		err = aq_ring_rx_fill(&self->ptp_rx);
		if (err < 0)
			goto err_exit;
		
		err = aq_nic->aq_hw_ops->hw_ring_rx_fill(aq_nic->aq_hw, &self->ptp_rx, sw_tail_old);
		if (err < 0)
			goto err_exit;
	}

	if (was_cleaned)
		work_done = budget;

	if (work_done < budget) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
		napi_complete_done(napi, work_done);
#else
		napi_complete(napi);
#endif
		aq_nic->aq_hw_ops->hw_irq_enable(aq_nic->aq_hw, 1 << self->ptp_ring_param.vec_idx);
	}
	
err_exit:
	return work_done;
}

static irqreturn_t aq_ptp_isr(int irq, void *private)
{
	struct aq_ptp_s *self = private;
	int err = 0;
	
	if (!self) {
		err = -EINVAL;
		goto err_exit;
	}
	napi_schedule(&self->napi);

err_exit:
	return err >= 0 ? IRQ_HANDLED : IRQ_NONE;
}

int aq_ptp_xmit(struct aq_nic_s *aq_nic, struct sk_buff *skb)
{
	struct aq_ptp_s *self = aq_nic->aq_ptp;
	struct aq_ring_s *ring;
	unsigned int frags;
	int err = NETDEV_TX_OK;

	if (skb->len <= 0) {
		dev_kfree_skb_any(skb);
		goto err_exit;
	}
	
	frags = skb_shinfo(skb)->nr_frags + 1;
	if (frags > AQ_CFG_SKB_FRAGS_MAX) {
		dev_kfree_skb_any(skb);
		goto err_exit;
	}

	skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
	aq_ptp_skb_put(&self->skb_ring, skb);
	skb_tx_timestamp(skb);
	
	ring = &self->ptp_tx;
	
	frags = aq_nic_map_skb(aq_nic, skb, ring);
	
	if (likely(frags)) {
		err = aq_nic->aq_hw_ops->hw_ring_tx_xmit(aq_nic->aq_hw,
						       ring, frags);
		if (err >= 0) {
			++ring->stats.tx.packets;
			ring->stats.tx.bytes += skb->len;
		}
	} else {
		err = NETDEV_TX_BUSY;
	}
	
err_exit:
	return err;
}

int aq_ptp_irq_alloc(struct aq_nic_s *aq_nic)
{
	struct pci_dev *pdev = aq_nic->pdev;
	struct aq_ptp_s *self = aq_nic->aq_ptp;
	int err = 0;
	
	if (pdev->msix_enabled || pdev->msi_enabled) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 7, 0)
		err = request_irq(self->num_vector, aq_ptp_isr, 0,
				aq_nic->ndev->name, self);
#else
		err = request_irq(pci_irq_vector(pdev, self->idx_vector), aq_ptp_isr, 0,
				aq_nic->ndev->name, self);
#endif
	} else {
		err = -EINVAL;
		goto err_exit;
	}

err_exit:
	return 0;
}

void aq_ptp_irq_free(struct aq_nic_s *aq_nic)
{
	struct aq_ptp_s *self = aq_nic->aq_ptp;
	
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 7, 0)
	free_irq(self->num_vector, self);
#else
	struct pci_dev *pdev = aq_nic->pdev;
	free_irq(pci_irq_vector(pdev, self->idx_vector), self);
#endif
}

int aq_ptp_ring_init(struct aq_nic_s *aq_nic)
{
	struct aq_ptp_s *self = aq_nic->aq_ptp;
	int err = 0;
	
	err = aq_ring_init(&self->ptp_tx);
	if (err < 0)
		goto err_exit;
	err = aq_nic->aq_hw_ops->hw_ring_tx_init(aq_nic->aq_hw,
			&self->ptp_tx, &self->ptp_ring_param);
	if (err < 0)
		goto err_exit;
	
	err = aq_ring_init(&self->ptp_rx);
	if (err < 0)
		goto err_exit;
	err = aq_nic->aq_hw_ops->hw_ring_rx_init(aq_nic->aq_hw,
			&self->ptp_rx, &self->ptp_ring_param);
	if (err < 0)
		goto err_exit;
	err = aq_ring_rx_fill(&self->ptp_rx);
	if (err < 0)
		goto err_exit;
	err = aq_nic->aq_hw_ops->hw_ring_rx_fill(aq_nic->aq_hw,
			&self->ptp_rx, 0U);
	if (err < 0)
		goto err_exit;
	
	err = aq_ring_init(&self->hwts_rx);
	if (err < 0)
		goto err_exit;
	err = aq_nic->aq_hw_ops->hw_ring_rx_init(aq_nic->aq_hw,
			&self->hwts_rx, &self->ptp_ring_param);
	if (err < 0)
		goto err_exit;
	err = aq_nic->aq_hw_ops->hw_ring_hwts_rx_fill(aq_nic->aq_hw,
			&self->hwts_rx);
	if (err < 0)
		goto err_exit;

err_exit:
	return err;
}

int aq_ptp_ring_start(struct aq_nic_s *aq_nic)
{
	struct aq_ptp_s *self = aq_nic->aq_ptp;
	int err = 0;
	
	err = aq_nic->aq_hw_ops->hw_rx_l3l4_udp_filter_set(aq_nic->aq_hw, 0, 319);
	if (err < 0)
		goto err_exit;

  err = aq_nic->aq_hw_ops->hw_rx_ethtype_filter_set(aq_nic->aq_hw, 0, 0x88f7);
  if (err < 0)
    goto err_exit;
	
	err = aq_nic->aq_hw_ops->hw_ring_tx_start(aq_nic->aq_hw, &self->ptp_tx);
	if (err < 0)
		goto err_exit;
	
	err = aq_nic->aq_hw_ops->hw_ring_rx_start(aq_nic->aq_hw, &self->ptp_rx);
	if (err < 0)
		goto err_exit;
	
	err = aq_nic->aq_hw_ops->hw_ring_rx_start(aq_nic->aq_hw, &self->hwts_rx);
	if (err < 0)
		goto err_exit;

	napi_enable(&self->napi);
	
err_exit:
	return err;
}

void aq_ptp_ring_stop(struct aq_nic_s *aq_nic)
{
	struct aq_ptp_s *self = aq_nic->aq_ptp;
	
	aq_nic->aq_hw_ops->hw_ring_tx_stop(aq_nic->aq_hw, &self->ptp_tx);
	aq_nic->aq_hw_ops->hw_ring_rx_stop(aq_nic->aq_hw, &self->ptp_rx);
	
	aq_nic->aq_hw_ops->hw_ring_rx_stop(aq_nic->aq_hw, &self->hwts_rx);
	
	napi_disable(&self->napi);
}

void aq_ptp_ring_deinit(struct aq_nic_s *aq_nic)
{
	struct aq_ptp_s *self = aq_nic->aq_ptp;
	
	aq_ring_tx_clean(&self->ptp_tx);
	aq_ring_rx_deinit(&self->ptp_rx);
}

#define PTP_8TC_RING_IDX             8
#define PTP_4TC_RING_IDX            16
#define PTP_HWST_RING_IDX           31

int aq_ptp_ring_alloc(struct aq_nic_s *aq_nic)
{
	struct aq_ptp_s *self = aq_nic->aq_ptp;
	struct aq_ring_s *ring;
	struct aq_ring_s *hwts;
	u32 tx_tc_mode, rx_tc_mode;
	unsigned int tx_ring_idx, rx_ring_idx;
	int err;
	
	/*
	 * Index must to be 8 (8 TCs) or 16 (4 TCs).
	 * It depends from Traffic Class mode.
	 */
	aq_nic->aq_hw_ops->hw_tx_tc_mode_get(aq_nic->aq_hw, &tx_tc_mode);
	if (tx_tc_mode == 0)
		tx_ring_idx = PTP_8TC_RING_IDX;
	else
		tx_ring_idx = PTP_4TC_RING_IDX;
	
	ring = aq_ring_tx_alloc(&self->ptp_tx, aq_nic,
			tx_ring_idx, &aq_nic->aq_nic_cfg);
	if (!ring) {
		err = -ENOMEM;
		goto err_exit_1;
	}

	aq_nic->aq_hw_ops->hw_rx_tc_mode_get(aq_nic->aq_hw, &rx_tc_mode);
	if (rx_tc_mode == 0)
		rx_ring_idx = PTP_8TC_RING_IDX;
	else
		rx_ring_idx = PTP_4TC_RING_IDX;

	ring = aq_ring_rx_alloc(&self->ptp_rx, aq_nic,
			rx_ring_idx, &aq_nic->aq_nic_cfg);
	if (!ring) {
		err = -ENOMEM;
		goto err_exit_2;
	}
	
	hwts = aq_ring_hwts_alloc(&self->hwts_rx, aq_nic, PTP_HWST_RING_IDX,
			aq_nic->aq_nic_cfg.rxds, aq_nic->aq_nic_cfg.aq_hw_caps->rxd_size);
	if (!hwts) {
		err = -ENOMEM;
		goto err_exit_3;
	}
	
	err = aq_ptp_skb_ring_init(&self->skb_ring, aq_nic->aq_nic_cfg.rxds);
	if (err != 0) {
		err = -ENOMEM;
		goto err_exit_4;
	}

	self->ptp_ring_param.vec_idx = self->idx_vector; 
	self->ptp_ring_param.cpu = self->ptp_ring_param.vec_idx +
			aq_nic_get_cfg(aq_nic)->aq_rss.base_cpu_number;
	cpumask_set_cpu(self->ptp_ring_param.cpu,
			&self->ptp_ring_param.affinity_mask);

	return 0;
	
err_exit_4:
	aq_ring_free(&self->hwts_rx);
err_exit_3:
	aq_ring_free(&self->ptp_rx);
err_exit_2:
	aq_ring_free(&self->ptp_tx);
err_exit_1:
	return err;
}

void aq_ptp_ring_free(struct aq_nic_s *aq_nic)
{
	struct aq_ptp_s *self = aq_nic->aq_ptp;
	
	aq_ring_free(&self->ptp_tx);
	aq_ring_free(&self->ptp_rx);
	aq_ring_free(&self->hwts_rx);
	
	aq_ptp_skb_ring_release(&self->skb_ring);
}

static struct ptp_pin_desc aq_ptp_pd[3] = {
	{
		.name = "AQ_GPIO0",
		.index = 0,
		.func = PTP_PF_PEROUT,
		.chan = 0
	},
	{
		.name = "AQ_GPIO1",
		.index = 1,
		.func = PTP_PF_PEROUT,
		.chan = 1
	},
	{
		.name = "AQ_GPIO2",
		.index = 2,
		.func = PTP_PF_PEROUT,
		.chan = 2
	}
};

static struct ptp_clock_info aq_ptp_clock = {
	.owner		= THIS_MODULE,
	.name		= "aQ PTP clock",
	.max_adj	= 999999999,
	.n_ext_ts	= 0,
	.pps		= 0,
	.adjfreq	= aq_ptp_adjfreq,
	.adjtime	= aq_ptp_adjtime,
	.gettime64	= aq_ptp_gettime,
	.settime64	= aq_ptp_settime,

	/* enable periodic outputs */
	.n_per_out     = 3,
	.enable        = aq_ptp_feature_enable,
	/* enable clock pins */
	.n_pins        = 3, // Three GPIOs
	.verify        = aq_ptp_verify,
	.pin_config    = aq_ptp_pd,
};

/**
 * aq_ptp_init
 * @adapter: the aqc private adapter structure
 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 7, 0)
int aq_ptp_init(struct aq_nic_s *aq_nic, unsigned int idx_vec, unsigned int num_vec)
#else
int aq_ptp_init(struct aq_nic_s *aq_nic, unsigned int idx_vec)
#endif
{
	struct aq_ptp_s *self;
	struct ptp_clock *clock;
	int err = 0;
	
	self = kzalloc(sizeof(*self), GFP_KERNEL);
	if (!self) {
		err = -ENOMEM;
		goto err_exit;
	}
	
	self->aq_nic = aq_nic;
	
	spin_lock_init(&self->ptp_lock);
	
	self->ptp_info = aq_ptp_clock;
	clock = ptp_clock_register(&self->ptp_info, &aq_nic->ndev->dev);
	if (IS_ERR(clock)) {
		dev_err(aq_nic->ndev->dev.parent, "ptp_clock_register failed\n");
		err = -EFAULT;
		goto err_exit;
	}
	self->ptp_clock = clock;

	self->cc.read = aq_ptp_read;
	self->cc.mask = CLOCKSOURCE_MASK(64);
	self->cc.mult = 1;
	self->cc.shift = 0;
	
	timecounter_init(&self->tc, &self->cc, ktime_to_ns(ktime_get_real()));

  atomic_set(&self->offset_egress, 0);
  atomic_set(&self->offset_ingress, 0);
	
	netif_napi_add(aq_nic_get_ndev(aq_nic), &self->napi,
			aq_ptp_poll, AQ_CFG_NAPI_WEIGHT);


#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 7, 0)
	self->num_vector = num_vec;
#endif
	self->idx_vector = idx_vec;
			
	aq_nic->aq_ptp = self;
	
err_exit:
	return err;
}

/**
 * aq_ptp_stop - close the PTP device
 * @adapter: pointer to adapter struct
 *
 * completely destroy the PTP device, should only be called when the device is
 * being fully closed.
 */
void aq_ptp_unregister(struct aq_nic_s *aq_nic)
{
	struct aq_ptp_s *self = aq_nic->aq_ptp;

	ptp_clock_unregister(self->ptp_clock);
}

void aq_ptp_free(struct aq_nic_s *aq_nic)
{
	struct aq_ptp_s *self = aq_nic->aq_ptp;
	
	if (self) {
		netif_napi_del(&self->napi);
		kfree(self);
		aq_nic->aq_ptp = NULL;
	}
}

struct ptp_clock *aq_ptp_get_ptp_clock(struct aq_nic_s *aq_nic)
{
	return aq_nic->aq_ptp->ptp_clock;
}
