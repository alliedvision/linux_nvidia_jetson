/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_STRING_H
#define NVGPU_STRING_H

#include <nvgpu/types.h>

#ifdef __KERNEL__
#include <linux/string.h>
#endif

struct gk20a;

/**
 * @brief Copy memory buffer
 *
 * Copy memory from source buffer \a srcb to destination buffer \a destb.
 * This function uses the library function \a memcpy internally.
 *
 * @param destb [out] Buffer into which data is to be copied.
 *		      Function does not perform validation of this parameter.
 * @param srcb [in] Buffer from which data is to be copied.
 *		    Function does not perform validation of this parameter.
 * @param n [in] Number of bytes to copy from src buffer to dest buffer.
 *		 Function does not perform validation of this parameter.
 */
void nvgpu_memcpy(u8 *destb, const u8 *srcb, size_t n);

/**
 * @brief Compare memory buffers
 *
 * Compare the first \a n bytes of two memory buffers. If the contents of the
 * two buffers match, then zero is returned. If the contents of \a b1 are less
 * than \a b2 then a value less than zero is returned. If the contents of \a b1
 * are greater than \a b2 then a value greater than zero is returned. This
 * function uses the library function \a memcmp internally.
 *
 * @param b1 [in] First buffer to use in memory comparison.
 *		  Function does not perform validation of this parameter.
 * @param b2 [in] Second buffer to use in memory comparison.
 *		  Function does not perform validation of this parameter.
 * @param n [in] Number of bytes to compare between two buffers.
 *		 Function does not perform validation of this parameter.
 *
 * @return 0 if the comparison matches else a non-zero value is returned.
 *
 * @retval 0 if both buffers match.
 * @retval <0 if object pointed to by \a b1 is less than the object pointed to
 * by \a b2.
 * @retval >0 if object pointed to by \a b1 is greater than the object pointed
 * to by \a b2.
 */
int nvgpu_memcmp(const u8 *b1, const u8 *b2, size_t n);

/**
 * @brief Formats u32 into null-terminated string
 *
 * Formats the integer variable \a value into a null terminated string. Uses
 * the function #nvgpu_safe_add_u32 to count the number of digits in the
 * input parameter \a value. A local variable is passed as an input parameter
 * to the function #nvgpu_safe_add_u32 to store the count of the digits.
 *
 * @param dst [in,out] Buffer to copy the value into.
 *		       Function does not perform validation of this parameter.
 * @param value [in] Value to be formatted into string.
 *		     Function does not perform validation of this parameter.
 * @param size [in] Size available in the destination buffer. Returns 0 If the
 *		    size available is less than the length required to create a
 *					NULL terminated string equivalent of \a value parameter.
 * @param radix	[in] Radix value to be used. Should be in the range 2 - 16,both
 *		     inclusive.
 *
 * @return Returns number of digits added to string (not including '\0') if
 * successful, else 0.
 *
 * @retval 0 in case of invalid radix or insufficient space.
 */
int nvgpu_strnadd_u32(char *dst, const u32 value, size_t size, u32 radix);

/**
 * @brief Check if the memory address is word (4-byte) aligned.
 *
 * Checks if the provided address is word aligned or not.
 *
 * @param g [in] The GPU structure, struct gk20a.
 *		 Function does not perform validation of this parameter.
 * @param addr [in] Memory address. Checks if the parameter is aligned with 4
 *		    bytes or not.
 *
 * @return Boolean value to indicate the alignment status of the address.
 *
 * @retval TRUE if \a addr is word aligned.
 * @retval FALSE if \a addr is not word aligned.
 */
bool nvgpu_mem_is_word_aligned(struct gk20a *g, u8 *addr);

/**
 * @brief Construct single string from multiple strings.
 *
 * Concatenates multiple source strings to generate single string.
 *
 * @param dest [in] Pointer to the destination string.
 * @param dest_len [in] Maximum length of destination string
 *			including NULL character.
 * @param src_str_list [in] Pointer to list of strings to be concatenated.
 * @param str_list_len [in] Number of strings in \a src_str_list.
 * @param joiner [in] Pointer to string used to join strings in
 *		      \a src_str_list.
 *
 * @return Number of bytes copied to \a dest.
 */
u32 nvgpu_str_join(char *dest, u32 dest_len, const char **src_str_list,
	u32 str_list_len, const char *joiner);

#endif /* NVGPU_STRING_H */
