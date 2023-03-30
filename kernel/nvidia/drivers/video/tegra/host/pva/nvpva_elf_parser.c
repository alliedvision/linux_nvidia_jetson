/*
 * Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
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

#if IS_ENABLED(CONFIG_TEGRA_GRHOST)
#include "stdalign.h"
#else
#define alignof _Alignof /*stdalign.h not found*/
#endif

#include "nvpva_elf_parser.h"
#include <linux/string.h>

#define UINT_MAX (~0U)

/* CERT complains about casts from const char*, so do intermediate cast to
 * void*
 */
static inline const void *to_void(const char *const p)
{
	return (const void *)p;
}

bool image_is_elf(const void *const image)
{
	if (image == NULL)
		return false;

	/* assume little endian format */
	if (ELFMAGIC_LSB == *(const u32 *)image)
		return true;

	return false;
}

bool elf_is_32bit(const void *e)
{
	if (image_is_elf(e)) {
		const struct elf_file_header *efh =
			(const struct elf_file_header *)e;
		if (efh->oclass == ELFCLASS32)
			return true;
	}
	return false;
}

static inline size_t get_table_end(u32 num, u16 entsize, size_t off)
{
	/* We need to ensure the off+(num*entsize) doesn't overflow. Originally
	 * num and entsize are ushort, which C converts to int for multiply so
	 * instead cast them up to u32 or u64
	 */
	size_t end;
	u32 tablesize = num * entsize;

	if (tablesize < num)
		return UZERO; /* wraparound error */

	end = off + tablesize;
	if (end < off)
		return UZERO; /* wraparound error */

	return end;
}

static const struct elf_file_header *elf_file_header(const void *e)
{
	if (!elf_is_32bit(e))
		return NULL;

	return (const struct elf_file_header *)e;
}

static inline const struct elf_section_header *elf_section_table(const void *e)
{
	const struct elf_file_header *efh = elf_file_header(e);
	const char *p = (const char *)e;

	if ((e == NULL) || (efh == NULL))
		return NULL;

	p = &p[efh->shoff];
	/* proper ELF should always have offsets be aligned, but add check just
	 * in case.
	 */
	if (((uintptr_t)(p) % alignof(struct elf_section_header)) != UZERO)
		return NULL; /* pointer not aligned */

	return (const struct elf_section_header *)to_void(p);
}

static size_t elf_section_size(const void *e,
			       const struct elf_section_header *esh)
{
	if ((e == NULL) || (esh == NULL))
		return UZERO;

	return (size_t)esh->size;
}

u32 elf_shnum(const void *e)
{
	const struct elf_file_header *efh = elf_file_header(e);

	if (efh == NULL)
		return UZERO;

	if (efh->shnum == UZERO) {
		/* get value from size of first (empty) section to avoid
		 * recursion, don't call elf_section_header(0)
		 */
		const struct elf_section_header *esh = elf_section_table(e);
		size_t size = elf_section_size(e, esh);

		if (size > UINT_MAX) { /* make sure we don't lose precision */
			return UZERO;
		} else {
			return (u32)size;
		}
	}
	return efh->shnum;
}

const struct elf_section_header *elf_section_header(const void *e,
						    unsigned int index)
{
	const struct elf_section_header *esh = elf_section_table(e);

	if (esh == NULL)
		return NULL;

	if (index >= elf_shnum(e))
		return NULL;

	esh = &esh[index];
	return esh;
}

size_t elf_size(const void *e)
{
	/* different elf writers emit elf in different orders, so look for end
	 * after program headers, section headers, or sections
	 */
	size_t max_size;
	unsigned int i;
	const struct elf_file_header *efh = elf_file_header(e);

	if (efh == NULL)
		return UZERO;

	if (efh->phoff > efh->shoff) {
		max_size =
			get_table_end(efh->phnum, efh->phentsize, efh->phoff);
		if (max_size == UZERO)
			return UZERO; /* wraparound error */
	} else {
		max_size =
			get_table_end(elf_shnum(e), efh->shentsize, efh->shoff);
		if (max_size == UZERO)
			return UZERO; /* wraparound error */
	}
	for (i = UZERO; i < elf_shnum(e); ++i) {
		u32 esh_end;
		const struct elf_section_header *esh = elf_section_header(e, i);

		if (esh == NULL)
			return UZERO;

		esh_end = esh->offset + esh->size;
		if (esh_end < esh->offset)
			return UZERO; /* wraparound error */

		if ((esh->type != SHT_NOBITS) && (esh_end > max_size))
			max_size = esh_end;
	}
	return max_size;
}

static u32 elf_shstrndx(const void *e)
{
	const struct elf_file_header *efh = elf_file_header(e);

	if (efh == NULL)
		return UZERO;

	if (efh->shstrndx == SHN_XINDEX) {
		/* get value from link field of first (empty) section to avoid
		 * recursion, don't call elf_section_header(0)
		 */
		const struct elf_section_header *esh = elf_section_table(e);

		if (esh == NULL)
			return UZERO;

		return esh->link;
	}
	return efh->shstrndx;
}

static const char *elf_string_at_offset(const void *e,
					const struct elf_section_header *eshstr,
					unsigned int offset)
{
	const char *strtab;
	u32 stroffset;

	if ((e == NULL) || (eshstr == NULL))
		return NULL;

	if (eshstr->type != SHT_STRTAB)
		return NULL;

	if (offset >= eshstr->size)
		return NULL;

	strtab = (const char *)e;
	stroffset = eshstr->offset + offset;

	if (stroffset < eshstr->offset)
		return NULL;

	strtab = &strtab[stroffset];
	return strtab;
}

