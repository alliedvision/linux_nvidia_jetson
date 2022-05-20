/*
 * Copyright (c) 2016-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_ACR_H
#define NVGPU_ACR_H

/**
 * @file
 * @page unit-acr Unit ACR(Access Controlled Regions)
 *
 * Acronyms
 * ========
 * + ACR     - Access Controlled Regions
 * + ACR HS  - Access Controlled Regions Heavy-Secure ucode
 * + FB      - Frame Buffer
 * + non-WPR - non-Write Protected Region
 * + WPR     - Write Protected Region
 * + LS      - Light-Secure
 * + HS      - Heavy-Secure
 * + Falcon  - Fast Logic CONtroller
 * + BLOB    - Binary Large OBject
 *
 * Overview
 * ========
 * The ACR unit is responsible for GPU secure boot. ACR unit divides its task
 * into two stages as below,
 *
 * + Blob construct:
 *   ACR unit creates LS ucode blob in system/FB's non-WPR memory. LS ucodes
 *   will be read from filesystem and added to blob as per ACR unit static
 *   config data. ACR unit static config data is set based on current chip.
 *   LS ucodes blob is required by the ACR HS ucode to authenticate & load LS
 *   ucode on to respective engine's LS Falcon.
 *
 * + ACR HS ucode load & bootstrap:
 *   ACR HS ucode is responsible for authenticating self(HS) & LS ucode.
 *
 *   ACR HS ucode is read from the filesystem based on the chip-id by the ACR
 *   unit. Read ACR HS ucode is loaded onto PMU/SEC2/GSP engines Falcon to
 *   bootstrap ACR HS ucode. ACR HS ucode does self-authentication using H/W
 *   based HS authentication methodology. Once authenticated the ACR HS ucode
 *   starts executing on the falcon.
 *
 *   Upon successful ACR HS ucode boot, ACR HS ucode performs a sanity check on
 *   WPR memory. If the WPR sanity check passes, then ACR HS ucode copies LS
 *   ucodes from system/FB's non-WPR memory to system/FB's WPR memory. The
 *   purpose of copying LS ucode to WPR memory is to protect ucodes from
 *   modification or tampering. The next step is to authenticate LS ucodes
 *   present in WPR memory using S/W based authentication methodology. If the
 *   LS ucode authentication passed, then ACR HS ucode loads LS ucode on to
 *   respective LS Falcons. If any of the LS ucode authentications fail, then
 *   ACR HS ucode updates error details in Falcon mailbox-0/1 & halts its
 *   execution. In the passing case, ACR HS ucode halts & updates mailbox-0 with
 *   ACR_OK(0x0) status.
 *
 *   ACR unit waits for ACR HS ucode to halt & checks for mailbox-0/1 to
 *   determine the status of ACR HS ucode. If there was an error then ACR unit
 *   returns an error else success.
 *
 * The ACR unit is a s/w unit which doesn't access any h/w registers by itself.
 * It depends on below units to access H/W resource to complete its task.
 *
 *   + PMU, SEC2 & GSP unit to access & load ucode on Engines Falcon.
 *   + Falcon unit to control/access Engines(PMU, SEC2 & GSP) Falcon to load &
 *     execute HS ucode
 *   + MM unit to fetch non-WPR/WPR info, allocate & read/write data in
 *     non-WPR memory.
 *
 * Data Structures
 * ===============
 *
 * There are no data structures exposed outside of ACR unit in nvgpu.
 *
 * Static Design
 * =============
 *
 * ACR Initialization
 * ------------------
 * ACR initialization happens as part of early NVGPU poweron sequence by calling
 * nvgpu_acr_init(). At ACR init stage memory gets allocated for ACR unit's
 * private data struct. The data struct holds static properties and ops of the
 * ACR unit and is populated based on the detected chip. These static properties
 * and ops will be used by blob-construct and load/bootstrap stage of ACR unit.
 *
 * ACR Teardown
 * ------------
 * The function nvgpu_acr_free() is called from #nvgpu_remove() as part of
 * poweroff sequence to clear and free the memory space allocated for ACR unit.
 *
 * External APIs
 * -------------
 *   + nvgpu_acr_init()
 *   + nvgpu_acr_free()
 *
 * Dynamic Design
 * ==============
 *
 * After ACR unit init completion, the properties and ops of the ACR unit are
 * set to perform blob construction in non-wpr memory & load/bootstrap of HS ACR
 * ucode on specific engine's Falcon.
 *
 * Blob construct
 * --------------
 * The ACR unit creates blob for LS ucodes in nop-WPR memory & update
 * WPR/LS-ucode details in interface which is part of non-wpr region. Interface
 * will be accessed by ACR HS ucode to know in detail about WPR & LS ucodes.
 *
 * Load/Bootstrap ACR HS ucode
 * ---------------------------
 * The ACR unit loads ACR HS ucode onto PMU/SEC2/GSP engines Falcon as per
 * static config data & performs a bootstrap.
 *
 * ACR HS ucode does self-authentication using H/W based HS authentication
 * methodology. Once authenticated the ACR HS ucode starts executing on the
 * falcon. Upon successful ACR HS ucode boot, ACR HS ucode copies LS ucodes
 * from non-WPR memory to WPR memory. The next step is to authenticate LS ucodes
 * present in WPR memory and loads LS ucode on to respective LS Falcons.
 *
 * The ACR unit waits for ACR HS to halt within predefined timeout. Upon ACR HS
 * ucode halt, the ACR unit checks mailbox-0/1 to determine the status of ACR
 * HS ucode. If there is an error then ACR unit returns error else success.
 *
 * External APIs
 * -------------
 *   + nvgpu_acr_construct_execute()
 *   + nvgpu_acr_is_lsf_lazy_bootstrap()
 *   + nvgpu_acr_bootstrap_hs_acr()
 *
 */

