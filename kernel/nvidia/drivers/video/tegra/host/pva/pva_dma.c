/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION. All rights reserved.
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
#include <linux/seq_file.h>
#include <linux/nospec.h>
#include "pva_dma.h"
#include "pva_queue.h"
#include "pva-sys-dma.h"
#include "pva.h"
#include "pva_vpu_exe.h"
#include "nvpva_client.h"
#include "pva-bit.h"
#include "fw_config.h"
#include "pva_hwseq.h"

#define BL_GOB_WIDTH_LOG2       6U
#define BL_GOB_WIDTH_LOG2_ALIGNMASK     (0xFFFFFFFFU >> (32U - BL_GOB_WIDTH_LOG2))
#define BL_GOB_HEIGHT_LOG2      3U
#define BL_GOB_SIZE_LOG2        (BL_GOB_WIDTH_LOG2 + BL_GOB_HEIGHT_LOG2)
#define LOW_BITS		(0xFFFFFFFFU >> (32U - 4U))

int64_t
pitch_linear_eq_offset(struct nvpva_dma_descriptor const *dma_desc,
		       const int64_t surf_bl_offset,
		       const uint8_t block_ht_log2,
		       uint8_t bppLog2,
		       bool is_dst,
		       bool is_dst2)
{
	uint8_t format = 0U;
	uint64_t offset = 0ULL;
	uint16_t line_pitch = 0U;
	uint8_t cb_enable = 0U;
	int64_t frame_buf_offset = 0;
	uint32_t line_pitch_bytes;
	int32_t offset_within_surface;
	uint32_t x;
	uint32_t y;
	uint32_t blockSizeLog2;
	uint32_t blockMask;
	uint32_t blocksPerRop;
	uint32_t ropSize;
	uint32_t ropIdx;
	uint32_t offsetToRop;
	uint32_t offsetWithinRop;
	uint32_t blockIdx;
	uint32_t offsetWithinBlock;
	uint32_t gobIdx;

	if (is_dst) {
		format = dma_desc->dstFormat;
		offset = dma_desc->dst_offset;
		line_pitch = dma_desc->dstLinePitch;
		cb_enable = dma_desc->dstCbEnable;
	} else if(is_dst2) {
		format = dma_desc->dstFormat;
		offset = dma_desc->dst2Offset;
		line_pitch = dma_desc->dstLinePitch;
		cb_enable = dma_desc->dstCbEnable;
	}else {
		format = dma_desc->srcFormat;
		offset = dma_desc->src_offset;
		line_pitch = dma_desc->srcLinePitch;
		cb_enable = dma_desc->srcCbEnable;
	}

	if (format == 0U) {
		frame_buf_offset = offset;
		goto done;
	}

	line_pitch_bytes = (line_pitch << bppLog2);

	if (cb_enable != 0)
	{
		pr_err("circular buffer not allowed for BL");
		goto done;
	}
	if ((line_pitch_bytes & BL_GOB_WIDTH_LOG2_ALIGNMASK) != 0)
	{
		pr_err("frame line pitch not a multiple of GOB width in BL");
		goto done;
	}
	if (offset % 64U != 0)
	{
		pr_err("block linear access offsets are misaligned ");
		goto done;
	}

	offset_within_surface = (offset - surf_bl_offset);

	/*
	 * Calculate the x and y coords within the GOB,
	 * using TEGRA_RAW format
	 */
	x = offset_within_surface & LOW_BITS;
	/* Bit 5 advances 16 x coords */
	x += ((offset_within_surface & (1U << 5U)) != 0) * 16;
	/* Bit 8 advances 32 x coords */
	x += ((offset_within_surface & (1U << 8U)) != 0) * 32;
	/* Bit 4 advances 1 y coord */
	y = ((offset_within_surface & (1U << 4U)) != 0);
	/* Bit 6 advances 2 y coords */
	y += ((offset_within_surface & (1U << 6U)) != 0) * 2;
	/* Bit 7 advances 4 y coords */
	y += ((offset_within_surface & (1U << 7U)) != 0) * 4;

	/*
	 * Calculate the block idx and the GOB idx
	 * so we can find offset to the GOB within the ROP
	 */
	blockSizeLog2	= BL_GOB_SIZE_LOG2 + block_ht_log2;
	blockMask	= (0xFFFFFFFFU >> (32U - blockSizeLog2));
	blocksPerRop	= line_pitch_bytes >> BL_GOB_WIDTH_LOG2;
	ropSize		= blocksPerRop << blockSizeLog2;
	ropIdx		= offset_within_surface / ropSize;
	offsetToRop	= ropIdx * ropSize;
	offsetWithinRop	= offset_within_surface - offsetToRop;
	blockIdx	= offsetWithinRop >> blockSizeLog2;
	offsetWithinBlock = offset_within_surface & blockMask;
	gobIdx		= offsetWithinBlock >> BL_GOB_SIZE_LOG2;

	x += blockIdx << BL_GOB_WIDTH_LOG2;
	y += gobIdx << BL_GOB_HEIGHT_LOG2;

	frame_buf_offset = surf_bl_offset + offsetToRop + y * line_pitch_bytes + x;

done:
	return frame_buf_offset;
}

static int32_t check_address_range(struct nvpva_dma_descriptor const *desc,
				   uint64_t max_size,
				   uint64_t max_size2,
				   bool src_dst,
				   bool dst2,
				   int8_t block_height_log2)
{
	int32_t err = 0;
	int64_t start = 0;
	int64_t end = 0;
	int64_t offset = 0;
	int64_t offset2 = 0;
	uint32_t i;
	int64_t bppSize = ((int64_t)desc->bytePerPixel == 0) ? 1 :
				((int64_t)desc->bytePerPixel == 1) ? 2 : 4;

	 /** max 5 dimension loop for DMA */
	 int64_t s[5] = {};
	int64_t last_tx = (int64_t)desc->tx - 1;
	int64_t last_ty = (int64_t)desc->ty - 1;

	/** dummy transfer mode with no data transfer */
	if (desc->tx == 0U)
		return err;

	/** ty = 0 is not allowed */
	if (desc->ty == 0U)
		return -EINVAL;

	/** Source transfer mode take care of padding */
	if (src_dst == false) {
		last_tx -= (int64_t)desc->px;
		last_ty -= (int64_t)desc->py;
	}

	/** 1st dimension */
	s[0] = last_tx;
	start = min((s[0]*bppSize), 0LL);
	end = max(((s[0]*bppSize) + (bppSize - 1)), 0LL);
	if (src_dst) {
		if ((desc->dstFormat == 1U) && (block_height_log2 == -1)) {
			pr_err("Invalid block height for BL format");
			return -EINVAL;
		}

		/* 2nd destination dim */
		s[1] = (int64_t)desc->dstLinePitch * last_ty;
		if (desc->dstCbEnable == 1U) {
			/* ((DLP_ADV * (Ty-1)) + Tx) * BPP <= DB_SIZE */
			if (((s[1] + last_tx + 1) * bppSize) <=
			    (int64_t)desc->dstCbSize)
				return 0;

			pr_err("invalid dst cb advance");
			return -EINVAL;
		}


		offset = pitch_linear_eq_offset(desc, desc->surfBLOffset,
			block_height_log2, desc->bytePerPixel, true, false);

		offset2 = pitch_linear_eq_offset(desc, desc->surfBLOffset,
			block_height_log2, desc->bytePerPixel, false, true);

		/* 3rd destination dim */
		s[2] = ((int64_t)desc->dstAdv1 * (int64_t)desc->dstRpt1);
		/* 4th destination dim */
		s[3] = ((int64_t)desc->dstAdv2 * (int64_t)desc->dstRpt2);
		/* 5th destination dim */
		s[4] = ((int64_t)desc->dstAdv3 * (int64_t)desc->dstRpt3);
	} else {
		if ((desc->srcFormat == 1U) && (block_height_log2 == -1)) {
			pr_err("Invalid block height for BL format");
			return -EINVAL;
		}

		/* 2nd source dim */
		s[1] = (int64_t)desc->srcLinePitch * last_ty;
		if (desc->srcCbEnable == 1U) {
			/* ((SLP_ADV * (Ty-1)) + Tx) * BPP <= SB_SIZE */
			if (((s[1] + last_tx + 1) * bppSize) <=
			    (int64_t)desc->srcCbSize)
				return 0;
			pr_err("invalid src cb");
				return -EINVAL;
		}

		offset = pitch_linear_eq_offset(desc, desc->surfBLOffset,
			block_height_log2, desc->bytePerPixel, false, false);
		/* 3rd source dim */
		s[2] = ((int64_t)desc->srcAdv1 * (int64_t)desc->srcRpt1);
		/* 4th source dim */
		s[3] = ((int64_t)desc->srcAdv2 * (int64_t)desc->srcRpt2);
		/* 5th source dim */
		s[4] = ((int64_t)desc->srcAdv3 * (int64_t)desc->srcRpt3);
	}

	for (i = 1U; i < 5U; i++) {
		start += min(s[i]*bppSize, 0LL);
		end += max(s[i]*bppSize, 0LL);
	}

	/* check for out of range access */
	if (((int64_t) max_size) < 0) {
		pr_err("max_size too large");
		err = -EINVAL;
		goto out;
	}

	if (!(((offset + start) >= 0)
	    && ((offset + end) < (int64_t)max_size))) {
		pr_err("ERROR: Out of range detected");
		err = -EINVAL;
	}

	if (dst2) {
		if ((max_size2 > UINT_MAX) || !(((offset2 + start) >= 0)
		    && ((offset2 + end) < (int64_t)max_size2))) {
			pr_err("ERROR: Out of range detected");
			err = -EINVAL;
		}
	}
out:
	return err;
}

