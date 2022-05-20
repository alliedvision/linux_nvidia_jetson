/*
 * Copyright (c) 2017-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_PMU_H
#define NVGPU_PMU_H

#include <nvgpu/kmem.h>
#include <nvgpu/allocator.h>
#include <nvgpu/lock.h>
#include <nvgpu/nvgpu_common.h>
#include <nvgpu/nvgpu_mem.h>
#include <nvgpu/flcnif_cmn.h>
#include <nvgpu/falcon.h>
#include <nvgpu/timers.h>
#ifdef CONFIG_NVGPU_LS_PMU
#include <nvgpu/pmu/queue.h>
#include <nvgpu/pmu/msg.h>
#include <nvgpu/pmu/fw.h>
#include <nvgpu/pmu/volt.h>

struct pmu_sequences;
struct pmu_mutexes;
struct nvgpu_pmu_lsfm;
struct nvgpu_pmu_super_surface;
struct nvgpu_pmu_pg;
struct nvgpu_pmu_perfmon;
struct nvgpu_clk_pmupstate;
#endif

/**
 * @file
 * @page unit-PMU Unit PMU(Programmable Management Unit)
 *
 * Acronyms
 * ========
 * PMU    - Programmable Management Unit
 * ACR    - Access Controlled Regions
 * FALCON - Fast Logic Controller
 * RTOS   - Real time operating system
 * NS     - Non Secure
 * LS     - Light Secure
 * HS     - Heavy Secure
 * VM     - virtual memory
 *
 * Overview
 * ========
 *
 * The PMU unit is responsible for managing the PMU Engine on the GPU & the PMU
 * RTOS ucode. The PMU unit helps to load different ucode's(iGPU-ACR & PMU RTOS)
 * onto PMU Engine Falcon at different stages of GPU boot process. Once PMU RTOS
 * is up then PMU Engine h/w is controlled by both PMU unit as well as PMU RTOS
 * executing on PMU Engine Flacon. In the absence of PMU RTOS support only PMU
 * unit controls PMU Engine by supporting below listed h/w functionalities in
 * the PMU Engine h/w management section.
 *
 * Below are the two features supported by the PMU unit
 *
 * - The PMU Engine h/w.
 * - The PMU RTOS.
 *
 * The PMU Engine h/w management
 * -----------------------------
 * The PMU unit is responsible for below PMU Engine h/w related functionalities:
 *   - Reset & enable PMU Engine to bring into good known state.
 *   - PMU Engine clock gating support configuration.
 *   - PMU Engine interrupt configuration & handling.
 *   - PMU Engine H/W error detection, handling & reporting to 3LSS.
 *   - Functions to control supported h/w functionalities.
 *
 * The PMU RTOS management
 * -----------------------
 * The PMU unit is responsible for loading PMU RTOS onto PMU Engine Falcon and
 * bootstrap. In non-secure mode(NS PMU RTOS), load & bootstrap is handled by
 * PMU unit. In secure mode(LS PMU RTOS), signature verification followed by
 * loading of ucode onto PMU Engine falcon is handled by HS ACR ucode and
 * bootstrap is done by PMU unit. Upon PMU RTOS bootup, PMU RTOS sends init
 * message to PMU unit to ACK PMU RTOS is up & ready to accept inputs from PMU
 * unit. PMU unit decodes init message to establish communication with PMU RTOS
 * by creating command & message queues as per init message parameters. Upon
 * successful communication establishment, its PMU sub-unit responsible to
 * communicate with PMU RTOS to get its tasks executed as required.
 *
 * Data Structures
 * ===============
 *
 * The major data structures exposed by PMU unit in nvgpu is:
 *
 *   + struct nvgpu_pmu
 *       The data struct holds PMU Engine h/w properties, PMU RTOS supporting
 *       data structs, sub-unit's data structs & ops of the PMU unit which will
 *       be populated based on the detected chip.
 *
 * Static Design
 * =============
 *
 * PMU Initialization
 * ------------------
 *
 * PMU unit initialization happens as part of early NVGPU poweron sequence by
 * calling nvgpu_pmu_early_init(). At PMU init stage memory gets allocated for
 * PMU unit's data struct #struct nvgpu_pmu. The data struct holds PMU Engine
 * h/w properties, PMU RTOS supporting data structs, sub-unit's data structs &
 * ops of the PMU unit which will be populated based on the detected chip.
 *
 * PMU Teardown
 * ------------
 *
 * The function nvgpu_pmu_remove_support() is called from nvgpu_remove() as part
 * of poweroff sequence to clear and free the memory space allocated for PMU
 * unit.
 *
 * External APIs
 * -------------
 *   + nvgpu_pmu_early_init()
 *   + nvgpu_pmu_remove_support()
 *
 * Dynamic Design
 * ==============
 *
 * Operation listed based on features of PMU unit:
 *
 * PMU Engine h/w
 * --------------
 *   + PMU Engine reset:
 *     Engine reset is required before loading any ucode onto
 *     PMU Engine Falcon. The reset sequence also configures PMU Engine clock
 *     gating & interrupts if interrupt support is enabled.
 *   + Error detection & reporting:
 *     Different types of PMU BAR0 error will be detected & reported to 3LSS
 *     during ucode load & execution of ucode on PMU Engine Falcon.
 *
 * PMU RTOS
 * --------
 * Loading & bootstrap of PMU RTOS ucode on to PMU Engine Falcon differs based
 * on secure mode.
 *    + NS PMU RTOS: load & bootstrap is handled by PMU unit. PMU RTOS ucode
 *      will be loaded onto IMEM/DMEM & trigger execution.
 *    + LS PMU RTOS: signature verification followed by loading of ucode onto
 *      PMU Engine falcon is handled by HS ACR ucode and bootstrap is done by
 *      PMU unit.
 *
 * Upon successful PMU RTOS load & bootstrap, PMU subunits related to PMU RTOS
 * will be initialized by sending subunit specific commands to PMU RTOS. Next,
 * PMU subunit waits to receive ACK to confirm init is done. Once subunit init
 * is successful then commands and message will be exchanged between PMU UNIT
 * and PMU RTOS to get subunit specific operation executed.
 *
 * External APIs
 * -------------
 *   + nvgpu_pmu_reset()
 *   + nvgpu_pmu_report_bar0_pri_err_status()
 *   + nvgpu_pmu_rtos_init()
 *   + nvgpu_pmu_destroy()
 *
 */