struct gk20a;
struct nvgpu_falcon;
struct nvgpu_firmware;
struct nvgpu_acr;

/**
 * This assigns an unique index for errors in ACR unit.
 */
#define	ACR_BOOT_TIMEDOUT		11U
#define	ACR_BOOT_FAILED			12U

/**
 * @brief The ACR is responsible for GPU secure boot. For this, it needs
 *        to allocate memory and set static properties and ops for LS
 *        ucode blob construction as well as for ACR HS ucode bootstrap.
 *        This function allocates the needed memory and sets the static
 *        properties and ops.
 *
 * @param g   [in] The GPU driver struct.
 *
 * Initializes ACR unit private data struct in the GPU driver based on current
 * chip. Allocate memory for #nvgpu_acr data struct & sets the static properties
 * and ops for LS ucode blob construction as well as for ACR HS ucode bootstrap.
 *
 * ACR init by following below steps,
 * + Allocate memory for ACR unit private struct #nvgpu_acr, return -ENOMEM upon
 *   failure else continue to next step.
 * + Based on detected chip, init calls chip specific s/w init. For gv11b,
 *   nvgpu_gv11b_acr_sw_init is called to set static properties like
 *   bootstrap_owner, supported LS Falcons & ops update.
 *   + Struct #nvgpu_acr member gets set as below for gv11b,
 *     + FALCON_ID_PMU for bootstrap_owner as ACR HS ucode runs on PMU engine
 *       Falcon.
 *     + Supported LS Falcon's FECS & GPCCS details to struct #acr_lsf_config
 *       & lsf_enable_mask member, lsf_enable_mask member used during blob
 *       creation to add LS Falcon ucode details to blob.
 *       + For FECS function #gv11b_acr_lsf_fecs is called.
 *       + For GPCCS function #gv11b_acr_lsf_gpccs is called.
 *     + Struct #hs_acr members get set as below for ACR HS ucode bootstrap by
 *       calling function #nvgpu_gv11b_acr_sw_init.
 *       + Set PMU engine Falcon for acr_flcn member.
 *       + acr_ucode.bin file as ACR HS ucode to load & bootstrap.
 *       + Set PMU Engine Falcon error handler to handle error caused during
 *         ACR HS execution on PMU Engine Falcon
 *         + Function #nvgpu_pmu_report_bar0_pri_err_status to report to 3LSS
 *         + Function #gv11b_pmu_bar0_error_status to know PMU bus error
 *         + Function #gv11b_pmu_validate_mem_integrity to know IMEM/DMEM error
 *       + Set ops as below for gv11b,
 *         + For blob creation set #prepare_ucode_blob to point to
 *           #nvgpu_acr_prepare_ucode_blob function.
 *         + For blob allocation in system memory set #alloc_blob_space to point
 *           to #nvgpu_acr_alloc_blob_space_sys function.
 *         + To patch ACR HS signature as part of ACR HS ucode set
 *           #patch_wpr_info_to_ucode to point to
 *			 #gv11b_acr_patch_wpr_info_to_ucode function.
 *         + For bootstrap set #bootstrap_hs_acr to point to
 *           #gv11b_bootstrap_hs_acr function.
 * + Return 0 upon successful else error if any of above step fails.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ENOMEM if memory allocation for struct #nvgpu_acr fails.
 * @retval -EINVAL if GPU id is invalid.
 */