const char *elf_section_name(const void *e,
			     const struct elf_section_header *esh)
{
	const char *name;
	/* get section header string table */
	u32 shstrndx = elf_shstrndx(e);
	const struct elf_section_header *eshstr =
		elf_section_header(e, shstrndx);
	if ((esh == NULL) || (eshstr == NULL))
		return NULL;

	name = elf_string_at_offset(e, eshstr, esh->name);
	return name;
}

const struct elf_section_header *elf_named_section_header(const void *e,
							  const char *name)
{
	const struct elf_section_header *esh;
	unsigned int i;

	if (name == NULL)
		return NULL;

	esh = elf_section_table(e);
	if (esh == NULL)
		return NULL;

	/* iterate through sections till find matching name */
	for (i = UZERO; i < elf_shnum(e); ++i) {
		const char *secname = elf_section_name(e, esh);

		if (secname != NULL) {
			size_t seclen = strlen(secname);

			/* use strncmp to avoid problem with input not being
			 * null-terminated, but then need to check for false
			 * partial match
			 */
			if ((strncmp(secname, name, seclen) == ZERO) &&
			    ((unsigned char)name[seclen]) == UZERO) {
				return esh;
			}
		}
		++esh;
	}
	return NULL;
}

static const struct elf_section_header *elf_typed_section_header(const void *e,
								 u32 type)
{
	unsigned int i;
	const struct elf_section_header *esh = elf_section_table(e);

	if (esh == NULL)
		return NULL;

	/* iterate through sections till find matching type */
	for (i = UZERO; i < elf_shnum(e); ++i) {
		if (esh->type == type)
			return esh;

		++esh;
	}
	return NULL;
}

const struct elf_section_header *elf_offset_section_header(const void *e,
							   u32 offset)
{
	unsigned int i;
	const struct elf_section_header *esh = elf_section_table(e);

	if (esh == NULL)
		return NULL;

	/* iterate through sections till find matching offset */
	for (i = UZERO; i < elf_shnum(e); ++i) {
		if (esh->offset == offset)
			return esh;
		++esh;
	}
	return NULL;
}

const u8 *elf_section_contents(const void *e,
			       const struct elf_section_header *esh)
{
	const u8 *p;

	if ((e == NULL) || (esh == NULL))
		return NULL;

	p = (const u8 *)e;
	return &p[esh->offset];
}

const struct elf_symbol *elf_symbol(const void *e, unsigned int index)
{
	const struct elf_section_header *esh;
	const struct elf_symbol *esymtab;
	const uint8_t *p = e;
	uint8_t align = 0;
	/* get symbol table */
	esh = elf_typed_section_header(e, SHT_SYMTAB);
	if ((esh == NULL) || (esh->entsize == UZERO))
		return NULL;

	if (index >= (esh->size / esh->entsize))
		return NULL;

	align = esh->addralign;
	p = &p[esh->offset];
	if ((align != 0) && (((uintptr_t)(p) % align != UZERO)))
		return NULL;

	esymtab = (const struct elf_symbol *)(p);
	return &esymtab[index];
}

const char *elf_symbol_name(const void *e, const struct elf_section_header *esh,
			    unsigned int index)
{
	const struct elf_section_header *eshstr;
	const struct elf_symbol *esymtab;
	const struct elf_symbol *esym;
	const char *name;
	const char *p;
	uint8_t align = 0;

	if ((esh == NULL) || (esh->entsize == UZERO))
		return NULL;

	if (esh->type != SHT_SYMTAB)
		return NULL;

	if (index >= (esh->size / esh->entsize))
		return NULL;

	/* get string table */
	eshstr = elf_section_header(e, esh->link);
	if (eshstr == NULL)
		return NULL;

	p = (const char *)e;
	align = esh->addralign;
	p = &p[esh->offset];
	if ((align != 0) && (((uintptr_t)(p) % align != UZERO)))
		return NULL;

	esymtab = (const struct elf_symbol *)to_void(p);
	esym = &esymtab[index];
	name = elf_string_at_offset(e, eshstr, esym->name);
	return name;
}

u32 elf_symbol_shndx(const void *e, const struct elf_symbol *esym,
		     unsigned int index)
{
	if ((e == NULL) || (esym == NULL))
		return UZERO;

	if (esym->shndx == SHN_XINDEX) {
		const u8 *contents;
		const u32 *shndx_array;
		const struct elf_section_header *esh =
			elf_typed_section_header(e, SHT_SYMTAB_SHNDX);
		if (esh == NULL || esh->entsize == UZERO)
			return UZERO;

		contents = elf_section_contents(e, esh);
		if (contents == NULL)
			return UZERO;

		if (((uintptr_t)(contents) % alignof(u32)) != UZERO)
			return UZERO;

		shndx_array = (const u32 *)(contents);
		if (index >= (esh->size / esh->entsize))
			return UZERO;

		return shndx_array[index];
	}
	return esym->shndx;
}

const struct elf_program_header *elf_program_header(const void *e,
						    unsigned int index)
{
	const struct elf_file_header *efh = elf_file_header(e);
	const struct elf_program_header *eph;
	const char *p = e;

	if (efh == NULL)
		return NULL;

	if (index >= efh->phnum)
		return NULL;

	p = &p[efh->phoff];
	if (((uintptr_t)(p) % alignof(struct elf_program_header)) != UZERO)
		return NULL;

	eph = (const struct elf_program_header *)to_void(p);
	eph = &eph[index];
	return eph;
}
