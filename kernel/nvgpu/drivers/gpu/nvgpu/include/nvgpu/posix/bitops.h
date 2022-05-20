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

#ifndef NVGPU_POSIX_BITOPS_H
#define NVGPU_POSIX_BITOPS_H

#include <nvgpu/types.h>

/*
 * Assume an 8 bit byte, of course.
 */

/**
 * Number of bits in a byte.
 */
#define BITS_PER_BYTE	8UL

/**
 * Number of bits in data type long.
 */
#define BITS_PER_LONG 	((unsigned long)__SIZEOF_LONG__ * BITS_PER_BYTE)

/**
 * @brief Convert number of bits into long.
 *
 * Converts the input param \a bits into equivalent number of long.
 * Uses the safe add API #nvgpu_safe_add_u64() with \a bits and
 * #BITS_PER_LONG - 1 as parameters to round up the value of \a bits.
 * The number of long variables required to store \a bits is calculated
 * from the rounded up value of \a bits. Macro does not perform any validation
 * of the input parameters.
 *
 * @param bits [in]	Number of bits to convert.
 */
#define BITS_TO_LONGS(bits)			\
	(nvgpu_safe_add_u64(bits, BITS_PER_LONG - 1UL) / BITS_PER_LONG)

/**
 * @brief Set the bit value.
 *
 * Invokes the macro #BIT64() to set the value at bit position indicated by
 * \a i. Deprecated; use the explicit BITxx() macros instead. Macro does not
 * perform any validation of the input parameter.
 *
 * @param i [in]	Bit position to set.
 */
#define BIT(i)		BIT64(i)

/**
 * @brief Create a bit mask.
 *
 * Creates a bit mask starting at bit position \a lo and ending at \a hi.
 * Macro does not perform any validation of the input parameters.
 *
 * @param hi [in]	MSB position of the bit mask.
 * @param lo [in]	LSB position of the bit mask.
 */
#define GENMASK(hi, lo) \
	(((~0UL) - (1UL << (lo)) + 1UL) &	\
		(~0UL >> (BITS_PER_LONG - 1UL - (unsigned long)(hi))))

/*
 * Can't use BITS_TO_LONGS to declare arrays where we can't use BUG(), so if the
 * range is invalid, use -1 for the size which will generate a compiler error.
 */

/**
 * @brief Create an array of unsigned long to represent a bitmap.
 *
 * Creates an array of data type unsigned long by name \a bmap. The size of the
 * array is taken as \a bits value converted into an equivalent rounded up long
 * value if the rounded up value is less than or equal to LONG_MAX. Otherwise
 * the array declaration will generate a compiler error. Macro does not perform
 * any validation of the input parameters.
 *
 * @param bmap [in]	Bitmap to create.
 * @param bits [in]	Number of bits.
 */
#define DECLARE_BITMAP(bmap, bits)					\
	unsigned long bmap[(((LONG_MAX - (bits)) < (BITS_PER_LONG - 1UL)) ? \
		-1 :							\
		(long int)(((bits) + BITS_PER_LONG - 1UL) /		\
						BITS_PER_LONG))]

/**
 * @brief Loop for each set bit.
 *
 * Macro does not perform any validation of the parameters.
 *
 * @param bit [in]	Each set bit, this is the loop index.
 * @param address [in]	Starting of the bitmap.
 * @param size [in]	Size of the bitmap.
 */
#define for_each_set_bit(bit, address, size) \
	for ((bit) = find_first_bit((address), (size));		\
	     (bit) < (size);					\
	     (bit) = find_next_bit((address), (size), (bit) + 1U))

/**
 * @brief Find first set bit.
 *
 * Function returns the position of the first set bit in \a word. This function
 * internally uses the builtin function __builtin_ffsl. Function does not
 * perform any validation of the parameter.
 *
 * @param word [in]	Input of datatype long.
 *
 * @return Returns one plus the index of the least significant 1-bit of input
 * param \a word. Returns 0 if input param is 0.
 */
unsigned long nvgpu_posix_ffs(unsigned long word);

/**
 * @brief Find last set bit.
 *
 * Function returns the position of the last set bit in \a word. This function
 * internally uses the builtin function __builtin_clzl. Function does not
 * perform any validation of the input parameter.
 *
 * @param word [in]	Input of datatype long.
 *
 * @return Returns one plus the index of the most significant 1-bit of input
 * param word. If input param is 0, returns 0.
 */
