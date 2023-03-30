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

#include <linux/dma-mapping.h>
#include <linux/types.h>
#include <linux/slab.h>
#include "nvpva_elf_parser.h"
#include "pva_bit_helpers.h"
#include "pva.h"
#include  "hw_vmem_pva.h"
#include "pva_vpu_exe.h"

#define ELF_MAXIMUM_SECTION_NAME 64
#define ELF_EXPORTS_SECTION "EXPORTS"
#define DATA_SECTION_ALIGNMENT 32
#define CODE_SECTION_ALIGNMENT 32

#define MAX_VPU_SEGMENTS 4

#define SWAP_DATA(OUT, IN)                                                     \
	do {                                                                   \
		OUT = PVA_INSERT(PVA_EXTRACT(IN, 31, 24, uint32_t), 7, 0);     \
		OUT |= PVA_INSERT(PVA_EXTRACT(IN, 23, 16, uint32_t), 15, 8);   \
		OUT |= PVA_INSERT(PVA_EXTRACT(IN, 15, 8, uint32_t), 23, 16);   \
		OUT |= PVA_INSERT(PVA_EXTRACT(IN, 7, 0, uint32_t), 31, 24);    \
	} while (0)

/*
 * Define mapping from VPU data, rodata and program sections into
 * corresponding segment types.
 */

static const struct pack_rule {
	const char *elf_section_name;
	int32_t pva_type;
} pack_rules[] = {
	{
		.elf_section_name = ".data",
		.pva_type = PVA_SEG_VPU_DATA,
	}, {
		.elf_section_name = ".rodata",
		.pva_type = PVA_SEG_VPU_DATA,
	}, {
		.elf_section_name = ".text",
		.pva_type = PVA_SEG_VPU_CODE,
	}
};

static int32_t find_pva_ucode_segment_type(const char *section_name, uint32_t addr)
{
	uint32_t i;
	int32_t ret = PVA_SEG_VPU_MAX_TYPE;

	for (i = 0; i < ARRAY_SIZE(pack_rules); i += 1) {
		/* Ignore the suffix of the section name */
		if (strncmp(section_name, pack_rules[i].elf_section_name,
			    strlen(pack_rules[i].elf_section_name)) == 0) {
			ret = pack_rules[i].pva_type;
			break;
		}
	}
	if (ret == PVA_SEG_VPU_DATA) {
		int section_name_len =
			strnlen(section_name, ELF_MAXIMUM_SECTION_NAME);
		int exports_section_name_len =
			strnlen(ELF_EXPORTS_SECTION, ELF_MAXIMUM_SECTION_NAME);
		if (section_name_len >= exports_section_name_len &&
		    strncmp((section_name +
			     (section_name_len - exports_section_name_len)),
			    ELF_EXPORTS_SECTION,
			    exports_section_name_len) == 0) {
			ret = PVA_SEG_VPU_IN_PARAMS;
		} else if (addr == 0xc0000U) {
			ret = PVA_SEG_VPU_IN_PARAMS;
		}
	}

	return ret;
}

void print_segments_info(struct pva_elf_image *elf_img)
{
	pr_info("PVA_SEG_VPU_CODE =%d", PVA_SEG_VPU_CODE);
	pr_info("PVA_SEG_VPU_DATA =%d", PVA_SEG_VPU_DATA);
	pr_info("Code Buffer");
	pr_info("vpu_segments_buffer[PVA_SEG_VPU_CODE]");
	pr_info("code_size = %u",
		elf_img->vpu_segments_buffer[PVA_SEG_VPU_CODE].localsize);
	pr_info("vpu_segments_buffer[PVA_SEG_VPU_DATA]");
	pr_info("data_size = %u",
		elf_img->vpu_segments_buffer[PVA_SEG_VPU_DATA].localsize);
}

int32_t pva_get_sym_offset(struct nvpva_elf_context *d, uint16_t exe_id,
			   uint32_t sym_id, uint32_t *addr, uint32_t *size)
{
	if ((!pva_vpu_elf_is_registered(d, exe_id))
	   || (addr == NULL)
	   || (size == NULL)
	   || (sym_id >= get_elf_image(d, exe_id)->num_symbols)
	   || (sym_id == NVPVA_INVALID_SYMBOL_ID))
		return -EINVAL;

	*addr = get_elf_image(d, exe_id)->sym[sym_id].addr;
	*size = get_elf_image(d, exe_id)->sym[sym_id].size;

	return 0;
}

dma_addr_t phys_get_bin_info(struct nvpva_elf_context *d, uint16_t exe_id)
{
	dma_addr_t addr = 0LL;

	if (pva_vpu_elf_is_registered(d, exe_id))
		addr = get_elf_image(d, exe_id)->vpu_bin_buffer.pa;

	return addr;
}

