/*
 * Copyright (c) 2016-2021, NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved.
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

#include "pva_mailbox.h"
#include <linux/workqueue.h>
#include "nvpva_client.h"
#include <linux/export.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/iommu.h>
#include <linux/reset.h>
#include <linux/version.h>
#include <linux/platform/tegra/common.h>
#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
#include <soc/tegra/chip-id.h>
#endif
#include <soc/tegra/fuse.h>

#include "nvhost_syncpt_unit_interface.h"
#include "dev.h"
#include "bus_client.h"
#include "nvhost_acm.h"
#include "t194/t194.h"
#ifdef CONFIG_TEGRA_T23X_GRHOST
#include "t23x/t23x.h"
#include "pva_mailbox_t23x.h"
#include "pva_interface_regs_t23x.h"
#include "pva_version_config_t23x.h"
#include "pva_ccq_t23x.h"
#endif
#include "nvpva_queue.h"
#include "pva_queue.h"
#include "pva.h"
#include "pva_regs.h"
#include "pva_mailbox_t19x.h"
#include "pva_interface_regs_t19x.h"
#include "pva_version_config_t19x.h"
#include "pva_ccq_t19x.h"
#include "class_ids_t194.h"
#include "pva-ucode-header.h"

/* Map PVA-A and PVA-B to respective configuration items in nvhost */
static struct of_device_id tegra_pva_of_match[] = {
	{
		.name = "pva0",
		.compatible = "nvidia,tegra194-pva",
		.data = (struct nvhost_device_data *)&t19_pva0_info },
	{
		.name = "pva1",
		.compatible = "nvidia,tegra194-pva",
		.data = (struct nvhost_device_data *)&t19_pva1_info },
#ifdef CONFIG_TEGRA_T23X_GRHOST
	{
		.name = "pva0",
		.compatible = "nvidia,tegra234-pva",
		.data = (struct nvhost_device_data *)&t23x_pva0_info },
#endif
	{ },
};

#define EVP_REG_NUM 8
static u32 pva_get_evp_reg(u32 index)
{
	u32 evp_reg[EVP_REG_NUM] = {
		evp_reset_addr_r(),
		evp_undef_addr_r(),
		evp_swi_addr_r(),
		evp_prefetch_abort_addr_r(),
		evp_data_abort_addr_r(),
		evp_rsvd_addr_r(),
		evp_irq_addr_r(),
		evp_fiq_addr_r()
	};

	return evp_reg[index];
}

/**
 * Allocate and set a circular array for FW to provide status info about
 * completed tasks from all the PVA R5 queues.
 * To avoid possible overwrite of info, the size of circular array needs to be
 * sufficient to hold the status info for maximum allowed number of tasks
 * across all PVA R5 queues at any time.
 * PVA R5 FW shall fill task status info at incremental positions in the array
 * while PVA KMD shall read the task status info at incremental positions from
 * the array.
 * Both PVA R5 FW and PVA KMD shall independently maintain an internal index
 * to dictate the current write position and read position respectively.
 */
static int pva_alloc_task_status_buffer(struct pva *pva)
{
	size_t min_size = 0U;

	/* Determine worst case size required for circular array based on
	 * maximum allowed per PVA engine and maximum allowed number of task
	 * submissions per PVA queue at any time.
	 */
	min_size = MAX_PVA_TASK_COUNT * sizeof(struct pva_task_error_s);

	pva->priv_circular_array.size = ALIGN(min_size + 64, 64);

	pva->priv_circular_array.va =
		dma_alloc_coherent(&pva->pdev->dev,
				   pva->priv_circular_array.size,
				   &pva->priv_circular_array.pa, GFP_KERNEL);

	if (pva->priv_circular_array.va == NULL) {
		pr_err("pva: failed to alloc mem for task status info");
		return -ENOMEM;
	}

	INIT_WORK(&pva->task_update_work, pva_task_update);

	atomic_set(&pva->n_pending_tasks, 0);
	pva->task_status_workqueue =
		create_workqueue("pva_task_status_workqueue");
	return 0;
}