unsigned long nvgpu_posix_fls(unsigned long word);

/**
 * Wrapper define for #nvgpu_posix_ffs.
 */
#define nvgpu_ffs(word)	nvgpu_posix_ffs(word)

/**
 * Wrapper define for #nvgpu_posix_fls.
 */
#define nvgpu_fls(word)	nvgpu_posix_fls(word)

/**
 * @brief Find first zero bit.
 *
 * Macro to find the position of first zero bit in input data \a word.
 * Uses the macro #nvgpu_ffs internally. Macro does not perform any
 * validation of the input parameter.
 *
 * @param word [in]	Input value to search for the bit.
 *
 * @return Returns the bit position of first zero bit in the input data.
 */
#define ffz(word)	(nvgpu_ffs(~(word)) - 1UL)

/**
 * @brief Find the first set bit.
 *
 * Finds the first set bit position in the input bitmap pointed by \a address.
 * Function does not perform any validation of the input parameter.
 *
 * @param address [in]	Input value to search for set bit.
 * @param size [in]	Size of the input value in bits.
 *
 * @return Returns the position of first set bit.
 */
unsigned long find_first_bit(const unsigned long *address, unsigned long size);

/**
 * @brief Finds the next set bit.
 *
 * Finds the next set bit position in the input data \a address.
 * Function does not perform any validation of the input parameter.
 *
 * @param address [in]	Input value to search for next set bit.
 * @param size [in]	Size of the input value in bits.
 * @param offset [in]	Offset to start from the input data.
 *
 * @return Returns the position of next set bit.
 */
unsigned long find_next_bit(const unsigned long *address, unsigned long size,
			    unsigned long offset);

/**
 * @brief Finds the first zero bit.
 *
 * Finds the first zero bit position in the input data \a address.
 * Function does not perform any validation of the input parameter.
 *
 * @param address [in]	Input value to search.
 * @param size [in]	Size of the input value in bits.
 *
 * @return Returns the position of first zero bit.
 */
unsigned long find_first_zero_bit(const unsigned long *address,
				  unsigned long size);

/**
 * @brief Test the bit value at given position.
 *
 * Checks if the bit at position mentioned by \a bit in \a address is set or
 * not. Function does not perform any validation of the input parameter.
 *
 * @param bit [in]	Bit position to check.
 * @param address [in]	Input data stream.
 *
 * @return Boolean value which indicates the status of the bit.
 *
 * @retval TRUE if the bit position is set.
 * @retval FALSE if the bit position is not set.
 */
bool nvgpu_test_bit(unsigned int bit, const volatile unsigned long *address);

/**
 * @brief Test and set the bit at given position.
 *
 * Tests and sets the bit at position \a bit in \a address. Uses the library
 * function \a atomic_fetch_or internally with a long pointer pointing to
 * the location where the bit position \a bit is stored and the mask value
 * of the bit position as parameters. Function does not perform any
 * validation of the input parameter.
 *
 * @param bit [in]	Bit position to test and set.
 * @param address [in]	Input data stream.
 *
 * @return Boolean value which indicates the status of the bit before the set
 * operation.
 *
 * @retval TRUE if the bit position was already set.
 * @retval FALSE if the bit position was not set before.
 */
bool nvgpu_test_and_set_bit(unsigned int bit, volatile unsigned long *address);

/**
 * @brief Test and clear the bit at given position.
 *
 * Tests and clears the bit at position \a bit in \a address. Uses the library
 * function \a atomic_fetch_and internally with a long pointer pointing to
 * the location where the bit position \a bit is stored and the mask value
 * of the bit position as parameters. Function does not perform any
 * validation of the input parameter.
 *
 * @param bit [in]	Bit position to test and clear.
 * @param address [in]	Input data stream.
 *
 * @return Boolean value which indicates the status of the bit before the clear
 * operation.
 *
 * @retval TRUE if the bit position was already set.
 * @retval FALSE if the bit position was not set before.
 */
bool nvgpu_test_and_clear_bit(unsigned int bit,
			      volatile unsigned long *address);

