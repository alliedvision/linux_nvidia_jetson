/*
 * Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_IO_H
#define NVGPU_IO_H

#include <nvgpu/types.h>

struct gk20a;

/**
 * @brief Read a value from a register.
 *
 * @param addr [in]		Register cpu virtual address.
 *
 * Read a 32-bit value from the register cpu virtuall address.
 * OS layer must implement this function.
 *
 * @return Value of the given register.
 */
u32 nvgpu_os_readl(uintptr_t addr);
/**
 * @brief Write a value to a register with an ordering constraint.
 *
 * @param v [in]		Value to write at the offset.
 * @param addr [in]     Register cpu virtual address.
 *
 * Write a 32-bit value to the register cpu virtual address with an
 * ordering constraint on memory operations.
 * OS layer must implement this function.
 *
 * @return None.
 */
void nvgpu_os_writel(u32 v, uintptr_t addr);
/**
 * @brief Write a value to a register without an ordering constraint.
 *
 * @param v [in]		Value to write at the offset.
 * @param addr [in]     Register cpu virtual address.
 *
 * Write a 32-bit value to the register cpu virtual address without an
 * ordering constraint on memory operations.
 * OS layer must implement this function.
 *
 * @return None.
 */
void nvgpu_os_writel_relaxed(u32 v, uintptr_t addr);

/**
 * @brief Create a virtual mapping for the specified physical address range.
 *
 * @param g [in]	The GPU driver structure.
 * @param addr [in]	Physical address start.
 * @param size [in]	Physical address range.
 *
 * Maps the specified physical address range into the kernel/process address
 * space. OS layer must implement this function.
 *
 * @return Virtual address which maps to the physical address range.
 */
uintptr_t nvgpu_io_map(struct gk20a *g, uintptr_t addr, size_t size);

/**
 * @brief Unmap an already mapped io-region.
 *
 * @param g[in]		GPU super structure.
 * @param addr[in]	Start virtual address of the io-region.
 * @param size[in]	Size of the io-region.
 *
 * - OS layer must implement this function.
 *
 * @return None.
 */
void nvgpu_io_unmap(struct gk20a *g, uintptr_t ptr, size_t size);

/**
 * @file
 *
 * Interface for mmio access.
 */

/* Legacy defines - should be removed once everybody uses nvgpu_* */
#define gk20a_writel nvgpu_writel
#define gk20a_readl nvgpu_readl
#define gk20a_writel_check nvgpu_writel_check
#define gk20a_bar1_writel nvgpu_bar1_writel
#define gk20a_bar1_readl nvgpu_bar1_readl
#define gk20a_io_exists nvgpu_io_exists
#define gk20a_io_valid_reg nvgpu_io_valid_reg

struct gk20a;

/**
 * @brief Write a value to a GPU register with an ordering constraint.
 *
 * @param g [in]		GPU super structure.
 * @param r [in]		Register offset in GPU IO space.
 *				Range: 0 to (TEGRA_GK20A_BAR0_SIZE - 4).
 * @param v [in]		Value to write at the offset.
 *
 * Write a 32-bit value to register offset in GPU IO space with an
 * ordering constraint on memory operations. API supports write from offset 0 to
 * TEGRA_GK20A_BAR0_SIZE but check for this range is not imposed as this is
 * called frequently.
 *
 * @return None.
 */
void nvgpu_writel(struct gk20a *g, u32 r, u32 v);

#ifdef CONFIG_NVGPU_DGPU
/**
 * @brief Write a value to GPU register without an ordering constraint.
 *
 * @param g [in]		GPU super structure.
 * @param r [in]		Register offset in GPU IO space.
 * @param v [in]		Value to write at the offset.
 *
 * Write a 32-bit value to register offset in GPU IO space without an ordering
 * constraint on memory operations. This function is implemented by the OS layer.
 *
 * @return None.
 */
void nvgpu_writel_relaxed(struct gk20a *g, u32 r, u32 v);
#endif

/**
 * @brief Read a value from a GPU register.
 *
 * @param g [in]		GPU super structure.
 * @param r [in]		Register offset in GPU IO space.
 *				Range: 0 to (TEGRA_GK20A_BAR0_SIZE - 4).
 *
 * Read a 32-bit value from register offset in GPU IO space. If all the bits are
 * set in the value read then check for gpu state validity. Refer
 * #nvgpu_check_gpu_state() for gpu state validity check. API supports read
 * from offset 0 to TEGRA_GK20A_BAR0_SIZE but check for this range is not imposed
 * as this is called frequently.
 *
 * @return Value at the given register offset in GPU IO space.
 */
