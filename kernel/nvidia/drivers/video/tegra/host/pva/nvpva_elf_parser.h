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

#ifndef NVPVA_ELF_PARSER_H
#define NVPVA_ELF_PARSER_H
#include <linux/types.h>
#include "elf_include_fix.h"

#define ZERO 0
#define UZERO 0U
#define ULLZERO 0ULL

/*---------------------------------- Header ----------------------------------*/

struct elf_file_header {
	u32 magic; /* 0x7f,0x45,0x4c,0x46 */
	u8 oclass; /* Object file class */
	u8 data; /* Data encoding */
	u8 formatVersion; /* Object format version */
	u8 abi; /* OS application binary interface */
	u8 abiVersion; /* Version of abi */
	u8 padd[7]; /* Elf ident padding */
	u16 type; /* Object file type */
	u16 machine; /* Architecture */
	u32 version; /* Object file version */
	u32 entry; /* Entry point virtual address */
	u32 phoff; /* Program header table file offset */
	u32 shoff; /* Section header table file offset */
	u32 flags; /* Processor-specific flags */
	u16 ehsize; /* ELF header size in bytes */
	u16 phentsize; /* Program header table entry size */
	u16 phnum; /* Program header table entry count */
	u16 shentsize; /* Section header table entry size */
	u16 shnum; /* Section header table entry count */
	u16 shstrndx; /* Section header string table index */
};

#define ELFMAGIC 0x7f454c46U /* This is in big endian */
#define ELFMAGIC_LSB 0x464c457fU /* This is in little endian */
#define ELFCLASS32 1U /* 32 bit object file */

#define EV_NONE 0 /* Invalid version */
#define EV_CURRENT 1 /* Current version */

/*---------------------------------- Section ---------------------------------*/

struct elf_section_header {
	u32 name; /* Section name, string table index */
	u32 type; /* Type of section */
	u32 flags; /* Miscellaneous section attributes */
	u32 addr; /* Section virtual addr at execution */
	u32 offset; /* Section file offset */
	u32 size; /* Size of section in bytes */
	u32 link; /* Index of another section */
	u32 info; /* Additional section information */
	u32 addralign; /* Section alignment */
	u32 entsize; /* Entry size if section holds table */
};

/*
 * Type
 */
#define SHT_NULL 0x00U /* NULL section */
#define SHT_PROGBITS 0x01U /* Loadable program data */
#define SHT_SYMTAB 0x02U /* Symbol table */
#define SHT_STRTAB 0x03U /* String table */
#define SHT_RELA 0x04U /* Relocation table with addents */
#define SHT_HASH 0x05U /* Hash table */
#define SHT_DYNAMIC 0x06U /* Information for dynamic linking */
#define SHT_NOTE 0x07U /* Information that marks file */
#define SHT_NOBITS 0x08U /* Section does not have data in file */
#define SHT_REL 0x09U /* Relocation table without addents */
#define SHT_SHLIB 0x0aU /* Reserved */
#define SHT_DYNSYM 0x0bU /* Dynamic linker symbol table */
#define SHT_INIT_ARRAY 0x0eU /* Array of pointers to init funcs */
#define SHT_FINI_ARRAY 0x0fU /* Array of function to finish funcs */
#define SHT_PREINIT_ARRAY 0x10U /* Array of pointers to pre-init functions */
#define SHT_GROUP 0x11U /* Section group */
#define SHT_SYMTAB_SHNDX 0x12U /* Table of 32bit symtab shndx */
#define SHT_LOOS 0x60000000U /* Start OS-specific. */
#define SHT_HIOS 0x6fffffffU /* End OS-specific type */
#define SHT_LOPROC 0x70000000U /* Start of processor-specific */
#define SHT_HIPROC 0x7fffffffU /* End of processor-specific */
#define SHT_LOUSER 0x80000000U /* Start of application-specific */
#define SHT_HIUSER 0x8fffffffU /* End of application-specific */