static int32_t
patch_dma_desc_address(struct pva_submit_task *task,
		      struct nvpva_dma_descriptor *umd_dma_desc,
		      struct pva_dtd_s *dma_desc, u8 desc_id, bool is_misr,
		      u8 block_height_log2)
{
	int32_t err = 0;
	uint64_t addr_base = 0;
	struct pva_dma_task_buffer_info_s *buff_info = &task->task_buff_info[desc_id];

	nvpva_dbg_fn(task->pva, "");

	switch (umd_dma_desc->srcTransferMode) {
	case DMA_DESC_SRC_XFER_L2RAM:
		/*
		 * PVA_HW_GEN1 has CVNAS RAM PVA_HW_GEN2 has L2SRAM CVNAS RAM
		 * memory is pinned and needs conversion from pin ID -> IOVA
		 * L2SRAM has memory offset which does not need conversion. The
		 * same conversion is applied for dst
		 */
		if (task->pva->version == PVA_HW_GEN1) {
			struct pva_pinned_memory *mem =
				pva_task_pin_mem(task, umd_dma_desc->srcPtr);
			if (IS_ERR(mem)) {
				err = PTR_ERR(mem);
				task_err(task,
					"invalid memory handle in"
					" descriptor for SRC CVSRAM");
				goto out;
			}

			addr_base = mem->dma_addr;
			err = check_address_range(umd_dma_desc,
						  mem->size,
						  0,
						  false,
						  false,
						  block_height_log2);
			buff_info->src_buffer_size = mem->size;
		} else {
			addr_base = 0;
			if ((task->desc_hwseq_frm & (1ULL << desc_id)) == 0ULL)
				err = check_address_range(umd_dma_desc,
							  task->l2_alloc_size,
							  0,
							  false,
							  false,
							  block_height_log2);
			buff_info->src_buffer_size = task->l2_alloc_size;
		}

		if (err)
			goto out;

		break;
	case DMA_DESC_SRC_XFER_VMEM:{
		/* calculate symbol address */
		u32 addr = 0;
		u32 size = 0;

		if (umd_dma_desc->src_offset > U32_MAX) {
			err = -EINVAL;
			goto out;
		}

		err = pva_get_sym_offset(&task->client->elf_ctx, task->exe_id,
					 umd_dma_desc->srcPtr, &addr, &size);
		if (err) {
			err = -EINVAL;
			task_err(
			    task,
			    "invalid symbol id in descriptor for src VMEM");
			goto out;
		}

		err = check_address_range(umd_dma_desc,
					  size,
					  0,
					  false,
					  false,
					  block_height_log2);
		if (err) {
			err = -EINVAL;
			task_err(
				task, "ERROR: Invalid offset or address");
			goto out;
		}

		addr_base = addr;
		buff_info->src_buffer_size = size;
		break;
	}
	case DMA_DESC_SRC_XFER_VPU_CONFIG: {
		u32 addr = 0;
		u32 size = 0;

		/* dest must be null*/
		if ((umd_dma_desc->dstPtr != NVPVA_INVALID_SYMBOL_ID)
		   || (umd_dma_desc->dst2Ptr != NVPVA_INVALID_SYMBOL_ID)
		   || (umd_dma_desc->src_offset > U32_MAX)) {
			task_err(task, "ERROR: Invalid VPUC");
			err = -EINVAL;
			goto out;
		}

		/* calculate symbol address */
		/* TODO: check VPUC handling in ELF segment */
		err = pva_get_sym_offset(&task->client->elf_ctx, task->exe_id,
					 umd_dma_desc->srcPtr, &addr, &size);
		if (err) {
			task_err(task, "ERROR: Invalid offset or address");
			err = -EINVAL;
			goto out;
		}

		addr_base = addr;
		buff_info->src_buffer_size = size;
		break;
	}
	case DMA_DESC_SRC_XFER_MC: {
		struct pva_pinned_memory *mem =
			pva_task_pin_mem(task, umd_dma_desc->srcPtr);
		if (IS_ERR(mem)) {
			err = PTR_ERR(mem);
			task_err(
				task,
				"invalid memory handle: descriptor: src MC");
			goto out;
		}
		if ((task->desc_hwseq_frm & (1ULL << desc_id)) == 0ULL)
			err = check_address_range(umd_dma_desc,
						  mem->size,
						  0,
						  false,
						  false,
						  block_height_log2);

		if (err) {
			err = -EINVAL;
			task_err(task, "ERROR: address");
			goto out;
		}

		addr_base = mem->dma_addr;
		task->src_surf_base_addr = addr_base;
		buff_info->src_buffer_size = mem->size;

		/** If BL format selected, set addr bit 39 to indicate */
		/* XBAR_RAW swizzling is required */
		addr_base |= (u64)umd_dma_desc->srcFormat << 39U;

		break;
	}
	case DMA_DESC_SRC_XFER_R5TCM:
		if (!task->is_system_app) {
			err = -EFAULT;
			goto out;
		} else {
			task->special_access = 1;
			addr_base = 0;
			break;
		}
	case DMA_DESC_SRC_XFER_MMIO:
	case DMA_DESC_SRC_XFER_INVAL:
	case DMA_DESC_SRC_XFER_RSVD:
		task_err(task, "invalid src mode %d",
			umd_dma_desc->srcTransferMode);
		err = -EINVAL;
		goto out;
	default:
		err = -EFAULT;
		goto out;
	}

	addr_base += umd_dma_desc->src_offset;
	dma_desc->src_adr0 = (uint32_t)(addr_base & 0xFFFFFFFFLL);
	dma_desc->src_adr1 = (uint8_t)((addr_base >> 32U) & 0xFF);
	if (umd_dma_desc->srcTransferMode ==
		(uint8_t)DMA_DESC_SRC_XFER_VPU_CONFIG)
		goto out;

	addr_base = 0;
	if (is_misr) {
		if (umd_dma_desc->dstTransferMode == DMA_DESC_DST_XFER_L2RAM
		    || umd_dma_desc->dstTransferMode == DMA_DESC_DST_XFER_MC) {
			addr_base = umd_dma_desc->dstPtr;
			goto done;
		} else {
			err = -EINVAL;
			task_err(
				task,
				"invalid dst transfer mode for MISR descriptor");
			goto out;
		}
	}

	switch (umd_dma_desc->dstTransferMode) {
	case DMA_DESC_DST_XFER_L2RAM:
		if (task->pva->version == PVA_HW_GEN1) {
			struct pva_pinned_memory *mem =
				pva_task_pin_mem(task, umd_dma_desc->dstPtr);
			if (IS_ERR(mem)) {
				err = PTR_ERR(mem);
				task_err(task,
					"invalid memory handle in"
					" descriptor for dst CVSRAM");
				goto out;
			}

			addr_base = mem->dma_addr;
			err = check_address_range(umd_dma_desc,
						  mem->size,
						  0,
						  true,
						  false,
						  block_height_log2);
			buff_info->dst_buffer_size = mem->size;
		} else {
			addr_base = 0;
			err = check_address_range(umd_dma_desc,
						  task->l2_alloc_size,
						  0,
						  true,
						  false,
						  block_height_log2);
			buff_info->dst_buffer_size = task->l2_alloc_size;
		}

		if (err) {
			task_err(task, "ERROR: Invalid offset or address");
			err = -EINVAL;
			goto out;
		}

		break;
	case DMA_DESC_DST_XFER_VMEM: {
		/* calculate symbol address */
		u32 addr = 0;
		u32 size = 0;
		u32 addr2 = 0;
		u32 size2 = 0;
		bool check_size2 = false;

		if ((umd_dma_desc->dst_offset > U32_MAX)
		   || (umd_dma_desc->dst2Offset > U32_MAX)) {
			err = -EINVAL;
			goto out;
		}

		err = pva_get_sym_offset(&task->client->elf_ctx, task->exe_id,
					 umd_dma_desc->dstPtr, &addr, &size);
		if (err) {
			err = -EINVAL;
			task_err(
				task,
				"invalid symbol id in descriptor for dst VMEM");
			goto out;
		}

		if (umd_dma_desc->dst2Ptr != NVPVA_INVALID_SYMBOL_ID) {
			err = pva_get_sym_offset(&task->client->elf_ctx,
						 task->exe_id,
						 umd_dma_desc->dst2Ptr,
						 &addr2,
						 &size2);

			if (err) {
				err = -EINVAL;
				task_err(
					task,
					"invalid symbol id in descriptor "
					"for dst2 VMEM");
				goto out;
			}

			if ((addr2 + umd_dma_desc->dst2Offset) & 0x3F) {
				task_err(task,
					 "ERR: dst2Ptr/Offset not aligned");
				err = -EINVAL;
				goto out;
			}

			check_size2 = true;
		}

		err = check_address_range(umd_dma_desc,
					  size,
					  size2,
					  true,
					  check_size2,
					  block_height_log2);
		if (err) {
			err = -EINVAL;
			task_err(
				task, "ERROR: Invalid offset or address");
			goto out;
		}

		addr_base = addr;
		buff_info->dst_buffer_size = size;
		buff_info->dst2_buffer_size = size2;

		break;
	}
	case DMA_DESC_DST_XFER_MC: {
		struct pva_pinned_memory *mem =
			pva_task_pin_mem(task, umd_dma_desc->dstPtr);
		if (IS_ERR(mem)) {
			err = PTR_ERR(mem);
			task_err(
				task,
				"invalid memory handle: descriptor: dst MC");
			goto out;
		}

		err = check_address_range(umd_dma_desc,
					  mem->size,
					  0,
					  true,
					  false,
					  block_height_log2);
		if (err) {
			err = -EINVAL;
			task_err(task, "ERROR: address");
			goto out;
		}

		addr_base = mem->dma_addr;
		task->dst_surf_base_addr = addr_base;
		buff_info->dst_buffer_size = mem->size;

		/* If BL format selected, set addr bit 39 to indicate */
		/* XBAR_RAW swizzling is required */
		addr_base |= (u64)umd_dma_desc->dstFormat << 39U;
		break;
	}
	case DMA_DESC_DST_XFER_R5TCM:
		if (!task->is_system_app) {
			err = -EFAULT;
			goto out;
		} else {
			task->special_access = 1;
			addr_base = 0;
			break;
		}
	case DMA_DESC_DST_XFER_MMIO:
	case DMA_DESC_DST_XFER_INVAL:
	case DMA_DESC_DST_XFER_RSVD1:
	case DMA_DESC_DST_XFER_RSVD2:
		task_err(task, "invalid dst mode %d",
		umd_dma_desc->dstTransferMode);
		err = -EINVAL;
		goto out;
	default:
		err = -EFAULT;
		goto out;
	}
done:
	addr_base += umd_dma_desc->dst_offset;
	dma_desc->dst_adr0 = (uint32_t)(addr_base & 0xFFFFFFFFLL);
	dma_desc->dst_adr1 = (uint8_t)((addr_base >> 32U) & 0xFF);
	nvpva_dbg_fn(task->pva, "dsts = %lld, srcbs=%lld",
		     buff_info->dst_buffer_size,
		     buff_info->src_buffer_size);
out:
	return err;
}

