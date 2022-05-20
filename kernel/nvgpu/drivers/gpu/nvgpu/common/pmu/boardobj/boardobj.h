/*
 * Copyright (c) 2016-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_BOARDOBJ_H
#define NVGPU_BOARDOBJ_H

struct pmu_board_obj;
struct nvgpu_list_node;
struct gk20a;
struct nv_pmu_boardobj;

/*
* Base Class for all physical or logical device on the PCB.
* Contains fields common to all devices on the board. Specific types of
* devices may extend this object adding any details specific to that
* device or device-type.
*/

struct pmu_board_obj {
	struct gk20a *g;

	u8 type; /*type of the device*/
	u8 idx;  /*index of boardobj within in its group*/
	/* true if allocated in constructor. destructor should free */
	bool allocated;
	u32 type_mask; /*mask of types this boardobjimplements*/
	bool (*implements)(struct gk20a *g, struct pmu_board_obj *obj,
			u8 type);
	int (*destruct)(struct pmu_board_obj *obj);
	/*
	* Access interface apis which will be overridden by the devices
	* that inherit from BOARDOBJ
	*/
	int (*pmudatainit)(struct gk20a *g, struct pmu_board_obj *obj,
			struct nv_pmu_boardobj *pmu_obj);
	struct nvgpu_list_node node;
};

#define HIGHESTBITIDX_32(n32)   \
{                               \
	u32 count = 0U;        \
	while (((n32) >>= 1U) != 0U) {       \
		count++;       \
	}                      \
	(n32) = count;            \
}

#define LOWESTBIT(x)            ((x) &  (((x)-1U) ^ (x)))

#define HIGHESTBIT(n32)     \
{                           \
	HIGHESTBITIDX_32(n32);  \
	n32 = NVBIT(n32);       \
}

#define ONEBITSET(x)            ((x) && (((x) & ((x)-1U)) == 0U))

#define LOWESTBITIDX_32(n32)  \
{                             \
	n32 = LOWESTBIT(n32); \
	IDX_32(n32);         \
}

#define NUMSETBITS_32(n32)                                         \
{                                                                  \
	(n32) = (n32) - (((n32) >> 1U) & 0x55555555U);                         \
	(n32) = ((n32) & 0x33333333U) + (((n32) >> 2U) & 0x33333333U);         \
	(n32) = ((((n32) + ((n32) >> 4U)) & 0x0F0F0F0FU) * 0x01010101U) >> 24U;\
}

#define IDX_32(n32)				\
{						\
	u32 idx = 0U;				\
	if (((n32) & 0xFFFF0000U) != 0U) {  	\
		idx += 16U;			\
	}					\
	if (((n32) & 0xFF00FF00U) != 0U) {	\
		idx += 8U;			\
	}					\
	if (((n32) & 0xF0F0F0F0U) != 0U) {	\
		idx += 4U;			\
	}					\
	if (((n32) & 0xCCCCCCCCU) != 0U) {	\
		idx += 2U;			\
	}					\
	if (((n32) & 0xAAAAAAAAU) != 0U) {	\
		idx += 1U;			\
	}					\
	(n32) = idx;				\
}

/*
* Fills out the appropriate the nv_pmu_xxxx_device_desc_<xyz> driver->PMU
* description structure, describing this BOARDOBJ board device to the PMU.
*
*/
int pmu_board_obj_pmu_data_init_super(struct gk20a *g, struct pmu_board_obj
		*obj, struct nv_pmu_boardobj *pmu_obj);

/*
* Constructor for the base Board Object. Called by each device-specific
* implementation of the BOARDOBJ interface to initialize the board object.
*/
int pmu_board_obj_construct_super(struct gk20a *g, struct pmu_board_obj
		*obj, void *args);

static inline struct pmu_board_obj *
boardobj_from_node(struct nvgpu_list_node *node)
{
	return (struct pmu_board_obj *)
		((uintptr_t)node - offsetof(struct pmu_board_obj, node));
};

u8 pmu_board_obj_get_type(void *obj);
u8 pmu_board_obj_get_idx(void *obj);

#endif /* NVGPU_BOARDOBJ_H */
