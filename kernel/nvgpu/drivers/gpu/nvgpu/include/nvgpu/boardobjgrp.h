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

#ifndef NVGPU_BOARDOBJGRP_H
#define NVGPU_BOARDOBJGRP_H

struct boardobjgrp;
struct gk20a;
struct nvgpu_list_node;

/* ------------------------ Includes ----------------------------------------*/
#include <nvgpu/boardobjgrpmask.h>
#include <nvgpu/list.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/pmu/super_surface.h>
#include <nvgpu/flcnif_cmn.h>
/* Dependency of this include will be removed in further CL */
#include "../../common/pmu/boardobj/boardobj.h"

struct nv_pmu_boardobjgrp_super;
struct nv_pmu_boardobjgrp;
struct nv_pmu_boardobj;
struct nv_pmu_boardobj_query;

/*
* Board Object Group Remover and destructor. This is used to remove and
* destruct specific entry from the Board Object Group.
*/
int boardobjgrp_objremoveanddestroy(struct boardobjgrp *pboardobjgrp,
	u8 index);

/*
* Fills out the appropriate the PMU_BOARDOBJGRP_<xyz> driver<->PMU description
* header structure, more specifically a mask of BOARDOBJs.
*/
int nvgpu_boardobjgrp_pmu_hdr_data_init_super(struct gk20a *g,
			struct boardobjgrp *pboardobjgrp,
			struct nv_pmu_boardobjgrp_super *pboardobjgrppmu,
			struct boardobjgrpmask *mask);

/*
* Fills out the appropriate the PMU_BOARDOBJGRP_<xyz> driver->PMU description
* structure, describing the BOARDOBJGRP and all of its BOARDOBJs to the PMU.
*/
int nvgpu_boardobjgrp_pmu_data_init_super(struct gk20a *g,
		struct boardobjgrp *pboardobjgrp,
		struct nv_pmu_boardobjgrp_super *pboardobjgrppmu);
int nvgpu_boardobjgrp_pmu_data_init_legacy(struct gk20a *g,
		struct boardobjgrp *pboardobjgrp,
		struct nv_pmu_boardobjgrp_super *pboardobjgrppmu);

/*
* Structure describing an PMU CMD for interacting with the representaition
* of this BOARDOBJGRP within the PMU.
*/
struct pmu_surface {
	struct nvgpu_mem vidmem_desc;
	struct nvgpu_mem sysmem_desc;
	struct flcn_mem_desc_v0 params;
};

struct boardobjgrp_pmu_cmd {
	u8   id;
	u8   msgid;
	u8   hdrsize;
	u8   entrysize;
	u16 dmem_buffer_size;
	u32 super_surface_offset;
	u32 fbsize;
	struct nv_pmu_boardobjgrp_super *buf;
	struct pmu_surface surf;
};

/*
* Structure of state describing how to communicate with representation of this
* BOARDOBJGRP in the PMU.
*/
struct boardobjgrp_pmu {
	u8   unitid;
	bool bset;
	u8 rpc_func_id;
	struct boardobjgrp_pmu_cmd set;
	struct boardobjgrp_pmu_cmd getstatus;
};

/*
* Function by which a class which implements BOARDOBJGRP can construct a PMU
* CMD.  This provides the various information describing the PMU CMD including
* the CMD and MSG ID and the size of the various sturctures in the payload.
*/

int nvgpu_boardobjgrp_pmucmd_construct_impl(struct gk20a *g,
				struct boardobjgrp *pboardobjgrp,
				struct boardobjgrp_pmu_cmd *cmd, u8 id,
				u8 msgid, u16 hdrsize, u16 entrysize,
				u32 fbsize, u32 ss_offset, u8 rpc_func_id);

/*
* Base Class Group for all physical or logical device on the PCB.
* Contains fields common to all devices on the board. Specific types of
* devices groups may extend this object adding any details specific to that
* device group or device-type.
*/
struct boardobjgrp {
	struct gk20a *g;
	u32  objmask;
	bool bconstructed;
	u8 type;
	u8 classid;
	struct pmu_board_obj **ppobjects;
	struct boardobjgrpmask *mask;
	u8  objslots;
	u8  objmaxidx;
	struct boardobjgrp_pmu  pmu;

	/* Basic interfaces */
	int (*destruct)(struct boardobjgrp *pboardobjgrp);
	int (*objinsert)(struct boardobjgrp *pboardobjgrp,
			struct pmu_board_obj *obj, u8 index);
	struct pmu_board_obj *(*objgetbyidx)(
			struct boardobjgrp *pBobrdobjgrp, u8 index);
	struct pmu_board_obj *(*objgetnext)(struct boardobjgrp *pboardobjgrp,
			u8 *currentindex, struct boardobjgrpmask *mask);
	int (*objremoveanddestroy)(struct boardobjgrp *pboardobjgrp, u8 index);