static int32_t pva_vpu_elf_alloc_mem(struct pva *pva,
				     struct pva_elf_buffer *buffer, size_t size)
{
	dma_addr_t pa = 0U;
	void *va = NULL;

	va = dma_alloc_coherent(&pva->pdev->dev, size, &pa, GFP_KERNEL);
	if (va == NULL)
		goto fail;

	nvpva_dbg_info(pva, "vpu app addr = %llx", (u64)pa);

	buffer->size = size;
	buffer->va = va;
	buffer->pa = pa;

	buffer->alloc_size = size;
	buffer->alloc_va = va;
	buffer->alloc_pa = pa;

	return 0;
fail:
	return -ENOMEM;
}

static int32_t pva_vpu_bin_info_allocate(struct pva *dev,
					 struct pva_elf_image *elf_img)
{
	int32_t ret = 0;
	size_t aligned_size;
	size_t size = sizeof(struct pva_bin_info_s);

	aligned_size = ALIGN(size + 128, 128);

	ret = pva_vpu_elf_alloc_mem(dev,
				    &elf_img->vpu_bin_buffer,
				    aligned_size);
	if (ret) {
		pr_err("Memory allocation failed");
		goto fail;
	}

	elf_img->vpu_bin_buffer.va =
		(void *)ALIGN((uintptr_t)elf_img->vpu_bin_buffer.va, 128);
	elf_img->vpu_bin_buffer.pa = ALIGN(elf_img->vpu_bin_buffer.pa, 128);

	(void)memcpy(elf_img->vpu_bin_buffer.va, (void *)&elf_img->info, size);

fail:

	return ret;
}

static int32_t pva_vpu_allocate_segment_memory(struct pva *dev,
					       struct pva_elf_image *elf_img)
{
	int32_t err = 0;
	int32_t i;
	uint32_t segment_size = 0;

	for (i = 0; i < PVA_SEG_VPU_MAX_TYPE; i++) {
		if (i == PVA_SEG_VPU_IN_PARAMS)
			continue;

		segment_size = elf_img->vpu_segments_buffer[i].localsize;
		if (i == PVA_SEG_VPU_CODE) {
			const u32 cache_size = (dev->version == PVA_HW_GEN1) ?
							     (8 * 1024) :
							     (16 * 1024);

			segment_size += cache_size;
		}
		segment_size = ALIGN(segment_size + 128, 128);
		if (segment_size == 0)
			continue;
		err = pva_vpu_elf_alloc_mem(
			dev, &elf_img->vpu_segments_buffer[i], segment_size);
		if (err) {
			pr_err("Memory allocation failed");
			break;
		}
		elf_img->vpu_segments_buffer[i].va = (void *)ALIGN(
			(uintptr_t)elf_img->vpu_segments_buffer[i].va, 128);

		elf_img->vpu_segments_buffer[i].pa =
			ALIGN(elf_img->vpu_segments_buffer[i].pa, 128);

		memcpy(elf_img->vpu_segments_buffer[i].va,
		       elf_img->vpu_segments_buffer[i].localbuffer,
		       elf_img->vpu_segments_buffer[i].localsize);

		kfree(elf_img->vpu_segments_buffer[i].localbuffer);
		elf_img->vpu_segments_buffer[i].localbuffer = NULL;
		elf_img->vpu_segments_buffer[i].localsize = 0;
	}

	return err;
}

static int32_t
pva_allocate_data_section_info(struct pva *dev,
			       struct pva_elf_image *elf_img)
{
	int32_t err = 0;

	if (elf_img->vpu_data_segment_info.localsize == 0U)
		goto out;

	err = pva_vpu_elf_alloc_mem(dev,
				    &elf_img->vpu_data_segment_info,
				    elf_img->vpu_data_segment_info.localsize);
	if (err != 0) {
		pr_err("Failed to allocate data segment info memory");
		goto out;
	}

	(void)memset(elf_img->vpu_data_segment_info.va, 0,
		     elf_img->vpu_data_segment_info.size);

	(void)memcpy(elf_img->vpu_data_segment_info.va,
		     (void *)elf_img->vpu_data_segment_info.localbuffer,
		     elf_img->vpu_data_segment_info.localsize);

	kfree(elf_img->vpu_data_segment_info.localbuffer);
	elf_img->vpu_data_segment_info.localbuffer = NULL;
	elf_img->vpu_data_segment_info.localsize = 0;

out:

	return err;

}

static int32_t write_bin_info(struct nvpva_elf_context *d,
			      struct pva_elf_image *elf_img)
{
	struct pva_bin_info_s *curr_bin_info;
	int32_t err = 0;

	err = pva_vpu_allocate_segment_memory(d->dev, elf_img);
	if (err < 0) {
		pr_err("pva: failed to allocate segment memory");
		goto fail;
	}

