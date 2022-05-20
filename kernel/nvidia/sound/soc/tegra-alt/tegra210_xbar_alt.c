/*
 * tegra210_xbar_alt.c - Tegra210 XBAR driver
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

#define DRV_NAME "tegra210-axbar"

static int tegra_xbar_get_value_enum(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_dapm_kcontrol_codec(kcontrol);
	struct tegra_xbar *xbar = snd_soc_codec_get_drvdata(codec);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int reg, i, bit_pos = 0;

	/*
	 * Find the bit position of current MUX input.
	 * If nothing is set, position would be 0 and it corresponds to 'None'.
	 */
	for (i = 0; i < xbar->soc_data->reg_count; i++) {
		unsigned int reg_val;

		reg = e->reg + (TEGRA210_XBAR_PART1_RX * i);
		reg_val = snd_soc_read(codec, reg) & xbar->soc_data->mask[i];

		if (reg_val) {
			bit_pos = ffs(reg_val) +
				  (8 * codec->component.val_bytes * i);
			break;
		}
	}

	/* Find index related to the item in array *_xbar_mux_texts[] */
	for (i = 0; i < e->items; i++) {
		if (bit_pos == e->values[i]) {
			ucontrol->value.enumerated.item[0] = i;
			break;
		}
	}

	return 0;
}

static int tegra_xbar_put_value_enum(struct snd_kcontrol *kctl,
				     struct snd_ctl_elem_value *uctl)
{
	struct snd_soc_codec *codec = snd_soc_dapm_kcontrol_codec(kctl);
	struct tegra_xbar *xbar = snd_soc_codec_get_drvdata(codec);
	struct snd_soc_dapm_context *dapm = snd_soc_dapm_kcontrol_dapm(kctl);
	struct soc_enum *e = (struct soc_enum *)kctl->private_value;
	struct snd_soc_dapm_update update[TEGRA_XBAR_UPDATE_MAX_REG] = { };
	unsigned int *item = uctl->value.enumerated.item;
	unsigned int value = e->values[item[0]];
	unsigned int i, bit_pos, reg_idx = 0, reg_val = 0;

	if (item[0] >= e->items)
		return -EINVAL;

	if (value) {
		/* get the register index and value to set */
		reg_idx = (value - 1) / (8 * codec->component.val_bytes);
		bit_pos = (value - 1) % (8 * codec->component.val_bytes);
		reg_val = BIT(bit_pos);
	}

	/*
	 * Run through all parts of a MUX register to find the state changes.
	 * There will be an additional update if new MUX input value is from
	 * different part of the MUX register.
	 */
	for (i = 0; i < xbar->soc_data->reg_count; i++) {
		update[i].reg = e->reg + (TEGRA210_XBAR_PART1_RX * i);
		update[i].val = (i == reg_idx) ? reg_val : 0;
		update[i].mask = xbar->soc_data->mask[i];
		update[i].kcontrol = kctl;

		/* update widget power if state has changed */
		if (snd_soc_test_bits(codec, update[i].reg, update[i].mask,
				      update[i].val))
			snd_soc_dapm_mux_update_power(dapm, kctl, item[0], e,
						      &update[i]);
	}

	return 0;
}

static struct snd_soc_dai_driver tegra210_xbar_dais[] = {
	DAI(ADMAIF1),
	DAI(ADMAIF2),
	DAI(ADMAIF3),
	DAI(ADMAIF4),
	DAI(ADMAIF5),
	DAI(ADMAIF6),
	DAI(ADMAIF7),
	DAI(ADMAIF8),
	DAI(ADMAIF9),
	DAI(ADMAIF10),
	DAI(I2S1),
	DAI(I2S2),
	DAI(I2S3),
	DAI(I2S4),
	DAI(I2S5),
	DAI(SFC1),
	DAI(SFC2),
	DAI(SFC3),
	DAI(SFC4),
	DAI(MIXER1-1),
	DAI(MIXER1-2),
	DAI(MIXER1-3),
	DAI(MIXER1-4),
	DAI(MIXER1-5),
	DAI(MIXER1-6),
	DAI(MIXER1-7),
	DAI(MIXER1-8),
	DAI(MIXER1-9),
	DAI(MIXER1-10),
	DAI(AFC1),
	DAI(AFC2),
	DAI(AFC3),
	DAI(AFC4),
	DAI(AFC5),
	DAI(AFC6),
	DAI(OPE1),
	DAI(SPKPROT1),
	DAI(MVC1),
	DAI(MVC2),
	DAI(IQC1-1),
	DAI(IQC1-2),
	DAI(IQC2-1),
	DAI(IQC2-2),
	DAI(DMIC1),
	DAI(DMIC2),
	DAI(DMIC3),
	DAI(AMX1),
	DAI(AMX1-1),
	DAI(AMX1-2),
	DAI(AMX1-3),
	DAI(AMX1-4),
	DAI(AMX2),
	DAI(AMX2-1),
	DAI(AMX2-2),
	DAI(AMX2-3),
	DAI(AMX2-4),
	DAI(ADX1-1),
	DAI(ADX1-2),
	DAI(ADX1-3),
	DAI(ADX1-4),
	DAI(ADX1),
	DAI(ADX2-1),
	DAI(ADX2-2),
	DAI(ADX2-3),
	DAI(ADX2-4),
	DAI(ADX2),
	DAI(OPE2),
};

static struct snd_soc_dai_driver tegra186_xbar_dais[] = {
	DAI(ADMAIF1),
	DAI(ADMAIF2),
	DAI(ADMAIF3),
	DAI(ADMAIF4),
	DAI(ADMAIF5),
	DAI(ADMAIF6),
	DAI(ADMAIF7),
	DAI(ADMAIF8),
	DAI(ADMAIF9),
	DAI(ADMAIF10),
	DAI(I2S1),
	DAI(I2S2),
	DAI(I2S3),
	DAI(I2S4),
	DAI(I2S5),
	DAI(SFC1),
	DAI(SFC2),
	DAI(SFC3),
	DAI(SFC4),
	DAI(MIXER1-1),
	DAI(MIXER1-2),
	DAI(MIXER1-3),
	DAI(MIXER1-4),
	DAI(MIXER1-5),
	DAI(MIXER1-6),
	DAI(MIXER1-7),
	DAI(MIXER1-8),
	DAI(MIXER1-9),
	DAI(MIXER1-10),
	DAI(AFC1),
	DAI(AFC2),
	DAI(AFC3),
	DAI(AFC4),
	DAI(AFC5),
	DAI(AFC6),
	DAI(OPE1),
	DAI(SPKPROT1),
	DAI(MVC1),
	DAI(MVC2),
	DAI(IQC1-1),
	DAI(IQC1-2),
	DAI(IQC2-1),
	DAI(IQC2-2),
	DAI(DMIC1),
	DAI(DMIC2),
	DAI(DMIC3),
	DAI(AMX1),
	DAI(AMX1-1),
	DAI(AMX1-2),
	DAI(AMX1-3),
	DAI(AMX1-4),
	DAI(AMX2),
	DAI(AMX2-1),
	DAI(AMX2-2),
	DAI(AMX2-3),
	DAI(AMX2-4),
	DAI(ADX1-1),
	DAI(ADX1-2),
	DAI(ADX1-3),
	DAI(ADX1-4),
	DAI(ADX1),
	DAI(ADX2-1),
	DAI(ADX2-2),
	DAI(ADX2-3),
	DAI(ADX2-4),
	DAI(ADX2),
	DAI(ADMAIF11),
	DAI(ADMAIF12),
	DAI(ADMAIF13),
	DAI(ADMAIF14),
	DAI(ADMAIF15),
	DAI(ADMAIF16),
	DAI(ADMAIF17),
	DAI(ADMAIF18),
	DAI(ADMAIF19),
	DAI(ADMAIF20),
	DAI(I2S6),
	DAI(AMX3),
	DAI(AMX3-1),
	DAI(AMX3-2),
	DAI(AMX3-3),
	DAI(AMX3-4),
	DAI(AMX4),
	DAI(AMX4-1),
	DAI(AMX4-2),
	DAI(AMX4-3),
	DAI(AMX4-4),
	DAI(ADX3-1),
	DAI(ADX3-2),
	DAI(ADX3-3),
	DAI(ADX3-4),
	DAI(ADX3),
	DAI(ADX4-1),
	DAI(ADX4-2),
	DAI(ADX4-3),
	DAI(ADX4-4),
	DAI(ADX4),
	DAI(DMIC4),
	DAI(ASRC1-1),
	DAI(ASRC1-2),
	DAI(ASRC1-3),
	DAI(ASRC1-4),
	DAI(ASRC1-5),
	DAI(ASRC1-6),
	DAI(ASRC1-7),
	DAI(ARAD1),
	DAI(DSPK1),
	DAI(DSPK2),
};

