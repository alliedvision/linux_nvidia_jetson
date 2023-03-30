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

static int32_t check_address_range(struct nvpva_dma_descriptor const *desc,
				   uint64_t max_size,
				   uint64_t max_size2,
				   bool src_dst,
				   bool dst2)
{
	int32_t err = 0;
	int64_t start = 0;
	int64_t end = 0;
	int64_t offset = 0;
	int64_t offset2 = 0;
	uint32_t i;
	int64_t bppSize = ((int64_t)desc->bytePerPixel == 0) ? 1 :
				((int64_t)desc->bytePerPixel == 1) ? 2 : 4;
	int64_t s[5] = {}; // max 5 dimension loop for DMA
	int64_t last_tx = (int64_t)desc->tx - 1;
	int64_t last_ty = (int64_t)desc->ty - 1;

	/** dummy transfer mode with no data transfer */
	if (desc->tx == 0U)
		return err;

	/** ty = 0 is not allowed */
	if (desc->ty == 0U)
		return -EINVAL;

	/** Source transfer mode take care padding */
	if (src_dst == false) {
		last_tx -= (int64_t)desc->px;
		last_ty -= (int64_t)desc->py;
	}

	/* 1st dimension */
	s[0] = last_tx;
	start = min((s[0]*bppSize), 0LL);
	end = max(((s[0]*bppSize) + (bppSize - 1)), 0LL);
	if (src_dst) {
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

		offset = (int64_t)desc->dst_offset;
		offset2 = (int64_t)desc->dst2Offset;
		/* 3rd destination dim */
		s[2] = ((int64_t)desc->dstAdv1 * (int64_t)desc->dstRpt1);
		/* 4th destination dim */
		s[3] = ((int64_t)desc->dstAdv2 * (int64_t)desc->dstRpt2);
		/* 5th destination dim */
		s[4] = ((int64_t)desc->dstAdv3 * (int64_t)desc->dstRpt3);
	} else {
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

		offset = (int64_t)desc->src_offset;
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
	if ((max_size > UINT_MAX) || !(((offset + start) >= 0)
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

	return err;
}

static int32_t
patch_dma_desc_address(struct pva_submit_task *task,
		      struct nvpva_dma_descriptor *umd_dma_desc,
		      struct pva_dtd_s *dma_desc, u8 desc_id, bool is_misr)
{
	int32_t err = 0;
	uint64_t addr_base = 0;

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
				pva_task_pin_mem(task, umd_dma_desc->srcPtr, false);
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
						  false);
		} else {
			addr_base = 0;
			if ((task->desc_hwseq_frm & (1ULL << desc_id)) == 0ULL)
				err = check_address_range(umd_dma_desc,
							  task->l2_alloc_size,
							  0,
							  false,
							  false);
		}

		if (err)
			goto out;

		break;
	case DMA_DESC_SRC_XFER_VMEM:{
		/* calculate symbol address */
		u32 addr = 0;
		u32 size = 0;

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
					  false);
		if (err) {
			err = -EINVAL;
			task_err(
				task, "ERROR: Invalid offset or address");
			goto out;
		}

		addr_base = addr;
		break;
	}
	case DMA_DESC_SRC_XFER_VPU_CONFIG: {
		u32 addr = 0;
		u32 size = 0;

		/* dest must be null*/
		if ((umd_dma_desc->dstPtr != NVPVA_INVALID_SYMBOL_ID)
		   || (umd_dma_desc->dst2Ptr != NVPVA_INVALID_SYMBOL_ID)) {
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

		if (err) {
			task_err(task, "ERROR: Invalid offset or address");
			goto out;
		}

		addr_base = addr;
		break;
	}
	case DMA_DESC_SRC_XFER_MC: {
		struct pva_pinned_memory *mem =
			pva_task_pin_mem(task, umd_dma_desc->srcPtr, true);
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
						  false);

		if (err) {
			err = -EINVAL;
			task_err(task, "ERROR: address");
			goto out;
		}

		addr_base = mem->dma_addr;
		task->src_surf_base_addr = addr_base;

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
				pva_task_pin_mem(task, umd_dma_desc->dstPtr, false);
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
						  false);
		} else {
			addr_base = 0;
			err = check_address_range(umd_dma_desc,
						  task->l2_alloc_size,
						  0,
						  true,
						  false);
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
					  check_size2);
		if (err) {
			err = -EINVAL;
			task_err(
				task, "ERROR: Invalid offset or address");
			goto out;
		}

		addr_base = addr;
		break;
	}
	case DMA_DESC_DST_XFER_MC: {
		struct pva_pinned_memory *mem =
			pva_task_pin_mem(task, umd_dma_desc->dstPtr, true);
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
					  false);
		if (err) {
			err = -EINVAL;
			task_err(task, "ERROR: address");
			goto out;
		}

		addr_base = mem->dma_addr;
		task->dst_surf_base_addr = addr_base;

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
			//should be either vpu config or read from VMEM
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
					   struct pva_hw_task *hw_task)
{
	struct nvpva_dma_descriptor *umd_dma_desc = NULL;
	struct pva_dtd_s *dma_desc = NULL;
	int32_t err = 0;
	unsigned int desc_num;
	uint32_t addr = 0U;
	uint32_t size = 0;
	bool is_misr;

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

