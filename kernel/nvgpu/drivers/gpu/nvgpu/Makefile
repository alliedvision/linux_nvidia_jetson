GCOV_PROFILE := y

# When building NVGPU as an external module, srctree.nvgpu is not
# defined. Given that NVGPU knows where its header files are located
# relatively, we can use the path information of this Makefile to
# set srctree.nvgpu if it is not already defined. Please note that
# 'lastword $(MAKEFILE_LIST)' refers to this Makefile when evaluated
# immediately.
ifeq ($(origin srctree.nvgpu), undefined)
srctree.nvgpu := $(abspath $(shell dirname $(lastword $(MAKEFILE_LIST)))/../../..)
endif

ccflags-y += -I$(srctree.nvgpu)/drivers/gpu/nvgpu/include
ccflags-y += -I$(srctree.nvgpu)/drivers/gpu/nvgpu
ccflags-y += -I$(srctree.nvgpu)/include
ccflags-y += -I$(srctree.nvgpu)/include/uapi

ccflags-y += -I$(srctree)/drivers/devfreq

ccflags-y += $(call cc-option, -Wimplicit-fallthrough=3)
ccflags-y += -Wframe-larger-than=2048
ccflags-y += -Wno-multichar
ccflags-y += -Werror
ccflags-y += -Wno-error=cpp
ifeq ($(VERSION),4)
ccflags-y += -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
endif

# Turn off when this is fixed upstream, if ever.
ccflags-y += -D__NVGPU_PREVENT_UNTRUSTED_SPECULATION

ifneq ($(srctree.nvgpu),)
include $(srctree.nvgpu)/drivers/gpu/nvgpu/Makefile.linux.configs
else
include Makefile.linux.configs
endif

# When using the upstream host1x driver, the Makefile must define the
# srctree.host1x path in order to find the necessary header files for
# the upstream host1x driver.
ifeq ($(CONFIG_TEGRA_GK20A_NVHOST_HOST1X),y)
ccflags-y += -I$(srctree.nvidia)/drivers/gpu/host1x/include
endif

# When using the upstream nvmap driver, the Makefile must define the
# srctree.nvmap path in order to find the necessary header files for
# the upstream nvmap driver.
ifeq ($(CONFIG_NVGPU_NVMAP_NEXT),y)
ifeq ($(origin srctree.nvmap), undefined)
srctree.nvmap := $(srctree.nvidia)/drivers/video/tegra/nvmap
endif
ccflags-y += -I$(srctree.nvmap)/include
endif

ifeq ($(CONFIG_NVGPU_DEBUGGER),y)
ccflags-y += -DCONFIG_NVGPU_DEBUGGER
ccflags-y += -DCONFIG_NVGPU_ENGINE_RESET
endif

ifeq ($(CONFIG_NVGPU_IVM_BUILD),y)
ccflags-y += -DCONFIG_NVGPU_IVM_BUILD
endif

ifeq ($(CONFIG_TEGRA_L1SS_SUPPORT),y)
ccflags-y += -DCONFIG_TEGRA_L1SS_SUPPORT
endif

ccflags-y += -DCONFIG_NVGPU_DETERMINISTIC_CHANNELS
ccflags-y += -DCONFIG_NVGPU_STATIC_POWERGATE
ccflags-y += -DCONFIG_NVGPU_ACR_LEGACY
ccflags-y += -DCONFIG_NVGPU_ENGINE_QUEUE
ccflags-y += -DCONFIG_NVGPU_FIFO_ENGINE_ACTIVITY
ccflags-y += -DCONFIG_NVGPU_USERD
ccflags-y += -DCONFIG_NVGPU_CHANNEL_WDT
ccflags-y += -DCONFIG_NVGPU_LS_PMU
ccflags-y += -DCONFIG_NVGPU_CILP
ccflags-y += -DCONFIG_NVGPU_GFXP
ccflags-y += -DCONFIG_NVGPU_GRAPHICS
ccflags-y += -DCONFIG_NVGPU_REPLAYABLE_FAULT
ccflags-y += -DCONFIG_NVGPU_CHANNEL_TSG_SCHEDULING
ccflags-y += -DCONFIG_NVGPU_CHANNEL_TSG_CONTROL
ccflags-y += -DCONFIG_NVGPU_POWER_PG
ccflags-y += -DCONFIG_NVGPU_KERNEL_MODE_SUBMIT
ccflags-y += -DCONFIG_NVGPU_SIM
ccflags-y += -DCONFIG_NVGPU_TRACE
ccflags-y += -DCONFIG_NVGPU_SYSFS
ccflags-y += -DCONFIG_NVGPU_CLK_ARB
ccflags-y += -DCONFIG_NVGPU_FALCON_DEBUG
ccflags-y += -DCONFIG_NVGPU_FALCON_NON_FUSA
ccflags-y += -DCONFIG_NVGPU_IOCTL_NON_FUSA
ccflags-y += -DCONFIG_NVGPU_NON_FUSA
ccflags-y += -DCONFIG_NVGPU_INJECT_HWERR
ccflags-y += -DCONFIG_NVGPU_NONSTALL_INTR
ccflags-y += -DCONFIG_NVGPU_GR_FALCON_NON_SECURE_BOOT
ccflags-y += -DCONFIG_NVGPU_SET_FALCON_ACCESS_MAP
ccflags-y += -DCONFIG_NVGPU_SW_SEMAPHORE
ccflags-y += -DCONFIG_NVGPU_FENCE
ccflags-y += -DCONFIG_NVGPU_PROFILER

ifeq ($(CONFIG_NVGPU_GSP_SCHEDULER),y)
ccflags-y += -DCONFIG_NVGPU_GSP_SCHEDULER
endif

ifeq ($(CONFIG_NVGPU_LOGGING),y)
ccflags-y += -DCONFIG_NVGPU_LOGGING=1
endif

ifeq ($(CONFIG_NVGPU_HAL_NON_FUSA),y)
ccflags-y += -DCONFIG_NVGPU_HAL_NON_FUSA
endif

ifeq ($(CONFIG_NVGPU_RECOVERY),y)
ccflags-y += -DCONFIG_NVGPU_RECOVERY
ccflags-y += -DCONFIG_NVGPU_ENGINE_RESET
nvgpu-y += \
	hal/rc/rc_gk20a.o \
	hal/rc/rc_gv11b.o
