/*
 * Copyright (c) 2017-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _DT_BINDINGS_INTERRUPT_TEGRA194_IRQ_H
#define _DT_BINDINGS_INTERRUPT_TEGRA194_IRQ_H

#define TEGRA194_IRQ_I2C1			25
#define TEGRA194_IRQ_I2C2			26
#define TEGRA194_IRQ_I2C3			27
#define TEGRA194_IRQ_I2C4			28
#define TEGRA194_IRQ_I2C5			29
#define TEGRA194_IRQ_I2C6			30
#define TEGRA194_IRQ_I2C7			31
#define TEGRA194_IRQ_I2C8			32
#define TEGRA194_IRQ_I2C9			33
#define TEGRA194_IRQ_UFSHC			44
#define TEGRA194_IRQ_SDMMC1			62
#define TEGRA194_IRQ_SDMMC3			64
#define TEGRA194_IRQ_SDMMC4			65
#define TEGRA194_IRQ_UARTA			112
#define TEGRA194_IRQ_UARTB			113
#define TEGRA194_IRQ_UARTC			114
#define TEGRA194_IRQ_UARTD			115
#define TEGRA194_IRQ_UARTE			116
#define TEGRA194_IRQ_UARTF			117
#define TEGRA194_IRQ_UARTG			118
#define TEGRA194_IRQ_UARTH			207
#define TEGRA194_IRQ_EQOS_TX0			186
#define TEGRA194_IRQ_EQOS_TX1			187
#define TEGRA194_IRQ_EQOS_TX2			188
#define TEGRA194_IRQ_EQOS_TX3			189
#define TEGRA194_IRQ_EQOS_RX0			190
#define TEGRA194_IRQ_EQOS_RX1			191
#define TEGRA194_IRQ_EQOS_RX2			192
#define TEGRA194_IRQ_EQOS_RX3			193
#define TEGRA194_IRQ_EQOS_COMMON		194
#define TEGRA194_IRQ_EQOS_POWER			195
#define TEGRA194_IRQ_SATA			197
#define TEGRA194_IRQ_ACTMON			210
#define TEGRA194_IRQ_GPIO0_0			288
#define TEGRA194_IRQ_GPIO0_1			289
#define TEGRA194_IRQ_GPIO0_2			290
#define TEGRA194_IRQ_GPIO0_3			291
#define TEGRA194_IRQ_GPIO0_4			292
#define TEGRA194_IRQ_GPIO0_5			293
#define TEGRA194_IRQ_GPIO0_6			294
#define TEGRA194_IRQ_GPIO0_7			295
#define TEGRA194_IRQ_GPIO1_0			296
#define TEGRA194_IRQ_GPIO1_1			297
#define TEGRA194_IRQ_GPIO1_2			298
#define TEGRA194_IRQ_GPIO1_3			299
#define TEGRA194_IRQ_GPIO1_4			300
#define TEGRA194_IRQ_GPIO1_5			301
#define TEGRA194_IRQ_GPIO1_6			302
#define TEGRA194_IRQ_GPIO1_7			303
#define TEGRA194_IRQ_GPIO2_0			304
#define TEGRA194_IRQ_GPIO2_1			305
#define TEGRA194_IRQ_GPIO2_2			306
#define TEGRA194_IRQ_GPIO2_3			307
#define TEGRA194_IRQ_GPIO2_4			308
#define TEGRA194_IRQ_GPIO2_5			309
#define TEGRA194_IRQ_GPIO2_6			310
#define TEGRA194_IRQ_GPIO2_7			311
#define TEGRA194_IRQ_GPIO3_0			312
#define TEGRA194_IRQ_GPIO3_1			313
#define TEGRA194_IRQ_GPIO3_2			314
#define TEGRA194_IRQ_GPIO3_3			315
#define TEGRA194_IRQ_GPIO3_4			316
#define TEGRA194_IRQ_GPIO3_5			317
#define TEGRA194_IRQ_GPIO3_6			318
#define TEGRA194_IRQ_GPIO3_7			319
#define TEGRA194_IRQ_GPIO4_0			320
#define TEGRA194_IRQ_GPIO4_1			321
#define TEGRA194_IRQ_GPIO4_2			322
#define TEGRA194_IRQ_GPIO4_3			323
#define TEGRA194_IRQ_GPIO4_4			324
#define TEGRA194_IRQ_GPIO4_5			325
#define TEGRA194_IRQ_GPIO4_6			326
#define TEGRA194_IRQ_GPIO4_7			327
#define TEGRA194_IRQ_GPIO5_0			328
#define TEGRA194_IRQ_GPIO5_1			329
#define TEGRA194_IRQ_GPIO5_2			330
#define TEGRA194_IRQ_GPIO5_3			331
#define TEGRA194_IRQ_GPIO5_4			332
#define TEGRA194_IRQ_GPIO5_5			333
#define TEGRA194_IRQ_GPIO5_6			334
#define TEGRA194_IRQ_GPIO5_7			335
#define TEGRA194_IRQ_AON_GPIO_0			56
#define TEGRA194_IRQ_AON_GPIO_1			57
#define TEGRA194_IRQ_AON_GPIO_2			58
#define TEGRA194_IRQ_AON_GPIO_3			59

#define TEGRA194_IRQ_BPMP_WDT_REMOTE		14
#define TEGRA194_IRQ_SPE_WDT_REMOTE		15
#define TEGRA194_IRQ_SCE_WDT_REMOTE		16
#define TEGRA194_IRQ_TOP_WDT_REMOTE		17
#define TEGRA194_IRQ_AOWDT_REMOTE		18
#define TEGRA194_IRQ_RCE_WDT_REMOTE		19
#define TEGRA194_IRQ_APE_WDT_REMOTE		20

#define TEGRA194_IRQ_TOP0_HSP_SHARED_0		120
#define TEGRA194_IRQ_TOP0_HSP_SHARED_1		121
#define TEGRA194_IRQ_TOP0_HSP_SHARED_2		122
#define TEGRA194_IRQ_TOP0_HSP_SHARED_3		123
#define TEGRA194_IRQ_TOP0_HSP_SHARED_4		124
#define TEGRA194_IRQ_TOP0_HSP_SHARED_5		125
#define TEGRA194_IRQ_TOP0_HSP_SHARED_6		126
#define TEGRA194_IRQ_TOP0_HSP_SHARED_7		127
#define TEGRA194_IRQ_TOP1_HSP_SHARED_0		128
#define TEGRA194_IRQ_TOP1_HSP_SHARED_1		129
#define TEGRA194_IRQ_TOP1_HSP_SHARED_2		130
#define TEGRA194_IRQ_TOP1_HSP_SHARED_3		131
#define TEGRA194_IRQ_TOP1_HSP_SHARED_4		132
#define TEGRA194_IRQ_AON_HSP_SHARED_1		133
#define TEGRA194_IRQ_AON_HSP_SHARED_2		134
#define TEGRA194_IRQ_AON_HSP_SHARED_3		135
#define TEGRA194_IRQ_AON_HSP_SHARED_4		136
#define TEGRA194_IRQ_BPMP_HSP_SHARED_1		137
#define TEGRA194_IRQ_BPMP_HSP_SHARED_2		138
#define TEGRA194_IRQ_BPMP_HSP_SHARED_3		139
#define TEGRA194_IRQ_BPMP_HSP_SHARED_4		140
#define TEGRA194_IRQ_SCE_HSP_SHARED_1		141
#define TEGRA194_IRQ_SCE_HSP_SHARED_2		142
#define TEGRA194_IRQ_SCE_HSP_SHARED_3		143
#define TEGRA194_IRQ_SCE_HSP_SHARED_4		144
#define TEGRA194_IRQ_RCE_HSP_SHARED_1		182
#define TEGRA194_IRQ_RCE_HSP_SHARED_2		183
#define TEGRA194_IRQ_RCE_HSP_SHARED_3		184
#define TEGRA194_IRQ_RCE_HSP_SHARED_4		185

#define TEGRA194_IRQ_PMIC_EXT_INTR		209
#endif
