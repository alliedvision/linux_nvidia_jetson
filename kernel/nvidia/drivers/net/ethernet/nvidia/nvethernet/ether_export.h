/*
 * Copyright (c) 2022-2023, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef ETHER_EXPORT_H
#define ETHER_EXPORT_H

#include <nvethernetrm_export.h>
/**
 * @addtogroup private IOCTL related info
 *
 * @brief MACRO are defined for driver supported
 * private IOCTLs. These IOCTLs can be called using
 * SIOCDEVPRIVATE custom ioctl command.
 * @{
 */
/** To set HW AVB configuration from user application */
#define ETHER_AVB_ALGORITHM		27
/** To get current configuration in HW */
#define ETHER_GET_AVB_ALGORITHM		46
/** To configure EST(802.1 bv) in HW */
#define ETHER_CONFIG_EST		49
/** For configure FPE (802.1 bu + 803.2 br) in HW */
#define ETHER_CONFIG_FPE		50
/* FRP command */
#define ETHER_CONFIG_FRP_CMD		51
/** To configure L2 Filter (Only with Ethernet virtualization) */
#define ETHER_L2_ADDR			61
/** @} */

/**
 * @brief Structure for L2 filters input
 */
struct ether_l2_filter {
	/** indicates enable(1)/disable(0) L2 filter */

	nveu32_t en_dis;
	/** Indicates the index of the filter to be modified.
	 * Filter index must be between 0 - 31 */
	nveu32_t index;
	/** Ethernet MAC address to be added */
	nveu8_t mac_address[OSI_ETH_ALEN];
};

/**
 * @brief struct ether_exported_ifr_data - Private data of struct ifreq
 */
struct ether_exported_ifr_data {
	/** Flags used for specific ioctl - like enable/disable */
	nveu32_t if_flags;
	/** qinx: Queue index to be used for certain ioctls */
	nveu32_t qinx;
	/** The private ioctl command number */
	nveu32_t ifcmd;
	/** Used to query the connected link speed */
	nveu32_t connected_speed;
	/** The return value of IOCTL handler func */
	nve32_t command_error;
	/** IOCTL cmd specific structure pointer */
	void *ptr;
};
#endif /* ETHER_EXPORT_H */
