/*
 * tegra210_admaif_alt.c - Tegra ADMAIF driver
 *
 * Copyright (c) 2014-2020 NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>

#include "tegra_pcm_alt.h"
#include "tegra210_xbar_alt.h"
#include "tegra_isomgr_bw_alt.h"
#include "tegra210_admaif_alt.h"

#define DRV_NAME "tegra210-ape-admaif"

#define CH_REG(offset, reg, id)						       \
	((offset) + (reg) + (TEGRA_ADMAIF_CHANNEL_REG_STRIDE * (id)))

#define CH_TX_REG(reg, id) CH_REG(admaif->soc_data->tx_base, reg, id)

#define CH_RX_REG(reg, id) CH_REG(admaif->soc_data->rx_base, reg, id)

#define REG_IOVA(reg) (admaif->base_addr + (reg))

#define REG_DEFAULTS(id, rx_ctrl, tx_ctrl, tx_base, rx_base)		       \
	{ CH_REG(rx_base, TEGRA_ADMAIF_XBAR_RX_INT_MASK, id), 0x00000001},     \
	{ CH_REG(rx_base, TEGRA_ADMAIF_CHAN_ACIF_RX_CTRL, id), 0x00007700},    \
	{ CH_REG(rx_base, TEGRA_ADMAIF_XBAR_RX_FIFO_CTRL, id), rx_ctrl},       \
	{ CH_REG(tx_base, TEGRA_ADMAIF_XBAR_TX_INT_MASK, id), 0x00000001},     \
	{ CH_REG(tx_base, TEGRA_ADMAIF_CHAN_ACIF_TX_CTRL, id), 0x00007700},    \
	{ CH_REG(tx_base, TEGRA_ADMAIF_XBAR_TX_FIFO_CTRL, id), tx_ctrl}

#define ADMAIF_REG_DEFAULTS(id, chip)					       \
	REG_DEFAULTS((id - 1),						       \
		chip ## _ADMAIF_RX ## id ## _FIFO_CTRL_REG_DEFAULT,	       \
		chip ## _ADMAIF_TX ## id ## _FIFO_CTRL_REG_DEFAULT,	       \
		chip ## _ADMAIF_XBAR_TX_BASE,				       \
		chip ## _ADMAIF_XBAR_RX_BASE)

static const struct reg_default tegra186_admaif_reg_defaults[] = {
	{(TEGRA_ADMAIF_GLOBAL_CG_0+TEGRA186_ADMAIF_GLOBAL_BASE), 0x00000003},
	ADMAIF_REG_DEFAULTS(1, TEGRA186),
	ADMAIF_REG_DEFAULTS(2, TEGRA186),
	ADMAIF_REG_DEFAULTS(3, TEGRA186),
	ADMAIF_REG_DEFAULTS(4, TEGRA186),
	ADMAIF_REG_DEFAULTS(5, TEGRA186),
	ADMAIF_REG_DEFAULTS(6, TEGRA186),
	ADMAIF_REG_DEFAULTS(7, TEGRA186),
	ADMAIF_REG_DEFAULTS(8, TEGRA186),
	ADMAIF_REG_DEFAULTS(9, TEGRA186),
	ADMAIF_REG_DEFAULTS(10, TEGRA186),
	ADMAIF_REG_DEFAULTS(11, TEGRA186),
	ADMAIF_REG_DEFAULTS(12, TEGRA186),
	ADMAIF_REG_DEFAULTS(13, TEGRA186),
	ADMAIF_REG_DEFAULTS(14, TEGRA186),
	ADMAIF_REG_DEFAULTS(15, TEGRA186),
	ADMAIF_REG_DEFAULTS(16, TEGRA186),
	ADMAIF_REG_DEFAULTS(17, TEGRA186),
	ADMAIF_REG_DEFAULTS(18, TEGRA186),
	ADMAIF_REG_DEFAULTS(19, TEGRA186),
	ADMAIF_REG_DEFAULTS(20, TEGRA186)
};

static const struct reg_default tegra210_admaif_reg_defaults[] = {
	{(TEGRA_ADMAIF_GLOBAL_CG_0+TEGRA210_ADMAIF_GLOBAL_BASE), 0x00000003},
	ADMAIF_REG_DEFAULTS(1, TEGRA210),
	ADMAIF_REG_DEFAULTS(2, TEGRA210),
	ADMAIF_REG_DEFAULTS(3, TEGRA210),
	ADMAIF_REG_DEFAULTS(4, TEGRA210),
	ADMAIF_REG_DEFAULTS(5, TEGRA210),
	ADMAIF_REG_DEFAULTS(6, TEGRA210),
	ADMAIF_REG_DEFAULTS(7, TEGRA210),
	ADMAIF_REG_DEFAULTS(8, TEGRA210),
	ADMAIF_REG_DEFAULTS(9, TEGRA210),
	ADMAIF_REG_DEFAULTS(10, TEGRA210)
};

static bool tegra_admaif_wr_reg(struct device *dev, unsigned int reg)
{
	struct tegra_admaif *admaif = dev_get_drvdata(dev);
	unsigned int ch_stride = TEGRA_ADMAIF_CHANNEL_REG_STRIDE;
	unsigned int num_ch = admaif->soc_data->num_ch;
	unsigned int rx_base = admaif->soc_data->rx_base;
	unsigned int tx_base = admaif->soc_data->tx_base;
	unsigned int global_base = admaif->soc_data->global_base;
	unsigned int reg_max = admaif->soc_data->regmap_conf->max_register;
	unsigned int rx_max = rx_base + (num_ch * ch_stride);
	unsigned int tx_max = tx_base + (num_ch * ch_stride);

	if ((reg >= rx_base) && (reg < rx_max)) {
		reg = (reg - rx_base) % ch_stride;
		if ((reg == TEGRA_ADMAIF_XBAR_RX_ENABLE) ||
		    (reg == TEGRA_ADMAIF_XBAR_RX_FIFO_CTRL) ||
		    (reg == TEGRA_ADMAIF_XBAR_RX_SOFT_RESET) ||
		    (reg == TEGRA_ADMAIF_CHAN_ACIF_RX_CTRL))
			return true;
	} else if ((reg >= tx_base) && (reg < tx_max)) {
		reg = (reg - tx_base) % ch_stride;
		if ((reg == TEGRA_ADMAIF_XBAR_TX_ENABLE) ||
		    (reg == TEGRA_ADMAIF_XBAR_TX_FIFO_CTRL) ||
		    (reg == TEGRA_ADMAIF_XBAR_TX_SOFT_RESET) ||
		    (reg == TEGRA_ADMAIF_CHAN_ACIF_TX_CTRL))
			return true;
	} else if ((reg >= global_base) && (reg < reg_max)) {
		if (reg == (global_base + TEGRA_ADMAIF_GLOBAL_ENABLE))
			return true;
	}
	return false;
}

static bool tegra_admaif_rd_reg(struct device *dev, unsigned int reg)
{
	struct tegra_admaif *admaif = dev_get_drvdata(dev);
	unsigned int ch_stride = TEGRA_ADMAIF_CHANNEL_REG_STRIDE;
	unsigned int num_ch = admaif->soc_data->num_ch;
	unsigned int rx_base = admaif->soc_data->rx_base;
	unsigned int tx_base = admaif->soc_data->tx_base;
	unsigned int global_base = admaif->soc_data->global_base;
	unsigned int reg_max = admaif->soc_data->regmap_conf->max_register;
	unsigned int rx_max = rx_base + (num_ch * ch_stride);
	unsigned int tx_max = tx_base + (num_ch * ch_stride);

	if ((reg >= rx_base) && (reg < rx_max)) {
		reg = (reg - rx_base) % ch_stride;
		if ((reg == TEGRA_ADMAIF_XBAR_RX_ENABLE) ||
		    (reg == TEGRA_ADMAIF_XBAR_RX_STATUS) ||
		    (reg == TEGRA_ADMAIF_XBAR_RX_INT_STATUS) ||
		    (reg == TEGRA_ADMAIF_XBAR_RX_FIFO_CTRL) ||
		    (reg == TEGRA_ADMAIF_XBAR_RX_SOFT_RESET) ||
		    (reg == TEGRA_ADMAIF_CHAN_ACIF_RX_CTRL))
			return true;
	} else if ((reg >= tx_base) && (reg < tx_max)) {
		reg = (reg - tx_base) % ch_stride;
		if ((reg == TEGRA_ADMAIF_XBAR_TX_ENABLE) ||
		    (reg == TEGRA_ADMAIF_XBAR_TX_STATUS) ||
		    (reg == TEGRA_ADMAIF_XBAR_TX_INT_STATUS) ||
		    (reg == TEGRA_ADMAIF_XBAR_TX_FIFO_CTRL) ||
		    (reg == TEGRA_ADMAIF_XBAR_TX_SOFT_RESET) ||
		    (reg == TEGRA_ADMAIF_CHAN_ACIF_TX_CTRL))
			return true;
	} else if ((reg >= global_base) && (reg < reg_max)) {
		if ((reg == (global_base + TEGRA_ADMAIF_GLOBAL_ENABLE)) ||
		    (reg == (global_base + TEGRA_ADMAIF_GLOBAL_CG_0)) ||
		    (reg == (global_base + TEGRA_ADMAIF_GLOBAL_STATUS)) ||
		    (reg == (global_base +
				TEGRA_ADMAIF_GLOBAL_RX_ENABLE_STATUS)) ||
		    (reg == (global_base +
				TEGRA_ADMAIF_GLOBAL_TX_ENABLE_STATUS)))
			return true;
	}
	return false;
}

static bool tegra_admaif_volatile_reg(struct device *dev, unsigned int reg)
{
	struct tegra_admaif *admaif = dev_get_drvdata(dev);
	unsigned int ch_stride = TEGRA_ADMAIF_CHANNEL_REG_STRIDE;
	unsigned int num_ch = admaif->soc_data->num_ch;
	unsigned int rx_base = admaif->soc_data->rx_base;
	unsigned int tx_base = admaif->soc_data->tx_base;
	unsigned int global_base = admaif->soc_data->global_base;
	unsigned int reg_max = admaif->soc_data->regmap_conf->max_register;
	unsigned int rx_max = rx_base + (num_ch * ch_stride);
	unsigned int tx_max = tx_base + (num_ch * ch_stride);

	if ((reg >= rx_base) && (reg < rx_max)) {
		reg = (reg - rx_base) % ch_stride;
		if ((reg == TEGRA_ADMAIF_XBAR_RX_ENABLE) ||
		    (reg == TEGRA_ADMAIF_XBAR_RX_STATUS) ||
		    (reg == TEGRA_ADMAIF_XBAR_RX_INT_STATUS) ||
		    (reg == TEGRA_ADMAIF_XBAR_RX_SOFT_RESET))
			return true;
	} else if ((reg >= tx_base) && (reg < tx_max)) {
		reg = (reg - tx_base) % ch_stride;
		if ((reg == TEGRA_ADMAIF_XBAR_TX_ENABLE) ||
		    (reg == TEGRA_ADMAIF_XBAR_TX_STATUS) ||
		    (reg == TEGRA_ADMAIF_XBAR_TX_INT_STATUS) ||
		    (reg == TEGRA_ADMAIF_XBAR_TX_SOFT_RESET))
			return true;
	} else if ((reg >= global_base) && (reg < reg_max)) {
		if ((reg == (global_base + TEGRA_ADMAIF_GLOBAL_STATUS)) ||
		    (reg == (global_base +
				TEGRA_ADMAIF_GLOBAL_RX_ENABLE_STATUS)) ||
		    (reg == (global_base +
				TEGRA_ADMAIF_GLOBAL_TX_ENABLE_STATUS)))
			return true;
	}
	return false;
}

static const struct regmap_config tegra210_admaif_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = TEGRA210_ADMAIF_LAST_REG,
	.writeable_reg = tegra_admaif_wr_reg,
	.readable_reg = tegra_admaif_rd_reg,
	.volatile_reg = tegra_admaif_volatile_reg,
	.reg_defaults = tegra210_admaif_reg_defaults,
	.num_reg_defaults = TEGRA210_ADMAIF_CHANNEL_COUNT * 6 + 1,
	.cache_type = REGCACHE_FLAT,
};

static const struct regmap_config tegra186_admaif_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = TEGRA186_ADMAIF_LAST_REG,
	.writeable_reg = tegra_admaif_wr_reg,
	.readable_reg = tegra_admaif_rd_reg,
	.volatile_reg = tegra_admaif_volatile_reg,
	.reg_defaults = tegra186_admaif_reg_defaults,
	.num_reg_defaults = TEGRA186_ADMAIF_CHANNEL_COUNT * 6 + 1,
	.cache_type = REGCACHE_FLAT,
};

static int tegra_admaif_runtime_suspend(struct device *dev)
{
	struct tegra_admaif *admaif = dev_get_drvdata(dev);

	regcache_cache_only(admaif->regmap, true);
	regcache_mark_dirty(admaif->regmap);

	return 0;
}

static int tegra_admaif_runtime_resume(struct device *dev)
{
	struct tegra_admaif *admaif = dev_get_drvdata(dev);

	regcache_cache_only(admaif->regmap, false);
	regcache_sync(admaif->regmap);

	return 0;
}

static int tegra_admaif_set_pack_mode(struct regmap *map, unsigned int reg,
					int valid_bit)
{
	switch (valid_bit) {
	case DATA_8BIT:
		regmap_update_bits(map, reg,
			TEGRA_ADMAIF_CHAN_ACIF_CTRL_PACK8_EN_MASK,
			TEGRA_ADMAIF_CHAN_ACIF_CTRL_PACK8_EN);
		regmap_update_bits(map, reg,
			TEGRA_ADMAIF_CHAN_ACIF_CTRL_PACK16_EN_MASK,
			0);
		break;
	case DATA_16BIT:
		regmap_update_bits(map, reg,
			TEGRA_ADMAIF_CHAN_ACIF_CTRL_PACK16_EN_MASK,
			TEGRA_ADMAIF_CHAN_ACIF_CTRL_PACK16_EN);
		regmap_update_bits(map, reg,
			TEGRA_ADMAIF_CHAN_ACIF_CTRL_PACK8_EN_MASK,
			0);
		break;
	case DATA_32BIT:
		regmap_update_bits(map, reg,
			TEGRA_ADMAIF_CHAN_ACIF_CTRL_PACK16_EN_MASK,
			0);
		regmap_update_bits(map, reg,
			TEGRA_ADMAIF_CHAN_ACIF_CTRL_PACK8_EN_MASK,
			0);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int tegra_admaif_prepare(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct tegra_admaif *admaif = snd_soc_dai_get_drvdata(dai);

	if (admaif->soc_data->is_isomgr_client)
		tegra_isomgr_adma_setbw(substream, true);

	return 0;
}

static void tegra_admaif_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct tegra_admaif *admaif = snd_soc_dai_get_drvdata(dai);

	if (admaif->soc_data->is_isomgr_client)
		tegra_isomgr_adma_setbw(substream, false);
}

static int tegra_admaif_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct tegra_admaif *admaif = snd_soc_dai_get_drvdata(dai);
	struct tegra210_xbar_cif_conf cif_conf;
	unsigned int reg, path;
	int valid_bit, channels;

	memset(&cif_conf, 0, sizeof(struct tegra210_xbar_cif_conf));

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		cif_conf.audio_bits = TEGRA210_AUDIOCIF_BITS_8;
		cif_conf.client_bits = TEGRA210_AUDIOCIF_BITS_8;
		valid_bit = DATA_8BIT;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		cif_conf.audio_bits = TEGRA210_AUDIOCIF_BITS_16;
		cif_conf.client_bits = TEGRA210_AUDIOCIF_BITS_16;
		valid_bit = DATA_16BIT;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		cif_conf.audio_bits = TEGRA210_AUDIOCIF_BITS_32;
		cif_conf.client_bits = TEGRA210_AUDIOCIF_BITS_32;
		valid_bit  = DATA_32BIT;
		break;
	default:
		dev_err(dev, "Wrong format!\n");
		return -EINVAL;
	}

	channels = params_channels(params);
	cif_conf.client_channels = channels;
	cif_conf.audio_channels = channels;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		path = ADMAIF_TX_PATH;
		reg = CH_TX_REG(TEGRA_ADMAIF_CHAN_ACIF_TX_CTRL, dai->id);
	} else {
		path = ADMAIF_RX_PATH;
		reg = CH_RX_REG(TEGRA_ADMAIF_CHAN_ACIF_RX_CTRL, dai->id);
	}

	if (admaif->audio_ch_override[path][dai->id])
		cif_conf.audio_channels =
			admaif->audio_ch_override[path][dai->id];

	if (admaif->client_ch_override[path][dai->id])
		cif_conf.client_channels =
			admaif->client_ch_override[path][dai->id];

	cif_conf.mono_conv = admaif->mono_to_stereo[path][dai->id];
	cif_conf.stereo_conv = admaif->stereo_to_mono[path][dai->id];

	tegra_admaif_set_pack_mode(admaif->regmap, reg, valid_bit);

	tegra210_xbar_set_cif(admaif->regmap, reg, &cif_conf);

	return 0;
}

static int tegra_admaif_start(struct snd_soc_dai *dai, int direction)
{
	struct tegra_admaif *admaif = snd_soc_dai_get_drvdata(dai);
	unsigned int reg, mask, val;

	switch (direction) {
	case SNDRV_PCM_STREAM_PLAYBACK:
		mask = TEGRA_ADMAIF_XBAR_TX_ENABLE_MASK;
		val = TEGRA_ADMAIF_XBAR_TX_EN;
		reg = CH_TX_REG(TEGRA_ADMAIF_XBAR_TX_ENABLE, dai->id);
		break;
	case SNDRV_PCM_STREAM_CAPTURE:
		mask = TEGRA_ADMAIF_XBAR_RX_ENABLE_MASK;
		val = TEGRA_ADMAIF_XBAR_RX_EN;
		reg = CH_RX_REG(TEGRA_ADMAIF_XBAR_RX_ENABLE, dai->id);
		break;
	default:
		return -EINVAL;
	}

	regmap_update_bits(admaif->regmap, reg, mask, val);

	return 0;
}

static int tegra_admaif_stop(struct snd_soc_dai *dai, int direction)
{
	struct tegra_admaif *admaif = snd_soc_dai_get_drvdata(dai);
	unsigned int enable_reg, status_reg, reset_reg, mask, val, enable;
	char *dir_name;
	int ret;

	switch (direction) {
	case SNDRV_PCM_STREAM_PLAYBACK:
		mask = TEGRA_ADMAIF_XBAR_TX_ENABLE_MASK;
		enable = TEGRA_ADMAIF_XBAR_TX_EN;
		dir_name = "TX";
		enable_reg = CH_TX_REG(TEGRA_ADMAIF_XBAR_TX_ENABLE, dai->id);
		status_reg = CH_TX_REG(TEGRA_ADMAIF_XBAR_TX_STATUS, dai->id);
		reset_reg = CH_TX_REG(TEGRA_ADMAIF_XBAR_TX_SOFT_RESET, dai->id);
		break;
	case SNDRV_PCM_STREAM_CAPTURE:
		mask = TEGRA_ADMAIF_XBAR_RX_ENABLE_MASK;
		enable = TEGRA_ADMAIF_XBAR_RX_EN;
		dir_name = "RX";
		enable_reg = CH_RX_REG(TEGRA_ADMAIF_XBAR_RX_ENABLE, dai->id);
		status_reg = CH_RX_REG(TEGRA_ADMAIF_XBAR_RX_STATUS, dai->id);
		reset_reg = CH_RX_REG(TEGRA_ADMAIF_XBAR_RX_SOFT_RESET, dai->id);
		break;
	default:
		return -EINVAL;
	}

	/* disable TX/RX channel */
	regmap_update_bits(admaif->regmap, enable_reg, mask, ~enable);

	/* wait until ADMAIF TX/RX status is disabled */
	ret = regmap_read_poll_timeout_atomic(admaif->regmap, status_reg, val,
					      !(val & enable), 10, 10000);

	/* Timeout may be hit if sink gets closed/blocked ahead of source */
	if (ret < 0)
		dev_warn(dai->dev, "timeout: failed to disable ADMAIF%d_%s\n",
			 dai->id + 1, dir_name);

	/* SW reset */
	regmap_update_bits(admaif->regmap, reset_reg, SW_RESET_MASK, SW_RESET);

	/* wait till SW reset is complete */
	ret = regmap_read_poll_timeout_atomic(admaif->regmap, reset_reg, val,
					      !(val & SW_RESET_MASK & SW_RESET),
					      10, 10000);
	if (ret < 0) {
		dev_err(dai->dev, "timeout: SW reset failed for ADMAIF%d_%s\n",
			dai->id + 1, dir_name);
		return ret;
	}

	return 0;
}

