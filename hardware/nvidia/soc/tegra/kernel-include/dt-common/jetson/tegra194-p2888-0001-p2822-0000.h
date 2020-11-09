// SPDX-License-Identifier: GPL-2.0-only
/*
 * Definitions for Jetson tegra194-p2888-0001-p2822-0000 board.
 *
 * Copyright (c) 2019 NVIDIA CORPORATION. All rights reserved.
 *
 */

#define JETSON_COMPATIBLE	"nvidia,p2822-0000+p2888-0001"

/* SoC function name for clock signal on 40-pin header pin 7 */
#define HDR40_CLK	"extperiph4"
/* SoC function name for I2S interface on 40-pin header pins 12, 35, 38 and 40 */
#define HDR40_I2S	"i2s2"
/* SoC function name for SPI interface on 40-pin header pins 19, 21, 23, 24 and 26 */
#define HDR40_SPI	"spi1"
/* SoC function name for UART interface on 40-pin header pins 8, 10, 11 and 36 */
#define HDR40_UART	"uarta"

/* SoC pin name definitions for 40-pin header */
#define HDR40_PIN7	"soc_gpio42_pq6"
#define HDR40_PIN11	"uart1_rts_pr4"
#define HDR40_PIN12	"dap2_sclk_ph7"
#define HDR40_PIN13	"soc_gpio44_pr0"
#define HDR40_PIN15	"soc_gpio54_pn1"
#define HDR40_PIN16	"can1_stb_pbb0"
#define HDR40_PIN18	"soc_gpio12_ph0"
#define HDR40_PIN19	"spi1_mosi_pz5"
#define HDR40_PIN21	"spi1_miso_pz4"
#define HDR40_PIN22	"soc_gpio21_pq1"
#define HDR40_PIN23	"spi1_sck_pz3"
#define HDR40_PIN24	"spi1_cs0_pz6"
#define HDR40_PIN26	"spi1_cs1_pz7"
#define HDR40_PIN29	"can0_din_paa3"
#define HDR40_PIN31	"can0_dout_paa2"
#define HDR40_PIN32	"can1_en_pbb1"
#define HDR40_PIN33	"can1_dout_paa0"
#define HDR40_PIN35	"dap2_fs_pi2"
#define HDR40_PIN36	"uart1_cts_pr5"
#define HDR40_PIN37	"can1_din_paa1"
#define HDR40_PIN38	"dap2_din_pi1"
#define HDR40_PIN40	"dap2_dout_pi0"
