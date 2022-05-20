/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <nvgpu/gk20a.h>
#include <nvgpu/regops.h>
#include <nvgpu/log.h>
#include <nvgpu/io.h>
#include <nvgpu/lock.h>
#include <nvgpu/channel.h>
#include <nvgpu/cyclestats.h>

#include "cyclestats_priv.h"

static inline bool is_valid_cyclestats_bar0_offset_gk20a(struct gk20a *g,
							 u32 offset)
{
	/* support only 24-bit 4-byte aligned offsets */
	bool valid = !(offset & 0xFF000003U);

	if (g->allow_all) {
		return true;
	}

	/* whitelist check */
	valid = valid &&
		is_bar0_global_offset_whitelisted_gk20a(g, offset);
	/* resource size check in case there was a problem
	 * with allocating the assumed size of bar0 */
	valid = valid && nvgpu_io_valid_reg(g, offset);
	return valid;
}

void nvgpu_cyclestats_exec(struct gk20a *g,
		struct nvgpu_channel *ch, u32 offset)
{
	void *virtual_address;
	u32 buffer_size;
	bool exit;

	/* GL will never use payload 0 for cycle state */
	if ((ch->cyclestate.cyclestate_buffer == NULL) || (offset == 0U)) {
		return;
	}

	nvgpu_mutex_acquire(&ch->cyclestate.cyclestate_buffer_mutex);

	virtual_address = ch->cyclestate.cyclestate_buffer;
	buffer_size = ch->cyclestate.cyclestate_buffer_size;
	exit = false;

	while (!exit) {
		struct share_buffer_head *sh_hdr;
		u32 min_element_size;

		/* validate offset */
		if (offset + sizeof(struct share_buffer_head) > buffer_size ||
		    offset + sizeof(struct share_buffer_head) < offset) {
			nvgpu_err(g,
				  "cyclestats buffer overrun at offset 0x%x",
				  offset);
			break;
		}

		sh_hdr = (struct share_buffer_head *)
			 ((char *)virtual_address + offset);

		min_element_size =
			U32(sh_hdr->operation == OP_END ?
			 sizeof(struct share_buffer_head) :
			 sizeof(struct nvgpu_cyclestate_buffer_elem));

		/* validate sh_hdr->size */
		if (sh_hdr->size < min_element_size ||
		    offset + sh_hdr->size > buffer_size ||
		    offset + sh_hdr->size < offset) {
			nvgpu_err(g,
				  "bad cyclestate buffer header size at offset 0x%x",
				  offset);
			sh_hdr->failed = U32(true);
			break;
		}

		switch (sh_hdr->operation) {
		case OP_END:
			exit = true;
			break;

		case BAR0_READ32:
		case BAR0_WRITE32:
		{
			struct nvgpu_cyclestate_buffer_elem *op_elem =
				(struct nvgpu_cyclestate_buffer_elem *)sh_hdr;
			bool valid = is_valid_cyclestats_bar0_offset_gk20a(
						g, op_elem->offset_bar0);
			u32 raw_reg;
			u64 mask_orig;
			u64 v;

			if (!valid) {
				nvgpu_err(g,
					   "invalid cycletstats op offset: 0x%x",
					   op_elem->offset_bar0);

				exit = true;
				sh_hdr->failed = U32(exit);
				break;
			}

			mask_orig =
				((1ULL << (op_elem->last_bit + 1)) - 1) &
				~((1ULL << op_elem->first_bit) - 1);

			raw_reg = nvgpu_readl(g, op_elem->offset_bar0);

			switch (sh_hdr->operation) {
			case BAR0_READ32:
				op_elem->data =	((raw_reg & mask_orig)
							>> op_elem->first_bit);
				break;

			case BAR0_WRITE32:
				v = 0;
				if ((unsigned int)mask_orig !=
							~((unsigned int)0)) {
					v = (unsigned int)
						(raw_reg & ~mask_orig);
				}

				v |= ((op_elem->data << op_elem->first_bit)
							& mask_orig);
				nvgpu_writel(g,op_elem->offset_bar0,
					     (unsigned int)v);
				break;
			default:
				/* nop ok?*/
				break;
			}
		}
		break;

		default:
			/* no operation content case */
			exit = true;
			break;
		}
		sh_hdr->completed = U32(true);
		offset += sh_hdr->size;
	}
	nvgpu_mutex_release(&ch->cyclestate.cyclestate_buffer_mutex);
}
