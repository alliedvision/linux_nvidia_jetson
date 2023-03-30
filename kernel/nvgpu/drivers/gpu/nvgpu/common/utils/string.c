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

#include <nvgpu/string.h>
#include <nvgpu/log.h>
#include <nvgpu/static_analysis.h>

void
nvgpu_memcpy(u8 *destb, const u8 *srcb, size_t n)
{
	(void) memcpy(destb, srcb, n);
}

int
nvgpu_memcmp(const u8 *b1, const u8 *b2, size_t n)
{
	return memcmp(b1, b2, n);
}

int nvgpu_strnadd_u32(char *dst, const u32 value, size_t size, u32 radix)
{
	u32 n;
	u32 v;
	char *p;
	u32 digit;

	if ((radix < 2U) || (radix > 16U)) {
		return 0;
	}

	/* how many digits do we need ? */
	n = 0;
	v = value;
	do {
		n = nvgpu_safe_add_u32(n, 1U);
		v = v / radix;
	} while (v > 0U);

	/* bail out if there is not room for '\0' */
	if (n >= size) {
		return 0;
	}

	/* number of digits (not including '\0') */
	p = dst + n;

	/* terminate with '\0' */
	*p = '\0';
	p--;

	v = value;
	do {
		digit = v % radix;
		*p = "0123456789abcdef"[digit];
		v = v / radix;
		p--;
	}
	while (v > 0U);

	return (int)n;
}

bool nvgpu_mem_is_word_aligned(struct gk20a *g, u8 *addr)
{
	if (((unsigned long)addr % 4UL) != 0UL) {
		nvgpu_log_info(g, "addr not 4-byte aligned");
		return false;
	}

	return true;
}

u32 nvgpu_str_join(char *dest, u32 dest_len, const char **src_str_list,
	u32 str_list_len, const char *joiner)
{
	u64 dest_empty_len = nvgpu_safe_sub_u64(dest_len, 1ULL);
	u64 joiner_len = strlen(joiner);
	u32 i = 0U;

	/* Initialize destination buffer */
	*dest = '\0';

	if (str_list_len == 0U) {
		return 0;
	}

	/* Copy first string */
	(void) strncat(dest, src_str_list[0], dest_empty_len);

	/*
	 * Calculate available size in destination buffer
	 * Subtract extra 1U for the NULL byte
	 */
	dest_empty_len = nvgpu_safe_sub_u64(
		nvgpu_safe_sub_u64(dest_len, strlen(dest)), 1ULL);

	for (i = 1; i < str_list_len; i++) {
		/* Make sure we are not writing beyond destination buffer */
		if (dest_empty_len < nvgpu_safe_add_u64(joiner_len,
						strlen(src_str_list[i]))) {
			goto ret;
		}

		(void) strncat(dest, joiner, dest_empty_len);
		(void) strncat(dest, src_str_list[i], dest_empty_len);

		dest_empty_len = nvgpu_safe_sub_u64(
			nvgpu_safe_sub_u64(dest_len, strlen(dest)), 1ULL);
	}

ret:
	/* Return number of bytes (or chars) copied */
	return nvgpu_safe_cast_u64_to_u32(strlen(dest));
}