	err = pva_allocate_data_section_info(d->dev, elf_img);
	if (err < 0) {
		pr_err("Failed to allocate data segment info memory");
		goto fail;
	}

	curr_bin_info = &elf_img->info;

	curr_bin_info->bin_info_size = sizeof(struct pva_bin_info_s);
	curr_bin_info->bin_info_version = PVA_BIN_INFO_VERSION_ID;
	curr_bin_info->code_base =
		elf_img->vpu_segments_buffer[PVA_SEG_VPU_CODE].pa;
	curr_bin_info->data_sec_base =
		elf_img->vpu_data_segment_info.pa;
	curr_bin_info->data_sec_count =
		 elf_img->vpu_data_segment_info.num_segments;
	curr_bin_info->data_base =
		elf_img->vpu_segments_buffer[PVA_SEG_VPU_DATA].pa;

fail:

	return err;
}

static int32_t copy_to_elf_buffer_code(struct pva_elf_buffer *buffer,
				       const void *src, size_t src_size,
				       uint32_t addr)
{
	uint32_t addr_bytes = addr * 4;
	uint32_t *dst_size = NULL;

	dst_size = &buffer->localsize;
	if (addr_bytes + src_size > *dst_size) {
		size_t aligned_size = addr_bytes + src_size;

		if (aligned_size % DATA_SECTION_ALIGNMENT)
			aligned_size +=
				(DATA_SECTION_ALIGNMENT -
				 (aligned_size % DATA_SECTION_ALIGNMENT));

		if (buffer->localbuffer == NULL) {
			/* First .text section must load at 0 */
			if (addr_bytes != 0) {
				pr_err("First .text section does not start at 0");
				return -EINVAL;
			}
			buffer->localbuffer = kzalloc(aligned_size, GFP_KERNEL);
			if (buffer->localbuffer == NULL)
				return -ENOMEM;

		} else {
			uint8_t *new_buffer = kzalloc(aligned_size, GFP_KERNEL);

			if (new_buffer == NULL)
				return -ENOMEM;

			memcpy(new_buffer, buffer->localbuffer, *dst_size);
			kfree(buffer->localbuffer);
			buffer->localbuffer = new_buffer;
		}
		*dst_size = aligned_size;
	}
	memcpy((void *)((uintptr_t)buffer->localbuffer + addr_bytes), src,
	       src_size);
	return 0;
}

static int32_t copy_to_elf_buffer(struct pva_elf_buffer *buffer,
				  const void *src, size_t src_size)
{
	uint8_t *resize_buffer = NULL;
	uint32_t *dst_size = NULL;

	dst_size = &buffer->localsize;
	if (src != NULL) {
		size_t aligned_size = src_size;

		if (src_size % DATA_SECTION_ALIGNMENT)
			aligned_size += (DATA_SECTION_ALIGNMENT -
					 (src_size % DATA_SECTION_ALIGNMENT));

		if (buffer->localbuffer == NULL) {
			buffer->localbuffer = kzalloc(aligned_size, GFP_KERNEL);
			if (buffer->localbuffer == NULL)
				return -ENOMEM;

		} else {
			resize_buffer = kzalloc(*dst_size, GFP_KERNEL);
			if (resize_buffer == NULL)
				return -ENOMEM;

			memcpy(resize_buffer, buffer->localbuffer, *dst_size);
			kfree(buffer->localbuffer);
			buffer->localbuffer = NULL;
			buffer->localbuffer =
				kzalloc((*dst_size) + aligned_size, GFP_KERNEL);
			if (buffer->localbuffer == NULL) {
				kfree(resize_buffer);
				return -ENOMEM;
			}
			memcpy(buffer->localbuffer, resize_buffer, *dst_size);
			kfree(resize_buffer);
		}
		memcpy((void *)((uintptr_t)buffer->localbuffer + *dst_size),
		       src, src_size);
		*dst_size += aligned_size;
	} else {
		return -EINVAL;
	}

	return 0;
}

static int32_t
copy_to_elf_data_sec_buffer(struct pva_elf_buffer *buffer,
			    const void *src,
			    uint32_t src_size)
{
	uint8_t *resize_buffer = NULL;
	uint32_t *dst_size = NULL;

	dst_size = &buffer->localsize;

	if ((src == NULL) || (src_size == 0U))
		return -EINVAL;

	if (buffer->localbuffer == NULL) {
		buffer->localbuffer = kzalloc(src_size, GFP_KERNEL);
		if (buffer->localbuffer == NULL)
			return -ENOMEM;
	} else {
		resize_buffer = kzalloc((*dst_size) + src_size, GFP_KERNEL);
		if (resize_buffer == NULL)
			return -ENOMEM;

		(void) memcpy(resize_buffer, buffer->localbuffer, *dst_size);
		kfree(buffer->localbuffer);
		buffer->localbuffer = resize_buffer;
	}

	(void) memcpy((void *)(buffer->localbuffer + *dst_size), src, src_size);
	if ((UINT_MAX - *dst_size) < src_size)
		return -EINVAL;

	*dst_size += src_size;

	return 0;
}

