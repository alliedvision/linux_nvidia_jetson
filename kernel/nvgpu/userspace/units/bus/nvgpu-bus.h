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

#ifndef UNIT_NVGPU_BUS_H
#define UNIT_NVGPU_BUS_H

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-bus
 *  @{
 *
 * Software Unit Test Specification for nvgpu.common.bus
 */

/**
 * Test specification for: test_bus_setup
 *
 * Description: Setup prerequisites for tests.
 *
 * Test Type: Other (setup)
 *
 * Input: None
 *
 * Steps:
 * - Initialize common.bus and few other necessary HAL function pointers.
 * - Map the register space for NV_PBUS, NV_PMC and NV_PTIMER.
 * - Register read/write callback functions.
 *
 * Output:
 * - UNIT_FAIL if encounters an error creating reg space
 * - UNIT_SUCCESS otherwise
 */
int test_bus_setup(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_bus_free_reg_space
 *
 * Description: Free resources from test_bus_setup()
 *
 * Test Type: Other (cleanup)
 *
 * Input: test_bus_setup() has been executed.
 *
 * Steps:
 * - Free up NV_PBUS, NV_PMC and NV_PTIMER register space.
 *
 * Output:
 * - UNIT_SUCCESS
 */
int test_bus_free_reg_space(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_init_hw
 *
 * Description: Verify the bus.init_hw and bus.configure_debug_bus HAL.
 *
 * Test Type: Feature
 *
 * Targets: gops_bus.init_hw, gk20a_bus_init_hw,
 *          gops_bus.configure_debug_bus, gv11b_bus_configure_debug_bus
 *
 * Input: test_bus_setup() has been executed.
 *
 * Steps:
 * - Initialize the Debug bus related registers to non-zero value.
 * - Set is_silicon and is_fpga flag to false to get branch coverage.
 * - Set configure_debug_bus HAL to NULL for branch coverage.
 * - Call init_hw() HAL.
 * - Read back the interrupt enable register and check if it is equal to 0.
 * - Read back the debug bus registers to make sure they are NOT zeroed out.
 *
 * - For more branch coverage, set is_silicon flag to true.
 * - Initialize the configure_debug_bus HAL to gv11b_bus_configure_debug_bus.
 * - Call init_hw() HAL.
 * - Read back the interrupt enable register and check if it is equal to 0xEU.
 * 	- PRI_SQUASH = Bit 1:1
 *	- PRI_FECSERR = Bit 2:2
 * 	- PRI_TIMEOUT = Bit 3:3
 * - Read back the debug bus registers to make sure they are zeroed out.
 *
 * - For better branch coverage, set is_silicon to false and is_fpga to true
 * - Call init_hw() HAL.
 * - Read back the interrupt enable register and check if it is equal to 0xEU.
 *
 * Output:
 * - UNIT_FAIL if above HAL fails to enable interrupts.
 * - UNIT_SUCCESS otherwise.
 */
int test_init_hw(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_bar_bind
 *
 * Description: Verify the bus.bar1_bind and bus.bar2_bind HAL.
 *
 * Test Type: Feature, Error injection
 *
 * Targets: gops_bus.bar1_bind, gm20b_bus_bar1_bind,
 *          gops_bus.bar2_bind, gp10b_bus_bar2_bind
 *
 * Input: test_bus_setup() has been executed.
 *
 * Steps:
 * - Initialize cpu_va to a known value (say 0xCE418000U).
 * - Set bus_bind_status_r to 0xF that is both bar1 and bar2 status
 *   pending and outstanding.
 * - Call bus.bar1_bind() HAL.
 * - Make sure HAL returns success as bind_status is marked as done in
 *   third polling attempt.
 * - Send error if bar1_block register is not set as expected:
 *     - Bit 27:0 - 4k aligned block pointer = bar_inst.cpu_va >> 12 = 0xCE418
 *     - Bit 29:28- Target = (11)b
 *     - Bit 30   - Debug CYA = (0)b
 *     - Bit 31   - Mode = virtual = (1)b
 * - Set bus_bind_status_r to 0x5U that is both bar1 and bar2 status
 *   is set as outstanding.
 * - Call bus.bar1_bind HAL again and except ret != 0 as the bind status
 *   will remain outstanding during this call.
 * - Set bus_bind_status_r to 0xAU that is both bar1 and bar2 status
 *   is set as pending.
 * - Call bus.bar1_bind HAL again and except ret != 0 as the bind status
 *   will remain pending during this call.
 * - The HAL should return error this time as timeout is expected to expire.
 * - Repeat the above steps for BAR2 but with different cpu_va = 0x2670C000U.
 *
 * Output:
 * - UNIT_FAIL if above HAL fails to bind BAR1/2
 * - UNIT_SUCCESS otherwise.
 */
int test_bar_bind(struct unit_module *m, struct gk20a *g, void *args);

/**
 * Test specification for: test_bus_isr
 *
 * Description: Verify the bus.isr HAL.
 *
 * Test Type: Feature, Error injection
 *
 * Targets: gops_bus.isr, gk20a_bus_isr
 *
 * Input: test_bus_setup() has been executed.
 *
 * Steps:
 * - Initialize interrupt register bus_intr_0_r() to 0x2(pri_squash)
 * - Call isr HAL.
 * - Initialize interrupt register bus_intr_0_r() to 0x4(pri_fecserr)
 * - Call isr HAL.
 * - Initialize interrupt register bus_intr_0_r() to 0x8(pri_timeout)
 * - Call isr HAL.
 * - Initialize interrupt register bus_intr_0_r() to 0x10(fb_req_timeout)
 * - Call isr HAL.
 *
 * Output:
 * - UNIT_SUCCESS.
 */
int test_bus_isr(struct unit_module *m, struct gk20a *g, void *args);
#endif /* UNIT_NVGPU_BUS_H */
