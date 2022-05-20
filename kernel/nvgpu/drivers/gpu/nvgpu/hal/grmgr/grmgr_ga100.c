/*
 * GA100 GR MANAGER
 *
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

#include <nvgpu/types.h>
#include <nvgpu/enabled.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include "grmgr_ga100.h"

#include <nvgpu/hw/ga100/hw_smcarb_ga100.h>

#define GA100_GRMGR_PSMCARB_ALLOWED_UGPU(gpu_instance_id, gpcgrp_id) \
	((gpu_instance_id == 0U) || ((gpu_instance_id == 1U) && (gpcgrp_id == 0U)) || \
	((gpu_instance_id == 2U) && (gpcgrp_id == 1U)) || ((gpu_instance_id == 3U) && (gpcgrp_id == 0U)) || \
	((gpu_instance_id == 4U) && (gpcgrp_id == 0U)) || ((gpu_instance_id == 5U) && (gpcgrp_id == 1U)) || \
	((gpu_instance_id == 6U) && (gpcgrp_id == 1U)) || ((gpu_instance_id == 7U) && (gpcgrp_id == 0U)) || \
	((gpu_instance_id == 8U) && (gpcgrp_id == 0U)) || ((gpu_instance_id == 9U) && (gpcgrp_id == 0U)) || \
	((gpu_instance_id == 10U) && (gpcgrp_id == 0U)) || ((gpu_instance_id == 11U) && (gpcgrp_id == 1U)) || \
	((gpu_instance_id == 12U) && (gpcgrp_id == 1U)) || ((gpu_instance_id == 13U) && (gpcgrp_id == 1U)) || \
	((gpu_instance_id == 14U) && (gpcgrp_id == 1U)))

#define GA100_GRMGR_PSMCARB_SYS_PIPE_ALLOWED_UGPU(gr_syspipe_id, gpcgrp_id) \
	(((gr_syspipe_id == 0U) && (gpcgrp_id == 0U)) || ((gr_syspipe_id == 1U) && (gpcgrp_id == 0U)) || \
	((gr_syspipe_id == 2U) && (gpcgrp_id == 0U)) || ((gr_syspipe_id == 3U) && (gpcgrp_id == 0U)) || \
	((gr_syspipe_id == 4U) && (gpcgrp_id == 1U)) || ((gr_syspipe_id == 5U) && (gpcgrp_id == 1U)) || \
	((gr_syspipe_id == 6U) && (gpcgrp_id == 1U)) || ((gr_syspipe_id == 7U) && (gpcgrp_id == 1U)))

/* Static mig config list for 8 syspipes(OxFFU) + 8 GPCs + 8 Aysnc LCEs + 4:4 gpc group config */
static const struct nvgpu_mig_gpu_instance_config ga100_gpu_instance_config_8_syspipes = {
	.usable_gr_syspipe_count	= 8U,
	.usable_gr_syspipe_mask		= 0xFFU,
	.num_config_supported		= 10U,
	.gpcgrp_gpc_count		= { 4U, 4U },
	.gpc_count			= 8U,
	.gpu_instance_config = {
		{.config_name = "2 GPU instances each with 4 GPCs",
		 .num_gpu_instances = 2U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 1U,
			 .gr_syspipe_id			= 0U,
			 .num_gpc			= 4U},
			{.gpu_instance_id 		= 2U,
			 .gr_syspipe_id			= 4U,
			 .num_gpc			= 4U}}},
		{.config_name = "4 GPU instances each with 2 GPCs",
		 .num_gpu_instances = 4U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 3U,
			 .gr_syspipe_id			= 0U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 4U,
			 .gr_syspipe_id			= 2U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 5U,
			 .gr_syspipe_id			= 4U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 6U,
			 .gr_syspipe_id			= 6U,
			 .num_gpc			= 2U}}},
		{.config_name = "8 GPU instances each with 1 GPC",
		 .num_gpu_instances = 8U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 7U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 8U,
			 .gr_syspipe_id 		= 1U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 9U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 10U,
			 .gr_syspipe_id 		= 3U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 11U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 12U,
			 .gr_syspipe_id 		= 5U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 13U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 14U,
			 .gr_syspipe_id 		= 7U,
			 .num_gpc			= 1U}}},
		{.config_name = "5 GPU instances - 1 GPU instance with 4 GPCs + 4 GPU instances each with 1 GPC",
		 .num_gpu_instances = 5U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 1U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 4U},
			{.gpu_instance_id 		= 11U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 12U,
			 .gr_syspipe_id 		= 5U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 13U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 14U,
			 .gr_syspipe_id 		= 7U,
			 .num_gpc			= 1U}}},
		{.config_name = "5 GPU instances - 4 GPU instances each with 1 GPC + 1 GPU instance with 4 GPCs",
		 .num_gpu_instances = 5U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 7U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 8U,
			 .gr_syspipe_id 		= 1U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 9U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 10U,
			 .gr_syspipe_id 		= 3U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 2U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 4U}}},
		{.config_name = "6 GPU instances - 2 GPU instances each with 2 GPCs + 4 GPU instances each with 1 GPC",
		 .num_gpu_instances = 6U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 3U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 4U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 11U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 12U,
			 .gr_syspipe_id 		= 5U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 13U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 14U,
			 .gr_syspipe_id 		= 7U,
			 .num_gpc			= 1U}}},
		{.config_name = "6 GPU instances -  4 GPU instances each with 1 GPC + 2 GPU instances with 2 GPCs",
		 .num_gpu_instances = 6U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 7U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 8U,
			 .gr_syspipe_id 		= 1U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 9U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 10U,
			 .gr_syspipe_id 		= 3U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 5U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 6U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 2U}}},
		{.config_name = "5 GPU instances - 2 GPU instances each with 2 GPCs + 1 GPC instance with 2 GPCs + 2 GPU instances each with 1 GPC",
		.num_gpu_instances = 5U,
		.gpu_instance_static_config = {
			{.gpu_instance_id 		= 3U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 4U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 5U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 13U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 14U,
			 .gr_syspipe_id 		= 7U,
			 .num_gpc			= 1U}}},
		{.config_name = "5 GPU instances - 1 GPC instance with 2 GPCs + 2 GPU instances each with 1 GPC + 2 GPU instances each with 2 GPCs",
		.num_gpu_instances = 5U,
		.gpu_instance_static_config = {
			{.gpu_instance_id 		= 3U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 9U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 10U,
			 .gr_syspipe_id 		= 3U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 5U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 6U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 2U}}},
		{.config_name = "1 GPU instance with 8 GPCs",
		 .num_gpu_instances = 1U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 0U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 8U}}}}
};

