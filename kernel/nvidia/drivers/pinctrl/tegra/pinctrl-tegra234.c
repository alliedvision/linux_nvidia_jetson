/*
 * Pinctrl data for the NVIDIA Tegra234 pinmux
 *
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/version.h>
#include "pinctrl-tegra.h"
/*
* Most pins affected by the pinmux can also be GPIOs. Define these first.
* These must match how the GPIO driver names/numbers its pins.
*/
#define T234_PIN_TABLE(fname)							\
	fname(DAP6_SCLK_PA0, dap6_sclk_pa0, _GPIO, 0)				\
	fname(DAP6_DOUT_PA1, dap6_dout_pa1, _GPIO, 1)				\
	fname(DAP6_DIN_PA2, dap6_din_pa2, _GPIO, 2)				\
	fname(DAP6_FS_PA3, dap6_fs_pa3, _GPIO, 3)				\
	fname(DAP4_SCLK_PA4, dap4_sclk_pa4, _GPIO, 4)				\
	fname(DAP4_DOUT_PA5, dap4_dout_pa5, _GPIO, 5)				\
	fname(DAP4_DIN_PA6, dap4_din_pa6, _GPIO, 6)				\
	fname(DAP4_FS_PA7, dap4_fs_pa7, _GPIO, 7)				\
	fname(SOC_GPIO08_PB0, soc_gpio08_pb0, _GPIO, 8)				\
	fname(QSPI0_SCK_PC0, qspi0_sck_pc0, _GPIO, 9)				\
	fname(QSPI0_CS_N_PC1, qspi0_cs_n_pc1, _GPIO, 10)			\
	fname(QSPI0_IO0_PC2, qspi0_io0_pc2, _GPIO, 11)				\
	fname(QSPI0_IO1_PC3, qspi0_io1_pc3, _GPIO, 12)				\
	fname(QSPI0_IO2_PC4, qspi0_io2_pc4, _GPIO, 13)				\
	fname(QSPI0_IO3_PC5, qspi0_io3_pc5, _GPIO, 14)				\
	fname(QSPI1_SCK_PC6, qspi1_sck_pc6, _GPIO, 15)				\
	fname(QSPI1_CS_N_PC7, qspi1_cs_n_pc7, _GPIO, 16)			\
	fname(QSPI1_IO0_PD0, qspi1_io0_pd0, _GPIO, 17)				\
	fname(QSPI1_IO1_PD1, qspi1_io1_pd1, _GPIO, 18)				\
	fname(QSPI1_IO2_PD2, qspi1_io2_pd2, _GPIO, 19)				\
	fname(QSPI1_IO3_PD3, qspi1_io3_pd3, _GPIO, 20)				\
	fname(EQOS_TXC_PE0, eqos_txc_pe0, _GPIO, 21)				\
	fname(EQOS_TD0_PE1, eqos_td0_pe1, _GPIO, 22)				\
	fname(EQOS_TD1_PE2, eqos_td1_pe2, _GPIO, 23)				\
	fname(EQOS_TD2_PE3, eqos_td2_pe3, _GPIO, 24)				\
	fname(EQOS_TD3_PE4, eqos_td3_pe4, _GPIO, 25)				\
	fname(EQOS_TX_CTL_PE5, eqos_tx_ctl_pe5, _GPIO, 26)			\
	fname(EQOS_RD0_PE6, eqos_rd0_pe6, _GPIO, 27)				\
	fname(EQOS_RD1_PE7, eqos_rd1_pe7, _GPIO, 28)				\
	fname(EQOS_RD2_PF0, eqos_rd2_pf0, _GPIO, 29)				\
	fname(EQOS_RD3_PF1, eqos_rd3_pf1, _GPIO, 30)				\
	fname(EQOS_RX_CTL_PF2, eqos_rx_ctl_pf2, _GPIO, 31)			\
	fname(EQOS_RXC_PF3, eqos_rxc_pf3, _GPIO, 32)				\
	fname(EQOS_SMA_MDIO_PF4, eqos_sma_mdio_pf4, _GPIO, 33)			\
	fname(EQOS_SMA_MDC_PF5, eqos_sma_mdc_pf5, _GPIO, 34)			\
	fname(SOC_GPIO13_PG0, soc_gpio13_pg0, _GPIO, 35)			\
	fname(SOC_GPIO14_PG1, soc_gpio14_pg1, _GPIO, 36)			\
	fname(SOC_GPIO15_PG2, soc_gpio15_pg2, _GPIO, 37)			\
	fname(SOC_GPIO16_PG3, soc_gpio16_pg3, _GPIO, 38)			\
	fname(SOC_GPIO17_PG4, soc_gpio17_pg4, _GPIO, 39)			\
	fname(SOC_GPIO18_PG5, soc_gpio18_pg5, _GPIO, 40)			\
	fname(SOC_GPIO19_PG6, soc_gpio19_pg6, _GPIO, 41)			\
	fname(SOC_GPIO20_PG7, soc_gpio20_pg7, _GPIO, 42)			\
	fname(SOC_GPIO21_PH0, soc_gpio21_ph0, _GPIO, 43)			\
	fname(SOC_GPIO22_PH1, soc_gpio22_ph1, _GPIO, 44)			\
	fname(SOC_GPIO06_PH2, soc_gpio06_ph2, _GPIO, 45)			\
	fname(UART4_TX_PH3, uart4_tx_ph3, _GPIO, 46)				\
	fname(UART4_RX_PH4, uart4_rx_ph4, _GPIO, 47)				\
	fname(UART4_RTS_PH5, uart4_rts_ph5, _GPIO, 48)				\
	fname(UART4_CTS_PH6, uart4_cts_ph6, _GPIO, 49)				\
	fname(SOC_GPIO41_PH7, soc_gpio41_ph7, _GPIO, 50)			\
	fname(SOC_GPIO42_PI0, soc_gpio42_pi0, _GPIO, 51)			\
	fname(SOC_GPIO43_PI1, soc_gpio43_pi1, _GPIO, 52)			\
	fname(SOC_GPIO44_PI2, soc_gpio44_pi2, _GPIO, 53)			\
	fname(GEN1_I2C_SCL_PI3, gen1_i2c_scl_pi3, _GPIO, 54)			\
	fname(GEN1_I2C_SDA_PI4, gen1_i2c_sda_pi4, _GPIO, 55)			\
	fname(CPU_PWR_REQ_PI5, cpu_pwr_req_pi5, _GPIO, 56)			\
	fname(SOC_GPIO07_PI6, soc_gpio07_pi6, _GPIO, 57)			\
	fname(SDMMC1_CLK_PJ0, sdmmc1_clk_pj0, _GPIO, 58)			\
	fname(SDMMC1_CMD_PJ1, sdmmc1_cmd_pj1, _GPIO, 59)			\
	fname(SDMMC1_DAT0_PJ2, sdmmc1_dat0_pj2, _GPIO, 60)			\
	fname(SDMMC1_DAT1_PJ3, sdmmc1_dat1_pj3, _GPIO, 61)			\
	fname(SDMMC1_DAT2_PJ4, sdmmc1_dat2_pj4, _GPIO, 62)			\
	fname(SDMMC1_DAT3_PJ5, sdmmc1_dat3_pj5, _GPIO, 63)			\
	fname(PEX_L0_CLKREQ_N_PK0, pex_l0_clkreq_n_pk0, _GPIO, 64)		\
	fname(PEX_L0_RST_N_PK1, pex_l0_rst_n_pk1, _GPIO, 65)			\
	fname(PEX_L1_CLKREQ_N_PK2, pex_l1_clkreq_n_pk2, _GPIO, 66)		\
	fname(PEX_L1_RST_N_PK3, pex_l1_rst_n_pk3, _GPIO, 67)			\
	fname(PEX_L2_CLKREQ_N_PK4, pex_l2_clkreq_n_pk4, _GPIO, 68)		\
	fname(PEX_L2_RST_N_PK5, pex_l2_rst_n_pk5, _GPIO, 69)			\
	fname(PEX_L3_CLKREQ_N_PK6, pex_l3_clkreq_n_pk6, _GPIO, 70)		\
	fname(PEX_L3_RST_N_PK7, pex_l3_rst_n_pk7, _GPIO, 71)			\
	fname(PEX_L4_CLKREQ_N_PL0, pex_l4_clkreq_n_pl0, _GPIO, 72)		\
	fname(PEX_L4_RST_N_PL1, pex_l4_rst_n_pl1, _GPIO, 73)			\
	fname(PEX_WAKE_N_PL2, pex_wake_n_pl2, _GPIO, 74)			\
	fname(SOC_GPIO34_PL3, soc_gpio34_pl3, _GPIO, 75)			\
	fname(DP_AUX_CH0_HPD_PM0, dp_aux_ch0_hpd_pm0, _GPIO, 76)		\
	fname(DP_AUX_CH1_HPD_PM1, dp_aux_ch1_hpd_pm1, _GPIO, 77)		\
	fname(DP_AUX_CH2_HPD_PM2, dp_aux_ch2_hpd_pm2, _GPIO, 78)		\
	fname(DP_AUX_CH3_HPD_PM3, dp_aux_ch3_hpd_pm3, _GPIO, 79)		\
	fname(SOC_GPIO55_PM4, soc_gpio55_pm4, _GPIO, 80)			\
	fname(SOC_GPIO36_PM5, soc_gpio36_pm5, _GPIO, 81)			\
	fname(SOC_GPIO53_PM6, soc_gpio53_pm6, _GPIO, 82)			\
	fname(SOC_GPIO38_PM7, soc_gpio38_pm7, _GPIO, 83)			\
	fname(DP_AUX_CH3_N_PN0, dp_aux_ch3_n_pn0, _GPIO, 84)			\
	fname(SOC_GPIO39_PN1, soc_gpio39_pn1, _GPIO, 85)			\
	fname(SOC_GPIO40_PN2, soc_gpio40_pn2, _GPIO, 86)			\
	fname(DP_AUX_CH1_P_PN3, dp_aux_ch1_p_pn3, _GPIO, 87)			\
	fname(DP_AUX_CH1_N_PN4, dp_aux_ch1_n_pn4, _GPIO, 88)			\
	fname(DP_AUX_CH2_P_PN5, dp_aux_ch2_p_pn5, _GPIO, 89)			\
	fname(DP_AUX_CH2_N_PN6, dp_aux_ch2_n_pn6, _GPIO, 90)			\
	fname(DP_AUX_CH3_P_PN7, dp_aux_ch3_p_pn7, _GPIO, 91)			\
	fname(EXTPERIPH1_CLK_PP0, extperiph1_clk_pp0, _GPIO, 92)		\
	fname(EXTPERIPH2_CLK_PP1, extperiph2_clk_pp1, _GPIO, 93)		\
	fname(CAM_I2C_SCL_PP2, cam_i2c_scl_pp2, _GPIO, 94)			\
	fname(CAM_I2C_SDA_PP3, cam_i2c_sda_pp3, _GPIO, 95)			\
	fname(SOC_GPIO23_PP4, soc_gpio23_pp4, _GPIO, 96)			\
	fname(SOC_GPIO24_PP5, soc_gpio24_pp5, _GPIO, 97)			\
	fname(SOC_GPIO25_PP6, soc_gpio25_pp6, _GPIO, 98)			\
	fname(PWR_I2C_SCL_PP7, pwr_i2c_scl_pp7, _GPIO, 99)			\
	fname(PWR_I2C_SDA_PQ0, pwr_i2c_sda_pq0, _GPIO, 100)			\
	fname(SOC_GPIO28_PQ1, soc_gpio28_pq1, _GPIO, 101)			\
	fname(SOC_GPIO29_PQ2, soc_gpio29_pq2, _GPIO, 102)			\
	fname(SOC_GPIO30_PQ3, soc_gpio30_pq3, _GPIO, 103)			\
	fname(SOC_GPIO31_PQ4, soc_gpio31_pq4, _GPIO, 104)			\
	fname(SOC_GPIO32_PQ5, soc_gpio32_pq5, _GPIO, 105)			\
	fname(SOC_GPIO33_PQ6, soc_gpio33_pq6, _GPIO, 106)			\
	fname(SOC_GPIO35_PQ7, soc_gpio35_pq7, _GPIO, 107)			\
	fname(SOC_GPIO37_PR0, soc_gpio37_pr0, _GPIO, 108)			\
	fname(SOC_GPIO56_PR1, soc_gpio56_pr1, _GPIO, 109)			\
	fname(UART1_TX_PR2, uart1_tx_pr2, _GPIO, 110)				\
	fname(UART1_RX_PR3, uart1_rx_pr3, _GPIO, 111)				\
	fname(UART1_RTS_PR4, uart1_rts_pr4, _GPIO, 112)				\
	fname(UART1_CTS_PR5, uart1_cts_pr5, _GPIO, 113)				\
	fname(GPU_PWR_REQ_PX0, gpu_pwr_req_px0, _GPIO, 114)			\
	fname(CV_PWR_REQ_PX1, cv_pwr_req_px1, _GPIO, 115)			\
	fname(GP_PWM2_PX2, gp_pwm2_px2, _GPIO, 116)				\
	fname(GP_PWM3_PX3, gp_pwm3_px3, _GPIO, 117)				\
	fname(UART2_TX_PX4, uart2_tx_px4, _GPIO, 118)				\
	fname(UART2_RX_PX5, uart2_rx_px5, _GPIO, 119)				\
	fname(UART2_RTS_PX6, uart2_rts_px6, _GPIO, 120)				\
	fname(UART2_CTS_PX7, uart2_cts_px7, _GPIO, 121)				\
	fname(SPI3_SCK_PY0, spi3_sck_py0, _GPIO, 122)				\
	fname(SPI3_MISO_PY1, spi3_miso_py1, _GPIO, 123)				\
	fname(SPI3_MOSI_PY2, spi3_mosi_py2, _GPIO, 124)				\
	fname(SPI3_CS0_PY3, spi3_cs0_py3, _GPIO, 125)				\
	fname(SPI3_CS1_PY4, spi3_cs1_py4, _GPIO, 126)				\
	fname(UART5_TX_PY5, uart5_tx_py5, _GPIO, 127)				\
	fname(UART5_RX_PY6, uart5_rx_py6, _GPIO, 128)				\
	fname(UART5_RTS_PY7, uart5_rts_py7, _GPIO, 129)				\
	fname(UART5_CTS_PZ0, uart5_cts_pz0, _GPIO, 130)				\
	fname(USB_VBUS_EN0_PZ1, usb_vbus_en0_pz1, _GPIO, 131)			\
	fname(USB_VBUS_EN1_PZ2, usb_vbus_en1_pz2, _GPIO, 132)			\
	fname(SPI1_SCK_PZ3, spi1_sck_pz3, _GPIO, 133)				\
	fname(SPI1_MISO_PZ4, spi1_miso_pz4, _GPIO, 134)				\
	fname(SPI1_MOSI_PZ5, spi1_mosi_pz5, _GPIO, 135)				\
	fname(SPI1_CS0_PZ6, spi1_cs0_pz6, _GPIO, 136)				\
	fname(SPI1_CS1_PZ7, spi1_cs1_pz7, _GPIO, 137)				\
	fname(CAN0_DOUT_PAA0, can0_dout_paa0, _GPIO, 138)			\
	fname(CAN0_DIN_PAA1, can0_din_paa1, _GPIO, 139)				\
	fname(CAN1_DOUT_PAA2, can1_dout_paa2, _GPIO, 140)			\
	fname(CAN1_DIN_PAA3, can1_din_paa3, _GPIO, 141)				\
	fname(CAN0_STB_PAA4, can0_stb_paa4, _GPIO, 142)				\
	fname(CAN0_EN_PAA5, can0_en_paa5, _GPIO, 143)				\
	fname(SOC_GPIO49_PAA6, soc_gpio49_paa6, _GPIO, 144)			\
	fname(CAN0_ERR_PAA7, can0_err_paa7, _GPIO, 145)				\
	fname(SPI5_SCK_PAC0, spi5_sck_pac0, _GPIO, 146)				\
	fname(SPI5_MISO_PAC1, spi5_miso_pac1, _GPIO, 147)			\
	fname(SPI5_MOSI_PAC2, spi5_mosi_pac2, _GPIO, 148)			\
	fname(SPI5_CS0_PAC3, spi5_cs0_pac3, _GPIO, 149)				\
	fname(SOC_GPIO57_PAC4, soc_gpio57_pac4, _GPIO, 150)			\
	fname(SOC_GPIO58_PAC5, soc_gpio58_pac5, _GPIO, 151)			\
	fname(SOC_GPIO59_PAC6, soc_gpio59_pac6, _GPIO, 152)			\
	fname(SOC_GPIO60_PAC7, soc_gpio60_pac7, _GPIO, 153)			\
	fname(SOC_GPIO45_PAD0, soc_gpio45_pad0, _GPIO, 154)			\
	fname(SOC_GPIO46_PAD1, soc_gpio46_pad1, _GPIO, 155)			\
	fname(SOC_GPIO47_PAD2, soc_gpio47_pad2, _GPIO, 156)			\
	fname(SOC_GPIO48_PAD3, soc_gpio48_pad3, _GPIO, 157)			\
	fname(UFS0_REF_CLK_PAE0, ufs0_ref_clk_pae0, _GPIO, 158)			\
	fname(UFS0_RST_N_PAE1, ufs0_rst_n_pae1, _GPIO, 159)			\
	fname(PEX_L5_CLKREQ_N_PAF0, pex_l5_clkreq_n_paf0, _GPIO, 160)		\
	fname(PEX_L5_RST_N_PAF1, pex_l5_rst_n_paf1, _GPIO, 161)			\
	fname(PEX_L6_CLKREQ_N_PAF2, pex_l6_clkreq_n_paf2, _GPIO, 162)		\
	fname(PEX_L6_RST_N_PAF3, pex_l6_rst_n_paf3, _GPIO, 163)			\
	fname(PEX_L7_CLKREQ_N_PAG0, pex_l7_clkreq_n_pag0, _GPIO, 164)		\
	fname(PEX_L7_RST_N_PAG1, pex_l7_rst_n_pag1, _GPIO, 165)			\
	fname(PEX_L8_CLKREQ_N_PAG2, pex_l8_clkreq_n_pag2, _GPIO, 166)		\
	fname(PEX_L8_RST_N_PAG3, pex_l8_rst_n_pag3, _GPIO, 167)			\
	fname(PEX_L9_CLKREQ_N_PAG4, pex_l9_clkreq_n_pag4, _GPIO, 168)		\
	fname(PEX_L9_RST_N_PAG5, pex_l9_rst_n_pag5, _GPIO, 169)			\
	fname(PEX_L10_CLKREQ_N_PAG6, pex_l10_clkreq_n_pag6, _GPIO, 170)		\
	fname(PEX_L10_RST_N_PAG7, pex_l10_rst_n_pag7, _GPIO, 171)		\
	fname(CAN1_STB_PBB0, can1_stb_pbb0, _GPIO, 172)				\
	fname(CAN1_EN_PBB1, can1_en_pbb1, _GPIO, 173)				\
	fname(SOC_GPIO50_PBB2, soc_gpio50_pbb2, _GPIO, 174)			\
	fname(CAN1_ERR_PBB3, can1_err_pbb3, _GPIO, 175)				\
	fname(SPI2_SCK_PCC0, spi2_sck_pcc0, _GPIO, 176)				\
	fname(SPI2_MISO_PCC1, spi2_miso_pcc1, _GPIO, 177)			\
	fname(SPI2_MOSI_PCC2, spi2_mosi_pcc2, _GPIO, 178)			\
	fname(SPI2_CS0_PCC3, spi2_cs0_pcc3, _GPIO, 179)				\
	fname(TOUCH_CLK_PCC4, touch_clk_pcc4, _GPIO, 180)			\
	fname(UART3_TX_PCC5, uart3_tx_pcc5, _GPIO, 181)				\
	fname(UART3_RX_PCC6, uart3_rx_pcc6, _GPIO, 182)				\
	fname(GEN2_I2C_SCL_PCC7, gen2_i2c_scl_pcc7, _GPIO, 183)			\
	fname(GEN2_I2C_SDA_PDD0, gen2_i2c_sda_pdd0, _GPIO, 184)			\
	fname(GEN8_I2C_SCL_PDD1, gen8_i2c_scl_pdd1, _GPIO, 185)			\
	fname(GEN8_I2C_SDA_PDD2, gen8_i2c_sda_pdd2, _GPIO, 186)			\
	fname(SCE_ERROR_PEE0, sce_error_pee0, _GPIO, 187)			\
	fname(VCOMP_ALERT_PEE1, vcomp_alert_pee1, _GPIO, 188)			\
	fname(AO_RETENTION_N_PEE2, ao_retention_n_pee2, _GPIO, 189)		\
	fname(BATT_OC_PEE3, batt_oc_pee3, _GPIO, 190)				\
	fname(POWER_ON_PEE4, power_on_pee4, _GPIO, 191)				\
	fname(SOC_GPIO26_PEE5, soc_gpio26_pee5, _GPIO, 192)			\
	fname(SOC_GPIO27_PEE6, soc_gpio27_pee6, _GPIO, 193)			\
	fname(BOOTV_CTL_N_PEE7, bootv_ctl_n_pee7, _GPIO, 194)			\
	fname(HDMI_CEC_PGG0, hdmi_cec_pgg0, _GPIO, 195)				\
	fname(EQOS_COMP, eqos_comp, _PIN, 0)					\
	fname(QSPI_COMP, qspi_comp, _PIN, 1)					\
	fname(SDMMC1_COMP, sdmmc1_comp, _PIN, 2)				\