static const char * const tegra210_xbar_mux_texts[] = {
	"None",
	"ADMAIF1",
	"ADMAIF2",
	"ADMAIF3",
	"ADMAIF4",
	"ADMAIF5",
	"ADMAIF6",
	"ADMAIF7",
	"ADMAIF8",
	"ADMAIF9",
	"ADMAIF10",
	"I2S1",
	"I2S2",
	"I2S3",
	"I2S4",
	"I2S5",
	"SFC1",
	"SFC2",
	"SFC3",
	"SFC4",
	/* index 0..19 above are inputs of PART0 Mux */
	"MIXER1-1",
	"MIXER1-2",
	"MIXER1-3",
	"MIXER1-4",
	"MIXER1-5",
	"AMX1",
	"AMX2",
	"AFC1",
	"AFC2",
	"AFC3",
	"AFC4",
	"AFC5",
	"AFC6",
	/* index 20..34 above are inputs of PART1 Mux */
	"OPE1",
	"OPE2",
	"SPKPROT1",
	"MVC1",
	"MVC2",
	"IQC1-1",
	"IQC1-2",
	"IQC2-1",
	"IQC2-2",
	"DMIC1",
	"DMIC2",
	"DMIC3",
	"ADX1-1",
	"ADX1-2",
	"ADX1-3",
	"ADX1-4",
	"ADX2-1",
	"ADX2-2",
	"ADX2-3",
	"ADX2-4",
	/* index 35..53 above are inputs of PART2 Mux */
};

static const char * const tegra186_xbar_mux_texts[] = {
	"None",
	"ADMAIF1",
	"ADMAIF2",
	"ADMAIF3",
	"ADMAIF4",
	"ADMAIF5",
	"ADMAIF6",
	"ADMAIF7",
	"ADMAIF8",
	"ADMAIF9",
	"ADMAIF10",
	"ADMAIF11",
	"ADMAIF12",
	"ADMAIF13",
	"ADMAIF14",
	"ADMAIF15",
	"ADMAIF16",
	"I2S1",
	"I2S2",
	"I2S3",
	"I2S4",
	"I2S5",
	"I2S6",
	"SFC1",
	"SFC2",
	"SFC3",
	"SFC4",
	/* index 0..19 above are inputs of PART0 Mux */
	"MIXER1-1",
	"MIXER1-2",
	"MIXER1-3",
	"MIXER1-4",
	"MIXER1-5",
	"AMX1",
	"AMX2",
	"AMX3",
	"AMX4",
	"ARAD1",
	"AFC1",
	"AFC2",
	"AFC3",
	"AFC4",
	"AFC5",
	"AFC6",
	/* index 20..34 above are inputs of PART1 Mux */
	"OPE1",
	"SPKPROT1",
	"MVC1",
	"MVC2",
	"IQC1-1",
	"IQC1-2",
	"IQC2-1",
	"IQC2-2",
	"DMIC1",
	"DMIC2",
	"DMIC3",
	"DMIC4",
	"ADX1-1",
	"ADX1-2",
	"ADX1-3",
	"ADX1-4",
	"ADX2-1",
	"ADX2-2",
	"ADX2-3",
	"ADX2-4",
	/* index 35..53 above are inputs of PART2 Mux */
	"ADX3-1",
	"ADX3-2",
	"ADX3-3",
	"ADX3-4",
	"ADX4-1",
	"ADX4-2",
	"ADX4-3",
	"ADX4-4",
	"ADMAIF17",
	"ADMAIF18",
	"ADMAIF19",
	"ADMAIF20",
	"ASRC1-1",
	"ASRC1-2",
	"ASRC1-3",
	"ASRC1-4",
	"ASRC1-5",
	"ASRC1-6",
	/* index 54..71 above are inputs of PART3 Mux */
};

static const int tegra210_xbar_mux_values[] = {
	/* Mux0 input,	Mux1 input, Mux2 input */
	0,
	MUX_VALUE(0, 0),
	MUX_VALUE(0, 1),
	MUX_VALUE(0, 2),
	MUX_VALUE(0, 3),
	MUX_VALUE(0, 4),
	MUX_VALUE(0, 5),
	MUX_VALUE(0, 6),
	MUX_VALUE(0, 7),
	MUX_VALUE(0, 8),
	MUX_VALUE(0, 9),
	MUX_VALUE(0, 16),
	MUX_VALUE(0, 17),
	MUX_VALUE(0, 18),
	MUX_VALUE(0, 19),
	MUX_VALUE(0, 20),
	MUX_VALUE(0, 24),
	MUX_VALUE(0, 25),
	MUX_VALUE(0, 26),
	MUX_VALUE(0, 27),
	/* index 0..19 above are inputs of PART0 Mux */
	MUX_VALUE(1, 0),
	MUX_VALUE(1, 1),
	MUX_VALUE(1, 2),
	MUX_VALUE(1, 3),
	MUX_VALUE(1, 4),
	MUX_VALUE(1, 8),
	MUX_VALUE(1, 9),
	MUX_VALUE(1, 24),
	MUX_VALUE(1, 25),
	MUX_VALUE(1, 26),
	MUX_VALUE(1, 27),
	MUX_VALUE(1, 28),
	MUX_VALUE(1, 29),
	/* index 20..34 above are inputs of PART1 Mux */
	MUX_VALUE(2, 0),
	MUX_VALUE(2, 1),
	MUX_VALUE(2, 4),
	MUX_VALUE(2, 8),
	MUX_VALUE(2, 9),
	MUX_VALUE(2, 12),
	MUX_VALUE(2, 13),
	MUX_VALUE(2, 14),
	MUX_VALUE(2, 15),
	MUX_VALUE(2, 18),
	MUX_VALUE(2, 19),
	MUX_VALUE(2, 20),
	MUX_VALUE(2, 24),
	MUX_VALUE(2, 25),
	MUX_VALUE(2, 26),
	MUX_VALUE(2, 27),
	MUX_VALUE(2, 28),
	MUX_VALUE(2, 29),
	MUX_VALUE(2, 30),
	MUX_VALUE(2, 31),
	/* index 35..53 above are inputs of PART2 Mux */
};