	/* PMU interfaces */
	int (*pmuinithandle)(struct gk20a *g,
			struct boardobjgrp *pboardobjgrp);
	int (*pmuhdrdatainit)(struct gk20a *g, struct boardobjgrp *pboardobjgrp,
			struct nv_pmu_boardobjgrp_super *pboardobjgrppmu,
			struct boardobjgrpmask *mask);
	int (*pmudatainit)(struct gk20a *g,
			struct boardobjgrp *pboardobjgrp,
			struct nv_pmu_boardobjgrp_super *pboardobjgrppmu);
	int (*pmuset)(struct gk20a *g,
			struct boardobjgrp *pboardobjgrp);
	int (*pmugetstatus)(struct gk20a *g,
			struct boardobjgrp *pboardobjgrp,
			struct boardobjgrpmask *mask);
	int (*pmudatainstget)(struct gk20a *g,
			struct nv_pmu_boardobjgrp  *boardobjgrppmu,
			struct nv_pmu_boardobj **pmu_obj, u8 idx);
	int (*pmustatusinstget)(struct gk20a *g, void *pboardobjgrppmu,
			struct nv_pmu_boardobj_query **obj_pmu_status,
			u8 idx);
	struct nvgpu_list_node node;
};

/*
* Macro test whether a specified index into the BOARDOBJGRP is valid.
*
*/
#define boardobjgrp_idxisvalid(_pboardobjgrp, _idx)                           \
	(((_idx) < (_pboardobjgrp)->objslots) &&                               \
	((_pboardobjgrp)->ppobjects[(_idx)] != NULL))

/*
* Macro test whether a specified BOARDOBJGRP is empty.
*/
#define BOARDOBJGRP_IS_EMPTY(_pboardobjgrp)                                    \
	((!((_pboardobjgrp)->bconstructed)) ||                                 \
	((_pboardobjgrp)->objmaxidx == CTRL_BOARDOBJ_IDX_INVALID))

#define boardobjgrp_objinsert(_pboardobjgrp, _pboardobj, _idx)                 \
	((_pboardobjgrp)->objinsert((_pboardobjgrp), (_pboardobj), (_idx)))

/*
* Helper macro to determine the "next" open/empty index after all allocated
* objects.  This is intended to be used to find the index at which objects can
* be inserted contiguously (i.e. w/o fear of colliding with existing objects).
*/
#define BOARDOBJGRP_NEXT_EMPTY_IDX(_pboardobjgrp)                              \
	((CTRL_BOARDOBJ_IDX_INVALID == (_pboardobjgrp)->objmaxidx) ? 0U :      \
	((((_pboardobjgrp)->objmaxidx + 1U) >= (_pboardobjgrp)->objslots) ?    \
	(u8)CTRL_BOARDOBJ_IDX_INVALID : (u8)((_pboardobjgrp)->objmaxidx + 1U)))

/*
* Helper macro to determine the number of @ref BOARDOBJ pointers
* that are required to be allocated in PMU @ref ppObjects.
*/
#define BOARDOBJGRP_PMU_SLOTS_GET(_pboardobjgrp)                            \
	((CTRL_BOARDOBJ_IDX_INVALID == (_pboardobjgrp)->objmaxidx) ? 0U :    \
	(u8)((_pboardobjgrp)->objmaxidx + 1U))

#define BOARDOBJGRP_OBJ_GET_BY_IDX(_pboardobjgrp, _idx)                     \
	((_pboardobjgrp)->objgetbyidx((_pboardobjgrp), (_idx)))

/*
* macro to look-up next object while tolerating error if
* Board Object Group is not constructed.
*/

#define boardobjgrpobjgetnextsafe(_pgrp, _pindex, _pmask)                      \
	((_pgrp)->bconstructed ?                                               \
	(_pgrp)->objgetnext((_pgrp), (_pindex), (_pmask)) : NULL)

/*
* Used to traverse all Board Objects stored within @ref _pgrp in the increasing
* index order.
* If @ref _pmask is provided only objects specified by the mask are traversed.
*/
#define BOARDOBJGRP_ITERATOR(_pgrp, _ptype, _pobj, _index, _pmask)             \
	for ((_index) = CTRL_BOARDOBJ_IDX_INVALID,                             \
	(_pobj)  = (_ptype)boardobjgrpobjgetnextsafe((_pgrp),		       \
						     &(_index), (_pmask));     \
	(_pobj) != NULL;                                                       \
	(_pobj)  = (_ptype)boardobjgrpobjgetnextsafe((_pgrp),		       \
						     &(_index), (_pmask)))
#define BOARDOBJGRP_FOR_EACH(_pgrp, _ptype, _pobj, _index)                     \
	BOARDOBJGRP_ITERATOR(_pgrp, _ptype, _pobj, _index, NULL)

