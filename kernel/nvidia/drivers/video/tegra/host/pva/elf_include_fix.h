/* SPDX-License-Identifier: GPL-2.0-only */
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


#ifndef ELF_INCLUDE_FIX_H
#undef ELF_INCLUDE_FIX_H

#include <linux/elf.h>

#undef SHT_NULL
#undef SHT_PROGBITS
#undef SHT_SYMTAB
#undef SHT_STRTAB
#undef SHT_RELA
#undef SHT_HASH
#undef SHT_DYNAMIC
#undef SHT_NOTE
#undef ELFCLASS32

#undef SHT_NOBITS
#undef SHT_REL
#undef SHT_SHLIB
#undef SHT_DYNSYM

#undef SHT_LOPROC
#undef SHT_HIPROC
#undef SHT_LOUSER
#undef SHT_HIUSER

#undef SHN_UNDEF

#undef ELF_ST_BIND
#undef ELF_ST_TYPE

#undef STT_NOTYPE
#undef STT_OBJECT
#undef STT_FUNC
#undef STT_SECTION
#undef STT_FILE
#undef STT_COMMON

#undef STB_LOCAL
#undef STB_GLOBAL
#undef STB_WEAK


#undef SHN_LORESERVE
#undef SHN_ABS
#undef SHN_COMMON

#endif // ELF_INCLUDE_FIX_H
