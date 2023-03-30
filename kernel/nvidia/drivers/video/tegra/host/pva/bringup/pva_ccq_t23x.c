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
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif

#include "dev.h"
#include "pva.h"
#include "pva_mailbox.h"
#include "pva_ccq_t23x.h"

#include "pva_regs.h"

#define MAX_CCQ_ELEMENTS	6

static int pva_ccq_wait(struct pva *pva, int timeout, unsigned int queue_id)
{
	unsigned long end_jiffies = jiffies + msecs_to_jiffies(timeout);
	/*
	 * Wait until there is free room in the CCQ. Otherwise the writes
	 * could stall the CPU. Ignore the timeout in simulation.
	 */

	while (time_before(jiffies, end_jiffies) ||
	       (pva->timeout_enabled == false)) {
		u32 val = PVA_EXTRACT(host1x_readl(pva->pdev,
				cfg_ccq_status_r(pva->version,
				queue_id, PVA_CCQ_STATUS2_INDEX)),
				4, 0, u32);
		if (val <= MAX_CCQ_ELEMENTS)
			return 0;

		usleep_range(5, 10);
	}

	return -ETIMEDOUT;
}

int pva_ccq_send_task_t23x(struct pva *pva, struct pva_cmd *cmd)
{
	int err = 0;
	unsigned int queue_id = 0;

	queue_id = PVA_EXTRACT(cmd->mbox[0], 15, 8, unsigned int);

	err = pva_ccq_wait(pva, 100, queue_id);
	if (err < 0)
		goto err_wait_ccq;

	/* Make the writes to CCQ */
	host1x_writel(pva->pdev, cfg_ccq_r(pva->version, queue_id), cmd->mbox[1]);
	host1x_writel(pva->pdev, cfg_ccq_r(pva->version, queue_id), cmd->mbox[0]);
	host1x_writel(pva->pdev, cfg_ccq_r(pva->version, queue_id), cmd->mbox[3]);
	host1x_writel(pva->pdev, cfg_ccq_r(pva->version, queue_id), cmd->mbox[2]);
	return err;

err_wait_ccq:
	pva_abort(pva);

	return err;
}

void pva_ccq_isr_handler(struct pva *pva, unsigned int queue_id) {
	struct platform_device *pdev = pva->pdev;
	u32 int_status;
	unsigned int cmd_status_index = queue_id + PVA_CCQ0_INDEX;
	int_status = host1x_readl(pdev, cfg_ccq_status_r(pva->version, queue_id, PVA_CCQ_STATUS7_INDEX));
	if (pva->cmd_status[cmd_status_index] != PVA_CMD_STATUS_WFI) {
		nvhost_warn(&pdev->dev, "No ISR for CCQ %u", queue_id);
		return;
	}
	/* Save the current command and subcommand for later processing */

	pva->version_config->read_status_interface(pva, cmd_status_index, int_status,
						&pva->cmd_status_regs[cmd_status_index]);
	/* Clear the mailbox interrupt status */

	/* Wake up the waiters */
	pva->cmd_status[cmd_status_index] = PVA_CMD_STATUS_DONE;
	wake_up(&pva->cmd_waitqueue[cmd_status_index]);

}

int pva_ccq_wait_event(struct pva *pva, unsigned int queue_id, int wait_time)
{
	int timeout = 1;
	int err;

	/* Wait for the event being triggered in ISR */
	if (pva->timeout_enabled == true) {
		timeout = wait_event_timeout(pva->cmd_waitqueue[queue_id],
			pva->cmd_status[queue_id] == PVA_CMD_STATUS_DONE ||
			pva->cmd_status[queue_id] == PVA_CMD_STATUS_ABORTED,
			msecs_to_jiffies(wait_time));
	}
	else {
		wait_event(pva->cmd_waitqueue[queue_id],
			pva->cmd_status[queue_id] == PVA_CMD_STATUS_DONE ||
			pva->cmd_status[queue_id] == PVA_CMD_STATUS_ABORTED);
	}
	if (timeout <= 0) {
		err = -ETIMEDOUT;
		pva_abort(pva);
	} else if  (pva->cmd_status[queue_id] == PVA_CMD_STATUS_ABORTED)
		err = -EIO;
	else
		err = 0;
	return err;
}

int pva_ccq_send_cmd_sync(struct pva *pva,
			struct pva_cmd *cmd, u32 nregs,
			struct pva_cmd_status_regs *status_regs)
{
	int err = 0;
	unsigned int queue_id = 0;

	if (status_regs == NULL) {
		err = -EINVAL;
		goto err_invalid_parameter;
	}

	queue_id = PVA_EXTRACT(cmd->mbox[0], 15, 8, unsigned int) + PVA_CCQ0_INDEX;
	if (queue_id > PVA_CCQ7_INDEX) {
		err = -EINVAL;
		goto err_invalid_parameter;
	}

	/* Ensure that mailbox state is sane */
	if (WARN_ON(pva->cmd_status[queue_id] != PVA_CMD_STATUS_INVALID)) {
		err = -EIO;
		goto err_check_status;
	}

	/* Mark that we are waiting for an interrupt */
	pva->cmd_status[queue_id] = PVA_CMD_STATUS_WFI;
	memset(&pva->cmd_status_regs[queue_id], 0, sizeof(struct pva_cmd_status_regs));

	/* Submit command to PVA */
	err = pva_ccq_send_task_t23x(pva, cmd);
	if (err < 0)
		goto err_send_command;

	err = pva_ccq_wait_event(pva, queue_id, 100);
	if (err < 0)
		goto err_wait_response;
	/* Return interrupt status back to caller */
	memcpy(status_regs, &pva->cmd_status_regs[queue_id],
				sizeof(struct pva_cmd_status_regs));

	pva->cmd_status[queue_id] = PVA_CMD_STATUS_INVALID;

	return err;

err_wait_response:
err_send_command:
	pva->cmd_status[queue_id] = PVA_CMD_STATUS_INVALID;
err_check_status:
err_invalid_parameter:
	return err;
}

int pva_send_cmd_sync(struct pva *pva,
			struct pva_cmd *cmd, u32 nregs,
			struct pva_cmd_status_regs *status_regs)
{
	int err = 0;

	switch (pva->submit_cmd_mode) {
	case PVA_SUBMIT_MODE_MAILBOX:
		err = pva_mailbox_send_cmd_sync(pva, cmd, nregs, status_regs);
		break;
	case PVA_SUBMIT_MODE_MMIO_CCQ:
	case PVA_SUBMIT_MODE_CHANNEL_CCQ:
		err = pva_ccq_send_cmd_sync(pva, cmd, nregs, status_regs);
		break;
	}

	return err;
}
int pva_send_cmd_sync_locked(struct pva *pva,
			struct pva_cmd *cmd, u32 nregs,
			struct pva_cmd_status_regs *status_regs)
{
	int err = 0;

	switch (pva->submit_cmd_mode) {
	case PVA_SUBMIT_MODE_MAILBOX:
		err = pva_mailbox_send_cmd_sync_locked(pva,
						cmd, nregs, status_regs);
		break;
	case PVA_SUBMIT_MODE_MMIO_CCQ:
	case PVA_SUBMIT_MODE_CHANNEL_CCQ:
		err = pva_ccq_send_cmd_sync(pva, cmd, nregs, status_regs);
		break;
	}

	return err;

}