endif

ifeq ($(CONFIG_NVGPU_GSP_STRESS_TEST),y)
ccflags-y += -DCONFIG_NVGPU_GSP_STRESS_TEST
endif

obj-$(CONFIG_GK20A) := nvgpu.o

# OS independent parts of nvgpu. The work to collect files here
# is in progress.

ifeq ($(CONFIG_NVGPU_DGPU),y)
nvgpu-$(CONFIG_NVGPU_DGPU) += \
	os/linux/pci.o \
	os/linux/pci_power.o \
	os/linux/dmabuf_vidmem.o \
	os/linux/os_ops_gv100.o \
	os/linux/os_ops_tu104.o \
	common/sec2/sec2.o \
	common/sec2/sec2_allocator.o \
	common/sec2/sec2_lsfm.o \
	common/sec2/ipc/sec2_cmd.o \
	common/sec2/ipc/sec2_msg.o \
	common/sec2/ipc/sec2_queue.o \
	common/sec2/ipc/sec2_seq.o \
	common/vbios/bios_sw_gv100.o \
	common/vbios/bios_sw_tu104.o \
	common/falcon/falcon_sw_tu104.o \
	common/acr/acr_sw_tu104.o \
	common/mm/allocators/page_allocator.o \
	common/mm/vidmem.o \
	common/pramin.o \
	common/ce/ce_app.o \
	common/clk_arb/clk_arb_gv100.o \
	common/engine_queues/engine_emem_queue.o \
	hal/mm/mm_gv100.o \
	hal/mm/mm_tu104.o \
	hal/mc/mc_gv100.o  \
	hal/mc/mc_tu104.o  \
	hal/bus/bus_gv100.o \
	hal/bus/bus_tu104.o \
	hal/class/class_tu104.o \
	hal/clk/clk_tu104.o \
	hal/clk/clk_mon_tu104.o \
	hal/gr/init/gr_init_gv100.o \
	hal/gr/init/gr_init_tu104.o \
	hal/gr/intr/gr_intr_tu104.o \
	hal/gr/falcon/gr_falcon_tu104.o \
	hal/fbpa/fbpa_tu104.o \
	hal/init/hal_tu104.o \
	hal/init/hal_tu104_litter.o \
	hal/power_features/cg/tu104_gating_reglist.o \
	hal/ltc/ltc_tu104.o \
	hal/fb/fb_gv100.o \
	hal/fb/fb_tu104.o \
	hal/fb/fb_mmu_fault_tu104.o \
	hal/fb/intr/fb_intr_gv100.o \
	hal/fb/intr/fb_intr_tu104.o \
	hal/func/func_tu104.o \
	hal/fifo/fifo_tu104.o \
	hal/fifo/usermode_tu104.o \
	hal/fifo/pbdma_tu104.o \
	hal/fifo/ramfc_tu104.o \
	hal/fifo/ramin_tu104.o \
	hal/fifo/channel_gv100.o \
	hal/fifo/runlist_ram_tu104.o \
	hal/fifo/runlist_fifo_gv100.o \
	hal/fifo/runlist_fifo_tu104.o \
	hal/fifo/fifo_intr_gv100.o \
	hal/fuse/fuse_gp106.o \
	hal/fuse/fuse_tu104.o \
	hal/netlist/netlist_gv100.o \
	hal/netlist/netlist_tu104.o \
	hal/nvdec/nvdec_gp106.o \
	hal/nvdec/nvdec_tu104.o \
	hal/gsp/gsp_tu104.o \
	hal/sec2/sec2_tu104.o \
	hal/pramin/pramin_gp10b.o \
	hal/pramin/pramin_gv100.o \
	hal/pramin/pramin_init.o \
	hal/pramin/pramin_tu104.o \
	hal/bios/bios_tu104.o \
	hal/top/top_gv100.o \
	hal/xve/xve_gp106.o \
	hal/xve/xve_tu104.o

nvgpu-$(CONFIG_DEBUG_FS) += \
	os/linux/debug_therm_tu104.o \
	os/linux/debug_bios.o \
	os/linux/debug_xve.o \
	os/linux/debug_clk_tu104.o
endif

# nvlink sources are not conditionally compiled. nvlink probe and
# public functions return -ENODEV when not supported.

nvgpu-y += \
	common/vbios/nvlink_bios.o \
	common/nvlink/probe.o \
	common/nvlink/init/device_reginit.o \
	common/nvlink/init/device_reginit_gv100.o \
	common/nvlink/minion.o \
	common/nvlink/link_mode_transitions.o \
	common/nvlink/nvlink.o \
	os/linux/nvlink_probe.o \
	os/linux/nvlink.o \
	hal/nvlink/minion_gv100.o \
	hal/nvlink/minion_tu104.o \
	hal/nvlink/nvlink_gv100.o \
	hal/nvlink/nvlink_tu104.o \
	hal/nvlink/intr_and_err_handling_tu104.o \
	hal/nvlink/link_mode_transitions_gv100.o \
	hal/nvlink/link_mode_transitions_tu104.o