static void pva_reset_task_status_buffer(struct pva *pva)
{
	flush_workqueue(pva->task_status_workqueue);
	WARN_ON(atomic_read(&pva->n_pending_tasks) != 0);
	atomic_set(&pva->n_pending_tasks, 0);
	pva->circular_array_rd_pos = 0U;
}

static void pva_free_task_status_buffer(struct pva *pva)
{
	flush_workqueue(pva->task_status_workqueue);
	destroy_workqueue(pva->task_status_workqueue);
	dma_free_coherent(&pva->pdev->dev, pva->priv_circular_array.size,
			  pva->priv_circular_array.va,
			  pva->priv_circular_array.pa);
}


int nvpva_set_task_status_buffer(struct pva *pva)
{
	struct pva_cmd_s cmd = {};
	struct pva_cmd_status_regs status = {};
	uint32_t flags = PVA_CMD_INT_ON_ERR | PVA_CMD_INT_ON_COMPLETE;
	uint32_t nregs = 0U;
	int32_t err = 0;

	/* clear for debugging */
	(void)memset(pva->priv_circular_array.va, 0,
		     pva->priv_circular_array.size);

	nregs = pva_cmd_set_status_buffer(&cmd, pva->priv_circular_array.pa,
					  MAX_PVA_TASK_COUNT, flags);

	err = pva_mailbox_send_cmd_sync_locked(pva, &cmd, nregs, &status);
	if (err || (status.error != (uint32_t)PVA_ERR_NO_ERROR)) {
		pr_err("pva: failed to configure task status info buffer: %d, %d",
		       err, status.error);
		return -EINVAL;
	}

	return 0;

}

/* Default buffer size (256 kbytes) used for ucode trace log*/
#define PVA_PRIV2_TRACE_LOG_BUFFER_SIZE 0x40000