/* Static mig config list for 7 syspipes(OxFDU) + 8 GPCs + 8 Aysnc LCEs + 3:4 gpc group config */
static const struct nvgpu_mig_gpu_instance_config ga100_gpu_instance_config_7_syspipes = {
	.usable_gr_syspipe_count	= 7U,
	.usable_gr_syspipe_mask		= 0xFDU,
	.num_config_supported		= 10U,
	.gpcgrp_gpc_count		= { 4U, 4U },
	.gpc_count			= 8U,
	.gpu_instance_config = {
		{.config_name = "2 GPU instances each with 4 GPCs",
		 .num_gpu_instances = 2U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 1U,
			 .gr_syspipe_id			= 0U,
			 .num_gpc			= 4U},
			{.gpu_instance_id 		= 2U,
			 .gr_syspipe_id			= 4U,
			 .num_gpc			= 4U}}},
		{.config_name = "4 GPU instances each with 2 GPCs",
		 .num_gpu_instances = 4U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 3U,
			 .gr_syspipe_id			= 0U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 4U,
			 .gr_syspipe_id			= 2U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 5U,
			 .gr_syspipe_id			= 4U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 6U,
			 .gr_syspipe_id			= 6U,
			 .num_gpc			= 2U}}},
		{.config_name = "7 GPU instances - 1 GPU instance with 2 GPCs + 6 GPU instances each with 1 GPC",
		 .num_gpu_instances = 7U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 7U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 9U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id		= 10U,
			 .gr_syspipe_id 		= 3U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 11U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 12U,
			 .gr_syspipe_id 		= 5U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 13U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 14U,
			 .gr_syspipe_id 		= 7U,
			 .num_gpc			= 1U}}},
		{.config_name = "5 GPU instances - 1 GPU instance with 4 GPCs + 4 GPU instances each with 1 GPC",
		 .num_gpu_instances = 5U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 1U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 4U},
			{.gpu_instance_id 		= 11U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 12U,
			 .gr_syspipe_id 		= 5U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 13U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 14U,
			 .gr_syspipe_id 		= 7U,
			 .num_gpc			= 1U}}},
		{.config_name = "4 GPU instances - 1 GPU instance with 2 GPCs + 2 GPU instances each with 1 GPC + 1 GPU instance with 4 GPCs",
		 .num_gpu_instances = 4U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 3U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 2U},
			{.gpu_instance_id		= 9U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id		= 10U,
			 .gr_syspipe_id 		= 3U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 2U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 4U}}},
		{.config_name = "6 GPU instances - 2 GPU instances each with 2 GPCs + 4 GPU instances each with 1 GPC",
		 .num_gpu_instances = 6U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 3U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 4U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 11U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 12U,
			 .gr_syspipe_id 		= 5U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 13U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 14U,
			 .gr_syspipe_id 		= 7U,
			 .num_gpc			= 1U}}},
		{.config_name = "5 GPU instances -  1 GPU instance with 2 GPCs + 2 GPU instances each with 1 GPC + 2 GPU instances with 2 GPCs",
		 .num_gpu_instances = 5U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 3U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 9U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 10U,
			 .gr_syspipe_id 		= 3U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 5U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 6U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 2U}}},
		{.config_name = "5 GPU instances - 2 GPU instances each with 2 GPCs + 1 GPC instance with 2 GPCs + 2 GPU instances with 1 GPC",
		.num_gpu_instances = 5U,
		.gpu_instance_static_config = {
			{.gpu_instance_id 		= 3U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 4U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 5U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 13U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 14U,
			 .gr_syspipe_id 		= 7U,
			 .num_gpc			= 1U}}},
		{.config_name = "5 GPU instances - 1 GPC instance with 2 GPCs + 2 GPU instances each with 1 GPC + 2 GPU instances each with 2 GPCs",
		.num_gpu_instances = 5U,
		.gpu_instance_static_config = {
			{.gpu_instance_id 		= 3U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 9U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 10U,
			 .gr_syspipe_id 		= 3U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 5U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 6U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 2U}}},
		{.config_name = "1 GPU instance with 8 GPCs",
		 .num_gpu_instances = 1U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 0U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 8U}}}}
};

