/*
 * PVA Command Queue Interface handling
 *
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


#include <linux/kernel.h>
#include <linux/nvhost.h>
#include <linux/delay.h>

#include <linux/version.h>
#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif

#include "dev.h"
#include "pva.h"
#include "pva_ccq_t19x.h"

#include "pva_regs.h"

#define MAX_CCQ_ELEMENTS	6

static int pva_ccq_wait(struct pva *pva, int timeout)
{
	unsigned long end_jiffies = jiffies + msecs_to_jiffies(timeout);
	/*
	 * Wait until there is free room in the CCQ. Otherwise the writes
	 * could stall the CPU. Ignore the timeout in simulation.
	 */

	while (time_before(jiffies, end_jiffies) ||
	       (pva->timeout_enabled == false)) {
		u32 val = host1x_readl(pva->pdev,
					cfg_ccq_status_r(pva->version,
					0, PVA_CCQ_STATUS2_INDEX));
		if (val <= MAX_CCQ_ELEMENTS)
			return 0;

		usleep_range(5, 10);
	}

	return -ETIMEDOUT;
}

int pva_ccq_send_task_t19x(struct pva *pva, struct pva_cmd *cmd)
{
	int err = 0;
	u64 fifo_cmd;
	u64 fifo_flags;
	unsigned int queue_id;
	uint64_t address;

	queue_id = PVA_EXTRACT(cmd->mbox[0], 15, 8, unsigned int);
	address = PVA_INSERT64(PVA_EXTRACT(cmd->mbox[0], 23, 16, uint32_t),
		39, 32) | PVA_INSERT64(cmd->mbox[1], 32, 0);

	fifo_flags = cmd->mbox[0] &
		    (PVA_CMD_INT_ON_ERR | PVA_CMD_INT_ON_COMPLETE)
		    >> PVA_CMD_MBOX_TO_FIFO_FLAG_SHIFT;

	fifo_cmd = pva_fifo_submit(queue_id, address, fifo_flags);
	mutex_lock(&pva->ccq_mutex);
	err = pva_ccq_wait(pva, 100);
	if (err < 0)
		goto err_wait_ccq;

	/* Make the writes to CCQ */
	host1x_writel(pva->pdev,
		cfg_ccq_r(pva->version, 0), (u32)(fifo_cmd >> 32));
	host1x_writel(pva->pdev,
		cfg_ccq_r(pva->version, 0), (u32)(fifo_cmd & 0xffffffff));

	mutex_unlock(&pva->ccq_mutex);

	return err;

err_wait_ccq:
	mutex_unlock(&pva->ccq_mutex);
	pva_abort(pva);

	return err;
}
