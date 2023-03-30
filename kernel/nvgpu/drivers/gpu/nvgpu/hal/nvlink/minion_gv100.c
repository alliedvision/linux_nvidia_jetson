/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/io.h>
#include <nvgpu/timers.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/nvlink_minion.h>

#include "minion_gv100.h"

#include <nvgpu/hw/gv100/hw_minion_gv100.h>

static u32 get_minion_falcon_intr_mask(void)
{
	u32 mask = minion_falcon_irqmset_wdtmr_set_f() |
				 minion_falcon_irqmset_halt_set_f()  |
				 minion_falcon_irqmset_exterr_set_f()|
				 minion_falcon_irqmset_swgen0_set_f()|
				 minion_falcon_irqmset_swgen1_set_f();
	return mask;
}

static u32 get_minion_falcon_intr_dest(void)
{
	u32 mask = minion_falcon_irqdest_host_wdtmr_host_f() |
		minion_falcon_irqdest_host_halt_host_f() |
		minion_falcon_irqdest_host_exterr_host_f() |
		minion_falcon_irqdest_host_swgen0_host_f() |
		minion_falcon_irqdest_host_swgen1_host_f() |
		minion_falcon_irqdest_target_wdtmr_host_normal_f() |
		minion_falcon_irqdest_target_halt_host_normal_f() |
		minion_falcon_irqdest_target_exterr_host_normal_f() |
		minion_falcon_irqdest_target_swgen0_host_normal_f() |
		minion_falcon_irqdest_target_swgen1_host_normal_f();

	return mask;
}

u32 gv100_nvlink_minion_base_addr(struct gk20a *g)
{
	return g->nvlink.minion_base;
}

/*
 * Check if minion is up
 */
bool gv100_nvlink_minion_is_running(struct gk20a *g)
{
	/* if minion is booted and not halted, it is running */
	if ((MINION_REG_RD32(g, minion_minion_status_r()) &
				minion_minion_status_status_f(1)) != 0U) {
		if (minion_falcon_irqstat_halt_v(
		MINION_REG_RD32(g, minion_falcon_irqstat_r())) == 0U) {
			return true;
		}
	}

	return false;
}

/*
 * Check if minion ucode boot is complete.
 */
int gv100_nvlink_minion_is_boot_complete(struct gk20a *g, bool *boot_cmplte)
{
	u32 reg;
	int err = 0;

	reg = MINION_REG_RD32(g, minion_minion_status_r());
	*boot_cmplte = false;
	if (minion_minion_status_status_v(reg) != 0U) {
		/* Minion sequence completed, check status */
		if (minion_minion_status_status_v(reg) ==
					minion_minion_status_status_boot_v()) {
			*boot_cmplte = true;
		} else {
			nvgpu_err(g, "MINION init sequence failed: 0x%x",
					minion_minion_status_status_v(reg));
			err = -EINVAL;
		}
	}

	return err;
}

/*
 * Check if MINION command is complete
 */
static int gv100_nvlink_minion_command_complete(struct gk20a *g, u32 link_id)
{
	u32 reg;
	struct nvgpu_timeout timeout;
	u32 delay = POLL_DELAY_MIN_US;

	nvgpu_timeout_init_cpu_timer(g, &timeout, nvgpu_get_poll_timeout(g));

	do {
		reg = MINION_REG_RD32(g, minion_nvlink_dl_cmd_r(link_id));

		if (minion_nvlink_dl_cmd_ready_v(reg) == 1U) {
			/* Command completed, check sucess */
			if (minion_nvlink_dl_cmd_fault_v(reg) ==
				minion_nvlink_dl_cmd_fault_fault_clear_v()) {
				nvgpu_err(g, "minion cmd(%d) error: 0x%x",
					link_id, reg);

				reg = minion_nvlink_dl_cmd_fault_f(1);
				MINION_REG_WR32(g,
					minion_nvlink_dl_cmd_r(link_id), reg);

				return -EINVAL;
			}

			/* Command success */
			break;
		}
		nvgpu_usleep_range(delay, delay * 2U);
		delay = min_t(unsigned int,
				delay << 1, POLL_DELAY_MAX_US);

	} while (nvgpu_timeout_expired_msg(&timeout,
					"minion cmd timeout") == 0);

	if (nvgpu_timeout_peek_expired(&timeout)) {
		return -ETIMEDOUT;
	}

	nvgpu_log(g, gpu_dbg_nvlink, "minion cmd Complete");
	return 0;
}

u32 gv100_nvlink_minion_get_dlcmd_ordinal(struct gk20a *g,
					enum nvgpu_nvlink_minion_dlcmd dlcmd)
{
	u32 dlcmd_ordinal;

