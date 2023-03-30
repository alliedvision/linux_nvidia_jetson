/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef DCE_BITOPS_H
#define DCE_BITOPS_H

#define DCE_BIT(_b_)	(((uint32_t)1U) << (_b_))

#define DCE_MASK(_msb_, _lsb_)					\
	(((DCE_BIT(_msb_) - (uint32_t)1U) |			\
	  DCE_BIT(_msb_)) & (~(DCE_BIT(_lsb_) - (uint32_t)1U)))

#define DCE_EXTRACT(_x_, _msb_, _lsb_, _type_)				\
	((_type_)((_type_)((_x_) & DCE_MASK(_msb_, _lsb_)) >> (_lsb_)))

#define DCE_INSERT(_x_, _msb_, _lsb_, _value_)				\
	((((uint32_t)_x_) & DCE_MASK(_msb_, _lsb_)) |			\
	 ((((uint32_t)_value_) << _lsb_) & DCE_MASK(_msb_, _lsb_)))

#endif
