// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
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

#include <linux/types.h>
#include <linux/io.h>
#include <linux/nvhost.h>
#include "pva.h"
#include "pva_vpu_ocd.h"

#define PVA_DEBUG_APERTURE_INDEX 1
#define VPU_OCD_MAX_NUM_DATA_ACCESS 7

static void block_writel(struct pva_vpu_dbg_block *block, u32 offset, u32 val)
{
	void __iomem *addr = block->vbase + offset;

	writel(val, addr);
}

static u32 block_readl(struct pva_vpu_dbg_block *block, u32 offset)
{
	void __iomem *addr = block->vbase + offset;

	return readl(addr);
}

static int init_vpu_dbg_block(struct pva *pva, struct pva_vpu_dbg_block *block,
			      u32 offset)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pva->pdev);
	void __iomem *aperture = pdata->aperture[PVA_DEBUG_APERTURE_INDEX];

	if (aperture == NULL)
		return -EINVAL;

	block->vbase = aperture + offset;
	return 0;
}

int pva_vpu_ocd_init(struct pva *pva)
{
	u32 i;
	int err;
	const phys_addr_t vpu_dbg_offsets[NUM_VPU_BLOCKS] = { 0x00050000,
							      0x00070000 };

	for (i = 0; i < NUM_VPU_BLOCKS; i++) {
		err = init_vpu_dbg_block(pva, &pva->vpu_dbg_blocks[i],
					 vpu_dbg_offsets[i]);
		if (err != 0)
			return err;
	}
	return 0;
}

int pva_vpu_ocd_io(struct pva_vpu_dbg_block *block, u32 instr, const u32 *wdata,
		   u32 nw, u32 *rdata, u32 nr)
{
	u32 i;

	if ((nr > VPU_OCD_MAX_NUM_DATA_ACCESS) ||
	    (nw > VPU_OCD_MAX_NUM_DATA_ACCESS)) {
		pr_err("pva: too many vpu dbg reg read (%u) or write (%u)\n",
		       nr, nw);
		return -EINVAL;
	}

	/* write instruction first */
	block_writel(block, 0, instr);

	/*
	 * write data
	 * if there's 1 word, write to addr 0x4,
	 * if there's 2 words, write to addr 2 * 0x4,
	 * ...
	 */
	for (i = 0; i < nw; i++)
		block_writel(block, nw * sizeof(u32), wdata[i]);

	/*
	 * read data
	 * if there's 1 word, read from addr 0x4,
	 * if there's 2 words, read from addr 2 * 0x4,
	 * ...
	 */
	for (i = 0; i < nr; i++)
		rdata[i] = block_readl(block, nr * sizeof(u32));

	return 0;
}