static bool
is_valid_vpu_trigger_mode(const struct nvpva_dma_descriptor *desc,
			  u32 trigger_mode)
{
	bool valid = true;

	if (desc->trigEventMode == 0U)
		return valid;

	switch ((enum nvpva_task_dma_trig_vpu_hw_events)
					desc->trigVpuEvents) {
		case TRIG_VPU_NO_TRIGGER:
			if (trigger_mode != NVPVA_HWSEQTM_DMATRIG)
				valid = false;

			break;
		case TRIG_VPU_CONFIG_START:
			/** If trig = VPU configuration trigger,
			 * the DSTM should be VPU configuration
			 * mode (0x7)
			 */
			if (desc->srcTransferMode !=
				(uint8_t) DMA_DESC_SRC_XFER_VPU_CONFIG) {
				valid = false;
			}
			break;
		case TRIG_VPU_DMA_READ0_START:
		case TRIG_VPU_DMA_READ1_START:
		case TRIG_VPU_DMA_READ2_START:
		case TRIG_VPU_DMA_READ3_START:
		case TRIG_VPU_DMA_READ4_START:
		case TRIG_VPU_DMA_READ5_START:
		case TRIG_VPU_DMA_READ6_START:
			/* should be either vpu config  or write to VMEM */
			valid = ((desc->srcTransferMode ==
					(uint8_t)DMA_DESC_SRC_XFER_VPU_CONFIG)
				|| (desc->dstTransferMode ==
					(uint8_t)DMA_DESC_DST_XFER_VMEM));
			break;
		case TRIG_VPU_DMA_STORE0_START:
		case TRIG_VPU_DMA_STORE1_START:
		case TRIG_VPU_DMA_STORE2_START:
		case TRIG_VPU_DMA_STORE3_START:
		case TRIG_VPU_DMA_STORE4_START:
		case TRIG_VPU_DMA_STORE5_START:
		case TRIG_VPU_DMA_STORE6_START:
			/* should be either vpu config or read from VMEM */
			valid = ((desc->srcTransferMode ==
					(uint8_t)DMA_DESC_SRC_XFER_VPU_CONFIG)
				|| (desc->srcTransferMode ==
					(uint8_t) DMA_DESC_SRC_XFER_VMEM));
			break;
		default:
			valid = false;
			break;
	}

	return valid;
}

static int32_t
validate_descriptor(const struct nvpva_dma_descriptor *desc,
		    u32 trigger_mode)
{
	uint32_t ret = 0;
	int32_t retval = 0;

	/** padding related validation */
	if (desc->dstTransferMode == (uint8_t) DMA_DESC_DST_XFER_VMEM) {
		ret |= ((desc->px != 0U) &&
				(desc->px >= desc->tx)) ? 1UL : 0UL;

		ret |= ((desc->py != 0U) &&
				(desc->py >= desc->ty)) ? 1UL : 0UL;
	}

	/** Validate VPU trigger event config */
	ret |= (is_valid_vpu_trigger_mode(desc, trigger_mode)) ? 0UL : 1UL;

	/** Check src/dstADV values with respect to ECET bits */
	ret |= (
		(desc->trigEventMode == (uint8_t) TRIG_EVENT_MODE_DIM4)
		&& ((desc->srcRpt1 == 0U) || (desc->srcRpt2 == 0U)
			|| (desc->dstRpt1 == 0U) ||
			(desc->dstRpt2 == 0U))) ? 1UL : 0UL;

	ret |= (((desc->trigEventMode) == ((uint8_t)TRIG_EVENT_MODE_DIM3)) &&
		((desc->srcRpt1 == 0U) || (desc->dstRpt1 == 0U))) ? 1UL : 0UL;

	/** BL format should be associated with MC only */
	if (desc->srcFormat == 1U) {
		ret |= (!(desc->srcTransferMode ==
			(uint8_t) DMA_DESC_SRC_XFER_MC)) ? 1UL : 0UL;
	}

	if (desc->dstFormat == 1U) {
		ret |= (!(desc->dstTransferMode ==
			(uint8_t) DMA_DESC_DST_XFER_MC)) ? 1UL : 0UL;
	}

	if (ret != 0U)
		retval = -EINVAL;

	return retval;
}
/* User to FW DMA descriptor structure mapping helper */
/* TODO: Need to handle DMA descriptor like dst2ptr and dst2Offset */
static int32_t nvpva_task_dma_desc_mapping(struct pva_submit_task *task,
					   struct pva_hw_task *hw_task,
					   u8 *block_height_log2)
{
	struct nvpva_dma_descriptor *umd_dma_desc = NULL;
	struct pva_dtd_s *dma_desc = NULL;
	int32_t err = 0;
	unsigned int desc_num;
	uint32_t addr = 0U;
	uint32_t size = 0;
	bool is_misr;

	nvpva_dbg_fn(task->pva, "");

	task->special_access = 0;

	for (desc_num = 0U; desc_num < task->num_dma_descriptors; desc_num++) {
		umd_dma_desc = &task->dma_descriptors[desc_num];
		dma_desc = &hw_task->dma_desc[desc_num];
		is_misr = !((task->dma_misr_config.descriptor_mask
			    & PVA_BIT64(desc_num)) == 0U);
		is_misr = is_misr && (task->dma_misr_config.enable != 0U);

		err = validate_descriptor(umd_dma_desc,
					  task->hwseq_config.hwseqTrigMode);
		if (err) {
			task_err(
			    task,
			    "DMA descriptor validation falied");
			goto out;
		}

		err = patch_dma_desc_address(task, umd_dma_desc,
					     dma_desc,
					     desc_num, is_misr,
					     block_height_log2[desc_num]);
		if (err)
			goto out;

		/* DMA_DESC_TRANS CNTL0 */
		dma_desc->transfer_control0 =
			umd_dma_desc->srcTransferMode |
			(umd_dma_desc->srcFormat << 3U) |
			(umd_dma_desc->dstTransferMode << 4U) |
			(umd_dma_desc->dstFormat << 7U);
		/* DMA_DESC_TRANS CNTL1 */
		dma_desc->transfer_control1 =
			umd_dma_desc->bytePerPixel |
			(umd_dma_desc->pxDirection << 2U) |
			(umd_dma_desc->pyDirection << 3U) |
			(umd_dma_desc->boundaryPixelExtension << 4U) |
			(umd_dma_desc->transTrueCompletion << 7U);
		/* DMA_DESC_TRANS CNTL2 */
		if (umd_dma_desc->prefetchEnable &&
		    (umd_dma_desc->tx == 0 || umd_dma_desc->ty == 0 ||
		    umd_dma_desc->srcTransferMode != DMA_DESC_SRC_XFER_MC ||
		    umd_dma_desc->dstTransferMode != DMA_DESC_DST_XFER_VMEM)) {
			/* also ECET must be non zero */
			task_err(task, " Invalid criteria to enable Prefetch");
			return -EINVAL;
		}
		dma_desc->transfer_control2 =
			umd_dma_desc->prefetchEnable |
			(umd_dma_desc->dstCbEnable << 1U) |
			(umd_dma_desc->srcCbEnable << 2U);

		/*!
		 * Block-linear surface offset. Only the surface in dram
		 * can be block-linear.
		 * BLBaseAddress = translate(srcPtr / dstPtr) + surfBLOffset;
		 * transfer_control2.bit[3:7] = BLBaseAddress[1].bit[1:5]
		 * GOB offset in BL mode and corresponds to surface address
		 * bits [13:9]
		 */
		if ((umd_dma_desc->srcFormat == 1U)
		   && (umd_dma_desc->srcTransferMode ==
					DMA_DESC_SRC_XFER_MC)) {
			task->src_surf_base_addr += umd_dma_desc->surfBLOffset;
			dma_desc->transfer_control2 |=
			    (u8)((task->src_surf_base_addr & 0x3E00) >> 6U);
		} else if ((umd_dma_desc->dstFormat == 1U) &&
				(umd_dma_desc->dstTransferMode ==
					DMA_DESC_DST_XFER_MC)) {
			task->dst_surf_base_addr += umd_dma_desc->surfBLOffset;
			dma_desc->transfer_control2 |=
			    (u8)((task->dst_surf_base_addr & 0x3E00) >> 6U);
		}

		if (umd_dma_desc->linkDescId > task->num_dma_descriptors) {
			task_err(task, "invalid link ID");
			return -EINVAL;
		}

		dma_desc->link_did = umd_dma_desc->linkDescId;

		/* DMA_DESC_TX */
		dma_desc->tx = umd_dma_desc->tx;
		/* DMA_DESC_TY */
		dma_desc->ty = umd_dma_desc->ty;
		/* DMA_DESC_DLP_ADV */
		dma_desc->dlp_adv = umd_dma_desc->dstLinePitch;
		/* DMA_DESC_SLP_ADV */
		dma_desc->slp_adv = umd_dma_desc->srcLinePitch;
		/* DMA_DESC_DB_START */
		dma_desc->db_start = umd_dma_desc->dstCbStart;
		/* DMA_DESC_DB_SIZE */
		dma_desc->db_size = umd_dma_desc->dstCbSize;
		/* DMA_DESC_SB_START */
		dma_desc->sb_start = umd_dma_desc->srcCbStart;
		/* DMA_DESC_SB_SIZE */
		dma_desc->sb_size = umd_dma_desc->srcCbSize;
		/* DMA_DESC_TRIG_CH */
		/* TODO: Need to handle this parameter */
		dma_desc->trig_ch_events = 0U;
		/* DMA_DESC_HW_SW_TRIG */
		dma_desc->hw_sw_trig_events =
			umd_dma_desc->trigEventMode |
			(umd_dma_desc->trigVpuEvents << 2U) |
			(umd_dma_desc->descReloadEnable << (8U + 4U));
		/* DMA_DESC_PX */
		dma_desc->px = (uint8_t)umd_dma_desc->px;
		/* DMA_DESC_PY */
		dma_desc->py = (uint8_t)umd_dma_desc->py;
		/* DMA_DESC_FRDA */
		if (umd_dma_desc->dst2Ptr != NVPVA_INVALID_SYMBOL_ID) {
			err = pva_get_sym_offset(&task->client->elf_ctx,
						 task->exe_id,
						 umd_dma_desc->dst2Ptr,
						 &addr,
						 &size);
			if (err) {
				task_err(task,
					 "invalid symbol id in descriptor");
				goto out;
			}

			addr = addr + umd_dma_desc->dst2Offset;
			dma_desc->frda |= ((addr >> 6U) & 0x3FFF);
		}

		/* DMA_DESC_NDTM_CNTL0 */
		dma_desc->cb_ext = (((umd_dma_desc->srcCbStart >> 16) & 0x1) << 0)
			| (((umd_dma_desc->dstCbStart >> 16) & 0x1) << 2)
			| (((umd_dma_desc->srcCbSize >> 16) & 0x1) << 4)
			| (((umd_dma_desc->dstCbSize >> 16) & 0x1) << 6);
		/* DMA_DESC_NS1_ADV & DMA_DESC_ST1_ADV */
		dma_desc->srcpt1_cntl =
			(((umd_dma_desc->srcRpt1 & 0xFF) << 24U) |
			 (umd_dma_desc->srcAdv1 & 0xFFFFFF));
		/* DMA_DESC_ND1_ADV & DMA_DESC_DT1_ADV */
		dma_desc->dstpt1_cntl =
			(((umd_dma_desc->dstRpt1 & 0xFF) << 24U) |
			 (umd_dma_desc->dstAdv1 & 0xFFFFFF));
		/* DMA_DESC_NS2_ADV & DMA_DESC_ST2_ADV */
		dma_desc->srcpt2_cntl =
			(((umd_dma_desc->srcRpt2 & 0xFF) << 24U) |
			 (umd_dma_desc->srcAdv2 & 0xFFFFFF));
		/* DMA_DESC_ND2_ADV & DMA_DESC_DT2_ADV */
		dma_desc->dstpt2_cntl =
			(((umd_dma_desc->dstRpt2 & 0xFF) << 24U) |
			 (umd_dma_desc->dstAdv2 & 0xFFFFFF));
		/* DMA_DESC_NS3_ADV & DMA_DESC_ST3_ADV */
		dma_desc->srcpt3_cntl =
			(((umd_dma_desc->srcRpt3 & 0xFF) << 24U) |
			 (umd_dma_desc->srcAdv3 & 0xFFFFFF));
		/* DMA_DESC_ND3_ADV & DMA_DESC_DT3_ADV */
		dma_desc->dstpt3_cntl =
			(((umd_dma_desc->dstRpt3 & 0xFF) << 24U) |
			 (umd_dma_desc->dstAdv3 & 0xFFFFFF));
	}
out:
	return err;
}

