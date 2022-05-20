/*
 * tegra_asoc_machine_virt_alt.c - Tegra xbar dai link for machine drivers
 *
 * Copyright (c) 2017-2021 NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/export.h>
#include <sound/soc.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <sound/soc.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>


#include "tegra_asoc_machine_virt_alt.h"

#define CODEC_NAME		NULL

#define DAI_NAME(i)		"AUDIO" #i
#define STREAM_NAME		"playback"
#define LINK_CPU_NAME		DRV_NAME
#define CPU_DAI_NAME(i)		"ADMAIF" #i
#define CODEC_DAI_NAME		"dit-hifi"
#define PLATFORM_NAME		LINK_CPU_NAME

static unsigned int num_dai_links;
static const struct snd_soc_pcm_stream default_params = {
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
};

static const struct snd_soc_pcm_stream adsp_default_params = {
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
};

static struct snd_soc_pcm_stream adsp_admaif_params[MAX_ADMAIF_IDS];

SND_SOC_DAILINK_DEFS(audio1,
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, CPU_DAI_NAME(1))),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF1 CIF")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(PLATFORM_NAME)));

SND_SOC_DAILINK_DEFS(audio2,
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, CPU_DAI_NAME(2))),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF2 CIF")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(PLATFORM_NAME)));

SND_SOC_DAILINK_DEFS(audio3,
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, CPU_DAI_NAME(3))),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF3 CIF")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(PLATFORM_NAME)));

SND_SOC_DAILINK_DEFS(audio4,
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, CPU_DAI_NAME(4))),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF4 CIF")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(PLATFORM_NAME)));

SND_SOC_DAILINK_DEFS(audio5,
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, CPU_DAI_NAME(5))),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF5 CIF")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(PLATFORM_NAME)));

SND_SOC_DAILINK_DEFS(audio6,
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, CPU_DAI_NAME(6))),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF6 CIF")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(PLATFORM_NAME)));

SND_SOC_DAILINK_DEFS(audio7,
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, CPU_DAI_NAME(7))),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF7 CIF")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(PLATFORM_NAME)));

SND_SOC_DAILINK_DEFS(audio8,
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, CPU_DAI_NAME(8))),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF8 CIF")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(PLATFORM_NAME)));

SND_SOC_DAILINK_DEFS(audio9,
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, CPU_DAI_NAME(9))),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF9 CIF")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(PLATFORM_NAME)));

SND_SOC_DAILINK_DEFS(audio10,
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, CPU_DAI_NAME(10))),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF10 CIF")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(PLATFORM_NAME)));

SND_SOC_DAILINK_DEFS(audio11,
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, CPU_DAI_NAME(11))),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF11 CIF")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(PLATFORM_NAME)));

SND_SOC_DAILINK_DEFS(audio12,
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, CPU_DAI_NAME(12))),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF12 CIF")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(PLATFORM_NAME)));

SND_SOC_DAILINK_DEFS(audio13,
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, CPU_DAI_NAME(13))),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF13 CIF")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(PLATFORM_NAME)));

SND_SOC_DAILINK_DEFS(audio14,
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, CPU_DAI_NAME(14))),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF14 CIF")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(PLATFORM_NAME)));

SND_SOC_DAILINK_DEFS(audio15,
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, CPU_DAI_NAME(15))),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF15 CIF")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(PLATFORM_NAME)));

SND_SOC_DAILINK_DEFS(audio16,
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, CPU_DAI_NAME(16))),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF16 CIF")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(PLATFORM_NAME)));

SND_SOC_DAILINK_DEFS(audio17,
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, CPU_DAI_NAME(17))),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF17 CIF")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(PLATFORM_NAME)));

SND_SOC_DAILINK_DEFS(audio18,
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, CPU_DAI_NAME(18))),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF18 CIF")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(PLATFORM_NAME)));

SND_SOC_DAILINK_DEFS(audio19,
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, CPU_DAI_NAME(19))),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF19 CIF")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(PLATFORM_NAME)));

SND_SOC_DAILINK_DEFS(audio20,
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, CPU_DAI_NAME(20))),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF20 CIF")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM(PLATFORM_NAME)));


SND_SOC_DAILINK_DEFS(adsp_admaif1,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP-ADMAIF1")),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF1 CIF")));

SND_SOC_DAILINK_DEFS(adsp_admaif2,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP-ADMAIF2")),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF2 CIF")));

SND_SOC_DAILINK_DEFS(adsp_admaif3,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP-ADMAIF3")),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF3 CIF")));

SND_SOC_DAILINK_DEFS(adsp_admaif4,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP-ADMAIF4")),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF4 CIF")));

SND_SOC_DAILINK_DEFS(adsp_admaif5,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP-ADMAIF5")),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF5 CIF")));

SND_SOC_DAILINK_DEFS(adsp_admaif6,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP-ADMAIF6")),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF6 CIF")));

SND_SOC_DAILINK_DEFS(adsp_admaif7,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP-ADMAIF7")),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF7 CIF")));

SND_SOC_DAILINK_DEFS(adsp_admaif8,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP-ADMAIF8")),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF8 CIF")));

SND_SOC_DAILINK_DEFS(adsp_admaif9,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP-ADMAIF9")),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF9 CIF")));

SND_SOC_DAILINK_DEFS(adsp_admaif10,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP-ADMAIF10")),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF10 CIF")));

SND_SOC_DAILINK_DEFS(adsp_admaif11,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP-ADMAIF11")),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF11 CIF")));

SND_SOC_DAILINK_DEFS(adsp_admaif12,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP-ADMAIF12")),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF12 CIF")));

SND_SOC_DAILINK_DEFS(adsp_admaif13,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP-ADMAIF13")),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF13 CIF")));

SND_SOC_DAILINK_DEFS(adsp_admaif14,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP-ADMAIF14")),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF14 CIF")));

SND_SOC_DAILINK_DEFS(adsp_admaif15,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP-ADMAIF15")),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF15 CIF")));

SND_SOC_DAILINK_DEFS(adsp_admaif16,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP-ADMAIF16")),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF16 CIF")));

SND_SOC_DAILINK_DEFS(adsp_admaif17,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP-ADMAIF17")),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF17 CIF")));

SND_SOC_DAILINK_DEFS(adsp_admaif18,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP-ADMAIF18")),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF18 CIF")));

SND_SOC_DAILINK_DEFS(adsp_admaif19,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP-ADMAIF19")),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF19 CIF")));

SND_SOC_DAILINK_DEFS(adsp_admaif20,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP-ADMAIF20")),
	DAILINK_COMP_ARRAY(COMP_CODEC(LINK_CPU_NAME, "ADMAIF20 CIF")));


SND_SOC_DAILINK_DEFS(adsp_pcm1,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP PCM1")),
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt", "ADSP-FE1")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("tegra210-adsp-virt")));

SND_SOC_DAILINK_DEFS(adsp_pcm2,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP PCM2")),
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt", "ADSP-FE2")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("tegra210-adsp-virt")));

SND_SOC_DAILINK_DEFS(adsp_pcm3,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP PCM3")),
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt", "ADSP-FE3")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("tegra210-adsp-virt")));

SND_SOC_DAILINK_DEFS(adsp_pcm4,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP PCM4")),
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt", "ADSP-FE4")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("tegra210-adsp-virt")));

SND_SOC_DAILINK_DEFS(adsp_pcm5,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP PCM5")),
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt", "ADSP-FE5")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("tegra210-adsp-virt")));

SND_SOC_DAILINK_DEFS(adsp_pcm6,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP PCM6")),
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt", "ADSP-FE6")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("tegra210-adsp-virt")));

SND_SOC_DAILINK_DEFS(adsp_pcm7,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP PCM7")),
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt", "ADSP-FE7")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("tegra210-adsp-virt")));

SND_SOC_DAILINK_DEFS(adsp_pcm8,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP PCM8")),
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt", "ADSP-FE8")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("tegra210-adsp-virt")));

SND_SOC_DAILINK_DEFS(adsp_pcm9,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP PCM9")),
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt", "ADSP-FE9")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("tegra210-adsp-virt")));

SND_SOC_DAILINK_DEFS(adsp_pcm10,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP PCM10")),
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt", "ADSP-FE10")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("tegra210-adsp-virt")));

SND_SOC_DAILINK_DEFS(adsp_pcm11,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP PCM11")),
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt", "ADSP-FE11")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("tegra210-adsp-virt")));

SND_SOC_DAILINK_DEFS(adsp_pcm12,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP PCM12")),
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt", "ADSP-FE12")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("tegra210-adsp-virt")));

SND_SOC_DAILINK_DEFS(adsp_pcm13,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP PCM13")),
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt", "ADSP-FE13")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("tegra210-adsp-virt")));

SND_SOC_DAILINK_DEFS(adsp_pcm14,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP PCM14")),
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt", "ADSP-FE14")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("tegra210-adsp-virt")));

SND_SOC_DAILINK_DEFS(adsp_pcm15,
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt","ADSP PCM15")),
	DAILINK_COMP_ARRAY(COMP_CODEC("tegra210-adsp-virt", "ADSP-FE15")),
	DAILINK_COMP_ARRAY(COMP_PLATFORM("tegra210-adsp-virt")));

static struct snd_soc_dai_link tegra_virt_t186ref_pcm_links[] = {
	{
		/* 0 */
		.name = DAI_NAME(1),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(audio1),
	},
	{
		/* 1 */
		.name = DAI_NAME(2),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(audio2),
	},
	{
		/* 2 */
		.name = DAI_NAME(3),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(audio3),
	},
	{
		/* 3 */
		.name = DAI_NAME(4),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(audio4),
	},
	{
		/* 4 */
		.name = DAI_NAME(5),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(audio5),
	},
	{
		/* 5 */
		.name = DAI_NAME(6),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(audio6),
	},
	{
		/* 6 */
		.name = DAI_NAME(7),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(audio7),
	},
	{
		/* 7 */
		.name = DAI_NAME(8),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(audio8),
	},
	{
		/* 8 */
		.name = DAI_NAME(9),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(audio9),
	},
	{
		/* 9 */
		.name = DAI_NAME(10),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(audio10),
	},
	{
		/* 10 */
		.name = DAI_NAME(11),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(audio11),
	},
	{
		/* 11 */
		.name = DAI_NAME(12),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(audio12),
	},
	{
		/* 12 */
		.name = DAI_NAME(13),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(audio13),
	},
	{
		/* 13 */
		.name = DAI_NAME(14),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(audio14),
	},
	{
		/* 14 */
		.name = DAI_NAME(15),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(audio15),
	},
	{
		/* 15 */
		.name = DAI_NAME(16),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(audio16),
	},
	{
		/* 16 */
		.name = DAI_NAME(17),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(audio17),
	},
	{
		/* 17 */
		.name = DAI_NAME(18),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(audio18),
	},
	{
		/* 18 */
		.name = DAI_NAME(19),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(audio19),
	},
	{
		/* 19 */
		.name = DAI_NAME(20),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(audio20),
	},
	{
		/* 20 */
		.name = "ADSP ADMAIF1",
		.stream_name = "ADSP AFMAIF1",
		.params = &adsp_default_params,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(adsp_admaif1),
	},
	{
		/* 21 */
		.name = "ADSP ADMAIF2",
		.stream_name = "ADSP AFMAIF2",
		.params = &adsp_default_params,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(adsp_admaif2),
	},
	{
		/* 22 */
		.name = "ADSP ADMAIF3",
		.stream_name = "ADSP AFMAIF3",
		.params = &adsp_default_params,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(adsp_admaif3),
	},
	{
		/* 23 */
		.name = "ADSP ADMAIF4",
		.stream_name = "ADSP AFMAIF4",
		.params = &adsp_default_params,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(adsp_admaif4),
	},
	{
		/* 24 */
		.name = "ADSP ADMAIF5",
		.stream_name = "ADSP AFMAIF5",
		.params = &adsp_default_params,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(adsp_admaif5),
	},
	{
		/* 25 */
		.name = "ADSP ADMAIF6",
		.stream_name = "ADSP AFMAIF6",
		.params = &adsp_default_params,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(adsp_admaif6),
	},
	{
		/* 26 */
		.name = "ADSP ADMAIF7",
		.stream_name = "ADSP AFMAIF7",
		.params = &adsp_default_params,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(adsp_admaif7),
	},
	{
		/* 27 */
		.name = "ADSP ADMAIF8",
		.stream_name = "ADSP AFMAIF8",
		.params = &adsp_default_params,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(adsp_admaif8),
	},
	{
		/* 28 */
		.name = "ADSP ADMAIF9",
		.stream_name = "ADSP AFMAIF9",
		.params = &adsp_default_params,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(adsp_admaif9),
	},
	{
		/* 29 */
		.name = "ADSP ADMAIF10",
		.stream_name = "ADSP AFMAIF10",
		.params = &adsp_default_params,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(adsp_admaif10),
	},
	{
		/* 30 */
		.name = "ADSP ADMAIF11",
		.stream_name = "ADSP AFMAIF11",
		.params = &adsp_default_params,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(adsp_admaif11),
	},
	{
		/* 31 */
		.name = "ADSP ADMAIF12",
		.stream_name = "ADSP AFMAIF12",
		.params = &adsp_default_params,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(adsp_admaif12),
	},
	{
		/* 32 */
		.name = "ADSP ADMAIF13",
		.stream_name = "ADSP AFMAIF13",
		.params = &adsp_default_params,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(adsp_admaif13),
	},
	{
		/* 33 */
		.name = "ADSP ADMAIF14",
		.stream_name = "ADSP AFMAIF14",
		.params = &adsp_default_params,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(adsp_admaif14),
	},
	{
		/* 34 */
		.name = "ADSP ADMAIF15",
		.stream_name = "ADSP AFMAIF15",
		.params = &adsp_default_params,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(adsp_admaif15),
	},
	{
		/* 35 */
		.name = "ADSP ADMAIF16",
		.stream_name = "ADSP AFMAIF16",
		.params = &adsp_default_params,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(adsp_admaif16),
	},
	{
		/* 36 */
		.name = "ADSP ADMAIF17",
		.stream_name = "ADSP AFMAIF17",
		.params = &adsp_default_params,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(adsp_admaif17),
	},
	{
		/* 37 */
		.name = "ADSP ADMAIF18",
		.stream_name = "ADSP AFMAIF18",
		.params = &adsp_default_params,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(adsp_admaif18),
	},
	{
		/* 38 */
		.name = "ADSP ADMAIF19",
		.stream_name = "ADSP AFMAIF19",
		.params = &adsp_default_params,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(adsp_admaif19),
	},
	{
		/* 39 */
		.name = "ADSP ADMAIF20",
		.stream_name = "ADSP AFMAIF20",
		.params = &adsp_default_params,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(adsp_admaif20),
	},
	{
		/* 40 */
		.name = "ADSP PCM1",
		.stream_name = "ADSP PCM1",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(adsp_pcm1),
	},
	{
		/* 41 */
		.name = "ADSP PCM2",
		.stream_name = "ADSP PCM2",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(adsp_pcm2),
	},
	{
		.name = "ADSP PCM3",
		.stream_name = "ADSP PCM3",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(adsp_pcm3),
	},
	{
		.name = "ADSP PCM4",
		.stream_name = "ADSP PCM4",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(adsp_pcm4),
	},
	{
		.name = "ADSP PCM5",
		.stream_name = "ADSP PCM5",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(adsp_pcm5),
	},
	{
		.name = "ADSP PCM6",
		.stream_name = "ADSP PCM6",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(adsp_pcm6),
	},
	{
		.name = "ADSP PCM7",
		.stream_name = "ADSP PCM7",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(adsp_pcm7),
	},
	{
		.name = "ADSP PCM8",
		.stream_name = "ADSP PCM8",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(adsp_pcm8),
	},
	{
		.name = "ADSP PCM9",
		.stream_name = "ADSP PCM9",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(adsp_pcm9),
	},
	{
		.name = "ADSP PCM10",
		.stream_name = "ADSP PCM10",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(adsp_pcm10),
	},
	{
		.name = "ADSP PCM11",
		.stream_name = "ADSP PCM11",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(adsp_pcm11),
	},
	{
		.name = "ADSP PCM12",
		.stream_name = "ADSP PCM12",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(adsp_pcm12),
	},
	{
		.name = "ADSP PCM13",
		.stream_name = "ADSP PCM13",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(adsp_pcm13),
	},
	{
		.name = "ADSP PCM14",
		.stream_name = "ADSP PCM14",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(adsp_pcm14),
	},
	{
		.name = "ADSP PCM15",
		.stream_name = "ADSP PCM15",
		.ignore_pmdown_time = 1,
		.ignore_suspend = 0,
		SND_SOC_DAILINK_REG(adsp_pcm15),
	},
};