	switch (dlcmd) {
	case NVGPU_NVLINK_MINION_DLCMD_INITPHY:
		dlcmd_ordinal = minion_nvlink_dl_cmd_command_initphy_v();
		break;
	case NVGPU_NVLINK_MINION_DLCMD_INITLANEENABLE:
		dlcmd_ordinal = minion_nvlink_dl_cmd_command_initlaneenable_v();
		break;
	case NVGPU_NVLINK_MINION_DLCMD_INITDLPL:
		dlcmd_ordinal = minion_nvlink_dl_cmd_command_initdlpl_v();
		break;
	case NVGPU_NVLINK_MINION_DLCMD_LANEDISABLE:
		dlcmd_ordinal = minion_nvlink_dl_cmd_command_lanedisable_v();
		break;
	case NVGPU_NVLINK_MINION_DLCMD_SETACMODE:
		dlcmd_ordinal = minion_nvlink_dl_cmd_command_setacmode_v();
		break;
	case NVGPU_NVLINK_MINION_DLCMD_LANESHUTDOWN:
		dlcmd_ordinal = minion_nvlink_dl_cmd_command_laneshutdown_v();
		break;
	case NVGPU_NVLINK_MINION_DLCMD_INITPLL_1:
		dlcmd_ordinal = minion_nvlink_dl_cmd_command_initpll_1_v();
		break;
	default:
		dlcmd_ordinal = U32_MAX;
		break;
	}

	return dlcmd_ordinal;
}

/*
 * Send Minion command (can be async)
 */
int gv100_nvlink_minion_send_dlcmd(struct gk20a *g, u32 link_id,
				enum nvgpu_nvlink_minion_dlcmd dlcmd, bool sync)
{
	int err = 0;
	u32 dlcmd_ordinal;

	dlcmd_ordinal = g->ops.nvlink.minion.get_dlcmd_ordinal(g, dlcmd);
	if (dlcmd_ordinal == U32_MAX) {
		nvgpu_err(g, "DLCMD not supported");
		return -EPERM;
	}

	/* Check last command succeded */
	err = gv100_nvlink_minion_command_complete(g, link_id);
	if (err != 0) {
		return -EINVAL;
	}

	nvgpu_log(g, gpu_dbg_nvlink, "sending MINION command 0x%x to link %d",
						dlcmd_ordinal, link_id);

	MINION_REG_WR32(g, minion_nvlink_dl_cmd_r(link_id),
		minion_nvlink_dl_cmd_command_f(dlcmd_ordinal) |
		minion_nvlink_dl_cmd_fault_f(1));

	if (sync) {
		err = gv100_nvlink_minion_command_complete(g, link_id);
	}

	return err;
}

/*
 * Clear minion Interrupts
 */
void gv100_nvlink_minion_clear_intr(struct gk20a *g)
{
	nvgpu_falcon_set_irq(&g->minion_flcn, true, get_minion_falcon_intr_mask(),
						get_minion_falcon_intr_dest());
}

/*
 * Initialization of link specific interrupts
 */
void gv100_nvlink_minion_enable_link_intr(struct gk20a *g, u32 link_id,
								bool enable)
{
	u32 intr, links;

	/* Only stall interrupts for now */
	intr = MINION_REG_RD32(g, minion_minion_intr_stall_en_r());
	links = minion_minion_intr_stall_en_link_v(intr);

	if (enable) {
		links |= BIT32(link_id);
	} else {
		links &= ~BIT32(link_id);
	}

	intr = set_field(intr, minion_minion_intr_stall_en_link_m(),
		minion_minion_intr_stall_en_link_f(links));
	MINION_REG_WR32(g, minion_minion_intr_stall_en_r(), intr);
}

/*
 * Initialization of falcon interrupts
 */
static void gv100_nvlink_minion_falcon_intr_enable(struct gk20a *g, bool enable)
{
	u32 reg;

	reg = MINION_REG_RD32(g, minion_minion_intr_stall_en_r());
	if (enable) {
		reg = set_field(reg, minion_minion_intr_stall_en_fatal_m(),
			minion_minion_intr_stall_en_fatal_enable_f());
		reg = set_field(reg, minion_minion_intr_stall_en_nonfatal_m(),
			minion_minion_intr_stall_en_nonfatal_enable_f());
		reg = set_field(reg, minion_minion_intr_stall_en_falcon_stall_m(),
			minion_minion_intr_stall_en_falcon_stall_enable_f());
		reg = set_field(reg, minion_minion_intr_stall_en_falcon_nostall_m(),
			minion_minion_intr_stall_en_falcon_nostall_enable_f());
	} else {
		reg = set_field(reg, minion_minion_intr_stall_en_fatal_m(),
			minion_minion_intr_stall_en_fatal_disable_f());
		reg = set_field(reg, minion_minion_intr_stall_en_nonfatal_m(),
			minion_minion_intr_stall_en_nonfatal_disable_f());
		reg = set_field(reg, minion_minion_intr_stall_en_falcon_stall_m(),
			minion_minion_intr_stall_en_falcon_stall_disable_f());
		reg = set_field(reg, minion_minion_intr_stall_en_falcon_nostall_m(),
			minion_minion_intr_stall_en_falcon_nostall_disable_f());
	}

	MINION_REG_WR32(g, minion_minion_intr_stall_en_r(), reg);
}