nvgpu-y += \
	common/device.o \
	common/utils/enabled.o \
	common/utils/errata.o \
	common/utils/rbtree.o \
	common/utils/string.o \
	common/utils/worker.o \
	common/timers_common.o \
	common/swdebug/profile.o \
	common/ptimer/ptimer.o \
	common/perf/perfbuf.o \
	common/therm/therm.o \
	common/mc/mc.o \
	common/sync/channel_sync.o \
	common/sync/channel_sync_semaphore.o \
	common/semaphore/semaphore_sea.o \
	common/semaphore/semaphore_pool.o \
	common/semaphore/semaphore_hw.o \
	common/semaphore/semaphore.o \
	common/regops/regops.o \
	common/ltc/ltc.o \
	common/fb/fb.o \
	common/fbp/fbp.o \
	common/gr/gr_utils.o \
	common/gr/gr.o \
	common/gr/gr_intr.o \
	common/gr/global_ctx.o \
	common/gr/ctx.o \
	common/gr/gr_falcon.o \
	common/gr/subctx.o \
	common/gr/zcull.o \
	common/gr/gr_config.o \
	common/gr/zbc.o \
	common/gr/gr_setup.o \
	common/gr/hwpm_map.o \
	common/gr/obj_ctx.o \
	common/gr/fs_state.o \
	common/gr/gr_ecc.o \
	common/netlist/netlist.o \
	common/init/nvgpu_init.o \
	common/pmu/pmu.o \
	common/pmu/allocator.o \
	common/pmu/pmu_mutex.o \
	common/pmu/fw/fw.o \
	common/pmu/fw/fw_ver_ops.o \
	common/pmu/fw/fw_ns_bootstrap.o \
	common/pmu/pg/pmu_pg.o \
	common/pmu/pg/pmu_aelpg.o \
	common/pmu/perfmon/pmu_perfmon.o \
	common/pmu/perfmon/pmu_perfmon_sw_gm20b.o \
	common/pmu/perfmon/pmu_perfmon_sw_gv11b.o \
	common/pmu/pmu_debug.o \
	common/pmu/pg/pg_sw_gm20b.o \
	common/pmu/pg/pg_sw_gp10b.o \
	common/pmu/pg/pg_sw_gp106.o \
	common/pmu/pg/pg_sw_gv11b.o \
	common/pmu/ipc/pmu_cmd.o \
	common/pmu/ipc/pmu_msg.o \
	common/pmu/ipc/pmu_queue.o \
	common/pmu/ipc/pmu_seq.o \
	common/acr/acr.o \
	common/acr/acr_wpr.o \
	common/acr/acr_blob_alloc.o \
	common/acr/acr_blob_construct_v0.o \
	common/acr/acr_blob_construct.o \
	common/acr/acr_bootstrap.o \
	common/acr/acr_sw_gm20b.o \
	common/acr/acr_sw_gp10b.o \
	common/acr/acr_sw_gv11b.o \
	common/sbr/sbr.o \
	common/pmu/super_surface/super_surface.o \
	common/pmu/lsfm/lsfm.o \
	common/pmu/lsfm/lsfm_sw_gm20b.o \
	common/pmu/lsfm/lsfm_sw_gp10b.o \
	common/pmu/lsfm/lsfm_sw_gv100.o \
	common/pmu/lsfm/lsfm_sw_tu104.o \
	common/pmu/perf/vfe_var.o \
	common/pmu/perf/vfe_equ.o \
	common/pmu/perf/perf.o \
	common/pmu/perf/change_seq.o \
	common/pmu/perf/pstate.o \
	common/pmu/pmgr/pwrdev.o \
	common/pmu/pmgr/pmgr.o \
	common/pmu/pmgr/pmgrpmu.o \
	common/pmu/pmgr/pwrmonitor.o \
	common/pmu/pmgr/pwrpolicy.o \
	common/pmu/volt/volt.o \
	common/pmu/volt/volt_rail.o \
	common/pmu/volt/volt_dev.o \
	common/pmu/volt/volt_policy.o \
	common/pmu/therm/thrm.o \
	common/pmu/therm/therm_dev.o \
	common/pmu/therm/therm_channel.o \
	common/pmu/lpwr/rppg.o \
	common/pmu/pmu_pstate.o \
	common/pmu/pmu_rtos_init.o \
	common/pmu/clk/clk_vin.o \
	common/pmu/clk/clk_fll.o \
	common/pmu/clk/clk_domain.o \
	common/pmu/clk/clk_prog.o \
	common/pmu/clk/clk_vf_point.o \
	common/pmu/clk/clk.o \
	common/pmu/boardobj/boardobj.o \
	common/pmu/boardobj/boardobjgrp.o \
	common/pmu/boardobj/boardobjgrpmask.o \
	common/pmu/boardobj/boardobjgrp_e255.o \
	common/pmu/boardobj/boardobjgrp_e32.o \
	common/clk_arb/clk_arb.o \
	common/clk_arb/clk_arb_gp10b.o \
	common/rc/rc.o \
	common/grmgr/grmgr.o \
	common/cic/mon/mon_init.o \
	common/cic/mon/mon_lut.o \
	common/cic/mon/mon_intr.o \
	common/cic/mon/mon_report_err.o \
	common/cic/rm/rm_init.o \
	common/cic/rm/rm_intr.o \
	hal/bus/bus_gk20a.o \
	hal/class/class_gm20b.o \
	hal/class/class_gp10b.o \
	hal/clk/clk_gm20b.o \
	hal/gr/ecc/ecc_gp10b.o \
	hal/gr/ecc/ecc_gv11b.o \
	hal/gr/zcull/zcull_gm20b.o \
	hal/gr/zcull/zcull_gv11b.o \
	hal/gr/ctxsw_prog/ctxsw_prog_gp10b.o \
	hal/gr/ctxsw_prog/ctxsw_prog_gv11b.o \
	hal/gr/init/gr_init_gm20b.o \
	hal/gr/init/gr_init_gp10b.o \
	hal/gr/init/gr_init_gv11b.o \
	hal/gr/intr/gr_intr_gm20b.o \
	hal/gr/intr/gr_intr_gp10b.o \
	hal/gr/hwpm_map/hwpm_map_gv100.o \
	hal/gr/zbc/zbc_gm20b.o \
	hal/gr/zbc/zbc_gp10b.o \
	hal/gr/zbc/zbc_gv11b.o \
	hal/gr/gr/gr_gk20a.o \
	hal/gr/gr/gr_gm20b.o \
	hal/gr/gr/gr_gp10b.o \
	hal/gr/gr/gr_gv100.o \
	hal/gr/gr/gr_gv11b.o \
	hal/gr/gr/gr_tu104.o \
	hal/init/hal_gv11b_litter.o \
	hal/init/hal_init.o \
	hal/perf/perf_gv11b.o \
	hal/perf/perf_tu104.o \
	hal/power_features/cg/gv11b_gating_reglist.o \
	hal/regops/regops_gv11b.o \
	hal/regops/allowlist_gv11b.o \
	hal/ce/ce2_gk20a.o \
	hal/therm/therm_gp10b.o \
	hal/therm/therm_tu104.o \
	hal/gr/falcon/gr_falcon_gm20b.o \
	hal/ltc/ltc_gp10b.o \
	hal/ltc/intr/ltc_intr_gm20b.o \
	hal/ltc/intr/ltc_intr_gp10b.o \
	hal/fb/fb_gm20b.o \
	hal/fb/fb_gp106.o \
	hal/fb/intr/fb_intr_ecc_gv11b.o \
	hal/fuse/fuse_gm20b.o \
	hal/fifo/fifo_gk20a.o \
	hal/fifo/preempt_gk20a.o \
	hal/fifo/ramfc_gk20a.o \
	hal/fifo/ramfc_gp10b.o \
	hal/fifo/ramin_gk20a.o \
	hal/fifo/ramin_gp10b.o \
	hal/fifo/runlist_fifo_gv11b.o \
	hal/fifo/channel_gk20a.o \
	hal/fifo/channel_gm20b.o \
	hal/fifo/tsg_gk20a.o \
	hal/fifo/userd_gk20a.o \
	hal/fifo/userd_gv11b.o \
	hal/fifo/fifo_intr_gk20a.o \
	hal/fifo/ctxsw_timeout_gk20a.o \
	hal/netlist/netlist_gp10b.o \
	hal/sync/sema_cmdbuf_gk20a.o \
	hal/sync/sema_cmdbuf_gv11b.o \
	hal/pmu/pmu_gk20a.o \
	hal/pmu/pmu_gm20b.o \
	hal/pmu/pmu_gp10b.o \
	hal/pmu/pmu_gv11b.o \
	hal/pmu/pmu_tu104.o \
	hal/top/top_gp106.o \
	hal/top/top_gp10b.o \
	hal/tpc/tpc_gv11b.o \
	hal/priv_ring/priv_ring_gv11b.o \
	hal/cic/mon/init_ga10b_fusa.o \
	hal/cic/mon/lut_ga10b_fusa.o

