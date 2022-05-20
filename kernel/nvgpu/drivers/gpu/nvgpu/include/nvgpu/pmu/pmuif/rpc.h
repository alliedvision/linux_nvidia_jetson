/*
 * Copyright (c) 2017-2019, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_PMUIF_RPC_H
#define NVGPU_PMUIF_RPC_H

/*
 * Command requesting execution of the RPC (Remote Procedure Call)
 */
struct nv_pmu_rpc_cmd {
	/* Must be set to @ref NV_PMU_RPC_CMD_ID */
	u8 cmd_type;
	/* RPC call flags (@see PMU_RPC_FLAGS) */
	u8 flags;
	/* Size of RPC structure allocated
	 *  within NV managed DMEM heap
	 */
	u16 rpc_dmem_size;
	/*
	 * DMEM pointer of RPC structure allocated
	 * within RM managed DMEM heap.
	 */
	u32 rpc_dmem_ptr;
};

#define NV_PMU_RPC_CMD_ID 0x80U

/* Message carrying the result of the RPC execution */
struct nv_pmu_rpc_msg {
	/* Must be set to @ref NV_PMU_RPC_MSG_ID */
	u8 msg_type;
	/* RPC call flags (@see PMU_RPC_FLAGS)*/
	u8 flags;
	/*
	 * Size of RPC structure allocated
	 *  within NV managed DMEM heap.
	 */
	u16 rpc_dmem_size;
	/*
	 * DMEM pointer of RPC structure allocated
	 * within NV managed DMEM heap.
	 */
	u32 rpc_dmem_ptr;
};

#define NV_PMU_RPC_MSG_ID 0x80U

#endif /* NVGPU_PMUIF_RPC_H */