static struct snd_soc_dai_link tegra_virt_t210ref_pcm_links[] = {
	{
		/* 0 */
		.name = DAI_NAME(1),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(audio1),
	},
	{
		/* 1 */
		.name = DAI_NAME(2),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(audio2),
	},
	{
		/* 2 */
		.name = DAI_NAME(3),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(audio3),
	},
	{
		/* 3 */
		.name = DAI_NAME(4),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(audio4),
	},
	{
		/* 4 */
		.name = DAI_NAME(5),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(audio5),
	},
	{
		/* 5 */
		.name = DAI_NAME(6),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(audio6),
	},
	{
		/* 6 */
		.name = DAI_NAME(7),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(audio7),
	},
	{
		/* 7 */
		.name = DAI_NAME(8),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(audio8),
	},
	{
		/* 8 */
		.name = DAI_NAME(9),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(audio9),
	},
	{
		/* 9 */
		.name = DAI_NAME(10),
		.stream_name = STREAM_NAME,
		.params = &default_params,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		SND_SOC_DAILINK_REG(audio10),
	},
};

void tegra_virt_machine_set_num_dai_links(unsigned int val)
{
	num_dai_links = val;
}
EXPORT_SYMBOL(tegra_virt_machine_set_num_dai_links);

unsigned int tegra_virt_machine_get_num_dai_links(void)
{
	return num_dai_links;
}
EXPORT_SYMBOL(tegra_virt_machine_get_num_dai_links);