#define R5_USER_SEGREG_OFFSET 0x40000000
static int pva_init_fw(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct pva *pva = pdata->private_data;
	struct pva_fw *fw_info = &pva->fw_info;
	struct pva_dma_alloc_info *priv1_buffer;
	struct pva_dma_alloc_info *priv2_buffer;
	u32 *ucode_ptr;
	int err = 0, w;
	u64 ucode_useg_addr;
	int sema_value = 0;

	nvhost_dbg_fn("");

	priv1_buffer = &fw_info->priv1_buffer;
	priv2_buffer = &fw_info->priv2_buffer;
	ucode_ptr = priv1_buffer->va;

	/* Set the Ucode Header address for R5 */
	/* Program user seg subtracting the offset */
	ucode_useg_addr = priv1_buffer->pa - R5_USER_SEGREG_OFFSET;
	host1x_writel(pdev, cfg_r5user_lsegreg_r(pva->version),
		      PVA_LOW32(ucode_useg_addr));
	host1x_writel(pdev, cfg_r5user_usegreg_r(pva->version),
		      PVA_EXTRACT64(ucode_useg_addr, 39, 32, u32));

	/* Program the extra memory to be used by R5 */
	ucode_useg_addr = priv2_buffer->pa - fw_info->priv2_reg_offset;
	host1x_writel(pdev, cfg_priv_ar2_start_r(pva->version),
		      fw_info->priv2_reg_offset);
	host1x_writel(pdev, cfg_priv_ar2_end_r(pva->version),
		      fw_info->priv2_reg_offset + priv2_buffer->size);
	host1x_writel(pdev, cfg_priv_ar2_lsegreg_r(pva->version),
		      PVA_LOW32(ucode_useg_addr));
	host1x_writel(pdev, cfg_priv_ar2_usegreg_r(pva->version),
		      PVA_EXTRACT64(ucode_useg_addr, 39, 32, u32));

	/* check the type of segments and their offset and address */
	for (w = 0; w < fw_info->hdr->nsegments; w++) {
		struct pva_ucode_seg_s *useg =
			(struct pva_ucode_seg_s *)((void *)ucode_ptr +
						   PVA_UCODE_SEG_HDR_LENGTH +
						   (PVA_UCODE_SEG_HDR_LENGTH *
						    w));

		switch (useg->type) {
		case PVA_UCODE_SEG_EVP: {
			/* First 32 bytes of the EVP payload are zeros.
			 * so skip first 32 bytes
			 */
			u32 *evpmem =
				(u32 *)((u8 *)ucode_ptr + useg->offset + 32);
			u32 i;

			for (i = 0; i < EVP_REG_NUM; i++)
				host1x_writel(pdev, pva_get_evp_reg(i),
					      evpmem[i]);
			break;
		}

		case PVA_UCODE_SEG_R5: {
			/* Subracting PRIV1 start for R5PRIV1 address */
			const u64 seg_addr = priv1_buffer->pa - useg->addr;
			/* Calculate segment start address */
			const u64 useg_addr = seg_addr + useg->offset;
			const u32 useg_addr_low = PVA_LOW32(useg_addr);
			const u32 useg_addr_high =
				PVA_EXTRACT64((useg_addr), 39, 32, u32);
			/* Calculate ar1 base and limit */
			const u32 ar1_start = useg->addr;
			const u32 ar1_end =
				useg->addr + priv1_buffer->size - useg->offset;

			host1x_writel(pdev, cfg_priv_ar1_start_r(pva->version),
				      ar1_start);
			host1x_writel(pdev, cfg_priv_ar1_end_r(pva->version),
				      ar1_end);
			host1x_writel(pdev,
				      cfg_priv_ar1_lsegreg_r(pva->version),
				      useg_addr_low);
			host1x_writel(pdev,
				      cfg_priv_ar1_usegreg_r(pva->version),
				      useg_addr_high);
			break;
		}
		}
	}

	/* Indicate the OS is waiting for PVA ready Interrupt */
	pva->cmd_status[PVA_MAILBOX_INDEX] = PVA_CMD_STATUS_WFI;

	if (pva->r5_dbg_wait) {
		sema_value = PVA_WAIT_DEBUG;
		pva->timeout_enabled = false;
	}

	if (pva->slcg_disable)
		sema_value |= PVA_CG_DISABLE;

	if (pva->vmem_war_disable)
		sema_value |= PVA_VMEM_RD_WAR_DISABLE;

	sema_value |= (PVA_BOOT_INT | PVA_TEST_WAIT | PVA_VMEM_MBX_WAR_ENABLE);
	host1x_writel(pdev, hsp_ss0_set_r(), sema_value);

	/* Take R5 out of reset */
	host1x_writel(pdev, proc_cpuhalt_r(),
		      proc_cpuhalt_ncpuhalt_f(proc_cpuhalt_ncpuhalt_done_v()));

	nvhost_dbg_fn("Waiting for PVA to be READY");

	/* Wait PVA to report itself as ready */
	err = pva_mailbox_wait_event(pva, 60000);
	if (err)
		goto wait_timeout;

	pva->cmd_status[PVA_MAILBOX_INDEX] = PVA_CMD_STATUS_INVALID;

	nvhost_dbg_fn("PVA boot returned: %d", err);

	/* Check the ucode is with testmode enabled */
	if ((host1x_readl(pdev, hsp_ss0_state_r()) & PVA_TEST_MODE))
		err = pva_run_ucode_selftest(pdev);

	pva_reset_task_status_buffer(pva);
	err = nvpva_set_task_status_buffer(pva);
wait_timeout:
	return err;
}