static int tegra_admaif_trigger(struct snd_pcm_substream *substream, int cmd,
				 struct snd_soc_dai *dai)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		return tegra_admaif_start(dai, substream->stream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return tegra_admaif_stop(dai, substream->stream);
	default:
		return -EINVAL;
	}
}

static struct snd_soc_dai_ops tegra_admaif_dai_ops = {
	.hw_params	= tegra_admaif_hw_params,
	.trigger	= tegra_admaif_trigger,
	.shutdown	= tegra_admaif_shutdown,
	.prepare	= tegra_admaif_prepare,
};

static int tegra_admaif_get_format(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct soc_enum *ec = (struct soc_enum *)kcontrol->private_value;
	struct tegra_admaif *admaif = snd_soc_codec_get_drvdata(codec);
	long *uctl_val = &ucontrol->value.integer.value[0];

	if (strstr(kcontrol->id.name, "Playback Audio Channels"))
		*uctl_val = admaif->audio_ch_override[ADMAIF_TX_PATH][mc->reg];
	else if (strstr(kcontrol->id.name, "Capture Audio Channels"))
		*uctl_val = admaif->audio_ch_override[ADMAIF_RX_PATH][mc->reg];
	else if (strstr(kcontrol->id.name, "Playback Client Channels"))
		*uctl_val = admaif->client_ch_override[ADMAIF_TX_PATH][mc->reg];
	else if (strstr(kcontrol->id.name, "Capture Client Channels"))
		*uctl_val = admaif->client_ch_override[ADMAIF_RX_PATH][mc->reg];
	else if (strstr(kcontrol->id.name, "Playback Mono To Stereo"))
		*uctl_val = admaif->mono_to_stereo[ADMAIF_TX_PATH][ec->reg];
	else if (strstr(kcontrol->id.name, "Playback Stereo To Mono"))
		*uctl_val = admaif->stereo_to_mono[ADMAIF_TX_PATH][ec->reg];
	else if (strstr(kcontrol->id.name, "Capture Mono To Stereo"))
		*uctl_val = admaif->mono_to_stereo[ADMAIF_RX_PATH][ec->reg];
	else if (strstr(kcontrol->id.name, "Capture Stereo To Mono"))
		*uctl_val = admaif->stereo_to_mono[ADMAIF_RX_PATH][ec->reg];

	return 0;
}

