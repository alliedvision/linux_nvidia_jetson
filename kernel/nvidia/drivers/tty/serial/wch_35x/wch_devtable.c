/*
 * Copyright (c) 2021-2023, NVIDIA CORPORATION & AFFILIATES.  All rights reserved.
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

#include "wch_common.h"

#define PCIE_UART_MAX 28

struct pci_board wch_pci_board_conf[] = {
	// NONE
	{
		// VenID	DevID	SubVenID	SubSysID
		0x00,		0x00,	0x00,		0x00,
		// SerPort	IntrBar	IntrOffset	IntrOffset1	IntrOffset2	IntrOffset3	Name	BoardFlag
		0x00,		0x00,	0x00,		0x00,		0x00,		0x00,		"none",	BOARDFLAG_NONE,
		{
			//	type	bar1	ofs1	len1		bar2	ofs2	len2	flags
			{	'n',	-1,		0,		0,			-1,		0,		0,		NONE_BOARD	},
		},
	},

	// CH351_2S
	{
		// VenID			DevID					SubVenID				SubSysID
		VENDOR_ID_WCH_CH351, DEVICE_ID_WCH_CH351_2S, SUB_VENDOR_ID_WCH_CH351, SUB_DEVICE_ID_WCH_CH351_2S,
		// SerPort  IntrBar IntrOffset	IntrOffset1 IntrOffset2 IntrOffset3 Name		BoardFlag
		0x02,		0x00,	0x00,		0x00,		0x00,		0x00,		"CH351_2S",	BOARDFLAG_REMAP,
		{
			// type		bar1	ofs1	len1		bar2	ofs2	len2	flags
			{	's',	0,		0,		8,			-1,		0,		0,		WCH_BOARD_CH351_2S	},
			{	's',	1,		0,		8,			-1,		0,		0,		WCH_BOARD_CH351_2S	},
		},
	},

	// CH352_2S
	{
		// VenID			DevID					SubVenID				SubSysID
		VENDOR_ID_WCH_PCI,	DEVICE_ID_WCH_CH352_2S, SUB_VENDOR_ID_WCH_PCI,	SUB_DEVICE_ID_WCH_CH352_2S,
		// SerPort	IntrBar	IntrOffset	IntrOffset1	IntrOffset2	IntrOffset3	Name		BoardFlag
		0x02,		0x00,	0x00,		0x00,		0x00,		0x00,		"CH352_2S",	BOARDFLAG_REMAP,
		{
			//	type	bar1	ofs1	len1		bar2	ofs2	len2	flags
			{	's',	0,		0,		8,			-1,		0,		0,		WCH_BOARD_CH352_2S	},
			{	's',	1,		0,		8,			-1,		0,		0,		WCH_BOARD_CH352_2S	},
		},
	},

	// CH352_1S1P
	{
		// VenID			DevID						SubVenID				SubSysID
		VENDOR_ID_WCH_PCI,	DEVICE_ID_WCH_CH352_1S1P,	SUB_VENDOR_ID_WCH_PCI,	SUB_DEVICE_ID_WCH_CH352_1S1P,
		// SerPort	IntrBar	IntrOffset	IntrOffset1	IntrOffset2	IntrOffset3	Name			BoardFlag
		1,			0,		0x00,		0x00,		0x00,		0x00,		"CH352_1S1P",	BOARDFLAG_REMAP,
		{
			//	type	bar1	ofs1	len1		bar2	ofs2	len2	flags
			{	's',	0,		0,		8,			-1,		0,		0,		WCH_BOARD_CH352_1S1P	},
		},
	},

	// CH353_4S
	{
		// VenID			DevID					SubVenID				SubSysID
		VENDOR_ID_WCH_PCI,	DEVICE_ID_WCH_CH353_4S, SUB_VENDOR_ID_WCH_PCI,	SUB_DEVICE_ID_WCH_CH353_4S,
		// SerPort	IntrBar	IntrOffset	IntrOffset1	IntrOffset2	IntrOffset3	Name		BoardFlag
		4,			3,		0x0F,		0x00,		0x00,		0x00,		"CH353_4S",	BOARDFLAG_NONE,
		{
			//	type	bar1	ofs1	len1		bar2	ofs2	len2	flags
			{	's',	0,		0,		8,			-1,		0,		0,		WCH_BOARD_CH353_4S	},
			{	's',	1,		0,		8,			-1,		0,		0,		WCH_BOARD_CH353_4S	},
			{	's',	2,		0,		8,			-1,		0,		0,		WCH_BOARD_CH353_4S	},
			{	's',	3,		0,		8,			-1,		0,		0,		WCH_BOARD_CH353_4S	},

		},
	},

	// CH353_2S1P
	{
		// VenID			DevID						SubVenID				SubSysID
		VENDOR_ID_WCH_PCI,	DEVICE_ID_WCH_CH353_2S1P,	SUB_VENDOR_ID_WCH_PCI,	SUB_DEVICE_ID_WCH_CH353_2S1P,
		// SerPort	IntrBar	IntrOffset	IntrOffset1	IntrOffset2	IntrOffset3	Name		    BoardFlag
		2,			3,		0x0F,		0x00,		0x00,		0x00,		"CH353_2S1P",	BOARDFLAG_NONE,
		{
			//	type	bar1	ofs1	len1		bar2	ofs2	len2	flags
			{	's',	0,		0,		8,			-1,		0,		0,		WCH_BOARD_CH353_2S1P  },
			{	's',	1,		0,		8,			-1,		0,		0,		WCH_BOARD_CH353_2S1P  },
		},
	},

	// CH353_2S1PAR
	{
		// VenID			DevID						SubVenID				SubSysID
		VENDOR_ID_WCH_PCI,	DEVICE_ID_WCH_CH353_2S1PAR, SUB_VENDOR_ID_WCH_PCI,	SUB_DEVICE_ID_WCH_CH353_2S1PAR,
		// SerPort	IntrBar	IntrOffset	IntrOffset1	IntrOffset2	IntrOffset3	Name		    BoardFlag
		2,			3,		0x0F,		0x00,		0x00,		0x00,		"CH353_2S1PAR",	BOARDFLAG_NONE,
		{
			//	type	bar1	ofs1	len1		bar2	ofs2	len2	flags
			{	's',	0,		0,		8,			-1,		0,		0,		WCH_BOARD_CH353_2S1PAR	},
			{	's',	1,		0,		8,			-1,		0,		0,		WCH_BOARD_CH353_2S1PAR	},
		},
	},

	// CH355_4S
	{
		// VenID			DevID					SubVenID				SubSysID
		VENDOR_ID_WCH_PCI,	DEVICE_ID_WCH_CH355_4S, SUB_VENDOR_ID_WCH_PCI,	SUB_DEVICE_ID_WCH_CH355_4S,
		// SerPort	IntrBar	IntrOffset	IntrOffset1	IntrOffset2	IntrOffset3	Name		BoardFlag
		4,			4,		0x20,		0x00,		0x00,		0x00,		"CH355_4S",	BOARDFLAG_NONE,
		{
			//	type	bar1	ofs1	len1		bar2	ofs2	len2	flags
			{	's',	0,		0,		8,			-1,		0,		0,		WCH_BOARD_CH355_4S	},
			{	's',	1,		0,		8,			-1,		0,		0,		WCH_BOARD_CH355_4S	},
			{	's',	2,		0,		8,			-1,		0,		0,		WCH_BOARD_CH355_4S	},
			{	's',	3,		0,		8,			-1,		0,		0,		WCH_BOARD_CH355_4S	},

		},
	},

	// CH356_4S1P
	{
		// VenID			DevID						SubVenID				SubSysID
		VENDOR_ID_WCH_PCI,	DEVICE_ID_WCH_CH356_4S1P,	SUB_VENDOR_ID_WCH_PCI,	SUB_DEVICE_ID_WCH_CH356_4S1P,
		// SerPort	IntrBar IntrOffset	IntrOffset1	IntrOffset2	IntrOffset3	Name		  BoardFlag
		4,			4,		 0x3F,		0x00,		0x00,		0x00,		"CH356_4S1P", BOARDFLAG_NONE,
		{
			//	type	bar1	ofs1	len1		bar2	ofs2	len2	flags
			{	's',	0,		0,		8,			-1,		0,		0,		WCH_BOARD_CH356_4S1P	},
			{	's',	1,		0,		8,			-1,		0,		0,		WCH_BOARD_CH356_4S1P	},
			{	's',	2,		0,		8,			-1,		0,		0,		WCH_BOARD_CH356_4S1P	},
			{	's',	3,		0,		8,			-1,		0,		0,		WCH_BOARD_CH356_4S1P	},
		},
	},

	// CH356_6S(CH356+CH432(1P))
	{
		// VenID			DevID					SubVenID				SubSysID
		VENDOR_ID_WCH_PCI,	DEVICE_ID_WCH_CH356_6S, SUB_VENDOR_ID_WCH_PCI,	SUB_DEVICE_ID_WCH_CH356_6S,
		// SerPort	IntrBar	IntrOffset	IntrOffset1	IntrOffset2	IntrOffset3	Name		BoardFlag
		6,			4,		0x3F,		0x00,		0x00,		0x00,		"CH356_6S",	BOARDFLAG_NONE,
		{
			//	type	bar1	ofs1	len1		bar2	ofs2	len2	flags
			{	's',	0,		0,		8,			-1,		0,		0,		WCH_BOARD_CH356_6S	},
			{	's',	1,		0,		8,			-1,		0,		0,		WCH_BOARD_CH356_6S	},
			{	's',	2,		0,		8,			-1,		0,		0,		WCH_BOARD_CH356_6S	},
			{	's',	3,		0,		8,			-1,		0,		0,		WCH_BOARD_CH356_6S	},
			{	's',	4,		0,		8,			-1,		0,		0,		WCH_BOARD_CH356_6S	},
			{	's',	4,		0x08,	8,			-1,		0,		0,		WCH_BOARD_CH356_6S	},
		},
	},

	// CH356_8S(CH356+CH432(2P))
	{
		// VenID			DevID					SubVenID				SubSysID
		VENDOR_ID_WCH_PCI,	DEVICE_ID_WCH_CH356_8S, SUB_VENDOR_ID_WCH_PCI,	SUB_DEVICE_ID_WCH_CH356_8S,
		// SerPort	IntrBar	IntrOffset	IntrOffset1	IntrOffset2	IntrOffset3	Name		BoardFlag
		8,			4,		0x3F,		0x00,		0x00,		0x00,		"CH356_8S",	BOARDFLAG_NONE,
		{
			//	type	bar1	ofs1	len1		bar2	ofs2	len2	flags
			{	's',	0,		0,		8,			-1,		0,		0,		WCH_BOARD_CH356_8S	},
			{	's',	1,		0,		8,			-1,		0,		0,		WCH_BOARD_CH356_8S	},
			{	's',	2,		0,		8,			-1,		0,		0,		WCH_BOARD_CH356_8S	},
			{	's',	3,		0,		8,			-1,		0,		0,		WCH_BOARD_CH356_8S	},
			{	's',	4,		0,		8,			-1,		0,		0,		WCH_BOARD_CH356_8S	},
			{	's',	4,		0x08,	8,			-1,		0,		0,		WCH_BOARD_CH356_8S	},
			{	's',	4,		0x10,	8,			-1,		0,		0,		WCH_BOARD_CH356_8S	},
			{	's',	4,		0x18,	8,			-1,		0,		0,		WCH_BOARD_CH356_8S	},
		},
	},

	// CH357_4S
	{
		// VenID			DevID					SubVenID				SubSysID
		VENDOR_ID_WCH_PCI,	DEVICE_ID_WCH_CH357_4S, SUB_VENDOR_ID_WCH_PCI,	SUB_DEVICE_ID_WCH_CH357_4S,
		// SerPort	IntrBar	IntrOffset	IntrOffset1	IntrOffset2	IntrOffset3	Name		BoardFlag
		4,			4,		0x6F,		0x00,		0x00,		0x00,		"CH357_4S",	BOARDFLAG_NONE,
		{
			//	type	bar1	ofs1	len1		bar2	ofs2	len2	flags
			{	's',	0,		0,		8,			-1,		0,		0,		WCH_BOARD_CH357_4S	},
			{	's',	1,		0,		8,			-1,		0,		0,		WCH_BOARD_CH357_4S	},
			{	's',	2,		0,		8,			-1,		0,		0,		WCH_BOARD_CH357_4S	},
			{	's',	3,		0,		8,			-1,		0,		0,		WCH_BOARD_CH357_4S	},
		},
	},

	// CH358_4S1P
	{
		// VenID			DevID						SubVenID				SubSysID
		VENDOR_ID_WCH_PCI,	DEVICE_ID_WCH_CH358_4S1P,	SUB_VENDOR_ID_WCH_PCI,	SUB_DEVICE_ID_WCH_CH358_4S1P,
		// SerPort	IntrBar	IntrOffset	IntrOffset1	IntrOffset2	IntrOffset3	Name			BoardFlag
		4,			4,		0x6F,		0x00,		0x00,		0x00,		"CH358_4S1P",	BOARDFLAG_NONE,
		{
			//	type	bar1	ofs1	len1		bar2	ofs2	len2	flags
			{	's',	0,		0,		8,			-1,		0,		0,		WCH_BOARD_CH358_4S1P	},
			{	's',	1,		0,		8,			-1,		0,		0,		WCH_BOARD_CH358_4S1P	},
			{	's',	2,		0,		8,			-1,		0,		0,		WCH_BOARD_CH358_4S1P	},
			{	's',	3,		0,		8,			-1,		0,		0,		WCH_BOARD_CH358_4S1P	},
		},
	},

	// CH358_8S
	{
		// VenID			DevID					SubVenID				SubSysID
		VENDOR_ID_WCH_PCI,	DEVICE_ID_WCH_CH358_8S, SUB_VENDOR_ID_WCH_PCI,	SUB_DEVICE_ID_WCH_CH358_8S,
		// SerPort	IntrBar IntrOffset	IntrOffset1	IntrOffset2	IntrOffset3	Name		BoardFlag
		8,			4,		0x6F,		0x00,		0x00,		0x00,		"CH358_8S", BOARDFLAG_NONE,
		{
			//	type	bar1	ofs1	len1		bar2	ofs2	len2	flags
			{	's',	0,		0,		8,			-1,		0,		0,		WCH_BOARD_CH358_8S	},
			{	's',	1,		0,		8,			-1,		0,		0,		WCH_BOARD_CH358_8S	},
			{	's',	2,		0,		8,			-1,		0,		0,		WCH_BOARD_CH358_8S	},
			{	's',	3,		0,		8,			-1,		0,		0,		WCH_BOARD_CH358_8S	},
			{	's',	0,		8,		8,			-1,		0,		0,		WCH_BOARD_CH358_8S	},
			{	's',	1,		8,		8,			-1,		0,		0,		WCH_BOARD_CH358_8S	},
			{	's',	2,		8,		8,			-1,		0,		0,		WCH_BOARD_CH358_8S	},
			{	's',	3,		8,		8,			-1,		0,		0,		WCH_BOARD_CH358_8S	},
		},
	},

	// CH359_16S
	{
		// VenID			DevID					SubVenID				SubSysID
		VENDOR_ID_WCH_PCI,	DEVICE_ID_WCH_CH359_16S, SUB_VENDOR_ID_WCH_PCI,	SUB_DEVICE_ID_WCH_CH359_16S,
		// SerPort	IntrBar IntrOffset	IntrOffset1	IntrOffset2	IntrOffset3 Name		BoardFlag
		16,			4,		0x6F,		0x00,		0x00,		0x00,		"CH359_16S", BOARDFLAG_NONE,
		{
			//	type	bar1	ofs1	len1		bar2	ofs2	len2	flags
			{	's',	0,		0,		8,			-1,		0,		0,		WCH_BOARD_CH359_16S	},
			{	's',	1,		0,		8,			-1,		0,		0,		WCH_BOARD_CH359_16S	},
			{	's',	2,		0,		8,			-1,		0,		0,		WCH_BOARD_CH359_16S	},
			{	's',	3,		0,		8,			-1,		0,		0,		WCH_BOARD_CH359_16S	},
			{	's',	0,		8,		8,			-1,		0,		0,		WCH_BOARD_CH359_16S	},
			{	's',	1,		8,		8,			-1,		0,		0,		WCH_BOARD_CH359_16S	},
			{	's',	2,		8,		8,			-1,		0,		0,		WCH_BOARD_CH359_16S	},
			{	's',	3,		8,		8,			-1,		0,		0,		WCH_BOARD_CH359_16S	},

			{	's',	4,		0,		8,			-1,		0,		0,		WCH_BOARD_CH359_16S	},
			{	's',	4,		16,		8,			-1,		0,		0,		WCH_BOARD_CH359_16S	},
			{	's',	4,		32,		8,			-1,		0,		0,		WCH_BOARD_CH359_16S	},
			{	's',	4,		48,		8,			-1,		0,		0,		WCH_BOARD_CH359_16S	},
			{	's',	4,		8,		8,			-1,		0,		0,		WCH_BOARD_CH359_16S	},
			{	's',	4,		24,		8,			-1,		0,		0,		WCH_BOARD_CH359_16S	},
			{	's',	4,		40,		8,			-1,		0,		0,		WCH_BOARD_CH359_16S	},
			{	's',	4,		56,		8,			-1,		0,		0,		WCH_BOARD_CH359_16S	},
		},
	},

	// CH382_2S
	{
		// VenID			DevID					SubVenID				SubSysID
		VENDOR_ID_WCH_PCIE,	DEVICE_ID_WCH_CH382_2S,	SUB_VENDOR_ID_WCH_PCIE,	SUB_DEVICE_ID_WCH_CH382_2S,
		// SerPort	IntrBar IntrOffset	IntrOffset1	IntrOffset2	IntrOffset3	Name			BoardFlag
		2,			0,		0xE9,		0x00,		0x00,		0x00,		"CH382_2S",	BOARDFLAG_NONE,
		{
			//	type	bar1	ofs1	len1		bar2	ofs2	len2	flags
			{	's',	0,		0xC0,	8,			-1,		0,		0,		WCH_BOARD_CH382_2S	},
			{	's',	0,		0xC8,	8,			-1,		0,		0,		WCH_BOARD_CH382_2S	},
		},
	},

	// CH382_2S1P
	{
		// VenID			DevID						SubVenID				SubSysID
		VENDOR_ID_WCH_PCIE,	DEVICE_ID_WCH_CH382_2S1P,	SUB_VENDOR_ID_WCH_PCIE,	SUB_DEVICE_ID_WCH_CH382_2S1P,
		// SerPort	IntrBar IntrOffset	IntrOffset1	IntrOffset2	IntrOffset3 Name			BoardFlag
		2,			0,		0xE9,		0x00,		0x00,		0x00,		"CH382_2S1P",	BOARDFLAG_NONE,
		{
			//	type	bar1	ofs1	len1		bar2	ofs2	len2	flags
			{	's',	0,		0xC0,	8,			-1,		0,		0,		WCH_BOARD_CH382_2S1P  },
			{	's',	0,		0xC8,	8,			-1,		0,		0,		WCH_BOARD_CH382_2S1P  },
		},
	},


	// CH384_4S
	{
		// VenID			DevID					SubVenID				SubSysID
		VENDOR_ID_WCH_PCIE,	DEVICE_ID_WCH_CH384_4S,	SUB_VENDOR_ID_WCH_PCIE,	SUB_DEVICE_ID_WCH_CH384_4S,
		// SerPort	IntrBar IntrOffset	IntrOffset1	IntrOffset2	IntrOffset3	Name			BoardFlag
		4,			0,		0xE9,		0x00,		0x00,		0x00,		"CH384_4S",	BOARDFLAG_NONE,
		{
			//	type	bar1	ofs1	len1		bar2	ofs2	len2	flags
			{	's',	0,		0xC0,	8,			-1,		0,		0,		WCH_BOARD_CH384_4S },
			{	's',	0,		0xC8,	8,			-1,		0,		0,		WCH_BOARD_CH384_4S },
			{	's',	0,		0xD0,	8,			-1,		0,		0,		WCH_BOARD_CH384_4S },
			{	's',	0,		0xD8,	8,			-1,		0,		0,		WCH_BOARD_CH384_4S },
		},
	},

	// CH384_4S1P
	{
		// VenID			DevID						SubVenID				SubSysID
		VENDOR_ID_WCH_PCIE,	DEVICE_ID_WCH_CH384_4S1P,	SUB_VENDOR_ID_WCH_PCIE,	SUB_DEVICE_ID_WCH_CH384_4S1P,
		// SerPort	IntrBar IntrOffset	IntrOffset1	IntrOffset2	IntrOffset3 Name			BoardFlag
		4,			0,		0xE9,		0x00,		0x00,		0x00,		"CH384_4S1P",	BOARDFLAG_NONE,
		{
			//	type	bar1	ofs1	len1		bar2	ofs2	len2	flags
			{	's',	0,		0xC0,	8,			-1,		0,		0,		WCH_BOARD_CH384_4S1P  },
			{	's',	0,		0xC8,	8,			-1,		0,		0,		WCH_BOARD_CH384_4S1P  },
			{	's',	0,		0xD0,	8,			-1,		0,		0,		WCH_BOARD_CH384_4S1P  },
			{	's',	0,		0xD8,	8,			-1,		0,		0,		WCH_BOARD_CH384_4S1P  },
		},
	},

	// CH384_8S
	{
		// VenID			DevID					SubVenID				SubSysID
		VENDOR_ID_WCH_PCIE,	DEVICE_ID_WCH_CH384_8S,	SUB_VENDOR_ID_WCH_PCIE,	SUB_DEVICE_ID_WCH_CH384_8S,
		// SerPort	IntrBar IntrOffset	IntrOffset1	IntrOffset2	IntrOffset3 Name		BoardFlag
		8,			0,		0xE0,		0x00,		0x00,		0x00,		"CH384_8S",	BOARDFLAG_CH384_8_PORTS,
		{
			//	type	bar1	ofs1	len1		bar2	ofs2	len2	flags
			{	's',	0,		0x00,	8,			-1,		0,		0,		WCH_BOARD_CH384_8S	},
			{	's',	0,		0x10,	8,			-1,		0,		0,		WCH_BOARD_CH384_8S	},
			{	's',	0,		0x20,	8,			-1,		0,		0,		WCH_BOARD_CH384_8S	},
			{	's',	0,		0x30,	8,			-1,		0,		0,		WCH_BOARD_CH384_8S	},
			{	's',	0,		0x08,	8,			-1,		0,		0,		WCH_BOARD_CH384_8S	},
			{	's',	0,		0x18,	8,			-1,		0,		0,		WCH_BOARD_CH384_8S	},
			{	's',	0,		0x28,	8,			-1,		0,		0,		WCH_BOARD_CH384_8S	},
			{	's',	0,		0x38,	8,			-1,		0,		0,		WCH_BOARD_CH384_8S	},
		},
	},

	// CH384_28S
	{
		// VenID			DevID					 SubVenID				 SubSysID
		VENDOR_ID_WCH_PCIE,	DEVICE_ID_WCH_CH384_28S, SUB_VENDOR_ID_WCH_PCIE, SUB_DEVICE_ID_WCH_CH384_28S,
		// SerPort				IntrBar IntrOffset 	IntrOffset1	IntrOffset2	IntrOffset3	Name		 BoardFlag
		PCIE_UART_MAX,		0,		0xE9,		0xE0,		0xE4,		0xE6,		"CH384_28S", BOARDFLAG_CH384_28_PORTS,
		{
			//	type	bar1	ofs1	len1		bar2	ofs2	len2	flags
			{	's',	0,		0xC0,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },
			{	's',	0,		0xC8,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },
			{	's',	0,		0xD0,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },
			{	's',	0,		0xD8,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },

			{	's',	0,		0x00,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },
			{	's',	0,		0x10,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },
			{	's',	0,		0x20,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },
			{	's',	0,		0x30,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },
			{	's',	0,		0x08,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },
			{	's',	0,		0x18,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },
			{	's',	0,		0x28,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },
			{	's',	0,		0x38,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },

			{	's',	0,		0x40,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },
			{	's',	0,		0x50,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },
			{	's',	0,		0x60,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },
			{	's',	0,		0x70,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },
			{	's',	0,		0x48,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },
			{	's',	0,		0x58,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },
			{	's',	0,		0x68,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },
			{	's',	0,		0x78,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },

			{	's',	0,		0x80,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },
			{	's',	0,		0x90,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },
			{	's',	0,		0xA0,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },
			{	's',	0,		0xB0,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },
			{	's',	0,		0x88,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },
			{	's',	0,		0x98,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },
			{	's',	0,		0xA8,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },
			{	's',	0,		0xB8,	8,			-1,		0,		0,		WCH_BOARD_CH384_28S	 },
		},
	},

	// CH365_32S
	{
		// VenID			DevID					 SubVenID				SubSysID
		VENDOR_ID_WCH_PCI,	DEVICE_ID_WCH_CH365_32S, SUB_VENDOR_ID_WCH_PCI, SUB_DEVICE_ID_WCH_CH365_32S,
		// SerPort	IntrBar IntrOffset 	IntrOffset1  IntrOffset2 IntrOffset3 Name		  BoardFlag
		32,			1,		0x00,		0x00,		 0x00,		 0x00,		 "CH365_32S", BOARDFLAG_CH365_32_PORTS,
		{
			//	type	bar1	ofs1	len1		bar2	ofs2	len2	flags
			{	's',	0,		0x00,	0,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	0,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	0,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	0,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	0,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	0,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	0,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	0,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },

			{	's',	0,		0x00,	0,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	0,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	0,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	0,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	0,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	0,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	8,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	8,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },

			{	's',	0,		0x00,	8,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	8,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	8,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	8,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	8,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	8,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	8,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	8,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },

			{	's',	0,		0x00,	8,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	8,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	8,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	8,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	8,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	8,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	8,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
			{	's',	0,		0x00,	8,			-1,		0,		0,		WCH_BOARD_CH365_32S	 },
		},
	},
};
