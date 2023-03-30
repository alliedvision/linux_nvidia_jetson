/*
 * Tegra NVDEC Module Support on T23x
 *
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/slab.h>         /* for kzalloc */
#include <linux/iommu.h>
#include <linux/iopoll.h>
#include <linux/dma-mapping.h>
#include <linux/platform/tegra/tegra_mc.h>
#if defined(CONFIG_TRUSTED_LITTLE_KERNEL) || defined(CONFIG_TRUSTY)
#include <linux/ote_protocol.h>
#endif

#include "dev.h"
#include "bus_client.h"
#include "riscv/riscv.h"
#include "nvhost_acm.h"
#include "platform.h"

#include "nvdec/nvdec.h"
#include "nvdec_t23x.h"
#include "nvdec/hw_nvdec_t23x.h"
#include "flcn/hw_flcn.h"

#define NVDEC_DEBUGINFO_DUMMY		(0xabcd1234)
#define NVDEC_DEBUGINFO_CLEAR		(0x0)
#define FW_NAME_SIZE			32

static int nvdec_riscv_wait_mem_scrubbing(struct platform_device *dev)
{
	u32 val;
	int err = 0;
	void __iomem *hwcfg2_addr;

	hwcfg2_addr = get_aperture(dev, 0) + flcn_hwcfg2_r();
	err  = readl_poll_timeout(hwcfg2_addr, val,
				(flcn_hwcfg2_mem_scrubbing_v(val) ==
				 flcn_hwcfg2_mem_scrubbing_done_v()),
				RISCV_IDLE_CHECK_PERIOD,
				RISCV_IDLE_TIMEOUT_DEFAULT);
	if (err) {
		dev_err(&dev->dev, "mem scrubbing timeout! val=0x%x", val);
		return -ETIMEDOUT;
	}

	return err;

}

static void nvdec_get_riscv_bin_name(struct platform_device *pdev, char *name,
					bool is_gsc)
{
	u8 maj, min;
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	u32 debug_mode = host1x_readl(pdev, flcn_hwcfg2_r()) &
					flcn_hwcfg2_dbgmode_m();

	nvdec_decode_ver(pdata->version, &maj, &min);
	if (is_gsc) {
		if (debug_mode) {
			snprintf(name, FW_NAME_SIZE,
				"nvhost_nvdec0%d%d_desc_dev.bin", maj, min);
		} else {
			snprintf(name, FW_NAME_SIZE,
				"nvhost_nvdec0%d%d_desc_prod.bin", maj, min);
		}
	} else {
		snprintf(name, FW_NAME_SIZE, "nvhost_nvdec0%d%d_desc_sim.bin",
			maj, min);
	}
}

static int nvdec_read_riscv_bin(struct platform_device *dev,
				const char *desc_bin_name, bool is_gsc)
{
	int err, w;
	const struct firmware *desc_bin, *riscv_image = NULL;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	struct riscv_data *m = (struct riscv_data *)pdata->riscv_data;

	if (!m) {
		dev_err(&dev->dev, "riscv data is NULL");
		return -ENODATA;
	}

	m->dma_addr = 0;
	m->mapped = NULL;
	desc_bin = nvhost_client_request_firmware(dev, desc_bin_name, true);
	if (!desc_bin) {
		dev_err(&dev->dev, "failed to get desc binary");
		return -ENOENT;
	}

	if (!is_gsc) {
		riscv_image = nvhost_client_request_firmware(dev, pdata->riscv_image_bin, true);
		if (!riscv_image) {
			dev_err(&dev->dev, "failed to get nvdec image binary");
			release_firmware(desc_bin);
			return -ENOENT;
		}

		m->size = riscv_image->size;
		m->mapped = dma_alloc_attrs(&dev->dev, m->size, &m->dma_addr,
					GFP_KERNEL,
					DMA_ATTR_READ_ONLY | DMA_ATTR_FORCE_CONTIGUOUS);
		if (!m->mapped) {
			dev_err(&dev->dev, "dma memory allocation failed");
			err = -ENOMEM;
			goto clean_up;
		}

		/* Copy the whole image taking endianness into account */
		for (w = 0; w < riscv_image->size/sizeof(u32); w++)
			m->mapped[w] = le32_to_cpu(((__le32 *)riscv_image->data)[w]);
	}

	/* Parse the desc binary for offsets */
	riscv_compute_ucode_offsets_2stage(dev, m, desc_bin);

	m->valid = true;
	release_firmware(desc_bin);
	release_firmware(riscv_image);

	return 0;

clean_up:
	if (m->mapped) {
		dma_free_attrs(&dev->dev, m->size, m->mapped, m->dma_addr,
				DMA_ATTR_READ_ONLY | DMA_ATTR_FORCE_CONTIGUOUS);
		m->mapped = NULL;
		m->dma_addr = 0;
	}
	release_firmware(desc_bin);
	release_firmware(riscv_image);
	return err;
}

