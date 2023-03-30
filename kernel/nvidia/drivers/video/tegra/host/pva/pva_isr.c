/*
 * PVA ISR code
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

#include "pva-interface.h"
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/nvhost.h>

#include "pva_regs.h"
#include "pva.h"

#define PVA_MASK_LOW_16BITS 0xff

#ifdef CONFIG_TEGRA_T23X_GRHOST
#include "pva_isr_t23x.h"
#endif

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
		}

		/* For now, just log the errors */
		if (status5 & PVA_AISR_TASK_ERROR)
			nvpva_warn(&pdev->dev, "PVA AISR: PVA_AISR_TASK_ERROR");
		if (status5 & PVA_AISR_THRESHOLD_EXCEEDED)
			nvpva_warn(&pdev->dev, "PVA AISR: PVA_AISR_THRESHOLD_EXCEEDED");
		if (status5 & PVA_AISR_LOGGING_OVERFLOW)
			nvpva_warn(&pdev->dev, "PVA AISR: PVA_AISR_LOGGING_OVERFLOW");
		if (status5 & PVA_AISR_PRINTF_OVERFLOW)
			nvpva_warn(&pdev->dev, "PVA AISR: PVA_AISR_PRINTF_OVERFLOW");
		if (status5 & PVA_AISR_CRASH_LOG)
			nvpva_warn(&pdev->dev, "PVA AISR: PVA_AISR_CRASH_LOG");
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
		if (i == 0) {
			irq_handler = pva_system_isr;
		} else {
#ifdef CONFIG_TEGRA_T23X_GRHOST
			irq_handler = pva_ccq_isr;
#else
			pr_err("%s: invalid number of IRQs for build type\n",
			       __func__);
			err = -EINVAL;
			break;
#endif
		}
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