static const int tegra186_xbar_mux_values[] = {
	/* Mux0 input,	Mux1 input, Mux2 input */
	0,
	MUX_VALUE(0, 0),
	MUX_VALUE(0, 1),
	MUX_VALUE(0, 2),
	MUX_VALUE(0, 3),
	MUX_VALUE(0, 4),
	MUX_VALUE(0, 5),
	MUX_VALUE(0, 6),
	MUX_VALUE(0, 7),
	MUX_VALUE(0, 8),
	MUX_VALUE(0, 9),
	MUX_VALUE(0, 10),
	MUX_VALUE(0, 11),
	MUX_VALUE(0, 12),
	MUX_VALUE(0, 13),
	MUX_VALUE(0, 14),
	MUX_VALUE(0, 15),
	MUX_VALUE(0, 16),
	MUX_VALUE(0, 17),
	MUX_VALUE(0, 18),
	MUX_VALUE(0, 19),
	MUX_VALUE(0, 20),
	MUX_VALUE(0, 21),
	MUX_VALUE(0, 24),
	MUX_VALUE(0, 25),
	MUX_VALUE(0, 26),
	MUX_VALUE(0, 27),
	/* index 0..19 above are inputs of PART0 Mux */
	MUX_VALUE(1, 0),
	MUX_VALUE(1, 1),
	MUX_VALUE(1, 2),
	MUX_VALUE(1, 3),
	MUX_VALUE(1, 4),
	MUX_VALUE(1, 8),
	MUX_VALUE(1, 9),
	MUX_VALUE(1, 10),
	MUX_VALUE(1, 11),
	MUX_VALUE(1, 16),
	MUX_VALUE(1, 24),
	MUX_VALUE(1, 25),
	MUX_VALUE(1, 26),
	MUX_VALUE(1, 27),
	MUX_VALUE(1, 28),
	MUX_VALUE(1, 29),
	/* index 20..34 above are inputs of PART1 Mux */
	MUX_VALUE(2, 0),
	MUX_VALUE(2, 4),
	MUX_VALUE(2, 8),
	MUX_VALUE(2, 9),
	MUX_VALUE(2, 12),
	MUX_VALUE(2, 13),
	MUX_VALUE(2, 14),
	MUX_VALUE(2, 15),
	MUX_VALUE(2, 18),
	MUX_VALUE(2, 19),
	MUX_VALUE(2, 20),
	MUX_VALUE(2, 21),
	MUX_VALUE(2, 24),
	MUX_VALUE(2, 25),
	MUX_VALUE(2, 26),
	MUX_VALUE(2, 27),
	MUX_VALUE(2, 28),
	MUX_VALUE(2, 29),
	MUX_VALUE(2, 30),
	MUX_VALUE(2, 31),
	/* index 35..53 above are inputs of PART2 Mux */
	MUX_VALUE(3, 0),
	MUX_VALUE(3, 1),
	MUX_VALUE(3, 2),
	MUX_VALUE(3, 3),
	MUX_VALUE(3, 4),
	MUX_VALUE(3, 5),
	MUX_VALUE(3, 6),
	MUX_VALUE(3, 7),
	MUX_VALUE(3, 16),
	MUX_VALUE(3, 17),
	MUX_VALUE(3, 18),
	MUX_VALUE(3, 19),
	MUX_VALUE(3, 24),
	MUX_VALUE(3, 25),
	MUX_VALUE(3, 26),
	MUX_VALUE(3, 27),
	MUX_VALUE(3, 28),
	MUX_VALUE(3, 29),
	/* index 54..71 above are inputs of PART3 Mux */
};

/* controls for t210 */
MUX_ENUM_CTRL_DECL(t210_admaif1_tx, 0x00);
MUX_ENUM_CTRL_DECL(t210_admaif2_tx, 0x01);
MUX_ENUM_CTRL_DECL(t210_admaif3_tx, 0x02);
MUX_ENUM_CTRL_DECL(t210_admaif4_tx, 0x03);
MUX_ENUM_CTRL_DECL(t210_admaif5_tx, 0x04);
MUX_ENUM_CTRL_DECL(t210_admaif6_tx, 0x05);
MUX_ENUM_CTRL_DECL(t210_admaif7_tx, 0x06);
MUX_ENUM_CTRL_DECL(t210_admaif8_tx, 0x07);
MUX_ENUM_CTRL_DECL(t210_admaif9_tx, 0x08);
MUX_ENUM_CTRL_DECL(t210_admaif10_tx, 0x09);
MUX_ENUM_CTRL_DECL(t210_i2s1_tx, 0x10);
MUX_ENUM_CTRL_DECL(t210_i2s2_tx, 0x11);
MUX_ENUM_CTRL_DECL(t210_i2s3_tx, 0x12);
MUX_ENUM_CTRL_DECL(t210_i2s4_tx, 0x13);
MUX_ENUM_CTRL_DECL(t210_i2s5_tx, 0x14);
MUX_ENUM_CTRL_DECL(t210_sfc1_tx, 0x18);
MUX_ENUM_CTRL_DECL(t210_sfc2_tx, 0x19);
MUX_ENUM_CTRL_DECL(t210_sfc3_tx, 0x1a);
MUX_ENUM_CTRL_DECL(t210_sfc4_tx, 0x1b);
MUX_ENUM_CTRL_DECL(t210_mixer11_tx, 0x20);
MUX_ENUM_CTRL_DECL(t210_mixer12_tx, 0x21);
MUX_ENUM_CTRL_DECL(t210_mixer13_tx, 0x22);
MUX_ENUM_CTRL_DECL(t210_mixer14_tx, 0x23);
MUX_ENUM_CTRL_DECL(t210_mixer15_tx, 0x24);
MUX_ENUM_CTRL_DECL(t210_mixer16_tx, 0x25);
MUX_ENUM_CTRL_DECL(t210_mixer17_tx, 0x26);
MUX_ENUM_CTRL_DECL(t210_mixer18_tx, 0x27);
MUX_ENUM_CTRL_DECL(t210_mixer19_tx, 0x28);
MUX_ENUM_CTRL_DECL(t210_mixer110_tx, 0x29);
MUX_ENUM_CTRL_DECL(t210_afc1_tx, 0x34);
MUX_ENUM_CTRL_DECL(t210_afc2_tx, 0x35);
MUX_ENUM_CTRL_DECL(t210_afc3_tx, 0x36);
MUX_ENUM_CTRL_DECL(t210_afc4_tx, 0x37);
MUX_ENUM_CTRL_DECL(t210_afc5_tx, 0x38);
MUX_ENUM_CTRL_DECL(t210_afc6_tx, 0x39);
MUX_ENUM_CTRL_DECL(t210_ope1_tx, 0x40);
MUX_ENUM_CTRL_DECL(t210_ope2_tx, 0x41);
MUX_ENUM_CTRL_DECL(t210_spkprot_tx, 0x44);
MUX_ENUM_CTRL_DECL(t210_mvc1_tx, 0x48);
MUX_ENUM_CTRL_DECL(t210_mvc2_tx, 0x49);
MUX_ENUM_CTRL_DECL(t210_amx11_tx, 0x50);
MUX_ENUM_CTRL_DECL(t210_amx12_tx, 0x51);
MUX_ENUM_CTRL_DECL(t210_amx13_tx, 0x52);
MUX_ENUM_CTRL_DECL(t210_amx14_tx, 0x53);
MUX_ENUM_CTRL_DECL(t210_amx21_tx, 0x54);
MUX_ENUM_CTRL_DECL(t210_amx22_tx, 0x55);
MUX_ENUM_CTRL_DECL(t210_amx23_tx, 0x56);
MUX_ENUM_CTRL_DECL(t210_amx24_tx, 0x57);
MUX_ENUM_CTRL_DECL(t210_adx1_tx, 0x58);
MUX_ENUM_CTRL_DECL(t210_adx2_tx, 0x59);

