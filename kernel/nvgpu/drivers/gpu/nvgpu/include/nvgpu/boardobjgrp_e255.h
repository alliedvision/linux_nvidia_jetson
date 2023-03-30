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

#ifndef NVGPU_BOARDOBJGRP_E255_H
#define NVGPU_BOARDOBJGRP_E255_H

#include <nvgpu/boardobjgrpmask.h>
#include <nvgpu/boardobjgrp.h>

/*
 * boardobjgrp_e255 is @ref BOARDOBJGRP child class allowing storage of up
 * to 255 @ref BOARDOBJ object pointers with single static 255-bit mask denoting
 * valid object pointers.
 */
struct boardobjgrp_e255 {
	struct boardobjgrp  super;
	struct pmu_board_obj *objects[CTRL_BOARDOBJGRP_E255_MAX_OBJECTS];
	struct boardobjgrpmask_e255  mask;
};

#define boardobjgrp_pmu_data_init_e255(g, pboardpbjgrp, pboardobjgrppmu) \
		nvgpu_boardobjgrp_pmu_data_init_super(g, \
		pboardpbjgrp, pboardobjgrppmu)

/* Constructor and destructor */
int nvgpu_boardobjgrp_construct_e255(struct gk20a *g,
	struct boardobjgrp_e255 *pboardobjgrp_e255);
int boardobjgrp_destruct_e255(struct boardobjgrp *pboardobjgrp);

#endif /* NVGPU_BOARDOBJGRP_E255_H */