static int tegra_admaif_put_format(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	struct soc_enum *ec = (struct soc_enum *)kcontrol->private_value;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra_admaif *admaif = snd_soc_codec_get_drvdata(codec);
	int value = ucontrol->value.integer.value[0];

	if (strstr(kcontrol->id.name, "Playback Audio Channels"))
		admaif->audio_ch_override[ADMAIF_TX_PATH][mc->reg] = value;
	else if (strstr(kcontrol->id.name, "Capture Audio Channels"))
		admaif->audio_ch_override[ADMAIF_RX_PATH][mc->reg] = value;
	else if (strstr(kcontrol->id.name, "Playback Client Channels"))
		admaif->client_ch_override[ADMAIF_TX_PATH][mc->reg] = value;
	else if (strstr(kcontrol->id.name, "Capture Client Channels"))
		admaif->client_ch_override[ADMAIF_RX_PATH][mc->reg] = value;
	else if (strstr(kcontrol->id.name, "Playback Mono To Stereo"))
		admaif->mono_to_stereo[ADMAIF_TX_PATH][ec->reg] = value;
	else if (strstr(kcontrol->id.name, "Playback Stereo To Mono"))
		admaif->stereo_to_mono[ADMAIF_TX_PATH][ec->reg] = value;
	else if (strstr(kcontrol->id.name, "Capture Mono To Stereo"))
		admaif->mono_to_stereo[ADMAIF_RX_PATH][ec->reg] = value;
	else if (strstr(kcontrol->id.name, "Capture Stereo To Mono"))
		admaif->stereo_to_mono[ADMAIF_RX_PATH][ec->reg] = value;

	return 0;
}