/* controls for t186 */
MUX_ENUM_CTRL_DECL_186(t186_admaif1_tx, 0x00);
MUX_ENUM_CTRL_DECL_186(t186_admaif2_tx, 0x01);
MUX_ENUM_CTRL_DECL_186(t186_admaif3_tx, 0x02);
MUX_ENUM_CTRL_DECL_186(t186_admaif4_tx, 0x03);
MUX_ENUM_CTRL_DECL_186(t186_admaif5_tx, 0x04);
MUX_ENUM_CTRL_DECL_186(t186_admaif6_tx, 0x05);
MUX_ENUM_CTRL_DECL_186(t186_admaif7_tx, 0x06);
MUX_ENUM_CTRL_DECL_186(t186_admaif8_tx, 0x07);
MUX_ENUM_CTRL_DECL_186(t186_admaif9_tx, 0x08);
MUX_ENUM_CTRL_DECL_186(t186_admaif10_tx, 0x09);
MUX_ENUM_CTRL_DECL_186(t186_i2s1_tx, 0x10);
MUX_ENUM_CTRL_DECL_186(t186_i2s2_tx, 0x11);
MUX_ENUM_CTRL_DECL_186(t186_i2s3_tx, 0x12);
MUX_ENUM_CTRL_DECL_186(t186_i2s4_tx, 0x13);
MUX_ENUM_CTRL_DECL_186(t186_i2s5_tx, 0x14);
MUX_ENUM_CTRL_DECL_186(t186_sfc1_tx, 0x18);
MUX_ENUM_CTRL_DECL_186(t186_sfc2_tx, 0x19);
MUX_ENUM_CTRL_DECL_186(t186_sfc3_tx, 0x1a);
MUX_ENUM_CTRL_DECL_186(t186_sfc4_tx, 0x1b);
MUX_ENUM_CTRL_DECL_186(t186_mixer11_tx, 0x20);
MUX_ENUM_CTRL_DECL_186(t186_mixer12_tx, 0x21);
MUX_ENUM_CTRL_DECL_186(t186_mixer13_tx, 0x22);
MUX_ENUM_CTRL_DECL_186(t186_mixer14_tx, 0x23);
MUX_ENUM_CTRL_DECL_186(t186_mixer15_tx, 0x24);
MUX_ENUM_CTRL_DECL_186(t186_mixer16_tx, 0x25);
MUX_ENUM_CTRL_DECL_186(t186_mixer17_tx, 0x26);
MUX_ENUM_CTRL_DECL_186(t186_mixer18_tx, 0x27);
MUX_ENUM_CTRL_DECL_186(t186_mixer19_tx, 0x28);
MUX_ENUM_CTRL_DECL_186(t186_mixer110_tx, 0x29);
MUX_ENUM_CTRL_DECL_186(t186_afc1_tx, 0x38);
MUX_ENUM_CTRL_DECL_186(t186_afc2_tx, 0x39);
MUX_ENUM_CTRL_DECL_186(t186_afc3_tx, 0x3a);
MUX_ENUM_CTRL_DECL_186(t186_afc4_tx, 0x3b);
MUX_ENUM_CTRL_DECL_186(t186_afc5_tx, 0x3c);
MUX_ENUM_CTRL_DECL_186(t186_afc6_tx, 0x3d);
MUX_ENUM_CTRL_DECL_186(t186_ope1_tx, 0x40);
MUX_ENUM_CTRL_DECL_186(t186_spkprot_tx, 0x44);
MUX_ENUM_CTRL_DECL_186(t186_mvc1_tx, 0x48);
MUX_ENUM_CTRL_DECL_186(t186_mvc2_tx, 0x49);
MUX_ENUM_CTRL_DECL_186(t186_amx11_tx, 0x50);
MUX_ENUM_CTRL_DECL_186(t186_amx12_tx, 0x51);
MUX_ENUM_CTRL_DECL_186(t186_amx13_tx, 0x52);
MUX_ENUM_CTRL_DECL_186(t186_amx14_tx, 0x53);
MUX_ENUM_CTRL_DECL_186(t186_amx21_tx, 0x54);
MUX_ENUM_CTRL_DECL_186(t186_amx22_tx, 0x55);
MUX_ENUM_CTRL_DECL_186(t186_amx23_tx, 0x56);
MUX_ENUM_CTRL_DECL_186(t186_amx24_tx, 0x57);
MUX_ENUM_CTRL_DECL_186(t186_adx1_tx, 0x60);
MUX_ENUM_CTRL_DECL_186(t186_adx2_tx, 0x61);
MUX_ENUM_CTRL_DECL_186(t186_dspk1_tx, 0x30);
MUX_ENUM_CTRL_DECL_186(t186_dspk2_tx, 0x31);
MUX_ENUM_CTRL_DECL_186(t186_amx31_tx, 0x58);
MUX_ENUM_CTRL_DECL_186(t186_amx32_tx, 0x59);
MUX_ENUM_CTRL_DECL_186(t186_amx33_tx, 0x5a);
MUX_ENUM_CTRL_DECL_186(t186_amx34_tx, 0x5b);
MUX_ENUM_CTRL_DECL_186(t186_amx41_tx, 0x64);
MUX_ENUM_CTRL_DECL_186(t186_amx42_tx, 0x65);
MUX_ENUM_CTRL_DECL_186(t186_amx43_tx, 0x66);
MUX_ENUM_CTRL_DECL_186(t186_amx44_tx, 0x67);
MUX_ENUM_CTRL_DECL_186(t186_admaif11_tx, 0x0a);
MUX_ENUM_CTRL_DECL_186(t186_admaif12_tx, 0x0b);
MUX_ENUM_CTRL_DECL_186(t186_admaif13_tx, 0x0c);
MUX_ENUM_CTRL_DECL_186(t186_admaif14_tx, 0x0d);
MUX_ENUM_CTRL_DECL_186(t186_admaif15_tx, 0x0e);
MUX_ENUM_CTRL_DECL_186(t186_admaif16_tx, 0x0f);
MUX_ENUM_CTRL_DECL_186(t186_i2s6_tx, 0x15);
MUX_ENUM_CTRL_DECL_186(t186_adx3_tx, 0x62);
MUX_ENUM_CTRL_DECL_186(t186_adx4_tx, 0x63);
MUX_ENUM_CTRL_DECL_186(t186_admaif17_tx, 0x68);
MUX_ENUM_CTRL_DECL_186(t186_admaif18_tx, 0x69);
MUX_ENUM_CTRL_DECL_186(t186_admaif19_tx, 0x6a);
MUX_ENUM_CTRL_DECL_186(t186_admaif20_tx, 0x6b);
MUX_ENUM_CTRL_DECL_186(t186_asrc11_tx, 0x6c);
MUX_ENUM_CTRL_DECL_186(t186_asrc12_tx, 0x6d);
MUX_ENUM_CTRL_DECL_186(t186_asrc13_tx, 0x6e);
MUX_ENUM_CTRL_DECL_186(t186_asrc14_tx, 0x6f);
MUX_ENUM_CTRL_DECL_186(t186_asrc15_tx, 0x70);
MUX_ENUM_CTRL_DECL_186(t186_asrc16_tx, 0x71);
MUX_ENUM_CTRL_DECL_186(t186_asrc17_tx, 0x72);

/*
 * The number of entries in, and order of, this array is closely tied to the
 * calculation of tegra210_xbar_codec.num_dapm_widgets near the end of
 * tegra210_xbar_probe()
 */
static const struct snd_soc_dapm_widget tegra210_xbar_widgets[] = {
	WIDGETS("ADMAIF1", t210_admaif1_tx),
	WIDGETS("ADMAIF2", t210_admaif2_tx),
	WIDGETS("ADMAIF3", t210_admaif3_tx),
	WIDGETS("ADMAIF4", t210_admaif4_tx),
	WIDGETS("ADMAIF5", t210_admaif5_tx),
	WIDGETS("ADMAIF6", t210_admaif6_tx),
	WIDGETS("ADMAIF7", t210_admaif7_tx),
	WIDGETS("ADMAIF8", t210_admaif8_tx),
	WIDGETS("ADMAIF9", t210_admaif9_tx),
	WIDGETS("ADMAIF10", t210_admaif10_tx),
	WIDGETS("I2S1", t210_i2s1_tx),
	WIDGETS("I2S2", t210_i2s2_tx),
	WIDGETS("I2S3", t210_i2s3_tx),
	WIDGETS("I2S4", t210_i2s4_tx),
	WIDGETS("I2S5", t210_i2s5_tx),
	WIDGETS("SFC1", t210_sfc1_tx),
	WIDGETS("SFC2", t210_sfc2_tx),
	WIDGETS("SFC3", t210_sfc3_tx),
	WIDGETS("SFC4", t210_sfc4_tx),
	WIDGETS("MIXER1-1", t210_mixer11_tx),
	WIDGETS("MIXER1-2", t210_mixer12_tx),
	WIDGETS("MIXER1-3", t210_mixer13_tx),
	WIDGETS("MIXER1-4", t210_mixer14_tx),
	WIDGETS("MIXER1-5", t210_mixer15_tx),
	WIDGETS("MIXER1-6", t210_mixer16_tx),
	WIDGETS("MIXER1-7", t210_mixer17_tx),
	WIDGETS("MIXER1-8", t210_mixer18_tx),
	WIDGETS("MIXER1-9", t210_mixer19_tx),
	WIDGETS("MIXER1-10", t210_mixer110_tx),
	WIDGETS("AFC1", t210_afc1_tx),
	WIDGETS("AFC2", t210_afc2_tx),
	WIDGETS("AFC3", t210_afc3_tx),
	WIDGETS("AFC4", t210_afc4_tx),
	WIDGETS("AFC5", t210_afc5_tx),
	WIDGETS("AFC6", t210_afc6_tx),
	WIDGETS("OPE1", t210_ope1_tx),
	WIDGETS("OPE2", t210_ope2_tx),
	WIDGETS("SPKPROT1", t210_spkprot_tx),
	WIDGETS("MVC1", t210_mvc1_tx),
	WIDGETS("MVC2", t210_mvc2_tx),
	WIDGETS("AMX1-1", t210_amx11_tx),
	WIDGETS("AMX1-2", t210_amx12_tx),
	WIDGETS("AMX1-3", t210_amx13_tx),
	WIDGETS("AMX1-4", t210_amx14_tx),
	WIDGETS("AMX2-1", t210_amx21_tx),
	WIDGETS("AMX2-2", t210_amx22_tx),
	WIDGETS("AMX2-3", t210_amx23_tx),
	WIDGETS("AMX2-4", t210_amx24_tx),
	WIDGETS("ADX1", t210_adx1_tx),
	WIDGETS("ADX2", t210_adx2_tx),
	TX_WIDGETS("IQC1-1"),
	TX_WIDGETS("IQC1-2"),
	TX_WIDGETS("IQC2-1"),
	TX_WIDGETS("IQC2-2"),
	TX_WIDGETS("DMIC1"),
	TX_WIDGETS("DMIC2"),
	TX_WIDGETS("DMIC3"),
	TX_WIDGETS("AMX1"),
	TX_WIDGETS("ADX1-1"),
	TX_WIDGETS("ADX1-2"),
	TX_WIDGETS("ADX1-3"),
	TX_WIDGETS("ADX1-4"),
	TX_WIDGETS("AMX2"),
	TX_WIDGETS("ADX2-1"),
	TX_WIDGETS("ADX2-2"),
	TX_WIDGETS("ADX2-3"),
	TX_WIDGETS("ADX2-4"),
};