static int
verify_dma_desc_hwseq(struct pva_submit_task *task,
		     struct nvpva_dma_channel *user_ch,
		     struct pva_hw_sweq_blob_s *blob,
		     u32 did)
{
	int err = 0;
	u64 *desc_hwseq_frm = &task->desc_hwseq_frm;
	struct nvpva_dma_descriptor *desc;

	nvpva_dbg_fn(task->pva, "");

	if ((did == 0U)
	|| (did >= NVPVA_TASK_MAX_DMA_DESCRIPTORS)) {
		pr_err("invalid Descritor ID");
		err = -EINVAL;
		goto out;
	}

	did = array_index_nospec((did - 1),
				 NVPVA_TASK_MAX_DMA_DESCRIPTORS);

	if ((*desc_hwseq_frm & (1ULL << did)) != 0ULL)
		goto out;

	*desc_hwseq_frm |= (1ULL << did);

	desc = &task->dma_descriptors[did];

	if ((desc->px != 0U)
	 || (desc->py != 0U)
	 || (desc->descReloadEnable != 0U)) {
		pr_err("invalid descriptor padding");
		err = -EINVAL;
		goto out;
	}

	switch (desc->srcTransferMode) {
	case DMA_DESC_SRC_XFER_VMEM:
		if (((desc->dstTransferMode != DMA_DESC_DST_XFER_MC)
		&& (desc->dstTransferMode != DMA_DESC_DST_XFER_L2RAM))
		|| (desc->dstCbEnable == 1U)) {
			pr_err("invalid dst transfer mode");
			err = -EINVAL;
		}
		break;
	case DMA_DESC_SRC_XFER_L2RAM:
	case DMA_DESC_SRC_XFER_MC:
		if ((desc->dstTransferMode != DMA_DESC_DST_XFER_VMEM)
		|| (desc->srcCbEnable == 1U)) {
			pr_err("invalid src transfer mode");
			err = -EINVAL;
		}
		break;
	case DMA_DESC_SRC_XFER_MMIO:
	case DMA_DESC_SRC_XFER_INVAL:
	case DMA_DESC_SRC_XFER_R5TCM:
	case DMA_DESC_SRC_XFER_RSVD:
	default:
		pr_err("invalid dma desc transfer mode");
		err = -EINVAL;
		break;
	}

	if (err)
		goto out;

	if (user_ch->hwseqTxSelect != 1U)
		goto out;

	if (((desc->srcFormat == 1U)
	|| (desc->dstFormat == 1U))
	   && (blob->f_header.to == 0)) {
		pr_err("invalid tile offset");
		err = -EINVAL;
		goto out;
	}

	if (user_ch->hwseqTraversalOrder == 0) {
		if (((uint32_t)((uint32_t)desc->tx +
				(uint32_t)blob->f_header.pad_l) > 0xFFFFU)
		|| ((uint32_t)((uint32_t)desc->tx +
				(uint32_t)blob->f_header.pad_r) > 0xFFFFU)) {
			pr_err("invalid tx + pad x");
			err = -EINVAL;
		}
	} else if (user_ch->hwseqTraversalOrder == 1) {
		if (((uint32_t)((uint32_t)desc->ty +
				(uint32_t)blob->f_header.pad_t) > 0xFFFFU)
		|| ((uint32_t)((uint32_t)desc->ty +
				(uint32_t)blob->f_header.pad_b) > 0xFFFFU)) {
			pr_err("invalid ty + pad y");
			err = -EINVAL;
		}
	} else {
		pr_err("invalid traversal order");
		err = -EINVAL;
	}
out:
	return err;
}

static inline
uint64_t get_buffer_size_hwseq(struct pva_hwseq_priv_s *hwseq, bool is_dst)
{
	uint64_t mem_size = 0ULL;
	uint8_t head_desc_index = hwseq->dma_descs[0].did1;
	struct pva_dma_task_buffer_info_s *buff_info;

	nvpva_dbg_fn(hwseq->task->pva, "");

	buff_info = &hwseq->task->task_buff_info[head_desc_index];
	if (buff_info == NULL) {
		pr_err("buf_info is null");
		goto out;
	}

	if (is_dst) {
		mem_size = buff_info->dst_buffer_size;
	} else {
		mem_size = buff_info->src_buffer_size;
	}
out:
	return mem_size;
}

static inline
int validate_adv_params(struct nvpva_dma_descriptor *head_desc, bool is_dst)
{
	int err = 0;
	if (is_dst) {
		if (head_desc->srcAdv1 != 0
		|| head_desc->srcAdv2 != 0
		|| head_desc->srcAdv3 != 0
		|| (head_desc->srcRpt1 +
		    head_desc->srcRpt2 +
		    head_desc->srcRpt3) != 0) {
			err = -EINVAL;
		}
	} else {
		if (head_desc->dstAdv1 != 0
		|| head_desc->dstAdv2 != 0
		|| head_desc->dstAdv3 != 0
		|| (head_desc->dstRpt1 +
		    head_desc->dstRpt2 +
		    head_desc->dstRpt3) != 0) {
			err = -EINVAL;
		}
	}
	return err;
}

static
int validate_cb_tiles(struct pva_hwseq_priv_s *hwseq, uint64_t vmem_size)
{
	struct nvpva_dma_descriptor *head_desc = hwseq->head_desc;
	struct nvpva_dma_descriptor *tail_desc = hwseq->tail_desc;

	struct nvpva_dma_descriptor *d0 = (hwseq->hdr->to >= 0) ? head_desc : tail_desc;
	struct nvpva_dma_descriptor *d1 = (hwseq->hdr->to >= 0) ? tail_desc : head_desc;
	uint32_t tx = 0;
	uint32_t ty = 0;
	uint64_t tile_size = 0U;

	nvpva_dbg_fn(hwseq->task->pva, "");

	if (head_desc->dstCbSize > vmem_size) {
		pr_err("symbol size smaller than destination buffer size");
		return -EINVAL;
	}

	if (hwseq->is_split_padding) {

		if (hwseq->is_raster_scan) {
			ty = head_desc->ty;
			if (((d0->tx + hwseq->hdr->pad_l) > 0xFFFFU) ||
			((d1->tx + hwseq->hdr->pad_r) > 0xFFFFU)) {
				pr_err("Invalid Tx + Pad X in HW Sequencer");
				return -EINVAL;
			}
			tx = get_max_uint((d0->tx + hwseq->hdr->pad_l),
					(d1->tx + hwseq->hdr->pad_r));
		} else {
			tx = head_desc->tx;
			if (((d0->ty + hwseq->hdr->pad_t) > 0xFFFFU) ||
			((d1->ty + hwseq->hdr->pad_b) > 0xFFFFU)) {
				pr_err("Invalid Ty + Pad Y in HW Sequencer");
				return -EINVAL;
			}
			ty = get_max_uint((d0->ty + hwseq->hdr->pad_t),
					(d1->ty + hwseq->hdr->pad_b));
		}
	} else {
		tx = get_max_uint(head_desc->tx, tail_desc->tx);
		ty = get_max_uint(head_desc->ty, tail_desc->ty);
	}

	tile_size = (int64_t)(head_desc->dstLinePitch) * (ty - 1) + tx;
	if ((tile_size << head_desc->bytePerPixel) > head_desc->dstCbSize)
	{
		pr_err("VMEM address range validation failed (dst, cb on)");
		return -EINVAL;
	}

	return 0;

}

static inline
int check_vmem_setup(struct nvpva_dma_descriptor *head_desc,
		     int32_t vmem_tile_count, bool is_dst)
{
	if (is_dst) {
		if ((vmem_tile_count > 1) &&
			(head_desc->dstAdv1 != 0
			|| head_desc->dstAdv2 != 0
			|| head_desc->dstAdv3 != 0)) {
			return -EINVAL;
		}
	} else {
		if (vmem_tile_count > 1 &&
			(head_desc->srcAdv1 != 0
			|| head_desc->srcAdv2 != 0
			|| head_desc->srcAdv3 != 0)) {
			return -EINVAL;
		}
	}
	return 0;
}

