/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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

/*
 * Author: Sameer Pujar <spujar@nvidia.com>
 *
 * This header provides macros for Tegra ASoC sound bindings.
 */

#ifndef __DT_TEGRA_ASOC_DAIS_H
#define __DT_TEGRA_ASOC_DAIS_H

/*
 * DAI links can have one of these value
 * PCM_LINK	: optional, if nothing is specified link is treated as PCM link
 * COMPR_LINK	: required, if link is used with compress device
 * C2C_LINK	: required, for any other back end codec-to-codec links
 */
#define PCM_LINK	0
#define COMPR_LINK	1
#define C2C_LINK	2

/*
 * Following DAI indices are derived from respective module drivers.
 * Thus below values have to be in sync with the DAI arrays defined
 * in the drivers.
 */

/* common XBAR dais */
#define XBAR_ADMAIF1			0
#define XBAR_ADMAIF2			1
#define XBAR_ADMAIF3			2
#define XBAR_ADMAIF4			3
#define XBAR_ADMAIF5			4
#define XBAR_ADMAIF6			5
#define XBAR_ADMAIF7			6
#define XBAR_ADMAIF8			7
#define XBAR_ADMAIF9			8
#define XBAR_ADMAIF10			9
#define XBAR_I2S1			10
#define XBAR_I2S2			11
#define XBAR_I2S3			12
#define XBAR_I2S4			13
#define XBAR_I2S5			14
#define XBAR_SFC1			15
#define XBAR_SFC2			16
#define XBAR_SFC3			17
#define XBAR_SFC4			18
#define XBAR_MIXER_IF1			19
#define XBAR_MIXER_IF2			20
#define XBAR_MIXER_IF3			21
#define XBAR_MIXER_IF4			22
#define XBAR_MIXER_IF5			23
#define XBAR_MIXER_IF6			24
#define XBAR_MIXER_IF7			25
#define XBAR_MIXER_IF8			26
#define XBAR_MIXER_IF9			27
#define XBAR_MIXER_IF10			28
#define XBAR_AFC1			29
#define XBAR_AFC2			30
#define XBAR_AFC3			31
#define XBAR_AFC4			32
#define XBAR_AFC5			33
#define XBAR_AFC6			34
#define XBAR_OPE1			35
#define XBAR_SPKPROT			36
#define XBAR_MVC1			37
#define XBAR_MVC2			38
#define XBAR_IQC1_1			39
#define XBAR_IQC1_2			40
#define XBAR_IQC2_1			41
#define XBAR_IQC2_2			42
#define XBAR_DMIC1			43
#define XBAR_DMIC2			44
#define XBAR_DMIC3			45
#define XBAR_AMX1_OUT			46
#define XBAR_AMX1_IN1			47
#define XBAR_AMX1_IN2			48
#define XBAR_AMX1_IN3			49
#define XBAR_AMX1_IN4			50
#define XBAR_AMX2_OUT			51
#define XBAR_AMX2_IN1			52
#define XBAR_AMX2_IN2			53
#define XBAR_AMX2_IN3			54
#define XBAR_AMX2_IN4			55
#define XBAR_ADX1_OUT1			56
#define XBAR_ADX1_OUT2			57
#define XBAR_ADX1_OUT3			58
#define XBAR_ADX1_OUT4			59
#define XBAR_ADX1_IN			60
#define XBAR_ADX2_OUT1			61
#define XBAR_ADX2_OUT2			62
#define XBAR_ADX2_OUT3			63
#define XBAR_ADX2_OUT4			64
#define XBAR_ADX2_IN			65
/* Tegra210 specific XBAR DAIs */
#define XBAR_OPE2			66
/* Tegra186 specific XBAR DAIs */
#define XBAR_ADMAIF11			66
#define XBAR_ADMAIF12			67
#define XBAR_ADMAIF13			68
#define XBAR_ADMAIF14			69
#define XBAR_ADMAIF15			70
#define XBAR_ADMAIF16			71
#define XBAR_ADMAIF17			72
#define XBAR_ADMAIF18			73
#define XBAR_ADMAIF19			74
#define XBAR_ADMAIF20			75
#define XBAR_I2S6			76
#define XBAR_AMX3_OUT			77
#define XBAR_AMX3_IN1			78
#define XBAR_AMX3_IN2			79
#define XBAR_AMX3_IN3			80
#define XBAR_AMX3_IN4			81
#define XBAR_AMX4_OUT			82
#define XBAR_AMX4_IN1			83
#define XBAR_AMX4_IN2			84
#define XBAR_AMX4_IN3			85
#define XBAR_AMX4_IN4			86
#define XBAR_ADX3_OUT1			87
#define XBAR_ADX3_OUT2			88
#define XBAR_ADX3_OUT3			89
#define XBAR_ADX3_OUT4			90
#define XBAR_ADX3_IN			91
#define XBAR_ADX4_OUT1			92
#define XBAR_ADX4_OUT2			93
#define XBAR_ADX4_OUT3			94
#define XBAR_ADX4_OUT4			95
#define XBAR_ADX4_IN			96
#define XBAR_DMIC4			97
#define XBAR_ASRC_IF1			98
#define XBAR_ASRC_IF2			99
#define XBAR_ASRC_IF3			100
#define XBAR_ASRC_IF4			101
#define XBAR_ASRC_IF5			102
#define XBAR_ASRC_IF6			103
#define XBAR_ASRC_IF7			104
#define XBAR_ARAD			105
#define XBAR_DSPK1			106
#define XBAR_DSPK2			107