static int pva_free_fw(struct platform_device *pdev, struct pva *pva)
{
	struct pva_fw *fw_info = &pva->fw_info;

	if (pva->priv1_dma.va)
		dma_free_coherent(&pdev->dev, pva->priv1_dma.size,
				  pva->priv1_dma.va, pva->priv1_dma.pa);

	if (pva->priv2_dma.va)
		dma_free_coherent(&pdev->dev, pva->priv2_dma.size,
				  pva->priv2_dma.va, pva->priv2_dma.pa);

	memset(fw_info, 0, sizeof(struct pva_fw));

	return 0;
}

/*
 * NO IOMMU set 0x60000000 as start address.
 * With IOMMU set 0x80000000(>2GB) as startaddress
 */
#define DRAM_PVA_IOVA_START_ADDRESS 0x80000000
#define DRAM_PVA_NO_IOMMU_START_ADDRESS 0x60000000

static int pva_read_ucode(struct platform_device *pdev, const char *fw_name,
			  struct pva *pva)
{
	int err = 0, w;
	u32 *ucode_ptr;
	const struct firmware *ucode_fw;
	struct pva_fw *fw_info = &pva->fw_info;
	struct pva_trace_log *trace = &pva->pva_trace;
	u32 segment_end_addr = 0U;

	nvhost_dbg_fn("loading pva fw:%s", fw_name);

	ucode_fw = nvhost_client_request_firmware(pdev, fw_name, true);
	if (!ucode_fw) {
		nvhost_dbg_fn("pva firmware request failed");
		dev_err(&pdev->dev, "Failed to load the %s firmware\n",
			fw_name);
		err = -ENOENT;
		return err;
	}

	/* set to default size, will add support to modify through debugfs */
	fw_info->trace_buffer_size = PVA_PRIV2_TRACE_LOG_BUFFER_SIZE;

	fw_info->priv1_buffer.size = ucode_fw->size;

	/* Make sure the address is aligned to 4K */
	pva->priv1_dma.size = ALIGN(fw_info->priv1_buffer.size + SZ_4K, SZ_4K);

	/* Allocate memory to R5 for app code, data or to log information */
	pva->priv1_dma.va = dma_alloc_coherent(&pdev->dev, pva->priv1_dma.size,
					       &pva->priv1_dma.pa, GFP_KERNEL);

	if (!pva->priv1_dma.va) {
		err = -ENOMEM;
		goto clean_up;
	}

	/* Make sure the buffer allocated to R5 are 4K aligned */
	fw_info->priv1_buffer.va = (void *)ALIGN((u64)pva->priv1_dma.va, SZ_4K);
	fw_info->priv1_buffer.pa =
		(dma_addr_t)ALIGN((u64)pva->priv1_dma.pa, SZ_4K);

	ucode_ptr = fw_info->priv1_buffer.va;

	/* copy the whole thing taking into account endianness */
	for (w = 0; w < ucode_fw->size / sizeof(u32); w++)
		ucode_ptr[w] = le32_to_cpu(((__le32 *)ucode_fw->data)[w]);

	/* set the header location accordingly */
	fw_info->hdr = (struct pva_ucode_hdr_s *)ucode_ptr;

	/* check for the magic number  and header version*/
	if ((fw_info->hdr->magic != PVA_HDR_MAGIC) &&
	    (fw_info->hdr->hdr_version != PVA_HDR_VERSION)) {
		dev_err(&pdev->dev, "Wrong PVA uCode header magic/version\n");
		err = -EINVAL;
	}