/*
 * Initialize minion IP interrupts
 */
void gv100_nvlink_minion_init_intr(struct gk20a *g)
{
	/* Disable non-stall tree */
	MINION_REG_WR32(g, minion_minion_intr_nonstall_en_r(), 0x0);

	gv100_nvlink_minion_falcon_intr_enable(g, true);
}

/*
 * Falcon specific ISR handling
 */
void gv100_nvlink_minion_falcon_isr(struct gk20a *g)
{
	u32 intr;

	intr = MINION_REG_RD32(g, minion_falcon_irqstat_r()) &
		MINION_REG_RD32(g, minion_falcon_irqmask_r());

	if (intr == 0u) {
		return;
	}

	if ((intr & minion_falcon_irqstat_exterr_true_f()) != 0u) {
		nvgpu_err(g, "falcon ext addr: 0x%x 0x%x 0x%x",
			MINION_REG_RD32(g, minion_falcon_csberrstat_r()),
			MINION_REG_RD32(g, minion_falcon_csberr_info_r()),
			MINION_REG_RD32(g, minion_falcon_csberr_addr_r()));
	}

	MINION_REG_WR32(g, minion_falcon_irqsclr_r(), intr);

	nvgpu_err(g, "fatal minion irq: 0x%08x", intr);

	return;
}

/*
 * link specific isr
 */

static void gv100_nvlink_minion_link_isr(struct gk20a *g, u32 link_id)
{
	u32 intr, code;
	bool fatal = false;

	intr = MINION_REG_RD32(g, minion_nvlink_link_intr_r(link_id));
	code = minion_nvlink_link_intr_code_v(intr);

	if (code == minion_nvlink_link_intr_code_swreq_v()) {
		nvgpu_err(g, " Intr SWREQ, link: %d subcode: %x",
			link_id, minion_nvlink_link_intr_subcode_v(intr));
	} else if (code == minion_nvlink_link_intr_code_pmdisabled_v()) {
		nvgpu_err(g, " Fatal Intr PMDISABLED, link: %d subcode: %x",
			link_id, minion_nvlink_link_intr_subcode_v(intr));
		fatal = true;
	} else if (code == minion_nvlink_link_intr_code_na_v()) {
		nvgpu_err(g, " Fatal Intr NA, link: %d subcode: %x",
			link_id, minion_nvlink_link_intr_subcode_v(intr));
		fatal = true;
	} else if (code == minion_nvlink_link_intr_code_dlreq_v()) {
		nvgpu_err(g, " Fatal Intr DLREQ, link: %d subcode: %x",
			link_id, minion_nvlink_link_intr_subcode_v(intr));
		fatal = true;
	} else {
		nvgpu_err(g, " Fatal Intr UNKN:%x, link: %d subcode: %x", code,
			link_id, minion_nvlink_link_intr_subcode_v(intr));
		fatal = true;
	}

	if (fatal) {
		g->ops.nvlink.minion.enable_link_intr(g, link_id, false);
	}

	intr = set_field(intr, minion_nvlink_link_intr_state_m(),
		minion_nvlink_link_intr_state_f(1));
	MINION_REG_WR32(g, minion_nvlink_link_intr_r(link_id), intr);

	return;
}

/*
 * Global minion routine to service interrupts
 */
void gv100_nvlink_minion_isr(struct gk20a *g) {

	u32 intr, link_id;
	unsigned long links;
	unsigned long bit;

	intr = MINION_REG_RD32(g, minion_minion_intr_r()) &
		MINION_REG_RD32(g, minion_minion_intr_stall_en_r());

	if ((minion_minion_intr_falcon_stall_v(intr) != 0U) ||
			(minion_minion_intr_falcon_nostall_v(intr) != 0U)) {
		gv100_nvlink_minion_falcon_isr(g);
	}

	if (minion_minion_intr_fatal_v(intr) != 0U) {
		gv100_nvlink_minion_falcon_intr_enable(g, false);
		MINION_REG_WR32(g, minion_minion_intr_r(),
					minion_minion_intr_fatal_f(1));
	}

	if (minion_minion_intr_nonfatal_v(intr) != 0U) {
		MINION_REG_WR32(g, minion_minion_intr_r(),
					minion_minion_intr_nonfatal_f(1));
	}

	links = minion_minion_intr_link_v(intr) &
					(unsigned long) g->nvlink.enabled_links;

	if (links != 0UL) {
		for_each_set_bit(bit, &links, NVLINK_MAX_LINKS_SW) {
			link_id = (u32)bit;
			gv100_nvlink_minion_link_isr(g, link_id);
		}
	}

	return;
}

#endif /* CONFIG_NVGPU_NVLINK */
