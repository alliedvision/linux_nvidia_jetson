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

/*
 * This file contains list of Memory Controller Client IDs used in Tegra234.
 * There is duplicate copy of this file present in
 * kernel/nvidia-t23x/include/dt-bindings/memory/tegra234-mc.h
 * make sure to update both to keep it in sync.
 */

#ifndef DT_BINDINGS_MEMORY_TEGRA234_MC_H
#define DT_BINDINGS_MEMORY_TEGRA234_MC_H

/*
 * memory client IDs
 */

/* Misses from System Memory Management Unit (SMMU) Page Table Cache (PTC) */
#define TEGRA234_MEMORY_CLIENT_PTCR 0x00
/* MSS internal memqual MIU7 read clients */
#define TEGRA234_MEMORY_CLIENT_MIU7R 0x01
/* MSS internal memqual MIU7 write clients */
#define TEGRA234_MEMORY_CLIENT_MIU7W 0x02
/* MSS internal memqual MIU8 read clients */
#define TEGRA234_MEMORY_CLIENT_MIU8R 0x03
/* MSS internal memqual MIU8 write clients */
#define TEGRA234_MEMORY_CLIENT_MIU8W 0x04
/* MSS internal memqual MIU9 read clients */
#define TEGRA234_MEMORY_CLIENT_MIU9R 0x05
/* MSS internal memqual MIU9 write clients */
#define TEGRA234_MEMORY_CLIENT_MIU9W 0x06
/* MSS internal memqual MIU10 read clients */
#define TEGRA234_MEMORY_CLIENT_MIU10R 0x07
/* MSS internal memqual MIU10 write clients */
#define TEGRA234_MEMORY_CLIENT_MIU10W 0x08
/* MSS internal memqual MIU11 read clients */
#define TEGRA234_MEMORY_CLIENT_MIU11R 0x09
/* MSS internal memqual MIU11 write clients */
#define TEGRA234_MEMORY_CLIENT_MIU11W 0x0a
/* MSS internal memqual MIU12 read clients */
#define TEGRA234_MEMORY_CLIENT_MIU12R 0x0b
/* MSS internal memqual MIU12 write clients */
#define TEGRA234_MEMORY_CLIENT_MIU12W 0x0c
/* MSS internal memqual MIU13 read clients */
#define TEGRA234_MEMORY_CLIENT_MIU13R 0x0d
/* MSS internal memqual MIU13 write clients */
#define TEGRA234_MEMORY_CLIENT_MIU13W 0x0e
#define TEGRA234_MEMORY_CLIENT_NVL5RHP 0x13
#define TEGRA234_MEMORY_CLIENT_NVL5R 0x14
/* High-definition audio (HDA) read clients */
#define TEGRA234_MEMORY_CLIENT_HDAR 0x15
/* Host channel data read clients */
#define TEGRA234_MEMORY_CLIENT_HOST1XDMAR 0x16
#define TEGRA234_MEMORY_CLIENT_NVL5W 0x17
#define TEGRA234_MEMORY_CLIENT_NVL6RHP 0x18
#define TEGRA234_MEMORY_CLIENT_NVL6R 0x19
#define TEGRA234_MEMORY_CLIENT_NVL6W 0x1a
#define TEGRA234_MEMORY_CLIENT_NVL7RHP 0x1b
#define TEGRA234_MEMORY_CLIENT_NVENCSRD 0x1c
#define TEGRA234_MEMORY_CLIENT_NVL7R 0x1d
#define TEGRA234_MEMORY_CLIENT_NVL7W 0x1e
#define TEGRA234_MEMORY_CLIENT_NVL8RHP 0x20
#define TEGRA234_MEMORY_CLIENT_NVL8R 0x21
#define TEGRA234_MEMORY_CLIENT_NVL8W 0x22
#define TEGRA234_MEMORY_CLIENT_NVL9RHP 0x23
#define TEGRA234_MEMORY_CLIENT_NVL9R 0x24
#define TEGRA234_MEMORY_CLIENT_NVL9W 0x25
/* Reads from Cortex-A9 4 CPU cores via the L2 cache */
#define TEGRA234_MEMORY_CLIENT_MPCORER 0x27
/* PCIE6 Read clients */
#define TEGRA234_MEMORY_CLIENT_PCIE6AR 0x28
/* VIFAL writes */
#define TEGRA234_MEMORY_CLIENT_PCIE6AW 0x29
/* PCIE7 Read clients */
#define TEGRA234_MEMORY_CLIENT_PCIE7AR 0x2a
#define TEGRA234_MEMORY_CLIENT_NVENCSWR 0x2b
/* DLA0ARDB read clinets */
#define TEGRA234_MEMORY_CLIENT_DLA0RDB 0x2c
/* DLA0ARDB1 read clinets */
#define TEGRA234_MEMORY_CLIENT_DLA0RDB1 0x2d
/* DLA0 writes */
#define TEGRA234_MEMORY_CLIENT_DLA0WDB 0x2e
/* DLA1ARDB read clinets */
#define TEGRA234_MEMORY_CLIENT_DLA1RDB 0x2f
/* PCIE 7 writes */
#define TEGRA234_MEMORY_CLIENT_PCIE7AW 0x30
/* PCIE8 Read clients */
#define TEGRA234_MEMORY_CLIENT_PCIE8AR 0x32
/* High-definition audio (HDA) write clients */
#define TEGRA234_MEMORY_CLIENT_HDAW 0x35
/* Writes from Cortex-A9 4 CPU cores via the L2 cache */
#define TEGRA234_MEMORY_CLIENT_MPCOREW 0x39
/* OFAA client */
#define TEGRA234_MEMORY_CLIENT_OFAR1 0x3a
/* PCIE 8 writes */
#define TEGRA234_MEMORY_CLIENT_PCIE8AW 0x3b
/* PCIE9 Read clients */
#define TEGRA234_MEMORY_CLIENT_PCIE9AR 0x3c
/* PCIE6r1 Read clients */
#define TEGRA234_MEMORY_CLIENT_PCIE6AR1 0x3d
/* PCIE 9 writes */
#define TEGRA234_MEMORY_CLIENT_PCIE9AW 0x3e
/* PCIE10 Read clients */
#define TEGRA234_MEMORY_CLIENT_PCIE10AR 0x3f
/* PCIE 10 writes */
#define TEGRA234_MEMORY_CLIENT_PCIE10AW 0x40
/* ISP read client for Crossbar A */
#define TEGRA234_MEMORY_CLIENT_ISPRA 0x44
/* ISP read client 1 for Crossbar A */
#define TEGRA234_MEMORY_CLIENT_ISPFALR 0x45
/* ISP Write client for Crossbar A */
#define TEGRA234_MEMORY_CLIENT_ISPWA 0x46
/* ISP Write client Crossbar B */
#define TEGRA234_MEMORY_CLIENT_ISPWB 0x47
/* PCIE10ar1 Read clients */
#define TEGRA234_MEMORY_CLIENT_PCIE10AR1 0x48
/* PCIE7ar1 Read clients */
#define TEGRA234_MEMORY_CLIENT_PCIE7AR1 0x49
/* XUSB_HOST read clients */
#define TEGRA234_MEMORY_CLIENT_XUSB_HOSTR 0x4a
/* XUSB_HOST write clients */
#define TEGRA234_MEMORY_CLIENT_XUSB_HOSTW 0x4b
/* XUSB read clients */
#define TEGRA234_MEMORY_CLIENT_XUSB_DEVR 0x4c
/* XUSB_DEV write clients */
#define TEGRA234_MEMORY_CLIENT_XUSB_DEVW 0x4d
/* TSEC Memory Return Data Client Description */
#define TEGRA234_MEMORY_CLIENT_TSECSRD 0x54
/* TSEC Memory Write Client Description */
#define TEGRA234_MEMORY_CLIENT_TSECSWR 0x55
/* XSPI writes */
#define TEGRA234_MEMORY_CLIENT_XSPI1W 0x56
/* MGBE0 client */
#define TEGRA234_MEMORY_CLIENT_MGBEARD 0x58
/* MGBEB client */
#define TEGRA234_MEMORY_CLIENT_MGBEBRD 0x59
/* MGBEC client */
#define TEGRA234_MEMORY_CLIENT_MGBECRD 0x5a
/* MGBED client */
#define TEGRA234_MEMORY_CLIENT_MGBEDRD 0x5b
/* MGBE0 writes */
#define TEGRA234_MEMORY_CLIENT_MGBEAWR 0x5c
/* OFAA client */
#define TEGRA234_MEMORY_CLIENT_OFAR 0x5d
/* OFAA writes */
#define TEGRA234_MEMORY_CLIENT_OFAW 0x5e
/* MGBEB writes */
#define TEGRA234_MEMORY_CLIENT_MGBEBWR 0x5f
/* sdmmca memory read client */
#define TEGRA234_MEMORY_CLIENT_SDMMCRA 0x60
/* MGBEC writes */
#define TEGRA234_MEMORY_CLIENT_MGBECWR 0x61
/* sdmmcd memory read client */
#define TEGRA234_MEMORY_CLIENT_SDMMCRAB 0x63
/* sdmmca memory write client */
#define TEGRA234_MEMORY_CLIENT_SDMMCWA 0x64
/* MGBED writes */
#define TEGRA234_MEMORY_CLIENT_MGBEDWR 0x65
/* sdmmcd memory write client */
#define TEGRA234_MEMORY_CLIENT_SDMMCWAB 0x67
/* SE Memory Return Data Client Description */
#define TEGRA234_MEMORY_CLIENT_SEU1RD 0x68
/* SE Memory Write Client Description */
#define TEGRA234_MEMORY_CLIENT_SUE1WR 0x69
#define TEGRA234_MEMORY_CLIENT_VICSRD 0x6c
#define TEGRA234_MEMORY_CLIENT_VICSWR 0x6d
/* DLA1ARDB1 read clinets */
#define TEGRA234_MEMORY_CLIENT_DLA1RDB1 0x6e
/* DLA1 writes */
#define TEGRA234_MEMORY_CLIENT_DLA1WRB 0x6f
/* VI FLACON read clinets */
#define TEGRA234_MEMORY_CLIENT_VI2FALR 0x71
/* VI Write client */
#define TEGRA234_MEMORY_CLIENT_VI2W 0x70
/* VI Write client */
#define TEGRA234_MEMORY_CLIENT_VIW 0x72
/* NISO display read client */
#define TEGRA234_MEMORY_CLIENT_NVDISPNISOR 0x73
/* NVDISPNISO writes */
#define TEGRA234_MEMORY_CLIENT_NVDISPNISOW 0x74
/* XSPI client */
#define TEGRA234_MEMORY_CLIENT_XSPI0R 0x75
/* XSPI writes */
#define TEGRA234_MEMORY_CLIENT_XSPI0W 0x76
/* XSPI client */
#define TEGRA234_MEMORY_CLIENT_XSPI1R 0x77
#define TEGRA234_MEMORY_CLIENT_NVDECSRD 0x78
#define TEGRA234_MEMORY_CLIENT_NVDECSWR 0x79
/* Audio Processing (APE) engine read clients */
#define TEGRA234_MEMORY_CLIENT_APER 0x7a
/* Audio Processing (APE) engine write clients */
#define TEGRA234_MEMORY_CLIENT_APEW 0x7b
/* VI2FAL writes */
#define TEGRA234_MEMORY_CLIENT_VI2FALW 0x7c
#define TEGRA234_MEMORY_CLIENT_NVJPGSRD 0x7e
#define TEGRA234_MEMORY_CLIENT_NVJPGSWR 0x7f
/* SE Memory Return Data Client Description */
#define TEGRA234_MEMORY_CLIENT_SESRD 0x80
/* SE Memory Write Client Description */
#define TEGRA234_MEMORY_CLIENT_SESWR 0x81
/* AXI AP and DFD-AUX0/1 read clients Both share the same interface on the on MSS */
#define TEGRA234_MEMORY_CLIENT_AXIAPR 0x82
/* AXI AP and DFD-AUX0/1 write clients Both sahre the same interface on MSS */
#define TEGRA234_MEMORY_CLIENT_AXIAPW 0x83
/* ETR read clients */
#define TEGRA234_MEMORY_CLIENT_ETRR 0x84
/* ETR write clients */
#define TEGRA234_MEMORY_CLIENT_ETRW 0x85
/* AXI Switch read client */
#define TEGRA234_MEMORY_CLIENT_AXISR 0x8c
/* AXI Switch write client */
#define TEGRA234_MEMORY_CLIENT_AXISW 0x8d
/* EQOS read client */
#define TEGRA234_MEMORY_CLIENT_EQOSR 0x8e
/* EQOS write client */
#define TEGRA234_MEMORY_CLIENT_EQOSW 0x8f
/* UFSHC read client */
#define TEGRA234_MEMORY_CLIENT_UFSHCR 0x90
/* UFSHC write client */
#define TEGRA234_MEMORY_CLIENT_UFSHCW 0x91
/* NVDISPLAY read client */
#define TEGRA234_MEMORY_CLIENT_NVDISPLAYR 0x92
/* BPMP read client */
#define TEGRA234_MEMORY_CLIENT_BPMPR 0x93
/* BPMP write client */
#define TEGRA234_MEMORY_CLIENT_BPMPW 0x94
/* BPMPDMA read client */
#define TEGRA234_MEMORY_CLIENT_BPMPDMAR 0x95
/* BPMPDMA write client */
#define TEGRA234_MEMORY_CLIENT_BPMPDMAW 0x96
/* AON read client */
#define TEGRA234_MEMORY_CLIENT_AONR 0x97
/* AON write client */
#define TEGRA234_MEMORY_CLIENT_AONW 0x98
/* AONDMA read client */
#define TEGRA234_MEMORY_CLIENT_AONDMAR 0x99
/* AONDMA write client */
#define TEGRA234_MEMORY_CLIENT_AONDMAW 0x9a
/* SCE read client */
#define TEGRA234_MEMORY_CLIENT_SCER 0x9b
/* SCE write client */
#define TEGRA234_MEMORY_CLIENT_SCEW 0x9c
/* SCEDMA read client */
#define TEGRA234_MEMORY_CLIENT_SCEDMAR 0x9d
/* SCEDMA write client */
#define TEGRA234_MEMORY_CLIENT_SCEDMAW 0x9e
/* APEDMA read client */
#define TEGRA234_MEMORY_CLIENT_APEDMAR 0x9f
/* APEDMA write client */
#define TEGRA234_MEMORY_CLIENT_APEDMAW 0xa0
/* NVDISPLAY read client instance 2 */
#define TEGRA234_MEMORY_CLIENT_NVDISPLAYR1 0xa1
#define TEGRA234_MEMORY_CLIENT_VICSRD1 0xa2
/* MSS internal memqual MIU0 read clients */
#define TEGRA234_MEMORY_CLIENT_MIU0R 0xa6
/* MSS internal memqual MIU0 write clients */
#define TEGRA234_MEMORY_CLIENT_MIU0W 0xa7
/* MSS internal memqual MIU1 read clients */
#define TEGRA234_MEMORY_CLIENT_MIU1R 0xa8
/* MSS internal memqual MIU1 write clients */
#define TEGRA234_MEMORY_CLIENT_MIU1W 0xa9
/* MSS internal memqual MIU2 read clients */
#define TEGRA234_MEMORY_CLIENT_MIU2R 0xae
/* MSS internal memqual MIU2 write clients */
#define TEGRA234_MEMORY_CLIENT_MIU2W 0xaf
/* MSS internal memqual MIU3 read clients */
#define TEGRA234_MEMORY_CLIENT_MIU3R 0xb0
/* MSS internal memqual MIU3 write clients */
#define TEGRA234_MEMORY_CLIENT_MIU3W 0xb1
/* MSS internal memqual MIU4 read clients */
#define TEGRA234_MEMORY_CLIENT_MIU4R 0xb2
/* MSS internal memqual MIU4 write clients */
#define TEGRA234_MEMORY_CLIENT_MIU4W 0xb3
#define TEGRA234_MEMORY_CLIENT_DPMUR 0xb4
#define TEGRA234_MEMORY_CLIENT_DPMUW 0xb5
#define TEGRA234_MEMORY_CLIENT_NVL0R 0xb6
#define TEGRA234_MEMORY_CLIENT_NVL0W 0xb7
#define TEGRA234_MEMORY_CLIENT_NVL1R 0xb8
#define TEGRA234_MEMORY_CLIENT_NVL1W 0xb9
#define TEGRA234_MEMORY_CLIENT_NVL2R 0xba
#define TEGRA234_MEMORY_CLIENT_NVL2W 0xbb
/* VI FLACON read clients */
#define TEGRA234_MEMORY_CLIENT_VIFALR 0xbc
/* VIFAL write clients */
#define TEGRA234_MEMORY_CLIENT_VIFALW 0xbd
/* DLA0ARDA read clients */
#define TEGRA234_MEMORY_CLIENT_DLA0RDA 0xbe
/* DLA0 Falcon read clients */
#define TEGRA234_MEMORY_CLIENT_DLA0FALRDB 0xbf
/* DLA0 write clients */
#define TEGRA234_MEMORY_CLIENT_DLA0WRA 0xc0
/* DLA0 write clients */
#define TEGRA234_MEMORY_CLIENT_DLA0FALWRB 0xc1
/* DLA1ARDA read clients */
#define TEGRA234_MEMORY_CLIENT_DLA1RDA 0xc2
/* DLA1 Falcon read clients */
#define TEGRA234_MEMORY_CLIENT_DLA1FALRDB 0xc3
/* DLA1 write clients */
#define TEGRA234_MEMORY_CLIENT_DLA1WRA 0xc4
/* DLA1 write clients */
#define TEGRA234_MEMORY_CLIENT_DLA1FALWRB 0xc5
/* PVA0RDA read clients */
#define TEGRA234_MEMORY_CLIENT_PVA0RDA 0xc6
/* PVA0RDB read clients */
#define TEGRA234_MEMORY_CLIENT_PVA0RDB 0xc7
/* PVA0RDC read clients */
#define TEGRA234_MEMORY_CLIENT_PVA0RDC 0xc8
/* PVA0WRA write clients */
#define TEGRA234_MEMORY_CLIENT_PVA0WRA 0xc9
/* PVA0WRB write clients */
#define TEGRA234_MEMORY_CLIENT_PVA0WRB 0xca
/* PVA0WRC write clients */
#define TEGRA234_MEMORY_CLIENT_PVA0WRC 0xcb
/* RCE read client */
#define TEGRA234_MEMORY_CLIENT_RCER 0xd2
/* RCE write client */
#define TEGRA234_MEMORY_CLIENT_RCEW 0xd3
/* RCEDMA read client */
#define TEGRA234_MEMORY_CLIENT_RCEDMAR 0xd4
/* RCEDMA write client */
#define TEGRA234_MEMORY_CLIENT_RCEDMAW 0xd5
/* PCIE0 read clients */
#define TEGRA234_MEMORY_CLIENT_PCIE0R 0xd8
/* PCIE0 write clients */
#define TEGRA234_MEMORY_CLIENT_PCIE0W 0xd9
/* PCIE1 read clients */
#define TEGRA234_MEMORY_CLIENT_PCIE1R 0xda
/* PCIE1 write clients */
#define TEGRA234_MEMORY_CLIENT_PCIE1W 0xdb
/* PCIE2 read clients */
#define TEGRA234_MEMORY_CLIENT_PCIE2AR 0xdc
/* PCIE2 write clients */
#define TEGRA234_MEMORY_CLIENT_PCIE2AW 0xdd
/* PCIE3 read clients */
#define TEGRA234_MEMORY_CLIENT_PCIE3R 0xde
/* PCIE3 write clients */
#define TEGRA234_MEMORY_CLIENT_PCIE3W 0xdf
/* PCIE4 read clients */
#define TEGRA234_MEMORY_CLIENT_PCIE4R 0xe0
/* PCIE4 write clients */
#define TEGRA234_MEMORY_CLIENT_PCIE4W 0xe1
/* PCIE5 read clients */
#define TEGRA234_MEMORY_CLIENT_PCIE5R 0xe2
/* PCIE5 write clients */
#define TEGRA234_MEMORY_CLIENT_PCIE5W 0xe3
/* ISP read client 1 for Crossbar A */
#define TEGRA234_MEMORY_CLIENT_ISPFALW 0xe4
#define TEGRA234_MEMORY_CLIENT_NVL3R 0xe5
#define TEGRA234_MEMORY_CLIENT_NVL3W 0xe6
#define TEGRA234_MEMORY_CLIENT_NVL4R 0xe7
#define TEGRA234_MEMORY_CLIENT_NVL4W 0xe8
/* DLA0ARDA1 read clients */
#define TEGRA234_MEMORY_CLIENT_DLA0RDA1 0xe9
/* DLA1ARDA1 read clients */
#define TEGRA234_MEMORY_CLIENT_DLA1RDA1 0xea
/* PVA0RDA1 read clients */
#define TEGRA234_MEMORY_CLIENT_PVA0RDA1 0xeb
/* PVA0RDB1 read clients */
#define TEGRA234_MEMORY_CLIENT_PVA0RDB1 0xec
/* PCIE5r1 read clients */
#define TEGRA234_MEMORY_CLIENT_PCIE5R1 0xef
#define TEGRA234_MEMORY_CLIENT_NVENCSRD1 0xf0
/* ISP read client for Crossbar A */
#define TEGRA234_MEMORY_CLIENT_ISPRA1 0xf2
#define TEGRA234_MEMORY_CLIENT_NVL0RHP 0xf4
#define TEGRA234_MEMORY_CLIENT_NVL1RHP 0xf5
#define TEGRA234_MEMORY_CLIENT_NVL2RHP 0xf6
#define TEGRA234_MEMORY_CLIENT_NVL3RHP 0xf7
#define TEGRA234_MEMORY_CLIENT_NVL4RHP 0xf8
/* MSS internal memqual MIU5 read clients */
#define TEGRA234_MEMORY_CLIENT_MIU5R 0xfc
/* MSS internal memqual MIU5 write clients */
#define TEGRA234_MEMORY_CLIENT_MIU5W 0xfd
/* MSS internal memqual MIU6 read clients */
#define TEGRA234_MEMORY_CLIENT_MIU6R 0xfe
/* MSS internal memqual MIU6 write clients */
#define TEGRA234_MEMORY_CLIENT_MIU6W 0xff

#endif /* DT_BINDINGS_MEMORY_TEGRA234_MC_H */
