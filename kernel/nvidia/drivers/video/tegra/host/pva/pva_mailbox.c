/*
 * PVA mailbox code
 *
 * Copyright (c) 2016-2022, NVIDIA Corporation.  All rights reserved.
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

#include "pva.h"
#include "pva_mailbox.h"
#include "pva-interface.h"

static u32 pva_get_mb_reg_id(u32 i)
{
	u32 mb_reg_id[VALID_MB_INPUT_REGS] = {
		0,
		1,
		2,
		3
	};

	return mb_reg_id[i];
}

static int pva_mailbox_send_cmd(struct pva *pva, struct pva_cmd_s *cmd,
				u32 nregs)
{
	struct platform_device *pdev = pva->pdev;
	u32 reg, status;
	int i;

	if (nregs > VALID_MB_INPUT_REGS) {
		pr_err("%s nregs %d more than expected\n", __func__, nregs);
		return -EINVAL;
	}

	/* Make sure the state is what we expect it to be. */
	status = pva->version_config->read_mailbox(pdev, PVA_MBOX_ISR);

	WARN_ON((status & PVA_INT_PENDING));
	WARN_ON((status & PVA_READY) == 0);
	WARN_ON((status & PVA_BUSY));

	/*set MSB of mailbox 0 to trigger FW interrupt*/
	cmd->cmd_field[0] |= PVA_BIT(31);
	/* Write all of the other command mailbox
	 * registers before writing mailbox 0.
	 */
	for (i = (nregs - 1); i >= 0; i--) {
		reg = pva_get_mb_reg_id(i);
		pva->version_config->write_mailbox(pdev, reg, cmd->cmd_field[i]);
	}

	return 0;
}

int pva_mailbox_wait_event(struct pva *pva, int wait_time)
{
	int timeout = 1;
	int err;
	/* Wait for the event being triggered in ISR */
	if (pva->timeout_enabled == true)
		timeout = wait_event_timeout(
			pva->cmd_waitqueue[PVA_MAILBOX_INDEX],
			pva->cmd_status[PVA_MAILBOX_INDEX] ==
						PVA_CMD_STATUS_DONE ||
			pva->cmd_status[PVA_MAILBOX_INDEX] ==
						PVA_CMD_STATUS_ABORTED,
			msecs_to_jiffies(wait_time));
	else
		wait_event(pva->cmd_waitqueue[PVA_MAILBOX_INDEX],
			pva->cmd_status[PVA_MAILBOX_INDEX] ==
						PVA_CMD_STATUS_DONE ||
			pva->cmd_status[PVA_MAILBOX_INDEX] ==
						PVA_CMD_STATUS_ABORTED);

	if (timeout <= 0) {
		err = -ETIMEDOUT;
		pva_abort(pva);
	} else if  (pva->cmd_status[PVA_MAILBOX_INDEX] ==
						PVA_CMD_STATUS_ABORTED)
		err = -EIO;
	else
		err = 0;

	return err;
}

void pva_mailbox_isr(struct pva *pva)
{
	struct platform_device *pdev = pva->pdev;
	u32 int_status = pva->version_config->read_mailbox(pdev, PVA_MBOX_ISR);
	if (pva->cmd_status[PVA_MAILBOX_INDEX] != PVA_CMD_STATUS_WFI) {
		nvpva_warn(&pdev->dev, "Unexpected PVA ISR (%x)", int_status);
		return;
	}

	/* Save the current command and subcommand for later processing */
	pva->cmd_status_regs[PVA_MAILBOX_INDEX].cmd =
			pva->version_config->read_mailbox(pdev,
							PVA_MBOX_COMMAND);
	pva->version_config->read_status_interface(pva,
				PVA_MAILBOX_INDEX, int_status,
				&pva->cmd_status_regs[PVA_MAILBOX_INDEX]);
	/* Clear the mailbox interrupt status */
	int_status = int_status & PVA_READY;
	pva->version_config->write_mailbox(pdev, PVA_MBOX_ISR, int_status);

	/* Wake up the waiters */
	pva->cmd_status[PVA_MAILBOX_INDEX] = PVA_CMD_STATUS_DONE;
	wake_up(&pva->cmd_waitqueue[PVA_MAILBOX_INDEX]);
}

int pva_mailbox_send_cmd_sync_locked(struct pva *pva,
			struct pva_cmd_s *cmd, u32 nregs,
			struct pva_cmd_status_regs *status_regs)
{
	int err = 0;

	if (status_regs == NULL) {
		err = -EINVAL;
		goto err_invalid_parameter;
	}

	/* Ensure that mailbox state is sane */
	if (WARN_ON(pva->cmd_status[PVA_MAILBOX_INDEX] !=
			PVA_CMD_STATUS_INVALID)) {
		err = -EIO;
		goto err_check_status;
	}

	/* Mark that we are waiting for an interrupt */
	pva->cmd_status[PVA_MAILBOX_INDEX] = PVA_CMD_STATUS_WFI;
	memset(&pva->cmd_status_regs, 0, sizeof(pva->cmd_status_regs));

	/* Submit command to PVA */
	err = pva_mailbox_send_cmd(pva, cmd, nregs);
	if (err < 0)
		goto err_send_command;

	err = pva_mailbox_wait_event(pva, 100);
	if (err < 0)
		goto err_wait_response;

	/* Return interrupt status back to caller */
	memcpy(status_regs, &pva->cmd_status_regs,
				sizeof(struct pva_cmd_status_regs));

	pva->cmd_status[PVA_MAILBOX_INDEX] = PVA_CMD_STATUS_INVALID;
	return err;

err_wait_response:
err_send_command:
	pva->cmd_status[PVA_MAILBOX_INDEX] = PVA_CMD_STATUS_INVALID;
err_check_status:
err_invalid_parameter:
	return err;
}

int pva_mailbox_send_cmd_sync(struct pva *pva,
			struct pva_cmd_s *cmd, u32 nregs,
			struct pva_cmd_status_regs *status_regs)
{
	int err = 0;

	if (status_regs == NULL) {
		err = -EINVAL;
		goto err_invalid_parameter;
	}

	mutex_lock(&pva->mailbox_mutex);
	err = pva_mailbox_send_cmd_sync_locked(pva,
					       cmd,
					       nregs,
					       status_regs);
	mutex_unlock(&pva->mailbox_mutex);

	return err;

err_invalid_parameter:
	return err;
}


EXPORT_SYMBOL(pva_mailbox_send_cmd_sync);
