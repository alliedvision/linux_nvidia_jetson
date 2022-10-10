/*
 * Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDE_TEGRA_IVC_RPC_TEST_H
#define INCLUDE_TEGRA_IVC_RPC_TEST_H

struct tegra_ivc_channel;
struct dentry;

void tegra_ivc_rpc_create_test_debugfs(
	struct tegra_ivc_channel *chan,
	struct dentry *debugfs_root);

#endif /* INCLUDE_TEGRA_IVC_RPC_TEST_H */