static void tegra_admaif_reg_dump(struct tegra_admaif *admaif)
{
	int i, stride;
	int ret;
	int tx_offset = admaif->soc_data->tx_base;

	ret = pm_runtime_get_sync(admaif->dev->parent);
	if (ret < 0) {
		dev_err(admaif->dev, "parent get_sync failed: %d\n", ret);
		return;
	}

	pr_info("=========ADMAIF reg dump=========\n");

	for (i = 0; i < admaif->soc_data->num_ch; i++) {
		stride = (i * TEGRA_ADMAIF_CHANNEL_REG_STRIDE);
		pr_info("RX%d_Enable	= %#x\n", i+1,
			readl(admaif->base_addr +
				TEGRA_ADMAIF_XBAR_RX_ENABLE + stride));
		pr_info("RX%d_STATUS	= %#x\n", i+1,
			readl(admaif->base_addr +
				TEGRA_ADMAIF_XBAR_RX_STATUS + stride));
		pr_info("RX%d_CIF_CTRL	= %#x\n", i+1,
			readl(admaif->base_addr +
				TEGRA_ADMAIF_CHAN_ACIF_RX_CTRL + stride));
		pr_info("RX%d_FIFO_CTRL	= %#x\n", i+1,
			readl(admaif->base_addr +
				TEGRA_ADMAIF_XBAR_RX_FIFO_CTRL + stride));
		pr_info("TX%d_Enable	= %#x\n", i+1,
			readl(admaif->base_addr + tx_offset +
				TEGRA_ADMAIF_XBAR_TX_ENABLE + stride));
		pr_info("TX%d_STATUS	= %#x\n", i+1,
			readl(admaif->base_addr + tx_offset +
				TEGRA_ADMAIF_XBAR_TX_STATUS + stride));
		pr_info("TX%d_CIF_CTRL	= %#x\n", i+1,
			readl(admaif->base_addr + tx_offset +
				TEGRA_ADMAIF_CHAN_ACIF_TX_CTRL + stride));
		pr_info("TX%d_FIFO_CTRL	= %#x\n", i+1,
			readl(admaif->base_addr + tx_offset +
				TEGRA_ADMAIF_XBAR_TX_FIFO_CTRL + stride));
	}
	pm_runtime_put_sync(admaif->dev->parent);
}
static int tegra210_ape_dump_reg_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra_admaif *admaif = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = admaif->reg_dump_flag;

	return 0;
}

static int tegra210_ape_dump_reg_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tegra_admaif *admaif = snd_soc_codec_get_drvdata(codec);

	admaif->reg_dump_flag = ucontrol->value.integer.value[0];

	if (admaif->reg_dump_flag) {
#if IS_ENABLED(CONFIG_TEGRA210_ADMA)
		tegra_adma_dump_ch_reg();
#endif
		tegra_admaif_reg_dump(admaif);
	}

	return 0;
}

static int tegra_admaif_dai_probe(struct snd_soc_dai *dai)
{
	struct tegra_admaif *admaif = snd_soc_dai_get_drvdata(dai);

	dai->capture_dma_data = &admaif->capture_dma_data[dai->id];
	dai->playback_dma_data = &admaif->playback_dma_data[dai->id];

	return 0;
}

