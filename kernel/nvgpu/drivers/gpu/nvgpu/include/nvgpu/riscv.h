/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_RISCV_H
#define NVGPU_RISCV_H

#include <nvgpu/types.h>
#include <nvgpu/falcon.h>
struct gk20a;
struct nvgpu_falcon;

/**
 * @brief Read the riscv register.
 *
 * @param flcn [in] The falcon.
 * @param offset [in] offset of the register.
 *
 * This function is called to read a register with common flcn offset.
 *
 * Steps:
 * - Read and return data from register at \a offset from
 *   the base of riscv core of \a flcn.
 *
 * @return register data.
 */
u32 nvgpu_riscv_readl(struct nvgpu_falcon *flcn, u32 offset);

/**
 * @brief Write to the riscv register.
 *
 * @param flcn [in] The falcon.
 * @param offset [in] Index of the register.
 * @param data [in] Data to be written to the register.
 *
 * This function is called to write to a register with common flcn offset.
 *
 * Steps:
 * - Write \a data to register at \a offet from base of
 *   riscv core of \a flcn.
 */
void nvgpu_riscv_writel(struct nvgpu_falcon *flcn,
                                       u32 offset, u32 val);

/**
 * @brief Dump RISCV BootROM status.
 *
 * @param flcn [in] The falcon.
 *
 * This function is called to get the status of RISCV BootROM.
 *
 * Steps:
 * - Print the flcn's RISCV BCR control configuratation.
 * - Print the flcn's RISCV BR priv lockdown status.
 * - Print the flcn's BR retcode value.
 */
void nvgpu_riscv_dump_brom_stats(struct nvgpu_falcon *flcn);

/**
 * @brief Get the size of falcon's memory.
 *
 * @param flcn [in] The falcon.
 * @param type [in] Falcon memory type (IMEM, DMEM).
 *		    - Supported types: MEM_DMEM (0), MEM_IMEM (1)
 * @param size [out] Size of the falcon memory type.
 *
 * This function is called to get the size of falcon's memory for validation
 * while copying to IMEM/DMEM.
 *
 * Steps:
 * - Validate that the passed in falcon struct is not NULL and is for supported
 *   falcon. If not valid, return -EINVAL.
 * - Read the size of the falcon memory of \a type in bytes from the HW config
 *   register in output parameter \a size.
 *
 * @return 0 in case of success, < 0 in case of failure.
 */
int nvgpu_riscv_hs_ucode_load_bootstrap(struct nvgpu_falcon *flcn,
						struct nvgpu_firmware *manifest_fw,
						struct nvgpu_firmware *code_fw,
						struct nvgpu_firmware *data_fw,
						u64 ucode_sysmem_desc_addr);
#endif /* NVGPU_RISCV_H */
