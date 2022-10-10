/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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
 * This header provides constants for binding nvidia,tegra186-hsp
 */


#ifndef _DT_BINDINGS_MAILBOX_TEGRA186_HSP_H
#define _DT_BINDINGS_MAILBOX_TEGRA186_HSP_H

/*
 * These define the type of mailbox that is to be used (doorbell, shared
 * mailbox, shared semaphore or arbitrated semaphore).
 */
#define TEGRA_HSP_MBOX_TYPE_DB 0x0
#define TEGRA_HSP_MBOX_TYPE_SM 0x1
#define TEGRA_HSP_MBOX_TYPE_SS 0x2
#define TEGRA_HSP_MBOX_TYPE_AS 0x3

#if TEGRA_HSP_DT_VERSION >= DT_VERSION_2
#define TEGRA_HSP_MBOX_TYPE_SM_128BIT (1 << 8)
#else
#define TEGRA_HSP_MBOX_TYPE_SM_128BIT 0x4
#endif
/*
 * These defines represent the bit associated with the given master ID in the
 * doorbell registers.
 */
#define TEGRA_HSP_DB_MASTER_CCPLEX 17
#define TEGRA_HSP_DB_MASTER_BPMP 19

/*
* Shared mailboxes are unidirectional, so the direction needs to be specified
* in the device tree.
*/
#define TEGRA_HSP_SM_MASK 0x00ffffff
#define TEGRA_HSP_SM_FLAG_RX (0 << 31)
#define TEGRA_HSP_SM_FLAG_TX (1 << 31)

#define TEGRA_HSP_SM_RX(x) (TEGRA_HSP_SM_FLAG_RX | ((x) & TEGRA_HSP_SM_MASK))
#define TEGRA_HSP_SM_TX(x) (TEGRA_HSP_SM_FLAG_TX | ((x) & TEGRA_HSP_SM_MASK))

#endif
