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

#include <linux/kernel.h>
#include <linux/nvhost.h>
#include <linux/delay.h>

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif

#include "pva.h"
#include "pva_mailbox.h"
#include "pva_ccq_t23x.h"

#include "pva_regs.h"

#define MAX_CCQ_ELEMENTS 6

static int pva_ccq_wait(struct pva *pva, int timeout, unsigned int queue_id)
{
	unsigned long end_jiffies = jiffies + msecs_to_jiffies(timeout);
	/*
	 * Wait until there is free room in the CCQ. Otherwise the writes
	 * could stall the CPU. Ignore the timeout in simulation.
	 */

	while (time_before(jiffies, end_jiffies) ||
	       (pva->timeout_enabled == false)) {
		u32 val = PVA_EXTRACT(
			host1x_readl(pva->pdev,
				     cfg_ccq_status_r(pva->version, queue_id,
						      PVA_CCQ_STATUS2_INDEX)),
			4, 0, u32);
		if (val <= MAX_CCQ_ELEMENTS)
			return 0;

		usleep_range(5, 10);
	}

	return -ETIMEDOUT;
}

static int pva_ccq_send_cmd(struct pva *pva, u32 queue_id,
			    struct pva_cmd_s *cmd)
{
	int err = 0;
	err = pva_ccq_wait(pva, 100, queue_id);
	if (err < 0)
		goto err_wait_ccq;

	/* Make the writes to CCQ */
	host1x_writel(pva->pdev, cfg_ccq_r(pva->version, queue_id),
		      cmd->cmd_field[1]);
	host1x_writel(pva->pdev, cfg_ccq_r(pva->version, queue_id),
		      cmd->cmd_field[0]);
	return err;

err_wait_ccq:
	pva_abort(pva);
	return err;
}

int pva_ccq_send_task_t23x(struct pva *pva, u32 queue_id, dma_addr_t task_addr,
			   u8 batchsize, u32 flags)
{
	int err = 0;
	struct pva_cmd_s cmd = { 0 };

	(void)pva_cmd_submit_batch(&cmd, queue_id, task_addr, batchsize, flags);

	err = pva_ccq_send_cmd(pva, queue_id, &cmd);
	return err;
}

void pva_ccq_isr_handler(struct pva *pva, unsigned int queue_id)
{
	struct platform_device *pdev = pva->pdev;
	u32 int_status;
	unsigned int cmd_status_index = queue_id + PVA_CCQ0_INDEX;
	int_status =
		host1x_readl(pdev, cfg_ccq_status_r(pva->version, queue_id,
						    PVA_CCQ_STATUS7_INDEX));
	if (pva->cmd_status[cmd_status_index] != PVA_CMD_STATUS_WFI) {
		nvpva_warn(&pdev->dev, "No ISR for CCQ %u", queue_id);
		return;
	}
	/* Save the current command and subcommand for later processing */

	pva->version_config->read_status_interface(
		pva, cmd_status_index, int_status,
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
	u32 interface = queue_id + 1;
	/* Wait for the event being triggered in ISR */
	if (pva->timeout_enabled == true) {
		timeout = wait_event_timeout(
			pva->cmd_waitqueue[interface],
			pva->cmd_status[interface] == PVA_CMD_STATUS_DONE ||
				pva->cmd_status[interface] ==
					PVA_CMD_STATUS_ABORTED,
			msecs_to_jiffies(wait_time));
	} else {
		wait_event(pva->cmd_waitqueue[interface],
			   pva->cmd_status[interface] == PVA_CMD_STATUS_DONE ||
				   pva->cmd_status[interface] ==
					   PVA_CMD_STATUS_ABORTED);
	}
	if (timeout <= 0) {
		err = -ETIMEDOUT;
		pva_abort(pva);
	} else if (pva->cmd_status[interface] == PVA_CMD_STATUS_ABORTED)
		err = -EIO;
	else
		err = 0;
	return err;
}

int pva_ccq_send_cmd_sync(struct pva *pva, struct pva_cmd_s *cmd, u32 nregs,
			  u32 queue_id, struct pva_cmd_status_regs *status_regs)
{
	int err = 0;
	u32 interface = queue_id + 1U;

	if (status_regs == NULL) {
		err = -EINVAL;
		goto err_invalid_parameter;
	}

	if (queue_id >= MAX_PVA_QUEUE_COUNT) {
		err = -EINVAL;
		goto err_invalid_parameter;
	}

	/* Ensure that mailbox state is sane */
	if (WARN_ON(pva->cmd_status[interface] != PVA_CMD_STATUS_INVALID)) {
		err = -EIO;
		goto err_check_status;
	}

	/* Mark that we are waiting for an interrupt */
	pva->cmd_status[interface] = PVA_CMD_STATUS_WFI;
	memset(&pva->cmd_status_regs[interface], 0,
	       sizeof(struct pva_cmd_status_regs));

	/* Submit command to PVA */
	err = pva_ccq_send_cmd(pva, queue_id, cmd);
	if (err < 0)
		goto err_send_command;

	err = pva_ccq_wait_event(pva, queue_id, 100);
	if (err < 0)
		goto err_wait_response;
	/* Return interrupt status back to caller */
	memcpy(status_regs, &pva->cmd_status_regs[interface],
	       sizeof(struct pva_cmd_status_regs));

	pva->cmd_status[interface] = PVA_CMD_STATUS_INVALID;

	return err;

err_wait_response:
err_send_command:
	pva->cmd_status[interface] = PVA_CMD_STATUS_INVALID;
err_check_status:
err_invalid_parameter:
	return err;
}

int pva_send_cmd_sync(struct pva *pva, struct pva_cmd_s *cmd, u32 nregs,
		      u32 queue_id, struct pva_cmd_status_regs *status_regs)
{
	int err = 0;

	switch (pva->submit_cmd_mode) {
	case PVA_SUBMIT_MODE_MAILBOX:
		err = pva_mailbox_send_cmd_sync(pva, cmd, nregs, status_regs);
		break;
	case PVA_SUBMIT_MODE_MMIO_CCQ:
		err = pva_ccq_send_cmd_sync(pva, cmd, nregs, queue_id,
					    status_regs);
		break;
	}

	return err;
}

int pva_send_cmd_sync_locked(struct pva *pva, struct pva_cmd_s *cmd, u32 nregs,
			     u32 queue_id,
			     struct pva_cmd_status_regs *status_regs)
{
	int err = 0;

	switch (pva->submit_cmd_mode) {
	case PVA_SUBMIT_MODE_MAILBOX:
		err = pva_mailbox_send_cmd_sync_locked(pva, cmd, nregs,
						       status_regs);
		break;
	case PVA_SUBMIT_MODE_MMIO_CCQ:
		err = pva_ccq_send_cmd_sync(pva, cmd, nregs, queue_id,
					    status_regs);
		break;
	}

	return err;
}