static const struct snd_soc_dapm_widget tegra186_xbar_widgets[] = {
	WIDGETS("ADMAIF1", t186_admaif1_tx),
	WIDGETS("ADMAIF2", t186_admaif2_tx),
	WIDGETS("ADMAIF3", t186_admaif3_tx),
	WIDGETS("ADMAIF4", t186_admaif4_tx),
	WIDGETS("ADMAIF5", t186_admaif5_tx),
	WIDGETS("ADMAIF6", t186_admaif6_tx),
	WIDGETS("ADMAIF7", t186_admaif7_tx),
	WIDGETS("ADMAIF8", t186_admaif8_tx),
	WIDGETS("ADMAIF9", t186_admaif9_tx),
	WIDGETS("ADMAIF10", t186_admaif10_tx),
	WIDGETS("I2S1", t186_i2s1_tx),
	WIDGETS("I2S2", t186_i2s2_tx),
	WIDGETS("I2S3", t186_i2s3_tx),
	WIDGETS("I2S4", t186_i2s4_tx),
	WIDGETS("I2S5", t186_i2s5_tx),
	WIDGETS("SFC1", t186_sfc1_tx),
	WIDGETS("SFC2", t186_sfc2_tx),
	WIDGETS("SFC3", t186_sfc3_tx),
	WIDGETS("SFC4", t186_sfc4_tx),
	WIDGETS("MIXER1-1", t186_mixer11_tx),
	WIDGETS("MIXER1-2", t186_mixer12_tx),
	WIDGETS("MIXER1-3", t186_mixer13_tx),
	WIDGETS("MIXER1-4", t186_mixer14_tx),
	WIDGETS("MIXER1-5", t186_mixer15_tx),
	WIDGETS("MIXER1-6", t186_mixer16_tx),
	WIDGETS("MIXER1-7", t186_mixer17_tx),
	WIDGETS("MIXER1-8", t186_mixer18_tx),
	WIDGETS("MIXER1-9", t186_mixer19_tx),
	WIDGETS("MIXER1-10", t186_mixer110_tx),
	WIDGETS("AFC1", t186_afc1_tx),
	WIDGETS("AFC2", t186_afc2_tx),
	WIDGETS("AFC3", t186_afc3_tx),
	WIDGETS("AFC4", t186_afc4_tx),
	WIDGETS("AFC5", t186_afc5_tx),
	WIDGETS("AFC6", t186_afc6_tx),
	WIDGETS("OPE1", t186_ope1_tx),
	WIDGETS("SPKPROT1", t186_spkprot_tx),
	WIDGETS("MVC1", t186_mvc1_tx),
	WIDGETS("MVC2", t186_mvc2_tx),
	WIDGETS("AMX1-1", t186_amx11_tx),
	WIDGETS("AMX1-2", t186_amx12_tx),
	WIDGETS("AMX1-3", t186_amx13_tx),
	WIDGETS("AMX1-4", t186_amx14_tx),
	WIDGETS("AMX2-1", t186_amx21_tx),
	WIDGETS("AMX2-2", t186_amx22_tx),
	WIDGETS("AMX2-3", t186_amx23_tx),
	WIDGETS("AMX2-4", t186_amx24_tx),
	WIDGETS("ADX1", t186_adx1_tx),
	WIDGETS("ADX2", t186_adx2_tx),
	TX_WIDGETS("IQC1-1"),
	TX_WIDGETS("IQC1-2"),
	TX_WIDGETS("IQC2-1"),
	TX_WIDGETS("IQC2-2"),
	TX_WIDGETS("DMIC1"),
	TX_WIDGETS("DMIC2"),
	TX_WIDGETS("DMIC3"),
	TX_WIDGETS("AMX1"),
	TX_WIDGETS("ADX1-1"),
	TX_WIDGETS("ADX1-2"),
	TX_WIDGETS("ADX1-3"),
	TX_WIDGETS("ADX1-4"),
	TX_WIDGETS("AMX2"),
	TX_WIDGETS("ADX2-1"),
	TX_WIDGETS("ADX2-2"),
	TX_WIDGETS("ADX2-3"),
	TX_WIDGETS("ADX2-4"),
	WIDGETS("ADMAIF11", t186_admaif11_tx),
	WIDGETS("ADMAIF12", t186_admaif12_tx),
	WIDGETS("ADMAIF13", t186_admaif13_tx),
	WIDGETS("ADMAIF14", t186_admaif14_tx),
	WIDGETS("ADMAIF15", t186_admaif15_tx),
	WIDGETS("ADMAIF16", t186_admaif16_tx),
	WIDGETS("ADMAIF17", t186_admaif17_tx),
	WIDGETS("ADMAIF18", t186_admaif18_tx),
	WIDGETS("ADMAIF19", t186_admaif19_tx),
	WIDGETS("ADMAIF20", t186_admaif20_tx),
	WIDGETS("I2S6", t186_i2s6_tx),
	WIDGETS("AMX3-1", t186_amx31_tx),
	WIDGETS("AMX3-2", t186_amx32_tx),
	WIDGETS("AMX3-3", t186_amx33_tx),
	WIDGETS("AMX3-4", t186_amx34_tx),
	WIDGETS("AMX4-1", t186_amx41_tx),
	WIDGETS("AMX4-2", t186_amx42_tx),
	WIDGETS("AMX4-3", t186_amx43_tx),
	WIDGETS("AMX4-4", t186_amx44_tx),
	WIDGETS("ADX3", t186_adx3_tx),
	WIDGETS("ADX4", t186_adx4_tx),
	WIDGETS("ASRC1-1", t186_asrc11_tx),
	WIDGETS("ASRC1-2", t186_asrc12_tx),
	WIDGETS("ASRC1-3", t186_asrc13_tx),
	WIDGETS("ASRC1-4", t186_asrc14_tx),
	WIDGETS("ASRC1-5", t186_asrc15_tx),
	WIDGETS("ASRC1-6", t186_asrc16_tx),
	WIDGETS("ASRC1-7", t186_asrc17_tx),
	WIDGETS("DSPK1", t186_dspk1_tx),
	WIDGETS("DSPK2", t186_dspk2_tx),
	TX_WIDGETS("AMX3"),
	TX_WIDGETS("ADX3-1"),
	TX_WIDGETS("ADX3-2"),
	TX_WIDGETS("ADX3-3"),
	TX_WIDGETS("ADX3-4"),
	TX_WIDGETS("AMX4"),
	TX_WIDGETS("ADX4-1"),
	TX_WIDGETS("ADX4-2"),
	TX_WIDGETS("ADX4-3"),
	TX_WIDGETS("ADX4-4"),
	TX_WIDGETS("DMIC4"),
	TX_WIDGETS("ARAD1"),
};

