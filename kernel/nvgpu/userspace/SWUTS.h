/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
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

/**
 * @file
 * @page NVGPU-SWUTS
 *
 * NVGPU Software Unit Test Specifications
 * =======================================
 *
 * The following pages cover the various unit test specifications needed
 * to test the NVGPU driver.
 *
 *   - @ref SWUTS-enabled
 *   - @ref SWUTS-interface-bit-utils
 *   - @ref SWUTS-interface-lock
 *   - @ref SWUTS-interface-nvgpu-gk20a
 *   - @ref SWUTS-interface-rbtree
 *   - @ref SWUTS-interface-static_analysis
 *   - @ref SWUTS-interface-string
 *   - @ref SWUTS-interface-worker
 *   - @ref SWUTS-interface-kref
 *   - @ref SWUTS-interface-list
 *   - @ref SWUTS-bus
 *   - @ref SWUTS-falcon
 *   - @ref SWUTS-netlist
 *   - @ref SWUTS-fifo
 *   - @ref SWUTS-fifo-channel
 *   - @ref SWUTS-fifo-channel-gk20a
 *   - @ref SWUTS-fifo-channel-gm20b
 *   - @ref SWUTS-fifo-channel-gv11b
 *   - @ref SWUTS-fifo-ctxsw_timeout-gv11b
 *   - @ref SWUTS-fifo-engine
 *   - @ref SWUTS-fifo-engine-gm20b
 *   - @ref SWUTS-fifo-engine-gp10b
 *   - @ref SWUTS-fifo-engine-gv100
 *   - @ref SWUTS-fifo-engine-gv11b
 *   - @ref SWUTS-fifo-fifo
 *   - @ref SWUTS-fifo-fifo-gk20a
 *   - @ref SWUTS-fifo-fifo-gv11b
 *   - @ref SWUTS-fifo-pbdma
 *   - @ref SWUTS-fifo-pbdma-gm20b
 *   - @ref SWUTS-fifo-pbdma-gp10b
 *   - @ref SWUTS-fifo-pbdma-gv11b
 *   - @ref SWUTS-fifo-preempt
 *   - @ref SWUTS-fifo-preempt-gv11b
 *   - @ref SWUTS-fifo-ramfc-gp10b
 *   - @ref SWUTS-fifo-ramfc-gv11b
 *   - @ref SWUTS-fifo-ramin-gk20a
 *   - @ref SWUTS-fifo-ramin-gm20b
 *   - @ref SWUTS-fifo-ramin-gp10b
 *   - @ref SWUTS-fifo-ramin-gv11b
 *   - @ref SWUTS-fifo-runlist
 *   - @ref SWUTS-fifo-runlist-gk20a
 *   - @ref SWUTS-fifo-runlist-gv11b
 *   - @ref SWUTS-fifo-tsg
 *   - @ref SWUTS-fifo-tsg-gv11b
 *   - @ref SWUTS-fifo-userd-gk20a
 *   - @ref SWUTS-fifo-usermode-gv11b
 *   - @ref SWUTS-nvgpu-sync
 *   - @ref SWUTS-init
 *   - @ref SWUTS-intr
 *   - @ref SWUTS-interface-atomic
 *   - @ref SWUTS-ltc
 *   - @ref SWUTS-nvgpu-rc
 *   - @ref SWUTS-mc
 *   - @ref SWUTS-mm-allocators-bitmap-allocator
 *   - @ref SWUTS-mm-allocators-buddy-allocator
 *   - @ref SWUTS-mm-allocators-nvgpu-allocator
 *   - @ref SWUTS-mm-as
 *   - @ref SWUTS-mm-dma
 *   - @ref SWUTS-mm-gmmu-page_table
 *   - @ref SWUTS-mm-gmmu-pd_cache
 *   - @ref SWUTS-mm-hal-cache-flush-gk20a-fusa
 *   - @ref SWUTS-mm-hal-cache-flush-gv11b-fusa
 *   - @ref SWUTS-mm-hal-gmmu-gmmu_gk20a_fusa
 *   - @ref SWUTS-mm-hal-gmmu-gmmu_gm20b_fusa
 *   - @ref SWUTS-mm-hal-gmmu-gmmu_gp10b_fusa
 *   - @ref SWUTS-mm-hal-gmmu-gmmu_gv11b_fusa
 *   - @ref SWUTS-mm-hal-gp10b_fusa
 *   - @ref SWUTS-mm-hal-gv11b-fusa
 *   - @ref SWUTS-mm-hal-mmu_fault-gv11b_fusa
 *   - @ref SWUTS-mm-nvgpu-mem
 *   - @ref SWUTS-mm-nvgpu-sgt
 *   - @ref SWUTS-mm-page_table_faults
 *   - @ref SWUTS-mm-mm
 *   - @ref SWUTS-mm-vm
 *   - @ref SWUTS-nvgpu-sync
 *   - @ref SWUTS-fb
 *   - @ref SWUTS-fbp
 *   - @ref SWUTS-fuse
 *   - @ref SWUTS-posix-bitops
 *   - @ref SWUTS-posix-cond
 *   - @ref SWUTS-posix-log2
 *   - @ref SWUTS-posix-fault-injection
 *   - @ref SWUTS-posix-sizes
 *   - @ref SWUTS-posix-thread
 *   - @ref SWUTS-posix-timers
 *   - @ref SWUTS-posix-queue
 *   - @ref SWUTS-posix-bug
 *   - @ref SWUTS-posix-ossched
 *   - @ref SWUTS-posix-rwsem
 *   - @ref SWUTS-posix-utils
 *   - @ref SWUTS-posix-circbuf
 *   - @ref SWUTS-posix-kmem
 *   - @ref SWUTS-priv_ring
 *   - @ref SWUTS-ptimer
 *   - @ref SWUTS-sdl
 *   - @ref SWUTS-therm
 *   - @ref SWUTS-acr
 *   - @ref SWUTS-ce
 *   - @ref SWUTS-cg
 *   - @ref SWUTS-init_test
 *   - @ref SWUTS-power_mgmt
 *   - @ref SWUTS-qnx-fuse
 *   - @ref SWUTS-nvrm_dev_os
 *   - @ref SWUTS-devctl_ctrl
 *   - @ref SWUTS-channel_os
 *   - @ref SWUTS-top
 *   - @ref SWUTS-class
 *   - @ref SWUTS-gr
 *   - @ref SWUTS-gr-setup
 *   - @ref SWUTS-gr-intr
 *   - @ref SWUTS-gr-init-hal-gv11b
 *   - @ref SWUTS-gr-falcon
 *   - @ref SWUTS-gr-falcon-gm20b
 *   - @ref SWUTS-gr-fs-state
 *   - @ref SWUTS-gr-global-ctx
 *   - @ref SWUTS-gr-ctx
 *   - @ref SWUTS-gr-obj-ctx
 *   - @ref SWUTS-gr-config
 *   - @ref SWUTS-ecc
 *   - @ref SWUTS-pmu
 *   - @ref SWUTS-io
 *
 */