/* Static mig config list for 8 syspipes(OxFFU) + 7 GPCs + 8 Aysnc LCEs + 4:3 gpc group config */
static const struct nvgpu_mig_gpu_instance_config ga100_gpu_instance_config_8_syspipes_7_gpcs_4_3_gpcgrp = {
	.usable_gr_syspipe_count	= 8U,
	.usable_gr_syspipe_mask		= 0xFFU,
	.num_config_supported		= 10U,
	.gpcgrp_gpc_count		= { 4U, 3U },
	.gpc_count			= 7U,
	.gpu_instance_config = {
		{.config_name = "2 GPU instances - 1 GPU instance with 4 GPCs + 1 GPU instance with 3 GPCs",
		 .num_gpu_instances = 2U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 1U,
			 .gr_syspipe_id			= 0U,
			 .num_gpc			= 4U},
			{.gpu_instance_id 		= 2U,
			 .gr_syspipe_id			= 4U,
			 .num_gpc			= 3U}}},
		{.config_name = "4 GPU instances - 3 GPU instances each with 2 GPCs + 1 GPU instance with 1 GPC",
		 .num_gpu_instances = 4U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 3U,
			 .gr_syspipe_id			= 0U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 4U,
			 .gr_syspipe_id			= 2U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 5U,
			 .gr_syspipe_id			= 4U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 13U,
			 .gr_syspipe_id			= 6U,
			 .num_gpc			= 1U}}},
		{.config_name = "7 GPU instances each with 1 GPC",
		 .num_gpu_instances = 7U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 7U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 8U,
			 .gr_syspipe_id 		= 1U,
			 .num_gpc			= 1U},
			{.gpu_instance_id		= 9U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id		= 10U,
			 .gr_syspipe_id 		= 3U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 11U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 12U,
			 .gr_syspipe_id 		= 5U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 13U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 1U}}},
		{.config_name = "4 GPU instances - 1 GPU instance with 4 GPCs + 3 GPU instances each with 1 GPC",
		 .num_gpu_instances = 4U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 1U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 4U},
			{.gpu_instance_id 		= 11U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 12U,
			 .gr_syspipe_id 		= 5U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 13U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 1U}}},
		{.config_name = "5 GPU instances - 2 GPU instances with 2 GPCs + 3 GPU instances each with 1 GPC",
		 .num_gpu_instances = 5U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 3U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 2U},
			{.gpu_instance_id		= 4U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 2U},
			{.gpu_instance_id		= 11U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 1U},
			{.gpu_instance_id		= 12U,
			 .gr_syspipe_id 		= 5U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 13U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 1U}}},
		{.config_name = "6 GPU instances - 1 GPU instance with 2 GPCs + 5 GPU instances each with 1 GPC",
		 .num_gpu_instances = 6U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 3U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 9U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id		= 10U,
			 .gr_syspipe_id 		= 3U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 11U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 12U,
			 .gr_syspipe_id 		= 5U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 13U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 1U}}},
		{.config_name = "5 GPU instances - 1 GPU instance with 2 GPCs + 2 GPU instances each with 1 GPC + 1 GPU instance with 2 GPCs + 1 GPU instance with 1 GPC",
		 .num_gpu_instances = 5U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 3U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 9U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 10U,
			 .gr_syspipe_id 		= 3U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 5U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 13U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 1U}}},
		{.config_name = "5 GPU instances - 2 GPU instances each with 1 GPC + 2 GPC instances each with 2 GPCs + + 1 GPC instance with 1 GPC",
		.num_gpu_instances = 5U,
		.gpu_instance_static_config = {
			{.gpu_instance_id		= 7U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 1U},
			{.gpu_instance_id		= 8U,
			 .gr_syspipe_id 		= 1U,
			 .num_gpc			= 1U},
			{.gpu_instance_id		= 4U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 5U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 13U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 1U}}},
		{.config_name = "5 GPU instances - 4 GPU instances each with 1 GPC + 1 GPC instance with 3 GPCs",
		.num_gpu_instances = 5U,
		.gpu_instance_static_config = {
			{.gpu_instance_id		= 7U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 8U,
			 .gr_syspipe_id 		= 1U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 9U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 10U,
			 .gr_syspipe_id 		= 3U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 2U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 3U}}},
		{.config_name = "1 GPU instance with 7 GPCs",
		 .num_gpu_instances = 1U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 0U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 7U}}}}
};

