/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
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
 * This header provides macros for different DT binding version used
 * by different OS/kernel versions.
 */

#ifndef _DT_BINDINGS_VERSION_H_
#define _DT_BINDINGS_VERSION_H_

#define DT_VERSION_1		1
#define DT_VERSION_2		2
#define DT_VERSION_3		3

/**
 * TEGRA_PWM_FAN_DT_VERSION
 * 		V1: Nv version of pwm-fan DT binding
 * 		V2: Main line compatible DT binding
 *
 * TEGRA_PMC_VERSION
 *		V1: Nv version of PMC DT binding
 * 		V2: Mainline compatible DT binding
 *
 * TEGRA_TCU_DT_VERSION
 *		V1: Nv version of Tegra Combined Uart DT binding
 * 		V2: Mainline compatible DT binding
 *
 * TEGRA_BPMP_FW_DT_VERSION
 *		V1: Nv version of DT binding
 * 		V2: Mainline compatible DT binding
 *
 * TEGRA_RTC_DT_VERSION
 * 		V1: Nv version of DT binding
 * 		V2: Mainline compatible DT binding
 *
 * TEGRA_AUDIO_BUS_DT_VERSION: Audio node DT version.
 *		V1: All audio bus are in root node.
 *		V2: All audio bus are in the under aconnect node.
 *
 * TEGRA_POWER_DOMAIN_DT_VERSION: Power domain DT versions.
 *		V1: Use legacy power gating API.
 *		V2: Use BPMP power domain provider.
 *		V3: Use Genpd power domain APIs.
 *
 * TEGRA_XUSB_PADCONTROL_VERSION
 *		V1: Nv Version of DT binding
 *		V2: Mainline compatible DT binding.
 *
 * TEGRA_XUSB_DT_VERSION: XUSB DT binding version
 *		V1: Nv Version of DT binding
 *		V2: NV Version2 DT binding
 *		V3: Mainline compatible DT binding.
 *
 * TEGRA_XUDC_DT_VERSION: XUDC DT binding version
 *		V1: Nv Version of DT binding
 *		V2: Mainline compatible DT binding.
 *
 * TEGRA_HSP_DT_VERSION: HSP DT binding version
 *		V1: Nv Version of DT binding
 *		V2: Mainline compatible DT binding.
 *
 * UART_CONSOLE_ON_TTYS0_ONLY: Uart console only on TTYS0. Some OS support
 *			       console port in the ttyS0 only.
 *		1 for those OS which support console only on ttyS0.
 *		0 for those OS which can support console on any ttySx.
 *
 * TEGRA_BOOTARGUMENT_VERSION: Boot argument via chosen node.
 *		V1: Only pass the console.
 *		V2: Pass the android boots parameters.

 * TEGRA_CPUFREQ_DT_VERSION: CPUFREQ DT binding version
 *		V1: the separate EDVD aperture for two clusters.
 *		V2: the unified aperture for two clusters.
 *
 * TEGRA_IOMMU_DT_VERSION: IOMMU/smmu DT binding version
 *		V1: Nv Version of DT binding
 *		V2: Mainline compatible DT binding.
 *
 * TEGRA_SDMMC_VERSION: SDMMC DT version.
 *		V1: Nv Version of DT binding
 * 		V2: Mainline compatible DT binding
 *
 * TEGRA_PCIE_VERSION
 *		V1: Nv version of PCIe DT binding
 * 		V2: Mainline compatible DT binding
 *
 * TEGRA_HOST1X_DT_VERSION: HOST1X DT version.
 *		V1: Nv Version of DT binding
 * 		V2: Mainline compatible DT binding
 *
 * TEGRA_GPIO_DT_VERSION: GPIO DT version.
 *		V1: Nv Version of DT binding
 * 		V2: Mainline compatible DT binding
 *
 * TEGRA_I2C_DT_VERSION: I2C DT version.
 *		V1: Nv Version of DT binding
 * 		V2: Mainline compatible DT binding
 *
 * TEGRA_ETHERNETPHY_DT_VERSION: ETHERNET PHY DT version.
 *		V1: Nv Version of DT binding
 * 		V2: Mainline compatible DT binding
 *
 * TEGRA_CBB_DT_VERSION: CBB DT version.
 *		V1: Nv Version of DT binding
 * 		V2: Mainline compatible DT binding
 */