#define ADMAIF_DAI(id)							\
	{							\
		.name = "ADMAIF" #id,				\
		.probe = tegra_admaif_dai_probe,		\
		.playback = {					\
			.stream_name = "Playback " #id,		\
			.channels_min = 1,			\
			.channels_max = 16,			\
			.rates = SNDRV_PCM_RATE_8000_192000,	\
			.formats = SNDRV_PCM_FMTBIT_S8 |	\
				SNDRV_PCM_FMTBIT_S16_LE |	\
				SNDRV_PCM_FMTBIT_S32_LE,	\
		},						\
		.capture = {					\
			.stream_name = "Capture " #id,		\
			.channels_min = 1,			\
			.channels_max = 16,			\
			.rates = SNDRV_PCM_RATE_8000_192000,		\
			.formats = SNDRV_PCM_FMTBIT_S8 |		\
				SNDRV_PCM_FMTBIT_S16_LE |		\
				SNDRV_PCM_FMTBIT_S32_LE,		\
		},						\
		.ops = &tegra_admaif_dai_ops,			\
	}

#define ADMAIF_CODEC_FIFO_DAI(id)					\
	{								\
		.name = "ADMAIF" #id " FIFO",				\
		.playback = {						\
			.stream_name = "ADMAIF" #id " FIFO Transmit",	\
			.channels_min = 1,				\
			.channels_max = 16,				\
			.rates = SNDRV_PCM_RATE_8000_192000,		\
			.formats = SNDRV_PCM_FMTBIT_S8 |		\
				SNDRV_PCM_FMTBIT_S16_LE |		\
				SNDRV_PCM_FMTBIT_S32_LE,		\
		},							\
		.capture = {						\
			.stream_name = "ADMAIF" #id " FIFO Receive",	\
			.channels_min = 1,				\
			.channels_max = 16,				\
			.rates = SNDRV_PCM_RATE_8000_192000,		\
			.formats = SNDRV_PCM_FMTBIT_S8 |		\
				SNDRV_PCM_FMTBIT_S16_LE |		\
				SNDRV_PCM_FMTBIT_S32_LE,		\
		},							\
		.ops = &tegra_admaif_dai_ops,			\
	}

#define ADMAIF_CODEC_CIF_DAI(id)					\
	{								\
		.name = "ADMAIF" #id " CIF",				\
		.playback = {						\
			.stream_name = "ADMAIF" #id " CIF Transmit",	\
			.channels_min = 1,				\
			.channels_max = 16,				\
			.rates = SNDRV_PCM_RATE_8000_192000,		\
			.formats = SNDRV_PCM_FMTBIT_S8 |		\
				SNDRV_PCM_FMTBIT_S16_LE |		\
				SNDRV_PCM_FMTBIT_S32_LE,		\
		},							\
		.capture = {						\
			.stream_name = "ADMAIF" #id " CIF Receive",	\
			.channels_min = 1,				\
			.channels_max = 16,				\
			.rates = SNDRV_PCM_RATE_8000_192000,		\
			.formats = SNDRV_PCM_FMTBIT_S8 |		\
				SNDRV_PCM_FMTBIT_S16_LE |		\
				SNDRV_PCM_FMTBIT_S32_LE,		\
		},							\
	}

static struct snd_soc_dai_driver tegra210_admaif_codec_dais[] = {
	ADMAIF_DAI(1),
	ADMAIF_DAI(2),
	ADMAIF_DAI(3),
	ADMAIF_DAI(4),
	ADMAIF_DAI(5),
	ADMAIF_DAI(6),
	ADMAIF_DAI(7),
	ADMAIF_DAI(8),
	ADMAIF_DAI(9),
	ADMAIF_DAI(10),
	ADMAIF_CODEC_FIFO_DAI(1),
	ADMAIF_CODEC_FIFO_DAI(2),
	ADMAIF_CODEC_FIFO_DAI(3),
	ADMAIF_CODEC_FIFO_DAI(4),
	ADMAIF_CODEC_FIFO_DAI(5),
	ADMAIF_CODEC_FIFO_DAI(6),
	ADMAIF_CODEC_FIFO_DAI(7),
	ADMAIF_CODEC_FIFO_DAI(8),
	ADMAIF_CODEC_FIFO_DAI(9),
	ADMAIF_CODEC_FIFO_DAI(10),
	ADMAIF_CODEC_CIF_DAI(1),
	ADMAIF_CODEC_CIF_DAI(2),
	ADMAIF_CODEC_CIF_DAI(3),
	ADMAIF_CODEC_CIF_DAI(4),
	ADMAIF_CODEC_CIF_DAI(5),
	ADMAIF_CODEC_CIF_DAI(6),
	ADMAIF_CODEC_CIF_DAI(7),
	ADMAIF_CODEC_CIF_DAI(8),
	ADMAIF_CODEC_CIF_DAI(9),
	ADMAIF_CODEC_CIF_DAI(10),
};

static struct snd_soc_dai_driver tegra186_admaif_codec_dais[] = {
	ADMAIF_DAI(1),
	ADMAIF_DAI(2),
	ADMAIF_DAI(3),
	ADMAIF_DAI(4),
	ADMAIF_DAI(5),
	ADMAIF_DAI(6),
	ADMAIF_DAI(7),
	ADMAIF_DAI(8),
	ADMAIF_DAI(9),
	ADMAIF_DAI(10),
	ADMAIF_DAI(11),
	ADMAIF_DAI(12),
	ADMAIF_DAI(13),
	ADMAIF_DAI(14),
	ADMAIF_DAI(15),
	ADMAIF_DAI(16),
	ADMAIF_DAI(17),
	ADMAIF_DAI(18),
	ADMAIF_DAI(19),
	ADMAIF_DAI(20),
	ADMAIF_CODEC_FIFO_DAI(1),
	ADMAIF_CODEC_FIFO_DAI(2),
	ADMAIF_CODEC_FIFO_DAI(3),
	ADMAIF_CODEC_FIFO_DAI(4),
	ADMAIF_CODEC_FIFO_DAI(5),
	ADMAIF_CODEC_FIFO_DAI(6),
	ADMAIF_CODEC_FIFO_DAI(7),
	ADMAIF_CODEC_FIFO_DAI(8),
	ADMAIF_CODEC_FIFO_DAI(9),
	ADMAIF_CODEC_FIFO_DAI(10),
	ADMAIF_CODEC_FIFO_DAI(11),
	ADMAIF_CODEC_FIFO_DAI(12),
	ADMAIF_CODEC_FIFO_DAI(13),
	ADMAIF_CODEC_FIFO_DAI(14),
	ADMAIF_CODEC_FIFO_DAI(15),
	ADMAIF_CODEC_FIFO_DAI(16),
	ADMAIF_CODEC_FIFO_DAI(17),
	ADMAIF_CODEC_FIFO_DAI(18),
	ADMAIF_CODEC_FIFO_DAI(19),
	ADMAIF_CODEC_FIFO_DAI(20),
	ADMAIF_CODEC_CIF_DAI(1),
	ADMAIF_CODEC_CIF_DAI(2),
	ADMAIF_CODEC_CIF_DAI(3),
	ADMAIF_CODEC_CIF_DAI(4),
	ADMAIF_CODEC_CIF_DAI(5),
	ADMAIF_CODEC_CIF_DAI(6),
	ADMAIF_CODEC_CIF_DAI(7),
	ADMAIF_CODEC_CIF_DAI(8),
	ADMAIF_CODEC_CIF_DAI(9),
	ADMAIF_CODEC_CIF_DAI(10),
	ADMAIF_CODEC_CIF_DAI(11),
	ADMAIF_CODEC_CIF_DAI(12),
	ADMAIF_CODEC_CIF_DAI(13),
	ADMAIF_CODEC_CIF_DAI(14),
	ADMAIF_CODEC_CIF_DAI(15),
	ADMAIF_CODEC_CIF_DAI(16),
	ADMAIF_CODEC_CIF_DAI(17),
	ADMAIF_CODEC_CIF_DAI(18),
	ADMAIF_CODEC_CIF_DAI(19),
	ADMAIF_CODEC_CIF_DAI(20),
};