static
int validate_xfer_mode(struct nvpva_dma_descriptor *dma_desc)
{
	int err = 0;

	switch (dma_desc->srcTransferMode) {
		case (uint8_t)DMA_DESC_SRC_XFER_VMEM:
			if (!((dma_desc->dstTransferMode == (uint8_t)DMA_DESC_DST_XFER_MC)
			|| (dma_desc->dstTransferMode == (uint8_t)DMA_DESC_DST_XFER_L2RAM))
			|| (dma_desc->dstCbEnable == 1U)) {
				pr_err("HWSequncer: Invalid dstTransferMode");
				err = -EINVAL;
			}
			break;
		case (uint8_t)DMA_DESC_SRC_XFER_L2RAM:
		case (uint8_t)DMA_DESC_SRC_XFER_MC:
			if ((dma_desc->dstTransferMode != (uint8_t)DMA_DESC_DST_XFER_VMEM)
			|| (dma_desc->srcCbEnable == 1U)) {
		/*
		 * Source or destination Circular Buffer mode should not
		 * be used for MC or L2 in frame addressing mode due to
		 * rtl bug 3136383
		 */
			pr_err("HW Sequencer: Invalid srcTransferMode");
			err = -EINVAL;
			}
			break;
		default:
			pr_err("Shouldn't be here %d",(int)dma_desc->srcTransferMode);
			err = -EINVAL;
			break;
	}

	return err;
}

static
int validate_dst_vmem(struct pva_hwseq_priv_s *hwseq, int32_t *vmem_tile_count)
{
	int err = 0;
	uint64_t vmem_size = 0U;
	uint32_t tx = 0U;
	uint32_t ty = 0U;
	uint64_t tile_size = 0ULL;
	struct nvpva_dma_descriptor *head_desc = hwseq->head_desc;
	struct nvpva_dma_descriptor *tail_desc = hwseq->tail_desc;

	nvpva_dbg_fn(hwseq->task->pva, "");

	*vmem_tile_count = (head_desc->dstRpt1 + 1) * (head_desc->dstRpt2 + 1)
					* (head_desc->dstRpt3 + 1);

	err = validate_xfer_mode(head_desc);
	if (err != 0) {
		pr_err("Invalid dst transfer mode");
		return -EINVAL;
	}

	err = validate_adv_params(head_desc, true);
	if (err != 0) {
		pr_err("Descriptor source tile looping not allowed");
		return -EINVAL;
	}

	vmem_size = get_buffer_size_hwseq(hwseq, true);
	if (vmem_size == 0U) {
		pr_err("Unable to find vmem size");
		return -EINVAL;
	}

	if (head_desc->dstCbEnable != 0U) {
		err = validate_cb_tiles(hwseq, vmem_size);
		if (err == 0)
			return err;

		pr_err("VMEM address range validation failed for dst vmem with cb");
		return -EINVAL;
	} else {
		if (hwseq->is_split_padding) {
			pr_err("Split padding not supported without circular buffer");
			return -EINVAL;
		}

		err = check_vmem_setup(head_desc, *vmem_tile_count, true);
		if (err != 0) {
			pr_err("Invalid VMEM destination setup");
			return -EINVAL;
		}

		tx = get_max_uint(head_desc->tx, tail_desc->tx);
		ty = get_max_uint(head_desc->ty, tail_desc->ty);
		tile_size = (int64_t)(head_desc->dstLinePitch) * (ty - 1) + tx;
		if (((tile_size << head_desc->bytePerPixel) +
		      head_desc->dst_offset) > vmem_size) {
			pr_err("VMEM address range validation failed (dst, cb off)");
			return -EINVAL;
		}
	}

	return err;
}

static inline
int check_no_padding(struct pva_hwseq_frame_header_s *header)
{
	if ((header->pad_l != 0U)
		|| (header->pad_r != 0U)
		|| (header->pad_t != 0U)
		|| (header->pad_b != 0)) {
		return -EINVAL;
	}
	return 0;
}

static
int validate_src_vmem(struct pva_hwseq_priv_s *hwseq, int32_t *vmem_tile_count)
{
	struct nvpva_dma_descriptor *head_desc = hwseq->head_desc;
	struct nvpva_dma_descriptor *tail_desc = hwseq->tail_desc;
	uint64_t vmem_size = 0U;
	int32_t tx = 0U;
	int32_t ty = 0U;
	int64_t tile_size = 0U;
	int err = 0;

	nvpva_dbg_fn(hwseq->task->pva, "");

	*vmem_tile_count = (head_desc->srcRpt1 + 1) *
		(head_desc->srcRpt2 + 1) *
		(head_desc->srcRpt3 + 1);
	err = validate_xfer_mode(head_desc);
	if (err != 0) {
		pr_err("Invalid dst transfer mode");
		return -EINVAL;
	}

	/* make sure last 3 loop dimensions are not used */
	err = validate_adv_params(head_desc, false);
	if (err != 0) {
		pr_err("Descriptor destination tile looping not allowed");
		return -EINVAL;
	}

	/*
	 * since we don't support output padding,
	 * make sure hwseq program header has none
	 */
	err = check_no_padding(hwseq->hdr);
	if (err != 0) {
		pr_err("invalid padding value in hwseq program");
		return -EINVAL;
	}

	vmem_size = get_buffer_size_hwseq(hwseq, false);

	tx = get_max_uint(head_desc->tx, tail_desc->tx);
	ty = get_max_uint(head_desc->ty, tail_desc->ty);
	tile_size = ((int64_t)(head_desc->srcLinePitch) * (ty - 1) + tx);

	if (head_desc->srcCbEnable) {
		if (head_desc->srcCbSize > vmem_size) {
			pr_err("VMEM symbol size is smaller than the source circular buffer size");
			return -EINVAL;
		}

		if (tile_size > head_desc->srcCbSize) {
			pr_err("VMEM address range validation failed (src, cb on)");
			return -EINVAL;
		}
	} else {
		err = check_vmem_setup(head_desc, *vmem_tile_count, false);
		if (err != 0) {
			pr_err("Invalid VMEM Source setup in hw sequencer");
			return -EINVAL;
		}

		if ((tile_size + head_desc->src_offset) > vmem_size) {
			pr_err("VMEM address range validation failed (src, cb off)");
			return -EINVAL;
		}
	}

	return 0;

}

static
int validate_grid_padding(struct pva_hwseq_grid_info_s*gi)
{
	/* make sure grid is large enough to support defined padding */
	if (gi->pad_x[0] > 0 && gi->pad_x[1] > 0 && gi->grid_size_x < 2) {
		pr_err("horizontal padding/tile count mismatch");
		return -EINVAL;
	}
	if (gi->pad_y[0] > 0 && gi->pad_y[1] > 0 && gi->grid_size_y < 1) {
		pr_err("vertical padding/tile count mismatch");
		return -EINVAL;
	}
	/* validate vertical padding */
	if (gi->tile_y[0] <= get_max_int(gi->pad_y[0], gi->pad_y[1])) {
		pr_err("invalid vertical padding");
		return -EINVAL;
	}
	/* make sure ty is fixed */
	if (gi->tile_y[0] != gi->tile_y[1]) {
		pr_err("tile height cannot change in raster-scan mode");
		return -EINVAL;
	}

	return 0;
}

static
int compute_frame_info(struct pva_hwseq_frame_info_s *fi, struct pva_hwseq_grid_info_s *gi)
{
	int32_t dim_offset = 0;

	if (validate_grid_padding(gi) != 0) {
		return -EINVAL;
	}

	/* update X span (partial) */
	dim_offset  = gi->grid_step_x * (gi->grid_size_x - 1);
	fi->start_x = get_min_int(dim_offset, 0);
	fi->end_x   = get_max_int(dim_offset, 0);
	/* update Y span (full) */
	dim_offset  = gi->grid_step_y * (gi->grid_size_y - 1);
	fi->start_y = get_min_int(dim_offset, 0);
	if (gi->grid_step_y < 0) {
		/*
		 * For reversed scans, when the padding is
		 * applied it will adjust the read offset
		 */
		fi->start_y += gi->pad_y[0];
	}

	fi->end_y   = get_max_int(dim_offset, 0);
	fi->end_y += (gi->tile_y[1] - gi->pad_y[0] - gi->pad_y[1]);

	if (gi->is_split_padding) {
		/* disallow overlapping tiles */
		const int32_t left_tile_x = gi->grid_step_x >= 0 ? gi->tile_x[0] : gi->tile_x[1];

		/* update X span (final) */
		fi->end_x += gi->tile_x[1];
		if (left_tile_x > abs(gi->grid_step_x)) {
			pr_err("sequencer horizontal jump offset smaller than tile width");
			return -EINVAL;
		}
	} else {
		/* compute alternative span from 1st descriptor */
		int32_t alt_start_x;
		int32_t alt_end_x;
		/* validate horizontal padding */
		/* swap pad values if sequencing in reverse */
		const int32_t pad_start = gi->grid_step_x >= 0 ? gi->pad_x[0] : gi->pad_x[1];
		const int32_t pad_end   = gi->grid_step_x >= 0 ? gi->pad_x[1] : gi->pad_x[0];
		/*
		 * update X span (final)
		 *  remove padding since it's already included in tx in this mode
		 */
		fi->end_x += gi->tile_x[1] - gi->pad_x[0] - gi->pad_x[1];
		if (gi->tile_x[0] <= pad_start || gi->tile_x[1] <= pad_end) {
			pr_err("invalid horizontal padding");
			return -EINVAL;
		}

		dim_offset = gi->grid_step_x * (gi->head_tile_count - 1);
		alt_start_x = get_min_int(dim_offset, 0);
		if (gi->grid_step_x < 0) {
			/*
			 * For reversed scans, when the padding is
			 * applied it will adjust the read offset
			 */
			fi->start_x += gi->pad_x[0];
			alt_start_x += gi->pad_x[0];
		}

		alt_end_x   = get_max_int(dim_offset, 0);
		alt_end_x += gi->tile_x[0] - pad_start;
		if (gi->head_tile_count == gi->grid_size_x) {
			/*
			 * if there is only a single tile configuration per grid row
			 * then we should subtract padding at the end below since
			 * repetitions of this single tile will include both pad at
			 * start and end
			 */
			alt_end_x -= pad_end;
		}
		/* pick the conservative span */
		fi->start_x = get_min_int(alt_start_x, fi->start_x);
		fi->end_x   = get_max_int(alt_end_x, fi->end_x);
	}

	return 0;
}

