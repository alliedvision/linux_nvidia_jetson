/*=============================================================================
  Copyright (C) 2020 Allied Vision Technologies.  All Rights Reserved.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

  -----------------------------------------------------------------------------

File:        alvium_helper.h

version:     1.0.0
=============================================================================*/

////////////////////////////////////////////////////////////////////////////////
// INCLUDES
////////////////////////////////////////////////////////////////////////////////
#include "alvium_regs.h"

////////////////////////////////////////////////////////////////////////////////
// DEFINES
////////////////////////////////////////////////////////////////////////////////
#ifndef ALVIUM_HELPER_H
#define ALVIUM_HELPER_H

#define AV_CAM_SOFTWARE_TRIGGER 4

////////////////////////////////////////////////////////////////////////////////
// ENUMS
////////////////////////////////////////////////////////////////////////////////
enum CCI_REG_INFO {
	CCI_REGISTER_LAYOUT_VERSION = 0,
	RESERVED4BIT,
	DEVICE_CAPABILITIES,
	GCPRM_ADDRESS,
	RESERVED2BIT,
	BCRM_ADDRESS,
	RESERVED2BIT_2,
	DEVICE_GUID,
	MANUFACTURER_NAME,
	MODEL_NAME,
	FAMILY_NAME,
	DEVICE_VERSION,
	MANUFACTURER_INFO,
	SERIAL_NUMBER,
	USER_DEFINED_NAME,
	CHECKSUM,
	CHANGE_MODE,
	CURRENT_MODE,
	SOFT_RESET,
	MAX_CMD = SOFT_RESET
};

////////////////////////////////////////////////////////////////////////////////
// STRUCTS
////////////////////////////////////////////////////////////////////////////////
struct cci_cmd {
	__u8 command_index; /* diagnostc test name */
	const __u32 address; /* NULL for no alias name */
	__u32 byte_count;
};

static struct cci_cmd cci_cmd_tbl[MAX_CMD] = {
	/* command index		address */
	{ CCI_REGISTER_LAYOUT_VERSION,	CCI_REG_LAYOUT_VER_32R, 4 },
	{ DEVICE_CAPABILITIES,		CCI_DEVICE_CAP_64R, 8 },
	{ GCPRM_ADDRESS,		CCI_GCPRM_16R, 2 },
	{ BCRM_ADDRESS,			CCI_BCRM_16R, 2 },
	{ DEVICE_GUID,			CCI_DEVICE_GUID_64R, 64 },
	{ MANUFACTURER_NAME,		CCI_MANUF_NAME_64R, 64 },
	{ MODEL_NAME,			CCI_MODEL_NAME_64R, 64 },
	{ FAMILY_NAME,			CCI_FAMILY_NAME_64R, 64 },
	{ DEVICE_VERSION,		CCI_DEVICE_VERSION_64R, 64 },
	{ MANUFACTURER_INFO,		CCI_MANUF_INFO_64R, 64 },
	{ SERIAL_NUMBER,		CCI_SERIAL_NUM_64R, 64 },
	{ USER_DEFINED_NAME,		CCI_USER_DEF_NAME_64R, 64 },
	{ CHECKSUM,			CCI_CHECKSUM_32R, 4 },
	{ CHANGE_MODE,			CCI_CHANGE_MODE_8W, 1 },
	{ CURRENT_MODE,			CCI_CURRENT_MODE_8R, 1 },
	{ SOFT_RESET,			CCI_SOFT_RESET_8W, 1 },
};

struct __attribute__((__packed__)) cci_reg {
	__u32   layout_version;
	__u32   reserved_4bit;
	__u64   device_capabilities;
	__u16   gcprm_address;
	__u16   reserved_2bit;
	__u16   bcrm_addr;
	__u16   reserved_2bit_2;
	char    device_guid[64];
	char    manufacturer_name[64];
	char    model_name[64];
	char    family_name[64];
	char    device_version[64];
	char    manufacturer_info[64];
	char    serial_number[64];
	char    user_defined_name[64];
	__u32   checksum;
	__u8    change_mode;
	__u8    current_mode;
	__u8    soft_reset;
};

struct __attribute__((__packed__)) gencp_reg {
	__u32   gcprm_layout_version;
	__u16   gencp_out_buffer_address;
	__u16   reserved_2byte;
	__u16   gencp_out_buffer_size;
	__u16   reserved_2byte_1;
	__u16	gencp_in_buffer_address;
	__u16   reserved_2byte_2;
	__u16   gencp_in_buffer_size;
	__u16   reserved_2byte_3;
	__u32   checksum;
};


#endif /* ALVIUM_HELPER_H */
