/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
 */

#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 13, 0)
/*
 * Upstream Linux commit bd4fb6d270bc ("arm64: Add support for SB
 * barrier and patch in over DSB; ISB sequences") added speculation
 * macro 'spec_bar' to inhibit speculation. This has since been removed
 * from the upstream kernel starting with Linux v5.13, because there are
 * no current users. Define this macro here for NVIDIA drivers to use.
 */
#define spec_bar()	asm volatile(ALTERNATIVE("dsb nsh\nisb\n",	\
				     SB_BARRIER_INSN"nop\n",		\
				     ARM64_HAS_SB))
#endif