static inline
void swap_frame_boundaries(struct pva_hwseq_frame_info_s *frame_info)
{
	int32_t tmp;
	tmp = frame_info->start_x;
	frame_info->start_x = frame_info->start_y;
	frame_info->start_y = tmp;
	tmp = frame_info->end_x;
	frame_info->end_x = frame_info->end_y;
	frame_info->end_y = tmp;
}

static inline
int check_cb_for_bl_inputs(struct nvpva_dma_descriptor *desc)
{
	if ((desc->srcCbEnable != 0U) && (desc->srcFormat != 0U)) {
		return -EINVAL;
	}
	if ((desc->dstCbEnable != 0U) && (desc->dstFormat != 0U)) {
		return -EINVAL;
	}

	return 0;
}

static
int validate_head_desc_transfer_fmt(struct pva_hwseq_priv_s *hwseq,
				uint16_t frame_line_pitch,
				int64_t frame_buffer_offset)
{
	struct nvpva_dma_descriptor *head_desc = hwseq->head_desc;
	int32_t grid_step_x = 0;

	nvpva_dbg_fn(hwseq->task->pva, "");

	if ((head_desc->srcFormat != 0U) || (head_desc->dstFormat != 0U)) {
		if ((hwseq->is_split_padding) && (hwseq->hdr->to == 0U)) {
			/*
			 * tile offset in pixel/Line Pitch must
			 * be non-zero for BL format
			 */
			pr_err("HWSequncer: Invalid Tile Format");
			return -EINVAL;
		}

		if ((head_desc->srcFormat != 0U) && (head_desc->dstFormat != 0U)) {
			pr_err("BL->BL transfer not permitted");
			return -EINVAL;
		}

		if (check_cb_for_bl_inputs(head_desc) != 0) {
			pr_err("circular buffer not allowed for BL inputs");
			return -EINVAL;
		}

		grid_step_x = hwseq->is_raster_scan ? hwseq->hdr->to : hwseq->colrow->cro;
		if (((frame_buffer_offset % 64) != 0) || ((grid_step_x | frame_line_pitch)
					& (31 >> head_desc->bytePerPixel)) != 0) {
			pr_err("block linear access offsets are misaligned ");
			return -EINVAL;
		}
	}

	return 0;
}

static
int check_padding_tiles(struct nvpva_dma_descriptor *head_desc,
			struct nvpva_dma_descriptor *tail_desc)
{
	if ((head_desc->px != 0U) ||
		(head_desc->py != 0U) ||
		(head_desc->descReloadEnable != 0U)) {

		pr_err("Invalid padding in descriptor");
		return -EINVAL;
	}


	if ((head_desc->tx == 0U) ||
		(head_desc->ty == 0U) ||
		(tail_desc->tx == 0U) ||
		(tail_desc->ty == 0U)) {
		return -EINVAL;
	}

	return 0;
}

static
void dump_frame_info(struct pva_hwseq_priv_s *hwseq,
		     struct pva_hwseq_frame_info_s *frame_info)
{
	nvpva_dbg_fn(hwseq->task->pva, "");
	nvpva_dbg_fn(hwseq->task->pva,"sx=%d", frame_info->start_x);
	nvpva_dbg_fn(hwseq->task->pva,"sy=%d", frame_info->start_y);
	nvpva_dbg_fn(hwseq->task->pva,"ex=%d", frame_info->end_x);
	nvpva_dbg_fn(hwseq->task->pva,"ey=%d", frame_info->end_y);

}
static
void dump_grid_info(struct pva_hwseq_priv_s *hwseq,
		    struct pva_hwseq_grid_info_s *grid_info)
{
	nvpva_dbg_fn(hwseq->task->pva, "");
	nvpva_dbg_fn(hwseq->task->pva, "tile_x[0]=%d", grid_info->tile_x[0]);
	nvpva_dbg_fn(hwseq->task->pva, "tile_x[1]=%d", grid_info->tile_x[1]);
	nvpva_dbg_fn(hwseq->task->pva, "tile_y[0]=%d", grid_info->tile_y[0]);
	nvpva_dbg_fn(hwseq->task->pva, "tile_y[1]=%d", grid_info->tile_y[1]);
	nvpva_dbg_fn(hwseq->task->pva, "pad_x[0]=%d", grid_info->pad_x[0]);
	nvpva_dbg_fn(hwseq->task->pva, "pad_x[1]=%d", grid_info->pad_x[1]);
	nvpva_dbg_fn(hwseq->task->pva, "pad_y[0]=%d", grid_info->pad_y[0]);
	nvpva_dbg_fn(hwseq->task->pva, "pad_y[1]=%d", grid_info->pad_y[1]);
	nvpva_dbg_fn(hwseq->task->pva, "grid_size_x=%d", grid_info->grid_size_x);
	nvpva_dbg_fn(hwseq->task->pva, "grid_size_y=%d", grid_info->grid_size_y);
	nvpva_dbg_fn(hwseq->task->pva, "grid_step_x=%d", grid_info->grid_step_x);
	nvpva_dbg_fn(hwseq->task->pva, "grid_step_y=%d", grid_info->grid_step_y);
	nvpva_dbg_fn(hwseq->task->pva, "head_tile_count=%d", grid_info->head_tile_count);
	nvpva_dbg_fn(hwseq->task->pva, "is_split_padding=%d", grid_info->is_split_padding);
}
static
int validate_dma_boundaries(struct pva_hwseq_priv_s *hwseq)
{
	int err = 0;
	bool sequencing_to_vmem = false;
	int32_t seq_tile_count = 0U;
	uint16_t frame_line_pitch = 0U;
	int64_t frame_buffer_offset = 0;
	int64_t frame_buffer_start = 0U;
	int64_t frame_buffer_end = 0U;
	int64_t frame_buffer_size = 0U;
	struct pva_hwseq_grid_info_s grid_info = {0};
	struct pva_hwseq_frame_info_s frame_info = {0};
	struct nvpva_dma_descriptor *head_desc = hwseq->head_desc;
	struct nvpva_dma_descriptor *tail_desc = hwseq->tail_desc;
	int32_t vmem_tile_count = 0;

	nvpva_dbg_fn(hwseq->task->pva, "");

	if (hwseq->tiles_per_packet > 1U && hwseq->hdr->to == 0U) {
		pr_err("unsupported hwseq program modality: Tile Offset = 0");
		return -EINVAL;
	}

	err = check_padding_tiles(head_desc, tail_desc);
	if (err != 0) {
		pr_err("DMA Descriptors have empty tiles");
		return -EINVAL;
	}

	sequencing_to_vmem = (hwseq->head_desc->dstTransferMode == (uint8_t)DMA_DESC_DST_XFER_VMEM);

	if (sequencing_to_vmem) {
		err = validate_dst_vmem(hwseq, &vmem_tile_count);
	} else {
		err = validate_src_vmem(hwseq, &vmem_tile_count);
	}

	if (err != 0) {
		return -EINVAL;
	}

	/* total count of tiles sequenced */
	seq_tile_count = hwseq->tiles_per_packet * (hwseq->colrow->crr + 1);
	if (vmem_tile_count != seq_tile_count) {
		pr_err("hwseq/vmem tile count mismatch");
		return -EINVAL;
	}

	if (hwseq->is_raster_scan) {
		nvpva_dbg_fn(hwseq->task->pva, "is raster scan");

		grid_info.tile_x[0]       = hwseq->head_desc->tx;
		grid_info.tile_x[1]       = hwseq->tail_desc->tx;
		grid_info.tile_y[0]       = hwseq->head_desc->ty;
		grid_info.tile_y[1]       = hwseq->tail_desc->ty;
		grid_info.pad_x[0]        = hwseq->hdr->pad_l;
		grid_info.pad_x[1]        = hwseq->hdr->pad_r;
		grid_info.pad_y[0]        = hwseq->hdr->pad_t;
		grid_info.pad_y[1]        = hwseq->hdr->pad_b;
		grid_info.grid_size_x      = hwseq->tiles_per_packet;
		grid_info.grid_size_y      = hwseq->colrow->crr + 1;
		grid_info.grid_step_x      = hwseq->hdr->to;
		grid_info.grid_step_y      = hwseq->colrow->cro;
		grid_info.head_tile_count  = hwseq->dma_descs[0].dr1 + 1;
		grid_info.is_split_padding = hwseq->is_split_padding;
		if (compute_frame_info(&frame_info, &grid_info) != 0) {
			pr_err("Error in converting grid to frame");
			return -EINVAL;
		}
	} else {
	/*
	 * vertical-mining mode
	 * this is just raster-scan transposed so let's
	 * transpose the tile and padding
	 */
		nvpva_dbg_fn(hwseq->task->pva, "is vertical mining");
		if (hwseq->is_split_padding) {
			pr_err("vertical mining not supported with split padding");
			return -EINVAL;
		}

		grid_info.tile_x[0]          = hwseq->head_desc->ty;
		grid_info.tile_x[1]          = hwseq->tail_desc->ty;
		grid_info.tile_y[0]          = hwseq->head_desc->tx;
		grid_info.tile_y[1]          = hwseq->tail_desc->tx;
		grid_info.pad_x[0]           = hwseq->hdr->pad_t;
		grid_info.pad_x[1]           = hwseq->hdr->pad_b;
		grid_info.pad_y[0]           = hwseq->hdr->pad_l;
		grid_info.pad_y[1]           = hwseq->hdr->pad_r;
		grid_info.grid_size_x      = hwseq->tiles_per_packet,
		grid_info.grid_size_y      = hwseq->colrow->crr + 1;
		grid_info.grid_step_x      = hwseq->hdr->to;
		grid_info.grid_step_y      = hwseq->colrow->cro;
		grid_info.head_tile_count  = hwseq->dma_descs[0].dr1 + 1;
		grid_info.is_split_padding = false;
		if (compute_frame_info(&frame_info, &grid_info) != 0) {
			pr_err("Error in converting grid to frame");
			return -EINVAL;
		}

		swap_frame_boundaries(&frame_info);
	}

	dump_grid_info(hwseq, &grid_info);
	dump_frame_info(hwseq, &frame_info);
	frame_line_pitch = sequencing_to_vmem ? head_desc->srcLinePitch : head_desc->dstLinePitch;
	frame_buffer_offset = pitch_linear_eq_offset(head_desc,
						     head_desc->surfBLOffset,
						     hwseq->dma_ch->blockHeight,
						     head_desc->bytePerPixel,
						     !sequencing_to_vmem, false);

	if (validate_head_desc_transfer_fmt(hwseq, frame_line_pitch, frame_buffer_offset) != 0) {
		pr_err("Error in validating head Descriptor");
		return -EINVAL;
	}

	frame_buffer_size = get_buffer_size_hwseq(hwseq, !sequencing_to_vmem);
	frame_buffer_start = frame_info.start_y * frame_line_pitch + frame_info.start_x;
	frame_buffer_end = (frame_info.end_y - 1) * frame_line_pitch + frame_info.end_x;

	nvpva_dbg_fn(hwseq->task->pva,"flp=%d, st = %lld, ed=%lld, fbo=%lld, bpp = %d, fbs=%lld",
		frame_line_pitch, frame_buffer_start, frame_buffer_end, frame_buffer_offset,
		head_desc->bytePerPixel, frame_buffer_size);

	/* convert to byte range */
	frame_buffer_start <<= head_desc->bytePerPixel;
	frame_buffer_end <<= head_desc->bytePerPixel;
	if (((frame_buffer_start + frame_buffer_offset) < 0)
	||  ((frame_buffer_end + frame_buffer_offset) > frame_buffer_size)) {
		pr_err("sequencer address validation failed");
		return -EINVAL;
	}

	return err;
}