ifeq ($(CONFIG_NVGPU_SUPPORT_GV11B),y)
nvgpu-$(CONFIG_NVGPU_SUPPORT_GV11B) += \
	hal/init/hal_gv11b.o
endif

ifeq ($(CONFIG_NVGPU_INTR_DEBUG),y)
nvgpu-$(CONFIG_NVGPU_INTR_DEBUG) += \
	common/cic/mon/mon_ce.o \
	common/cic/mon/mon_ctxsw.o \
	common/cic/mon/mon_msg.o \
	common/cic/mon/mon_ecc.o \
	common/cic/mon/mon_host.o \
	common/cic/mon/mon_gr.o \
	common/cic/mon/mon_pri.o \
	common/cic/mon/mon_pmu.o \
	common/cic/mon/mon_mmu.o
endif

ifeq ($(CONFIG_NVGPU_GSP_SCHEDULER),y)
nvgpu-$(CONFIG_NVGPU_GSP_SCHEDULER) += \
	common/gsp/gsp_init.o \
	common/gsp/gsp_bootstrap.o \
	common/gsp_scheduler/ipc/gsp_seq.o \
	common/gsp_scheduler/ipc/gsp_queue.o \
	common/gsp_scheduler/ipc/gsp_cmd.o \
	common/gsp_scheduler/ipc/gsp_msg.o \
	common/gsp_scheduler/gsp_scheduler.o \
	common/gsp_scheduler/gsp_runlist.o
endif

ifeq ($(CONFIG_NVGPU_GSP_STRESS_TEST),y)
nvgpu-$(CONFIG_NVGPU_GSP_STRESS_TEST) += \
	common/gsp_test/gsp_test.o
endif

# Linux specific parts of nvgpu.
nvgpu-y += \
	os/linux/os_ops.o \
	os/linux/os_ops_gm20b.o \
	os/linux/os_ops_gp10b.o \
	os/linux/os_ops_gv11b.o \
	os/linux/kmem.o \
	os/linux/timers.o \
	os/linux/ioctl.o \
	os/linux/ioctl_ctrl.o \
	os/linux/ioctl_as.o \
	os/linux/ioctl_channel.o \
	os/linux/ioctl_tsg.o \
	os/linux/ioctl_dbg.o \
	os/linux/ioctl_prof.o \
	os/linux/ioctl_clk_arb.o \
	os/linux/cond.o \
	os/linux/nvgpu_mem.o \
	os/linux/linux-dma.o \
	os/linux/driver_common.o \
	os/linux/firmware.o \
	os/linux/thread.o \
	os/linux/vm.o \
	os/linux/intr.o \
	os/linux/sysfs.o \
	os/linux/linux-io.o \
	os/linux/io_usermode.o \
	os/linux/rwsem.o \
	os/linux/sched.o \
	os/linux/linux-channel.o \
	os/linux/sim.o \
	os/linux/sim_pci.o \
	os/linux/os_sched.o \
	os/linux/dt.o \
	os/linux/ecc_sysfs.o \
	os/linux/bsearch.o \
	os/linux/cic/cic_report_err.o \
	os/linux/dmabuf_priv.o \
	os/linux/power_ops.o \
	os/linux/periodic_timer.o

nvgpu-$(CONFIG_NVGPU_IVM_BUILD) += \
	os/linux/nvgpu_ivm.o

ifeq ($(CONFIG_NVGPU_COMPRESSION),y)
nvgpu-$(CONFIG_NVGPU_IVM_BUILD) += \
	common/cbc/contig_pool.o
endif

nvgpu-$(CONFIG_NVGPU_VPR) += os/linux/vpr.o

nvgpu-$(CONFIG_DEBUG_FS) += \
	os/linux/debug.o \
	os/linux/debug_gr.o \
	os/linux/debug_fifo.o \
	os/linux/debug_ce.o \
	os/linux/debug_pmu.o \
	os/linux/debug_pmgr.o \
	os/linux/debug_sched.o \
	os/linux/debug_allocator.o \
	os/linux/debug_hal.o \
	os/linux/debug_clk_gm20b.o \
	os/linux/debug_ltc.o \
	os/linux/debug_volt.o \
	os/linux/debug_s_param.o \
	os/linux/swprofile_debugfs.o \
	os/linux/debug_gsp.o

nvgpu-$(CONFIG_NVGPU_LOGGING) += os/linux/log.o

ifeq ($(CONFIG_NVGPU_TRACK_MEM_USAGE),y)
nvgpu-$(CONFIG_DEBUG_FS) += \
	os/linux/debug_kmem.o
endif

