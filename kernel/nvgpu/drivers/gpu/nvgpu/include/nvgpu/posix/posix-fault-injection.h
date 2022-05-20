/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_POSIX_FAULT_INJECTION_H
#define NVGPU_POSIX_FAULT_INJECTION_H

/**
 * State for fault injection instance.
 */
struct nvgpu_posix_fault_inj {
	bool enabled;
	unsigned int counter;
	unsigned long bitmask;
};

/**
 * Container for all fault injection instances.
 */
struct nvgpu_posix_fault_inj_container {
	/* nvgpu-core */
	struct nvgpu_posix_fault_inj thread_fi;
	struct nvgpu_posix_fault_inj thread_running_true_fi;
	struct nvgpu_posix_fault_inj thread_serial_fi;
	struct nvgpu_posix_fault_inj cond_fi;
	struct nvgpu_posix_fault_inj cond_broadcast_fi;
	struct nvgpu_posix_fault_inj fstat_op;
	struct nvgpu_posix_fault_inj fread_op;
	struct nvgpu_posix_fault_inj kmem_fi;
	struct nvgpu_posix_fault_inj nvgpu_fi;
	struct nvgpu_posix_fault_inj golden_ctx_verif_fi;
	struct nvgpu_posix_fault_inj local_golden_image_fi;
	struct nvgpu_posix_fault_inj dma_fi;
	struct nvgpu_posix_fault_inj queue_out_fi;
	struct nvgpu_posix_fault_inj timers_fi;
	struct nvgpu_posix_fault_inj falcon_memcpy_fi;

	/* qnx */
	struct nvgpu_posix_fault_inj usleep_fi;
	struct nvgpu_posix_fault_inj sdl_nvguard_fi;
	struct nvgpu_posix_fault_inj clock_gettime_fi;
	struct nvgpu_posix_fault_inj chip_id;
	struct nvgpu_posix_fault_inj chip_id_tegra14;
	struct nvgpu_posix_fault_inj nvdt_node_compatible;
	struct nvgpu_posix_fault_inj nvdt_read_prop_array_by_name_fi;
	struct nvgpu_posix_fault_inj nvdt_read_prop_array_by_index_var;
	struct nvgpu_posix_fault_inj nvdt_node_get_prop_fi;
	struct nvgpu_posix_fault_inj tegra_platform_null_fi;
	struct nvgpu_posix_fault_inj tegra_platform_fpga_fi;
	struct nvgpu_posix_fault_inj nvgpu_platform_simulation_var;
	struct nvgpu_posix_fault_inj nvdt_open_fi;
	struct nvgpu_posix_fault_inj check_os_native_var;
	struct nvgpu_posix_fault_inj get_chip_revision_var;
	struct nvgpu_posix_fault_inj nvdt_reg_prop_fi;
	struct nvgpu_posix_fault_inj nvdt_reg_prop_zero_fi;
	struct nvgpu_posix_fault_inj nvdt_reg_prop_size_zero_fi;
	struct nvgpu_posix_fault_inj nvmap_alloc_fi;
	struct nvgpu_posix_fault_inj nvmap_physinfo_fi;
	struct nvgpu_posix_fault_inj nvmap_mmap_fi;
	struct nvgpu_posix_fault_inj nvmap_handleparams_fi;
	struct nvgpu_posix_fault_inj nvmap_useddesc_fi;
	struct nvgpu_posix_fault_inj nvmap_munmap_fi;
	struct nvgpu_posix_fault_inj nvmap_importid_fi;
	struct nvgpu_posix_fault_inj nvmap_unmap_user_mode_fi;
	struct nvgpu_posix_fault_inj nvmap_user_mode_fi;
	struct nvgpu_posix_fault_inj nvmap_user_mode_addr_fi;
	struct nvgpu_posix_fault_inj nvmap_user_mode_size_fi;
	struct nvgpu_posix_fault_inj nvgpu_hv_ipa_pa_var;
	struct nvgpu_posix_fault_inj nvgpu_io_map_var;
	struct nvgpu_posix_fault_inj nvgpu_readl_fi;
	struct nvgpu_posix_fault_inj nvgpu_readl_impl_fi;
	struct nvgpu_posix_fault_inj report_error_fi;
	struct nvgpu_posix_fault_inj nvhost1x_open_fi;
	struct nvgpu_posix_fault_inj nvhost1x_syncpointallocate_fi;
	struct nvgpu_posix_fault_inj nvhost1x_getid_fi;
	struct nvgpu_posix_fault_inj nvhost1x_waiterallocate_fi;
	struct nvgpu_posix_fault_inj nvhost1x_syncpointread_fi;
	struct nvgpu_posix_fault_inj nvhost1x_creatememhandle_fi;
	struct nvgpu_posix_fault_inj nvhost1x_createfullhandle_fi;
	struct nvgpu_posix_fault_inj iofunc_devctl_default;
	struct nvgpu_posix_fault_inj iofunc_devctl_verify;
	struct nvgpu_posix_fault_inj iofunc_attr_lock;
	struct nvgpu_posix_fault_inj iofunc_attr_unlock;
	struct nvgpu_posix_fault_inj resmgr_msgread_fi;
	struct nvgpu_posix_fault_inj resmgr_msgwrite_fi;
	struct nvgpu_posix_fault_inj resmgr_msgreply_fi;
	struct nvgpu_posix_fault_inj resmgr_attach_fi;
	struct nvgpu_posix_fault_inj dispatch_create_fi;
	struct nvgpu_posix_fault_inj thread_pool_create_fi;
	struct nvgpu_posix_fault_inj thread_pool_start_fi;
	struct nvgpu_posix_fault_inj resmgr_detach_fi;
	struct nvgpu_posix_fault_inj nvclock_device_clock_control_fi;
	struct nvgpu_posix_fault_inj nvclock_set_clock_freq_fi;
	struct nvgpu_posix_fault_inj nvclock_get_clock_freq_fi;
	struct nvgpu_posix_fault_inj nvclock_get_max_clock_freq_fi;
	struct nvgpu_posix_fault_inj nvdt_node_get_prop_clock_rates_fi;
	struct nvgpu_posix_fault_inj qnx_intattach_event_fi;
	struct nvgpu_posix_fault_inj qnx_int_wait_fi;
	struct nvgpu_posix_fault_inj guest_vm_id_fi;
	struct nvgpu_posix_fault_inj nvgpu_readl_loop_fi;
	struct nvgpu_posix_fault_inj tegra_sys_init_fi;
	struct nvgpu_posix_fault_inj waitfor_fi;
	struct nvgpu_posix_fault_inj procmgr_daemon_fi;
	struct nvgpu_posix_fault_inj procmgr_ability_fi;
	struct nvgpu_posix_fault_inj sem_init_fi;
	struct nvgpu_posix_fault_inj sem_wait_fi;
	struct nvgpu_posix_fault_inj sem_post_fi;
	struct nvgpu_posix_fault_inj pthread_sigmask_fi;
	struct nvgpu_posix_fault_inj sigaction_fi;
	struct nvgpu_posix_fault_inj sigaction_execute_handler_fi;
	struct nvgpu_posix_fault_inj nvclock_get_device_clock_status_fi;
	struct nvgpu_posix_fault_inj nvclk_get_device_clock_status_fi;
	struct nvgpu_posix_fault_inj syncpt_alloc_buf_fi;
};

