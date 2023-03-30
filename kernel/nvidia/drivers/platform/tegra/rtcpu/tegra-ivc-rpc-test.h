/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022, NVIDIA CORPORATION & AFFILIATES.  All rights reserved.
 */

#ifndef INCLUDE_TEGRA_IVC_RPC_TEST_H
#define INCLUDE_TEGRA_IVC_RPC_TEST_H

struct tegra_ivc_channel;
struct dentry;

void tegra_ivc_rpc_create_test_debugfs(
	struct tegra_ivc_channel *chan,
	struct dentry *debugfs_root);

#endif /* INCLUDE_TEGRA_IVC_RPC_TEST_H */
