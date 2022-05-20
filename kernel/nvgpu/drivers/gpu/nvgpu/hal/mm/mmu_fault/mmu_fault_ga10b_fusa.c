/*
 * Copyright (c) 2020-2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/dma.h>
#include <nvgpu/log.h>
#include <nvgpu/enabled.h>
#include <nvgpu/gmmu.h>
#include <nvgpu/bug.h>
#include <nvgpu/io.h>
#include <nvgpu/utils.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/mmu_fault.h>

#include "hal/mm/mmu_fault/mmu_fault_ga10b.h"

#include <nvgpu/hw/ga10b/hw_gmmu_ga10b.h>

static const char mmufault_invalid_str[] = "invalid";

static const char *const ga10b_fault_type_descs[] = {
	[0x00U] = "invalid pde",
	[0x01U] = "invalid pde size",
	[0x02U] = "invalid pte",
	[0x03U] = "limit violation",
	[0x04U] = "unbound inst block",
	[0x05U] = "priv violation",
	[0x06U] = "write, ro violation",
	[0x07U] = "read, wo violation",
	[0x08U] = "pitch mask violation",
	[0x09U] = "work creation",
	[0x0AU] = "unsupported aperture",
	[0x0BU] = "compression failure",
	[0x0CU] = "unsupported kind",
	[0x0DU] = "region violation",
	[0x0EU] = "poison",
	[0x0FU] = "atomic violation"
};

static const char *const ga10b_fault_client_type_descs[] = {
	"gpc",
	"hub",
};

static const char *const ga10b_hub_client_descs[] = {
	[0x00U] = "vip",
	[0x01U] = "ce0",
	[0x02U] = "ce1",
	[0x03U] = "dniso/dispniso",
	[0x04U] = "fe0/fe",
	[0x05U] = "fecs0/fecs",
	[0x06U] = "host",
	[0x07U] = "host_cpu",
	[0x08U] = "host_cpu_nb",
	[0x09U] = "iso",
	[0x0AU] = "mmu",
	[0x0BU] = "nvdec0/nvdec",
	[0x0CU] = "ce3",
	[0x0DU] = "nvenc1",
	[0x0EU] = "niso/actrs",
	[0x0FU] = "p2p",
	[0x10U] = "pd",
	[0x11U] = "perf0/perf",
	[0x12U] = "pmu",
	[0x13U] = "rastertwod",
	[0x14U] = "scc",
	[0x15U] = "scc nb",
	[0x16U] = "sec",
	[0x17U] = "ssync",
	[0x18U] = "grcopy/ce2",
	[0x19U] = "xv",
	[0x1AU] = "mmu nb",
	[0x1BU] = "nvenc0/nvenc",
	[0x1CU] = "dfalcon",
	[0x1DU] = "sked0/sked",
	[0x1EU] = "afalcon",
	[0x1FU] = "dont_care",
	[0x20U] = "hsce0",
	[0x21U] = "hsce1",
	[0x22U] = "hsce2",
	[0x23U] = "hsce3",
	[0x24U] = "hsce4",
	[0x25U] = "hsce5",
	[0x26U] = "hsce6",
	[0x27U] = "hsce7",
	[0x28U] = "hsce8",
	[0x29U] = "hsce9",
	[0x2AU] = "hshub",
	[0x2BU] = "ptp_x0",
	[0x2CU] = "ptp_x1",
	[0x2DU] = "ptp_x2",
	[0x2EU] = "ptp_x3",
	[0x2FU] = "ptp_x4",
	[0x30U] = "ptp_x5",
	[0x31U] = "ptp_x6",
	[0x32U] = "ptp_x7",
	[0x33U] = "nvenc2",
	[0x34U] = "vpr scrubber0",
	[0x35U] = "vpr scrubber1",
	[0x36U] = "dwbif",
	[0x37U] = "fbfalcon",
	[0x38U] = "ce shim",
	[0x39U] = "gsp",
	[0x3AU] = "nvdec1",
	[0x3BU] = "nvdec2",
	[0x3CU] = "nvjpg0",
	[0x3DU] = "nvdec3",
	[0x3EU] = "nvdec4",
	[0x3FU] = "ofa0",
	[0x40U] = "hsce10",
	[0x41U] = "hsce11",
	[0x42U] = "hsce12",
	[0x43U] = "hsce13",
	[0x44U] = "hsce14",
	[0x45U] = "hsce15",
	[0x46U] = "ptp_x8",
	[0x47U] = "ptp_x9",
	[0x48U] = "ptp_x10",
	[0x49U] = "ptp_x11",
	[0x4AU] = "ptp_x12",
	[0x4BU] = "ptp_x13",
	[0x4CU] = "ptp_x14",
	[0x4DU] = "ptp_x15",
	[0x4EU] = "fe1",
	[0x4FU] = "fe2",
	[0x50U] = "fe3",
	[0x51U] = "fe4",
	[0x52U] = "fe5",
	[0x53U] = "fe6",
	[0x54U] = "fe7",
	[0x55U] = "fecs1",
	[0x56U] = "fecs2",
	[0x57U] = "fecs3",
	[0x58U] = "fecs4",
	[0x59U] = "fecs5",
	[0x5AU] = "fecs6",
	[0x5BU] = "fecs7",
	[0x5CU] = "sked1",
	[0x5DU] = "sked2",
	[0x5EU] = "sked3",
	[0x5FU] = "sked4",
	[0x60U] = "sked5",
	[0x61U] = "sked6",
	[0x62U] = "sked7",
	[0x63U] = "esc",
	[0x6FU] = "nvdec5",
	[0x70U] = "nvdec6",
	[0x71U] = "nvdec7",
	[0x72U] = "nvjpg1",
	[0x73U] = "nvjpg2",
	[0x74U] = "nvjpg3",
	[0x75U] = "nvjpg4",
	[0x76U] = "nvjpg5",
	[0x77U] = "nvjpg6",
	[0x78U] = "nvjpg7",
};

static const char *const ga10b_gpc_client_descs[] = {
	[0x00U] = "t1_0",
	[0x01U] = "t1_1",
	[0x02U] = "t1_2",
	[0x03U] = "t1_3",
	[0x04U] = "t1_4",
	[0x05U] = "t1_5",
	[0x06U] = "t1_6",
	[0x07U] = "t1_7",
	[0x08U] = "pe_0",
	[0x09U] = "pe_1",
	[0x0AU] = "pe_2",
	[0x0BU] = "pe_3",
	[0x0CU] = "pe_4",
	[0x0DU] = "pe_5",
	[0x0EU] = "pe_6",
	[0x0FU] = "pe_7",
	[0x10U] = "rast",
	[0x11U] = "gcc",
	[0x12U] = "gpccs",
	[0x13U] = "prop_0",
	[0x14U] = "prop_1",
	[0x15U] = "prop_2",
	[0x16U] = "prop_3",
	[0x21U] = "t1_8",
	[0x22U] = "t1_9",
	[0x23U] = "t1_10",
	[0x24U] = "t1_11",
	[0x25U] = "t1_12",
	[0x26U] = "t1_13",
	[0x27U] = "t1_14",
	[0x28U] = "t1_15",
	[0x29U] = "tpccs_0",
	[0x2AU] = "tpccs_1",
	[0x2BU] = "tpccs_2",
	[0x2CU] = "tpccs_3",
	[0x2DU] = "tpccs_4",
	[0x2EU] = "tpccs_5",
	[0x2FU] = "tpccs_6",
	[0x30U] = "tpccs_7",
	[0x31U] = "pe_8",
	[0x32U] = "pe_9",
	[0x33U] = "tpccs_8",
	[0x34U] = "tpccs_9",
	[0x35U] = "t1_16",
	[0x36U] = "t1_17",
	[0x37U] = "t1_18",
	[0x38U] = "t1_19",
	[0x39U] = "pe_10",
	[0x3AU] =  "pe_11",
	[0x3BU] = "tpccs_10",
	[0x3CU] = "tpccs_11",
	[0x3DU] = "t1_20",
	[0x3EU] = "t1_21",
	[0x3FU] = "t1_22",
	[0x40U] = "t1_23",
	[0x41U] = "pe_12",
	[0x42U] = "pe_13",
	[0x43U] = "tpccs_12",
	[0x44U] = "tpccs_13",
	[0x45U] = "t1_24",
	[0x46U] = "t1_25",
	[0x47U] = "t1_26",
	[0x48U] = "t1_27",
	[0x49U] = "pe_14",
	[0x4AU] = "pe_15",
	[0x4BU] = "tpccs_14",
	[0x4CU] = "tpccs_15",
	[0x4DU] = "t1_28",
	[0x4EU] = "t1_29",
	[0x4FU] = "t1_30",
	[0x50U] = "t1_31",
	[0x51U] = "pe_16",
	[0x52U] = "pe_17",
	[0x53U] = "tpccs_16",
	[0x54U] = "tpccs_17",
	[0x55U] = "t1_32",
	[0x56U] = "t1_33",
	[0x57U] = "t1_34",
	[0x58U] = "t1_35",
	[0x59U] = "pe_18",
	[0x5AU] = "pe_19",
	[0x5BU] = "tpccs_18",
	[0x5CU] = "tpccs_19",
	[0x5DU] = "t1_36",
	[0x5EU] = "t1_37",
	[0x5FU] = "t1_38",
	[0x60U] = "t1_39",
	[0x70U] = "rop_0",
	[0x71U] = "rop_1",
	[0x72U] = "rop_2",
	[0x73U] = "rop_3",
};

void ga10b_mm_mmu_fault_parse_mmu_fault_info(struct mmu_fault_info *mmufault)
{
	if (mmufault->mmu_engine_id == gmmu_fault_mmu_eng_id_bar2_v()) {
		mmufault->mmu_engine_id_type = NVGPU_MMU_ENGINE_ID_TYPE_BAR2;

	} else if (mmufault->mmu_engine_id ==
			gmmu_fault_mmu_eng_id_physical_v()) {
		mmufault->mmu_engine_id_type = NVGPU_MMU_ENGINE_ID_TYPE_PHYSICAL;
	} else {
		mmufault->mmu_engine_id_type = NVGPU_MMU_ENGINE_ID_TYPE_OTHER;
	}

	if (mmufault->fault_type >= ARRAY_SIZE(ga10b_fault_type_descs)) {
		nvgpu_do_assert();
		mmufault->fault_type_desc = mmufault_invalid_str;
	} else {
		mmufault->fault_type_desc =
			 ga10b_fault_type_descs[mmufault->fault_type];
	}

	if (mmufault->client_type >= ARRAY_SIZE(ga10b_fault_client_type_descs)) {
		nvgpu_do_assert();
		mmufault->client_type_desc = mmufault_invalid_str;
	} else {
		mmufault->client_type_desc =
			 ga10b_fault_client_type_descs[mmufault->client_type];
	}

	mmufault->client_id_desc = mmufault_invalid_str;
	if (mmufault->client_type == gmmu_fault_client_type_hub_v()) {
		if (mmufault->client_id < ARRAY_SIZE(ga10b_hub_client_descs)) {
			mmufault->client_id_desc =
				 ga10b_hub_client_descs[mmufault->client_id] ==
					NULL ? "TBD" :
				ga10b_hub_client_descs[mmufault->client_id];
		} else {
			nvgpu_do_assert();
		}
	} else if (mmufault->client_type ==
			gmmu_fault_client_type_gpc_v()) {
		if (mmufault->client_id < ARRAY_SIZE(ga10b_gpc_client_descs)) {
			mmufault->client_id_desc =
				 ga10b_gpc_client_descs[mmufault->client_id] ==
					NULL ? "TBD" :
				ga10b_gpc_client_descs[mmufault->client_id];
		} else {
			nvgpu_do_assert();
		}
	} else {
		/* Nothing to do here */
	}
}