#define _GPIO(offset)			(offset)
#define NUM_GPIOS			(TEGRA_PIN_HDMI_CEC_PGG0 + 1)
#define _PIN(offset)			(NUM_GPIOS + (offset))

/* Define unique ID for each pins */
#define TEGRA_PINCTRL_PIN_NUM(id, lid, _f, num)		\
	TEGRA_PIN_##id = _f(num),
enum pin_id {
	T234_PIN_TABLE(TEGRA_PINCTRL_PIN_NUM)
};

/* Table for pin descriptor */
#define TEGRA_PINCTRL_PIN(id, lid, f, num)	PINCTRL_PIN(TEGRA_PIN_##id, #id),
static const struct pinctrl_pin_desc tegra234_pins[] = {
	T234_PIN_TABLE(TEGRA_PINCTRL_PIN)
};

#define TEGRA_PINCTRL_PINS_STRUCT(id, lid, f, num)	\
	static const unsigned lid##_pins[] = {		\
		TEGRA_PIN_##id,				\
	};
T234_PIN_TABLE(TEGRA_PINCTRL_PINS_STRUCT)

#define T234_FUNCTION_TABLE(fname)		\
	fname(GP, gp)				\
	fname(UARTC, uartc)			\
	fname(I2C8, i2c8)			\
	fname(SPI2, spi2)			\
	fname(I2C2, i2c2)			\
	fname(CAN1, can1)			\
	fname(CAN0, can0)			\
	fname(RSVD0, rsvd0)			\
	fname(ETH0, eth0)			\
	fname(ETH2, eth2)			\
	fname(ETH1, eth1)			\
	fname(DP, dp)				\
	fname(ETH3, eth3)			\
	fname(I2C4, i2c4)			\
	fname(I2C7, i2c7)			\
	fname(I2C9, i2c9)			\
	fname(EQOS, eqos)			\
	fname(PE2, pe2)				\
	fname(PE1, pe1)				\
	fname(PE0, pe0)				\
	fname(PE3, pe3)				\
	fname(PE4, pe4)				\
	fname(PE5, pe5)				\
	fname(PE6, pe6)				\
	fname(PE10, pe10)			\
	fname(PE7, pe7)				\
	fname(PE8, pe8)				\
	fname(PE9, pe9)				\
	fname(QSPI0, qspi0)			\
	fname(QSPI1, qspi1)			\
	fname(QSPI, qspi)			\
	fname(SDMMC1, sdmmc1)			\
	fname(SCE, sce)				\
	fname(SOC, soc)				\
	fname(GPIO, gpio)			\
	fname(HDMI, hdmi)			\
	fname(UFS0, ufs0)			\
	fname(SPI3, spi3)			\
	fname(SPI1, spi1)			\
	fname(UARTB, uartb)			\
	fname(UARTE, uarte)			\
	fname(USB, usb)				\
	fname(EXTPERIPH2, extperiph2)		\
	fname(EXTPERIPH1, extperiph1)		\
	fname(I2C3, i2c3)			\
	fname(VI0, vi0)				\
	fname(I2C5, i2c5)			\
	fname(UARTA, uarta)			\
	fname(UARTD, uartd)			\
	fname(I2C1, i2c1)			\
	fname(I2S4, i2s4)			\
	fname(I2S6, i2s6)			\
	fname(AUD, aud)				\
	fname(SPI5, spi5)			\
	fname(TOUCH, touch)			\
	fname(UARTJ, uartj)			\
	fname(RSVD1, rsvd1)			\
	fname(WDT, wdt)				\
	fname(TSC, tsc)				\
	fname(DMIC3, dmic3)			\
	fname(LED, led)				\
	fname(VI0_ALT, vi0_alt)			\
	fname(I2S5, i2s5)			\
	fname(NV, nv)				\
	fname(EXTPERIPH3, extperiph3)		\
	fname(EXTPERIPH4, extperiph4)		\
	fname(SPI4, spi4)			\
	fname(CCLA, ccla)			\
	fname(I2S2, i2s2)			\
	fname(I2S1, i2s1)			\
	fname(I2S8, i2s8)			\
	fname(I2S3, i2s3)			\
	fname(RSVD2, rsvd2)			\
	fname(DMIC5, dmic5)			\
	fname(DCA, dca)				\
	fname(DISPLAYB, displayb)		\
	fname(DISPLAYA, displaya)		\
	fname(VI1, vi1)				\
	fname(DCB, dcb)				\
	fname(DMIC1, dmic1)			\
	fname(DMIC4, dmic4)			\
	fname(I2S7, i2s7)			\
	fname(DMIC2, dmic2)			\
	fname(DSPK0, dspk0)			\
	fname(RSVD3, rsvd3)			\
	fname(TSC_ALT, tsc_alt)			\
	fname(ISTCTRL, istctrl)			\
	fname(VI1_ALT, vi1_alt)			\
	fname(DSPK1, dspk1)			\
	fname(IGPU, igpu)			\