	/* find the size needed for priv2 buffer allocation */
	/* check the type of segments and their offset and address */
	for (w = 0; w < fw_info->hdr->nsegments; w++) {
		struct pva_ucode_seg_s *useg =
			(struct pva_ucode_seg_s *)((void *)ucode_ptr +
						   PVA_UCODE_SEG_HDR_LENGTH +
						   (PVA_UCODE_SEG_HDR_LENGTH *
						    w));

		switch (useg->type) {
		case PVA_UCODE_SEG_DRAM_CACHED:
			/* Total 2GB of contiguous memory for cache
			 * Set the DRAM CACHE physical addr as iova start
			 */
			if (iommu_get_domain_for_dev(&pdev->dev))
				useg->phys_addr = DRAM_PVA_IOVA_START_ADDRESS;
			else
				useg->phys_addr =
					DRAM_PVA_NO_IOMMU_START_ADDRESS;
			break;
		case PVA_UCODE_SEG_DRAM_UNCACHED:
			/* Set the Uncache size as Zero */
			useg->size = 0;
			break;
		case PVA_UCODE_SEG_R5_OVERLAY:
		case PVA_UCODE_SEG_CRASHDUMP:
			fw_info->priv2_buffer.size += useg->size;
			break;
		case PVA_UCODE_SEG_TRACE_LOG:
			/* set the trace log buffer offset from priv2 start
			 * offset must be 64bytes aligned for dma usage
			 */
			fw_info->priv2_buffer.size =
				ALIGN(fw_info->priv2_buffer.size + 64, 64);

			/* set the trace log buffer offset from priv2 start */
			useg->offset = fw_info->priv2_buffer.size;
			/* set os specified size if uCode passes zero size */
			if (!useg->size)
				useg->size = fw_info->trace_buffer_size;

			useg->addr = ALIGN(useg->addr + 64, 64);
			trace->size = useg->size;
			trace->offset = useg->offset;

			fw_info->priv2_buffer.size += useg->size;
			segment_end_addr = useg->addr + useg->size;
			break;
		case PVA_UCODE_SEG_CODE_COVERAGE:
			fw_info->priv2_buffer.size =
				ALIGN(fw_info->priv2_buffer.size + 64, 64);

			useg->addr = ALIGN(segment_end_addr + 64, 64);
			fw_info->priv2_buffer.size += useg->size;
			segment_end_addr = useg->addr + useg->size;
			break;
		case PVA_UCODE_SEG_DEBUG_LOG:
			fw_info->priv2_buffer.size =
				ALIGN(fw_info->priv2_buffer.size + 64, 64);

			useg->addr = ALIGN(segment_end_addr + 64, 64);
			fw_info->priv2_buffer.size += useg->size;
			segment_end_addr = useg->addr + useg->size;
			break;
		default:
			break;
		}
	}

	/* Make sure the address is aligned to 4K */
	pva->priv2_dma.size = ALIGN(fw_info->priv2_buffer.size + SZ_4K, SZ_4K);

	/* Allocate memory to R5 for app code, data or to log information */
	pva->priv2_dma.va = dma_alloc_coherent(&pdev->dev, pva->priv2_dma.size,
					       &pva->priv2_dma.pa, GFP_KERNEL);

	if (!pva->priv2_dma.va) {
		err = -ENOMEM;
		goto clean_up;
	}

	/* Make sure the buffer allocated to R5 are 4K aligned */
	fw_info->priv2_buffer.va = (void *)ALIGN((u64)pva->priv2_dma.va, SZ_4K);

	trace->addr = (void *)((u8 *)fw_info->priv2_buffer.va + trace->offset);
	memset(trace->addr, 0, trace->size);

	fw_info->priv2_buffer.pa =
		(dma_addr_t)ALIGN((u64)pva->priv2_dma.pa, SZ_4K);

	/* set the crashdump offsets and addresses */
	for (w = 0; w < fw_info->hdr->nsegments; w++) {
		struct pva_seg_info *seg_info = NULL;
		struct pva_ucode_seg_s *useg =
			(struct pva_ucode_seg_s *)((void *)ucode_ptr +
						   PVA_UCODE_SEG_HDR_LENGTH +
						   (PVA_UCODE_SEG_HDR_LENGTH *
						    w));
		int offset = useg->addr - fw_info->priv2_reg_offset;

		switch (useg->type) {
		case PVA_UCODE_SEG_R5_OVERLAY:
			fw_info->priv2_reg_offset = useg->addr;
			break;
		case PVA_UCODE_SEG_CRASHDUMP:
			break;
		default:
			break;
		}

		if (seg_info) {
			seg_info->offset = offset;
			seg_info->size = useg->size;
			seg_info->addr =
				(void *)((u8 *)fw_info->priv2_buffer.va +
					 offset);
		}
	}
clean_up:
	release_firmware(ucode_fw);
	return err;
}