/* Static mig config list for 8 syspipes(OxFFU) + 7 GPCs + 8 Aysnc LCEs + 3:4 gpc group config */
static const struct nvgpu_mig_gpu_instance_config ga100_gpu_instance_config_8_syspipes_7_gpcs_3_4_gpcgrp = {
	.usable_gr_syspipe_count	= 8U,
	.usable_gr_syspipe_mask		= 0xFFU,
	.num_config_supported		= 10U,
	.gpcgrp_gpc_count		= { 3U, 4U },
	.gpc_count			= 7U,
	.gpu_instance_config = {
		{.config_name = "2 GPU instances - 1 GPU instance with 3 GPCs + 1 GPU instance with 4 GPCs",
		 .num_gpu_instances = 2U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 1U,
			 .gr_syspipe_id			= 0U,
			 .num_gpc			= 3U},
			{.gpu_instance_id 		= 2U,
			 .gr_syspipe_id			= 4U,
			 .num_gpc			= 4U}}},
		{.config_name = "4 GPU instances - 1 GPU instance with 2 GPCs + 1 GPU instance with 1 GPC + 2 GPU instances with 2 GPCs",
		 .num_gpu_instances = 4U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 3U,
			 .gr_syspipe_id			= 0U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 9U,
			 .gr_syspipe_id			= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 5U,
			 .gr_syspipe_id			= 4U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 6U,
			 .gr_syspipe_id			= 6U,
			 .num_gpc			= 2U}}},
		{.config_name = "7 GPU instances each with 1 GPC",
		 .num_gpu_instances = 7U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 7U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 8U,
			 .gr_syspipe_id 		= 1U,
			 .num_gpc			= 1U},
			{.gpu_instance_id		= 9U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 11U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 12U,
			 .gr_syspipe_id 		= 5U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 13U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 1U},
 			{.gpu_instance_id 		= 14U,
			 .gr_syspipe_id 		= 7U,
			 .num_gpc			= 1U}}},
		{.config_name = "4 GPU instances - 3 GPU instances each with 1 GPC + 1 GPU instance with 4 GPCs",
		 .num_gpu_instances = 4U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 7U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 8U,
			 .gr_syspipe_id 		= 1U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 9U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 2U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 4U}}},
		{.config_name = "6 GPU instances - 1 GPU instance with 2 GPCs + 1 GPU instance with 1 GPC + 4 GPU instances each with 1 GPC",
		 .num_gpu_instances = 6U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 3U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 2U},
			{.gpu_instance_id		= 9U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id		= 11U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 1U},
			{.gpu_instance_id		= 12U,
			 .gr_syspipe_id 		= 5U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 13U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 14U,
			 .gr_syspipe_id 		= 7U,
			 .num_gpc			= 1U}}},
		{.config_name = "6 GPU instances - 1 GPU instances with 2 GPCs + 5 GPU instances each with 1 GPC",
		 .num_gpu_instances = 6U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 3U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 9U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 11U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 12U,
			 .gr_syspipe_id 		= 5U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 13U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 14U,
			 .gr_syspipe_id 		= 7U,
			 .num_gpc			= 1U}}},
		{.config_name = "5 GPU instances - 1 GPU instance with 2 GPCs + 1 GPU instance with 1 GPC + 1 GPU instance with 2 GPCs + 2 GPU instances each with 1 GPC",
		 .num_gpu_instances = 5U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 3U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 9U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 5U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 13U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 14U,
			 .gr_syspipe_id 		= 7U,
			 .num_gpc			= 1U}}},
		{.config_name = "5 GPU instances - 3 GPU instances each with 1 GPC + 2 GPC instances with 2 GPCs",
		.num_gpu_instances = 5U,
		.gpu_instance_static_config = {
			{.gpu_instance_id		= 7U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 1U},
			{.gpu_instance_id		= 8U,
			 .gr_syspipe_id 		= 1U,
			 .num_gpc			= 1U},
			{.gpu_instance_id		= 9U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id		= 5U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 6U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 2U}}},
		{.config_name = "4 GPU instances - 3 GPU instances each with 1 GPC + 1 GPC instance with 4 GPCs",
		.num_gpu_instances = 4U,
		.gpu_instance_static_config = {
			{.gpu_instance_id		= 7U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 8U,
			 .gr_syspipe_id 		= 1U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 9U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 2U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 4U}}},
		{.config_name = "1 GPU instance with 7 GPCs",
		 .num_gpu_instances = 1U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 0U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 7U}}}}
};

