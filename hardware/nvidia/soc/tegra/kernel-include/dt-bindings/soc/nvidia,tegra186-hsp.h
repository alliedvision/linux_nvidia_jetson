/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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

/*
 * This header provides constants for binding nvidia,tegra186-hsp.
 */

#ifndef _DT_BINDINGS_SOC_NVIDIA_TEGRA186_HSP_H_
#define _DT_BINDINGS_SOC_NVIDIA_TEGRA186_HSP_H_

#include <dt-bindings/mailbox/tegra186-hsp.h>

/*
 * These define the type of mailbox that is to be used (doorbell, shared
 * mailbox, shared semaphore or arbitrated semaphore).
 */
#ifndef TEGRA_HSP_MBOX_TYPE_DB
#define TEGRA_HSP_MBOX_TYPE_DB 0x0
#endif
#ifndef TEGRA_HSP_MBOX_TYPE_SM
#define TEGRA_HSP_MBOX_TYPE_SM 0x1
#endif
#ifndef TEGRA_HSP_MBOX_TYPE_SS
#define TEGRA_HSP_MBOX_TYPE_SS 0x2
#endif
#ifndef TEGRA_HSP_MBOX_TYPE_AS
#define TEGRA_HSP_MBOX_TYPE_AS 0x3
#endif
#ifndef TEGRA_HSP_MBOX_TYPE_SM_128BIT
#define TEGRA_HSP_MBOX_TYPE_SM_128BTT 0x4
#endif

/*
* Shared mailboxes are unidirectional, so the direction needs to be specified
* in the device tree.
*/
#ifndef TEGRA_HSP_SM_FLAG_RXTX_MASK
#define TEGRA_HSP_SM_FLAG_RXTX_MASK (1 << 31)
#endif
#ifndef TEGRA_HSP_SM_FLAG_RX
#define TEGRA_HSP_SM_FLAG_RX (0 << 31)
#endif
#ifndef TEGRA_HSP_SM_FLAG_TX
#define TEGRA_HSP_SM_FLAG_TX (1 << 31)
#endif

#ifndef TEGRA_HSP_SM_MASK
#define TEGRA_HSP_SM_MASK 0x00ffffff
#endif

#ifndef TEGRA_HSP_SM_RX
#define TEGRA_HSP_SM_RX(x) (TEGRA_HSP_SM_FLAG_RX | ((x) & TEGRA_HSP_SM_MASK))
#endif
#ifndef TEGRA_HSP_SM_TX
#define TEGRA_HSP_SM_TX(x) (TEGRA_HSP_SM_FLAG_TX | ((x) & TEGRA_HSP_SM_MASK))
#endif

#endif	/* _DT_BINDINGS_SOC_NVIDIA_TEGRA186_HSP_H_ */