/* Define unique ID for each function */
#define TEGRA_PIN_FUNCTION_MUX_ENUM(id, lid)	\
	TEGRA_MUX_##id,
enum tegra_mux_dt {
	T234_FUNCTION_TABLE(TEGRA_PIN_FUNCTION_MUX_ENUM)
};

/* Make list of each function name */
#define TEGRA_PIN_FUNCTION(id, lid)		\
	{					\
		.name = #lid,			\
	},
static struct tegra_function tegra234_functions[] = {
	T234_FUNCTION_TABLE(TEGRA_PIN_FUNCTION)
};

#define PINGROUP_REG_Y(r) ((r))
#define PINGROUP_REG_N(r) -1

#define DRV_PINGROUP_Y(r) ((r))
#define DRV_PINGROUP_N(r) -1

#define DRV_PINGROUP_ENTRY_N(pg_name)				\
		.drv_reg = -1,					\
		.drv_bank = -1,					\
		.drvdn_bit = -1,				\
		.drvup_bit = -1,				\
		.slwr_bit = -1,					\
		.slwf_bit = -1

#define DRV_PINGROUP_ENTRY_Y(r, drvdn_b, drvdn_w, drvup_b,	\
			     drvup_w, slwr_b, slwr_w, slwf_b,	\
			     slwf_w, bank)			\
		.drv_reg = DRV_PINGROUP_Y(r),			\
		.drv_bank = bank,				\
		.drvdn_bit = drvdn_b,				\
		.drvdn_width = drvdn_w,				\
		.drvup_bit = drvup_b,				\
		.drvup_width = drvup_w,				\
		.slwr_bit = slwr_b,				\
		.slwr_width = slwr_w,				\
		.slwf_bit = slwf_b,				\
		.slwf_width = slwf_w

#define PIN_PINGROUP_ENTRY_N(pg_name)				\
		.mux_reg = -1,					\
		.pupd_reg = -1,					\
		.tri_reg = -1,					\
		.lpbk_reg = -1,					\
		.einput_bit = -1,				\
		.e_io_hv_bit = -1,				\
		.odrain_bit = -1,				\
		.lock_bit = -1,					\
		.parked_bit = -1,				\
		.lpmd_bit = -1,					\
		.drvtype_bit = -1,				\
		.lpdr_bit = -1,					\
		.pbias_buf_bit = -1,				\
		.preemp_bit = -1,				\
		.lpbk_bit = -1,					\
		.rfu_in_bit = -1