int nvgpu_acr_init(struct gk20a *g);

#ifdef CONFIG_NVGPU_DGPU
int nvgpu_acr_alloc_blob_prerequisite(struct gk20a *g, struct nvgpu_acr *acr,
	size_t size);
#endif

/**
 * @brief After ACR init which allocates and sets required properties of ACR,
 *        blob of LS ucode(s) are to be constructed in non-wpr memory. After
 *        blob construction HS ACR ucode is to be loaded and then bootstrapped
 *        on specified engine Falcon for GPU secure boot. This function is
 *        responsible for blob construct and loading and bootstrapping ACR
 *        ucode.
 *
 * @param g   [in] The GPU driver struct.
 *
 * Construct blob of LS ucode in non-wpr memory. Allocation happens in non-WPR
 * system/FB memory based on type of GPU iGPU/dGPU currently in execution. Next,
 * ACR unit loads & bootstrap ACR HS ucode on specified engine Falcon.
 *
 * This function is responsible for GPU secure boot. This function divides its
 * task into two stages as below.
 *
 * + Blob construct:
 *   ACR unit creates LS ucode blob in system/FB's non-WPR memory. LS ucodes
 *   will be read from filesystem and added to blob as per ACR unit static
 *   config data. ACR unit static config data is set based on current chip.
 *   LS ucodes blob is required by the ACR HS ucode to authenticate & load LS
 *   ucode on to respective engine's LS Falcon.
 *
 *   ACR blob construct task does below steps to construct LS blob using struct
 *   #ls_flcn_mgr.
 *   + Calls GR unit's #nvgpu_gr_falcon_init_ctxsw_ucode() to read ctxsw ucode
 *     (FECS & GPCCS LS Falcon ucode) & copy to ctxsw ucode surface.
 *     + Reads fecs.bin & gpccs.bin firmware file from the filesystem.
 *       Upon failure returns error to the NvGPU ACR unit.
 *     + Sanity check firmware image descriptor data like size, offsets of DMEM/
 *       IMEM & boot vector. Upon Sanity check failure returns error to the
 *       NvGPU ACR unit.
 *   + Discover supported LS Falcon ucodes for the detected chip. Keep track of
 *     supported LS Falcon ucodes using struct #lsfm_managed_ucode_img linked
 *     list.
 *     + Parse LS-Falcon static configuration data struct #acr_lsf_config to
 *       know supported LS Falcon for the detected chip.struct #acr_lsf_config
 *       is a member of struct #nvgpu_acr & set during ACR init stage.
 *     + Supported LS Falcon ucodes details are filled to struct #flcn_ucode_img
 *       members mentioned below, struct #flcn_ucode_img is a member of struct
 *       #lsfm_managed_ucode_img
 *       + struct #ls_falcon_ucode_desc to hold code details
 *       + struct #lsf_ucode_desc to hold signature, version, other ucode
 *         related data.
 *     + FECS ucode read & ucode details update in struct #flcn_ucode_img by
 *       calling #nvgpu_acr_lsf_fecs_ucode_details().
 *       + Read file fecs_sig.bin file from file system & fill struct
 *         #lsf_ucode_desc, if read fails return error to exit boot sequence.
 *       + Fetch FECS ucode details from ctxsw ucode surface to fill struct
 *         struct #ls_falcon_ucode_desc
 *     + GPCCS ucode read & ucode details update in struct #flcn_ucode_img
 *       by calling #nvgpu_acr_lsf_gpccs_ucode_details()
 *       + Read file gpccs_sig.bin file from file system & fill struct
 *         #lsf_ucode_desc, if read fails return error to exit boot sequence.
 *       + Fetch GPCCS ucode details from ctxsw ucode surface to fill struct
 *         #ls_falcon_ucode_desc.
 *     + Supported LS Falcon ucodes details will be added to the linked list
 *       struct #lsfm_managed_ucode_img & its members. Each LS Falcon ucode
 *       details will be updated to the below struct.
 *       + Light Secure WPR Header struct #lsf_wpr_header, defines state
 *         allowing Light Secure Falcon bootstrapping
 *       + Light Secure Bootstrap Header struct #lsf_lsb_header, defines state
 *         allowing Light Secure Falcon bootstrapping
 *   + Parse the supported LS Falcon details by going through struct
 *     #lsfm_managed_ucode_img linked list & generate WPR requirements for
 *     ACR memory allocation request.
 *   + Allocate memory of size as calculated from previous step to hold ucode
 *     blob contents by calling function #nvgpu_acr_alloc_blob_space_sys().
 *     This allocated space called NON-WPR blob.
 *   + Parse the supported LS Falcon details by going through struct
 *     #lsfm_managed_ucode_img linked list & copy ucode to blob. Copy WPR
 *     header, LSB header, bootloader args & ucode image to NON-WPR blob.
 *     LS Falcon ucode details needs to be added in below format to NON-WPR
 *     blob.
 *     + Copy WPR header struct #lsf_wpr_header.
 *     + Copy LSB header struct #lsf_lsb_header.
 *     + Copy Boot loader struct #flcn_bl_dmem_desc.
 *     + Copy ucode image.
 *     + Tag the terminator WPR header with an invalid falcon ID
 *    + Return 0 upon successful blob construction else error to exit boot
 *      sequence if any of the above steps fails.
 *
 * + ACR HS ucode load & bootstrap:
 *   ACR HS ucode is responsible for authenticating self(HS) & LS ucode.
 *
 *   ACR HS ucode is read from the filesystem based on the chip-id by the ACR
 *   unit. Read ACR HS ucode is loaded onto PMU/SEC2/GSP engines Falcon to
 *   bootstrap ACR HS ucode. ACR HS ucode does self-authentication using H/W
 *   based HS authentication methodology. Once authenticated the ACR HS ucode
 *   starts executing on the falcon.
 *
 *   On GV11B, ACR HS ucode load & execution happens on PMU Engine Falcon.
 *
 *   ACR bootstrap task does below steps,
 *   + Reads ACR HS firmware from file system for the detected chip.
 *   + Sanity check firmware image descriptor data like size, offsets of DMEM/
 *     IMEM & boot vector. If error exit boot sequence.
 *   + Patch ucode signature to the ACR HS ucode at patch_loc of struct
 *     #acr_fw_header.
 *   + Load ACR HS ucode on to respective Engine Falcon by calling
 *     #nvgpu_falcon_hs_ucode_load_bootstrap() function, this function performs
 *     below steps to load & bootstrap.
 *     + Reset the falcon
 *     + setup falcon apertures & boot-config.
 *     + Copy non-secure/secure code to IMEM and data to DMEM
 *     + Write non-zero value to mailbox register which is updated by HS bin to
 *       denote its return status
 *     + Bootstrap falcon
 *   + NvGPU ACR waits for Falcon halt & check the mailbox-0 to know the ACR HS
 *     ucode status
 *     + Waits for Falcon halt by calling #nvgpu_falcon_wait_for_halt() function
 *     + Upon Falcon halt or Falcon halt timeout, NvGPU ACR checks for bus error
 *       & validate the integrity of falcon IMEM and DMEM.
 *     + Reports to 3LSS & return error if any error seen from ACR HS ucode,
 *       integrity of falcon IMEM/DMEM error or bus error.
 *     + Return 0 upon Falcon halt with ACR_OK status.
 *   + Return 0 upon ACR_OK status else error to exit the boot sequence.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -EINVAL if struct #nvgpu_acr is not allocated.
 * @retval -ENOENT if GR/ACR related ucode read fails.
 * @retval -ENOMEM if memory allocation fails for descriptor/blob.
 * @retval -EAGAIN if HS ACR ucode bootstrap fails.
 */
