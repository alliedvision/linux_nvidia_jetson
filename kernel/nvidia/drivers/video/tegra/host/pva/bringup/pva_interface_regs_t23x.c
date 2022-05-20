/*
 * Copyright (c) 2016-2020, NVIDIA Corporation. All rights reserved.
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
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif
#include <linux/platform_device.h>

#include "nvhost_acm.h"
#include "dev.h"
#include "pva.h"
#include "pva_mailbox.h"
#include "pva_interface_regs_t23x.h"

static struct pva_status_interface_registers t23x_status_regs[NUM_INTERFACES_T23X] = {
	{
		{
			PVA_EMPTY_STATUS_REG,
			PVA_MBOX_STATUS4_REG,
			PVA_MBOX_STATUS5_REG,
			PVA_MBOX_STATUS6_REG,
			PVA_MBOX_STATUS7_REG
		}
	},
	{
		{
			PVA_EMPTY_STATUS_REG,
			PVA_CCQ0_STATUS3_REG,
			PVA_CCQ0_STATUS4_REG,
			PVA_CCQ0_STATUS5_REG,
			PVA_CCQ0_STATUS6_REG
		}
	},
	{
		{
			PVA_EMPTY_STATUS_REG,
			PVA_CCQ1_STATUS3_REG,
			PVA_CCQ1_STATUS4_REG,
			PVA_CCQ1_STATUS5_REG,
			PVA_CCQ1_STATUS6_REG
		}
	},
	{
		{
			PVA_EMPTY_STATUS_REG,
			PVA_CCQ2_STATUS3_REG,
			PVA_CCQ2_STATUS4_REG,
			PVA_CCQ2_STATUS5_REG,
			PVA_CCQ2_STATUS6_REG
		}
	},
	{
		{
			PVA_EMPTY_STATUS_REG,
			PVA_CCQ3_STATUS3_REG,
			PVA_CCQ3_STATUS4_REG,
			PVA_CCQ3_STATUS5_REG,
			PVA_CCQ3_STATUS6_REG
		}
	},
	{
		{
			PVA_EMPTY_STATUS_REG,
			PVA_CCQ4_STATUS3_REG,
			PVA_CCQ4_STATUS4_REG,
			PVA_CCQ4_STATUS5_REG,
			PVA_CCQ4_STATUS6_REG
		}
	},
	{
		{
			PVA_EMPTY_STATUS_REG,
			PVA_CCQ5_STATUS3_REG,
			PVA_CCQ5_STATUS4_REG,
			PVA_CCQ5_STATUS5_REG,
			PVA_CCQ5_STATUS6_REG
		}
	},
	{
		{
			PVA_EMPTY_STATUS_REG,
			PVA_CCQ6_STATUS3_REG,
			PVA_CCQ6_STATUS4_REG,
			PVA_CCQ6_STATUS5_REG,
			PVA_CCQ6_STATUS6_REG
		}
	},
	{
		{
			PVA_EMPTY_STATUS_REG,
			PVA_CCQ7_STATUS3_REG,
			PVA_CCQ7_STATUS4_REG,
			PVA_CCQ7_STATUS5_REG,
			PVA_CCQ7_STATUS6_REG
		}
	}
};


void read_status_interface_t23x(struct pva *pva,
				uint32_t interface_id, u32 isr_status,
				struct pva_cmd_status_regs *status_output)
{
	int i;
	u32 valid_status = PVA_VALID_STATUS3;
	uint32_t *status_registers;
	status_registers = t23x_status_regs[interface_id].registers;
	if (isr_status & PVA_CMD_ERROR) {
		status_output->error = PVA_GET_ERROR_CODE(isr_status);
	}
	if (isr_status & PVA_VALID_STATUS3) {
		status_output->status[0] = PVA_GET_ERROR_CODE(isr_status);
	}
	for (i = 1; i < NUM_STATUS_REGS; i++) {
		valid_status = valid_status << 1;
		if (isr_status & valid_status) {
			status_output->status[i] = host1x_readl(pva->pdev,
							status_registers[i]);
		}
	}

}
