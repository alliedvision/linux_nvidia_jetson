/*
 * PVA ISR code for T23X
 *
 * Copyright (c) 2019-2022, NVIDIA Corporation.  All rights reserved.
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

#include "pva_isr_t23x.h"

#include <linux/irq.h>
#include <linux/nvhost.h>

#include "pva_regs.h"
#include "pva.h"
#include "pva_ccq_t23x.h"

#define PVA_MASK_LOW_16BITS 0xff

irqreturn_t pva_ccq_isr(int irq, void *dev_id)
{
	uint32_t int_status = 0, isr_status = 0, aisr_status = 0;
	unsigned int queue_id = MAX_PVA_QUEUE_COUNT + 1;
	int i;
	struct pva *pva = dev_id;
	struct platform_device *pdev = pva->pdev;
	bool recover = false;

	for (i = 1; i < MAX_PVA_IRQS; i++) {
		if (pva->irq[i] == irq) {
			queue_id = i - 1;
			break;
		}
	}
	if (queue_id == MAX_PVA_QUEUE_COUNT + 1) {
		printk("Invalid IRQ received. Returning from ISR");
		return IRQ_HANDLED;
	}
	nvpva_dbg_info(pva, "Received ISR from CCQ block, IRQ: %d", irq);
	int_status = host1x_readl(pdev, cfg_ccq_status_r(pva->version,
				queue_id, PVA_CCQ_STATUS2_INDEX))
				& ~PVA_MASK_LOW_16BITS;

	if (int_status != 0x0) {
		nvpva_dbg_info(pva, "Clear CCQ interrupt for %d, \
			       current status: 0x%x",
			       queue_id, int_status);
		host1x_writel(pdev,
			      cfg_ccq_status_r(pva->version, queue_id,
					       PVA_CCQ_STATUS2_INDEX),
			      int_status);
	}

	if (int_status & PVA_VALID_CCQ_ISR) {
		isr_status = host1x_readl(pdev, cfg_ccq_status_r(pva->version,
					queue_id, PVA_CCQ_STATUS7_INDEX));
	}
	if (int_status & PVA_VALID_CCQ_AISR) {
		aisr_status = host1x_readl(pdev, cfg_ccq_status_r(pva->version,
					queue_id, PVA_CCQ_STATUS8_INDEX));
	}
	if (aisr_status & PVA_AISR_INT_PENDING) {
		nvpva_dbg_info(pva, "PVA CCQ AISR (%x)", aisr_status);
		if (aisr_status &
		    (PVA_AISR_TASK_COMPLETE | PVA_AISR_TASK_ERROR)) {
			atomic_add(1, &pva->n_pending_tasks);
			queue_work(pva->task_status_workqueue,
				   &pva->task_update_work);
			if ((aisr_status & PVA_AISR_ABORT) == 0U)
				pva_push_aisr_status(pva, aisr_status);
		}

		/* For now, just log the errors */

		if (aisr_status & PVA_AISR_TASK_ERROR)
			nvpva_warn(&pdev->dev,
				    "PVA AISR: \
				    PVA_AISR_TASK_ERROR for queue id = %d",
				    queue_id);
		if (aisr_status & PVA_AISR_ABORT) {
			nvpva_warn(&pdev->dev, "PVA AISR: \
				PVA_AISR_ABORT for queue id = %d",
				queue_id);
			nvpva_warn(&pdev->dev, "Checkpoint value: 0x%08x",
				    aisr_status);
			recover = true;
		}
		/* Acknowledge AISR by writing status 1 */
		host1x_writel(pdev, cfg_ccq_status_r(pva->version, queue_id,
			      PVA_CCQ_STATUS1_INDEX), 0x01U);
	}
	if (isr_status & PVA_INT_PENDING) {
		pva_ccq_isr_handler(pva, queue_id);
	}
	if (recover)
		pva_abort(pva);

	return IRQ_HANDLED;
}