#define ADMAIF_WIDGETS(id)					\
	SND_SOC_DAPM_AIF_IN("ADMAIF" #id " FIFO RX", NULL, 0,	\
		SND_SOC_NOPM, 0, 0),				\
	SND_SOC_DAPM_AIF_OUT("ADMAIF" #id " FIFO TX", NULL, 0,	\
		SND_SOC_NOPM, 0, 0),				\
	SND_SOC_DAPM_AIF_IN("ADMAIF" #id " CIF RX", NULL, 0,	\
		SND_SOC_NOPM, 0, 0),				\
	SND_SOC_DAPM_AIF_OUT("ADMAIF" #id " CIF TX", NULL, 0,	\
		SND_SOC_NOPM, 0, 0)

static const struct snd_soc_dapm_widget tegra_admaif_widgets[] = {
	ADMAIF_WIDGETS(1),
	ADMAIF_WIDGETS(2),
	ADMAIF_WIDGETS(3),
	ADMAIF_WIDGETS(4),
	ADMAIF_WIDGETS(5),
	ADMAIF_WIDGETS(6),
	ADMAIF_WIDGETS(7),
	ADMAIF_WIDGETS(8),
	ADMAIF_WIDGETS(9),
	ADMAIF_WIDGETS(10),
	ADMAIF_WIDGETS(11),
	ADMAIF_WIDGETS(12),
	ADMAIF_WIDGETS(13),
	ADMAIF_WIDGETS(14),
	ADMAIF_WIDGETS(15),
	ADMAIF_WIDGETS(16),
	ADMAIF_WIDGETS(17),
	ADMAIF_WIDGETS(18),
	ADMAIF_WIDGETS(19),
	ADMAIF_WIDGETS(20)
};