static int pva_load_fw(struct platform_device *pdev)
{
	int err = 0;
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct pva *pva = pdata->private_data;

	err = pva_read_ucode(pdev, pdata->firmware_name, pva);
	if (err < 0)
		goto load_fw_err;

	return err;

load_fw_err:
	pva_free_fw(pdev, pva);
	return err;
}

int pva_get_firmware_version(struct pva *pva, struct pva_version_info *info)
{
	uint32_t flags = PVA_CMD_INT_ON_ERR | PVA_CMD_INT_ON_COMPLETE;
	struct pva_cmd_status_regs status;
	struct pva_cmd_s cmd;
	int err = 0;
	u32 nregs;

	nregs = pva_cmd_R5_version(&cmd, flags);

	/* Submit request to PVA and wait for response */
	err = pva_mailbox_send_cmd_sync(pva, &cmd, nregs, &status);
	if (err < 0) {
		nvhost_warn(&pva->pdev->dev,
			    "mbox get firmware version cmd failed: %d\n", err);

		return err;
	}

	info->pva_r5_version = status.status[PVA_CMD_STATUS4_INDEX];
	info->pva_compat_version = status.status[PVA_CMD_STATUS5_INDEX];
	info->pva_revision = status.status[PVA_CMD_STATUS6_INDEX];
	info->pva_built_on = status.status[PVA_CMD_STATUS7_INDEX];

	return err;
}

int pva_boot_kpi(struct pva *pva, u64 *r5_boot_time)
{
	uint32_t flags = PVA_CMD_INT_ON_ERR | PVA_CMD_INT_ON_COMPLETE;
	struct pva_cmd_status_regs status;
	struct pva_cmd_s cmd;
	int err = 0;
	u32 nregs;

	nregs = pva_cmd_pva_uptime(&cmd, 255, flags);

	/* Submit request to PVA and wait for response */
	err = pva_mailbox_send_cmd_sync(pva, &cmd, nregs, &status);
	if (err < 0) {
		nvhost_warn(&pva->pdev->dev, "mbox get uptime cmd failed: %d\n",
			    err);
		return err;
	}
	*r5_boot_time = status.status[PVA_CMD_STATUS7_INDEX];
	*r5_boot_time = ((*r5_boot_time) << 32);
	*r5_boot_time = (*r5_boot_time) | status.status[PVA_CMD_STATUS6_INDEX];

	return err;
}

int pva_set_log_level(struct pva *pva, u32 log_level, bool mailbox_locked)
{
	uint32_t flags = PVA_CMD_INT_ON_ERR | PVA_CMD_INT_ON_COMPLETE;
	struct pva_cmd_status_regs status;
	struct pva_cmd_s cmd;
	int err = 0;
	u32 nregs;

	nregs = pva_cmd_set_logging_level(&cmd, log_level, flags);

	if (mailbox_locked)
		pva_mailbox_send_cmd_sync_locked(pva, &cmd, nregs, &status);
	else
		pva_mailbox_send_cmd_sync(pva, &cmd, nregs, &status);

	if (err < 0)
		nvhost_warn(&pva->pdev->dev, "mbox set log level failed: %d\n",
			    err);

	return err;
}