int nvgpu_acr_construct_execute(struct gk20a *g);

/**
 * @brief After LS ucode blob is created, HS ACR ucode needs to be
 *        loaded and bootstrapped on Engine's Falcon.
 *        This function reads HS ACR ucode from filesystem and patches
 *        required HS signature to load on to specified engine falcon
 *        to bootstrap the HS ACR ucode.
 *
 * @param g   [in] The GPU driver struct.
 * @param acr [in] The ACR private data struct
 *
 * + ACR HS ucode load & bootstrap:
 *   ACR HS ucode is responsible for authenticating self(HS) & LS ucode.
 *
 *   ACR HS ucode is read from the filesystem based on the chip-id by the ACR
 *   unit. Read ACR HS ucode is loaded onto PMU/SEC2/GSP engines Falcon to
 *   bootstrap ACR HS ucode. ACR HS ucode does self-authentication using H/W
 *   based HS authentication methodology. Once authenticated the ACR HS ucode
 *   starts executing on the falcon.
 *
 *   ACR bootstrap task does below steps,
 *   + Reads ACR HS firmware from file system for the detected chip.
 *   + Sanity check firmware image descriptor data like size, offsets of DMEM/
 *     IMEM & boot vector. Exits boot sequence upon sanity check error.
 *   + Patch ucode signature to ACR HS ucode at struct #acr_fw_header patch_loc.
 *   + Load ACR HS ucode on to respective Engine Falcon by calling
 *     #nvgpu_falcon_hs_ucode_load_bootstrap() function, this function performs
 *     below steps to load & bootstrap.
 *     + Reset the falcon
 *     + setup falcon apertures, boot-config
 *     + Copy non-secure/secure code to IMEM and data to DMEM
 *     + Write non-zero value to mailbox register which is updated by HS bin to
 *       denote its return status
 *     + Bootstrap falcon
 *   + NvGPU ACR waits for Falcon halt & check the mailbox-0 to know the ACR HS
 *     ucode status
 *     + Waits for Falcon halt by calling #nvgpu_falcon_wait_for_halt() function
 *     + Upon Falcon halt or Falcon halt timeout, NvGPU ACR checks for bus error
 *       & validate the integrity of falcon IMEM and DMEM.
 *     + Reports to 3LSS & return error if any error seen from ACR HS ucode,
 *       integrity of falcon IMEM/DMEM error or bus error.
 *     + Return 0 upon Falcon halt with ACR_OK status.
 *   + Return 0 upon ACR_OK status else error to exit the boot sequence.
 *
 * @return 0 in case of success, < 0 in case of failure.
 * @retval -ENOENT if ACR ucode read fails from file-system.
 * @retval -EAGAIN if HS ACR ucode bootstrap fails.
 */
int nvgpu_acr_bootstrap_hs_acr(struct gk20a *g, struct nvgpu_acr *acr);

/**
 * @brief During ACR initialization lsf_enable_mask is set for supported
 *        LS Falcon and it is used during blob creation to add LS Falcon
 *        ucode details to blob. It has details like falcon id, dma id
 *        and lazy bootstrap status.
 *        This function checks if ls-Falcon lazy-bootstrap status to
 *        load & bootstrap from LS-RTOS or not.
 *
 * @param g   [in] The GPU driver struct.
 * @param acr [in] The ACR private data struct
 *
 * Chek if ls-Falcon lazy-bootstrap status to load & bootstrap from
 * LS-RTOS or not.
 *
 * @return True in case of success, False in case of failure.
 */
bool nvgpu_acr_is_lsf_lazy_bootstrap(struct gk20a *g, struct nvgpu_acr *acr,
	u32 falcon_id);

#endif /* NVGPU_ACR_H */

