/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020 Google, Inc.
 */

/*
 * Copyright (c) 2017-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/linkage.h>
#include <linux/version.h>

.macro push ra, rb
stp \ra, \rb, [sp,#-16]!
.endm

.macro pop ra, rb
ldp \ra, \rb, [sp], #16
.endm

lr .req x30

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
SYM_FUNC_START(trusty_smc8)
#else
ENTRY(trusty_smc8)
#endif

    /*
     * Save x8 (return value ptr) and lr. The SMC calling convention says el3
     * does not need to preserve x8. The normal ABI does not require either x8
     * or lr to be preserved.
     */
    push    x8, lr
    smc     #0
    pop     x8, lr

    /* Copy 8-register smc return value to struct smc_ret8 return value */
    stp     x0, x1, [x8], #16
    stp     x2, x3, [x8], #16
    stp     x4, x5, [x8], #16
    stp     x6, x7, [x8], #16

    ret

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
SYM_FUNC_END(trusty_smc8)
#else
ENDPROC(trusty_smc8)
#endif