/* gpio_bit is renamed as sfsel_bit */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 9, 0)
#define PIN_PINGROUP_ENTRY_Y(r, bank, pupd, e_io_hv, e_lpbk, e_input,	\
				e_lpdr, e_pbias_buf, gpio_sfio_sel,	\
				schmitt_b)				\
		.mux_reg = PINGROUP_REG_Y(r),			\
		.lpmd_bit = -1,					\
		.lock_bit = -1,					\
		.hsm_bit = -1,					\
		.mux_bank = bank,				\
		.mux_bit = 0,					\
		.pupd_reg = PINGROUP_REG_##pupd(r),		\
		.pupd_bank = bank,				\
		.pupd_bit = 2,					\
		.tri_reg = PINGROUP_REG_Y(r),			\
		.tri_bank = bank,				\
		.tri_bit = 4,					\
		.einput_bit = e_input,				\
		.gpio_bit = gpio_sfio_sel,			\
		.schmitt_bit = schmitt_b,			\
		.drvtype_bit = 13,				\
		.lpdr_bit = e_lpdr,				\
		.drv_reg = -1,					\
		.lpbk_reg = PINGROUP_REG_Y(r),			\
		.lpbk_bank = bank,				\
		.lpbk_bit = e_lpbk,
#else
#define PIN_PINGROUP_ENTRY_Y(r, bank, pupd, e_io_hv, e_lpbk, e_input,	\
				e_lpdr, e_pbias_buf, gpio_sfio_sel,	\
				schmitt_b)				\
		.mux_reg = PINGROUP_REG_Y(r),			\
		.lpmd_bit = -1,					\
		.lock_bit = -1,					\
		.hsm_bit = -1,					\
		.mux_bank = bank,				\
		.mux_bit = 0,					\
		.pupd_reg = PINGROUP_REG_##pupd(r),		\
		.pupd_bank = bank,				\
		.pupd_bit = 2,					\
		.tri_reg = PINGROUP_REG_Y(r),			\
		.tri_bank = bank,				\
		.tri_bit = 4,					\
		.einput_bit = e_input,				\
		.sfsel_bit = gpio_sfio_sel,			\
		.schmitt_bit = schmitt_b,			\
		.drvtype_bit = 13,				\
		.lpdr_bit = e_lpdr,				\
		.drv_reg = -1,					\
		.lpbk_reg = PINGROUP_REG_Y(r),			\
		.lpbk_bank = bank,				\
		.lpbk_bit = e_lpbk,
#endif