static int
verify_hwseq_blob(struct pva_submit_task *task,
		  struct nvpva_dma_channel *user_ch,
		  struct nvpva_dma_descriptor *decriptors,
		  uint8_t *hwseqbuf_cpuva,
		  int8_t ch_num)

{
	struct pva_hw_sweq_blob_s *blob;
	struct pva_hwseq_desc_header_s *blob_desc;
	struct pva_hwseq_cr_header_s *cr_header;
	struct pva_hwseq_cr_header_s *end_addr;
	struct pva_hwseq_priv_s *hwseq_info = &task->hwseq_info[ch_num - 1];
	struct pva_dma_hwseq_desc_entry_s *desc_entries = &task->desc_entries[ch_num - 1][0];
	u32 end = user_ch->hwseqEnd * 4;
	u32 start = user_ch->hwseqStart * 4;
	int err = 0;
	u32 i;
	u32 j;
	u32 k;
	u32 cr_count = 0;
	u32 entry_size;
	uintptr_t tmp_addr;
	u32 num_desc_entries;
	u32 num_descriptors;

	nvpva_dbg_fn(task->pva, "");

	blob = (struct pva_hw_sweq_blob_s *)&hwseqbuf_cpuva[start];
	end_addr = (struct pva_hwseq_cr_header_s *)&hwseqbuf_cpuva[end + 4];
	cr_header = &blob->cr_header;
	blob_desc = &blob->desc_header;

	hwseq_info->hdr = &blob->f_header;
	hwseq_info->colrow = &blob->cr_header;
	hwseq_info->task = task;
	hwseq_info->dma_ch = user_ch;
	hwseq_info->is_split_padding = (user_ch->hwseqTxSelect != 0U);
	hwseq_info->is_raster_scan = (user_ch->hwseqTraversalOrder == 0U);

	if ((end <= start)
	   || (((end - start + 4U) < sizeof(*blob)))) {
		pr_err("invalid size of HW sequencer blob");
		err = -EINVAL;
		goto out;
	}

	if (end > task->hwseq_config.hwseqBuf.size) {
		pr_err("blob end greater than buffer size");
		err = -EINVAL;
		goto out;
	}

	if (is_desc_mode(blob->f_header.fid)) {
		if (task->hwseq_config.hwseqTrigMode == NVPVA_HWSEQTM_DMATRIG) {
			pr_err("dma master not allowed");
			err = -EINVAL;
		}

		goto out;
	}

	if (!is_frame_mode(blob->f_header.fid)) {
		pr_err("invalid addressing mode");
		err = -EINVAL;
		goto out;
	}

	cr_count = (blob->f_header.no_cr + 1U);
	if(cr_count > PVA_HWSEQ_COL_ROW_LIMIT) {
		pr_err("number of col/row headers is greater than %d",
			PVA_HWSEQ_COL_ROW_LIMIT);
		err = -EINVAL;
		goto out;
	}

	start += sizeof(blob->f_header);
	end += 4;
	for (i = 0; i < cr_count; i++) {
		num_descriptors = cr_header->dec + 1;
		num_desc_entries = (cr_header->dec + 2) / 2;
		nvpva_dbg_fn(task->pva,
			     "n_descs=%d, n_entries=%d",
			     num_descriptors,
			     num_desc_entries);
		if(num_descriptors > PVA_HWSEQ_DESC_LIMIT) {
			pr_err("number of descriptors is greater than %d",
				PVA_HWSEQ_DESC_LIMIT);
			err = -EINVAL;
			goto out;
		}

		entry_size = num_desc_entries;
		entry_size *= sizeof(struct pva_hwseq_desc_header_s);
		entry_size += sizeof(struct pva_hwseq_cr_header_s);
		if ((start + entry_size) > end) {
			pr_err("row/column entries larger than blob");
			err = -EINVAL;
			goto out;
		}

		nvpva_dbg_fn(task->pva,"entry size=%d", entry_size);
		nvpva_dbg_fn(task->pva,"tiles per packet=%d",
			     hwseq_info->tiles_per_packet);
		for (j = 0, k = 0; j < num_desc_entries; j++) {
			err = verify_dma_desc_hwseq(task,
						    user_ch,
						    blob,
						    blob_desc->did1);
			if (err) {
				pr_err("seq descriptor 1 verification failed");
				goto out;
			}

			desc_entries[k].did = array_index_nospec((blob_desc->did1 -1),
								  NVPVA_TASK_MAX_DMA_DESCRIPTORS);
			desc_entries[k].dr = blob_desc->dr1;
			hwseq_info->tiles_per_packet += (blob_desc->dr1 + 1U);
			nvpva_dbg_fn(task->pva,"tiles per packet=%d", hwseq_info->tiles_per_packet);
			++k;
			if (k >= num_descriptors) {
				++blob_desc;
				break;
			}

			err = verify_dma_desc_hwseq(task,
						    user_ch,
						    blob,
						    blob_desc->did2);
			if (err) {
				pr_err("seq descriptor 2 verification failed");
				goto out;
			}

			desc_entries[k].did = array_index_nospec((blob_desc->did2 -1),
								  NVPVA_TASK_MAX_DMA_DESCRIPTORS);
			desc_entries[k].dr = blob_desc->dr2;
			hwseq_info->tiles_per_packet += (blob_desc->dr2 + 1U);
			nvpva_dbg_fn(task->pva,"tiles per packet=%d", hwseq_info->tiles_per_packet);
			++blob_desc;
		}

		nvpva_dbg_fn(task->pva,"entry size=%d", entry_size);
		nvpva_dbg_fn(task->pva,"tiles per packet=%d", hwseq_info->tiles_per_packet);
		start += entry_size;
		cr_header = (struct pva_hwseq_cr_header_s *)blob_desc;
		tmp_addr = (uintptr_t)blob_desc + sizeof(*cr_header);
		blob_desc = (struct pva_hwseq_desc_header_s *)tmp_addr;
		if (cr_header > end_addr) {
			pr_err("blob size smaller than entries");
			err = -EINVAL;
			goto out;
		}
	}

	hwseq_info->dma_descs = (struct pva_hwseq_desc_header_s *) desc_entries;
	hwseq_info->head_desc = &decriptors[desc_entries[0].did];
	hwseq_info->tail_desc = &decriptors[desc_entries[num_descriptors - 1U].did];
	hwseq_info->verify_bounds = true;
out:
	return err;
}

/* User to FW mapping for DMA channel */
static int
nvpva_task_dma_channel_mapping(struct pva_submit_task *task,
			       struct pva_dma_ch_config_s *ch,
			       u8 *hwseqbuf_cpuva,
			       int8_t ch_num,
			       int32_t hwgen,
			       bool hwseq_in_use)

{
	struct nvpva_dma_channel *user_ch = &task->dma_channels[ch_num - 1];
	struct nvpva_dma_descriptor *decriptors = task->dma_descriptors;
	u32 adb_limit;
	int err = 0;

	nvpva_dbg_fn(task->pva, "");

	if (((user_ch->descIndex > PVA_NUM_DYNAMIC_DESCS) ||
	     ((user_ch->vdbSize + user_ch->vdbOffset) >
	      PVA_NUM_DYNAMIC_VDB_BUFFS))) {
		pr_err("ERR: Invalid Channel control data");
		err = -EINVAL;
		goto out;
	}

	if (hwgen == PVA_HW_GEN1)
		adb_limit = PVA_NUM_DYNAMIC_ADB_BUFFS_T19X;
	else
		adb_limit = PVA_NUM_DYNAMIC_ADB_BUFFS_T23X;

	if ((user_ch->adbSize + user_ch->adbOffset) > adb_limit) {
		pr_err("ERR: Invalid ADB Buff size or offset");
		err =  -EINVAL;
		goto out;
	}

	/* DMA_CHANNEL_CNTL0_CHSDID: DMA_CHANNEL_CNTL0[0] = descIndex + 1;*/
	ch->cntl0 = (((user_ch->descIndex + 1U) & 0xFFU) << 0U);

	/* DMA_CHANNEL_CNTL0_CHVMEMOREQ */
	ch->cntl0 |= ((user_ch->vdbSize & 0xFFU) << 8U);

	/* DMA_CHANNEL_CNTL0_CHBH */
	ch->cntl0 |= ((user_ch->adbSize & 0x1FFU) << 16U);

	/* DMA_CHANNEL_CNTL0_CHAXIOREQ */
	ch->cntl0 |= ((user_ch->blockHeight & 7U) << 25U);

	/* DMA_CHANNEL_CNTL0_CHPREF */
	ch->cntl0 |= ((user_ch->prefetchEnable & 1U) << 30U);

	/* Enable DMA channel */
	ch->cntl0 |= (0x1U << 31U);

	/* DMA_CHANNEL_CNTL1_CHPWT */
	ch->cntl1 = ((user_ch->reqPerGrant & 0x7U) << 2U);

	/* DMA_CHANNEL_CNTL1_CHVDBSTART */
	ch->cntl1 |= ((user_ch->vdbOffset & 0x7FU) << 16U);

	/* DMA_CHANNEL_CNTL1_CHADBSTART */
	if (hwgen == PVA_HW_GEN1)
		ch->cntl1 |= ((user_ch->adbOffset & 0xFFU) << 24U);
	else
		ch->cntl1 |= ((user_ch->adbOffset & 0x1FFU) << 23U);

	ch->boundary_pad = user_ch->padValue;
	if (hwgen == PVA_HW_GEN1)
		goto out;

	/* Applicable only for T23x */

	/* DMA_CHANNEL_CNTL1_CHREP */
	if ((user_ch->chRepFactor) && (user_ch->chRepFactor != 6)) {
		pr_err("ERR: Invalid replication factor");
		err = -EINVAL;
		goto out;
	}

	ch->cntl1 |= ((user_ch->chRepFactor & 0x7U) << 8U);

	/* DMA_CHANNEL_HWSEQCNTL_CHHWSEQSTART */
	ch->hwseqcntl = ((user_ch->hwseqStart & 0xFFU) << 0U);

	/* DMA_CHANNEL_HWSEQCNTL_CHHWSEQEND */
	ch->hwseqcntl |= ((user_ch->hwseqEnd & 0xFFU) << 12U);

	/* DMA_CHANNEL_HWSEQCNTL_CHHWSEQTD */
	ch->hwseqcntl |= ((user_ch->hwseqTriggerDone & 0x3U) << 24U);

	/* DMA_CHANNEL_HWSEQCNTL_CHHWSEQTS */
	ch->hwseqcntl |= ((user_ch->hwseqTxSelect & 0x1U) << 27U);

	/* DMA_CHANNEL_HWSEQCNTL_CHHWSEQTO */
	ch->hwseqcntl |= ((user_ch->hwseqTraversalOrder & 0x1U) << 30U);

	/* DMA_CHANNEL_HWSEQCNTL_CHHWSEQEN */
	ch->hwseqcntl |= ((user_ch->hwseqEnable & 0x1U) << 31U);

	if ((user_ch->hwseqEnable & 0x1U) && hwseq_in_use)
		err = verify_hwseq_blob(task,
					user_ch,
					decriptors,
					hwseqbuf_cpuva,
					ch_num);

out:
	return err;
}