/*
 * These two are atomic.
 */

/**
 * @brief Sets the bit at given position.
 *
 * Sets the bit atomically at bit position \a bit in \a address. Uses the
 * library function \a atomic_fetch_or internally with a long pointer pointing
 * to the location where the bit position \a bit is stored and the mask value
 * of the bit position as parameters. Function does not perform any
 * validation of the input parameter.
 *
 * @param bit [in]	Bit position to set.
 * @param address [in]	Input data stream.
 */
void nvgpu_set_bit(unsigned int bit, volatile unsigned long *address);

/**
 * @brief Clears the bit at given position.
 *
 * Clears the bit atomically at bit position \a bit in \a address. Uses the
 * library function \a atomic_fetch_and internally with a long pointer pointing
 * to the location where the bit position \a bit is stored and the mask value
 * of the bit position as parameters. Function does not perform any
 * validation of the input parameter.
 *
 * @param bit [in]	Bit position to clear.
 * @param address [in]	Input data stream.
 */
void nvgpu_clear_bit(unsigned int bit, volatile unsigned long *address);

/**
 * @brief Sets a bitmap.
 *
 * Sets a bitmap of length \a len starting from bit position \a start in
 * \a map. Uses the function #nvgpu_set_bit() internally in a loop with the bit
 * positions and \a map as input parameters. Function does not perform any
 * validation of the input parameters.
 *
 * @param map [in,out]	Input data to set bitmap.
 * @param start [in]	Start position of the bitmap.
 * @param len [in]	Length of the bitmap.
 */
void nvgpu_bitmap_set(unsigned long *map, unsigned int start, unsigned int len);

/**
 * @brief Clears a bitmap.
 *
 * Clears a bitmap of length \a len starting from bit position \a start in \a
 * map. Uses the function #nvgpu_clear_bit() internally in a loop with the bit
 * positions and \a map as input parameters. Function does not perform any
 * validation of the input parameters.
 *
 * @param map [in,out]	Input data to clear bitmap.
 * @param start [in]	Start position of the bitmap.
 * @param len [in]	Length of the bitmap.
 */
void nvgpu_bitmap_clear(unsigned long *map, unsigned int start,
			unsigned int len);

/**
 * @brief Find first bitmap space from an offset.
 *
 * Finds the first space of contiguous zeros in input data stream which can
 * accommodate a bitmap of length \a nr starting from position \a start.
 * Uses the functions #find_next_zero_bit and #find_next_bit with \a map,
 * \a size and a start position as parameters. The function does not perform
 * any validation of the parameters.
 *
 * @param map [in]		Input data stream.
 * @param size [in]		Size of the input data.
 * @param start [in]		Start position in input.
 * @param nr [in]		Number of bits in bitmap.
 * @param align_mask [in]	Align mask for start.
 * @param align_offset [in]	Align offset from Input data.
 *
 * @return Returns the position at which the bitmap starts if enough free space
 * is present in the input stream to accommodate the bitmap; otherwise, return
 * the size of the input data.
 */
unsigned long bitmap_find_next_zero_area_off(unsigned long *map,
					     unsigned long size,
					     unsigned long start,
					     unsigned int nr,
					     unsigned long align_mask,
					     unsigned long align_offset);

/**
 * @brief Find first bitmap space.
 *
 * Searches a bitmap for the first space of contiguous zeros that is large
 * enough to accommodate the requested number of bits. Invokes the function
 * #bitmap_find_next_zero_area_off() with \a mask, \a size, \a start, \a nr,
 * \a align_mask and 0 as parameters. Function does not perform any validation
 * of the parameters.
 *
 * @param map [in]		Input data stream.
 * @param size [in]		Size of the input data.
 * @param start [in]		Start position in input.
 * @param bit [in]		Number of bits in bitmap.
 * @param align_mask [in]	Align mask for start.
 *
 * @return Returns the position at which the bitmap starts if enough free space
 * is present in the input stream to accommodate the bitmap; otherwise, return
 * the size of the input data.
 */
unsigned long bitmap_find_next_zero_area(unsigned long *map,
					 unsigned long size,
					 unsigned long start,
					 unsigned int bit,
					 unsigned long align_mask);

#endif /* NVGPU_POSIX_BITOPS_H */
