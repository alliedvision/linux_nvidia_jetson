/* SPDX-License-Identifier: GPL-2.0 */
/*
 * include/linux/tegra-timer.h
 *
 * Copyright (c) 2016-2020 NVIDIA Corporation. All rights reserved.
 */

#ifndef _RTC_TEGRA_H_
#define _RTC_TEGRA_H_

#define RTC_SECONDS		0x08
#define RTC_SHADOW_SECONDS	0x0c
#define RTC_MILLISECONDS	0x10

#ifdef CONFIG_RTC_DRV_TEGRA
void tegra_rtc_set_trigger(unsigned long cycles);
u64 tegra_rtc_read_ms(void);
#else
static inline void tegra_rtc_set_trigger(unsigned long cycles) { return; };
static inline u64 tegra_rtc_read_ms(void) { return 0; };
#endif
#endif /* _RTC_TEGRA_H_ */
