/*
 * IOMMU API for ARM architected SMMU implementations.
 *
 * Copyright (c) 2020 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


/* Configuration registers */
#define ARM_SMMU_GR0_sCR2		0x8

/* Context bank attribute registers */
#define ARM_SMMU_CB_TTBCR2		0x10
#define ARM_SMMU_CB_TTBR0_LO		0x20
#define ARM_SMMU_CB_TTBR0_HI		0x24
#define ARM_SMMU_CB_TTBR1_LO		0x28
#define ARM_SMMU_CB_TTBR1_HI		0x2c
#define ARM_SMMU_CB_TTBCR		0x30