#define	drive_touch_clk_pcc4			DRV_PINGROUP_ENTRY_Y(0x2004,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	1)
#define	drive_uart3_rx_pcc6			DRV_PINGROUP_ENTRY_Y(0x200c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	1)
#define	drive_uart3_tx_pcc5			DRV_PINGROUP_ENTRY_Y(0x2014,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	1)
#define	drive_gen8_i2c_sda_pdd2			DRV_PINGROUP_ENTRY_Y(0x201c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	1)
#define	drive_gen8_i2c_scl_pdd1			DRV_PINGROUP_ENTRY_Y(0x2024,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	1)
#define	drive_spi2_mosi_pcc2			DRV_PINGROUP_ENTRY_Y(0x202c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	1)
#define	drive_gen2_i2c_scl_pcc7			DRV_PINGROUP_ENTRY_Y(0x2034,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	1)
#define	drive_spi2_cs0_pcc3			DRV_PINGROUP_ENTRY_Y(0x203c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	1)
#define	drive_gen2_i2c_sda_pdd0			DRV_PINGROUP_ENTRY_Y(0x2044,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	1)
#define	drive_spi2_sck_pcc0			DRV_PINGROUP_ENTRY_Y(0x204c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	1)
#define	drive_spi2_miso_pcc1			DRV_PINGROUP_ENTRY_Y(0x2054,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	1)
#define	drive_can1_dout_paa2			DRV_PINGROUP_ENTRY_Y(0x3004,	28,	2,	30,	2,	-1,	-1,	-1,	-1,	1)
#define	drive_can1_din_paa3			DRV_PINGROUP_ENTRY_Y(0x300c,	28,	2,	30,	2,	-1,	-1,	-1,	-1,	1)
#define	drive_can0_dout_paa0			DRV_PINGROUP_ENTRY_Y(0x3014,	28,	2,	30,	2,	-1,	-1,	-1,	-1,	1)
#define	drive_can0_din_paa1			DRV_PINGROUP_ENTRY_Y(0x301c,	28,	2,	30,	2,	-1,	-1,	-1,	-1,	1)
#define	drive_can0_stb_paa4			DRV_PINGROUP_ENTRY_Y(0x3024,	28,	2,	30,	2,	-1,	-1,	-1,	-1,	1)
#define	drive_can0_en_paa5			DRV_PINGROUP_ENTRY_Y(0x302c,	28,	2,	30,	2,	-1,	-1,	-1,	-1,	1)
#define	drive_soc_gpio49_paa6			DRV_PINGROUP_ENTRY_Y(0x3034,	28,	2,	30,	2,	-1,	-1,	-1,	-1,	1)
#define	drive_can0_err_paa7			DRV_PINGROUP_ENTRY_Y(0x303c,	28,	2,	30,	2,	-1,	-1,	-1,	-1,	1)
#define	drive_can1_stb_pbb0			DRV_PINGROUP_ENTRY_Y(0x3044,	28,	2,	30,	2,	-1,	-1,	-1,	-1,	1)
#define	drive_can1_en_pbb1			DRV_PINGROUP_ENTRY_Y(0x304c,	28,	2,	30,	2,	-1,	-1,	-1,	-1,	1)
#define	drive_soc_gpio50_pbb2			DRV_PINGROUP_ENTRY_Y(0x3054,	28,	2,	30,	2,	-1,	-1,	-1,	-1,	1)
#define	drive_can1_err_pbb3			DRV_PINGROUP_ENTRY_Y(0x305c,	28,	2,	30,	2,	-1,	-1,	-1,	-1,	1)
#define	drive_soc_gpio08_pb0			DRV_PINGROUP_ENTRY_Y(0x500c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio36_pm5			DRV_PINGROUP_ENTRY_Y(0x10004,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio53_pm6			DRV_PINGROUP_ENTRY_Y(0x1000c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio55_pm4			DRV_PINGROUP_ENTRY_Y(0x10014,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio38_pm7			DRV_PINGROUP_ENTRY_Y(0x1001c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio39_pn1			DRV_PINGROUP_ENTRY_Y(0x10024,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio40_pn2			DRV_PINGROUP_ENTRY_Y(0x1002c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_dp_aux_ch0_hpd_pm0		DRV_PINGROUP_ENTRY_Y(0x10034,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_dp_aux_ch1_hpd_pm1		DRV_PINGROUP_ENTRY_Y(0x1003c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_dp_aux_ch2_hpd_pm2		DRV_PINGROUP_ENTRY_Y(0x10044,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_dp_aux_ch3_hpd_pm3		DRV_PINGROUP_ENTRY_Y(0x1004c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_dp_aux_ch1_p_pn3			DRV_PINGROUP_ENTRY_Y(0x10054,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_dp_aux_ch1_n_pn4			DRV_PINGROUP_ENTRY_Y(0x1005c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_dp_aux_ch2_p_pn5			DRV_PINGROUP_ENTRY_Y(0x10064,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_dp_aux_ch2_n_pn6			DRV_PINGROUP_ENTRY_Y(0x1006c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_dp_aux_ch3_p_pn7			DRV_PINGROUP_ENTRY_Y(0x10074,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_dp_aux_ch3_n_pn0			DRV_PINGROUP_ENTRY_Y(0x1007c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_pex_l2_clkreq_n_pk4		DRV_PINGROUP_ENTRY_Y(0x7004,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_pex_wake_n_pl2			DRV_PINGROUP_ENTRY_Y(0x700c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_pex_l1_clkreq_n_pk2		DRV_PINGROUP_ENTRY_Y(0x7014,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_pex_l1_rst_n_pk3			DRV_PINGROUP_ENTRY_Y(0x701c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_pex_l0_clkreq_n_pk0		DRV_PINGROUP_ENTRY_Y(0x7024,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_pex_l0_rst_n_pk1			DRV_PINGROUP_ENTRY_Y(0x702c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_pex_l2_rst_n_pk5			DRV_PINGROUP_ENTRY_Y(0x7034,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_pex_l3_clkreq_n_pk6		DRV_PINGROUP_ENTRY_Y(0x703c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_pex_l3_rst_n_pk7			DRV_PINGROUP_ENTRY_Y(0x7044,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_pex_l4_clkreq_n_pl0		DRV_PINGROUP_ENTRY_Y(0x704c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_pex_l4_rst_n_pl1			DRV_PINGROUP_ENTRY_Y(0x7054,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio34_pl3			DRV_PINGROUP_ENTRY_Y(0x705c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_pex_l5_clkreq_n_paf0		DRV_PINGROUP_ENTRY_Y(0x14004,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_pex_l5_rst_n_paf1			DRV_PINGROUP_ENTRY_Y(0x1400c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_pex_l6_clkreq_n_paf2		DRV_PINGROUP_ENTRY_Y(0x14014,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_pex_l6_rst_n_paf3			DRV_PINGROUP_ENTRY_Y(0x1401c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_pex_l10_clkreq_n_pag6		DRV_PINGROUP_ENTRY_Y(0x19004,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_pex_l10_rst_n_pag7		DRV_PINGROUP_ENTRY_Y(0x1900c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_pex_l7_clkreq_n_pag0		DRV_PINGROUP_ENTRY_Y(0x19014,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_pex_l7_rst_n_pag1			DRV_PINGROUP_ENTRY_Y(0x1901c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_pex_l8_clkreq_n_pag2		DRV_PINGROUP_ENTRY_Y(0x19024,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_pex_l8_rst_n_pag3			DRV_PINGROUP_ENTRY_Y(0x1902c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_pex_l9_clkreq_n_pag4		DRV_PINGROUP_ENTRY_Y(0x19034,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_pex_l9_rst_n_pag5			DRV_PINGROUP_ENTRY_Y(0x1903c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_sdmmc1_clk_pj0			DRV_PINGROUP_ENTRY_Y(0x8004,	28,	2,	30,	2,	-1,	-1,	-1,	-1,	0)
#define	drive_sdmmc1_cmd_pj1			DRV_PINGROUP_ENTRY_Y(0x800c,	28,	2,	30,	2,	-1,	-1,	-1,	-1,	0)
#define	drive_sdmmc1_dat3_pj5			DRV_PINGROUP_ENTRY_Y(0x801c,	28,	2,	30,	2,	-1,	-1,	-1,	-1,	0)
#define	drive_sdmmc1_dat2_pj4			DRV_PINGROUP_ENTRY_Y(0x8024,	28,	2,	30,	2,	-1,	-1,	-1,	-1,	0)
#define	drive_sdmmc1_dat1_pj3			DRV_PINGROUP_ENTRY_Y(0x802c,	28,	2,	30,	2,	-1,	-1,	-1,	-1,	0)
#define	drive_sdmmc1_dat0_pj2			DRV_PINGROUP_ENTRY_Y(0x8034,	28,	2,	30,	2,	-1,	-1,	-1,	-1,	0)
#define	drive_sce_error_pee0			DRV_PINGROUP_ENTRY_Y(0x1014,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	1)
#define	drive_batt_oc_pee3			DRV_PINGROUP_ENTRY_Y(0x1024,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	1)
#define	drive_bootv_ctl_n_pee7			DRV_PINGROUP_ENTRY_Y(0x102c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	1)
#define	drive_power_on_pee4			DRV_PINGROUP_ENTRY_Y(0x103c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	1)
#define	drive_soc_gpio26_pee5			DRV_PINGROUP_ENTRY_Y(0x1044,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	1)
#define	drive_soc_gpio27_pee6			DRV_PINGROUP_ENTRY_Y(0x104c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	1)
#define	drive_ao_retention_n_pee2		DRV_PINGROUP_ENTRY_Y(0x1054,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	1)
#define	drive_vcomp_alert_pee1			DRV_PINGROUP_ENTRY_Y(0x105c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	1)
#define	drive_hdmi_cec_pgg0			DRV_PINGROUP_ENTRY_Y(0x1064,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	1)
#define	drive_ufs0_rst_n_pae1			DRV_PINGROUP_ENTRY_Y(0x11004,	12,	5,	24,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_ufs0_ref_clk_pae0			DRV_PINGROUP_ENTRY_Y(0x1100c,	12,	5,	24,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_spi3_miso_py1			DRV_PINGROUP_ENTRY_Y(0xd004,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_spi1_cs0_pz6			DRV_PINGROUP_ENTRY_Y(0xd00c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_spi3_cs0_py3			DRV_PINGROUP_ENTRY_Y(0xd014,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_spi1_miso_pz4			DRV_PINGROUP_ENTRY_Y(0xd01c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_spi3_cs1_py4			DRV_PINGROUP_ENTRY_Y(0xd024,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_spi1_sck_pz3			DRV_PINGROUP_ENTRY_Y(0xd02c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_spi3_sck_py0			DRV_PINGROUP_ENTRY_Y(0xd034,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_spi1_cs1_pz7			DRV_PINGROUP_ENTRY_Y(0xd03c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_spi1_mosi_pz5			DRV_PINGROUP_ENTRY_Y(0xd044,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_spi3_mosi_py2			DRV_PINGROUP_ENTRY_Y(0xd04c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_uart2_tx_px4			DRV_PINGROUP_ENTRY_Y(0xd054,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_uart2_rx_px5			DRV_PINGROUP_ENTRY_Y(0xd05c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_uart2_rts_px6			DRV_PINGROUP_ENTRY_Y(0xd064,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_uart2_cts_px7			DRV_PINGROUP_ENTRY_Y(0xd06c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_uart5_tx_py5			DRV_PINGROUP_ENTRY_Y(0xd074,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_uart5_rx_py6			DRV_PINGROUP_ENTRY_Y(0xd07c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_uart5_rts_py7			DRV_PINGROUP_ENTRY_Y(0xd084,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_uart5_cts_pz0			DRV_PINGROUP_ENTRY_Y(0xd08c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_gpu_pwr_req_px0			DRV_PINGROUP_ENTRY_Y(0xd094,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_gp_pwm3_px3			DRV_PINGROUP_ENTRY_Y(0xd09c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_gp_pwm2_px2			DRV_PINGROUP_ENTRY_Y(0xd0a4,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_cv_pwr_req_px1			DRV_PINGROUP_ENTRY_Y(0xd0ac,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_usb_vbus_en0_pz1			DRV_PINGROUP_ENTRY_Y(0xd0b4,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_usb_vbus_en1_pz2			DRV_PINGROUP_ENTRY_Y(0xd0bc,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_extperiph2_clk_pp1		DRV_PINGROUP_ENTRY_Y(0x0004,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_extperiph1_clk_pp0		DRV_PINGROUP_ENTRY_Y(0x000c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_cam_i2c_sda_pp3			DRV_PINGROUP_ENTRY_Y(0x0014,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_cam_i2c_scl_pp2			DRV_PINGROUP_ENTRY_Y(0x001c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio23_pp4			DRV_PINGROUP_ENTRY_Y(0x0024,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio24_pp5			DRV_PINGROUP_ENTRY_Y(0x002c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio25_pp6			DRV_PINGROUP_ENTRY_Y(0x0034,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_pwr_i2c_scl_pp7			DRV_PINGROUP_ENTRY_Y(0x003c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_pwr_i2c_sda_pq0			DRV_PINGROUP_ENTRY_Y(0x0044,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio28_pq1			DRV_PINGROUP_ENTRY_Y(0x004c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio29_pq2			DRV_PINGROUP_ENTRY_Y(0x0054,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio30_pq3			DRV_PINGROUP_ENTRY_Y(0x005c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio31_pq4			DRV_PINGROUP_ENTRY_Y(0x0064,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio32_pq5			DRV_PINGROUP_ENTRY_Y(0x006c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio33_pq6			DRV_PINGROUP_ENTRY_Y(0x0074,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio35_pq7			DRV_PINGROUP_ENTRY_Y(0x007c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio37_pr0			DRV_PINGROUP_ENTRY_Y(0x0084,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio56_pr1			DRV_PINGROUP_ENTRY_Y(0x008c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_uart1_cts_pr5			DRV_PINGROUP_ENTRY_Y(0x0094,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_uart1_rts_pr4			DRV_PINGROUP_ENTRY_Y(0x009c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_uart1_rx_pr3			DRV_PINGROUP_ENTRY_Y(0x00a4,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_uart1_tx_pr2			DRV_PINGROUP_ENTRY_Y(0x00ac,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_cpu_pwr_req_pi5			DRV_PINGROUP_ENTRY_Y(0x4004,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_uart4_cts_ph6			DRV_PINGROUP_ENTRY_Y(0x400c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_uart4_rts_ph5			DRV_PINGROUP_ENTRY_Y(0x4014,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_uart4_rx_ph4			DRV_PINGROUP_ENTRY_Y(0x401c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_uart4_tx_ph3			DRV_PINGROUP_ENTRY_Y(0x4024,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_gen1_i2c_scl_pi3			DRV_PINGROUP_ENTRY_Y(0x402c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_gen1_i2c_sda_pi4			DRV_PINGROUP_ENTRY_Y(0x4034,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio20_pg7			DRV_PINGROUP_ENTRY_Y(0x403c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio21_ph0			DRV_PINGROUP_ENTRY_Y(0x4044,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio22_ph1			DRV_PINGROUP_ENTRY_Y(0x404c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio13_pg0			DRV_PINGROUP_ENTRY_Y(0x4054,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio14_pg1			DRV_PINGROUP_ENTRY_Y(0x405c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio15_pg2			DRV_PINGROUP_ENTRY_Y(0x4064,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio16_pg3			DRV_PINGROUP_ENTRY_Y(0x406c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio17_pg4			DRV_PINGROUP_ENTRY_Y(0x4074,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio18_pg5			DRV_PINGROUP_ENTRY_Y(0x407c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio19_pg6			DRV_PINGROUP_ENTRY_Y(0x4084,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio41_ph7			DRV_PINGROUP_ENTRY_Y(0x408c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio42_pi0			DRV_PINGROUP_ENTRY_Y(0x4094,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio43_pi1			DRV_PINGROUP_ENTRY_Y(0x409c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio44_pi2			DRV_PINGROUP_ENTRY_Y(0x40a4,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio06_ph2			DRV_PINGROUP_ENTRY_Y(0x40ac,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio07_pi6			DRV_PINGROUP_ENTRY_Y(0x40b4,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_dap4_sclk_pa4			DRV_PINGROUP_ENTRY_Y(0x2004,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_dap4_dout_pa5			DRV_PINGROUP_ENTRY_Y(0x200c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_dap4_din_pa6			DRV_PINGROUP_ENTRY_Y(0x2014,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_dap4_fs_pa7			DRV_PINGROUP_ENTRY_Y(0x201c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_dap6_sclk_pa0			DRV_PINGROUP_ENTRY_Y(0x2024,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_dap6_dout_pa1			DRV_PINGROUP_ENTRY_Y(0x202c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_dap6_din_pa2			DRV_PINGROUP_ENTRY_Y(0x2034,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_dap6_fs_pa3			DRV_PINGROUP_ENTRY_Y(0x203c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio45_pad0			DRV_PINGROUP_ENTRY_Y(0x18004,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio46_pad1			DRV_PINGROUP_ENTRY_Y(0x1800c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio47_pad2			DRV_PINGROUP_ENTRY_Y(0x18014,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio48_pad3			DRV_PINGROUP_ENTRY_Y(0x1801c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio57_pac4			DRV_PINGROUP_ENTRY_Y(0x18024,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio58_pac5			DRV_PINGROUP_ENTRY_Y(0x1802c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio59_pac6			DRV_PINGROUP_ENTRY_Y(0x18034,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_soc_gpio60_pac7			DRV_PINGROUP_ENTRY_Y(0x1803c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_spi5_cs0_pac3			DRV_PINGROUP_ENTRY_Y(0x18044,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_spi5_miso_pac1			DRV_PINGROUP_ENTRY_Y(0x1804c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_spi5_mosi_pac2			DRV_PINGROUP_ENTRY_Y(0x18054,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)
#define	drive_spi5_sck_pac0			DRV_PINGROUP_ENTRY_Y(0x1805c,	12,	5,	20,	5,	-1,	-1,	-1,	-1,	0)