/**
 * @brief Initialize fault injection container for this thread.
 *
 * c [in]	Pointer to fault injection container to use in this thread.
 *
 * The fault injection container stores the fault injection state for all
 * the fault injection users. This container is in thread-local-storage and
 * must be initialized for a unit test thread and any child threads that need
 * to have the same fault injection state.
 */
void nvgpu_posix_init_fault_injection
				(struct nvgpu_posix_fault_inj_container *c);

/**
 * @brief Get the fault injection container for this thread.
 *
 * @return A pointer to the fault injection container in use for this thread.
 */
struct nvgpu_posix_fault_inj_container
	*nvgpu_posix_fault_injection_get_container(void);

/**
 * nvgpu_posix_enable_fault_injection - Enable/Disable fault injection for the
 *                                      object after @number calls to the
 *                                      module. This depends on
 *                                      nvgpu_posix_fault_injection_handle_call
 *                                      being called by each function in the
 *                                      module that can fault. Only routines
 *                                      that can fault will decrement the
 *                                      delay count.
 *
 * @fi - pointer to the fault_inj object.
 * @enable - true to enable.  false to disable.
 * @number - After <number> of calls to kmem alloc or cache routines, enable or
 *           disable fault injection. Use 0 to enable/disable immediately.
 */
void nvgpu_posix_enable_fault_injection(struct nvgpu_posix_fault_inj *fi,
					bool enable, unsigned int number);


/**
 * nvgpu_posix_is_fault_injection_triggered - Query if fault injection is
 *                                            currently enabled for the @fi
 *                                            object.
 *
 * @fi - pointer to the fault_inj object
 *
 * Returns true if fault injections are enabled.
 */
bool nvgpu_posix_is_fault_injection_triggered(struct nvgpu_posix_fault_inj *fi);

/**
 * nvgpu_posix_is_fault_injection_cntr_set - Query if fault injection is
 *                                           set to trigger in future for the
 *                                           @fi object.
 *
 * @fi - pointer to the fault_inj object
 *
 * Returns true if fault injection counter is non-zero.
 */
bool nvgpu_posix_is_fault_injection_cntr_set(struct nvgpu_posix_fault_inj *fi);

/**
 * nvgpu_posix_fault_injection_handle_call - Called by module functions to
 *                                           track enabling or disabling fault
 *                                           injections. Returns true if the
 *                                           module should return an error.
 *
 * @fi - pointer to the fault_inj object
 *
 * Returns true if the module should return an error.
 */
bool nvgpu_posix_fault_injection_handle_call(struct nvgpu_posix_fault_inj *fi);

/**
 * nvgpu_posix_set_fault_injection - Set fault injection bitmask for the given
 *				     object @fi with @bitmask and @number of
 * 				     times fi is needed. For example a bitmask
 * 				     0x12 and number as 6 will inject fault at
 * 				     2nd and 5th iteration. Currently it only
 * 				     supports upto 64 counter with bitmask. In
 *                                   future an array of bitmask can be passed
 *                                   and only implementation of this function
 *                                   need to be changed.
 *
 * @fi - pointer to the fault_inj object.
 * @bitmask - Call Interation to be faulted in bitmask format.
 * @number - Fault injection supported upto <number> count.
 */
void nvgpu_posix_set_fault_injection_bitmask(struct nvgpu_posix_fault_inj *fi,
	unsigned long *bitmask, unsigned int number);

/**
 * nvgpu_posix_fault_injection_handle_call - Reset the bitmask fi.
 *
 * @fi - pointer to the fault_inj object
 *
 */
void nvgpu_posix_reset_fault_injection_bitmask(struct nvgpu_posix_fault_inj *fi);

#endif /* NVGPU_POSIX_FAULT_INJECTION_H */