#define TEGRA_COMMON_ROUTES(name)					\
	{ name " RX",       NULL,		name " Receive"},	\
	{ name " Transmit", NULL,		name " TX"},		\
	{ name " TX",       NULL,		name " Mux"},		\
	{ name " Mux",      "ADMAIF1",		"ADMAIF1 RX" },		\
	{ name " Mux",      "ADMAIF2",		"ADMAIF2 RX" },		\
	{ name " Mux",      "ADMAIF3",		"ADMAIF3 RX" },		\
	{ name " Mux",      "ADMAIF4",		"ADMAIF4 RX" },		\
	{ name " Mux",      "ADMAIF5",		"ADMAIF5 RX" },		\
	{ name " Mux",      "ADMAIF6",		"ADMAIF6 RX" },		\
	{ name " Mux",      "ADMAIF7",		"ADMAIF7 RX" },		\
	{ name " Mux",      "ADMAIF8",		"ADMAIF8 RX" },		\
	{ name " Mux",      "ADMAIF9",		"ADMAIF9 RX" },		\
	{ name " Mux",      "ADMAIF10",		"ADMAIF10 RX" },	\
	{ name " Mux",      "I2S1",		"I2S1 RX" },		\
	{ name " Mux",      "I2S2",		"I2S2 RX" },		\
	{ name " Mux",      "I2S3",		"I2S3 RX" },		\
	{ name " Mux",      "I2S4",		"I2S4 RX" },		\
	{ name " Mux",      "I2S5",		"I2S5 RX" },		\
	{ name " Mux",      "SFC1",		"SFC1 RX" },		\
	{ name " Mux",      "SFC2",		"SFC2 RX" },		\
	{ name " Mux",      "SFC3",		"SFC3 RX" },		\
	{ name " Mux",      "SFC4",		"SFC4 RX" },		\
	{ name " Mux",      "MIXER1-1",		"MIXER1-1 RX" },	\
	{ name " Mux",      "MIXER1-2",		"MIXER1-2 RX" },	\
	{ name " Mux",      "MIXER1-3",		"MIXER1-3 RX" },	\
	{ name " Mux",      "MIXER1-4",		"MIXER1-4 RX" },	\
	{ name " Mux",      "MIXER1-5",		"MIXER1-5 RX" },	\
	{ name " Mux",      "AFC1",		"AFC1 RX" },		\
	{ name " Mux",      "AFC2",		"AFC2 RX" },		\
	{ name " Mux",      "AFC3",		"AFC3 RX" },		\
	{ name " Mux",      "AFC4",		"AFC4 RX" },		\
	{ name " Mux",      "AFC5",		"AFC5 RX" },		\
	{ name " Mux",      "AFC6",		"AFC6 RX" },		\
	{ name " Mux",      "OPE1",		"OPE1 RX" },		\
	{ name " Mux",      "MVC1",		"MVC1 RX" },		\
	{ name " Mux",      "MVC2",		"MVC2 RX" },		\
	{ name " Mux",      "IQC1-1",		"IQC1-1 RX" },		\
	{ name " Mux",      "IQC1-2",		"IQC1-2 RX" },		\
	{ name " Mux",      "IQC2-1",		"IQC2-1 RX" },		\
	{ name " Mux",      "IQC2-2",		"IQC2-2 RX" },		\
	{ name " Mux",      "DMIC1",		"DMIC1 RX" },		\
	{ name " Mux",      "DMIC2",		"DMIC2 RX" },		\
	{ name " Mux",      "DMIC3",		"DMIC3 RX" },		\
	{ name " Mux",      "AMX1",		"AMX1 RX" },		\
	{ name " Mux",      "ADX1-1",		"ADX1-1 RX" },		\
	{ name " Mux",      "ADX1-2",		"ADX1-2 RX" },		\
	{ name " Mux",      "ADX1-3",		"ADX1-3 RX" },		\
	{ name " Mux",      "ADX1-4",		"ADX1-4 RX" },		\
	{ name " Mux",      "AMX2",		"AMX2 RX" },		\
	{ name " Mux",      "ADX2-1",		"ADX2-1 RX" },		\
	{ name " Mux",      "ADX2-2",		"ADX2-2 RX" },		\
	{ name " Mux",      "ADX2-3",		"ADX2-3 RX" },		\
	{ name " Mux",      "ADX2-4",		"ADX2-4 RX" },

#define TEGRA210_ONLY_ROUTES(name)					\
	{ name " Mux",      "OPE2",		"OPE2 RX" },

#define TEGRA186_ONLY_ROUTES(name)					\
	{ name " Mux",      "ADMAIF11",		"ADMAIF11 RX" },	\
	{ name " Mux",      "ADMAIF12",		"ADMAIF12 RX" },	\
	{ name " Mux",      "ADMAIF13",		"ADMAIF13 RX" },	\
	{ name " Mux",      "ADMAIF14",		"ADMAIF14 RX" },	\
	{ name " Mux",      "ADMAIF15",		"ADMAIF15 RX" },	\
	{ name " Mux",      "ADMAIF16",		"ADMAIF16 RX" },	\
	{ name " Mux",      "ADMAIF17",		"ADMAIF17 RX" },	\
	{ name " Mux",      "ADMAIF18",		"ADMAIF18 RX" },	\
	{ name " Mux",      "ADMAIF19",		"ADMAIF19 RX" },	\
	{ name " Mux",      "ADMAIF20",		"ADMAIF20 RX" },	\
	{ name " Mux",      "DMIC4",		"DMIC4 RX" },		\
	{ name " Mux",      "I2S6",		"I2S6 RX" },		\
	{ name " Mux",      "ASRC1-1",		"ASRC1-1 RX" },		\
	{ name " Mux",      "ASRC1-2",		"ASRC1-2 RX" },		\
	{ name " Mux",      "ASRC1-3",		"ASRC1-3 RX" },		\
	{ name " Mux",      "ASRC1-4",		"ASRC1-4 RX" },		\
	{ name " Mux",      "ASRC1-5",		"ASRC1-5 RX" },		\
	{ name " Mux",      "ASRC1-6",		"ASRC1-6 RX" },		\
	{ name " Mux",      "AMX3",		"AMX3 RX" },		\
	{ name " Mux",      "ADX3-1",		"ADX3-1 RX" },		\
	{ name " Mux",      "ADX3-2",		"ADX3-2 RX" },		\
	{ name " Mux",      "ADX3-3",		"ADX3-3 RX" },		\
	{ name " Mux",      "ADX3-4",		"ADX3-4 RX" },		\
	{ name " Mux",      "AMX4",		"AMX4 RX" },		\
	{ name " Mux",      "ADX4-1",		"ADX4-1 RX" },		\
	{ name " Mux",      "ADX4-2",		"ADX4-2 RX" },		\
	{ name " Mux",      "ADX4-3",		"ADX4-3 RX" },		\
	{ name " Mux",      "ADX4-4",		"ADX4-4 RX" },		\
	{ name " Mux",      "ARAD1",		"ARAD1 RX" },

#define TEGRA210_ROUTES(name)						\
	TEGRA_COMMON_ROUTES(name)					\
	TEGRA210_ONLY_ROUTES(name)

#define TEGRA186_ROUTES(name)						\
	TEGRA_COMMON_ROUTES(name)					\
	TEGRA186_ONLY_ROUTES(name)
/*
 * The number of entries in, and order of, this array is closely tied to the
 * calculation of tegra210_xbar_codec.num_dapm_routes near the end of
 * tegra210_xbar_probe()
 */