/* Static mig config list for 8 syspipes(OxFFU) + 6 GPCs + 8 Aysnc LCEs + 3:3 gpc group config */
static const struct nvgpu_mig_gpu_instance_config ga100_gpu_instance_config_8_syspipes_6_gpcs_3_3_gpcgrp = {
	.usable_gr_syspipe_count	= 8U,
	.usable_gr_syspipe_mask		= 0xFFU,
	.num_config_supported		= 7U,
	.gpcgrp_gpc_count		= { 3U, 3U },
	.gpc_count			= 6U,
	.gpu_instance_config = {
		{.config_name = "2 GPU instances each with 3 GPCs",
		 .num_gpu_instances = 2U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 1U,
			 .gr_syspipe_id			= 0U,
			 .num_gpc			= 3U},
			{.gpu_instance_id 		= 2U,
			 .gr_syspipe_id			= 4U,
			 .num_gpc			= 3U}}},
		{.config_name = "3 GPU instances - 1 GPU instance with 3 GPCs + 1 GPU instance with 2 GPCs + 1 GPU instance with 1 GPC",
		 .num_gpu_instances = 3U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 1U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 3U},
			{.gpu_instance_id 		= 5U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 13U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 1U}}},
			{.config_name = "6 GPU instances each with 1 GPC",
			 .num_gpu_instances = 6U,
			 .gpu_instance_static_config = {
			{.gpu_instance_id		= 7U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 1U},
			{.gpu_instance_id		= 8U,
			 .gr_syspipe_id 		= 1U,
			 .num_gpc			= 1U},
			{.gpu_instance_id		= 9U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id		= 11U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 1U},
			{.gpu_instance_id		= 12U,
			 .gr_syspipe_id 		= 5U,
			 .num_gpc			= 1U},
			{.gpu_instance_id		= 13U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 1U}}},
		{.config_name = "4 GPU instances - 1 GPU instance with 3 GPCs + 3 GPU instances each with 1 GPC",
		 .num_gpu_instances = 4U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 1U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 3U},
			{.gpu_instance_id 		= 11U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 12U,
			 .gr_syspipe_id 		= 5U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 13U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 1U}}},
		{.config_name = "4 GPU instances - 1 GPU instance with 2 GPCs + 1 GPU instance with 1 GPC + 1 GPU instance with 2 GPCs + 1 GPU instance with 1 GPC",
		 .num_gpu_instances = 4U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 3U,
			 .gr_syspipe_id			= 0U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 9U,
			 .gr_syspipe_id			= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 5U,
			 .gr_syspipe_id			= 4U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 13U,
			 .gr_syspipe_id			= 6U,
			 .num_gpc			= 1U}}},
		{.config_name = "5 GPU instances - 1 GPU instance with 2 GPCs + 1 GPU instance with 1 GPC + 3 GPU instances each with 1 GPC",
		 .num_gpu_instances = 5U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 3U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 9U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 11U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 12U,
			 .gr_syspipe_id 		= 5U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 13U,
			 .gr_syspipe_id 		= 6U,
			 .num_gpc			= 1U}}},
		{.config_name = "1 GPU instance with 6 GPCs",
		 .num_gpu_instances = 1U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 0U,
			 .gr_syspipe_id			= 0U,
			 .num_gpc			= 6U}}}}
};

/* Static mig config list for 8 syspipes(OxFFU) + 6 GPCs + 8 Aysnc LCEs + 4:2 gpc group config */
static const struct nvgpu_mig_gpu_instance_config ga100_gpu_instance_config_8_syspipes_6_gpcs_4_2_gpcgrp = {
	.usable_gr_syspipe_count	= 8U,
	.usable_gr_syspipe_mask		= 0xFFU,
	.num_config_supported		= 7U,
	.gpcgrp_gpc_count		= { 4U, 2U },
	.gpc_count			= 6U,
	.gpu_instance_config = {
		{.config_name = "3 GPU instances each with 2 GPCs",
		 .num_gpu_instances = 3U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 3U,
			 .gr_syspipe_id			= 0U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 4U,
			 .gr_syspipe_id			= 2U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 5U,
			 .gr_syspipe_id			= 4U,
			 .num_gpc			= 2U}}},
		{.config_name = "4 GPU instances - 2 GPU instances each with 2 GPCs + 2 GPU instances each with 1 GPC ",
		 .num_gpu_instances = 4U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 3U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 4U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 11U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 12U,
			 .gr_syspipe_id 		= 5U,
			 .num_gpc			= 1U}}},
			{.config_name = "6 GPU instances each with 1 GPC",
			 .num_gpu_instances = 6U,
			 .gpu_instance_static_config = {
			{.gpu_instance_id		= 7U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 1U},
			{.gpu_instance_id		= 8U,
			 .gr_syspipe_id 		= 1U,
			 .num_gpc			= 1U},
			{.gpu_instance_id		= 9U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id		= 10U,
			 .gr_syspipe_id 		= 3U,
			 .num_gpc			= 1U},
			{.gpu_instance_id		= 11U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 1U},
			{.gpu_instance_id		= 12U,
			 .gr_syspipe_id 		= 5U,
			 .num_gpc			= 1U}}},
		{.config_name = "4 GPU instances - 2 GPU instances each with 1 GPC + 2 GPU instances each with 2 GPCs",
		 .num_gpu_instances = 4U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 7U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 8U,
			 .gr_syspipe_id 		= 1U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 4U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 5U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 2U}}},
		{.config_name = "4 GPU instances - 1 GPU instance with 2 GPCs + 2 GPU instances each with 1 GPC + 1 GPU instance with 2 GPCs ",
		 .num_gpu_instances = 4U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 3U,
			 .gr_syspipe_id			= 0U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 9U,
			 .gr_syspipe_id			= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 10U,
			 .gr_syspipe_id			= 3U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 5U,
			 .gr_syspipe_id			= 4U,
			 .num_gpc			= 2U}}},
		{.config_name = "5 GPU instances - 1 GPU instance with 2 GPCs + 4 GPU instances each with 1 GPC",
		 .num_gpu_instances = 5U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 3U,
			 .gr_syspipe_id 		= 0U,
			 .num_gpc			= 2U},
			{.gpu_instance_id 		= 9U,
			 .gr_syspipe_id 		= 2U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 10U,
			 .gr_syspipe_id 		= 3U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 11U,
			 .gr_syspipe_id 		= 4U,
			 .num_gpc			= 1U},
			{.gpu_instance_id 		= 12U,
			 .gr_syspipe_id 		= 5U,
			 .num_gpc			= 1U}}},
		{.config_name = "1 GPU instance with 6 GPCs",
		 .num_gpu_instances = 1U,
		 .gpu_instance_static_config = {
			{.gpu_instance_id 		= 0U,
			 .gr_syspipe_id			= 0U,
			 .num_gpc			= 6U}}}}
};