/**
 *  The PMU unit debugging macro
 */
#define nvgpu_pmu_dbg(g, fmt, args...) \
	nvgpu_log(g, gpu_dbg_pmu, fmt, ##args)

/**
 *  The PMU unit system memory VM space
 */
#define GK20A_PMU_VA_SIZE		(512U * 1024U * 1024U)

/**
 * The PMU's frame-buffer interface block has several slots/indices which can
 * be bound to support DMA to various surfaces in memory. These defines give
 * name to each index based on type of memory-aperture the index is used to
 * access.
 */
#define	GK20A_PMU_DMAIDX_UCODE		U32(0)
#define	GK20A_PMU_DMAIDX_VIRT		U32(1)
#define	GK20A_PMU_DMAIDX_PHYS_VID	U32(2)
#define	GK20A_PMU_DMAIDX_PHYS_SYS_COH	U32(3)
#define	GK20A_PMU_DMAIDX_PHYS_SYS_NCOH	U32(4)
#define	GK20A_PMU_DMAIDX_RSVD		U32(5)
#define	GK20A_PMU_DMAIDX_PELPG		U32(6)
#define	GK20A_PMU_DMAIDX_END		U32(7)

/**
 * This assigns an unique index for errors in PMU unit.
 */
#define	PMU_BAR0_SUCCESS		0U
#define	PMU_BAR0_HOST_READ_TOUT		1U
#define	PMU_BAR0_HOST_WRITE_TOUT	2U
#define	PMU_BAR0_FECS_READ_TOUT		3U
#define	PMU_BAR0_FECS_WRITE_TOUT	4U
#define	PMU_BAR0_CMD_READ_HWERR		5U
#define	PMU_BAR0_CMD_WRITE_HWERR	6U
#define	PMU_BAR0_READ_HOSTERR		7U
#define	PMU_BAR0_WRITE_HOSTERR		8U
#define	PMU_BAR0_READ_FECSERR		9U
#define	PMU_BAR0_WRITE_FECSERR		10U

#ifdef CONFIG_NVGPU_LS_PMU
struct rpc_handler_payload {
	void *rpc_buff;
	bool is_mem_free_set;
	bool complete;
};

struct pmu_rpc_desc {
	void   *prpc;
	u16 size_rpc;
	u16 size_scratch;
};

struct pmu_in_out_payload_desc {
	void *buf;
	u32 offset;
	u32 size;
	u32 fb_size;
};

struct pmu_payload {
	struct pmu_in_out_payload_desc in;
	struct pmu_in_out_payload_desc out;
	struct pmu_rpc_desc rpc;
};

#define PMU_UCODE_NB_MAX_OVERLAY	64U
#define PMU_UCODE_NB_MAX_DATE_LENGTH  64U

struct pmu_ucode_desc {
	u32 descriptor_size;
	u32 image_size;
	u32 tools_version;
	u32 app_version;
	char date[PMU_UCODE_NB_MAX_DATE_LENGTH];
	u32 bootloader_start_offset;
	u32 bootloader_size;
	u32 bootloader_imem_offset;
	u32 bootloader_entry_point;
	u32 app_start_offset;
	u32 app_size;
	u32 app_imem_offset;
	u32 app_imem_entry;
	u32 app_dmem_offset;
	/* Offset from appStartOffset */
	u32 app_resident_code_offset;
	/* Exact size of the resident code
	 * ( potentially contains CRC inside at the end )
	 */
	u32 app_resident_code_size;
	/* Offset from appStartOffset */
	u32 app_resident_data_offset;
	/* Exact size of the resident code
	 * ( potentially contains CRC inside at the end )
	 */
	u32 app_resident_data_size;
	u32 nb_overlays;
	struct {u32 start; u32 size; } load_ovl[PMU_UCODE_NB_MAX_OVERLAY];
	u32 compressed;
};

/*
 * nvgpu-next PMU ucode built with below new ucode descriptor, so use
 * below descriptor to read nvgpu-next PMU ucode details from PMU desc
 * bin.
 */
struct pmu_ucode_desc_v1 {
	u32 descriptor_size;
	u32 image_size;
	u32 tools_version;
	u32 app_version;
	char date[PMU_UCODE_NB_MAX_DATE_LENGTH];
	u32 secure_bootloader;
	u32 bootloader_start_offset;
	u32 bootloader_size;
	u32 bootloader_imem_offset;
	u32 bootloader_entry_point;
	u32 app_start_offset;
	u32 app_size;
	u32 app_imem_offset;
	u32 app_imem_entry;
	u32 app_dmem_offset;
	/* Offset from appStartOffset */
	u32 app_resident_code_offset;
	/* Exact size of the resident code
	 * ( potentially contains CRC inside at the end )
	 */
	u32 app_resident_code_size;
	/* Offset from appStartOffset */
	u32 app_resident_data_offset;
	/* Exact size of the resident code
	 * ( potentially contains CRC inside at the end )
	 */
	u32 app_resident_data_size;
	u32 nb_overlays;
	struct {u32 start; u32 size; } load_ovl[PMU_UCODE_NB_MAX_OVERLAY];
};

/*
 * configuration for bootloader
 */
struct nv_next_core_bootldr_params {
	/*
	 *                   *** warning ***
	 * first 3 fields must be frozen like that always. should never
	 * be reordered or changed.
	 */
	/* set to 'nvrm' if booting from rm. */
	u32 boot_type;
	/* size of boot params.*/
	u16 size;
	/* version of boot params. */
	u8  version;
	/*
	 * you can reorder or change below this point but update version.
	 */
};

/*
 * Version of bootloader struct, increment on struct changes (while on prod).
 */
/* Macro to build and u32 from four bytes, listed from msb to lsb */
#define U32_BUILD(a, b, c, d) (((a) << 24U) | ((b) << 16U) | ((c) << 8U) | (d))

#define NV_NEXT_CORE_BOOTLDR_VERSION            1U
#define NV_NEXT_CORE_BOOTLDR_BOOT_TYPE_UNKNOWN  0U
#define NV_NEXT_CORE_BOOTLDR_BOOT_TYPE_RM       U32_BUILD('N', 'V', 'R', 'M')

#define NV_REG_STR_NEXT_CORE_DUMP_SIZE_DEFAULT  8192U

#define NV_NEXT_CORE_AMAP_EXTMEM2_START		0x8060000000000000ull

/*
 * configuration for rtos
 */
struct nv_next_core_rtos_params {
	/* address (next-core pa) of ucode core dump buffer */
	u64 core_dump_phys;
	/* size of ucode core dump buffer */
	u32 core_dump_size;
};

struct nv_next_core_boot_params {
	struct nv_next_core_bootldr_params bl;
	/* rtos specific configuration should be added here. */
	struct nv_next_core_rtos_params rtos;
	u32 dummy[24];
};

struct nv_pmu_boot_params {
	struct nv_next_core_boot_params boot_params;
	struct pmu_cmdline_args_v7 cmd_line_args;
};

struct falcon_next_core_ucode_desc {
	u32 version;
	u32 bootloader_offset;
	u32 bootloader_size;
	u32 bootloader_param_offset;
	u32 bootloader_param_size;
	u32 next_core_elf_offset;
	u32 next_core_elf_size;
	u32 app_version;
	/* manifest contains information about monitor and it is input to br */
	u32 manifest_offset;
	u32 manifest_size;
	/* monitor data offset within next_core image and size */
	u32 monitor_data_offset;
	u32 monitor_data_size;
	/* monitor code offset withtin next_core image and size */
	u32 monitor_code_offset;
	u32 monitor_code_size;
	bool is_monitor_enabled;
};
#endif

/**
 * This data struct holds PMU Engine h/w properties, PMU RTOS supporting data
 * structs, sub-unit's data structs & ops of the PMU unit which will be
 * populated based on the detected chip.
 */
struct nvgpu_pmu {
	struct gk20a *g;
	bool sw_ready;
	bool isr_enabled;
	struct nvgpu_mutex isr_mutex;
	struct nvgpu_falcon *flcn;
#ifdef CONFIG_NVGPU_LS_PMU
	struct nvgpu_allocator dmem;
	struct nvgpu_mem trace_buf;
	struct pmu_sha1_gid gid_info;

	struct pmu_rtos_fw *fw;
	struct pmu_queues queues;
	struct pmu_sequences *sequences;
	struct pmu_mutexes *mutexes;

	struct nvgpu_pmu_lsfm *lsfm;
	struct nvgpu_pmu_super_surface *super_surface;
	struct nvgpu_pmu_pg *pg;
	struct nvgpu_pmu_perfmon *pmu_perfmon;
	struct nvgpu_clk_pmupstate *clk_pmu;
	struct nvgpu_pmu_perf *perf_pmu;
	struct nvgpu_pmu_therm *therm_pmu;
	struct nvgpu_pmu_volt *volt;

	void (*remove_support)(struct nvgpu_pmu *pmu);
	void (*therm_rpc_handler)(struct gk20a *g, struct nvgpu_pmu *pmu,
			struct nv_pmu_rpc_header *rpc);
#endif
};

#ifdef CONFIG_NVGPU_LS_PMU
/*!
 * Structure/object which single register write need to be done during PG init
 * sequence to set PROD values.
 */
struct pg_init_sequence_list {
	u32 regaddr;
	u32 writeval;
};

/* PMU locks used along with PMU-RTOS */
int nvgpu_pmu_lock_acquire(struct gk20a *g, struct nvgpu_pmu *pmu,
	u32 id, u32 *token);
int nvgpu_pmu_lock_release(struct gk20a *g, struct nvgpu_pmu *pmu,
	u32 id, u32 *token);

/* PMU RTOS init/setup functions*/
int nvgpu_pmu_rtos_early_init(struct gk20a *g, struct nvgpu_pmu *pmu);
int nvgpu_pmu_rtos_init(struct gk20a *g);
int nvgpu_pmu_destroy(struct gk20a *g, struct nvgpu_pmu *pmu);

void nvgpu_pmu_rtos_cmdline_args_init(struct gk20a *g, struct nvgpu_pmu *pmu);
#if defined(CONFIG_NVGPU_HAL_NON_FUSA)
void nvgpu_pmu_next_core_rtos_args_setup(struct gk20a *g,
		struct nvgpu_pmu *pmu);
s32 nvgpu_pmu_next_core_rtos_args_allocate(struct gk20a *g,
		struct nvgpu_pmu *pmu);
#endif
#endif

/**
 * @brief Report PMU BAR0 error to 3LSS.
 *
 * @param g           [in] The GPU driver struct.
 * @param bar0_status [in] bar0 error status value.
 * @param err_type    [in] Error type.
 *
 * This function reports PMU BAR0 error to 3LSS.
 *
 */
void nvgpu_pmu_report_bar0_pri_err_status(struct gk20a *g, u32 bar0_status,
	u32 error_type);

/**
 * @brief Enable/Disable PMU ECC interrupt.
 *
 * @param g		[in] The GPU driver struct.
 * @param enable	[in] boolean parameter to enable/disable.
 *
 * Enable/Disable PMU ECC interrupt.
 *  + Check that g->pmu and g->ops.pmu.pmu_enable_irq are not null.
 *  + Acquire the mutex g->pmu->isr_mutex.
 *  + Disable the PMU interrupts at MC and PMU level.
 *  + If enabling, enable ECC interrupt in PMU interrupt configuration
 *    registers and enable PMU interrupts at MC level.
 *  + Release the mutex g->pmu->isr_mutex.
 */
void nvgpu_pmu_enable_irq(struct gk20a *g, bool enable);

/**
 * @brief Reset the PMU Engine.
 *
 * @param g   [in] The GPU driver struct.
 *
 * Does the PMU Engine reset to bring into good known state. The reset sequence
 * also configures PMU Engine clock gating & interrupts if interrupt support is
 * enabled.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ETIMEDOUT if PMU engine reset times out.
 */
int nvgpu_pmu_reset(struct gk20a *g);

/**
 * @brief PMU early initialization to allocate memory for PMU unit & set PMU
 *        Engine h/w properties, PMU RTOS supporting data structs, sub-unit's
 *        data structs & ops of the PMU unit by populating data based on the
 *        detected chip,
 *
 * @param g         [in] The GPU driver struct.
 *
 * Initializes PMU unit data struct in the GPU driver based on detected chip.
 * Allocate memory for #nvgpu_pmu data struct & set PMU Engine h/w properties,
 * PMU RTOS supporting data structs & ops of the PMU unit by populating data
 * based on the detected chip. Allocates memory for ECC counters for PMU
 * unit. Initializes the isr_mutex.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ENOMEM if memory allocation for struct #nvgpu_pmu fails.
 */
int nvgpu_pmu_early_init(struct gk20a *g);

/**
 * @brief PMU remove to free space allocted for PMU unit
 *
 * @param g [in] The GPU
 * @param nvgpu_pmu [in] The PMU unit.
 *
 */
void nvgpu_pmu_remove_support(struct gk20a *g, struct nvgpu_pmu *pmu);

/*
 * @brief Allocate and initialize ECC counter for memories within PMU.
 *
 * @param stat [in] Address of pointer to struct nvgpu_ecc_stat.
 *
 */
#define NVGPU_ECC_COUNTER_INIT_PMU(stat) \
	nvgpu_ecc_counter_init(g, &g->ecc.pmu.stat, #stat)

/*
 * @brief Remove ECC counter from the list and free the counter.
 *
 * @param stat [in] Address of pointer to struct nvgpu_ecc_stat.
 *
 */
#define NVGPU_ECC_COUNTER_FREE_PMU(stat) \
	nvgpu_ecc_counter_deinit(g, &g->ecc.pmu.stat)

#endif /* NVGPU_PMU_H */