static const struct snd_soc_dapm_route tegra210_xbar_routes[] = {
	TEGRA210_ROUTES("ADMAIF1")
	TEGRA210_ROUTES("ADMAIF2")
	TEGRA210_ROUTES("ADMAIF3")
	TEGRA210_ROUTES("ADMAIF4")
	TEGRA210_ROUTES("ADMAIF5")
	TEGRA210_ROUTES("ADMAIF6")
	TEGRA210_ROUTES("ADMAIF7")
	TEGRA210_ROUTES("ADMAIF8")
	TEGRA210_ROUTES("ADMAIF9")
	TEGRA210_ROUTES("ADMAIF10")
	TEGRA210_ROUTES("I2S1")
	TEGRA210_ROUTES("I2S2")
	TEGRA210_ROUTES("I2S3")
	TEGRA210_ROUTES("I2S4")
	TEGRA210_ROUTES("I2S5")
	TEGRA210_ROUTES("SFC1")
	TEGRA210_ROUTES("SFC2")
	TEGRA210_ROUTES("SFC3")
	TEGRA210_ROUTES("SFC4")
	TEGRA210_ROUTES("MIXER1-1")
	TEGRA210_ROUTES("MIXER1-2")
	TEGRA210_ROUTES("MIXER1-3")
	TEGRA210_ROUTES("MIXER1-4")
	TEGRA210_ROUTES("MIXER1-5")
	TEGRA210_ROUTES("MIXER1-6")
	TEGRA210_ROUTES("MIXER1-7")
	TEGRA210_ROUTES("MIXER1-8")
	TEGRA210_ROUTES("MIXER1-9")
	TEGRA210_ROUTES("MIXER1-10")
	TEGRA210_ROUTES("AFC1")
	TEGRA210_ROUTES("AFC2")
	TEGRA210_ROUTES("AFC3")
	TEGRA210_ROUTES("AFC4")
	TEGRA210_ROUTES("AFC5")
	TEGRA210_ROUTES("AFC6")
	TEGRA210_ROUTES("OPE1")
	TEGRA210_ROUTES("OPE2")
	TEGRA210_ROUTES("SPKPROT1")
	TEGRA210_ROUTES("MVC1")
	TEGRA210_ROUTES("MVC2")
	TEGRA210_ROUTES("AMX1-1")
	TEGRA210_ROUTES("AMX1-2")
	TEGRA210_ROUTES("AMX1-3")
	TEGRA210_ROUTES("AMX1-4")
	TEGRA210_ROUTES("AMX2-1")
	TEGRA210_ROUTES("AMX2-2")
	TEGRA210_ROUTES("AMX2-3")
	TEGRA210_ROUTES("AMX2-4")
	TEGRA210_ROUTES("ADX1")
	TEGRA210_ROUTES("ADX2")
	IN_OUT_ROUTES("IQC1-1")
	IN_OUT_ROUTES("IQC1-2")
	IN_OUT_ROUTES("IQC2-1")
	IN_OUT_ROUTES("IQC2-1")
	IN_OUT_ROUTES("DMIC1")
	IN_OUT_ROUTES("DMIC2")
	IN_OUT_ROUTES("DMIC3")
	IN_OUT_ROUTES("AMX1")
	IN_OUT_ROUTES("AMX2")
	IN_OUT_ROUTES("ADX1-1")
	IN_OUT_ROUTES("ADX1-2")
	IN_OUT_ROUTES("ADX1-3")
	IN_OUT_ROUTES("ADX1-4")
	IN_OUT_ROUTES("ADX2-1")
	IN_OUT_ROUTES("ADX2-2")
	IN_OUT_ROUTES("ADX2-3")
	IN_OUT_ROUTES("ADX2-4")
};

static const struct snd_soc_dapm_route tegra186_xbar_routes[] = {
	TEGRA186_ROUTES("ADMAIF1")
	TEGRA186_ROUTES("ADMAIF2")
	TEGRA186_ROUTES("ADMAIF3")
	TEGRA186_ROUTES("ADMAIF4")
	TEGRA186_ROUTES("ADMAIF5")
	TEGRA186_ROUTES("ADMAIF6")
	TEGRA186_ROUTES("ADMAIF7")
	TEGRA186_ROUTES("ADMAIF8")
	TEGRA186_ROUTES("ADMAIF9")
	TEGRA186_ROUTES("ADMAIF10")
	TEGRA186_ROUTES("I2S1")
	TEGRA186_ROUTES("I2S2")
	TEGRA186_ROUTES("I2S3")
	TEGRA186_ROUTES("I2S4")
	TEGRA186_ROUTES("I2S5")
	TEGRA186_ROUTES("SFC1")
	TEGRA186_ROUTES("SFC2")
	TEGRA186_ROUTES("SFC3")
	TEGRA186_ROUTES("SFC4")
	TEGRA186_ROUTES("MIXER1-1")
	TEGRA186_ROUTES("MIXER1-2")
	TEGRA186_ROUTES("MIXER1-3")
	TEGRA186_ROUTES("MIXER1-4")
	TEGRA186_ROUTES("MIXER1-5")
	TEGRA186_ROUTES("MIXER1-6")
	TEGRA186_ROUTES("MIXER1-7")
	TEGRA186_ROUTES("MIXER1-8")
	TEGRA186_ROUTES("MIXER1-9")
	TEGRA186_ROUTES("MIXER1-10")
	TEGRA186_ROUTES("AFC1")
	TEGRA186_ROUTES("AFC2")
	TEGRA186_ROUTES("AFC3")
	TEGRA186_ROUTES("AFC4")
	TEGRA186_ROUTES("AFC5")
	TEGRA186_ROUTES("AFC6")
	TEGRA186_ROUTES("OPE1")
	TEGRA186_ROUTES("SPKPROT1")
	TEGRA186_ROUTES("MVC1")
	TEGRA186_ROUTES("MVC2")
	TEGRA186_ROUTES("AMX1-1")
	TEGRA186_ROUTES("AMX1-2")
	TEGRA186_ROUTES("AMX1-3")
	TEGRA186_ROUTES("AMX1-4")
	TEGRA186_ROUTES("AMX2-1")
	TEGRA186_ROUTES("AMX2-2")
	TEGRA186_ROUTES("AMX2-3")
	TEGRA186_ROUTES("AMX2-4")
	TEGRA186_ROUTES("ADX1")
	TEGRA186_ROUTES("ADX2")
	IN_OUT_ROUTES("IQC1-1")
	IN_OUT_ROUTES("IQC1-2")
	IN_OUT_ROUTES("IQC2-1")
	IN_OUT_ROUTES("IQC2-1")
	IN_OUT_ROUTES("DMIC1")
	IN_OUT_ROUTES("DMIC2")
	IN_OUT_ROUTES("DMIC3")
	IN_OUT_ROUTES("AMX1")
	IN_OUT_ROUTES("AMX2")
	IN_OUT_ROUTES("ADX1-1")
	IN_OUT_ROUTES("ADX1-2")
	IN_OUT_ROUTES("ADX1-3")
	IN_OUT_ROUTES("ADX1-4")
	IN_OUT_ROUTES("ADX2-1")
	IN_OUT_ROUTES("ADX2-2")
	IN_OUT_ROUTES("ADX2-3")
	IN_OUT_ROUTES("ADX2-4")
	TEGRA186_ROUTES("ADMAIF11")
	TEGRA186_ROUTES("ADMAIF12")
	TEGRA186_ROUTES("ADMAIF13")
	TEGRA186_ROUTES("ADMAIF14")
	TEGRA186_ROUTES("ADMAIF15")
	TEGRA186_ROUTES("ADMAIF16")
	TEGRA186_ROUTES("ADMAIF17")
	TEGRA186_ROUTES("ADMAIF18")
	TEGRA186_ROUTES("ADMAIF19")
	TEGRA186_ROUTES("ADMAIF20")
	TEGRA186_ROUTES("AMX3-1")
	TEGRA186_ROUTES("AMX3-2")
	TEGRA186_ROUTES("AMX3-3")
	TEGRA186_ROUTES("AMX3-4")
	TEGRA186_ROUTES("AMX4-1")
	TEGRA186_ROUTES("AMX4-2")
	TEGRA186_ROUTES("AMX4-3")
	TEGRA186_ROUTES("AMX4-4")
	TEGRA186_ROUTES("ADX3")
	TEGRA186_ROUTES("ADX4")
	TEGRA186_ROUTES("I2S6")
	TEGRA186_ROUTES("ASRC1-1")
	TEGRA186_ROUTES("ASRC1-2")
	TEGRA186_ROUTES("ASRC1-3")
	TEGRA186_ROUTES("ASRC1-4")
	TEGRA186_ROUTES("ASRC1-5")
	TEGRA186_ROUTES("ASRC1-6")
	TEGRA186_ROUTES("ASRC1-7")
	TEGRA186_ROUTES("DSPK1")
	TEGRA186_ROUTES("DSPK2")
	IN_OUT_ROUTES("DMIC4")
	IN_OUT_ROUTES("AMX3")
	IN_OUT_ROUTES("AMX4")
	IN_OUT_ROUTES("ADX3-1")
	IN_OUT_ROUTES("ADX3-2")
	IN_OUT_ROUTES("ADX3-3")
	IN_OUT_ROUTES("ADX3-4")
	IN_OUT_ROUTES("ADX4-1")
	IN_OUT_ROUTES("ADX4-2")
	IN_OUT_ROUTES("ADX4-3")
	IN_OUT_ROUTES("ADX4-4")
	IN_OUT_ROUTES("ARAD1")
};