nvgpu-$(CONFIG_NVGPU_FECS_TRACE) += \
	common/gr/fecs_trace.o \
	hal/gr/fecs_trace/fecs_trace_gm20b.o \
	hal/gr/fecs_trace/fecs_trace_gv11b.o \
	os/linux/fecs_trace_linux.o

ifeq ($(CONFIG_NVGPU_FECS_TRACE),y)
nvgpu-$(CONFIG_DEBUG_FS) += \
	os/linux/debug_fecs_trace.o
endif

nvgpu-$(CONFIG_TEGRA_GK20A) += \
	os/linux/fuse.o \
	os/linux/module.o \
	os/linux/module_usermode.o \
	os/linux/platform_gk20a_tegra.o \
	os/linux/platform_gp10b_tegra.o \
	os/linux/platform_gv11b_tegra.o

ifeq ($(CONFIG_TEGRA_GK20A),y)
nvgpu-$(CONFIG_NVGPU_TEGRA_FUSE) += os/linux/soc.o
nvgpu-$(CONFIG_NVGPU_NVMEM_FUSE) += os/linux/nvmem.o
endif

nvgpu-$(CONFIG_NVGPU_SYNCFD_ANDROID) += \
	os/linux/sync_sema_android.o \
	os/linux/os_fence_android.o \
	os/linux/os_fence_android_sema.o

nvgpu-$(CONFIG_NVGPU_SYNCFD_STABLE) += \
	os/linux/sync_sema_dma.o \
	os/linux/os_fence_dma.o \
	os/linux/os_fence_dma_sema.o

nvgpu-$(CONFIG_TEGRA_GK20A_NVHOST) += \
	common/sync/channel_sync_syncpt.o \
	common/fence/fence_syncpt.o
ifneq ($(CONFIG_NVGPU_SYNCFD_NONE),y)
nvgpu-$(CONFIG_TEGRA_GK20A_NVHOST) += \
	os/linux/os_fence_syncpt.o
endif

ifeq ($(CONFIG_TEGRA_GK20A_NVHOST_HOST1X),y)
nvgpu-$(CONFIG_TEGRA_GK20A_NVHOST) += \
	os/linux/nvhost_host1x.o
else
nvgpu-$(CONFIG_TEGRA_GK20A_NVHOST) += \
	os/linux/nvhost.o
endif

nvgpu-$(CONFIG_TEGRA_GK20A_NVHOST) += \
	os/linux/nvhost_common.o \
	hal/sync/syncpt_cmdbuf_gk20a.o \
	hal/sync/syncpt_cmdbuf_gv11b.o \
	hal/sync/syncpt_cmdbuf_gv11b_fusa.o \
	common/sync/channel_user_syncpt.o

nvgpu-$(CONFIG_NVGPU_GR_VIRTUALIZATION) += \
	os/linux/vgpu/platform_vgpu_tegra.o \
	os/linux/vgpu/sysfs_vgpu.o \
	os/linux/vgpu/vgpu_ivc.o \
	os/linux/vgpu/vgpu_linux.o \
	os/linux/vgpu/gv11b/platform_gv11b_vgpu_tegra.o

ifeq ($(CONFIG_NVGPU_FECS_TRACE),y)
nvgpu-$(CONFIG_NVGPU_GR_VIRTUALIZATION) += \
	os/linux/vgpu/fecs_trace_vgpu_linux.o
endif

nvgpu-$(CONFIG_COMMON_CLK) += \
	os/linux/clk.o \
	os/linux/clk_ga10b.o

nvgpu-$(CONFIG_GK20A_DEVFREQ) += \
	os/linux/scale.o

nvgpu-$(CONFIG_NVGPU_SUPPORT_CDE) += \
	os/linux/cde.o \
	os/linux/cde_gm20b.o \
	os/linux/cde_gp10b.o

ifeq ($(CONFIG_DEBUG_FS),y)
nvgpu-$(CONFIG_NVGPU_SUPPORT_CDE) += \
	os/linux/debug_cde.o
endif

nvgpu-$(CONFIG_TEGRA_L1SS_SUPPORT) += \
	os/linux/cic/l1ss_report_err.o \
	hal/cic/mon/init_gv11b_non_fusa.o \
	hal/cic/mon/lut_gv11b_non_fusa.o \

nvgpu-y += \
	common/mm/allocators/nvgpu_allocator.o \
	common/mm/allocators/bitmap_allocator.o \
	common/mm/allocators/buddy_allocator.o \
	common/mm/gmmu/page_table.o \
	common/mm/gmmu/pd_cache.o \
	common/mm/gmmu/pte.o \
	common/mm/as.o \
	common/mm/vm.o \
	common/mm/vm_area.o \
	common/mm/nvgpu_mem.o \
	common/mm/nvgpu_sgt.o \
	common/mm/ipa_pa_cache.o \
	common/mm/mm.o \
	common/mm/dma.o \
	common/vbios/bios.o \
	common/falcon/falcon.o \
	common/falcon/falcon_debug.o \
	common/falcon/falcon_sw_gk20a.o \
	common/engine_queues/engine_mem_queue.o \
	common/engine_queues/engine_dmem_queue.o \
	common/engine_queues/engine_fb_queue.o \
	common/io/io.o \
	common/power_features/power_features.o \
	common/power_features/cg/cg.o \
	common/power_features/pg/pg.o \
	common/sim/sim.o \
	common/sim/sim_pci.o \
	common/sim/sim_netlist.o \
	common/fifo/fifo.o \
	common/fifo/preempt.o \
	common/fifo/channel.o \
	common/fifo/channel_wdt.o \
	common/fifo/channel_worker.o \
	common/fifo/pbdma.o \
	common/fifo/submit.o \
	common/fifo/job.o \
	common/fifo/priv_cmdbuf.o \
	common/fifo/tsg.o \
	common/fifo/runlist.o \
	common/fifo/engine_status.o \
	common/fifo/engines.o \
	common/fifo/pbdma_status.o \
	common/fifo/userd.o \
	common/fifo/watchdog.o \
	common/fence/fence.o \
	common/fence/fence_sema.o \
	common/ecc.o \
	common/log_common.o \
	common/ce/ce.o \
	common/debugger.o \
	common/profiler/profiler.o \
	common/profiler/pm_reservation.o