int pva_finalize_poweron(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct pva *pva = pdata->private_data;
	int err = 0;
	int i;

	/* Enable LIC_INTERRUPT line for HSP1, H1X and WDT */
	host1x_writel(pva->pdev, sec_lic_intr_enable_r(pva->version),
		      sec_lic_intr_enable_hsp_f(SEC_LIC_INTR_HSP1) |
			      sec_lic_intr_enable_h1x_f(SEC_LIC_INTR_H1X_ALL) |
			      sec_lic_intr_enable_wdt_f(SEC_LIC_INTR_WDT));

	err = pva_load_fw(pdev);
	if (err < 0) {
		nvhost_err(&pdev->dev, " pva fw failed to load\n");
		goto err_poweron;
	}

	for (i = 0; i < pva->version_config->irq_count; i++)
		enable_irq(pva->irq[i]);

	err = pva_init_fw(pdev);
	if (err < 0) {
		nvhost_err(&pdev->dev, " pva fw failed to init\n");
		goto err_poweron;
	}

	pva_set_log_level(pva, pva->log_level, true);

	pva->booted = true;

	return err;

err_poweron:
	for (i = 0; i < pva->version_config->irq_count; i++)
		disable_irq(pva->irq[i]);
	return err;
}

int pva_prepare_poweroff(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct pva *pva = pdata->private_data;
	int i;

	/*
	 * Disable IRQs. Interrupt handler won't be under execution after the
	 * call returns.
	 */
	for (i = 0; i < pva->version_config->irq_count; i++)
		disable_irq(pva->irq[i]);

	/* Put PVA to reset to ensure that the firmware doesn't get accessed */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0)
	reset_control_acquire(pdata->reset_control);
#endif
	reset_control_assert(pdata->reset_control);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 2, 0)
	reset_control_release(pdata->reset_control);
#endif
	pva->booted = false;

	pva_free_fw(pdev, pva);

	return 0;
}

static int pva_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct nvhost_device_data *pdata;
	const struct of_device_id *match;
	struct pva *pva;
	int err = 0;
	size_t i;
	nvhost_dbg_fn("%s", __func__);

	match = of_match_device(tegra_pva_of_match, dev);
	pdata = (struct nvhost_device_data *)match->data;

	WARN_ON(!pdata);
	if (!pdata) {
		dev_info(dev, "no platform data\n");
		err = -ENODATA;
		goto err_get_pdata;
	}

#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA19 &&
#else
	if (tegra_get_chip_id() == TEGRA194 &&
#endif
	    tegra_get_sku_id() == 0x9E) {
		dev_err(dev, "PVA IP is disabled in SKU\n");
		err = -ENODEV;
		goto err_no_ip;
	}

#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA19 &&
#else
	if (tegra_get_chip_id() == TEGRA194 &&
#endif
	    tegra_get_sku_id() == 0x9F && pdata->class == NV_PVA1_CLASS_ID) {
		dev_err(dev, "PVA1 IP is disabled in SKU\n");
		err = -ENODEV;
		goto err_no_ip;
	}

	pva = devm_kzalloc(dev, sizeof(*pva), GFP_KERNEL);
	if (!pva) {
		err = -ENOMEM;
		goto err_alloc_pva;
	}

	/* Initialize PVA private data */
#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA23) {
#else
	if (tegra_get_chip_id() == TEGRA234) {
#endif
		pva->version = PVA_HW_GEN2;
		pdata->firmware_name = "nvpva_020.fw";
		pdata->firmware_not_in_subdir = true;
		pva->submit_cmd_mode = PVA_SUBMIT_MODE_MMIO_CCQ;
#ifdef CONFIG_TEGRA_T23X_GRHOST
		pva->version_config = &pva_t23x_config;
#else
		dev_err(dev, "No T23x config available\n");
		err = -ENODEV;
		goto err_no_ip;
#endif
		nvhost_dbg_info("PVA gen2 detected.");
	} else {
		pva->version = PVA_HW_GEN1;
		pdata->firmware_name = "nvpva_010.fw";
		pdata->firmware_not_in_subdir = true;
		pva->submit_cmd_mode = PVA_SUBMIT_MODE_MAILBOX;
		pva->version_config = &pva_t19x_config;
		nvhost_dbg_info("PVA gen1 detected.");
	}
	pva->pdev = pdev;

	/* Enable powergating and timeout only on silicon */
	if (!tegra_platform_is_silicon()) {
		pdata->can_powergate = false;
		pva->timeout_enabled = false;
	} else {
		pva->timeout_enabled = true;
	}

	/* Initialize nvhost specific data */
	pdata->pdev = pdev;
	mutex_init(&pdata->lock);
	pdata->private_data = pva;
	platform_set_drvdata(pdev, pdata);
	mutex_init(&pva->mailbox_mutex);
	mutex_init(&pva->ccq_mutex);
	pva->submit_task_mode = PVA_SUBMIT_MODE_MMIO_CCQ;
	pva->slcg_disable = 0;
	pva->vmem_war_disable = 0;
	pva->vpu_perf_counters_enable = false;
	pva->vpu_debug_enabled = true;

