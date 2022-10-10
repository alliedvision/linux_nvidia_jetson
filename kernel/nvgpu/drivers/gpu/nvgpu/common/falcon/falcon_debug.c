/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/timers.h>
#include <nvgpu/falcon.h>
#include <nvgpu/io.h>
#include <nvgpu/static_analysis.h>
#include <nvgpu/string.h>

#include "falcon_debug.h"

#define NV_NVRISCV_DEBUG_BUFFER_MAGIC   0xf007ba11

#define FLCN_DMEM_ACCESS_ALIGNMENT    (4U)

#define NV_ALIGN_DOWN(v, g) ((v) & ~((g) - 1U))

#define NV_IS_ALIGNED(addr, align)	((addr & (align - 1U)) == 0U)

void nvgpu_falcon_dbg_buf_destroy(struct nvgpu_falcon *flcn)
{
	struct gk20a *g = flcn->g;
	struct nvgpu_falcon_dbg_buf *debug_buffer = &flcn->debug_buffer;

	if (debug_buffer->local_buf != NULL) {
		nvgpu_kfree(g, debug_buffer->local_buf);
		debug_buffer->local_buf = NULL;
	}

	debug_buffer->first_msg_received = false;
	debug_buffer->read_offset = 0;
	debug_buffer->buffer_size = 0;
	debug_buffer->dmem_offset = 0;
}

int nvgpu_falcon_dbg_buf_init(struct nvgpu_falcon *flcn,
	u32 debug_buffer_max_size, u32 write_reg_addr, u32 read_reg_addr)
{
	struct gk20a *g = flcn->g;
	struct nvgpu_falcon_dbg_buf *debug_buffer = &flcn->debug_buffer;
	int status = 0;

	/*
	 * Set the debugBufferSize to it's initial value of max size.
	 * We will refine it later once ucode informs us of the size it wants
	 * the debug buffer to be.
	 */
	debug_buffer->buffer_size = debug_buffer_max_size;
	debug_buffer->first_msg_received = false;
	debug_buffer->read_offset = 0;

	/* upon initialisation set the flag to false for all falcon */
	nvgpu_falcon_dbg_error_print_enable(flcn, false);

	if (debug_buffer->local_buf == NULL) {
		/*
		 * Allocate memory for nvgpu-side debug buffer, used for copies
		 * from nvriscv dmem. we make it 1 byte larger than the actual debug
		 * buffer to keep a null character at the end for ease of printing.
		 */
		debug_buffer->local_buf = nvgpu_kzalloc(g, debug_buffer_max_size + 1);

		if (debug_buffer->local_buf == NULL) {
			nvgpu_err(g, "Failed to alloc memory for flcn debug buffer");
			nvgpu_err(g, "status=0x%08x", status);
			status = -ENOMEM;
			goto exit;
		}
	}

	/* Zero out memory in the local debug buffer. */
	memset(debug_buffer->local_buf, 0, debug_buffer_max_size + 1);

	/*
	 * Debug buffer is located at the very end of available DMEM.
	 * NVGPU don't know the exact size until the ucode informs us of
	 * the size it wants, so only make it as large as the metadata
	 * at the end of the buffer.
	 */
	debug_buffer->dmem_offset = g->ops.falcon.get_mem_size(flcn, MEM_DMEM) -
		(u32)(sizeof(struct nvgpu_falcon_dbg_buf_metadata));

	/* The DMEM offset must be 4-byte aligned */
	if (!NV_IS_ALIGNED(debug_buffer->dmem_offset, FLCN_DMEM_ACCESS_ALIGNMENT)) {
		nvgpu_err(g, "metadata DMEM offset is not 4-byte aligned.");
		nvgpu_err(g, "dmem_offset=0x%08x", debug_buffer->dmem_offset);
		status = -EINVAL;
		goto exit;
	}

	/* The DMEM buffer size must be 4-byte aligned */
	if (!NV_IS_ALIGNED(sizeof(struct nvgpu_falcon_dbg_buf_metadata),
			FLCN_DMEM_ACCESS_ALIGNMENT)) {
		nvgpu_err(g, "The debug buffer metadata size is not 4-byte aligned");
		status =  -EINVAL;
		goto exit;
	}

	debug_buffer->read_offset_address  = read_reg_addr;
	debug_buffer->write_offset_address = write_reg_addr;

exit:
	if (status != 0) {
		nvgpu_falcon_dbg_buf_destroy(flcn);
	}
	return status;
}

/*
 * Copy new data from the nvriscv debug buffer to the local buffer.
 * Get all data from the last read offset to the current write offset.
 *
 * @return '0' if data fetched successfully, error otherwise.
 */