static int32_t
copy_segments(void *elf, struct pva_elf_image *elf_img,
		const struct elf_section_header *section_header,
		const char *section_name, int hw_gen)
{
	int32_t segment_type = 0U;
	int32_t ret = 0;

	const u8 *elf_data;
	uint32_t *data;
	uint32_t sw_data;
	uint32_t dst_buffer_size_old = 0;
	struct pva_bin_info_s *bin_info = NULL;
	struct pva_elf_buffer *buffer = NULL;
	struct pva_vpu_data_section_s data_sec_info = {0};

	if ((section_header == NULL) || (section_name == NULL)) {
		ret = -EINVAL;
		goto out;
	}

	segment_type = find_pva_ucode_segment_type(section_name,
						   section_header->addr);

	bin_info = &elf_img->info;
	if (!(segment_type == PVA_SEG_VPU_DATA) &&
	    !(segment_type == PVA_SEG_VPU_CODE)) {
		ret = 0;
		goto out;
	}

	buffer = &elf_img->vpu_segments_buffer[segment_type];
	dst_buffer_size_old = buffer->localsize;
	elf_data = elf_section_contents(elf, section_header);
	if (elf_data == NULL)
		goto inc_num_segments;



	if (segment_type == (int32_t)PVA_SEG_VPU_CODE) {
		unsigned int idx;

		for (idx = 0; idx < (section_header->size / 4); idx++) {
			data = ((uint32_t *)elf_data) + idx;
			SWAP_DATA(sw_data, *data);
			*data = sw_data;
		}

		ret = copy_to_elf_buffer_code(buffer,
					      elf_data,
					      section_header->size,
					      section_header->addr);
		if (ret != 0)
			goto out;

		bin_info->code_size = buffer->localsize;

	} else {
		ret = copy_to_elf_buffer(buffer,
					 elf_data,
					 section_header->size);
		if (ret != 0)
			goto out;
	}

	if (segment_type == (int32_t)PVA_SEG_VPU_DATA) {
		struct pva_vpu_data_section_s *pdsec;
		struct pva_elf_buffer *buffer_temp;
		u32 size_temp;

		pdsec = &data_sec_info;
		pdsec->offset = dst_buffer_size_old;
		pdsec->addr = section_header->addr;
		if (buffer->localsize < dst_buffer_size_old) {
			pr_err("Invalid buffer size");
			ret = -EINVAL;
			goto out;
		}

		pdsec->size = (buffer->localsize - dst_buffer_size_old);
		ret = nvpva_validate_vmem_offset(pdsec->addr,
						 pdsec->size,
						 hw_gen);
		if (ret != 0)
			goto out;

		buffer_temp = &elf_img->vpu_data_segment_info;
		size_temp = (uint32_t)sizeof(struct pva_vpu_data_section_s);
		ret = copy_to_elf_data_sec_buffer(buffer_temp,
						  &data_sec_info,
						  size_temp);
		if (ret != 0)
			goto out;

		if (buffer_temp->num_segments >= (UINT_MAX - 1U)) {
			ret = -EINVAL;
			pr_err("Number of data segments exceeds UINT_MAX");
			goto out;
		}

		buffer_temp->num_segments++;
	}

inc_num_segments:

	buffer->num_segments++;

out:
	return ret;
}


static int32_t
populate_segments(void *elf, struct pva_elf_image *elf_img,
				 int hw_gen)
{
	const struct elf_section_header *section_header;
	int32_t ret = 0;
	uint32_t index = 0;
	const char *section_name;
	const u32 sectionCount = elf_shnum(elf);

	for (index = 0; index < sectionCount; index++) {
		section_header = elf_section_header(elf, index);
		if (section_header == NULL) {
			ret = -EINVAL;
			goto out;
		}
		section_name = elf_section_name(elf, section_header);
		if (section_header->type == SHT_PROGBITS) {
			ret = copy_segments(elf, elf_img, section_header,
					    section_name, hw_gen);
			if (ret)
				goto out;
		}
	}
out:
	return ret;
}

/**
 * Data about symbol information in EXPORTS sections is present as follows
 * typedef struct {
 *   uint32_t type; From VMEM_TYPE enums
 *   uint32_t addr_offset; Offset from VMEM base
 *   uint32_t size; Size of VMEM region in bytes
 * } vmem_symbol_metadata_t;
 */
