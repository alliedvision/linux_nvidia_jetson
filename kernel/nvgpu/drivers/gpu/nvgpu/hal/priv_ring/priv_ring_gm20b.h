/*
 * Copyright (c) 2011-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#ifndef NVGPU_PRIV_RING_GM20B_H
#define NVGPU_PRIV_RING_GM20B_H

struct gk20a;

/**
 * @addtogroup unit-common-priv-ring
 * @{
 */

/**
 * @defgroup PRIV_RING_HAL_DEFINES
 *
 * Priv Ring Hal defines
 */

/**
 * @ingroup PRIV_RING_HAL_DEFINES
 * @{
 */

#define COMMAND_CMD_ENUMERATE_AND_START_RING 0x4
#define CONFIG_RING_WAIT_FOR_RING_START_COMPLETE 0x2

#define GM20B_PRIV_RING_POLL_CLEAR_INTR_RETRIES	100
#define GM20B_PRIV_RING_POLL_CLEAR_INTR_UDELAY	20

/**
 * @}
 */

#ifdef CONFIG_NVGPU_HAL_NON_FUSA
void gm20b_priv_ring_isr(struct gk20a *g);
#endif
int gm20b_priv_ring_enable(struct gk20a *g);
void gm20b_priv_set_timeout_settings(struct gk20a *g);
u32 gm20b_priv_ring_enum_ltc(struct gk20a *g);

u32 gm20b_priv_ring_get_gpc_count(struct gk20a *g);
u32 gm20b_priv_ring_get_fbp_count(struct gk20a *g);

#endif /* NVGPU_PRIV_RING_GM20B_H */
