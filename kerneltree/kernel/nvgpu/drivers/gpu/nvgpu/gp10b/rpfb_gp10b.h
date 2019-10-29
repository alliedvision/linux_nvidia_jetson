/*
 * Copyright (c) 2015, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef RPFB_GP20B_H
#define RPFB_GP20B_H
struct gk20a;

#define NV_UVM_FAULT_BUF_SIZE 32

int gp10b_replayable_pagefault_buffer_init(struct gk20a *g);
u32 gp10b_replayable_pagefault_buffer_get_index(struct gk20a *g);
u32 gp10b_replayable_pagefault_buffer_put_index(struct gk20a *g);
bool gp10b_replayable_pagefault_buffer_is_empty(struct gk20a *g);
bool gp10b_replayable_pagefault_buffer_is_full(struct gk20a *g);
bool gp10b_replayable_pagefault_buffer_is_overflow(struct gk20a *g);
void gp10b_replayable_pagefault_buffer_clear_overflow(struct gk20a *g);
void gp10b_replayable_pagefault_buffer_info(struct gk20a *g);
void gp10b_replayable_pagefault_buffer_deinit(struct gk20a *g);

#endif