static int ga100_grmgr_get_first_available_gr_syspipe_id(struct gk20a *g,
		u32 gpu_instance_id, u32 ugpu_id, u32 *gr_syspipe_id)
{
	u32 usable_gr_syspipe_count = g->mig.usable_gr_syspipe_count;
	u32 index;

	if (gr_syspipe_id == NULL) {
		nvgpu_err(g, "gr_syspipe_id NULL");
		return -EINVAL;
	}
	for (index = 0U; index < usable_gr_syspipe_count; index++) {
		u32 local_gr_syspipe_id =
			g->mig.usable_gr_syspipe_instance_id[index];
		if (GA100_GRMGR_PSMCARB_SYS_PIPE_ALLOWED_UGPU(local_gr_syspipe_id, ugpu_id)) {
			*gr_syspipe_id = local_gr_syspipe_id;
			return 0;
		}
	}

	return -EINVAL;
}

const struct nvgpu_mig_gpu_instance_config *ga100_grmgr_get_mig_config_ptr(
		struct gk20a *g) {
	static struct nvgpu_mig_gpu_instance_config ga100_gpu_instance_default_config;
	struct nvgpu_gpu_instance_config *gpu_instance_config;
	u32 gpc_count_per_gpu_instance = 0U;
	u32 num_config = 0U;
	int err;

	if ((g->mig.usable_gr_syspipe_count ==
				ga100_gpu_instance_config_8_syspipes_6_gpcs_3_3_gpcgrp.usable_gr_syspipe_count) &&
			(g->mig.usable_gr_syspipe_mask ==
				ga100_gpu_instance_config_8_syspipes_6_gpcs_3_3_gpcgrp.usable_gr_syspipe_mask) &&
			(g->mig.gpc_count ==
				ga100_gpu_instance_config_8_syspipes_6_gpcs_3_3_gpcgrp.gpc_count) &&
			(g->mig.gpcgrp_gpc_count[0] ==
				ga100_gpu_instance_config_8_syspipes_6_gpcs_3_3_gpcgrp.gpcgrp_gpc_count[0]) &&
			(g->mig.gpcgrp_gpc_count[1] ==
				ga100_gpu_instance_config_8_syspipes_6_gpcs_3_3_gpcgrp.gpcgrp_gpc_count[1])) {
		nvgpu_log(g, gpu_dbg_mig,
			"Static mig config list for 8 syspipes(0xFFU) + 6 GPCs + 8 Aysnc LCEs "
			"+ 3:3 gpc group config ");
		return &ga100_gpu_instance_config_8_syspipes_6_gpcs_3_3_gpcgrp;
	} else if ((g->mig.usable_gr_syspipe_count ==
				ga100_gpu_instance_config_8_syspipes_6_gpcs_4_2_gpcgrp.usable_gr_syspipe_count) &&
			(g->mig.usable_gr_syspipe_mask ==
				ga100_gpu_instance_config_8_syspipes_6_gpcs_4_2_gpcgrp.usable_gr_syspipe_mask) &&
			(g->mig.gpc_count ==
				ga100_gpu_instance_config_8_syspipes_6_gpcs_4_2_gpcgrp.gpc_count) &&
			(g->mig.gpcgrp_gpc_count[0] ==
				ga100_gpu_instance_config_8_syspipes_6_gpcs_4_2_gpcgrp.gpcgrp_gpc_count[0]) &&
			(g->mig.gpcgrp_gpc_count[1] ==
				ga100_gpu_instance_config_8_syspipes_6_gpcs_4_2_gpcgrp.gpcgrp_gpc_count[1])) {
		nvgpu_log(g, gpu_dbg_mig,
			"Static mig config list for 8 syspipes(0xFFU) + 6 GPCs + 8 Aysnc LCEs "
			"+ 4:2 gpc group config ");
		return &ga100_gpu_instance_config_8_syspipes_6_gpcs_4_2_gpcgrp;
	} else if ((g->mig.usable_gr_syspipe_count ==
				ga100_gpu_instance_config_8_syspipes.usable_gr_syspipe_count) &&
			(g->mig.usable_gr_syspipe_mask ==
				ga100_gpu_instance_config_8_syspipes.usable_gr_syspipe_mask) &&
			(g->mig.gpc_count ==
				ga100_gpu_instance_config_8_syspipes.gpc_count) &&
			(g->mig.gpcgrp_gpc_count[0] ==
				ga100_gpu_instance_config_8_syspipes.gpcgrp_gpc_count[0]) &&
			(g->mig.gpcgrp_gpc_count[1] ==
				ga100_gpu_instance_config_8_syspipes.gpcgrp_gpc_count[1])) {
		nvgpu_log(g, gpu_dbg_mig,
			"Static mig config list for 8 syspipes(0xFFU) + 8 GPCs + 8 Aysnc LCEs "
				"+ 4:4 gpc group config ");
		return &ga100_gpu_instance_config_8_syspipes;
	} else if ((g->mig.usable_gr_syspipe_count ==
				ga100_gpu_instance_config_7_syspipes.usable_gr_syspipe_count) &&
			(g->mig.usable_gr_syspipe_mask ==
				ga100_gpu_instance_config_7_syspipes.usable_gr_syspipe_mask) &&
			(g->mig.gpc_count ==
				ga100_gpu_instance_config_7_syspipes.gpc_count) &&
			(g->mig.gpcgrp_gpc_count[0] ==
				ga100_gpu_instance_config_7_syspipes.gpcgrp_gpc_count[0]) &&
			(g->mig.gpcgrp_gpc_count[1] ==
				ga100_gpu_instance_config_7_syspipes.gpcgrp_gpc_count[1])) {
		nvgpu_log(g, gpu_dbg_mig,
			"Static mig config list for 7 syspipes(0xFDU) + 8 GPCs + 8 Aysnc LCEs "
				"+ 4:4 gpc group config ");
		return &ga100_gpu_instance_config_7_syspipes;
	} else if ((g->mig.usable_gr_syspipe_count ==
				ga100_gpu_instance_config_8_syspipes_7_gpcs_4_3_gpcgrp.usable_gr_syspipe_count) &&
			(g->mig.usable_gr_syspipe_mask ==
				ga100_gpu_instance_config_8_syspipes_7_gpcs_4_3_gpcgrp.usable_gr_syspipe_mask) &&
			(g->mig.gpc_count ==
				ga100_gpu_instance_config_8_syspipes_7_gpcs_4_3_gpcgrp.gpc_count) &&
			(g->mig.gpcgrp_gpc_count[0] ==
				ga100_gpu_instance_config_8_syspipes_7_gpcs_4_3_gpcgrp.gpcgrp_gpc_count[0]) &&
			(g->mig.gpcgrp_gpc_count[1] ==
				ga100_gpu_instance_config_8_syspipes_7_gpcs_4_3_gpcgrp.gpcgrp_gpc_count[1])) {
		nvgpu_log(g, gpu_dbg_mig,
			"Static mig config list for 8 syspipes(0xFFU) + 7 GPCs + 8 Aysnc LCEs "
				"+ 4:3 gpc group config ");
		return &ga100_gpu_instance_config_8_syspipes_7_gpcs_4_3_gpcgrp;
	} else if ((g->mig.usable_gr_syspipe_count ==
				ga100_gpu_instance_config_8_syspipes_7_gpcs_3_4_gpcgrp.usable_gr_syspipe_count) &&
			(g->mig.usable_gr_syspipe_mask ==
				ga100_gpu_instance_config_8_syspipes_7_gpcs_3_4_gpcgrp.usable_gr_syspipe_mask) &&
			(g->mig.gpc_count ==
				ga100_gpu_instance_config_8_syspipes_7_gpcs_3_4_gpcgrp.gpc_count) &&
			(g->mig.gpcgrp_gpc_count[0] ==
				ga100_gpu_instance_config_8_syspipes_7_gpcs_3_4_gpcgrp.gpcgrp_gpc_count[0]) &&
			(g->mig.gpcgrp_gpc_count[1] ==
				ga100_gpu_instance_config_8_syspipes_7_gpcs_3_4_gpcgrp.gpcgrp_gpc_count[1])) {
		nvgpu_log(g, gpu_dbg_mig,
			"Static mig config list for 8 syspipes(0xFFU) + 7 GPCs + 8 Aysnc LCEs "
				"+ 3:4 gpc group config ");
		return &ga100_gpu_instance_config_8_syspipes_7_gpcs_3_4_gpcgrp;
	}

	/* Fall back to default config */
	ga100_gpu_instance_default_config.usable_gr_syspipe_count =
		g->mig.usable_gr_syspipe_count;
	ga100_gpu_instance_default_config.usable_gr_syspipe_mask =
		g->mig.usable_gr_syspipe_mask;
	ga100_gpu_instance_default_config.gpcgrp_gpc_count[0] =
		g->mig.gpcgrp_gpc_count[0];
	ga100_gpu_instance_default_config.gpcgrp_gpc_count[1] =
		g->mig.gpcgrp_gpc_count[1];
	ga100_gpu_instance_default_config.gpc_count = g->mig.gpc_count;

	gpc_count_per_gpu_instance = (g->mig.gpc_count / 2);

	if ((g->mig.usable_gr_syspipe_count >= 0x2U) &&
			(g->mig.gpcgrp_gpc_count[0] >= gpc_count_per_gpu_instance) &&
			(g->mig.gpcgrp_gpc_count[1] >= gpc_count_per_gpu_instance)) {
		u32 index;
		u32 start_id_of_half_partition = 0x1;
		gpu_instance_config =
			&ga100_gpu_instance_default_config.gpu_instance_config[num_config];
		snprintf(gpu_instance_config->config_name,
			NVGPU_MIG_MAX_CONFIG_NAME_SIZE,
			"2 GPU instances each with %u GPCs", gpc_count_per_gpu_instance);
		gpu_instance_config->num_gpu_instances = 2U;

		for (index = 0U; index < 2U; index++) {
			struct nvgpu_gpu_instance_static_config
				*gpu_instance_static_config =
					&gpu_instance_config->gpu_instance_static_config[index];
			gpu_instance_static_config->gpu_instance_id =
				nvgpu_safe_add_u32(start_id_of_half_partition, index);

			err = ga100_grmgr_get_first_available_gr_syspipe_id(g,
					nvgpu_safe_add_u32(start_id_of_half_partition, index),
					index,
					&gpu_instance_static_config->gr_syspipe_id);
			if (err != 0) {
				nvgpu_err(g, "ga100_grmgr_get_first_available_gr_syspipe_id-failed "
					"index[%u] gpu_instance_id[%u] ",
					index,
					gpu_instance_static_config->gpu_instance_id);
				return NULL;
			}
			gpu_instance_static_config->num_gpc = gpc_count_per_gpu_instance;

			nvgpu_log(g, gpu_dbg_mig,
				"Fall back to default HALF partition index[%u] config_index[%u] "
					"gpu_instance_id[%u] gr_syspipe_id[%u] num_gpc[%u]",
					index,
					num_config,
					gpu_instance_static_config->gpu_instance_id,
					gpu_instance_static_config->gr_syspipe_id,
					gpu_instance_static_config->num_gpc);
		}
		num_config++;
	}

	gpu_instance_config =
		&ga100_gpu_instance_default_config.gpu_instance_config[num_config];

	snprintf(gpu_instance_config->config_name,
		NVGPU_MIG_MAX_CONFIG_NAME_SIZE,
		"1 GPU instance with %u GPCs", g->mig.gpc_count);
	gpu_instance_config->num_gpu_instances = 1U;
	gpu_instance_config->gpu_instance_static_config[0].gpu_instance_id = 0U;
	gpu_instance_config->gpu_instance_static_config[0].gr_syspipe_id = 0U;
	gpu_instance_config->gpu_instance_static_config[0].num_gpc =
		g->mig.gpc_count;

	++num_config;
	ga100_gpu_instance_default_config.num_config_supported = num_config;

	nvgpu_err(g,
		"mig gpu instance config is not found for usable_gr_syspipe_count[%u] "
			"usable_gr_syspipe_mask[%x] gpc[%u] "
			"fall back to %u default config mode",
		g->mig.usable_gr_syspipe_count,
		g->mig.usable_gr_syspipe_mask,
		g->mig.gpc_count,
		num_config);
	return ((const struct nvgpu_mig_gpu_instance_config *)
		&ga100_gpu_instance_default_config);
}