static int32_t update_exports_symbol(void *elf, const struct elf_section_header *section_header,
				     struct pva_elf_symbol *symID)
{
	const u8 *data;
	const char *section_name;
	int32_t section_type;

	section_name = elf_section_name(elf, section_header);
	if (section_name == NULL)
		return -EINVAL;

	section_type = find_pva_ucode_segment_type(section_name, section_header->addr);
	if (section_type == PVA_SEG_VPU_IN_PARAMS) {
		uint32_t symOffset = symID->addr - section_header->addr;
		data = elf_section_contents(elf, section_header);
		if (data == NULL)
			return -EINVAL;
		symID->type = *(uint32_t *)&data[symOffset];
		if ((symID->type > (uint32_t)VMEM_TYPE_SYSTEM) ||
		    (symID->type == (uint32_t)VMEM_TYPE_INVALID))
			return -EINVAL;
		symID->addr = *(uint32_t *)&data[symOffset + sizeof(uint32_t)];
		symID->size = *(uint32_t *)&data[symOffset + (2UL * sizeof(uint32_t))];
	}

	return 0;
}

static int32_t
populate_symtab(void *elf, struct nvpva_elf_context *d,
		uint16_t exe_id, int hw_gen)
{
	const struct elf_section_header *section_header;
	const struct elf_section_header *sym_scn;
	int32_t ret = 0;
	const struct elf_symbol *sym;
	uint32_t i, count;
	struct pva_elf_symbol *symID;
	uint32_t num_symbols = 0;
	uint32_t num_sys_symbols = 0;
	uint32_t total_sym_size = 0;
	const char *symname = NULL;
	const char *section_name;
	uint32_t stringsize = 0;
	int32_t sec_type;
	struct pva_elf_image *image;

	section_header =
		(const struct elf_section_header *)elf_named_section_header(
			elf, ".symtab");
	if (section_header == NULL)
		goto update_elf_info;

	count = section_header->size / section_header->entsize;
	for (i = 0; i < count; i++) {
		if (num_symbols >= NVPVA_TASK_MAX_SYMBOLS) {
			ret = -EINVAL;
			goto fail;
		}

		sym = elf_symbol(elf, i);
		if ((sym == NULL)
		   || (ELF_ST_BIND(sym) != STB_GLOBAL)
		   || (ELF_ST_TYPE(sym) == STT_FUNC)
		   || sym->size <= 0)
			continue;

		sym_scn = elf_section_header(elf, sym->shndx);
		if (sym_scn == NULL) {
			ret = -EINVAL;
			goto fail;
		}

		section_name = elf_section_name(elf, sym_scn);
		if (section_name == NULL) {
			ret = -EINVAL;
			goto fail;
		}

		sec_type = find_pva_ucode_segment_type(section_name,
						       sym_scn->addr);
		if (sec_type != PVA_SEG_VPU_IN_PARAMS)
			continue;

		symname = elf_symbol_name(elf, section_header, i);
		if (symname == NULL) {
			ret = -EINVAL;
			goto fail;
		}

		stringsize = strnlen(symname, (ELF_MAX_SYMBOL_LENGTH - 1));
		symID = &get_elf_image(d, exe_id)->sym[num_symbols];
		symID->symbol_name =
			kcalloc(ELF_MAX_SYMBOL_LENGTH,
				sizeof(char), GFP_KERNEL);
		if (symID->symbol_name == NULL) {
			ret = -ENOMEM;
			goto fail;
		}

		(void)strncpy(symID->symbol_name, symname, stringsize);
		symID->symbol_name[stringsize] = '\0';
		if (strncmp(symID->symbol_name,
			   PVA_SYS_INSTANCE_DATA_V1_SYMBOL,
			   ELF_MAX_SYMBOL_LENGTH) == 0) {
			++num_sys_symbols;
			symID->is_sys = true;
		} else
			symID->is_sys = false;

		symID->symbolID = num_symbols;
		symID->size = sym->size;
		symID->addr = sym->value;
		ret = update_exports_symbol(elf, sym_scn, symID);
		if (ret != 0) {
			kfree(symID->symbol_name);
			goto fail;
		}

		num_symbols++;
		total_sym_size += symID->size;
		ret = nvpva_validate_vmem_offset(symID->addr,
						 symID->size,
						 hw_gen);
		if (ret != 0)
			goto fail;
	}

update_elf_info:
	get_elf_image(d, exe_id)->num_symbols = num_symbols;
	get_elf_image(d, exe_id)->num_sys_symbols = num_sys_symbols;
	get_elf_image(d, exe_id)->symbol_size_total = total_sym_size;

	return ret;
fail:
	image = get_elf_image(d, exe_id);
	for (i = 0; i < image->num_symbols; i++) {
		kfree(image->sym[i].symbol_name);
		image->sym[i].symbolID = 0;
		image->sym[i].size = 0;
		image->sym[i].offset = 0;
	}

	return ret;
}