static int nvhost_nvdec_riscv_init_sw(struct platform_device *pdev, bool is_gsc)
{
	int err = 0;
	char riscv_desc_bin[FW_NAME_SIZE];
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct riscv_data *m = (struct riscv_data *)pdata->riscv_data;

	if (m)
		return 0;

	m = kzalloc(sizeof(*m), GFP_KERNEL);
	if (!m) {
		dev_err(&pdev->dev, "Couldn't allocate memory for riscv data");
		return -ENOMEM;
	}
	pdata->riscv_data = m;

	nvdec_get_riscv_bin_name(pdev, riscv_desc_bin, is_gsc);
	dev_info(&pdev->dev, "RISC-V desc binary name:%s", riscv_desc_bin);
	err = nvdec_read_riscv_bin(pdev, riscv_desc_bin, is_gsc);
	if (err || !m->valid) {
		dev_err(&pdev->dev, "binary not valid");
		goto clean_up;
	}

	return 0;

clean_up:
	kfree(m);
	pdata->riscv_data = NULL;
	return err;
}

static void nvhost_nvdec_riscv_deinit_sw(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	struct riscv_data *m = (struct riscv_data *)pdata->riscv_data;

	if (m->mapped) {
		dma_free_attrs(&dev->dev, m->size, m->mapped, m->dma_addr,
				DMA_ATTR_READ_ONLY | DMA_ATTR_FORCE_CONTIGUOUS);
		m->mapped = NULL;
		m->dma_addr = 0;
	}

	kfree(m);
	pdata->riscv_data = NULL;
}

static int load_ucode(struct platform_device *dev, u64 base, u32 gscid,
			struct riscv_image_desc desc)
{
	void __iomem *retcode_addr, *debuginfo_addr;
	u64 addr;
	int err = 0;
	u32 val;

	/* Protect engine/falcon registers from channel programing */
	flcn_enable_thi_sec(dev);

	/* Check if mem scrubbing is done */
	err = nvdec_riscv_wait_mem_scrubbing(dev);
	if (err)
		return err;

	/* Select RISC-V core for nvdec */
	host1x_writel(dev, nvdec_riscv_bcr_ctrl_r(),
			nvdec_riscv_bcr_ctrl_core_select_riscv_f());

	/* Program manifest start address */
	addr = (base + desc.manifest_offset) >> 8;
	host1x_writel(dev, nvdec_riscv_bcr_dmaaddr_pkcparam_lo_r(),
			lower_32_bits(addr));
	host1x_writel(dev, nvdec_riscv_bcr_dmaaddr_pkcparam_hi_r(),
			upper_32_bits(addr));

	/* Program FMC code start address */
	addr = (base + desc.code_offset) >> 8;
	host1x_writel(dev, nvdec_riscv_bcr_dmaaddr_fmccode_lo_r(),
			lower_32_bits(addr));
	host1x_writel(dev, nvdec_riscv_bcr_dmaaddr_fmccode_hi_r(),
			upper_32_bits(addr));

	/* Program FMC data start address */
	addr = (base + desc.data_offset) >> 8;
	host1x_writel(dev, nvdec_riscv_bcr_dmaaddr_fmcdata_lo_r(),
			lower_32_bits(addr));
	host1x_writel(dev, nvdec_riscv_bcr_dmaaddr_fmcdata_hi_r(),
			upper_32_bits(addr));

	/* Program DMA config registers. GSC ID = 0x1 for CARVEOUT1 */
	host1x_writel(dev, nvdec_riscv_bcr_dmacfg_sec_r(),
			nvdec_riscv_bcr_dmacfg_sec_gscid_f(gscid));
	host1x_writel(dev, nvdec_riscv_bcr_dmacfg_r(),
			nvdec_riscv_bcr_dmacfg_target_local_fb_f() |
			nvdec_riscv_bcr_dmacfg_lock_locked_f());

	/* Write a knwon pattern into DEBUGINFO register */
	host1x_writel(dev, nvdec_debuginfo_r(), NVDEC_DEBUGINFO_DUMMY);

	/* Kick start RISC-V and let BR take over */
	host1x_writel(dev, nvdec_riscv_cpuctl_r(),
			nvdec_riscv_cpuctl_startcpu_true_f());

