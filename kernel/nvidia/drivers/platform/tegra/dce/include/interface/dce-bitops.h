/*
 * Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
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
