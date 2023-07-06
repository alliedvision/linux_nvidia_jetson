/*
 * Copyright (c) 2021-2023, NVIDIA CORPORATION.  All rights reserved.
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
 * Definitions for Jetson tegra234-p3737-0000-p3701-0000 board.
 */

#include <dt-bindings/gpio/tegra234-gpio.h>

#define JETSON_COMPATIBLE	"nvidia,p3737-0000+p3701-0000", "nvidia,p3737-0000+p3701-0004", "nvidia,p3737-0000+p3701-0005"

/* SoC function name for clock signal on 40-pin header pin 7 */
#define HDR40_CLK	"extperiph4"
/* SoC function name for I2S interface on 40-pin header pins 12, 35, 38 and 40 */
#define HDR40_I2S	"i2s2"
/* SoC function name for SPI interface on 40-pin header pins 19, 21, 23, 24 and 26 */
#define HDR40_SPI	"spi1"
/* SoC function name for UART interface on 40-pin header pins 8, 10, 11 and 36 */
#define HDR40_UART	"uarta"

/* SoC pin name definitions for 40-pin header */
#define HDR40_PIN7	"soc_gpio33_pq6"
#define HDR40_PIN11	"uart1_rts_pr4"
#define HDR40_PIN12	"soc_gpio41_ph7"
#define HDR40_PIN13	"soc_gpio37_pr0"
#define HDR40_PIN15	"soc_gpio39_pn1"
#define HDR40_PIN16	"can1_stb_pbb0"
#define HDR40_PIN18	"soc_gpio21_ph0"
#define HDR40_PIN19	"spi1_mosi_pz5"
#define HDR40_PIN21	"spi1_miso_pz4"
#define HDR40_PIN22	"soc_gpio23_pp4"
#define HDR40_PIN23	"spi1_sck_pz3"
#define HDR40_PIN24	"spi1_cs0_pz6"
#define HDR40_PIN26	"spi1_cs1_pz7"
#define HDR40_PIN29	"can0_din_paa1"
#define HDR40_PIN31	"can0_dout_paa0"
#define HDR40_PIN32	"can1_en_pbb1"
#define HDR40_PIN33	"can1_dout_paa2"
#define HDR40_PIN35	"soc_gpio44_pi2"
#define HDR40_PIN36	"uart1_cts_pr5"
#define HDR40_PIN37	"can1_din_paa3"
#define HDR40_PIN38	"soc_gpio43_pi1"
#define HDR40_PIN40	"soc_gpio42_pi0"

/* SoC GPIO definitions for 40-pin header */
#define HDR40_PIN7_GPIO		TEGRA_MAIN_GPIO(Q, 6)
#define HDR40_PIN11_GPIO	TEGRA_MAIN_GPIO(R, 4)
#define HDR40_PIN12_GPIO	TEGRA_MAIN_GPIO(H, 7)
#define HDR40_PIN13_GPIO	TEGRA_MAIN_GPIO(R, 0)
#define HDR40_PIN15_GPIO	TEGRA_MAIN_GPIO(N, 1)
#define HDR40_PIN16_GPIO	TEGRA_AON_GPIO(BB, 0)
#define HDR40_PIN18_GPIO	TEGRA_MAIN_GPIO(H, 0)
#define HDR40_PIN19_GPIO	TEGRA_MAIN_GPIO(Z, 5)
#define HDR40_PIN21_GPIO	TEGRA_MAIN_GPIO(Z, 4)
#define HDR40_PIN22_GPIO	TEGRA_MAIN_GPIO(P, 4)
#define HDR40_PIN23_GPIO	TEGRA_MAIN_GPIO(Z, 3)
#define HDR40_PIN24_GPIO	TEGRA_MAIN_GPIO(Z, 6)
#define HDR40_PIN26_GPIO	TEGRA_MAIN_GPIO(Z, 7)
#define HDR40_PIN29_GPIO	TEGRA_AON_GPIO(AA, 1)
#define HDR40_PIN31_GPIO	TEGRA_AON_GPIO(AA, 0)
#define HDR40_PIN32_GPIO	TEGRA_AON_GPIO(BB, 1)
#define HDR40_PIN33_GPIO	TEGRA_AON_GPIO(AA, 2)
#define HDR40_PIN35_GPIO	TEGRA_MAIN_GPIO(I, 2)
#define HDR40_PIN36_GPIO	TEGRA_MAIN_GPIO(R, 5)
#define HDR40_PIN37_GPIO	TEGRA_AON_GPIO(AA, 3)
#define HDR40_PIN38_GPIO	TEGRA_MAIN_GPIO(I, 1)
#define HDR40_PIN40_GPIO	TEGRA_MAIN_GPIO(I, 0)
