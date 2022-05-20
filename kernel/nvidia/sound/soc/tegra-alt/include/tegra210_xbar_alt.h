/*
 * tegra210_xbar_alt.h - TEGRA210 XBAR registers
 *
 * Copyright (c) 2014-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __TEGRA210_XBAR_ALT_H__
#define __TEGRA210_XBAR_ALT_H__

#define TEGRA210_XBAR_PART0_RX					0x0
#define TEGRA210_XBAR_PART1_RX					0x200
#define TEGRA210_XBAR_PART2_RX					0x400
#define TEGRA210_XBAR_RX_STRIDE					0x4
#define TEGRA210_XBAR_AUDIO_RX_COUNT				90

/* This register repeats twice for each XBAR TX CIF */
/* The fields in this register are 1 bit per XBAR RX CIF */

/* Fields in *_CIF_RX/TX_CTRL; used by AHUB FIFOs, and all other audio modules */

#define TEGRA210_AUDIOCIF_CTRL_FIFO_THRESHOLD_SHIFT	24
/* Channel count minus 1 */
#define TEGRA210_AUDIOCIF_CTRL_AUDIO_CHANNELS_SHIFT	20
/* Channel count minus 1 */
#define TEGRA210_AUDIOCIF_CTRL_CLIENT_CHANNELS_SHIFT	16

#define TEGRA210_AUDIOCIF_BITS_8			1
#define TEGRA210_AUDIOCIF_BITS_16			3
#define TEGRA210_AUDIOCIF_BITS_24			5
#define TEGRA210_AUDIOCIF_BITS_32			7

#define TEGRA210_AUDIOCIF_CTRL_AUDIO_BITS_SHIFT		12
#define TEGRA210_AUDIOCIF_CTRL_CLIENT_BITS_SHIFT	8
#define TEGRA210_AUDIOCIF_CTRL_EXPAND_SHIFT		6
#define TEGRA210_AUDIOCIF_CTRL_STEREO_CONV_SHIFT	4
#define TEGRA210_AUDIOCIF_CTRL_REPLICATE_SHIFT		3
#define TEGRA210_AUDIOCIF_CTRL_TRUNCATE_SHIFT		1
#define TEGRA210_AUDIOCIF_CTRL_MONO_CONV_SHIFT		0

/* Fields in *AHUBRAMCTL_CTRL; used by different AHUB modules */
#define TEGRA210_AHUBRAMCTL_CTRL_RW_READ		0
#define TEGRA210_AHUBRAMCTL_CTRL_RW_WRITE		(1 << 14)
#define TEGRA210_AHUBRAMCTL_CTRL_ADDR_INIT_EN		(1 << 13)
#define TEGRA210_AHUBRAMCTL_CTRL_SEQ_ACCESS_EN		(1 << 12)
#define TEGRA210_AHUBRAMCTL_CTRL_RAM_ADDR_MASK		0x1ff

#define TEGRA210_MAX_REGISTER_ADDR (TEGRA210_XBAR_PART2_RX +		\
	(TEGRA210_XBAR_RX_STRIDE * (TEGRA210_XBAR_AUDIO_RX_COUNT - 1)))

#define TEGRA186_XBAR_PART3_RX				0x600
#define TEGRA186_XBAR_AUDIO_RX_COUNT			115

#define TEGRA186_MAX_REGISTER_ADDR (TEGRA186_XBAR_PART3_RX +\
	(TEGRA210_XBAR_RX_STRIDE * (TEGRA186_XBAR_AUDIO_RX_COUNT - 1)))

#define TEGRA210_XBAR_REG_MASK_0		0xf1f03ff
#define TEGRA210_XBAR_REG_MASK_1		0x3f30031f
#define TEGRA210_XBAR_REG_MASK_2		0xff1cf313
#define TEGRA210_XBAR_REG_MASK_3		0x0
#define TEGRA210_XBAR_UPDATE_MAX_REG		3

#define TEGRA186_XBAR_REG_MASK_0		0xF3FFFFF
#define TEGRA186_XBAR_REG_MASK_1		0x3F310F1F
#define TEGRA186_XBAR_REG_MASK_2		0xFF3CF311
#define TEGRA186_XBAR_REG_MASK_3		0x3F0F00FF
#define TEGRA186_XBAR_UPDATE_MAX_REG		4

#define TEGRA_XBAR_UPDATE_MAX_REG		(TEGRA186_XBAR_UPDATE_MAX_REG)

#define MUX_ENUM_CTRL_DECL(ename, id)				\
	SOC_VALUE_ENUM_WIDE_DECL(ename##_enum, MUX_REG(id), 0,	\
				 tegra210_xbar_mux_texts,	\
				 tegra210_xbar_mux_values);	\
	static const struct snd_kcontrol_new ename##_control =	\
		SOC_DAPM_ENUM_EXT("Route", ename##_enum,	\
				  tegra_xbar_get_value_enum,	\
				  tegra_xbar_put_value_enum)

