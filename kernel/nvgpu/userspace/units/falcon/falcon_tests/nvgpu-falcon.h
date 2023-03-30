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

struct gk20a;
struct unit_module;

/** @addtogroup SWUTS-falcon
 *  @{
 *
 * Software Unit Test Specification for falcon
 */

/**
 * Test specification for: test_falcon_sw_init_free
 *
 * Description: The falcon unit shall be able to initialize the falcon's
 * base register address, required software setup for valid falcon ID.
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: nvgpu_falcon_get_instance, nvgpu_falcon_sw_init,
 *	    nvgpu_falcon_sw_free, gops_pmu.falcon_base_addr,
 *	    gv11b_pmu_falcon_base_addr, gops_pmu.setup_apertures,
 *	    gv11b_setup_apertures, gops_pmu.flcn_setup_boot_config,
 *	    gv11b_pmu_flcn_setup_boot_config
 *
 * Input: None.
 *
 * Steps:
 * - Invoke nvgpu_falcon_sw_init with valid falcon ID before initializing HAL.
 *   - Verify that falcon initialization fails since valid gpu_arch|impl
 *     are not initialized.
 * - Invoke nvgpu_falcon_sw_free with above falcon ID.
 * - Initialize the test environment:
 *   - Register read/write IO callbacks that handle falcon IO as well.
 *   - Add relevant fuse registers to the register space.
 *   - Initialize hal to setup the hal functions.
 *   - Initialize UTF (Unit Test Framework) falcon structures for PMU and
 *     GPCCS falcons.
 *   - Create and initialize test buffer with random data.
 * - Invoke nvgpu_falcon_sw_init with invalid falcon ID.
 *   - Verify that falcon initialization fails.
 * - Invoke nvgpu_falcon_sw_free with above falcon ID.
 * - Invoke nvgpu_falcon_sw_init with valid falcon ID.
 *   - Verify that falcon initialization succeeds.
 * - Invoke nvgpu_falcon_sw_free with above falcon ID.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_falcon_sw_init_free(struct unit_module *m, struct gk20a *g,
			     void *__args);

/**
 * Test specification for: test_falcon_get_id
 *
 * Description: The falcon unit shall be able to return the falcon ID
 * for the falcon.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_falcon_get_id
 *
 * Input: None.
 *
 * Steps:
 * - Invoke nvgpu_falcon_get_id with the gpccs falcon struct.
 *   - Verify that return falcon ID is #FALCON_ID_GPCCS.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_falcon_get_id(struct unit_module *m, struct gk20a *g,
			     void *__args);

/**
 * Test specification for: test_falcon_reset
 *
 * Description: The falcon unit shall be able to reset the falcon CPU or trigger
 * engine specific reset for valid falcon ID.
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: nvgpu_falcon_reset, gops_falcon.reset, gk20a_falcon_reset
 *
 * Input: None.
 *
 * Steps:
 * - Invoke nvgpu_falcon_reset with NULL falcon pointer.
 *   - Verify that reset fails with -EINVAL return value.
 * - Invoke nvgpu_falcon_reset with uninitialized falcon struct.
 *   - Verify that reset fails with -EINVAL return value.
 * - Invoke nvgpu_falcon_reset with valid falcon ID.
 *   - Verify that falcon initialization succeeds and check that bit
 *     falcon_cpuctl_hreset_f is set in falcon_cpuctl register.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_falcon_reset(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_falcon_mem_scrub
 *
 * Description: The falcon unit shall be able to check and return the falcon
 * memory scrub status.
 *
 * Test Type: Feature, Error guessing, Error injection
 *
 * Targets: nvgpu_falcon_mem_scrub_wait, gops_falcon.is_falcon_scrubbing_done,
 *	    gk20a_is_falcon_scrubbing_done
 *
 * Input: None.
 *
 * Steps:
 * - Invoke nvgpu_falcon_mem_scrub_wait with uninitialized falcon struct.
 *   - Verify that wait fails with -EINVAL return value.
 * - Invoke nvgpu_falcon_mem_scrub_wait with initialized falcon struct where
 *   underlying falcon's memory scrub is completed.
 *   - Verify that wait succeeds with 0 return value.
 * - Invoke nvgpu_falcon_mem_scrub_wait with initialized falcon struct where
 *   underlying falcon's memory scrub is yet to complete.
 *   - Verify that wait fails with -ETIMEDOUT return value.
 * - Enable fault injection for the timer init call for branch coverage.
 *   - Verify that wait fails with -ETIMEDOUT return value.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_falcon_mem_scrub(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_falcon_idle
 *
 * Description: The falcon unit shall be able to check and return the falcon
 * idle status.
 *
 * Test Type: Feature, Error guessing
 *
 * Input: None.
 *
 * Targets: nvgpu_falcon_wait_idle, gops_falcon.is_falcon_idle,
 *	    gk20a_is_falcon_idle
 *
 * Steps:
 * - Invoke nvgpu_falcon_wait_idle with uninitialized falcon struct.
 *   - Verify that wait fails with -EINVAL return value.
 * - Invoke nvgpu_falcon_wait_idle with initialized falcon struct where
 *   underlying falcon is idle.
 *   - Verify that wait succeeds with 0 return value.
 * - Invoke nvgpu_falcon_wait_idle with initialized falcon struct where
 *   underlying falcon's ext units are busy but falcon CPU is idle.
 *   - Verify that wait fails with -ETIMEDOUT return value.
 * - Invoke nvgpu_falcon_wait_idle with initialized falcon struct where
 *   underlying falcon is not idle.
 *   - Verify that wait fails with -ETIMEDOUT return value.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_falcon_idle(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_falcon_halt
 *
 * Description: The falcon unit shall be able to check and return the falcon
 * halt status.
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: nvgpu_falcon_wait_for_halt, gops_falcon.is_falcon_cpu_halted,
 *	    gk20a_is_falcon_cpu_halted
 *
 * Input: None.
 *
 * Steps:
 * - Invoke nvgpu_falcon_wait_for_halt with uninitialized falcon struct.
 *   - Verify that wait fails with -EINVAL return value.
 * - Invoke nvgpu_falcon_wait_for_halt with initialized falcon struct where
 *   underlying falcon is halted.
 *   - Verify that wait succeeds with 0 return value.
 * - Invoke nvgpu_falcon_wait_for_halt with initialized falcon struct where
 *   underlying falcon is not halted.
 *   - Verify that wait fails with -ETIMEDOUT return value.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_falcon_halt(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_falcon_mem_rw_init
 *
 * Description: The falcon unit shall be able to write to falcon's IMEM and
 * DMEM.
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: nvgpu_falcon_copy_to_imem, nvgpu_falcon_copy_to_dmem,
 *	    gops_falcon.copy_to_imem, gops_falcon.copy_to_dmem,
 *	    gk20a_falcon_copy_to_imem, gk20a_falcon_copy_to_dmem
 *
 * Input: None.
 *
 * Steps:
 * - Invoke nvgpu_falcon_copy_to_imem and nvgpu_falcon_copy_to_dmem with
 *   uninitialized falcon struct with sample random data.
 *   - Verify that writes fail with -EINVAL return value in both cases.
 * - Invoke nvgpu_falcon_copy_to_imem and nvgpu_falcon_copy_to_dmem with
 *   initialized falcon struct with sample random data.
 *   - Verify that writes succeed with 0 return value in both cases.
 * - Invoke nvgpu_falcon_copy_to_imem and nvgpu_falcon_copy_to_dmem with
 *   initialized falcon struct with sample random data of size that is
 *   not multiple of words.
 *   - Verify that writes succeed with 0 return value in both cases.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_falcon_mem_rw_init(struct unit_module *m, struct gk20a *g,
			    void *__args);

/**
 * Test specification for: test_falcon_mem_rw_range
 *
 * Description: The falcon unit shall be able to write to falcon's IMEM and
 * DMEM in accessible range.
 *
 * Test Type: Feature, Boundary values
 *
 * Targets: nvgpu_falcon_copy_to_imem, nvgpu_falcon_copy_to_dmem,
 *	    gops_falcon.copy_to_imem, gops_falcon.copy_to_dmem,
 *	    gops_falcon.get_mem_size, gk20a_falcon_copy_to_imem,
 *	    gk20a_falcon_copy_to_dmem, gk20a_falcon_get_mem_size
 *
 * Input: None.
 *
 * Steps:
 * - Invoke nvgpu_falcon_copy_to_imem and nvgpu_falcon_copy_to_dmem with
 *   initialized falcon struct with sample random data and valid range.
 *   - Verify that writes succeed with 0 return value in both cases.
 * - Invoke nvgpu_falcon_copy_to_imem and nvgpu_falcon_copy_to_dmem with
 *   initialized falcon struct with sample random data and invalid range
 *   with valid and invalid offset.
 *   - Verify that writes fail with -EINVAL return value in both cases.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_falcon_mem_rw_range(struct unit_module *m, struct gk20a *g,
			     void *__args);

/**
 * Test specification for: test_falcon_mem_rw_fault
 *
 * Description: The falcon unit shall fail the call to copy to DMEM when
 *		DMEMC reads return invalid value due to HW fault.
 *
 * Test Type: Error injection
 *
 * Targets: nvgpu_falcon_copy_to_dmem, gops_falcon.copy_to_dmem,
 *	    gk20a_falcon_copy_to_dmem
 *
 * Input: None.
 *
 * Steps:
 * - Enable the falcon DMEMC read fault.
 * - Invoke nvgpu_falcon_copy_to_dmem with initialized falcon struct with
 *   sample random data and valid range.
 * - Disable the falcon DMEMC read fault.
 * - Verify that writes failed.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_falcon_mem_rw_fault(struct unit_module *m, struct gk20a *g,
			     void *__args);

/**
 * Test specification for: test_falcon_mem_rw_aligned
 *
 * Description: The falcon unit shall be able to write to falcon's IMEM and
 * DMEM only at aligned offsets.
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: nvgpu_falcon_copy_to_imem, nvgpu_falcon_copy_to_dmem,
 *	    gops_falcon.copy_to_imem, gops_falcon.copy_to_dmem,
 *	    gk20a_falcon_copy_to_imem, gk20a_falcon_copy_to_dmem
 *
 * Input: None.
 *
 * Steps:
 * - Invoke nvgpu_falcon_copy_to_imem and nvgpu_falcon_copy_to_dmem with
 *   initialized falcon struct with sample random data and 4-byte aligned
 *   offset.
 *   - Verify that writes succeed with 0 return value in both cases.
 * - Invoke nvgpu_falcon_copy_to_imem and nvgpu_falcon_copy_to_dmem with
 *   initialized falcon struct with sample random data and non 4-byte
 *   aligned offset.
 *   - Verify that writes fail with -EINVAL return value in both cases.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_falcon_mem_rw_aligned(struct unit_module *m, struct gk20a *g,
			       void *__args);

/**
 * Test specification for: test_falcon_mem_rw_zero
 *
 * Description: The falcon unit shall fail the API call to write zero
 * bytes to falcon memory.
 *
 * Test Type: Error guessing
 *
 * Targets: nvgpu_falcon_copy_to_imem, nvgpu_falcon_copy_to_dmem,
 *	    gops_falcon.copy_to_imem, gops_falcon.copy_to_dmem,
 *	    gk20a_falcon_copy_to_imem, gk20a_falcon_copy_to_dmem
 *
 * Input: None.
 *
 * Steps:
 * - Invoke nvgpu_falcon_copy_to_imem and nvgpu_falcon_copy_to_dmem with
 *   initialized falcon struct with sample random data and zero bytes.
 *   - Verify that writes fail with -EINVAL return value in both cases.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_falcon_mem_rw_zero(struct unit_module *m, struct gk20a *g,
			    void *__args);

/**
 * Test specification for: test_falcon_mailbox
 *
 * Description: The falcon unit shall read and write value of falcon's mailbox
 * registers.
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: nvgpu_falcon_mailbox_read, nvgpu_falcon_mailbox_write,
 *	    gops_falcon.mailbox_read, gops_falcon.mailbox_write,
 *	    gk20a_falcon_mailbox_read, gk20a_falcon_mailbox_write
 *
 * Input: None.
 *
 * Steps:
 * - Invoke nvgpu_falcon_mailbox_read and nvgpu_falcon_mailbox_write with
 *   uninitialized falcon struct.
 *   - Verify that read returns zero.
 * - Write a sample value to mailbox registers and read using the nvgpu APIs.
 *   - Verify the value by reading the registers through IO accessor.
 * - Read/Write value from invalid mailbox register of initialized falcon.
 *   - Verify that read returns zero.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_falcon_mailbox(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_falcon_bootstrap
 *
 * Description: The falcon unit shall configure the bootstrap parameters into
 * falcon memory and registers.
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: nvgpu_falcon_hs_ucode_load_bootstrap, gops_falcon.bootstrap,
 *	    gk20a_falcon_bootstrap
 *
 * Input: None.
 *
 * Steps:
 * - Invoke nvgpu_falcon_hs_ucode_load_bootstrap with uninitialized
 *   falcon struct.
 *   - Verify that call fails with -EINVAL return value.
 * - Fetch the ACR firmware from filesystem.
 * - Invoke nvgpu_falcon_hs_ucode_load_bootstrap with initialized falcon struct.
 *   Fail the falcon reset by failing mem scrub wait.
 *   - Verify that bootstrap fails.
 * - Invoke nvgpu_falcon_hs_ucode_load_bootstrap with initialized falcon struct.
 *   Fail the imem copy for non-secure code by setting invalid size in ucode
 *   header.
 *   - Verify that bootstrap fails.
 * - Invoke nvgpu_falcon_hs_ucode_load_bootstrap with initialized falcon struct.
 *   Fail the imem copy for secure code by setting invalid size in ucode header.
 *   - Verify that bootstrap fails.
 * - Invoke nvgpu_falcon_hs_ucode_load_bootstrap with initialized falcon struct.
 *   Fail the imem copy for secure code by setting invalid size in ucode header.
 *   - Verify that bootstrap fails.
 * - Invoke nvgpu_falcon_hs_ucode_load_bootstrap with initialized falcon struct.
 *   Fail the dmem copy setting invalid dmem size in ucode header.
 *   - Verify that bootstrap fails.
 * - Invoke nvgpu_falcon_hs_ucode_load_bootstrap with initialized falcon struct.
 *   - Verify that bootstrap succeeds and verify the expected state of registers
 *     falcon_dmactl_r, falcon_falcon_bootvec_r, falcon_falcon_cpuctl_r.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_falcon_bootstrap(struct unit_module *m, struct gk20a *g, void *__args);

/**
 * Test specification for: test_falcon_mem_rw_unaligned_cpu_buffer
 *
 * Description: The falcon unit shall be able to read/write from/to falcon's
 * IMEM and DMEM from memory buffer that is unaligned.
 *
 * Test Type: Feature
 *
 * Targets: nvgpu_falcon_copy_to_imem, nvgpu_falcon_copy_to_dmem,
 *	    gops_falcon.copy_to_imem, gops_falcon.copy_to_dmem,
 *	    gk20a_falcon_copy_to_imem, gk20a_falcon_copy_to_dmem
 *
 * Input: None.
 *
 * Steps:
 * - Initialize unaligned random data memory buffer and set size.
 * - Invoke nvgpu_falcon_copy_to_imem and nvgpu_falcon_copy_to_dmem with
 *   initialized falcon struct with above initialized sample random data
 *   and valid range.
 *   - Verify that writes succeed with 0 return value in both cases.
 * - Write data of size 1K to valid range in imem/dmem from unaligned data
 *   to verify the buffering logic and cover branches in
 *   falcon_copy_to_dmem|imem_unaligned_src.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_falcon_mem_rw_unaligned_cpu_buffer(struct unit_module *m,
					    struct gk20a *g, void *__args);

/**
 * Test specification for: test_falcon_mem_rw_inval_port
 *
 * Description: The falcon unit shall not be able to read/write from/to falcon's
 * memory from invalid port.
 *
 * Test Type: Error guessing
 *
 * Targets: nvgpu_falcon_copy_to_imem, gops_falcon.copy_to_imem,
 *	    gops_falcon.get_ports_count, gk20a_falcon_copy_to_imem,
 *	    gk20a_falcon_get_ports_count
 *
 * Input: None.
 *
 * Steps:
 * - Invoke nvgpu_falcon_copy_to_imem and nvgpu_falcon_copy_from_imem with
 *   initialized falcon struct with initialized sample random data, valid
 *   range but invalid port.
 *   - Verify that return value is -EINVAL.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_falcon_mem_rw_inval_port(struct unit_module *m, struct gk20a *g,
				  void *__args);
/**
 * Test specification for: test_falcon_irq
 *
 * Description: The falcon unit shall be able to set or clear the falcon irq
 * mask and destination registers for supported falcons.
 *
 * Test Type: Feature, Error guessing
 *
 * Targets: nvgpu_falcon_set_irq, gops_falcon.set_irq,
 *	    gk20a_falcon_set_irq
 *
 * Input: None.
 *
 * Steps:
 * - Invoke nvgpu_falcon_set_irq with uninitialized falcon struct.
 * - Invoke nvgpu_falcon_set_irq with initialized falcon struct where
 *   underlying falcon has interrupt support disabled.
 * - Invoke nvgpu_falcon_set_irq to enable the interrupts with
 *   initialized falcon struct and sample interrupt mask and
 *   destination values and the underlying falcon has
 *   interrupt support enabled.
 *   - Verify that falcon_irqmset_r and falcon_irqdest_r are set as
 *     expected.
 * - Invoke nvgpu_falcon_set_irq to disable the interrupts with
 *   initialized falcon struct and the underlying falcon has
 *   interrupt support enabled.
 *   - Verify that falcon_irqmclr_r is set to 0xffffffff.
 *
 * Output: Returns PASS if the steps above were executed successfully. FAIL
 * otherwise.
 */
int test_falcon_irq(struct unit_module *m, struct gk20a *g, void *__args);