#define ADMAIF_ROUTES(id)						\
	{ "ADMAIF" #id " FIFO RX",      NULL, "ADMAIF" #id " FIFO Transmit" }, \
	{ "ADMAIF" #id " CIF TX",       NULL, "ADMAIF" #id " FIFO RX" },\
	{ "ADMAIF" #id " CIF Receive",  NULL, "ADMAIF" #id " CIF TX" }, \
	{ "ADMAIF" #id " CIF RX",       NULL, "ADMAIF" #id " CIF Transmit" },  \
	{ "ADMAIF" #id " FIFO TX",      NULL, "ADMAIF" #id " CIF RX" }, \
	{ "ADMAIF" #id " FIFO Receive", NULL, "ADMAIF" #id " FIFO TX" } \

static const struct snd_soc_dapm_route tegra_admaif_routes[] = {
	ADMAIF_ROUTES(1),
	ADMAIF_ROUTES(2),
	ADMAIF_ROUTES(3),
	ADMAIF_ROUTES(4),
	ADMAIF_ROUTES(5),
	ADMAIF_ROUTES(6),
	ADMAIF_ROUTES(7),
	ADMAIF_ROUTES(8),
	ADMAIF_ROUTES(9),
	ADMAIF_ROUTES(10),
	ADMAIF_ROUTES(11),
	ADMAIF_ROUTES(12),
	ADMAIF_ROUTES(13),
	ADMAIF_ROUTES(14),
	ADMAIF_ROUTES(15),
	ADMAIF_ROUTES(16),
	ADMAIF_ROUTES(17),
	ADMAIF_ROUTES(18),
	ADMAIF_ROUTES(19),
	ADMAIF_ROUTES(20)
};

static const char * const tegra_admaif_stereo_conv_text[] = {
	"CH0", "CH1", "AVG",
};

static const char * const tegra_admaif_mono_conv_text[] = {
	"Zero", "Copy",
};

static const struct soc_enum tegra_admaif_mono_conv_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
		ARRAY_SIZE(tegra_admaif_mono_conv_text),
		tegra_admaif_mono_conv_text);

static const struct soc_enum tegra_admaif_stereo_conv_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
		ARRAY_SIZE(tegra_admaif_stereo_conv_text),
		tegra_admaif_stereo_conv_text);

#define TEGRA_ADMAIF_CHANNEL_CTRL(reg)					       \
	SOC_SINGLE_EXT("ADMAIF" #reg " Playback Audio Channels", reg - 1,      \
		       0, 16, 0, tegra_admaif_get_format,		       \
		       tegra_admaif_put_format),			       \
	SOC_SINGLE_EXT("ADMAIF" #reg " Capture Audio Channels", reg - 1,       \
		       0, 16, 0, tegra_admaif_get_format,		       \
		       tegra_admaif_put_format),			       \
	SOC_SINGLE_EXT("ADMAIF" #reg " Playback Client Channels", reg - 1,     \
		       0, 16, 0, tegra_admaif_get_format,		       \
		       tegra_admaif_put_format),			       \
	SOC_SINGLE_EXT("ADMAIF" #reg " Capture Client Channels", reg - 1,      \
		       0, 16, 0, tegra_admaif_get_format,		       \
		       tegra_admaif_put_format)

/*
 * below macro is added to avoid looping over all ADMAIFx controls related
 * to mono/stereo conversions in get()/put() callbacks.
 */
#define NV_SOC_ENUM_EXT(xname, xreg, xhandler_get, xhandler_put, xenum_text)   \
{									       \
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,				       \
	.info = snd_soc_info_enum_double,				       \
	.name = xname,							       \
	.get = xhandler_get,						       \
	.put = xhandler_put,						       \
	.private_value = (unsigned long)&(struct soc_enum)		       \
		SOC_ENUM_SINGLE(xreg, 0, ARRAY_SIZE(xenum_text), xenum_text)   \
}

#define TEGRA_ADMAIF_CIF_CTRL(reg)					       \
	NV_SOC_ENUM_EXT("ADMAIF" #reg " Playback Mono To Stereo", reg - 1,\
			tegra_admaif_get_format, tegra_admaif_put_format,      \
			tegra_admaif_mono_conv_text),			       \
	NV_SOC_ENUM_EXT("ADMAIF" #reg " Playback Stereo To Mono", reg - 1,\
			tegra_admaif_get_format, tegra_admaif_put_format,      \
			tegra_admaif_stereo_conv_text),			       \
	NV_SOC_ENUM_EXT("ADMAIF" #reg " Capture Mono To Stereo", reg - 1, \
			tegra_admaif_get_format, tegra_admaif_put_format,      \
			tegra_admaif_mono_conv_text),			       \
	NV_SOC_ENUM_EXT("ADMAIF" #reg " Capture Stereo To Mono", reg - 1, \
			tegra_admaif_get_format, tegra_admaif_put_format,      \
			tegra_admaif_stereo_conv_text)

static struct snd_kcontrol_new tegra210_admaif_controls[] = {
	TEGRA_ADMAIF_CHANNEL_CTRL(1),
	TEGRA_ADMAIF_CHANNEL_CTRL(2),
	TEGRA_ADMAIF_CHANNEL_CTRL(3),
	TEGRA_ADMAIF_CHANNEL_CTRL(4),
	TEGRA_ADMAIF_CHANNEL_CTRL(5),
	TEGRA_ADMAIF_CHANNEL_CTRL(6),
	TEGRA_ADMAIF_CHANNEL_CTRL(7),
	TEGRA_ADMAIF_CHANNEL_CTRL(8),
	TEGRA_ADMAIF_CHANNEL_CTRL(9),
	TEGRA_ADMAIF_CHANNEL_CTRL(10),
	TEGRA_ADMAIF_CIF_CTRL(1),
	TEGRA_ADMAIF_CIF_CTRL(2),
	TEGRA_ADMAIF_CIF_CTRL(3),
	TEGRA_ADMAIF_CIF_CTRL(4),
	TEGRA_ADMAIF_CIF_CTRL(5),
	TEGRA_ADMAIF_CIF_CTRL(6),
	TEGRA_ADMAIF_CIF_CTRL(7),
	TEGRA_ADMAIF_CIF_CTRL(8),
	TEGRA_ADMAIF_CIF_CTRL(9),
	TEGRA_ADMAIF_CIF_CTRL(10),
	SOC_SINGLE_EXT("APE Reg Dump", SND_SOC_NOPM, 0, 1, 0,
		tegra210_ape_dump_reg_get, tegra210_ape_dump_reg_put),
};

static struct snd_kcontrol_new tegra186_admaif_controls[] = {
	TEGRA_ADMAIF_CHANNEL_CTRL(1),
	TEGRA_ADMAIF_CHANNEL_CTRL(2),
	TEGRA_ADMAIF_CHANNEL_CTRL(3),
	TEGRA_ADMAIF_CHANNEL_CTRL(4),
	TEGRA_ADMAIF_CHANNEL_CTRL(5),
	TEGRA_ADMAIF_CHANNEL_CTRL(6),
	TEGRA_ADMAIF_CHANNEL_CTRL(7),
	TEGRA_ADMAIF_CHANNEL_CTRL(8),
	TEGRA_ADMAIF_CHANNEL_CTRL(9),
	TEGRA_ADMAIF_CHANNEL_CTRL(10),
	TEGRA_ADMAIF_CHANNEL_CTRL(11),
	TEGRA_ADMAIF_CHANNEL_CTRL(12),
	TEGRA_ADMAIF_CHANNEL_CTRL(13),
	TEGRA_ADMAIF_CHANNEL_CTRL(14),
	TEGRA_ADMAIF_CHANNEL_CTRL(15),
	TEGRA_ADMAIF_CHANNEL_CTRL(16),
	TEGRA_ADMAIF_CHANNEL_CTRL(17),
	TEGRA_ADMAIF_CHANNEL_CTRL(18),
	TEGRA_ADMAIF_CHANNEL_CTRL(19),
	TEGRA_ADMAIF_CHANNEL_CTRL(20),
	TEGRA_ADMAIF_CIF_CTRL(1),
	TEGRA_ADMAIF_CIF_CTRL(2),
	TEGRA_ADMAIF_CIF_CTRL(3),
	TEGRA_ADMAIF_CIF_CTRL(4),
	TEGRA_ADMAIF_CIF_CTRL(5),
	TEGRA_ADMAIF_CIF_CTRL(6),
	TEGRA_ADMAIF_CIF_CTRL(7),
	TEGRA_ADMAIF_CIF_CTRL(8),
	TEGRA_ADMAIF_CIF_CTRL(9),
	TEGRA_ADMAIF_CIF_CTRL(10),
	TEGRA_ADMAIF_CIF_CTRL(11),
	TEGRA_ADMAIF_CIF_CTRL(12),
	TEGRA_ADMAIF_CIF_CTRL(13),
	TEGRA_ADMAIF_CIF_CTRL(14),
	TEGRA_ADMAIF_CIF_CTRL(15),
	TEGRA_ADMAIF_CIF_CTRL(16),
	TEGRA_ADMAIF_CIF_CTRL(17),
	TEGRA_ADMAIF_CIF_CTRL(18),
	TEGRA_ADMAIF_CIF_CTRL(19),
	TEGRA_ADMAIF_CIF_CTRL(20),
	SOC_SINGLE_EXT("APE Reg Dump", SND_SOC_NOPM, 0, 1, 0,
		tegra210_ape_dump_reg_get, tegra210_ape_dump_reg_put),
};

static struct snd_soc_codec_driver tegra210_admaif_codec = {
	.idle_bias_off = 1,
	.component_driver = {
		.dapm_widgets = tegra_admaif_widgets,
		.num_dapm_widgets = TEGRA210_ADMAIF_CHANNEL_COUNT * 4,
		.dapm_routes = tegra_admaif_routes,
		.num_dapm_routes = TEGRA210_ADMAIF_CHANNEL_COUNT * 6,
		.controls = tegra210_admaif_controls,
		.num_controls = ARRAY_SIZE(tegra210_admaif_controls),
	},
};

static struct snd_soc_codec_driver tegra186_admaif_codec = {
	.idle_bias_off = 1,
	.component_driver = {
		.dapm_widgets = tegra_admaif_widgets,
		.num_dapm_widgets = TEGRA186_ADMAIF_CHANNEL_COUNT * 4,
		.dapm_routes = tegra_admaif_routes,
		.num_dapm_routes = TEGRA186_ADMAIF_CHANNEL_COUNT * 6,
		.controls = tegra186_admaif_controls,
		.num_controls = ARRAY_SIZE(tegra186_admaif_controls),
	},
};

static struct tegra_admaif_soc_data soc_data_tegra210 = {
	.num_ch = TEGRA210_ADMAIF_CHANNEL_COUNT,
	.admaif_codec = &tegra210_admaif_codec,
	.codec_dais = tegra210_admaif_codec_dais,
	.regmap_conf = &tegra210_admaif_regmap_config,
	.global_base = TEGRA210_ADMAIF_GLOBAL_BASE,
	.tx_base = TEGRA210_ADMAIF_XBAR_TX_BASE,
	.rx_base = TEGRA210_ADMAIF_XBAR_RX_BASE,
	.is_isomgr_client = false,
};

static struct tegra_admaif_soc_data soc_data_tegra186 = {
	.num_ch = TEGRA186_ADMAIF_CHANNEL_COUNT,
	.admaif_codec = &tegra186_admaif_codec,
	.codec_dais = tegra186_admaif_codec_dais,
	.regmap_conf = &tegra186_admaif_regmap_config,
	.global_base = TEGRA186_ADMAIF_GLOBAL_BASE,
	.tx_base = TEGRA186_ADMAIF_XBAR_TX_BASE,
	.rx_base = TEGRA186_ADMAIF_XBAR_RX_BASE,
	.is_isomgr_client = true,
};


static const struct of_device_id tegra_admaif_of_match[] = {
	{ .compatible = "nvidia,tegra210-admaif", .data = &soc_data_tegra210 },
	{ .compatible = "nvidia,tegra186-admaif", .data = &soc_data_tegra186 },
	{},
};

static int tegra_admaif_probe(struct platform_device *pdev)
{
	int ret, i;
	struct tegra_admaif *admaif;
	void __iomem *regs;
	struct resource *res;
	const struct of_device_id *match;
	unsigned int buffer_size;

	match = of_match_device(tegra_admaif_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		return -ENODEV;
	}

	admaif = devm_kzalloc(&pdev->dev, sizeof(*admaif), GFP_KERNEL);
	if (!admaif)
		return -ENOMEM;

	admaif->dev = &pdev->dev;
	admaif->soc_data = (struct tegra_admaif_soc_data *)match->data;
	dev_set_drvdata(&pdev->dev, admaif);

	admaif->capture_dma_data = devm_kzalloc(&pdev->dev,
			sizeof(struct tegra_alt_pcm_dma_params) *
				admaif->soc_data->num_ch,
			GFP_KERNEL);
	if (!admaif->capture_dma_data)
		return -ENOMEM;

	admaif->playback_dma_data = devm_kzalloc(&pdev->dev,
			sizeof(struct tegra_alt_pcm_dma_params) *
				admaif->soc_data->num_ch,
			GFP_KERNEL);
	if (!admaif->playback_dma_data)
		return -ENOMEM;

	for (i = 0; i < ADMAIF_PATHS; i++) {
		admaif->audio_ch_override[i] =
			devm_kcalloc(&pdev->dev, admaif->soc_data->num_ch,
				     sizeof(unsigned int), GFP_KERNEL);
		if (!admaif->audio_ch_override[i])
			return -ENOMEM;

		admaif->client_ch_override[i] =
			devm_kcalloc(&pdev->dev, admaif->soc_data->num_ch,
				     sizeof(unsigned int), GFP_KERNEL);
		if (!admaif->client_ch_override[i])
			return -ENOMEM;

		admaif->mono_to_stereo[i] =
			devm_kcalloc(&pdev->dev, admaif->soc_data->num_ch,
				     sizeof(unsigned int), GFP_KERNEL);
		if (!admaif->mono_to_stereo[i])
			return -ENOMEM;

		admaif->stereo_to_mono[i] =
			devm_kcalloc(&pdev->dev, admaif->soc_data->num_ch,
				     sizeof(unsigned int), GFP_KERNEL);
		if (!admaif->stereo_to_mono[i])
			return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(regs))
		return PTR_ERR(regs);
	admaif->base_addr = regs;
	admaif->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					       admaif->soc_data->regmap_conf);
	if (IS_ERR(admaif->regmap)) {
		dev_err(&pdev->dev, "regmap init failed\n");
		return PTR_ERR(admaif->regmap);
	}
	regcache_cache_only(admaif->regmap, true);

	if (admaif->soc_data->is_isomgr_client)
		tegra_isomgr_adma_register();

	for (i = 0; i < admaif->soc_data->num_ch; i++) {
		admaif->playback_dma_data[i].addr = res->start +
			CH_TX_REG(TEGRA_ADMAIF_XBAR_TX_FIFO_WRITE, i);

		admaif->capture_dma_data[i].addr = res->start +
			CH_RX_REG(TEGRA_ADMAIF_XBAR_RX_FIFO_READ, i);

		admaif->playback_dma_data[i].width = 32;
		admaif->playback_dma_data[i].req_sel = i + 1;
		ret = of_property_read_string_index(pdev->dev.of_node,
				"dma-names",
				(i * 2) + 1,
				&admaif->playback_dma_data[i].chan_name);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Missing property nvidia,dma-names\n");
			return ret;
		}
		buffer_size = 0;
		if (of_property_read_u32_index(pdev->dev.of_node,
				"dma-buffer-size",
				(i * 2) + 1,
				&buffer_size) < 0) {
			dev_dbg(&pdev->dev,
				"Missing property nvidia,dma-buffer-size\n");
		}
		admaif->playback_dma_data[i].buffer_size = buffer_size;

		admaif->capture_dma_data[i].width = 32;
		admaif->capture_dma_data[i].req_sel = i + 1;
		ret = of_property_read_string_index(pdev->dev.of_node,
				"dma-names",
				(i * 2),
				&admaif->capture_dma_data[i].chan_name);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Missing property nvidia,dma-names\n");
			return ret;
		}
		buffer_size = 0;
		if (of_property_read_u32_index(pdev->dev.of_node,
				"dma-buffer-size",
				(i * 2),
				&buffer_size) < 0) {
			dev_dbg(&pdev->dev,
				"Missing property nvidia,dma-buffer-size\n");
		}
		admaif->capture_dma_data[i].buffer_size = buffer_size;
	}

	regmap_update_bits(admaif->regmap, admaif->soc_data->global_base +
			   TEGRA_ADMAIF_GLOBAL_ENABLE, 1, 1);

	pm_runtime_enable(&pdev->dev);
	ret = snd_soc_register_codec(&pdev->dev, admaif->soc_data->admaif_codec,
				     admaif->soc_data->codec_dais,
				     admaif->soc_data->num_ch * 3);
	if (ret != 0) {
		dev_err(&pdev->dev, "Could not register CODEC: %d\n", ret);
		goto pm_disable;
	}

	ret = tegra_alt_pcm_platform_register(&pdev->dev);
	if (ret) {
		dev_err(&pdev->dev, "Could not register PCM: %d\n", ret);
		goto unregister_codec;
	}

	return 0;

unregister_codec:
	snd_soc_unregister_codec(&pdev->dev);
pm_disable:
	pm_runtime_disable(&pdev->dev);

	return ret;
}

static int tegra_admaif_remove(struct platform_device *pdev)
{
	struct tegra_admaif *admaif = dev_get_drvdata(&pdev->dev);

	if (admaif->soc_data->is_isomgr_client)
		tegra_isomgr_adma_unregister();

	snd_soc_unregister_codec(&pdev->dev);

	tegra_alt_pcm_platform_unregister(&pdev->dev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra_admaif_runtime_suspend(&pdev->dev);

	return 0;
}

static const struct dev_pm_ops tegra_admaif_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra_admaif_runtime_suspend,
			   tegra_admaif_runtime_resume, NULL)
	SET_LATE_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				     pm_runtime_force_resume)
};

static struct platform_driver tegra_admaif_driver = {
	.probe = tegra_admaif_probe,
	.remove = tegra_admaif_remove,
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tegra_admaif_of_match,
		.pm = &tegra_admaif_pm_ops,
	},
};
module_platform_driver(tegra_admaif_driver);

MODULE_DEVICE_TABLE(of, tegra_admaif_of_match);
MODULE_AUTHOR("Songhee Baek <sbaek@nvidia.com>");
MODULE_DESCRIPTION("Tegra ADMAIF driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