#ifdef __linux__
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
	if (tegra_chip_get_revision() != TEGRA194_REVISION_A01)
#else
	if (tegra_get_chip_id() != TEGRA194)
#endif
		pva->vmem_war_disable = 1;
#endif
#endif

	/* Map MMIO range to kernel space */
	err = nvhost_client_device_get_resources(pdev);
	if (err < 0)
		goto err_get_resources;

	/* Get clocks */
	err = nvhost_module_init(pdev);
	if (err < 0)
		goto err_module_init;

	/*
	 * Add this to nvhost device list, initialize scaling,
	 * setup memory management for the device, create dev nodes
	 */
	err = nvhost_client_device_init(pdev);
	if (err < 0)
		goto err_client_device_init;

	pva->pool =
		nvpva_queue_init(pdev, &pva_queue_ops, MAX_PVA_QUEUE_COUNT);
	if (IS_ERR(pva->pool)) {
		err = PTR_ERR(pva->pool);
		goto err_queue_init;
	}

	err = pva_alloc_task_status_buffer(pva);
	if (err) {
		dev_err(&pva->pdev->dev, "failed to init task status buffer");
		goto err_status_init;
	}

	err = nvpva_client_context_init(pva);
	if (err) {
		dev_err(&pva->pdev->dev, "failed to init client context");
		goto err_client_ctx_init;
	}

	err = pva_register_isr(pdev);
	if (err < 0)
		goto err_isr_init;

	for (i = 0; i < pva->version_config->irq_count; i++)
		init_waitqueue_head(&pva->cmd_waitqueue[i]);

	pva_abort_init(pva);

	err = nvhost_syncpt_unit_interface_init(pdev);
	if (err)
		goto err_mss_init;

#ifdef CONFIG_DEBUG_FS
	pva_debugfs_init(pdev);
#endif
	return 0;

err_mss_init:
err_isr_init:
	nvpva_client_context_deinit(pva);
err_client_ctx_init:
	pva_free_task_status_buffer(pva);
err_status_init:
	nvpva_queue_deinit(pva->pool);
err_queue_init:
	nvhost_client_device_release(pdev);
err_client_device_init:
	nvhost_module_deinit(pdev);
err_module_init:
err_get_resources:
	devm_kfree(dev, pva);
err_alloc_pva:
err_no_ip:
err_get_pdata:

	return err;
}

static int __exit pva_remove(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct pva *pva = pdata->private_data;
	int i;

	pva_free_task_status_buffer(pva);
	nvpva_client_context_deinit(pva);
	nvpva_queue_deinit(pva->pool);
	nvhost_client_device_release(pdev);
	for (i = 0; i < pva->version_config->irq_count; i++)
		free_irq(pva->irq[i], pdata);

	return 0;
}

static struct platform_driver pva_driver = {
	.probe = pva_probe,
	.remove = __exit_p(pva_remove),
	.driver = {
		.owner = THIS_MODULE,
		.name = "pva",
#ifdef CONFIG_OF
		.of_match_table = tegra_pva_of_match,
#endif
#ifdef CONFIG_PM
		.pm = &nvhost_module_pm_ops,
#endif
	},
};

module_platform_driver(pva_driver);