nvgpu-$(CONFIG_NVGPU_GR_VIRTUALIZATION) += \
	common/vgpu/ltc/ltc_vgpu.o \
	common/vgpu/fbp/fbp_vgpu.o \
	common/vgpu/fb/fb_vgpu.o \
	common/vgpu/fb/vab_vgpu.o \
	common/vgpu/gr/gr_vgpu.o \
	common/vgpu/gr/ctx_vgpu.o \
	common/vgpu/gr/subctx_vgpu.o \
	common/vgpu/top/top_vgpu.o \
	common/vgpu/fifo/fifo_vgpu.o \
	common/vgpu/fifo/channel_vgpu.o \
	common/vgpu/fifo/tsg_vgpu.o \
	common/vgpu/fifo/preempt_vgpu.o \
	common/vgpu/fifo/runlist_vgpu.o \
	common/vgpu/fifo/ramfc_vgpu.o \
	common/vgpu/fifo/userd_vgpu.o \
	common/vgpu/ce_vgpu.o \
	common/vgpu/mm/mm_vgpu.o \
	common/vgpu/mm/vm_vgpu.o \
	common/vgpu/init/init_vgpu.o \
	common/vgpu/ivc/comm_vgpu.o \
	common/vgpu/intr/intr_vgpu.o \
	common/vgpu/ptimer/ptimer_vgpu.o \
	common/vgpu/debugger_vgpu.o \
	common/vgpu/pm_reservation_vgpu.o \
	common/vgpu/perf/perf_vgpu.o \
	common/vgpu/profiler/profiler_vgpu.o \
	common/vgpu/ecc_vgpu.o \
	common/vgpu/clk_vgpu.o \
	common/vgpu/gr/fecs_trace_vgpu.o \
	hal/vgpu/init/init_hal_vgpu.o \
	hal/vgpu/fifo/fifo_gv11b_vgpu.o \
	hal/vgpu/sync/syncpt_cmdbuf_gv11b_vgpu.o

nvgpu-$(CONFIG_NVGPU_CYCLESTATS) += \
	common/perf/cyclestats_snapshot.o \
	common/cyclestats/cyclestats.o

ifeq ($(CONFIG_NVGPU_GR_VIRTUALIZATION),y)
nvgpu-$(CONFIG_NVGPU_CYCLESTATS) += \
	common/vgpu/perf/cyclestats_snapshot_vgpu.o
nvgpu-$(CONFIG_NVGPU_COMPRESSION) += \
	common/vgpu/cbc/cbc_vgpu.o
endif

nvgpu-$(CONFIG_NVGPU_COMPRESSION) += \
	os/linux/comptags.o \
	common/mm/comptags.o \
	common/cbc/cbc.o \
	hal/cbc/cbc_gm20b.o \
	hal/cbc/cbc_gp10b.o \
	hal/cbc/cbc_gv11b.o \
	hal/cbc/cbc_tu104.o \

nvgpu-$(CONFIG_NVGPU_REMAP) += \
	os/linux/vm_remap.o \
	common/mm/vm_remap.o

# FUSA (Functionally Safe) HAL source files
nvgpu-y += \
	hal/mm/mm_gv11b_fusa.o \
	hal/mm/mm_gp10b_fusa.o \
	hal/mm/mm_ga10b_fusa.o \
	hal/mm/gmmu/gmmu_gv11b_fusa.o \
	hal/mm/gmmu/gmmu_gp10b_fusa.o \
	hal/mm/gmmu/gmmu_gk20a_fusa.o \
	hal/mm/gmmu/gmmu_gm20b_fusa.o \
	hal/mm/cache/flush_gk20a_fusa.o \
	hal/mm/cache/flush_gv11b_fusa.o \
	hal/mm/mmu_fault/mmu_fault_gv11b_fusa.o \
	hal/ltc/intr/ltc_intr_gp10b_fusa.o \
	hal/ltc/intr/ltc_intr_gv11b_fusa.o \
	hal/bus/bus_gk20a_fusa.o \
	hal/bus/bus_gm20b_fusa.o \
	hal/bus/bus_gp10b_fusa.o \
	hal/bus/bus_gv11b_fusa.o \
	hal/ce/ce_gp10b_fusa.o \
	hal/ce/ce_gv11b_fusa.o \
	hal/class/class_gv11b_fusa.o \
	hal/falcon/falcon_gk20a_fusa.o \
	hal/fb/fb_gm20b_fusa.o \
	hal/fb/fb_gv11b_fusa.o \
	hal/fb/fb_gp10b_fusa.o \
	hal/fb/fb_tu104_fusa.o \
	hal/fb/fb_mmu_fault_gv11b_fusa.o \
	hal/fb/ecc/fb_ecc_gv11b_fusa.o \
	hal/fb/intr/fb_intr_ecc_gv11b_fusa.o \
	hal/fb/intr/fb_intr_gv11b_fusa.o \
	hal/fifo/channel_gk20a_fusa.o \
	hal/fifo/channel_gm20b_fusa.o \
	hal/fifo/channel_gv11b_fusa.o \
	hal/fifo/ctxsw_timeout_gv11b_fusa.o \
	hal/fifo/engine_status_gm20b_fusa.o \
	hal/fifo/engine_status_gv100_fusa.o \
	hal/fifo/engines_gp10b_fusa.o \
	hal/fifo/engines_gv11b_fusa.o \
	hal/fifo/fifo_gk20a_fusa.o \
	hal/fifo/fifo_gv11b_fusa.o \
	hal/fifo/fifo_intr_gk20a_fusa.o \
	hal/fifo/fifo_intr_gv11b_fusa.o \
	hal/fifo/pbdma_gm20b_fusa.o \
	hal/fifo/pbdma_gp10b_fusa.o \
	hal/fifo/pbdma_gv11b_fusa.o \
	hal/fifo/pbdma_status_gm20b_fusa.o \
	hal/fifo/preempt_gv11b_fusa.o \
	hal/fifo/ramfc_gp10b_fusa.o \
	hal/fifo/ramfc_gv11b_fusa.o \
	hal/fifo/ramin_gk20a_fusa.o \
	hal/fifo/ramin_gm20b_fusa.o \
	hal/fifo/ramin_gv11b_fusa.o \
	hal/fifo/runlist_fifo_gk20a_fusa.o \
	hal/fifo/runlist_fifo_gv11b_fusa.o \
	hal/fifo/runlist_ram_gv11b_fusa.o \
	hal/fifo/tsg_gk20a_fusa.o \
	hal/fifo/tsg_gv11b_fusa.o \
	hal/fifo/usermode_gv11b_fusa.o \
	hal/fuse/fuse_gm20b_fusa.o \
	hal/fuse/fuse_gp10b_fusa.o \
	hal/fuse/fuse_gv11b_fusa.o \
	hal/gr/config/gr_config_gm20b_fusa.o \
	hal/gr/config/gr_config_gv100_fusa.o \
	hal/gr/config/gr_config_gv11b_fusa.o \
	hal/gr/config/gr_config_ga10b_fusa.o \
	hal/gr/ctxsw_prog/ctxsw_prog_gm20b_fusa.o \
	hal/gr/ctxsw_prog/ctxsw_prog_gp10b_fusa.o \
	hal/gr/ctxsw_prog/ctxsw_prog_gv11b_fusa.o \
	hal/gr/ecc/ecc_gv11b_fusa.o \
	hal/gr/falcon/gr_falcon_gm20b_fusa.o \
	hal/gr/falcon/gr_falcon_gp10b_fusa.o \
	hal/gr/falcon/gr_falcon_gv11b_fusa.o \
	hal/gr/init/gr_init_gm20b_fusa.o \
	hal/gr/init/gr_init_gp10b_fusa.o \
	hal/gr/init/gr_init_gv11b_fusa.o \
	hal/gr/intr/gr_intr_gm20b_fusa.o \
	hal/gr/intr/gr_intr_gp10b_fusa.o \
	hal/gr/intr/gr_intr_gv11b_fusa.o \
	hal/ltc/ltc_gm20b_fusa.o \
	hal/ltc/ltc_gp10b_fusa.o \
	hal/ltc/ltc_gv11b_fusa.o \
	hal/mc/mc_gm20b_fusa.o \
	hal/mc/mc_gp10b_fusa.o \
	hal/mc/mc_gv11b_fusa.o \
	hal/netlist/netlist_gv11b_fusa.o \
	hal/pmu/pmu_gk20a_fusa.o \
	hal/pmu/pmu_gv11b_fusa.o \
	hal/priv_ring/priv_ring_gm20b_fusa.o \
	hal/priv_ring/priv_ring_gp10b_fusa.o \
	hal/ptimer/ptimer_gk20a_fusa.o \
	hal/ptimer/ptimer_gp10b.o \
	hal/ptimer/ptimer_gv11b.o \
	hal/therm/therm_gv11b_fusa.o \
	hal/top/top_gm20b_fusa.o \
	hal/top/top_gv11b_fusa.o

