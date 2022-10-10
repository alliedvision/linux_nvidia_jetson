/*
 * Copyright (c) 2016-2022, NVIDIA CORPORATION. All rights reserved.
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

#include <linux/export.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/version.h>
#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif
#include <linux/platform_device.h>
#include <linux/nvhost.h>

#include "pva.h"
#include "pva_mailbox.h"
#include "pva_interface_regs_t19x.h"

static struct pva_status_interface_registers t19x_status_regs[NUM_INTERFACES_T19X] = {
	{
		{
			PVA_CCQ_STATUS3_REG,
			PVA_CCQ_STATUS4_REG,
			PVA_CCQ_STATUS5_REG,
			PVA_CCQ_STATUS6_REG,
			PVA_CCQ_STATUS7_REG
		}
	},
};

void read_status_interface_t19x(struct pva *pva,
				uint32_t interface_id, u32 isr_status,
				struct pva_cmd_status_regs *status_output)
{
	int i;
	uint32_t *status_registers;

	status_registers = t19x_status_regs[interface_id].registers;

	for (i = 0; i < PVA_CMD_STATUS_REGS; i++) {
		if (isr_status & (PVA_VALID_STATUS3 << i)) {
			status_output->status[i] = host1x_readl(pva->pdev,
						    status_registers[i]);
			if ((i == 0) && (isr_status & PVA_CMD_ERROR)) {
				status_output->error =
					PVA_GET_ERROR_CODE(
						status_output->status[i]);
			}
		}
	}
}
