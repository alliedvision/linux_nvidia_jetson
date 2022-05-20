/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _DT_BINDINGS_GPIO_TEGRA234_GPIO_H
#define _DT_BINDINGS_GPIO_TEGRA234_GPIO_H

#include <dt-bindings/gpio/gpio.h>

/* GPIOs implemented by main GPIO controller */
#define TEGRA234_MAIN_GPIO_PORT_A 0
#define TEGRA234_MAIN_GPIO_PORT_B 1
#define TEGRA234_MAIN_GPIO_PORT_C 2
#define TEGRA234_MAIN_GPIO_PORT_D 3
#define TEGRA234_MAIN_GPIO_PORT_E 4
#define TEGRA234_MAIN_GPIO_PORT_F 5
#define TEGRA234_MAIN_GPIO_PORT_G 6
#define TEGRA234_MAIN_GPIO_PORT_H 7
#define TEGRA234_MAIN_GPIO_PORT_I 8
#define TEGRA234_MAIN_GPIO_PORT_J 9
#define TEGRA234_MAIN_GPIO_PORT_K 10
#define TEGRA234_MAIN_GPIO_PORT_L 11
#define TEGRA234_MAIN_GPIO_PORT_M 12
#define TEGRA234_MAIN_GPIO_PORT_N 13
#define TEGRA234_MAIN_GPIO_PORT_O 14
#define TEGRA234_MAIN_GPIO_PORT_P 15
#define TEGRA234_MAIN_GPIO_PORT_Q 16
#define TEGRA234_MAIN_GPIO_PORT_R 17
#define TEGRA234_MAIN_GPIO_PORT_S 18
#define TEGRA234_MAIN_GPIO_PORT_T 19
#define TEGRA234_MAIN_GPIO_PORT_U 20
#define TEGRA234_MAIN_GPIO_PORT_V 21
#define TEGRA234_MAIN_GPIO_PORT_W 22
#define TEGRA234_MAIN_GPIO_PORT_X 23
#define TEGRA234_MAIN_GPIO_PORT_Y 24
#define TEGRA234_MAIN_GPIO_PORT_Z 25
#define TEGRA234_MAIN_GPIO_PORT_AB 26
#define TEGRA234_MAIN_GPIO_PORT_AC 27
#define TEGRA234_MAIN_GPIO_PORT_AD 28
#define TEGRA234_MAIN_GPIO_PORT_AE 29
#define TEGRA234_MAIN_GPIO_PORT_AF 30
#define TEGRA234_MAIN_GPIO_PORT_AG 31
#define TEGRA234_MAIN_GPIO_PORT_FF 32

#define TEGRA234_MAIN_GPIO(port, offset) \
		((TEGRA234_MAIN_GPIO_PORT_##port * 8) + offset)

/* GPIOs implemented by AON GPIO controller */
#define TEGRA234_AON_GPIO_PORT_AA 0
#define TEGRA234_AON_GPIO_PORT_BB 1
#define TEGRA234_AON_GPIO_PORT_CC 2
#define TEGRA234_AON_GPIO_PORT_DD 3
#define TEGRA234_AON_GPIO_PORT_EE 4
#define TEGRA234_AON_GPIO_PORT_GG 5
#define TEGRA234_AON_GPIO_PORT_HH 6

#define TEGRA234_AON_GPIO(port, offset) \
		((TEGRA234_AON_GPIO_PORT_##port * 8) + offset)

/* All pins */
#define TEGRA_PIN_BASE_ID_A 0
#define TEGRA_PIN_BASE_ID_B 1
#define TEGRA_PIN_BASE_ID_C 2
#define TEGRA_PIN_BASE_ID_D 3
#define TEGRA_PIN_BASE_ID_E 4
#define TEGRA_PIN_BASE_ID_F 5
#define TEGRA_PIN_BASE_ID_G 6
#define TEGRA_PIN_BASE_ID_H 7
#define TEGRA_PIN_BASE_ID_I 8
#define TEGRA_PIN_BASE_ID_J 9
#define TEGRA_PIN_BASE_ID_K 10
#define TEGRA_PIN_BASE_ID_L 11
#define TEGRA_PIN_BASE_ID_M 12
#define TEGRA_PIN_BASE_ID_N 13
#define TEGRA_PIN_BASE_ID_O 14
#define TEGRA_PIN_BASE_ID_P 15
#define TEGRA_PIN_BASE_ID_Q 16
#define TEGRA_PIN_BASE_ID_R 17
#define TEGRA_PIN_BASE_ID_S 18
#define TEGRA_PIN_BASE_ID_T 19
#define TEGRA_PIN_BASE_ID_U 20
#define TEGRA_PIN_BASE_ID_V 21
#define TEGRA_PIN_BASE_ID_W 22
#define TEGRA_PIN_BASE_ID_X 23
#define TEGRA_PIN_BASE_ID_Y 24
#define TEGRA_PIN_BASE_ID_Z 25
#define TEGRA_PIN_BASE_ID_AA 26
#define TEGRA_PIN_BASE_ID_BB 27
#define TEGRA_PIN_BASE_ID_CC 28
#define TEGRA_PIN_BASE_ID_DD 29
#define TEGRA_PIN_BASE_ID_EE 30
#define TEGRA_PIN_BASE_ID_FF 31
#define TEGRA_PIN_BASE_ID_GG 32
#define TEGRA_PIN_BASE_ID_HH 33
#define TEGRA_PIN_BASE_ID_AB 34
#define TEGRA_PIN_BASE_ID_AC 35
#define TEGRA_PIN_BASE_ID_AD 36
#define TEGRA_PIN_BASE_ID_AE 37
#define TEGRA_PIN_BASE_ID_AF 38
#define TEGRA_PIN_BASE_ID_AG 39

#define TEGRA_PIN_BASE(port) (TEGRA_PIN_BASE_ID_##port * 8)

#define TEGRA234_MAIN_GPIO_RANGE(st, end) \
	((TEGRA234_MAIN_GPIO_PORT_##end - TEGRA234_MAIN_GPIO_PORT_##st + 1) * 8)
#define TEGRA234_MAIN_GPIO_BASE(port) (TEGRA234_MAIN_GPIO_PORT_##port * 8)

#define TEGRA234_AON_GPIO_RANGE(st, end) \
	((TEGRA234_AON_GPIO_PORT_##end - TEGRA234_AON_GPIO_PORT_##st + 1) * 8)
#define TEGRA234_AON_GPIO_BASE(port) (TEGRA234_AON_GPIO_PORT_##port * 8)

#endif