static int falcon_update_debug_buffer_from_dmem(struct nvgpu_falcon *flcn,
	u32 write_offset)
{
	struct gk20a *g = flcn->g;
	struct nvgpu_falcon_dbg_buf *debug_buffer = &flcn->debug_buffer;
	u32 first_read_size     = 0;
	u32 second_read_size    = 0;

	/*
	 * Align read offset, since reading DMEM only works with 32-bit words.
	 * We only need to align the offset since dmem_offset is already aligned.
	 * We don't need to align the write offset since nvgpu_falcon_copy_from_dmem
	 * handles unaligned-size reads.
	 */
	u32 read_offset_aligned = NV_ALIGN_DOWN(debug_buffer->read_offset,
			FLCN_DMEM_ACCESS_ALIGNMENT);

	if (write_offset >= debug_buffer->read_offset) {
		first_read_size = write_offset - read_offset_aligned;
		second_read_size = 0;
	} else {
		/* Write offset has wrapped around, need two reads */
		first_read_size = debug_buffer->buffer_size - read_offset_aligned;
		second_read_size = write_offset;
	}

	if (first_read_size > 0) {
		if (read_offset_aligned + first_read_size >
			debug_buffer->buffer_size) {
			nvgpu_err(g,
				"Invalid read (first read) from print buffer attempted!");
			return -EINVAL;
		}

		if (nvgpu_falcon_copy_from_dmem(flcn,
			debug_buffer->dmem_offset + read_offset_aligned,
			debug_buffer->local_buf + read_offset_aligned,
			first_read_size,
			0) != 0) {
			nvgpu_err(g, "Failed to copy debug buffer contents from DMEM");
			return -EINVAL;
		}
	}

	if (second_read_size > 0) {
		if (second_read_size > debug_buffer->buffer_size) {
			nvgpu_err(g,
				"Invalid read (second read) from print buffer attempted!");
			return -EINVAL;
		}

		/*
		 * Wrap around, read from start
		 * Assume dmem_offset is always aligned.
		 */
		if (nvgpu_falcon_copy_from_dmem(flcn, debug_buffer->dmem_offset,
			debug_buffer->local_buf, second_read_size,
			0) != 0) {
			nvgpu_err(g,
				"Failed to copy debug buffer contents from nvriscv DMEM");
			return -EINVAL;
		}
	}

	if (first_read_size == 0 && second_read_size == 0) {
		nvgpu_falcon_dbg(g, "Debug buffer empty, can't read any data!");
		return -EINVAL;
	}

	return 0;
}

/*
 * There is a metadata buffer at the end of the DMEM buffer in nvriscv.
 * It sets the buffer size, the magic number for identification etc.
 *
 */
static int falcon_fetch_debug_buffer_metadata(struct nvgpu_falcon *flcn)
{
	struct gk20a *g = flcn->g;
	struct nvgpu_falcon_dbg_buf *debug_buffer = &flcn->debug_buffer;
	struct nvgpu_falcon_dbg_buf_metadata  buffer_metadata_copy;

	/* DMEM offset will point to metadata initially */
	if (nvgpu_falcon_copy_from_dmem(flcn, debug_buffer->dmem_offset,
		(u8 *)&buffer_metadata_copy, sizeof(buffer_metadata_copy),
		0) != 0) {
		nvgpu_err(g, "Failed to copy debug buffer metadata from nvriscv DMEM");
		return -EINVAL;
	}

	nvgpu_falcon_dbg(g, "metadata magic        - 0x%x",
			buffer_metadata_copy.magic);
	nvgpu_falcon_dbg(g, "metadata buffer size  - 0x%x",
			buffer_metadata_copy.buffer_size);
	nvgpu_falcon_dbg(g, "metadata write offset - 0x%x",
			buffer_metadata_copy.write_offset);
	nvgpu_falcon_dbg(g, "metadata read offset  - 0x%x",
			buffer_metadata_copy.read_offset);

	if (buffer_metadata_copy.magic != NV_NVRISCV_DEBUG_BUFFER_MAGIC) {
		nvgpu_err(g, "Failed to verify magic number in debug buffer");
		nvgpu_err(g, " metadata copied from nvriscv DMEM");
		return -EINVAL;
	}

	if (buffer_metadata_copy.buffer_size >= debug_buffer->buffer_size) {
		nvgpu_err(g, "Debug buffer size requested by ucode too big!");
		return -EINVAL;
	}

	debug_buffer->buffer_size = buffer_metadata_copy.buffer_size;

	/* The DMEM buffer size must be 4-byte aligned */
	if (!NV_IS_ALIGNED(debug_buffer->buffer_size, FLCN_DMEM_ACCESS_ALIGNMENT)) {
		nvgpu_err(g, "The debug buffer size is not 4-byte aligned");
		nvgpu_err(g, "buffer_size=0x%08x", debug_buffer->buffer_size);
		return -EINVAL;
	}

	/*
	 * NVGPU don't want to overwrite the metadata since NVGPU might want to use
	 * it to pass read and write offsets if no registers are available.
	 */
	debug_buffer->dmem_offset -= buffer_metadata_copy.buffer_size;

	/* The DMEM offset must be 4-byte aligned */
	if (!NV_IS_ALIGNED(debug_buffer->dmem_offset, FLCN_DMEM_ACCESS_ALIGNMENT)) {
		nvgpu_err(g, "The debug buffer DMEM offset is not 4-byte aligned.");
		nvgpu_err(g, " dmem_offset=0x%08x", debug_buffer->dmem_offset);
		return -EINVAL;
	}

	return 0;
}