#define	drive_eqos_td3_pe4			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_eqos_td2_pe3			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_eqos_td1_pe2			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_eqos_td0_pe1			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_eqos_rd3_pf1			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_eqos_rd2_pf0			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_eqos_rd1_pe7			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_eqos_sma_mdio_pf4			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_eqos_rd0_pe6			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_eqos_sma_mdc_pf5			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_eqos_comp				DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_eqos_txc_pe0			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_eqos_rxc_pf3			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_eqos_tx_ctl_pe5			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_eqos_rx_ctl_pf2			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_qspi0_io3_pc5			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_qspi0_io2_pc4			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_qspi0_io1_pc3			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_qspi0_io0_pc2			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_qspi0_sck_pc0			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_qspi0_cs_n_pc1			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_qspi1_io3_pd3			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_qspi1_io2_pd2			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_qspi1_io1_pd1			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_qspi1_io0_pd0			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_qspi1_sck_pc6			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_qspi1_cs_n_pc7			DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_qspi_comp				DRV_PINGROUP_ENTRY_N(no_entry)
#define	drive_sdmmc1_comp			DRV_PINGROUP_ENTRY_N(no_entry)


#define PINGROUP(pg_name, f0, f1, f2, f3, r, bank, pupd, e_io_hv, e_lpbk, e_input, e_lpdr, e_pbias_buf,	\
			gpio_sfio_sel, schmitt_b)							\
	{								\
		.name = #pg_name,					\
		.pins = pg_name##_pins,					\
		.npins = ARRAY_SIZE(pg_name##_pins),			\
			.funcs = {					\
				TEGRA_MUX_##f0,				\
				TEGRA_MUX_##f1,				\
				TEGRA_MUX_##f2,				\
				TEGRA_MUX_##f3,				\
			},						\
		PIN_PINGROUP_ENTRY_Y(r, bank, pupd, e_io_hv, e_lpbk,	\
					e_input, e_lpdr, e_pbias_buf,	\
					gpio_sfio_sel, schmitt_b)	\
		drive_##pg_name,					\
	}
static const struct tegra_pingroup tegra234_groups[] = {
	PINGROUP(touch_clk_pcc4,	GP,		TOUCH,		RSVD2,		RSVD3,		0x2000,		1,	Y,	5,	7,	6,	8,	-1,	10,	12),
	PINGROUP(uart3_rx_pcc6,		UARTC,		UARTJ,		RSVD2,		RSVD3,		0x2008,		1,	Y,	5,	7,	6,	8,	-1,	10,	12),
	PINGROUP(uart3_tx_pcc5,		UARTC,		UARTJ,		RSVD2,		RSVD3,		0x2010,		1,	Y,	5,	7,	6,	8,	-1,	10,	12),
	PINGROUP(gen8_i2c_sda_pdd2,	I2C8,		RSVD1,		RSVD2,		RSVD3,		0x2018,		1,	Y,	5,	7,	6,	8,	-1,	10,	12),
	PINGROUP(gen8_i2c_scl_pdd1,	I2C8,		RSVD1,		RSVD2,		RSVD3,		0x2020,		1,	Y,	5,	7,	6,	8,	-1,	10,	12),
	PINGROUP(spi2_mosi_pcc2,	SPI2,		RSVD1,		RSVD2,		RSVD3,		0x2028,		1,	Y,	5,	7,	6,	8,	-1,	10,	12),
	PINGROUP(gen2_i2c_scl_pcc7,	I2C2,		RSVD1,		RSVD2,		RSVD3,		0x2030,		1,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(spi2_cs0_pcc3,		SPI2,		RSVD1,		RSVD2,		RSVD3,		0x2038,		1,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(gen2_i2c_sda_pdd0,	I2C2,		RSVD1,		RSVD2,		RSVD3,		0x2040,		1,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(spi2_sck_pcc0,		SPI2,		RSVD1,		RSVD2,		RSVD3,		0x2048,		1,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(spi2_miso_pcc1,	SPI2,		RSVD1,		RSVD2,		RSVD3,		0x2050,		1,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(can1_dout_paa2,	CAN1,		RSVD1,		RSVD2,		RSVD3,		0x3000,		1,	Y,	-1,	5,	6,	-1,	9,	10,     12),
	PINGROUP(can1_din_paa3,		CAN1,		RSVD1,		RSVD2,		RSVD3,		0x3008,		1,	Y,	-1,	5,	6,	-1,	9,	10,     12),
	PINGROUP(can0_dout_paa0,	CAN0,		RSVD1,		RSVD2,		RSVD3,		0x3010,		1,	Y,	-1,	5,	6,	-1,	9,	10,     12),
	PINGROUP(can0_din_paa1,		CAN0,		RSVD1,		RSVD2,		RSVD3,		0x3018,		1,	Y,	-1,	5,	6,	-1,	9,	10,     12),
	PINGROUP(can0_stb_paa4,		RSVD0,		WDT,		TSC,		TSC_ALT,	0x3020,		1,	Y,	-1,	5,	6,	-1,	9,	10,     12),
	PINGROUP(can0_en_paa5,		RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x3028,		1,	Y,	-1,	5,	6,	-1,	9,	10,     12),
	PINGROUP(soc_gpio49_paa6,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x3030,		1, 	Y,	-1,	5,	6,	-1,	9,	10,     12),
	PINGROUP(can0_err_paa7,		RSVD0,		TSC,		RSVD2,		TSC_ALT,	0x3038,		1,	Y,	-1,	5,	6,	-1,	9,	10,     12),
	PINGROUP(can1_stb_pbb0,		RSVD0,		DMIC3,		DMIC5,		RSVD3,		0x3040,		1,	Y,	-1,	5,	6,	-1,	9,	10,     12),
	PINGROUP(can1_en_pbb1,		RSVD0,		DMIC3,		DMIC5,		RSVD3,		0x3048,		1,	Y,	-1,	5,	6,	-1,	9,	10,     12),
	PINGROUP(soc_gpio50_pbb2,	RSVD0,		TSC,		RSVD2,		TSC_ALT,	0x3050,		1,	Y,	-1,	5,	6,	-1,	9,	10,     12),
	PINGROUP(can1_err_pbb3,		RSVD0,		TSC,		RSVD2,		TSC_ALT,	0x3058,		1,	Y,	-1,	5,	6,	-1,	9,	10,     12),
	PINGROUP(soc_gpio08_pb0,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x5008,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio36_pm5,	ETH0,		RSVD1,		DCA,		RSVD3,		0x10000,	0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio53_pm6,	ETH0,		RSVD1,		DCA,		RSVD3,		0x10008,	0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio55_pm4,	ETH2,		RSVD1,		RSVD2,		RSVD3,		0x10010,	0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio38_pm7,	ETH1,		RSVD1,		RSVD2,		RSVD3,		0x10018,	0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio39_pn1,	GP,		RSVD1,		RSVD2,		RSVD3,		0x10020,	0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio40_pn2,	ETH1,		RSVD1,		RSVD2,		RSVD3,		0x10028,	0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(dp_aux_ch0_hpd_pm0,	DP,		RSVD1,		RSVD2,		RSVD3,		0x10030,	0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(dp_aux_ch1_hpd_pm1,	ETH3,		RSVD1,		RSVD2,		RSVD3,		0x10038,	0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(dp_aux_ch2_hpd_pm2,	ETH3,		RSVD1,		DISPLAYB,	RSVD3,		0x10040,	0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(dp_aux_ch3_hpd_pm3,	ETH2,		RSVD1,		DISPLAYA,	RSVD3,		0x10048,	0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(dp_aux_ch1_p_pn3,	I2C4,		RSVD1,		RSVD2,		RSVD3,		0x10050,	0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(dp_aux_ch1_n_pn4,	I2C4,		RSVD1,		RSVD2,		RSVD3,		0x10058,	0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(dp_aux_ch2_p_pn5,	I2C7,		RSVD1,		RSVD2,		RSVD3,		0x10060,	0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(dp_aux_ch2_n_pn6,	I2C7,		RSVD1,		RSVD2,		RSVD3,		0x10068,	0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(dp_aux_ch3_p_pn7,	I2C9,		RSVD1,		RSVD2,		RSVD3,		0x10070,	0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(dp_aux_ch3_n_pn0,	I2C9,		RSVD1,		RSVD2,		RSVD3,		0x10078,	0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(eqos_td3_pe4,		EQOS,		RSVD1,		RSVD2,		RSVD3,		0x15000,	0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(eqos_td2_pe3,		EQOS,		RSVD1,		RSVD2,		RSVD3,		0x15008,	0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(eqos_td1_pe2,		EQOS,		RSVD1,		RSVD2,		RSVD3,		0x15010,	0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(eqos_td0_pe1,		EQOS,		RSVD1,		RSVD2,		RSVD3,		0x15018,	0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(eqos_rd3_pf1,		EQOS,		RSVD1,		RSVD2,		RSVD3,		0x15020,	0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(eqos_rd2_pf0,		EQOS,		RSVD1,		RSVD2,		RSVD3,		0x15028,	0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(eqos_rd1_pe7,		EQOS,		RSVD1,		RSVD2,		RSVD3,		0x15030,	0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(eqos_sma_mdio_pf4,	EQOS,		RSVD1,		RSVD2,		RSVD3,		0x15038,	0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(eqos_rd0_pe6,		EQOS,		RSVD1,		RSVD2,		RSVD3,		0x15040,	0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(eqos_sma_mdc_pf5,	EQOS,		RSVD1,		RSVD2,		RSVD3,		0x15048,	0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(eqos_comp,		EQOS,		RSVD1,		RSVD2,		RSVD3,		0x15050,	0,	N,	-1,	-1,	-1,	-1,	-1,	-1,     -1),
	PINGROUP(eqos_txc_pe0,		EQOS,		RSVD1,		RSVD2,		RSVD3,		0x15058,	0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(eqos_rxc_pf3,		EQOS,		RSVD1,		RSVD2,		RSVD3,		0x15060,	0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(eqos_tx_ctl_pe5,	EQOS,		RSVD1,		RSVD2,		RSVD3,		0x15068,	0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(eqos_rx_ctl_pf2,	EQOS,		RSVD1,		RSVD2,		RSVD3,		0x15070,	0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(pex_l2_clkreq_n_pk4,	PE2,		RSVD1,		RSVD2,		RSVD3,		0x7000,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(pex_wake_n_pl2,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x7008,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(pex_l1_clkreq_n_pk2,	PE1,		RSVD1,		RSVD2,		RSVD3,		0x7010,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(pex_l1_rst_n_pk3,	PE1,		RSVD1,		RSVD2,		RSVD3,		0x7018,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(pex_l0_clkreq_n_pk0,	PE0,		RSVD1,		RSVD2,		RSVD3,		0x7020,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(pex_l0_rst_n_pk1,	PE0,		RSVD1,		RSVD2,		RSVD3,		0x7028,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(pex_l2_rst_n_pk5,	PE2,		RSVD1,		RSVD2,		RSVD3,		0x7030,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(pex_l3_clkreq_n_pk6,	PE3,		RSVD1,		RSVD2,		RSVD3,		0x7038,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(pex_l3_rst_n_pk7,	PE3,		RSVD1,		RSVD2,		RSVD3,		0x7040,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(pex_l4_clkreq_n_pl0,	PE4,		RSVD1,		RSVD2,		RSVD3,		0x7048,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(pex_l4_rst_n_pl1,	PE4,		RSVD1,		RSVD2,		RSVD3,		0x7050,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio34_pl3,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x7058,		0,	Y,	5,	7,	6, 	8,	-1,	10,     12),
	PINGROUP(pex_l5_clkreq_n_paf0,	PE5,		RSVD1,		RSVD2,		RSVD3,		0x14000,	0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(pex_l5_rst_n_paf1,	PE5,		RSVD1,		RSVD2,		RSVD3,		0x14008,	0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(pex_l6_clkreq_n_paf2,  PE6,		RSVD1,		RSVD2,		RSVD3,		0x14010,	0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(pex_l6_rst_n_paf3,	PE6,		RSVD1,		RSVD2,		RSVD3,		0x14018,	0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(pex_l10_clkreq_n_pag6,	PE10,		RSVD1,		RSVD2,		RSVD3,		0x19000,	0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(pex_l10_rst_n_pag7,	PE10,		RSVD1,		RSVD2,		RSVD3,		0x19008,	0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(pex_l7_clkreq_n_pag0,	PE7,		RSVD1,		RSVD2,		RSVD3,		0x19010,	0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(pex_l7_rst_n_pag1,	PE7,		RSVD1,		RSVD2,		RSVD3,		0x19018,	0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(pex_l8_clkreq_n_pag2,	PE8,		RSVD1,		RSVD2,		RSVD3,		0x19020,	0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(pex_l8_rst_n_pag3,	PE8,		RSVD1,		RSVD2,		RSVD3,		0x19028,	0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(pex_l9_clkreq_n_pag4,	PE9,		RSVD1,		RSVD2,		RSVD3,		0x19030,	0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(pex_l9_rst_n_pag5,	PE9,		RSVD1,		RSVD2,		RSVD3,		0x19038,	0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(qspi0_io3_pc5,		QSPI0,		RSVD1,		RSVD2,		RSVD3,		0xB000,		0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(qspi0_io2_pc4,		QSPI0,		RSVD1,		RSVD2,		RSVD3,		0xB008,		0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(qspi0_io1_pc3,		QSPI0,		RSVD1,		RSVD2,		RSVD3,		0xB010,		0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(qspi0_io0_pc2,		QSPI0,		RSVD1,		RSVD2,		RSVD3,		0xB018,		0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(qspi0_sck_pc0,		QSPI0,		RSVD1,		RSVD2,		RSVD3,		0xB020,		0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(qspi0_cs_n_pc1,	QSPI0,		RSVD1,		RSVD2,		RSVD3,		0xB028,		0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(qspi1_io3_pd3,		QSPI1,		RSVD1,		RSVD2,		RSVD3,		0xB030,		0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(qspi1_io2_pd2,		QSPI1,		RSVD1,		RSVD2,		RSVD3,		0xB038,		0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(qspi1_io1_pd1,		QSPI1,		RSVD1,		RSVD2,		RSVD3,		0xB040,		0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(qspi1_io0_pd0,		QSPI1,		RSVD1,		RSVD2,		RSVD3,		0xB048,		0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(qspi1_sck_pc6,		QSPI1,		RSVD1,		RSVD2,		RSVD3,		0xB050,		0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(qspi1_cs_n_pc7,	QSPI1,		RSVD1,		RSVD2,		RSVD3,		0xB058,		0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(qspi_comp,		QSPI,		RSVD1,		RSVD2,		RSVD3,		0xB060,		0,	N,	-1,	-1,	-1,	-1,	-1,	-1,     -1),
	PINGROUP(sdmmc1_clk_pj0,	SDMMC1,		RSVD1,		RSVD2,		RSVD3,		0x8000,		0,	Y,	-1,	5,	6,	-1,	9,	10,     12),
	PINGROUP(sdmmc1_cmd_pj1,	SDMMC1,		RSVD1,		RSVD2,		RSVD3,		0x8008,		0,	Y,	-1,	5,	6,	-1,	9,	10,     12),
	PINGROUP(sdmmc1_comp,		SDMMC1,		RSVD1,		RSVD2,		RSVD3,		0x8010,		0,	N,	-1,	-1,	-1,	-1,	-1,	-1,     -1),
	PINGROUP(sdmmc1_dat3_pj5,	SDMMC1,		RSVD1,		RSVD2,		RSVD3,		0x8018,		0,	Y,	-1,	5,	6,	-1,	9,	10,     12),
	PINGROUP(sdmmc1_dat2_pj4,	SDMMC1,		RSVD1,		RSVD2,		RSVD3,		0x8020,		0,	Y,	-1,	5,	6,	-1,	9,	10,     12),
	PINGROUP(sdmmc1_dat1_pj3,	SDMMC1,		RSVD1,		RSVD2,		RSVD3,		0x8028,		0,	Y,	-1,	5,	6,	-1,	9,	10,     12),
	PINGROUP(sdmmc1_dat0_pj2,	SDMMC1,		RSVD1,		RSVD2,		RSVD3,		0x8030,		0,	Y,	-1,	5,	6,	-1,	9,	10,     12),
	PINGROUP(sce_error_pee0,	SCE,		RSVD1,		RSVD2,		RSVD3,		0x1010,		1,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(batt_oc_pee3,		SOC,		RSVD1,		RSVD2,		RSVD3,		0x1020,		1,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(bootv_ctl_n_pee7,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x1028,		1,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(power_on_pee4,		RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x1038,		1,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio26_pee5,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x1040,		1,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio27_pee6,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x1048,		1,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(ao_retention_n_pee2,	GPIO,		LED,		RSVD2,		ISTCTRL,	0x1050,		1,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(vcomp_alert_pee1,	SOC,		RSVD1,		RSVD2,		RSVD3,		0x1058,		1,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(hdmi_cec_pgg0,		HDMI,		RSVD1,		RSVD2,		RSVD3,		0x1060,		1,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(ufs0_rst_n_pae1,	UFS0,		RSVD1,		RSVD2,		RSVD3,		0x11000,	0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(ufs0_ref_clk_pae0,	UFS0,		RSVD1,		RSVD2,		RSVD3,		0x11008,	0,	Y,	-1,	5,	6,	-1,	-1,	10,     12),
	PINGROUP(spi3_miso_py1,		SPI3,		RSVD1,		RSVD2,		RSVD3,		0xD000,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(spi1_cs0_pz6,		SPI1,		RSVD1,		RSVD2,		RSVD3,		0xD008,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(spi3_cs0_py3,		SPI3,		RSVD1,		RSVD2,		RSVD3,		0xD010,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(spi1_miso_pz4,		SPI1,		RSVD1,		RSVD2,		RSVD3,		0xD018,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(spi3_cs1_py4,		SPI3,		RSVD1,		RSVD2,		RSVD3,		0xD020,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(spi1_sck_pz3,		SPI1,		RSVD1,		RSVD2,		RSVD3,		0xD028,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(spi3_sck_py0,		SPI3,		RSVD1,		RSVD2,		RSVD3,		0xD030,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(spi1_cs1_pz7,		SPI1,		RSVD1,		RSVD2,		RSVD3,		0xD038,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(spi1_mosi_pz5,		SPI1,		RSVD1,		RSVD2,		RSVD3,		0xD040,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(spi3_mosi_py2,		SPI3,		RSVD1,		RSVD2,		RSVD3,		0xD048,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(uart2_tx_px4,		UARTB,		RSVD1,		RSVD2,		RSVD3,		0xD050,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(uart2_rx_px5,		UARTB,		RSVD1,		RSVD2,		RSVD3,		0xD058,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(uart2_rts_px6,		UARTB,		RSVD1,		RSVD2,		RSVD3,		0xD060,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(uart2_cts_px7,		UARTB,		RSVD1,		RSVD2,		RSVD3,		0xD068,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(uart5_tx_py5,		UARTE,		RSVD1,		RSVD2,		RSVD3,		0xD070,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(uart5_rx_py6,		UARTE,		RSVD1,		RSVD2,		RSVD3,		0xD078,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(uart5_rts_py7,		UARTE,		RSVD1,		RSVD2,		RSVD3,		0xD080,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(uart5_cts_pz0,		UARTE,		RSVD1,		RSVD2,		RSVD3,		0xD088,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(gpu_pwr_req_px0,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0xD090,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(gp_pwm3_px3,		GP,		RSVD1,		RSVD2,		RSVD3,		0xD098,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(gp_pwm2_px2,		GP,		RSVD1,		RSVD2,		RSVD3,		0xD0A0,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(cv_pwr_req_px1,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0xD0A8,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(usb_vbus_en0_pz1,	USB,		RSVD1,		RSVD2,		RSVD3,		0xD0B0,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(usb_vbus_en1_pz2,	USB,		RSVD1,		RSVD2,		RSVD3,		0xD0B8,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(extperiph2_clk_pp1,	EXTPERIPH2,	RSVD1,		RSVD2,		RSVD3,		0x0000,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(extperiph1_clk_pp0,	EXTPERIPH1,	RSVD1,		RSVD2,		RSVD3,		0x0008,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(cam_i2c_sda_pp3,	I2C3,		VI0,		RSVD2,		VI1,		0x0010,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(cam_i2c_scl_pp2,	I2C3,		VI0,		VI0_ALT,	VI1,		0x0018,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio23_pp4,	VI0,		VI0_ALT,	VI1,		VI1_ALT,	0x0020,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio24_pp5,	VI0,		SOC,		VI1,		VI1_ALT,	0x0028,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio25_pp6,	VI0,		I2S5,		VI1,		DMIC1,		0x0030,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(pwr_i2c_scl_pp7,	I2C5,		RSVD1,		RSVD2,		RSVD3,		0x0038,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(pwr_i2c_sda_pq0,	I2C5,		RSVD1,		RSVD2,		RSVD3,		0x0040,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio28_pq1,	VI0,		RSVD1,		VI1,		RSVD3,		0x0048,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio29_pq2,	RSVD0,		NV,		RSVD2,		RSVD3,		0x0050,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio30_pq3,	RSVD0,		WDT,		RSVD2,		RSVD3,		0x0058,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio31_pq4,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x0060,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio32_pq5,	RSVD0,		EXTPERIPH3,	DCB,		RSVD3,		0x0068,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio33_pq6,	RSVD0,		EXTPERIPH4,	DCB,		RSVD3,		0x0070,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio35_pq7,	RSVD0,		I2S5,		DMIC1,		RSVD3,		0x0078,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio37_pr0,	GP,		I2S5,		DMIC4,		DSPK1,		0x0080,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio56_pr1,	RSVD0,		I2S5,		DMIC4,		DSPK1,		0x0088,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(uart1_cts_pr5,		UARTA,		RSVD1,		RSVD2,		RSVD3,		0x0090,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(uart1_rts_pr4,		UARTA,		RSVD1,		RSVD2,		RSVD3,		0x0098,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(uart1_rx_pr3,		UARTA,		RSVD1,		RSVD2,		RSVD3,		0x00A0,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(uart1_tx_pr2,		UARTA,		RSVD1,		RSVD2,		RSVD3,		0x00A8,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(cpu_pwr_req_pi5,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x4000,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(uart4_cts_ph6,		UARTD,		RSVD1,		I2S7,		RSVD3,		0x4008,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(uart4_rts_ph5,		UARTD,		SPI4,		RSVD2,		RSVD3,		0x4010,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(uart4_rx_ph4,		UARTD,		RSVD1,		I2S7,		RSVD3,		0x4018,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(uart4_tx_ph3,		UARTD,		SPI4,		RSVD2,		RSVD3,		0x4020,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(gen1_i2c_scl_pi3,	I2C1,		RSVD1,		RSVD2,		RSVD3,		0x4028,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(gen1_i2c_sda_pi4,	I2C1,		RSVD1,		RSVD2,		RSVD3,		0x4030,		0,	Y,	5,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio20_pg7,	RSVD0,		SDMMC1,		RSVD2,		RSVD3,		0x4038,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio21_ph0,	RSVD0,		GP,		I2S7,		RSVD3,		0x4040,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio22_ph1,	RSVD0,		RSVD1,		I2S7,		RSVD3,		0x4048,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio13_pg0,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x4050,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio14_pg1,	RSVD0,		SPI4,		RSVD2,		RSVD3,		0x4058,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio15_pg2,	RSVD0,		SPI4,		RSVD2,		RSVD3,		0x4060,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio16_pg3,	RSVD0,		SPI4,		RSVD2,		RSVD3,		0x4068,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio17_pg4,	RSVD0,		CCLA,		RSVD2,		RSVD3,		0x4070,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio18_pg5,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x4078,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio19_pg6,	GP,		RSVD1,		RSVD2,		RSVD3,		0x4080,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio41_ph7,	RSVD0,		I2S2,		RSVD2,		RSVD3,		0x4088,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio42_pi0,	RSVD0,		I2S2,		RSVD2,		RSVD3,		0x4090,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio43_pi1,	RSVD0,		I2S2,		RSVD2,		RSVD3,		0x4098,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio44_pi2,	RSVD0,		I2S2,		RSVD2,		RSVD3,		0x40A0,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio06_ph2,	RSVD0,		RSVD1,		RSVD2,		RSVD3,		0x40A8,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio07_pi6,	GP,		RSVD1,		RSVD2,		RSVD3,		0x40B0,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(dap4_sclk_pa4,		I2S4,		RSVD1,		RSVD2,		RSVD3,		0x2000,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(dap4_dout_pa5,		I2S4,		RSVD1,		RSVD2,		RSVD3,		0x2008,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(dap4_din_pa6,		I2S4,		RSVD1,		RSVD2,		RSVD3,		0x2010,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(dap4_fs_pa7,		I2S4,		RSVD1,		RSVD2,		RSVD3,		0x2018,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(dap6_sclk_pa0,		I2S6,		RSVD1,		RSVD2,		RSVD3,		0x2020,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(dap6_dout_pa1,		I2S6,		RSVD1,		RSVD2,		RSVD3,		0x2028,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(dap6_din_pa2,		I2S6,		RSVD1,		RSVD2,		RSVD3,		0x2030,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(dap6_fs_pa3,		I2S6,		RSVD1,		RSVD2,		RSVD3,		0x2038,		0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio45_pad0,	RSVD0,		I2S1,		RSVD2,		RSVD3,		0x18000,	0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio46_pad1,	RSVD0,		I2S1,		RSVD2,		RSVD3,		0x18008,	0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio47_pad2,	RSVD0,		I2S1,		RSVD2,		RSVD3,		0x18010,	0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio48_pad3,	RSVD0,		I2S1,		RSVD2,		RSVD3,		0x18018,	0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio57_pac4,	RSVD0,		I2S8,		RSVD2,		SDMMC1,		0x18020,	0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio58_pac5,	RSVD0,		I2S8,		RSVD2,		SDMMC1,		0x18028,	0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio59_pac6,	AUD,		I2S8,		RSVD2,		RSVD3,		0x18030,	0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(soc_gpio60_pac7,	RSVD0,		I2S8,		NV,		IGPU,		0x18038,	0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(spi5_cs0_pac3,		SPI5,		I2S3,		DMIC2,		RSVD3,		0x18040,	0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(spi5_miso_pac1,	SPI5,		I2S3,		DSPK0,		RSVD3,		0x18048,	0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(spi5_mosi_pac2,	SPI5,		I2S3,		DMIC2,		RSVD3,		0x18050,	0,	Y,	-1,	7,	6,	8,	-1,	10,     12),
	PINGROUP(spi5_sck_pac0,		SPI5,		I2S3,		DSPK0,		RSVD3,		0x18058,	0,	Y,	-1,	7,	6,	8,	-1,	10,     12),

};

static const struct tegra_pinctrl_soc_data tegra234_pinctrl = {
	.ngpios = NUM_GPIOS,
	.pins = tegra234_pins,
	.npins = ARRAY_SIZE(tegra234_pins),
	.functions = tegra234_functions,
	.nfunctions = ARRAY_SIZE(tegra234_functions),
	.groups = tegra234_groups,
	.ngroups = ARRAY_SIZE(tegra234_groups),
	.hsm_in_mux = false,
	.schmitt_in_mux = true,
	.drvtype_in_mux = true,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
	.sfsel_in_mux = true,
#endif
};

static int tegra234_pinctrl_probe(struct platform_device *pdev)
{
	return tegra_pinctrl_probe(pdev, &tegra234_pinctrl);
}

static struct of_device_id tegra234_pinctrl_of_match[] = {
	{ .compatible = "nvidia,tegra234-pinmux", },
	{ },
};
MODULE_DEVICE_TABLE(of, tegra234_pinctrl_of_match);

static struct platform_driver tegra234_pinctrl_driver = {
	.driver = {
		.name = "tegra234-pinctrl",
		.owner = THIS_MODULE,
		.of_match_table = tegra234_pinctrl_of_match,
	},
	.probe = tegra234_pinctrl_probe,
};

static int __init tegra234_pinctrl_init(void)
{
	return platform_driver_register(&tegra234_pinctrl_driver);
}
module_init(tegra234_pinctrl_init);

MODULE_AUTHOR("Prathamesh Shete <pshete@nvidia.com>");
MODULE_DESCRIPTION("NVIDIA Tegra194 pinctrl driver");
MODULE_LICENSE("GPL v2");