	/* Check BR return code */
	retcode_addr = get_aperture(dev, 0) + nvdec_riscv_br_retcode_r();
	err  = readl_poll_timeout(retcode_addr, val,
				(nvdec_riscv_br_retcode_result_v(val) ==
					nvdec_riscv_br_retcode_result_pass_v()),
				RISCV_IDLE_CHECK_PERIOD,
				RISCV_IDLE_TIMEOUT_DEFAULT);
	if (err) {
		dev_err(&dev->dev, "BR return code timeout! val=0x%x", val);
		return err;
	}

	/* Check if it has reached a proper initialized state */
	debuginfo_addr = get_aperture(dev, 0) + nvdec_debuginfo_r();
	err  = readl_poll_timeout(debuginfo_addr, val,
				(val == NVDEC_DEBUGINFO_CLEAR),
				RISCV_IDLE_CHECK_PERIOD_LONG,
				RISCV_IDLE_TIMEOUT_LONG);
	if (err) {
		dev_err(&dev->dev,
			"RISC-V couldn't reach init state, timeout! val=0x%x",
			val);
		return err;
	}

	return 0;
}

int nvhost_nvdec_riscv_finalize_poweron(struct platform_device *dev)
{
	struct riscv_data *m;
	struct mc_carveout_info inf;
	int err = 0;
	bool is_gsc = 0;
	phys_addr_t dma_pa;
	struct iommu_domain *domain;
	unsigned int gscid = 0x0;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	/* Get GSC carvout info */
	err = mc_get_carveout_info(&inf, NULL, MC_SECURITY_CARVEOUT1);
	if (err) {
		dev_err(&dev->dev, "failed to fetch carveout info");
		err = -ENOMEM;
		goto clean_up;
	}

	dev_dbg(&dev->dev, "CARVEOUT1 base=0x%llx size=0x%llx",
		inf.base, inf.size);
	if (inf.base)
		is_gsc = 1;

	err = nvhost_nvdec_riscv_init_sw(dev, is_gsc);
	if (err)
		return err;

	m = (struct riscv_data *)pdata->riscv_data;

	/* Get the physical address of corresponding dma address */
	domain = iommu_get_domain_for_dev(&dev->dev);

	if (is_gsc) {
		dma_pa = inf.base;
		gscid = 0x1;
		dev_info(&dev->dev, "RISC-V booting from GSC\n");
	} else {
		/* For non-secure boot only */
		dma_pa = iommu_iova_to_phys(domain, m->dma_addr);
		dev_info(&dev->dev, "RISC-V boot using kernel allocated Mem\n");

		/*
		 * Add the offset to skip boot_component_header_t, which is
		 * present at the start of binary. This struct is used
		 * by MB1 for loading the binary from GSC carvout. However,
		 * it is redundant when the binary is stored in the kernel
		 * allocated memory.
		 * As the firmwares are generated using the same script in
		 * both these case, this offset is added in the non-gsc case
		 * to exclude boot_component_header_t.
		 * sizeof(boot_component_header_t) = 0x2000 bytes (8k).
		 */
		dma_pa += 8192; // 0x2000
	}

	/* Load BL ucode in stage-1*/
	err = load_ucode(dev, dma_pa, gscid, m->bl);
	if (err) {
		dev_err(&dev->dev, "RISC-V stage-1 boot failed, err=0x%x", err);
		goto clean_up;
	}

	/* Reset NVDEC before stage-2 boot */
	nvhost_module_reset_for_stage2(dev);

	/* Load LS ucode in stage-2 */
	err = load_ucode(dev, dma_pa, gscid, m->os);
	if (err) {
		dev_err(&dev->dev, "RISC-V stage-2 boot failed = 0x%x", err);
		goto clean_up;
	}

#if defined(CONFIG_TRUSTED_LITTLE_KERNEL)
	tlk_restore_keyslots();
#endif
#if defined(CONFIG_TRUSTY)
	trusty_restore_keyslots();
#endif
	dev_info(&dev->dev, "RISCV boot success");
	return 0;

clean_up:
	dev_err(&dev->dev, "RISCV boot failed");
	nvhost_nvdec_riscv_deinit_sw(dev);
	return err;

}

int nvhost_nvdec_finalize_poweron_t23x(struct platform_device *dev)
{
	int err;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	if (!pdata) {
		dev_err(&dev->dev, "no platform data");
		return -ENODATA;
	}

	if (pdata->enable_riscv_boot) {
		err = nvhost_nvdec_riscv_finalize_poweron(dev);
	} else {
		flcn_enable_thi_sec(dev);
		err = nvhost_nvdec_finalize_poweron(dev);
	}

	return err;
}

int nvhost_nvdec_prepare_poweroff_t23x(struct platform_device *dev)
{
	return 0;
}