/**
 *
 * Validate if elf file passed is valid
 *
 * @param elf			Buffer containing elf file
 * @param size			Size of buffer containing elf file
 *
 * @return			0 if everything is correct else return error
 */

static int32_t validate_vpu(const void *elf, size_t size)
{
	int32_t err = 0;

	if (!image_is_elf(elf) || !elf_is_32bit(elf)) {
		pr_err("pva: Invalid 32 bit VPU ELF");
		err = -EINVAL;
	}
	return err;
}

static void pva_elf_free_buffer(struct pva *pva, struct pva_elf_buffer *buf)
{
	if (buf->localbuffer != NULL) {
		kfree(buf->localbuffer);
		buf->localbuffer = NULL;
		buf->localsize = 0;
		buf->num_segments = 0;
	}
	if (buf->pa != 0U) {
		dma_free_coherent(&pva->pdev->dev,
				  buf->alloc_size, buf->alloc_va,
				  buf->alloc_pa);
	}
}

static void
vpu_bin_clean(struct pva *dev, struct pva_elf_image *elf_img)
{
	size_t i;

	if (elf_img == NULL)
		return;

	/* Initialize vpu_bin_buffer */
	pva_elf_free_buffer(dev, &elf_img->vpu_bin_buffer);

	pva_elf_free_buffer(dev, &elf_img->vpu_data_segment_info);

	/* Initiaize VPU segments buffer */
	for (i = 0; i < PVA_SEG_VPU_MAX_TYPE; i++)
		pva_elf_free_buffer(dev, &elf_img->vpu_segments_buffer[i]);

	/* clean up symbols */
	for (i = 0; i < elf_img->num_symbols; i++)
		kfree(elf_img->sym[i].symbol_name);

	/* Clean elf img and set everything to 0 */
	memset(elf_img, 0, sizeof(struct pva_elf_image));
}

static int32_t pva_get_vpu_app_id(struct nvpva_elf_context *d, uint16_t *exe_id,
					bool is_system_app)
{
	int32_t ret = 0;
	uint16_t index = 0;
	struct pva_elf_images *images;
	struct pva_elf_image **image;
	int32_t alloc_size;

	mutex_lock(&d->elf_mutex);
	images = d->elf_images;
	image = &images->elf_img[images->num_allocated / ALOC_SEGMENT_SIZE];

	if (images->num_assigned >= MAX_NUM_VPU_EXE) {
		pr_err("No space for more VPU binaries");
		ret = -ENOMEM;
		goto out;
	}

	if (images->num_assigned >= images->num_allocated) {
		alloc_size = ALOC_SEGMENT_SIZE * sizeof(struct pva_elf_image);
		*image = kzalloc(alloc_size, GFP_KERNEL);
		if (*image == NULL) {
			ret = -ENOMEM;
			goto out;
		}

		images->num_allocated += ALOC_SEGMENT_SIZE;
	}


	index = rmos_find_first_zero_bit(d->elf_images->alloctable,
					 MAX_NUM_VPU_EXE);
	if (index == MAX_NUM_VPU_EXE) {
		pr_err("No space for more VPU binaries");
		ret = -ENOMEM;
		goto out;
	}

	*exe_id = index;
	rmos_set_bit32((index%32), &d->elf_images->alloctable[index/32U]);
	++(images->num_assigned);
	get_elf_image(d, *exe_id)->elf_id = *exe_id;
	get_elf_image(d, *exe_id)->is_system_app = is_system_app;
out:
	mutex_unlock(&d->elf_mutex);
	return ret;
}

int32_t
pva_unload_vpu_app(struct nvpva_elf_context *d, uint16_t exe_id, bool locked)
{
	int32_t err = 0;
	struct pva_elf_images *images;

	images = d->elf_images;

	if (!locked)
		mutex_lock(&d->elf_mutex);

	if (exe_id >= MAX_NUM_VPU_EXE) {
		err = -EINVAL;
		goto out;
	}

	if (!rmos_test_bit32((exe_id%32), &images->alloctable[exe_id/32])) {
		err = -EINVAL;
		goto out;
	}

	vpu_bin_clean(d->dev, get_elf_image(d, exe_id));
	rmos_clear_bit32((exe_id%32), &images->alloctable[exe_id/32]);
	--(images->num_assigned);
out:
	if (!locked)
		mutex_unlock(&d->elf_mutex);

	return err;
}

