/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifdef CONFIG_NVGPU_NVLINK

#include <nvgpu/nvlink_minion.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>

#include "minion_gv100.h"
#include "minion_tu104.h"

#include <nvgpu/hw/tu104/hw_minion_tu104.h>

struct gk20a;

u32 tu104_nvlink_minion_get_dlcmd_ordinal(struct gk20a *g,
					enum nvgpu_nvlink_minion_dlcmd dlcmd)
{
	u32 dlcmd_ordinal;

	switch (dlcmd) {
	case NVGPU_NVLINK_MINION_DLCMD_INITRXTERM:
		dlcmd_ordinal = 0x05U;
		break;
	case NVGPU_NVLINK_MINION_DLCMD_TURING_RXDET:
		dlcmd_ordinal = minion_nvlink_dl_cmd_command_turing_rxdet_v();
		break;
	case NVGPU_NVLINK_MINION_DLCMD_TXCLKSWITCH_PLL:
		dlcmd_ordinal = minion_nvlink_dl_cmd_command_txclkswitch_pll_v();
		break;
	case NVGPU_NVLINK_MINION_DLCMD_TURING_INITDLPL_TO_CHIPA:
		dlcmd_ordinal = minion_nvlink_dl_cmd_command_turing_initdlpl_to_chipa_v();
		break;
	case NVGPU_NVLINK_MINION_DLCMD_INITTL:
		dlcmd_ordinal = minion_nvlink_dl_cmd_command_inittl_v();
		break;
	default:
		dlcmd_ordinal = gv100_nvlink_minion_get_dlcmd_ordinal(g, dlcmd);
		break;
	}

	return dlcmd_ordinal;
}

bool tu104_nvlink_minion_is_debug_mode(struct gk20a *g)
{
	u32 reg_val;

	reg_val = MINION_REG_RD32(g, minion_scp_ctl_stat_r());
	return (minion_scp_ctl_stat_debug_mode_v(reg_val) != 0U);
}

#endif /* CONFIG_NVGPU_NVLINK */