nvgpu-$(CONFIG_NVGPU_HAL_NON_FUSA) += \
	hal/ce/ce_tu104.o \
	hal/ce/ce_gp10b.o \
	hal/clk/clk_gm20b.o \
	hal/init/hal_gm20b.o \
	hal/init/hal_gm20b_litter.o \
	hal/fifo/engine_status_gm20b.o \
	hal/fifo/engines_gm20b.o \
	hal/fifo/pbdma_gm20b.o \
	hal/fifo/pbdma_gp10b.o \
	hal/fifo/mmu_fault_gk20a.o \
	hal/fifo/mmu_fault_gm20b.o \
	hal/fifo/mmu_fault_gp10b.o \
	hal/fifo/runlist_fifo_gk20a.o \
	hal/fifo/runlist_ram_gk20a.o \
	hal/gr/config/gr_config_gm20b.o \
	hal/gr/ctxsw_prog/ctxsw_prog_gm20b.o \
	hal/gr/ctxsw_prog/ctxsw_prog_gm20b_dbg.o \
	hal/gr/gr/gr_gk20a.o \
	hal/gr/gr/gr_gm20b.o \
	hal/gr/gr/gr_gp10b.o \
	hal/ltc/ltc_gm20b.o \
	hal/ltc/ltc_gm20b_dbg.o \
	hal/mc/mc_gm20b.o \
	hal/mm/mm_gm20b.o \
	hal/mm/mm_gk20a.o \
	hal/mm/gmmu/gmmu_gk20a.o \
	hal/mm/gmmu/gmmu_gm20b.o \
	hal/falcon/falcon_gk20a.o \
	hal/netlist/netlist_gm20b.o \
	hal/perf/perf_gm20b.o \
	hal/power_features/cg/gm20b_gating_reglist.o \
	hal/priv_ring/priv_ring_gm20b.o \
	hal/regops/regops_gm20b.o \
	hal/regops/regops_tu104.o \
	hal/regops/allowlist_tu104.o \
	hal/therm/therm_gm20b.o \
	hal/top/top_gm20b.o \
	hal/mssnvlink/mssnvlink_ga10b.o

ifeq ($(CONFIG_NVGPU_GR_VIRTUALIZATION),y)
nvgpu-$(CONFIG_NVGPU_HAL_NON_FUSA) += \
	hal/vgpu/init/vgpu_hal_gv11b.o
endif

ifeq ($(CONFIG_NVGPU_HAL_NON_FUSA),y)
nvgpu-y += \
	common/falcon/falcon_sw_ga10b.o \
	common/riscv/riscv.o \
	common/acr/acr_sw_ga10b.o \
	common/acr/acr_sw_ga100.o \
	common/pmu/perfmon/pmu_perfmon_sw_ga10b.o \
	common/pmu/pg/pg_sw_ga10b.o

