/*
 * PVA ISR code
 *
 * Copyright (c) 2016-2023, NVIDIA Corporation.  All rights reserved.
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

#define PVA_MASK_LOW_16BITS 0xff

#include "pva-interface.h"
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/nvhost.h>
#include "pva_regs.h"
#include "pva.h"
#include "pva_isr_t23x.h"

void pva_push_aisr_status(struct pva *pva, uint32_t aisr_status)
{
	struct pva_task_error_s *err_array = pva->priv_circular_array.va;
	struct pva_task_error_s *src_va = &err_array[pva->circular_array_wr_pos];

	src_va->queue = PVA_GET_QUEUE_ID_FROM_STATUS(aisr_status);
	src_va->vpu = PVA_GET_VPU_ID_FROM_STATUS(aisr_status);
	src_va->error = PVA_GET_ERROR_FROM_STATUS(aisr_status);
	src_va->task_id = PVA_GET_TASK_ID_FROM_STATUS(aisr_status);
	src_va->valid = 1U;

	if (pva->circular_array_wr_pos == (MAX_PVA_TASK_COUNT-1))
		pva->circular_array_wr_pos = 0;
	else
		pva->circular_array_wr_pos += 1;
}

static irqreturn_t pva_system_isr(int irq, void *dev_id)
{
	struct pva *pva = dev_id;
	struct platform_device *pdev = pva->pdev;
	u32 checkpoint = host1x_readl(pdev,
		cfg_ccq_status_r(pva->version, 0, 8));
	u32 status7 = pva->version_config->read_mailbox(pdev, PVA_MBOX_ISR);
	u32 status5 = pva->version_config->read_mailbox(pdev, PVA_MBOX_AISR);
	u32 lic_int_status = host1x_readl(pdev,
		sec_lic_intr_status_r(pva->version));
	u32 h1xflgs;
	bool recover = false;

	if (status5 & PVA_AISR_INT_PENDING) {
		nvpva_dbg_info(pva, "PVA AISR (%x)", status5);

		if (status5 & (PVA_AISR_TASK_COMPLETE | PVA_AISR_TASK_ERROR)) {
			atomic_add(1, &pva->n_pending_tasks);
			queue_work(pva->task_status_workqueue,
				   &pva->task_update_work);
			if ((status5 & PVA_AISR_ABORT) == 0U)
				pva_push_aisr_status(pva, status5);
		}

		/* For now, just log the errors */
		if (status5 & PVA_AISR_TASK_ERROR)
			nvpva_warn(&pdev->dev, "PVA AISR: PVA_AISR_TASK_ERROR");
		if (status5 & PVA_AISR_ABORT) {
			nvpva_warn(&pdev->dev, "PVA AISR: PVA_AISR_ABORT");
			nvpva_warn(&pdev->dev, "Checkpoint value: 0x%08x",
				    checkpoint);
			recover = true;
		}

		pva->version_config->write_mailbox(pdev, PVA_MBOX_AISR, 0x0);
	}

	if (status7 & PVA_INT_PENDING) {
		nvpva_dbg_info(pva, "PVA ISR (%x)", status7);

		pva_mailbox_isr(pva);
	}


	/* Check for watchdog timer interrupt */
	if (lic_int_status & sec_lic_intr_enable_wdt_f(SEC_LIC_INTR_WDT)) {
		nvpva_warn(&pdev->dev, "WatchDog Timer");
		recover = true;
	}

	/* Check for host1x errors*/
	if (pva->version == PVA_HW_GEN1) {
		h1xflgs = sec_lic_intr_enable_h1x_f(SEC_LIC_INTR_H1X_ALL_19);
	} else {
		h1xflgs = sec_lic_intr_enable_h1x_f(SEC_LIC_INTR_H1X_ALL_23);
	}
	if (lic_int_status & h1xflgs) {
		nvpva_warn(&pdev->dev, "Pva Host1x errors (0x%x)",
			     lic_int_status);

		/* Clear the interrupt */
		host1x_writel(pva->pdev, sec_lic_intr_status_r(pva->version),
			      (lic_int_status & h1xflgs));
		recover = true;
	}

	/* Copy trace points to ftrace buffer */
	pva_trace_copy_to_ftrace(pva);

	if (recover)
		pva_abort(pva);

	return IRQ_HANDLED;
}

int pva_register_isr(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	struct pva *pva = pdata->private_data;
	int err;
	int i;
	irq_handler_t irq_handler;

	for (i = 0; i < pva->version_config->irq_count; i++) {
		pva->irq[i] = platform_get_irq(dev, i);
		if (pva->irq[i] <= 0) {
			dev_err(&dev->dev, "no irq %d\n", i);
			err = -ENOENT;
			break;
		}

		/* IRQ0 is for mailbox/h1x/watchdog */
		if (i == 0)
			irq_handler = pva_system_isr;
		else
			irq_handler = pva_ccq_isr;

		err = request_threaded_irq(pva->irq[i], NULL, irq_handler,
					IRQF_ONESHOT, "pva-isr", pva);
		if (err) {
			pr_err("%s: request_irq(%d) failed(%d)\n", __func__,
				pva->irq[i], err);
			break;
		}
		disable_irq(pva->irq[i]);
	}
	return err;
}