int32_t
pva_get_sym_tab_size(struct nvpva_elf_context *d,
		     uint16_t exe_id,
		     u64 *tab_size)
{
	struct pva_elf_image *image;
	u32 number_of_symbols;

	image = get_elf_image(d, exe_id);
	if (image == NULL)
		return -EINVAL;

	number_of_symbols = image->num_symbols - image->num_sys_symbols;
	*tab_size = number_of_symbols * sizeof(struct nvpva_sym_info);

	return 0;
}

int32_t
pva_get_sym_tab(struct nvpva_elf_context *d,
		  uint16_t exe_id,
		  struct nvpva_sym_info *sym_tab)

{
	u32 i;
	struct pva_elf_image *image;

	image = get_elf_image(d, exe_id);
	if (image == NULL)
		return -EINVAL;

	for (i = 0; i < image->num_symbols; i++) {
		if (image->sym[i].is_sys)
			continue;
		memcpy(sym_tab->sym_name,
		       image->sym[i].symbol_name,
		       NVPVA_SYM_NAME_MAX_LEN);
		sym_tab->sym_size = image->sym[i].size;
		sym_tab->sym_type = image->sym[i].type;
		sym_tab->sym_id   = image->sym[i].symbolID;
		++sym_tab;
	}

	return 0;
}
int32_t pva_get_sym_info(struct nvpva_elf_context *d, uint16_t vpu_exe_id,
		       const char *sym_name, struct pva_elf_symbol *symbol)
{
	struct pva_elf_image *elf;
	uint32_t i;
	int32_t err = 0;
	size_t strSize = strnlen(sym_name, ELF_MAX_SYMBOL_LENGTH);

	elf = get_elf_image(d, vpu_exe_id);
	for (i = 0; i < elf->num_symbols; i++) {
		if (strncmp(sym_name, elf->sym[i].symbol_name, strSize) == 0) {
			symbol->symbolID = elf->sym[i].symbolID;
			symbol->size = elf->sym[i].size;
			symbol->type = elf->sym[i].type;
			break;
		}
	}
	if (i == elf->num_symbols)
		err = -EINVAL;

	return err;
}

int32_t
pva_release_vpu_app(struct nvpva_elf_context *d, uint16_t exe_id, bool locked)
{
	int32_t err = 0;
	struct pva_elf_image *image = NULL;

	image = get_elf_image(d, exe_id);
	if (image != NULL && image->user_registered == true) {
		image->user_registered = false;
		if (atomic_read(&image->submit_refcount) <= 0)
			(void)pva_unload_vpu_app(d, exe_id, locked);
	} else {
		err = -EINVAL;
	}

	return err;
}

int32_t pva_task_release_ref_vpu_app(struct nvpva_elf_context *d,
				     uint16_t exe_id)
{
	int32_t err = 0;
	struct pva_elf_image *image = NULL;

	if (exe_id == NVPVA_NOOP_EXE_ID)
		goto out;

	image = get_elf_image(d, exe_id);
	if (image == NULL) {
		err = -EINVAL;
		goto out_err;
	}

	atomic_sub(1, &image->submit_refcount);
	if ((atomic_read(&image->submit_refcount) <= 0) &&
	    (image->user_registered == false))
		(void)pva_unload_vpu_app(d, exe_id, false);
out_err:
out:
	return err;
}

int32_t pva_task_acquire_ref_vpu_app(struct nvpva_elf_context *d,
				     uint16_t exe_id)
{
	int32_t err = 0;
	struct pva_elf_image *image = get_elf_image(d, exe_id);

	if (image != NULL)
		(void)atomic_add(1, &image->submit_refcount);
	else
		err = -EINVAL;

	return err;
}

int32_t pva_load_vpu_app(struct nvpva_elf_context *d, uint8_t *buffer,
			 size_t size, uint16_t *exe_id,
			 bool is_system_app, int hw_gen)
{
	void *elf = NULL;
	int32_t err = 0;
	uint16_t assigned_exe_id = 0;
	struct pva_elf_image *image = NULL;
	struct pva *pva = d->dev;
	struct device *dev = &pva->pdev->dev;

	err = validate_vpu((void *)buffer, size);
	if (err < 0) {
		dev_err(dev, "Not valid elf or null elf");
		goto out;
	}
	err = pva_get_vpu_app_id(d, &assigned_exe_id, is_system_app);
	if (err) {
		dev_err(dev, "Unable to get valid VPU id");
		goto out;
	}
	elf = (void *)buffer;
	image = get_elf_image(d, assigned_exe_id);
	err = populate_symtab(elf, d, assigned_exe_id, pva->version);
	if (err) {
		dev_err(dev, "Populating symbol table failed");
		err = -EINVAL;
		goto out_elf_end;
	}
	err = populate_segments(elf, image, hw_gen);
	if (err) {
		dev_err(dev, "Populating segments failed");
		err = -EINVAL;
		goto out_elf_end;
	}
	err = write_bin_info(d, image);
	if (err) {
		dev_err(dev, "Writing bin_info failed");
		err = -EINVAL;
		goto out_elf_end;
	}
	err = pva_vpu_bin_info_allocate(pva, image);
	if (err) {
		dev_err(dev, "Allocating bin info failed");
		err = -EINVAL;
		goto out_elf_end;
	}
	*exe_id = assigned_exe_id;
	image->user_registered = true;
	(void)atomic_set(&image->submit_refcount, 0);
out_elf_end:
	if (err)
		pva_unload_vpu_app(d, assigned_exe_id, false);

out:
	return err;
}