		err = patch_dma_desc_address(task, umd_dma_desc, dma_desc,
					     desc_num, is_misr);
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
	u32 end = user_ch->hwseqEnd * 4;
	u32 start = user_ch->hwseqStart * 4;
	int err = 0;
	u32 i;
	u32 j;
	u32 k;
	u32 cr_count = 0;
	u32 entry_size;
	uintptr_t tmp_addr;

	blob = (struct pva_hw_sweq_blob_s *)&hwseqbuf_cpuva[start];
	end_addr = (struct pva_hwseq_cr_header_s *)&hwseqbuf_cpuva[end + 4];
	cr_header = &blob->cr_header;
	blob_desc = &blob->desc_header;

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
	start += sizeof(blob->f_header);
	end += 4;
	for (i = 0; i < cr_count; i++) {
		u32 num_descriptors = cr_header->dec + 1;
		u32 num_desc_entries = (cr_header->dec + 2) / 2;

		entry_size = num_desc_entries;
		entry_size *= sizeof(struct pva_hwseq_desc_header_s);
		entry_size += sizeof(struct pva_hwseq_cr_header_s);
		if ((start + entry_size) > end) {
			pr_err("row/column entries larger than blob");
			err = -EINVAL;
			goto out;
		}

		for (j = 0, k = 0; j < num_desc_entries; j++) {
			err = verify_dma_desc_hwseq(task,
						    user_ch,
						    blob,
						    blob_desc->did1);
			if (err) {
				pr_err("seq descriptor 1 verification failed");
				goto out;
			}

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

			++blob_desc;
		}

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
				       task->hwseq_config.hwseqBuf.pin_id,
				       false);
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

	err = nvpva_task_dma_desc_mapping(task, hw_task);
	if (err) {
		task_err(task, "failed to map DMA desc info");
		goto out;
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
	// AXI output enable bit in DMA COMMON CONFIG
	uint32_t common_config_ao_enable_mask = PVA_BIT(15U);
	// SW Event select bit in DMA COMMON CONFIG
	uint32_t common_config_sw_event0 = PVA_BIT(5U);
	// MISR TO interrupt enable bit in DMA COMMON CONFIG
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

		// Select channels that will participate in MISR computation
		common_config = ((common_config & ~common_config_ch_mask)
				 | (~task->dma_misr_config.channel_mask << 16U));
		// Set SW_EVENT0 bit to 0
		common_config = (common_config & ~common_config_sw_event0);
		// Disable AXI output
		common_config = common_config & ~common_config_ao_enable_mask;
		// common_config = common_config | common_config_ao_enable_mask;
		// Enable MISR TO interrupts
		common_config = common_config | common_config_misr_to_enable_mask;

		hw_task->dma_info.dma_common_config = common_config;
	}

	return 0;
}