int pva_task_write_dma_info(struct pva_submit_task *task,
			    struct pva_hw_task *hw_task)
{
	int err = 0;
	u8 ch_num = 0L;
	int hwgen = task->pva->version;
	bool is_hwseq_mode = false;
	struct pva_pinned_memory *mem;
	u8 *hwseqbuf_cpuva = NULL;
	u32 i;
	u32 j;
	u32 mask;
	s8 *desc_block_height_log2 = task->desc_block_height_log2;

	nvpva_dbg_fn(task->pva, "");

	memset(task->desc_block_height_log2, -1, sizeof(task->desc_block_height_log2));
	memset(task->hwseq_info, 0, sizeof(task->hwseq_info));

	if (task->num_dma_descriptors == 0L || task->num_dma_channels == 0L) {
		nvpva_dbg_info(task->pva, "pva: no DMA resources: NOOP mode");
		goto out;
	}

	if (task->hwseq_config.hwseqBuf.pin_id != 0U) {
		if (hwgen != PVA_HW_GEN2) {
			/* HW sequencer is supported only in HW_GEN2 */
			err = -EINVAL;
			goto out;
		}

		/* Ensure that HWSeq blob size is valid and within the
		 * acceptable range, i.e. up to 1KB, as per HW Sequencer RAM
		 * size from T23x DMA IAS doc.
		 */
		if ((task->hwseq_config.hwseqBuf.size == 0U) ||
		    (task->hwseq_config.hwseqBuf.size > 1024U)) {
			err = -EINVAL;
			goto out;
		}

		is_hwseq_mode = true;

		/* Configure HWSeq trigger mode selection in DMA Configuration
		 * Register
		 */
		hw_task->dma_info.dma_common_config |=
			(task->hwseq_config.hwseqTrigMode & 0x1U) << 12U;

		mem = pva_task_pin_mem(task,
				       task->hwseq_config.hwseqBuf.pin_id);
		if (IS_ERR(mem)) {
			err = PTR_ERR(mem);
			task_err(task, "failed to pin hwseq buffer");
			goto out;
		}

		hwseqbuf_cpuva = pva_dmabuf_vmap(mem->dmabuf) +
				 task->hwseq_config.hwseqBuf.offset;
		hw_task->dma_info.dma_hwseq_base =
			mem->dma_addr + task->hwseq_config.hwseqBuf.offset;
		hw_task->dma_info.num_hwseq = task->hwseq_config.hwseqBuf.size;
	}

	/* write dma channel info */
	hw_task->dma_info.num_channels = task->num_dma_channels;
	hw_task->dma_info.num_descriptors = task->num_dma_descriptors;
	hw_task->dma_info.descriptor_id = 1U; /* PVA_DMA_DESC0 */
	task->desc_hwseq_frm = 0ULL;

	for (i = 0; i < task->num_dma_channels; i++) {
		struct nvpva_dma_channel *user_ch = &task->dma_channels[i];
		struct nvpva_dma_descriptor *decriptors = task->dma_descriptors;

		if ((user_ch->hwseqEnable == 0U)
		&&  (user_ch->blockHeight != U8_MAX)) {
			u8 did = user_ch->descIndex + 1U;
			while ((did != 0U)
			&& (desc_block_height_log2[did - 1U] == -1)) {
				desc_block_height_log2[did - 1U] =
						user_ch->blockHeight;
				did = decriptors[did - 1U].linkDescId;
			 }
		}

		ch_num = i + 1; /* Channel 0 can't use */
		err = nvpva_task_dma_channel_mapping(
			task,
			&hw_task->dma_info.dma_channels[i],
			hwseqbuf_cpuva,
			ch_num,
			hwgen,
			is_hwseq_mode);
		if (err) {
			task_err(task, "failed to map DMA channel info");
			goto out;
		}

		/* Ensure that HWSEQCNTRL is zero for all dma channels in SW
		 * mode
		 */
		if (!is_hwseq_mode &&
		    (hw_task->dma_info.dma_channels[i].hwseqcntl != 0U)) {
			task_err(task, "invalid HWSeq config in SW mode");
			err = -EINVAL;
			goto out;
		}

		hw_task->dma_info.dma_channels[i].ch_number = ch_num;
		mask = task->dma_channels[i].outputEnableMask;
		for (j = 0; j < 7; j++) {
			u32 *trig = &(hw_task->dma_info.dma_triggers[j]);

			(*trig) |= (((mask >> 2*j) & 1U) << ch_num);
			(*trig) |= (((mask >> (2*j + 1)) & 1U) << (ch_num + 16U));
		}

		hw_task->dma_info.dma_triggers[7] |= (((mask >> 14) & 1U) << ch_num);
		if (hwgen == PVA_HW_GEN2) {
			u32 *trig = &(hw_task->dma_info.dma_triggers[8]);

			(*trig) |= (((mask >> 15) & 1U) << ch_num);
			(*trig) |= (((mask >> 16) & 1U) << (ch_num + 16U));
		}
	}

	err = nvpva_task_dma_desc_mapping(task, hw_task, desc_block_height_log2);
	if (err) {
		task_err(task, "failed to map DMA desc info");
		goto out;
	}

	if (task->pva->version <= PVA_HW_GEN2) {
		for (i = 0; i < task->num_dma_channels; i++) {
			err = 0;
			if(task->hwseq_info[i].verify_bounds)
				err = validate_dma_boundaries(&task->hwseq_info[i]);

			if (err != 0) {
				pr_err("HW Sequncer DMA out of memory bounds");
				err = -EINVAL;
				goto out;
			}
		}
	}

	hw_task->task.dma_info =
		task->dma_addr + offsetof(struct pva_hw_task, dma_info);
	hw_task->dma_info.dma_descriptor_base =
		task->dma_addr + offsetof(struct pva_hw_task, dma_desc);

	hw_task->dma_info.dma_info_version = PVA_DMA_INFO_VERSION_ID;
	hw_task->dma_info.dma_info_size = sizeof(struct pva_dma_info_s);
out:
	if (hwseqbuf_cpuva != NULL)
		pva_dmabuf_vunmap(mem->dmabuf, hwseqbuf_cpuva);

	return err;
}

int pva_task_write_dma_misr_info(struct pva_submit_task *task,
				 struct pva_hw_task *hw_task)
{
	uint32_t common_config = hw_task->dma_info.dma_common_config;
	// MISR channel mask bits in DMA COMMON CONFIG
	uint32_t common_config_ch_mask = PVA_MASK(31, 16);
	/* AXI output enable bit in DMA COMMON CONFIG */
	uint32_t common_config_ao_enable_mask = PVA_BIT(15U);
	/* SW Event select bit in DMA COMMON CONFIG */
	uint32_t common_config_sw_event0 = PVA_BIT(5U);
	/* MISR TO interrupt enable bit in DMA COMMON CONFIG */
	uint32_t common_config_misr_to_enable_mask = PVA_BIT(0U);

	hw_task->dma_info.dma_misr_base = 0U;
	if (task->dma_misr_config.enable != 0U) {
		hw_task->dma_misr_config.ref_addr   =
			task->dma_misr_config.ref_addr;
		hw_task->dma_misr_config.seed_crc0  =
			task->dma_misr_config.seed_crc0;
		hw_task->dma_misr_config.ref_data_1 =
			task->dma_misr_config.ref_data_1;
		hw_task->dma_misr_config.seed_crc1  =
			task->dma_misr_config.seed_crc1;
		hw_task->dma_misr_config.ref_data_2 =
			task->dma_misr_config.ref_data_2;
		hw_task->dma_misr_config.misr_timeout =
			task->dma_misr_config.misr_timeout;

		hw_task->dma_info.dma_misr_base = task->dma_addr +
			offsetof(struct pva_hw_task, dma_misr_config);

		/* Prepare data to be written to DMA COMMON CONFIG register */

		/* Select channels that will participate in MISR computation */
		common_config = ((common_config & ~common_config_ch_mask)
				 | (~task->dma_misr_config.channel_mask << 16U));
		/* Set SW_EVENT0 bit to 0 */
		common_config = (common_config & ~common_config_sw_event0);
		/* Disable AXI output */
		common_config = common_config & ~common_config_ao_enable_mask;
		/* Enable MISR TO interrupts */
		common_config = common_config | common_config_misr_to_enable_mask;

		hw_task->dma_info.dma_common_config = common_config;
	}

	return 0;
}