#define BOARDOBJGRP_FOR_EACH_INDEX_IN_MASK(mask_width, index, mask)        \
{                                                           \
	u##mask_width lcl_msk = (u##mask_width)(mask);         \
	for ((index) = 0; lcl_msk != 0U; (index)++, lcl_msk >>= 1U) {     \
		if (((u##mask_width)((u64)1) & lcl_msk) == 0U) {     \
			continue;                                       \
		}

#define BOARDOBJGRP_FOR_EACH_INDEX_IN_MASK_END                          \
	}                                                       \
}


/*!
* Invalid UNIT_ID.  Used to indicate that the implementing class has not set
* @ref BOARDOBJGRP::unitId and, thus, certain BOARDOBJGRP PMU interfaces are
* not supported.
*/
#define BOARDOBJGRP_UNIT_ID_INVALID                                   255U

/*!
* Invalid UNIT_ID.  Used to indicate that the implementing class has not set
* @ref BOARDOBJGRP::grpType and, thus, certain BOARDOBJGRP PMU interfaces are
* not supported.
*/
#define BOARDOBJGRP_GRP_CLASS_ID_INVALID                              255U

/*!
* Invalid UNIT_ID.  Used to indicate that the implementing class has not set
* @ref BOARDOBJGRP::grpSetCmdId and, thus, certain BOARDOBJGRP PMU interfaces
* are not supported.
*/
#define BOARDOBJGRP_GRP_CMD_ID_INVALID                                255U
#define BOARDOBJGRP_GRP_RPC_FUNC_ID_INVALID                           255U

/*!
* Helper macro to construct a BOARDOBJGRP's PMU SW state.
*
* @param[out] pboardobjgrp    BOARDOBJGRP pointer
* @param[in]  _eng
*     Implementing engine/unit which manages the BOARDOBJGRP.
* @param[in]  _class
*     Class ID of BOARDOBJGRP.
*/
#define BOARDOBJGRP_PMU_CONSTRUCT(pboardobjgrp, _ENG, _CLASS)	              \
do {                                                                          \
	(pboardobjgrp)->pmu.unitid  = PMU_UNIT_##_ENG;                        \
	(pboardobjgrp)->classid =                                         \
	NV_PMU_##_ENG##_BOARDOBJGRP_CLASS_ID_##_CLASS;                        \
} while (false)

#define BOARDOBJGRP_PMU_CMD_GRP_SET_CONSTRUCT(g, pboardobjgrp, eng, ENG, \
	class, CLASS)                                                 \
	(nvgpu_boardobjgrp_pmucmd_construct_impl(    \
	g,                                              /* pgpu */    \
	pboardobjgrp,                                      /* pboardobjgrp */ \
	&((pboardobjgrp)->pmu.set),                        /* pcmd */         \
	NV_PMU_##ENG##_CMD_ID_BOARDOBJ_GRP_SET,               /* id */        \
	NV_PMU_##ENG##_MSG_ID_BOARDOBJ_GRP_SET,               /* msgid */     \
	(u32)sizeof(union nv_pmu_##eng##_##class## \
		_boardobjgrp_set_header_aligned), \
	(u32)sizeof(union nv_pmu_##eng##_##class## \
		_boardobj_set_union_aligned), \
	(u32)nvgpu_pmu_get_ss_member_set_size(g, g->pmu, \
		NV_PMU_SUPER_SURFACE_MEMBER_##CLASS##_GRP), \
	(u32)nvgpu_pmu_get_ss_member_set_offset(g, g->pmu, \
	NV_PMU_SUPER_SURFACE_MEMBER_##CLASS##_GRP), \
	NV_PMU_RPC_ID_##ENG##_BOARD_OBJ_GRP_CMD))

#define BOARDOBJGRP_PMU_CMD_GRP_GET_STATUS_CONSTRUCT(g, pboardobjgrp, \
	eng, ENG, class, CLASS)                                       \
	(nvgpu_boardobjgrp_pmucmd_construct_impl(    \
	g,                                              /* pGpu */    \
	pboardobjgrp,                                      /* pBoardObjGrp */ \
	&((pboardobjgrp)->pmu.getstatus),                  /* pCmd */         \
	NV_PMU_##ENG##_CMD_ID_BOARDOBJ_GRP_GET_STATUS,        /* id */        \
	NV_PMU_##ENG##_MSG_ID_BOARDOBJ_GRP_GET_STATUS,        /* msgid */     \
	(u32)sizeof(union nv_pmu_##eng##_##class## \
		_boardobjgrp_get_status_header_aligned), \
	(u32)sizeof(union nv_pmu_##eng##_##class## \
		_boardobj_get_status_union_aligned), \
	(u32)nvgpu_pmu_get_ss_member_get_status_size(g, g->pmu, \
		NV_PMU_SUPER_SURFACE_MEMBER_##CLASS##_GRP), \
	(u32)nvgpu_pmu_get_ss_member_get_status_offset(g, g->pmu, \
	NV_PMU_SUPER_SURFACE_MEMBER_##CLASS##_GRP), \
	NV_PMU_RPC_ID_##ENG##_BOARD_OBJ_GRP_CMD))

/* ------------------------ Function Prototypes ----------------------------- */
/* Constructor and destructor */
int nvgpu_boardobjgrp_construct_super(struct gk20a *g,
	struct boardobjgrp *pboardobjgrp);

void nvgpu_boardobjgrp_e32_hdr_set(struct nv_pmu_boardobjgrp *hdr, u32 objmask);

static inline struct boardobjgrp *
boardobjgrp_from_node(struct nvgpu_list_node *node)
{
	return (struct boardobjgrp *)
		((uintptr_t)node - offsetof(struct boardobjgrp, node));
};

#endif /* NVGPU_BOARDOBJGRP_H */