u32 nvgpu_readl(struct gk20a *g, u32 r);

/**
 * @brief Read a value from a GPU register.
 *
 * @param g [in]		GPU super structure.
 * @param r [in]		Register offset in GPU IO space.
 *				Range: 0 to (TEGRA_GK20A_BAR0_SIZE - 4).
 *
 * Read a 32-bit value from register offset in GPU IO space. If all the bits are
 * set in the value read then check for gpu state validity. API supports read
 * from offset 0 to TEGRA_GK20A_BAR0_SIZE but check for this range is not imposed
 * as this is called frequently.
 *
 * @return Value at the given register offset in GPU IO space.
 */
u32 nvgpu_readl_impl(struct gk20a *g, u32 r);

/**
 * @brief Write validate to a GPU register.
 *
 * @param g [in]		GPU super structure.
 * @param r [in]		Register offset in GPU IO space.
 *				Range: 0 to (TEGRA_GK20A_BAR0_SIZE - 4).
 * @param v [in]		Value to write at the offset.
 *
 * Write a 32-bit value to register offset in GPU IO space and reads it
 * back. Check whether the write/read values match and logs the event on
 * a mismatch.
 *
 * @return None.
 */
void nvgpu_writel_check(struct gk20a *g, u32 r, u32 v);

/**
 * @brief Ensure write to a GPU register.
 *
 * @param g [in]		GPU super structure.
 * @param r [in]		Register offset in GPU IO space.
 * @param v [in]		Value to write at the offset.
 *
 * This is a blocking call. It keeps on writing a 32-bit value to a GPU
 * register and reads it back until read/write values are not match.
 *
 * @return None.
 */
void nvgpu_writel_loop(struct gk20a *g, u32 r, u32 v);

/**
 * @brief Write a value to an already mapped bar1 io-region.
 *
 * @param g [in]		GPU super structure.
 * @param r [in]		Register offset in io-region.
 *				Range: 0 to (TEGRA_GK20A_BAR1_SIZE - 4).
 * @param v [in]		Value to write at the offset.
 *
 * - Write a 32-bit value to register offset of region bar1.
 *
 * @return None.
 */
void nvgpu_bar1_writel(struct gk20a *g, u32 b, u32 v);

/**
 * @brief Read a value from an already mapped bar1 io-region.
 *
 * @param g [in]		GPU super structure.
 * @param b [in]		Register offset in io-region.
 *				Range: 0 to (TEGRA_GK20A_BAR1_SIZE - 4).
 *
 * - Read a 32-bit value from a region bar1.
 *
 * @return Value at the given offset of the io-region.
 */
u32 nvgpu_bar1_readl(struct gk20a *g, u32 b);

/**
 * @brief Check bar0 io-region is mapped or not
 *
 * @param g [in]		GPU super structure.
 *
 * - io mapping exists if bar0 address is assigned to regs.
 *
 * @return TRUE if bar0 is mapped or else FALSE.
 */
bool nvgpu_io_exists(struct gk20a *g);

/**
 * @brief Validate BAR0 io-mapped offset.
 *
 * @param g [in]		GPU super structure.
 * @param r [in]		Register offset in io-region.
 *				Range: 0 to (TEGRA_GK20A_BAR0_SIZE - 4).
 *
 * - BAR0 Offset is valid if it falls into BAR0 range.
 *
 * @return TRUE if bar0 offset is valid or else FALSE.
 */
bool nvgpu_io_valid_reg(struct gk20a *g, u32 r);

/**
 * @brief Write value to register at phys offset.
 *
 * @param g [in]		GPU super structure.
 * @param r [in]		Register offset in GPU IO-space.
 * @param v [in]		Value to write at the offset.
 *
 * - Write a 32-bit value at phys offset. Phys_offset can be retrieved using
 * gops.func.get_full_phys_offset().
 *
 * @return None.
 */
void nvgpu_func_writel(struct gk20a *g, u32 r, u32 v);

/**
 * @brief Read value from register at phys offset.
 *
 * @param g [in]		GPU super structure.
 * @param b [in]		Register offset in GPU IO-space.
 *
 * - Read a 32-bit value from phys offset. Phys_offset can be retrieved using
 * gops.func.get_full_phys_offset().
 *
 * @return Value at the given offset.
 */
u32 nvgpu_func_readl(struct gk20a *g, u32 r);

#endif /* NVGPU_IO_H */