nvgpu-y += \
	hal/init/hal_ga10b.o \
	hal/init/hal_ga10b_litter.o \
	hal/gr/zbc/zbc_ga10b.o \
	hal/class/class_ga10b.o \
	hal/class/class_ga100.o \
	hal/mc/mc_ga10b_fusa.o \
	hal/mc/mc_intr_ga10b_fusa.o \
	hal/mm/mmu_fault/mmu_fault_ga10b_fusa.o \
	hal/mm/gmmu/gmmu_ga10b_fusa.o \
	hal/func/func_ga10b.o \
	hal/fuse/fuse_ga10b.o \
	hal/falcon/falcon_ga10b_fusa.o \
	hal/fifo/usermode_ga10b_fusa.o \
	hal/fifo/fifo_intr_ga10b_fusa.o \
	hal/fifo/ctxsw_timeout_ga10b_fusa.o \
	hal/fifo/fifo_ga10b_fusa.o \
	hal/fifo/pbdma_ga10b_fusa.o \
	hal/fifo/pbdma_ga10b.o \
	hal/fifo/usermode_ga10b_fusa.o \
	hal/fifo/fifo_utils_ga10b_fusa.o \
	hal/fifo/engine_status_ga10b_fusa.o \
	hal/fifo/pbdma_status_ga10b_fusa.o \
	hal/fifo/preempt_ga10b_fusa.o \
	hal/fifo/runlist_fifo_ga10b_fusa.o \
	hal/fifo/runlist_fifo_ga10b.o \
	hal/fifo/tsg_ga10b.o \
	hal/fifo/userd_ga10b.o \
	hal/fifo/ramin_ga10b_fusa.o \
	hal/fifo/ramfc_ga10b_fusa.o \
	hal/fifo/runlist_ga10b_fusa.o \
	hal/gr/ctxsw_prog/ctxsw_prog_ga10b.o \
	hal/gr/ctxsw_prog/ctxsw_prog_ga10b_dbg.o \
	hal/gr/ctxsw_prog/ctxsw_prog_ga10b_fusa.o \
	hal/gr/ctxsw_prog/ctxsw_prog_ga100.o \
	hal/gr/ctxsw_prog/ctxsw_prog_ga100_dbg.o \
	hal/gr/ctxsw_prog/ctxsw_prog_ga100_fusa.o \
	hal/gr/gr/gr_ga10b.o \
	hal/gr/init/gr_init_ga10b.o \
	hal/gr/init/gr_init_ga10b_fusa.o \
	hal/gr/intr/gr_intr_ga10b_fusa.o \
	hal/gr/falcon/gr_falcon_ga10b_fusa.o \
	hal/gr/falcon/gr_falcon_ga10b.o \
	hal/gr/ecc/ecc_ga10b.o \
	hal/gr/ecc/ecc_ga10b_fusa.o \
	hal/netlist/netlist_ga10b_fusa.o \
	hal/fifo/channel_ga10b_fusa.o \
	hal/ltc/ltc_ga10b_fusa.o \
	hal/ltc/ltc_ga10b.o \
	hal/ltc/intr/ltc_intr_ga10b_fusa.o \
	hal/top/top_ga10b_fusa.o \
	hal/bus/bus_ga10b.o \
	hal/pmu/pmu_ga10b.o \
	hal/gsp/gsp_ga10b.o \
	hal/fb/fb_ga100.o \
	hal/fb/fb_ga10b.o \
	hal/fb/fb_ga10b_fusa.o \
	hal/fb/fb_mmu_fault_ga10b_fusa.o \
	hal/fb/ecc/fb_ecc_ga10b_fusa.o \
	hal/fb/intr/fb_intr_ga10b_fusa.o \
	hal/fb/intr/fb_intr_ecc_ga10b_fusa.o \
	hal/fb/vab/vab_ga10b.o \
	hal/priv_ring/priv_ring_ga10b_fusa.o \
	hal/ptimer/ptimer_ga10b_fusa.o \
	hal/perf/perf_ga10b.o \
	hal/regops/regops_ga10b.o \
	hal/regops/allowlist_ga10b.o \
	hal/power_features/cg/ga10b_gating_reglist.o \
	hal/therm/therm_ga10b_fusa.o \
	hal/ce/ce_ga10b_fusa.o \
	hal/grmgr/grmgr_ga10b.o \
	hal/sim/sim_ga10b.o

ifeq ($(CONFIG_NVGPU_COMPRESSION),y)
nvgpu-y += \
	hal/cbc/cbc_ga100.o \
	hal/cbc/cbc_ga10b.o
endif

nvgpu-$(CONFIG_NVGPU_GR_VIRTUALIZATION) += \
	hal/vgpu/init/vgpu_hal_ga10b.o \
	os/linux/vgpu/ga10b/platform_ga10b_vgpu_tegra.o

endif

ifeq ($(CONFIG_NVGPU_DGPU),y)
nvgpu-y += \
	hal/init/hal_ga100.o \
	hal/init/hal_ga100_litter.o \
	hal/gr/gr/gr_ga100.o \
	hal/bus/bus_ga100.o \
	hal/fuse/fuse_ga100.o \
	hal/gr/intr/gr_intr_ga100_fusa.o \
	hal/gr/init/gr_init_ga100_fusa.o \
	hal/gr/init/gr_init_ga100.o \
	hal/gr/falcon/gr_falcon_ga100.o \
	hal/clk/clk_ga100.o \
	hal/nvdec/nvdec_ga100.o \
	hal/pmu/pmu_ga100.o \
	hal/perf/perf_ga100.o \
	hal/fb/fb_ga100.o \
	hal/fifo/channel_ga100_fusa.o \
	hal/fifo/pbdma_ga100_fusa.o \
	hal/fifo/runlist_fifo_ga100_fusa.o \
	hal/netlist/netlist_ga100.o \
	common/vbios/bios_sw_ga100.o \
	hal/power_features/cg/ga100_gating_reglist.o \
	hal/priv_ring/priv_ring_ga100_fusa.o \
	hal/regops/regops_ga100.o \
	hal/regops/allowlist_ga100.o
endif

ifeq ($(CONFIG_NVGPU_MIG),y)
ccflags-y += -DCONFIG_NVGPU_MIG
ifeq ($(CONFIG_NVGPU_DGPU),y)
nvgpu-y += \
	hal/grmgr/grmgr_ga100.o
endif
endif

nvgpu-y += \
	os/linux/platform_ga10b_tegra.o

ifeq ($(CONFIG_NVS_PRESENT),y)
include $(srctree.nvgpu)/nvsched/Makefile.sources
nvgpu-y += $(patsubst %.c,../../../nvsched/%.o,$(NVS_SOURCES))
nvgpu-y += \
	os/linux/ioctl_nvs.o \
	common/nvs/nvs_sched.o
ccflags-y += \
	$(patsubst %,-I$(srctree.nvgpu)/nvsched/%,$(NVS_INCLUDE)) \
	-I$(srctree.nvgpu)/drivers/gpu/nvgpu/include/external-nvs \
	-DNVS_USE_IMPL_TYPES
endif