/* common ADMAIF DAIs */
#define ADMAIF1				0
#define ADMAIF2				1
#define ADMAIF3				2
#define ADMAIF4				3
#define ADMAIF5				4
#define ADMAIF6				5
#define ADMAIF7				6
#define ADMAIF8				7
#define ADMAIF9				8
#define ADMAIF10			9
/* Tegra186 specific ADMAIF DAIs */
#define ADMAIF11			10
#define ADMAIF12			11
#define ADMAIF13			12
#define ADMAIF14			13
#define ADMAIF15			14
#define ADMAIF16			15
#define ADMAIF17			16
#define ADMAIF18			17
#define ADMAIF19			18
#define ADMAIF20			19

/*
 * ADMAIF_FIFO: DAIs used for DAI links between ADMAIF and ADSP.
 * Offset depends on the number of ADMAIF channels for a chip.
 * The DAI indices for these are derived from below offsets.
 */
#define TEGRA210_ADMAIF_FIFO_OFFSET	10
#define TEGRA186_ADMAIF_FIFO_OFFSET	20

/*
 * ADMAIF_CIF: DAIs used for codec-to-codec links between ADMAIF and XBAR.
 * Offset depends on the number of ADMAIF channels for a chip.
 * The DAI indices for these are derived from below offsets.
 */
#define TEGRA210_ADMAIF_CIF_OFFSET	20
#define TEGRA186_ADMAIF_CIF_OFFSET	40

/* I2S */
#define I2S_CIF		0
#define I2S_DAP		1
#define I2S_DUMMY	2

/* DMIC */
#define DMIC_CIF	0
#define DMIC_DAP	1
#define DMIC_DUMMY	2

/* DSPK */
#define DSPK_CIF	0
#define DSPK_DAP	1
#define DSPK_DUMMY	4

/* SFC */
#define SFC_IN		0
#define SFC_OUT		1

/* MIXER */
#define MIXER_IN1	0
#define MIXER_IN2	1
#define MIXER_IN3	2
#define MIXER_IN4	3
#define MIXER_IN5	4
#define MIXER_IN6	5
#define MIXER_IN7	6
#define MIXER_IN8	7
#define MIXER_IN9	8
#define MIXER_IN10	9
#define MIXER_OUT1	10
#define MIXER_OUT2	11
#define MIXER_OUT3	12
#define MIXER_OUT4	13
#define MIXER_OUT5	14

/* AFC */
#define AFC_IN		0
#define AFC_OUT		1

/* OPE */
#define OPE_IN		0
#define OPE_OUT		1

/* MVC */
#define MVC_IN		0
#define MVC_OUT		1

/* AMX */
#define AMX_IN1		0
#define AMX_IN2		1
#define AMX_IN3		2
#define AMX_IN4		3
#define AMX_OUT		4

/* ADX */
#define ADX_OUT1	0
#define ADX_OUT2	1
#define ADX_OUT3	2
#define ADX_OUT4	3
#define ADX_IN		4

/* ASRC */
#define ASRC_IN1	0
#define ASRC_IN2	1
#define ASRC_IN3	2
#define ASRC_IN4	3
#define ASRC_IN5	4
#define ASRC_IN6	5
#define ASRC_IN7	6
#define ASRC_OUT1	7
#define ASRC_OUT2	8
#define ASRC_OUT3	9
#define ASRC_OUT4	10
#define ASRC_OUT5	11
#define ASRC_OUT6	12

/* ARAD */
#define ARAD		0

/* ADSP */
#define ADSP_FE1	0
#define ADSP_FE2	1
#define ADSP_FE3	2
#define ADSP_FE4	3
#define ADSP_FE5	4
#define ADSP_FE6	5
#define ADSP_FE7	6
#define ADSP_FE8	7
#define ADSP_FE9	8
#define ADSP_FE10	9
#define ADSP_FE11	10
#define ADSP_FE12	11
#define ADSP_FE13	12
#define ADSP_FE14	13
#define ADSP_FE15	14
#define ADSP_EAVB_CODEC	15
#define ADSP_ADMAIF1	16
#define ADSP_ADMAIF2	17
#define ADSP_ADMAIF3	18
#define ADSP_ADMAIF4	19
#define ADSP_ADMAIF5	20
#define ADSP_ADMAIF6	21
#define ADSP_ADMAIF7	22
#define ADSP_ADMAIF8	23
#define ADSP_ADMAIF9	24
#define ADSP_ADMAIF10	25
#define ADSP_ADMAIF11	26
#define ADSP_ADMAIF12	27
#define ADSP_ADMAIF13	28
#define ADSP_ADMAIF14	29
#define ADSP_ADMAIF15	30
#define ADSP_ADMAIF16	31
#define ADSP_ADMAIF17	32
#define ADSP_ADMAIF18	33
#define ADSP_ADMAIF19	34
#define ADSP_ADMAIF20	35
#define ADSP_PCM1	36
#define ADSP_PCM2	37
#define ADSP_PCM3	38
#define ADSP_PCM4	39
#define ADSP_PCM5	40
#define ADSP_PCM6	41
#define ADSP_PCM7	42
#define ADSP_PCM8	43
#define ADSP_PCM9	44
#define ADSP_PCM10	45
#define ADSP_PCM11	46
#define ADSP_PCM12	47
#define ADSP_PCM13	48
#define ADSP_PCM14	49
#define ADSP_PCM15	50
#define ADSP_COMPR1	51
#define ADSP_COMPR2	52
#define ADSP_EAVB	53

#endif