#define MUX_ENUM_CTRL_DECL_186(ename, id)			\
	SOC_VALUE_ENUM_WIDE_DECL(ename##_enum, MUX_REG(id), 0,	\
				 tegra186_xbar_mux_texts,	\
				 tegra186_xbar_mux_values);	\
	static const struct snd_kcontrol_new ename##_control =	\
		SOC_DAPM_ENUM_EXT("Route", ename##_enum,	\
				  tegra_xbar_get_value_enum,	\
				  tegra_xbar_put_value_enum)

#define DAI(sname)						\
	{							\
		.name = #sname,					\
		.playback = {					\
			.stream_name = #sname " Receive",	\
			.channels_min = 1,			\
			.channels_max = 16,			\
			.rates = SNDRV_PCM_RATE_8000_192000,	\
			.formats = SNDRV_PCM_FMTBIT_S8 |	\
				SNDRV_PCM_FMTBIT_S16_LE |	\
				SNDRV_PCM_FMTBIT_S32_LE,	\
		},						\
		.capture = {					\
			.stream_name = #sname " Transmit",	\
			.channels_min = 1,			\
			.channels_max = 16,			\
			.rates = SNDRV_PCM_RATE_8000_192000,	\
			.formats = SNDRV_PCM_FMTBIT_S8 |	\
				SNDRV_PCM_FMTBIT_S16_LE |	\
				SNDRV_PCM_FMTBIT_S32_LE,	\
		},						\
	}

#define MUX_REG(id) (TEGRA210_XBAR_RX_STRIDE * (id))

#define SOC_VALUE_ENUM_WIDE(xreg, shift, xmax, xtexts, xvalues) \
{	.reg = xreg, .shift_l = shift, .shift_r = shift,	\
	.items = xmax, .texts = xtexts, .values = xvalues,	\
	.mask = xmax ? roundup_pow_of_two(xmax) - 1 : 0}

#define SOC_VALUE_ENUM_WIDE_DECL(name, xreg, shift, xtexts, xvalues)	\
	static struct soc_enum name = SOC_VALUE_ENUM_WIDE(xreg, shift,	\
		ARRAY_SIZE(xtexts), xtexts, xvalues)

#define WIDGETS(sname, ename)						\
	SND_SOC_DAPM_AIF_IN(sname " RX", NULL, 0, SND_SOC_NOPM, 0, 0),	\
	SND_SOC_DAPM_AIF_OUT(sname " TX", NULL, 0, SND_SOC_NOPM, 0, 0), \
	SND_SOC_DAPM_MUX(sname " Mux", SND_SOC_NOPM, 0, 0,		\
			 &ename##_control)

#define TX_WIDGETS(sname)						\
	SND_SOC_DAPM_AIF_IN(sname " RX", NULL, 0, SND_SOC_NOPM, 0, 0),	\
	SND_SOC_DAPM_AIF_OUT(sname " TX", NULL, 0, SND_SOC_NOPM, 0, 0)

#define MUX_VALUE(npart, nbit) (1 + nbit + npart * 32)

#define IN_OUT_ROUTES(name)					\
	{ name " RX",       NULL,	name " Receive" },	\
	{ name " Transmit", NULL,       name " TX" },

struct tegra210_xbar_cif_conf {
	unsigned int threshold;
	unsigned int audio_channels;
	unsigned int client_channels;
	unsigned int audio_bits;
	unsigned int client_bits;
	unsigned int expand;
	unsigned int stereo_conv;
	union {
		unsigned int fifo_size_downshift;
		unsigned int replicate;
	};
	unsigned int truncate;
	unsigned int mono_conv;
};

struct tegra_xbar_soc_data {
	const struct regmap_config *regmap_config;
	unsigned int mask[4];
	unsigned int reg_count;
	unsigned int num_dais;
	struct snd_soc_codec_driver *codec_drv;
	struct snd_soc_dai_driver *dai_drv;
};

struct tegra_xbar {
	struct clk *clk;
	struct regmap *regmap;
	const struct tegra_xbar_soc_data *soc_data;
};

/* Extension of soc_bytes structure defined in sound/soc.h */
struct tegra_soc_bytes {
	struct soc_bytes soc;
	u32 shift; /* Used as offset for ahub ram related programing */
};

void tegra210_xbar_set_cif(struct regmap *regmap, unsigned int reg,
			  struct tegra210_xbar_cif_conf *conf);
void tegra210_xbar_write_ahubram(struct regmap *regmap, unsigned int reg_ctrl,
				unsigned int reg_data, unsigned int ram_offset,
				unsigned int *data, size_t size);
void tegra210_xbar_read_ahubram(struct regmap *regmap, unsigned int reg_ctrl,
				unsigned int reg_data, unsigned int ram_offset,
				unsigned int *data, size_t size);

/* Utility structures for using mixer control of type snd_soc_bytes */
#define TEGRA_SOC_BYTES_EXT(xname, xbase, xregs, xshift, xmask, \
	xhandler_get, xhandler_put, xinfo) \
	{.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	 .info = xinfo, .get = xhandler_get, .put = xhandler_put, \
	 .private_value = ((unsigned long)&(struct tegra_soc_bytes) \
		{.soc.base = xbase, .soc.num_regs = xregs, .soc.mask = xmask, \
		 .shift = xshift })}
#endif