int nvgpu_falcon_dbg_buf_display(struct nvgpu_falcon *flcn)
{
	struct gk20a *g = flcn->g;
	struct nvgpu_falcon_dbg_buf *debug_buffer = &flcn->debug_buffer;
	u8  *buffer_data  = debug_buffer->local_buf;
	u32 write_offset  = nvgpu_readl(g, debug_buffer->write_offset_address);
	u32 itr_Offset    = debug_buffer->read_offset;

	bool is_line_split = false;

	if (debug_buffer->local_buf == NULL) {
		nvgpu_err(g, "Local Debug Buffer not allocated!");
		return -EINVAL;
	}

	if (!debug_buffer->first_msg_received) {
		if (falcon_fetch_debug_buffer_metadata(flcn) != 0) {
			nvgpu_err(g, "Failed to process debug buffer metadata!");
			return -EINVAL;
		}

		debug_buffer->first_msg_received = true;
	}

	if (write_offset >= debug_buffer->buffer_size) {
		nvgpu_err(g, "Invalid write offset (%u >= %u)",
				  write_offset, debug_buffer->buffer_size);
		nvgpu_err(g, "abort Debug buffer display");
		return -EINVAL;
	}

	if (falcon_update_debug_buffer_from_dmem(flcn, write_offset) != 0) {
		nvgpu_falcon_dbg(g, "Failed to fetch debug buffer contents");
		// Return error once Bug 3623500 issue is fixed
		return 0;
	}

	/* Buffer is empty when read_offset == write_offset */
	while (itr_Offset != write_offset) {
		/* Null character is the newline marker in falcon firmware logs */
		if (buffer_data[itr_Offset] != '\0') {
			itr_Offset = (itr_Offset + 1) % debug_buffer->buffer_size;
			if (itr_Offset == 0) {
				is_line_split = true;
			}
		} else {
			int status   = 0;
			u8 *tmp_buf   = NULL;
			u8 *curr_data = NULL;
			u32  buf_size  = 0;

			if (is_line_split) {
				/* Logic to concat the split line into a temp buffer */
				u32 first_chunk_len  = (u32)
					strlen((char *)&buffer_data[debug_buffer->read_offset]);
				u32 second_chunk_len = (u32)strlen((char *)&buffer_data[0]);

				buf_size = first_chunk_len + second_chunk_len + 1;
				tmp_buf  = nvgpu_kzalloc(g, buf_size);

				if (tmp_buf == NULL) {
					status = -ENOMEM;
					nvgpu_err(g,
						"Failed to alloc tmp buf for line-split print %d",
						status);
					return status;
				}

				nvgpu_memcpy(tmp_buf, &buffer_data[debug_buffer->read_offset],
						first_chunk_len + 1);
				strcat((char *)tmp_buf, (char *)&buffer_data[0]);

				/* Set the byte array that gets printed as a string */
				curr_data = tmp_buf;

				/* Reset line-split flag */
				is_line_split = false;
			} else {
				buf_size = (u32)
					strlen((char *)&buffer_data[debug_buffer->read_offset]) + 1;

				/* Set the byte array that gets printed as a string */
				curr_data = &buffer_data[debug_buffer->read_offset];
			}

			/*
			 * if the flag is set to true print the riscv
			 * buffer as error
			 */
			if (debug_buffer->is_prints_as_err == true) {
				nvgpu_err(g, "Flcn-%d Async: %s", flcn->flcn_id,
								curr_data);
			} else {
				nvgpu_falcon_dbg(g, "Flcn-%d Async: %s",
							flcn->flcn_id, curr_data);
			}

			/* Cleanup in case we had to allocate a temp buffer */
			if (tmp_buf != NULL) {
				nvgpu_kfree(g, tmp_buf);
			}

			itr_Offset = (itr_Offset + 1) % debug_buffer->buffer_size;
			debug_buffer->read_offset = itr_Offset;
		}
	}

	nvgpu_writel(g, debug_buffer->read_offset_address,
			debug_buffer->read_offset);

	return 0;
}

void nvgpu_falcon_dbg_error_print_enable(struct nvgpu_falcon *flcn, bool enable)
{
	struct nvgpu_falcon_dbg_buf *debug_buffer = &flcn->debug_buffer;

	debug_buffer->is_prints_as_err = enable;
}
