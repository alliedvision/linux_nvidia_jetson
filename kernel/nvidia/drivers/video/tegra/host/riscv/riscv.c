/*
 * Tegra riscv boot support file
 *
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
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

#include "dev.h"
#include "riscv.h"

int riscv_compute_ucode_offsets(struct platform_device *dev,
				struct riscv_data *v,
				const struct firmware *ucode_desc)
{
	PRM_RISCV_UCODE_DESC riscv_desc;

	riscv_desc = (PRM_RISCV_UCODE_DESC)ucode_desc->data;
	v->os.manifest_offset = le32_to_cpu(riscv_desc->manifestOffset);
	v->os.code_offset = le32_to_cpu(riscv_desc->monitorCodeOffset);
	v->os.data_offset = le32_to_cpu(riscv_desc->monitorDataOffset);

	return 0;
}

void riscv_compute_ucode_offsets_2stage(struct platform_device *dev,
				struct riscv_data *v,
				const struct firmware *riscv_desc_bin)
{
	PRM_RISCV_UCODE_DESC riscv_desc;

	/* Fetch offsets for BL ucode */
	riscv_desc = (PRM_RISCV_UCODE_DESC)riscv_desc_bin->data;
	v->bl.manifest_offset = le32_to_cpu(riscv_desc->manifestOffset);
	v->bl.code_offset = le32_to_cpu(riscv_desc->monitorCodeOffset);
	v->bl.data_offset = le32_to_cpu(riscv_desc->monitorDataOffset);

	/* Fetch offsets and sizes for LS ucode */
	riscv_desc = (PRM_RISCV_UCODE_DESC)((u8 *)riscv_desc_bin->data +
						RISCV_UCODE_DESC_ALIGNMENT);
	v->os.manifest_offset = le32_to_cpu(riscv_desc->manifestOffset);
	v->os.code_offset = le32_to_cpu(riscv_desc->monitorCodeOffset);
	v->os.code_size = le32_to_cpu(riscv_desc->monitorCodeSize);
	v->os.data_offset = le32_to_cpu(riscv_desc->monitorDataOffset);
	v->os.data_size = le32_to_cpu(riscv_desc->monitorDataSize);

}