void pva_unload_all_apps(struct nvpva_elf_context *d)
{
	uint32_t elf_alloc_table = 0U;
	uint32_t id = 0U;
	uint32_t i;

	mutex_lock(&d->elf_mutex);
	for (i = 0; i < NUM_ALLOC_SEGMENTS; i++) {
		elf_alloc_table = d->elf_images->alloctable[i];
		while (elf_alloc_table != 0U) {
			id = rmos_get_first_set_bit(elf_alloc_table);
			(void)pva_release_vpu_app(d, (i * 32 + id), true);
			rmos_clear_bit32(id, &elf_alloc_table);
		}

		d->elf_images->alloctable[i] = 0;
	}
	mutex_unlock(&d->elf_mutex);
}

void pva_vpu_deinit(struct nvpva_elf_context *d)
{
	int32_t i;
	int32_t allocated_segments;
	struct pva_elf_images *images = d->elf_images;

	if (d->elf_images == NULL)
		goto out;

	pva_unload_all_apps(d);
	allocated_segments = (images->num_allocated/ALOC_SEGMENT_SIZE);
	for (i = 0; i < allocated_segments; i++) {
		if (images->elf_img[i] != NULL) {
			kfree(images->elf_img[i]);
			images->elf_img[i] = NULL;
		}
	}

	d->elf_images->num_allocated = 0;
	d->elf_images->num_assigned = 0;

	kfree(d->elf_images);
	d->elf_images = NULL;
	mutex_destroy(&d->elf_mutex);
out:
	return;
}

int32_t pva_vpu_init(struct pva *dev, struct nvpva_elf_context *d)
{
	int32_t err = 0;
	int32_t alloc_size;

	d->dev = dev;
	d->elf_images = kzalloc(sizeof(struct pva_elf_images), GFP_KERNEL);
	if (d->elf_images == NULL) {
		err = -ENOMEM;
		goto fail_elf_img_init;
	}

	d->elf_images->num_allocated = 0;
	d->elf_images->num_assigned = 0;
	memset(d->elf_images->elf_img, 0, sizeof(d->elf_images->elf_img));
	alloc_size = ALOC_SEGMENT_SIZE * sizeof(struct pva_elf_image);
	d->elf_images->elf_img[0] = kzalloc(alloc_size, GFP_KERNEL);
	if (d->elf_images->elf_img[0] == NULL) {
		err = -ENOMEM;
		kfree(d->elf_images);
		goto fail_elf_img_init;
	}

	d->elf_images->num_allocated = ALOC_SEGMENT_SIZE;
	mutex_init(&d->elf_mutex);

fail_elf_img_init:

	return err;
}
struct vmem_region {
	uint32_t start;
	uint32_t end;
};

struct vmem_region vmem_regions_tab[NUM_HEM_GEN + 1][VMEM_REGION_COUNT] = {
	{{.start = 0, .end = 0},
	 {.start = 0, .end = 0},
	 {.start = 0, .end = 0}},
	{{.start = T19X_VMEM0_START, .end = T19X_VMEM0_END},
	 {.start = T19X_VMEM1_START, .end = T19X_VMEM1_END},
	 {.start = T19X_VMEM2_START, .end = T19X_VMEM2_END}},
	{{.start = T23x_VMEM0_START, .end = T23x_VMEM0_END},
	 {.start = T23x_VMEM1_START, .end = T23x_VMEM1_END},
	 {.start = T23x_VMEM2_START, .end = T23x_VMEM2_END}},
};

int32_t
nvpva_validate_vmem_offset(const uint32_t vmem_offset,
			   const uint32_t size,
			   const int hw_gen)
{

	int i;
	int32_t err = -EINVAL;

	if (hw_gen < 0 || hw_gen > NUM_HEM_GEN) {
		pr_err("invalid hw_gen index: %d", hw_gen);
		return err;
	}

	for (i = VMEM_REGION_COUNT; i > 0; i--) {
		if (vmem_offset >= vmem_regions_tab[hw_gen][i-1].start)
			break;
	}

	if ((i > 0) && ((vmem_offset + size) <= vmem_regions_tab[hw_gen][i-1].end))
		err = 0;

	return err;
}
