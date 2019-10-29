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
 * File aq_ptp.c: Declaration of PTP functions.
 */

#ifndef aq_ptp_h
#define aq_ptp_h

#include <linux/net_tstamp.h>


#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 7, 0)
int aq_ptp_init(struct aq_nic_s *aq_nic, unsigned int idx_vec, unsigned int num_vec);
#else
int aq_ptp_init(struct aq_nic_s *aq_nic, unsigned int idx_vec);
#endif

void aq_ptp_unregister(struct aq_nic_s *aq_nic);
void aq_ptp_free(struct aq_nic_s *aq_nic);

int aq_ptp_irq_alloc(struct aq_nic_s *aq_nic);
void aq_ptp_irq_free(struct aq_nic_s *aq_nic);

int aq_ptp_xmit(struct aq_nic_s *aq_nic, struct sk_buff *skb);
void aq_ptp_tx_hwtstamp(struct aq_nic_s *aq_nic, u64 timestamp);

int aq_ptp_ring_alloc(struct aq_nic_s *aq_nic);
void aq_ptp_ring_free(struct aq_nic_s *aq_nic);

int aq_ptp_ring_init(struct aq_nic_s *aq_nic);
int aq_ptp_ring_start(struct aq_nic_s *aq_nic);
void aq_ptp_ring_stop(struct aq_nic_s *aq_nic);
void aq_ptp_ring_deinit(struct aq_nic_s *aq_nic);

struct ptp_clock *aq_ptp_get_ptp_clock(struct aq_nic_s *aq_nic);

void aq_ptp_hwtstamp_config_set(struct aq_nic_s *aq_nic, struct hwtstamp_config *config);
void aq_ptp_hwtstamp_config_get(struct aq_nic_s *aq_nic, struct hwtstamp_config *config);

void aq_ptp_tm_offset_set(struct aq_nic_s *aq_nic, unsigned int mbps);

#endif /* aq_ptp_h */