/*
 * Special section index
 */
#define SHN_UNDEF 0U /* Undefined section */
#define SHN_LORESERVE 0xff00U /* lower bound of reserved indexes */
#define SHN_ABS 0xfff1U /* Associated symbol is absolute */
#define SHN_COMMON 0xfff2U /* Associated symbol is common */
#define SHN_XINDEX 0xffffU /* Index is in symtab_shndx */

/*
 * Special section names
 */
#define SHNAME_SHSTRTAB ".shstrtab" /* section string table */
#define SHNAME_STRTAB ".strtab" /* string table */
#define SHNAME_SYMTAB ".symtab" /* symbol table */
#define SHNAME_SYMTAB_SHNDX ".symtab_shndx" /* symbol table shndx array */
#define SHNAME_TEXT ".text." /* suffix with entry name */

/*---------------------------------- Program Segment -------------------------*/

struct elf_program_header {
	u32 type; /* Identifies program segment type */
	u32 offset; /* Segment file offset */
	u32 vaddr; /* Segment virtual address */
	u32 paddr; /* Segment physical address */
	u32 filesz; /* Segment size in file */
	u32 memsz; /* Segment size in memory */
	u32 flags; /* Segment flags */
	u32 align; /* Segment alignment, file & memory */
};

/*----------------------------------- Symbol ---------------------------------*/

struct elf_symbol {
	u32 name; /* Symbol name, index in string tbl */
	u32 value; /* Value of the symbol */
	u32 size; /* Associated symbol size */
	u8 info; /* Type and binding attributes */
	u8 other; /* Extra flags */
	u16 shndx; /* Associated section index */
};

#define ELF_ST_BIND(s) ((u32)((s)->info) >> 4)
#define ELF_ST_TYPE(s) ((u32)((s)->info) & 0xfU)
#define ELF_ST_INFO(b, t) (((b) << 4) + ((t)&0xfU))

/*
 * Type
 */
#define STT_NOTYPE 0U /* No type known */
#define STT_OBJECT 1U /* Data symbol */
#define STT_FUNC 2U /* Code symbol */
#define STT_SECTION 3U /* Section */
#define STT_FILE 4U /* File */
#define STT_COMMON 5U /* Common symbol */
#define STT_LOOS 10U /* Start of OS-specific */

/*
 * Scope
 */
#define STB_LOCAL 0U /* Symbol not visible outside object */
#define STB_GLOBAL 1U /* Symbol visible outside object */
#define STB_WEAK 2U /* Weak symbol */

/*
 * The following routines that return file/program/section headers
 * all return NULL when not found.
 */

/*
 *  Typical elf readers create a table of information that is passed
 *  to the different routines.  For simplicity, we're going to just
 *  keep the image of the whole file and pass that around.  Later, if we see
 *  a need to speed this up, we could consider changing void * to be something
 *  more complicated.
 */

bool image_is_elf(const void *const image);

bool elf_is_32bit(const void *e);

u32 elf_shnum(const void *e);

const struct elf_section_header *elf_section_header(const void *e,
						    unsigned int index);

const char *elf_section_name(const void *e,
			     const struct elf_section_header *esh);

const struct elf_section_header *elf_named_section_header(const void *e,
							  const char *name);

const u8 *elf_section_contents(const void *e,
			       const struct elf_section_header *esh);

const struct elf_symbol *elf_symbol(const void *e, unsigned int index);

const char *elf_symbol_name(const void *e, const struct elf_section_header *esh,
			    unsigned int index);

const struct elf_program_header *elf_program_header(const void *e,
						    unsigned int index);

u32 elf_symbol_shndx(const void *e, const struct elf_symbol *esym,
		     unsigned int index);

const struct elf_section_header *elf_offset_section_header(const void *e,
							   u32 offset);

size_t elf_size(const void *e);
#endif
