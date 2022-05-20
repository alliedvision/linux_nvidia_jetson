/*
 * tegra210_xbar_utils_alt.c - Tegra XBAR driver utils
 *
 * Copyright (c) 2017-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/version.h>
#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif
#include <sound/soc.h>
#include <linux/clk/tegra.h>
#include "tegra210_xbar_alt.h"

void tegra210_xbar_set_cif(struct regmap *regmap, unsigned int reg,
			  struct tegra210_xbar_cif_conf *conf)
{
	unsigned int value;

	value = (conf->threshold <<
			TEGRA210_AUDIOCIF_CTRL_FIFO_THRESHOLD_SHIFT) |
		((conf->audio_channels - 1) <<
			TEGRA210_AUDIOCIF_CTRL_AUDIO_CHANNELS_SHIFT) |
		((conf->client_channels - 1) <<
			TEGRA210_AUDIOCIF_CTRL_CLIENT_CHANNELS_SHIFT) |
		(conf->audio_bits <<
			TEGRA210_AUDIOCIF_CTRL_AUDIO_BITS_SHIFT) |
		(conf->client_bits <<
			TEGRA210_AUDIOCIF_CTRL_CLIENT_BITS_SHIFT) |
		(conf->expand <<
			TEGRA210_AUDIOCIF_CTRL_EXPAND_SHIFT) |
		(conf->stereo_conv <<
			TEGRA210_AUDIOCIF_CTRL_STEREO_CONV_SHIFT) |
		(conf->replicate <<
			TEGRA210_AUDIOCIF_CTRL_REPLICATE_SHIFT) |
		(conf->truncate <<
			TEGRA210_AUDIOCIF_CTRL_TRUNCATE_SHIFT) |
		(conf->mono_conv <<
			TEGRA210_AUDIOCIF_CTRL_MONO_CONV_SHIFT);

	regmap_update_bits(regmap, reg, 0x3fffffff, value);
}
EXPORT_SYMBOL_GPL(tegra210_xbar_set_cif);

void tegra210_xbar_write_ahubram(struct regmap *regmap, unsigned int reg_ctrl,
				unsigned int reg_data, unsigned int ram_offset,
				unsigned int *data, size_t size)
{
	unsigned int val = 0;
	int i = 0;

	val = ram_offset & TEGRA210_AHUBRAMCTL_CTRL_RAM_ADDR_MASK;
	val |= TEGRA210_AHUBRAMCTL_CTRL_ADDR_INIT_EN;
	val |= TEGRA210_AHUBRAMCTL_CTRL_SEQ_ACCESS_EN;
	val |= TEGRA210_AHUBRAMCTL_CTRL_RW_WRITE;

	regmap_write(regmap, reg_ctrl, val);
	for (i = 0; i < size; i++)
		regmap_write(regmap, reg_data, data[i]);

	return;
}
EXPORT_SYMBOL_GPL(tegra210_xbar_write_ahubram);

void tegra210_xbar_read_ahubram(struct regmap *regmap, unsigned int reg_ctrl,
				unsigned int reg_data, unsigned int ram_offset,
				unsigned int *data, size_t size)
{
	unsigned int val = 0;
	int i = 0;

	val = ram_offset & TEGRA210_AHUBRAMCTL_CTRL_RAM_ADDR_MASK;
	val |= TEGRA210_AHUBRAMCTL_CTRL_ADDR_INIT_EN;
	val |= TEGRA210_AHUBRAMCTL_CTRL_SEQ_ACCESS_EN;
	val |= TEGRA210_AHUBRAMCTL_CTRL_RW_READ;

	regmap_write(regmap, reg_ctrl, val);
	/* Since all ahub non-io modules work under same ahub clock it is not
	   necessary to check ahub read busy bit after every read */
	for (i = 0; i < size; i++)
		regmap_read(regmap, reg_data, &data[i]);

	return;
}
EXPORT_SYMBOL_GPL(tegra210_xbar_read_ahubram);

MODULE_LICENSE("GPL");