/* OS Linux */
#if defined(LINUX_VERSION)
#define _OS_FOUND_
#define TEGRA_CPUFREQ_DT_VERSION		DT_VERSION_2
#if LINUX_VERSION >= 419
#define TEGRA_PMC_VERSION			DT_VERSION_2
#define TEGRA_TCU_DT_VERSION			DT_VERSION_2
#define TEGRA_BPMP_FW_DT_VERSION		DT_VERSION_2
#define TEGRA_GPIO_DT_VERSION			DT_VERSION_2
#define TEGRA_RTC_DT_VERSION			DT_VERSION_2
#define TEGRA_IOMMU_DT_VERSION			DT_VERSION_2
#define TEGRA_SDMMC_VERSION			DT_VERSION_2
#define TEGRA_PCIE_VERSION			DT_VERSION_2
#define TEGRA_XUSB_DT_VERSION			DT_VERSION_3
#define TEGRA_PWM_FAN_DT_VERSION		DT_VERSION_2
#define TEGRA_POWER_DOMAIN_DT_VERSION		DT_VERSION_3
#define TEGRA_I2C_DT_VERSION			DT_VERSION_2
#define TEGRA_ETHERNETPHY_DT_VERSION		DT_VERSION_2
#define TEGRA_CBB_DT_VERSION			DT_VERSION_2
#define TEGRA_HSP_DT_VERSION			DT_VERSION_2
#ifndef TEGRA_HOST1X_DT_VERSION
#define TEGRA_HOST1X_DT_VERSION			DT_VERSION_2
#endif
#else
#define TEGRA_PMC_VERSION			DT_VERSION_1
#define TEGRA_TCU_DT_VERSION			DT_VERSION_1
#define TEGRA_BPMP_FW_DT_VERSION		DT_VERSION_1
#define TEGRA_GPIO_DT_VERSION			DT_VERSION_1
#define TEGRA_RTC_DT_VERSION			DT_VERSION_1
#define TEGRA_IOMMU_DT_VERSION			DT_VERSION_1
#define TEGRA_SDMMC_VERSION			DT_VERSION_1
#define TEGRA_PCIE_VERSION			DT_VERSION_1
#define TEGRA_XUSB_DT_VERSION			DT_VERSION_2
#define TEGRA_PWM_FAN_DT_VERSION		DT_VERSION_1
#define TEGRA_POWER_DOMAIN_DT_VERSION		DT_VERSION_2
#define TEGRA_I2C_DT_VERSION			DT_VERSION_1
#define TEGRA_ETHERNETPHY_DT_VERSION		DT_VERSION_1
#define TEGRA_CBB_DT_VERSION			DT_VERSION_1
#define TEGRA_HSP_DT_VERSION			DT_VERSION_1
#define TEGRA_HOST1X_DT_VERSION			DT_VERSION_1
#endif
#define TEGRA_AUDIO_BUS_DT_VERSION		DT_VERSION_2
#define TEGRA_XUSB_PADCONTROL_VERSION		DT_VERSION_2
#define TEGRA_XUDC_DT_VERSION			DT_VERSION_2
#define TEGRA_BOOTARGUMENT_VERSION		DT_VERSION_2
#define UART_CONSOLE_ON_TTYS0_ONLY		0
#endif

/* OS QNX */
#if defined (__QNX__)
#define _OS_FOUND_
#define TEGRA_PMC_VERSION			DT_VERSION_1
#define TEGRA_TCU_DT_VERSION			DT_VERSION_2
#define TEGRA_BPMP_FW_DT_VERSION		DT_VERSION_1
#define TEGRA_GPIO_DT_VERSION			DT_VERSION_1
#define TEGRA_RTC_DT_VERSION			DT_VERSION_1
#define TEGRA_AUDIO_BUS_DT_VERSION		DT_VERSION_1
#define TEGRA_POWER_DOMAIN_DT_VERSION		DT_VERSION_2
#define TEGRA_XUSB_PADCONTROL_VERSION		DT_VERSION_2
#define TEGRA_XUSB_DT_VERSION			DT_VERSION_2
#define TEGRA_I2C_DT_VERSION			DT_VERSION_1
#define TEGRA_XUDC_DT_VERSION			DT_VERSION_2
#define TEGRA_HSP_DT_VERSION			DT_VERSION_1
#define TEGRA_BOOTARGUMENT_VERSION		DT_VERSION_2
#define TEGRA_CPUFREQ_DT_VERSION		DT_VERSION_2
#define TEGRA_IOMMU_DT_VERSION			DT_VERSION_1
#define UART_CONSOLE_ON_TTYS0_ONLY		1
#define TEGRA_SDMMC_VERSION			DT_VERSION_1
#define TEGRA_PCIE_VERSION			DT_VERSION_1
#define TEGRA_HOST1X_DT_VERSION			DT_VERSION_1
#define TEGRA_ETHERNETPHY_DT_VERSION		DT_VERSION_1
#define TEGRA_CBB_DT_VERSION			DT_VERSION_1
#endif

