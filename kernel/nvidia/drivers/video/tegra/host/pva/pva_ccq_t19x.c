/*
 * PVA Command Queue Interface handling
 *
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include "pva-interface.h"
#include <linux/kernel.h>
#include <linux/nvhost.h>
#include <linux/delay.h>

#include <linux/version.h>
#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif

#include "pva.h"
#include "pva_ccq_t19x.h"

#include "pva_regs.h"
#include "pva-interface.h"

#define MAX_CCQ_ELEMENTS 6

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
				       cfg_ccq_status_r(pva->version, 0,
							PVA_CCQ_STATUS2_INDEX));
		if (val <= MAX_CCQ_ELEMENTS)
			return 0;

		usleep_range(5, 10);
	}

	return -ETIMEDOUT;
}

int pva_ccq_send_task_t19x(struct pva *pva, u32 queue_id, dma_addr_t task_addr,
			   u8 batchsize, u32 flags)
{
	int err = 0;
	struct pva_cmd_s cmd = {0};

	(void)pva_cmd_submit_batch(&cmd, queue_id, task_addr, batchsize, flags);

	mutex_lock(&pva->ccq_mutex);
	err = pva_ccq_wait(pva, 100);
	if (err < 0)
		goto err_wait_ccq;

	/* Make the writes to CCQ */
	host1x_writel(pva->pdev, cfg_ccq_r(pva->version, 0), cmd.cmd_field[1]);
	host1x_writel(pva->pdev, cfg_ccq_r(pva->version, 0), cmd.cmd_field[0]);

	mutex_unlock(&pva->ccq_mutex);

	return err;

err_wait_ccq:
	mutex_unlock(&pva->ccq_mutex);
	pva_abort(pva);

	return err;
}
