/*
 * Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
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

#ifndef INCLUDED_CORE_COMMON_H
#define INCLUDED_CORE_COMMON_H

#include "core_local.h"
#define MTL_EST_ADDR_MASK		(OSI_BIT(8) | OSI_BIT(9) | \
					 OSI_BIT(10) | OSI_BIT(11) | \
					 OSI_BIT(12) | OSI_BIT(13) | \
					 OSI_BIT(14) | OSI_BIT(15) | \
					 OSI_BIT(16) | (17) | \
					 OSI_BIT(18) | OSI_BIT(19))
#define MTL_EST_SRWO			OSI_BIT(0)
#define MTL_EST_R1W0			OSI_BIT(1)
#define MTL_EST_GCRR			OSI_BIT(2)
#define MTL_EST_DBGM			OSI_BIT(4)
#define MTL_EST_DBGB			OSI_BIT(5)
#define MTL_EST_ERR0			OSI_BIT(20)
#define MTL_EST_CONTROL_EEST		OSI_BIT(0)
#define MTL_EST_STATUS_SWOL		OSI_BIT(7)

/**
 * @addtogroup typedef related info
 *
 * @brief typedefs that indeicates variable address and memory addr
 * @{
 */

struct est_read {
	/* variable pointer */
	nveu32_t *var;
	/* memory register/address offset */
	nveu32_t addr;
};

/** @} */

nve32_t gcl_validate(struct osi_core_priv_data *const osi_core,
		     struct osi_est_config *const est,
		     const nveu32_t *btr, nveu32_t mac);
#endif /* INCLUDED_CORE_COMMON_H */