/* OS Integrity */
#if defined( __INTEGRITY)
#define _OS_FOUND_
#define TEGRA_PMC_VERSION			DT_VERSION_1
#define TEGRA_TCU_DT_VERSION			DT_VERSION_1
#define TEGRA_BPMP_FW_DT_VERSION		DT_VERSION_1
#define TEGRA_GPIO_DT_VERSION			DT_VERSION_1
#define TEGRA_RTC_DT_VERSION			DT_VERSION_1
#define TEGRA_AUDIO_BUS_DT_VERSION		DT_VERSION_1
#define TEGRA_POWER_DOMAIN_DT_VERSION		DT_VERSION_2
#define TEGRA_XUSB_PADCONTROL_VERSION		DT_VERSION_2
#define TEGRA_XUSB_DT_VERSION			DT_VERSION_2
#define TEGRA_I2C_DT_VERSION			DT_VERSION_1
#define TEGRA_XUDC_DT_VERSION			DT_VERSION_2
#define TEGRA_HSP_DT_VERSION			DT_VERSION_1
#define TEGRA_BOOTARGUMENT_VERSION		DT_VERSION_2
#define TEGRA_CPUFREQ_DT_VERSION		DT_VERSION_2
#define TEGRA_IOMMU_DT_VERSION			DT_VERSION_1
#define UART_CONSOLE_ON_TTYS0_ONLY		1
#define TEGRA_SDMMC_VERSION			DT_VERSION_1
#define TEGRA_PCIE_VERSION			DT_VERSION_1
#define TEGRA_HOST1X_DT_VERSION			DT_VERSION_1
#define TEGRA_ETHERNETPHY_DT_VERSION		DT_VERSION_1
#define TEGRA_CBB_DT_VERSION			DT_VERSION_1
#endif

/* OS LK */
#if defined (__LK__)
#define _OS_FOUND_
#define TEGRA_PMC_VERSION			DT_VERSION_1
#define TEGRA_SDMMC_VERSION			DT_VERSION_1
#define TEGRA_TCU_DT_VERSION			DT_VERSION_1
#define TEGRA_BPMP_FW_DT_VERSION		DT_VERSION_1
#define TEGRA_GPIO_DT_VERSION			DT_VERSION_1
#define TEGRA_RTC_DT_VERSION			DT_VERSION_1
#define TEGRA_AUDIO_BUS_DT_VERSION		DT_VERSION_2
#define TEGRA_POWER_DOMAIN_DT_VERSION		DT_VERSION_2
#define TEGRA_XUSB_PADCONTROL_VERSION		DT_VERSION_2
#define TEGRA_XUSB_DT_VERSION			DT_VERSION_2
#define TEGRA_I2C_DT_VERSION			DT_VERSION_1
#define TEGRA_XUDC_DT_VERSION			DT_VERSION_2
#define TEGRA_HSP_DT_VERSION			DT_VERSION_1
#define TEGRA_BOOTARGUMENT_VERSION		DT_VERSION_2
#define TEGRA_CPUFREQ_DT_VERSION		DT_VERSION_1
#define TEGRA_IOMMU_DT_VERSION			DT_VERSION_1
#define TEGRA_PCIE_VERSION			DT_VERSION_1
#define TEGRA_HOST1X_DT_VERSION			DT_VERSION_1
#define TEGRA_ETHERNETPHY_DT_VERSION		DT_VERSION_1
#define TEGRA_CBB_DT_VERSION			DT_VERSION_1
#define TEGRA_GENERIC_CARVEOUT_SUPPORT_ENABLE	0
#define UART_CONSOLE_ON_TTYS0_ONLY		1
#endif

/**
 * If no OS found then abort compilation.
 */
#if !defined(_OS_FOUND_)
#error "Valid OS not found"
#endif

#endif /* _DT_BINDINGS_VERSION_H_ */