u32 ga100_grmgr_get_max_sys_pipes(struct gk20a *g)
{
	return smcarb_max_partitionable_sys_pipes_v();
}

u32 ga100_grmgr_get_allowed_swizzid_size(struct gk20a *g)
{
	return smcarb_allowed_swizzid__size1_v();
}

int ga100_grmgr_get_gpc_instance_gpcgrp_id(struct gk20a *g,
		u32 gpu_instance_id, u32 gr_syspipe_id, u32 *gpcgrp_id)
{
	u32 local_gpcgrp_id;
	bool supported;

	if ((gr_syspipe_id >= g->ops.grmgr.get_max_sys_pipes(g)) ||
			(gpu_instance_id >= smcarb_allowed_swizzid__size1_v()) ||
			(gpcgrp_id == NULL)) {
		nvgpu_err(g,
			"[Invalid param] gr_syspipe_id[%u %u] gpu_instance_id[%u %u] "
				"or gpcgrp_id == NULL ",
				gr_syspipe_id, g->ops.grmgr.get_max_sys_pipes(g),
				gpu_instance_id, smcarb_allowed_swizzid__size1_v());
		return -EINVAL;
	}

	for (local_gpcgrp_id = 0U; local_gpcgrp_id < 2U; local_gpcgrp_id++) {
		supported = GA100_GRMGR_PSMCARB_ALLOWED_UGPU(
			gpu_instance_id, local_gpcgrp_id);
		supported &= GA100_GRMGR_PSMCARB_SYS_PIPE_ALLOWED_UGPU(
			gr_syspipe_id, local_gpcgrp_id);
		if (supported) {
			*gpcgrp_id = local_gpcgrp_id;
			nvgpu_log(g, gpu_dbg_mig,
					"Found [%u] gpcgrp id for gpu_instance_id[%u] "
						"gr_syspipe_id[%u] ",
					*gpcgrp_id,
					gpu_instance_id,
					gr_syspipe_id);
			return 0;
		}
	}

	return -EINVAL;
}