struct snd_soc_dai_link *tegra_virt_machine_get_dai_link(void)
{
	struct snd_soc_dai_link *link = tegra_virt_t186ref_pcm_links;
	unsigned int size = TEGRA186_XBAR_DAI_LINKS;

	if (of_machine_is_compatible("nvidia,tegra210")) {
		link = tegra_virt_t210ref_pcm_links;
		size = TEGRA210_XBAR_DAI_LINKS;
	}

	tegra_virt_machine_set_num_dai_links(size);
	return link;
}
EXPORT_SYMBOL(tegra_virt_machine_get_dai_link);

void tegra_virt_machine_set_adsp_admaif_dai_params(
		uint32_t id, struct snd_soc_pcm_stream *params)
{
	struct snd_soc_dai_link *link = tegra_virt_t186ref_pcm_links;

	/* Check for valid ADSP ADMAIF ID */
	if (id >= MAX_ADMAIF_IDS) {
		pr_err("Invalid ADSP ADMAIF ID: %d\n", id);
		return;
	}

	/* Find DAI link corresponding to ADSP ADMAIF */
	link += id + MAX_ADMAIF_IDS;

	memcpy(&adsp_admaif_params[id], params,
		sizeof(struct snd_soc_pcm_stream));

	link->params = &adsp_admaif_params[id];
}
EXPORT_SYMBOL(tegra_virt_machine_set_adsp_admaif_dai_params);

MODULE_AUTHOR("Dipesh Gandhi <dipeshg@nvidia.com>");
MODULE_DESCRIPTION("Tegra Virt ASoC machine code");
MODULE_LICENSE("GPL");