static struct snd_soc_codec_driver tegra210_xbar_codec = {
	.idle_bias_off = 1,
	.component_driver = {
		.dapm_widgets = tegra210_xbar_widgets,
		.dapm_routes = tegra210_xbar_routes,
		.num_dapm_widgets = ARRAY_SIZE(tegra210_xbar_widgets),
		.num_dapm_routes = ARRAY_SIZE(tegra210_xbar_routes),
	},
};

static struct snd_soc_codec_driver tegra186_xbar_codec = {
	.idle_bias_off = 1,
	.component_driver = {
		.dapm_widgets = tegra186_xbar_widgets,
		.dapm_routes = tegra186_xbar_routes,
		.num_dapm_widgets = ARRAY_SIZE(tegra186_xbar_widgets),
		.num_dapm_routes = ARRAY_SIZE(tegra186_xbar_routes),
	},
};

static const struct regmap_config tegra210_xbar_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = TEGRA210_MAX_REGISTER_ADDR,
	.cache_type = REGCACHE_FLAT,
};

static const struct regmap_config tegra186_xbar_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = TEGRA186_MAX_REGISTER_ADDR,
	.cache_type = REGCACHE_FLAT,
};

static const struct tegra_xbar_soc_data soc_data_tegra210 = {
	.regmap_config = &tegra210_xbar_regmap_config,
	.mask[0] = TEGRA210_XBAR_REG_MASK_0,
	.mask[1] = TEGRA210_XBAR_REG_MASK_1,
	.mask[2] = TEGRA210_XBAR_REG_MASK_2,
	.mask[3] = TEGRA210_XBAR_REG_MASK_3,
	.reg_count = TEGRA210_XBAR_UPDATE_MAX_REG,
	.codec_drv = &tegra210_xbar_codec,
	.dai_drv = tegra210_xbar_dais,
	.num_dais = ARRAY_SIZE(tegra210_xbar_dais),
};

static const struct tegra_xbar_soc_data soc_data_tegra186 = {
	.regmap_config = &tegra186_xbar_regmap_config,
	.mask[0] = TEGRA186_XBAR_REG_MASK_0,
	.mask[1] = TEGRA186_XBAR_REG_MASK_1,
	.mask[2] = TEGRA186_XBAR_REG_MASK_2,
	.mask[3] = TEGRA186_XBAR_REG_MASK_3,
	.reg_count = TEGRA186_XBAR_UPDATE_MAX_REG,
	.codec_drv = &tegra186_xbar_codec,
	.dai_drv = tegra186_xbar_dais,
	.num_dais = ARRAY_SIZE(tegra186_xbar_dais),
};

static const struct of_device_id tegra_xbar_of_match[] = {
	{ .compatible = "nvidia,tegra210-axbar", .data = &soc_data_tegra210 },
	{ .compatible = "nvidia,tegra186-axbar", .data = &soc_data_tegra186 },
	{},
};

static int tegra_xbar_runtime_suspend(struct device *dev)
{
	struct tegra_xbar *xbar = dev_get_drvdata(dev);

#ifdef CONFIG_TEGRA186_AHC
	tegra186_free_ahc_interrupts();
#endif
	regcache_cache_only(xbar->regmap, true);
	regcache_mark_dirty(xbar->regmap);

	if (!(tegra_platform_is_fpga()))
		clk_disable_unprepare(xbar->clk);

	return 0;
}

static int tegra_xbar_runtime_resume(struct device *dev)
{
	struct tegra_xbar *xbar = dev_get_drvdata(dev);
	int ret;

	if (!(tegra_platform_is_fpga())) {
		ret = clk_prepare_enable(xbar->clk);
		if (ret) {
			dev_err(dev, "clk_prepare_enable failed: %d\n", ret);
			return ret;
		}
	}
#ifdef CONFIG_TEGRA186_AHC
	tegra186_setup_ahc_interrupts();
#endif
	regcache_cache_only(xbar->regmap, false);
	regcache_sync(xbar->regmap);

	return 0;
}

static int tegra_xbar_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct tegra_xbar *xbar;
	struct resource *res;
	void __iomem *regs;
	int ret;

	xbar = devm_kzalloc(&pdev->dev, sizeof(*xbar), GFP_KERNEL);
	if (!xbar)
		return -ENOMEM;

	match = of_match_device(tegra_xbar_of_match, &pdev->dev);
	if (!match) {
		dev_err(&pdev->dev, "Error: No device match found\n");
		return -ENODEV;
	}

	xbar->soc_data = (struct tegra_xbar_soc_data *)match->data;

	dev_set_drvdata(&pdev->dev, xbar);

	if (!(tegra_platform_is_fpga())) {
		xbar->clk = devm_clk_get(&pdev->dev, "ahub");
		if (IS_ERR(xbar->clk)) {
			dev_err(&pdev->dev, "Can't retrieve ahub clock\n");
			return PTR_ERR(xbar->clk);
		}
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	xbar->regmap = devm_regmap_init_mmio(&pdev->dev, regs,
					     xbar->soc_data->regmap_config);
	if (IS_ERR(xbar->regmap)) {
		dev_err(&pdev->dev, "regmap init failed\n");
		return PTR_ERR(xbar->regmap);
	}

	regcache_cache_only(xbar->regmap, true);

	ret = snd_soc_register_codec(&pdev->dev,
				     xbar->soc_data->codec_drv,
				     xbar->soc_data->dai_drv,
				     xbar->soc_data->num_dais);

	if (ret < 0)
		return ret;

	ret = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (ret < 0) {
		snd_soc_unregister_codec(&pdev->dev);
		return ret;
	}

	pm_runtime_enable(&pdev->dev);

	return 0;
}

static int tegra_xbar_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);

	pm_runtime_disable(&pdev->dev);
	if (!pm_runtime_status_suspended(&pdev->dev))
		tegra_xbar_runtime_suspend(&pdev->dev);

	return 0;
}

static const struct dev_pm_ops tegra_xbar_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra_xbar_runtime_suspend,
			   tegra_xbar_runtime_resume, NULL)
	SET_LATE_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				     pm_runtime_force_resume)
};

static struct platform_driver tegra_xbar_driver = {
	.probe = tegra_xbar_probe,
	.remove = tegra_xbar_remove,
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = tegra_xbar_of_match,
		.pm = &tegra_xbar_pm_ops,
	},
};
module_platform_driver(tegra_xbar_driver);

MODULE_AUTHOR("Stephen Warren <swarren@nvidia.com>");
MODULE_AUTHOR("Mohan Kumar <mkumard@nvidia.com>");
MODULE_DESCRIPTION("Tegra XBAR driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, tegra_xbar_of_match);
