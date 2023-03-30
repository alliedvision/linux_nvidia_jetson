/*
 * drivers/video/tegra/host/tpg/tpg.c
 *
 * Tegra VI test pattern generator driver
 *
 * Copyright (c) 2017-2022, NVIDIA CORPORATION. All rights reserved.
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
#include <linux/init.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/debugfs.h>

#include <media/mc_common.h>
#include <media/csi.h>

#include "nvcsi/nvcsi.h"
#include "host1x/host1x.h"
#include "soc/tegra/camrtc-capture-messages.h"

#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif
/*
 * T19x TPG is generating 64 bits per cycle
 * it will insert (TPG_LANE_NUM-8) * nvcsi_clock cycles between
 * two 64bit pixel_packages to reduce framerate
 * TPG_LANE_NUM=8 means no blank insertion.
 * 7 means insert 1 clock between two 64bit pixel packages,
 * 6 means 2 clocks blank, …, 1 means 7 blank clocks.
 */
#define TPG_BLANK 6

#define TPG_HBLANK 0
#define TPG_VBLANK 40800

static struct tpg_frmfmt *frmfmt_table;

static bool override_frmfmt;
module_param(override_frmfmt, bool, 0444);
MODULE_PARM_DESC(override_frmfmt, "override existing format table");

static int framerate = 30;
module_param(framerate, int, 0444);

static bool ls_le = false;
module_param(ls_le, bool, 0644);
MODULE_PARM_DESC(ls_le, "Enable/disable LS/LE (line start and line end) in TPG. Default is OFF");

/* Leaving uninitialised to keep default value as false */
static bool emb_data;
module_param(emb_data, bool, 0644);
MODULE_PARM_DESC(emb_data, "Embedded-data generation by TPG. Default is OFF");

static bool phy = 0; /* 0 - DPHY, 1 - CPHY */
module_param(phy, bool, 0644);
MODULE_PARM_DESC(phy, "PHY mode, CPHY or DPHY. 0 - DPHY (default), 1 - CPHY");

static char *pattern = "PATCH";
module_param(pattern, charp, 0644);
MODULE_PARM_DESC(pattern, "Supported TPG patterns, PATCH, SINE. Default is PATCH mode");
#define PARAM_STRING_LENGTH	10

#define FREQ_RATE_RED 4
#define FREQ_RATE_GREEN 3
#define FREQ_RATE_BLUE 1

/* PG generate 32 bit per nvcsi_clk:
 * clks_per_line = width * bits_per_pixel / 32
 * ((clks_per_line + hblank) * height + vblank) * fps * lanes = nvcsi_clk_freq
 *
 */
static const struct tpg_frmfmt tegra19x_csi_tpg_frmfmt[] = {
	{{320, 240}, V4L2_PIX_FMT_SRGGB10, 30, 0, 0},
	{{1280, 720}, V4L2_PIX_FMT_SRGGB10, 30, 0, 0},
	{{1920, 1080}, V4L2_PIX_FMT_SRGGB10, 30, 0, 0},
	{{3840, 2160}, V4L2_PIX_FMT_SRGGB10, 30, 0, 0},
	{{1280, 720}, V4L2_PIX_FMT_RGB32, 30, 0, 0},
	{{1920, 1080}, V4L2_PIX_FMT_RGB32, 30, 0, 0},
	{{3840, 2160}, V4L2_PIX_FMT_RGB32, 30, 0, 0},
	{{1280, 720}, V4L2_PIX_FMT_NV16, 30, 0, 0},
	{{1920, 1080}, V4L2_PIX_FMT_NV16, 30, 0, 0},
	{{3840, 2160}, V4L2_PIX_FMT_NV16, 30, 0, 0},
	{{1280, 720}, V4L2_PIX_FMT_UYVY, 30, 0, 0},
	{{1920, 1080}, V4L2_PIX_FMT_UYVY, 30, 0, 0},
	{{3840, 2160}, V4L2_PIX_FMT_UYVY, 30, 0, 0},
};

#define TPG_PORT_IDX	0

static int tpg_debugfs_height_show(void *data, u64 *val)
{
	struct tegra_csi_channel *chan = data;
	struct tegra_csi_port *port = &chan->ports[TPG_PORT_IDX];

	mutex_lock(&chan->format_lock);
	*val = port->format.height;
	mutex_unlock(&chan->format_lock);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(tpg_debugfs_height_fops,
			tpg_debugfs_height_show,
			NULL,
			"%lld\n");

static int tpg_debugfs_width_show(void *data, u64 *val)
{
	struct tegra_csi_channel *chan = data;
	struct tegra_csi_port *port = &chan->ports[TPG_PORT_IDX];

	mutex_lock(&chan->format_lock);
	*val = port->format.width;
	mutex_unlock(&chan->format_lock);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(tpg_debugfs_width_fops,
			tpg_debugfs_width_show,
			NULL,
			"%lld\n");


static void tpg_remove_debugfs(struct tegra_csi_device *csi)
{
	debugfs_remove_recursive(csi->debugdir);
	csi->debugdir = NULL;
}

static int tpg_create_debugfs(struct tegra_csi_device *csi)
{
	struct dentry *dir;
	struct tegra_csi_channel *chan;

	csi->debugdir = dir = debugfs_create_dir("tpg", NULL);
	if (dir == NULL)
		return -ENOMEM;

	chan = csi->tpg_start;
	list_for_each_entry_from(chan, &csi->csi_chans, list) {
		const struct tegra_channel *vi_chan =
				v4l2_get_subdev_hostdata(&chan->subdev);
		if (vi_chan->pg_mode) {
			const char *name = vi_chan->video->name;

			dev_dbg(csi->dev, "debugfs node installed %s\n", name);
			dir = debugfs_create_dir(name, csi->debugdir);
			if (!dir)
				goto error;

			if (!debugfs_create_file("height", 0444,
						dir, chan,
						&tpg_debugfs_height_fops))
				goto error;
			if (!debugfs_create_file("width", 0444,
						dir, chan,
						&tpg_debugfs_width_fops))
				goto error;
		}
	}

	return 0;
error:
	tpg_remove_debugfs(csi);
	return -ENOMEM;
}

static int get_tpg_settings_t19x(struct tegra_csi_port *port,
				union nvcsi_tpg_config *const tpg_config)
{
	/* TPG native resolution */
	const size_t px_max = 0x4000;
	const size_t py_max = 0x2000;
	size_t hfreq = 0;
	size_t vfreq = 0;

	hfreq = px_max / port->format.width;
	vfreq = py_max / port->format.height;

	tpg_config->t194.virtual_channel_id = port->virtual_channel_id;
	tpg_config->t194.datatype = port->core_format->img_dt;

	tpg_config->t194.lane_count = TPG_BLANK;
	tpg_config->t194.flags = NVCSI_TPG_FLAG_PATCH_MODE;

	tpg_config->t194.initial_frame_number = 1;
	tpg_config->t194.maximum_frame_number = 32768;
	tpg_config->t194.image_width = port->format.width;
	tpg_config->t194.image_height = port->format.height;

	tpg_config->t194.red_horizontal_init_freq = hfreq;
	tpg_config->t194.red_vertical_init_freq = vfreq;

	tpg_config->t194.green_horizontal_init_freq = hfreq;
	tpg_config->t194.green_vertical_init_freq = vfreq;

	tpg_config->t194.blue_horizontal_init_freq = hfreq;
	tpg_config->t194.blue_vertical_init_freq = vfreq;

	return 0;
}

static void patch_pattern_tpg_settings(struct tegra_csi_port *port,
				union nvcsi_tpg_config *const tpg_config)
{
	const size_t px_max = 0x4000;
	const size_t py_max = 0x2000;
	size_t hfreq = 0;
	size_t vfreq = 0;

	hfreq = px_max / port->format.width;
	vfreq = py_max / port->format.height;

	tpg_config->tpg_ng.initial_phase_red = 0U;
	tpg_config->tpg_ng.red_horizontal_init_freq = hfreq;
	tpg_config->tpg_ng.red_vertical_init_freq = vfreq;

	tpg_config->tpg_ng.initial_phase_green = 0U;
	tpg_config->tpg_ng.green_horizontal_init_freq = hfreq;
	tpg_config->tpg_ng.green_vertical_init_freq = vfreq;

	tpg_config->tpg_ng.initial_phase_blue = 0U;
	tpg_config->tpg_ng.blue_horizontal_init_freq = hfreq;
	tpg_config->tpg_ng.blue_vertical_init_freq = vfreq;
}

#define freq(px, rate, size)	((px) - (rate * port->format.size / 2U))

static void sine_pattern_tpg_settings(struct tegra_csi_port *port,
				union nvcsi_tpg_config *const tpg_config)
{
	const size_t px_max = 0x4000;
	size_t hr_freq = freq(px_max, FREQ_RATE_RED, width);
	size_t vr_freq = freq(px_max, FREQ_RATE_RED, height);

	size_t hg_freq = freq(px_max, FREQ_RATE_GREEN, width);
	size_t vg_freq = freq(px_max, FREQ_RATE_GREEN, height);

	size_t hb_freq = freq(px_max, FREQ_RATE_BLUE, width);
	size_t vb_freq = freq(px_max, FREQ_RATE_BLUE, height);

	tpg_config->tpg_ng.initial_phase_red = 0U;
	tpg_config->tpg_ng.red_horizontal_init_freq = hr_freq;
	tpg_config->tpg_ng.red_vertical_init_freq = vr_freq;
	tpg_config->tpg_ng.red_horizontal_freq_rate = FREQ_RATE_RED;
	tpg_config->tpg_ng.red_vertical_freq_rate = FREQ_RATE_RED;

	tpg_config->tpg_ng.initial_phase_green = 0U;
	tpg_config->tpg_ng.green_horizontal_init_freq = hg_freq;
	tpg_config->tpg_ng.green_vertical_init_freq = vg_freq;
	tpg_config->tpg_ng.green_horizontal_freq_rate = FREQ_RATE_GREEN;
	tpg_config->tpg_ng.green_vertical_freq_rate = FREQ_RATE_GREEN;

	tpg_config->tpg_ng.initial_phase_blue = 0U;
	tpg_config->tpg_ng.blue_horizontal_init_freq = hb_freq;
	tpg_config->tpg_ng.blue_vertical_init_freq = vb_freq;
	tpg_config->tpg_ng.blue_horizontal_freq_rate = FREQ_RATE_BLUE;
	tpg_config->tpg_ng.blue_vertical_freq_rate = FREQ_RATE_BLUE;
}

static int get_tpg_settings_t23x(struct tegra_csi_port *port,
				union nvcsi_tpg_config *const tpg_config)
{
	/* TPG native resolution */
	u16 flags = 0;
	char param_name[PARAM_STRING_LENGTH];
	char *temp = NULL;

	dev_info(NULL, "pattern - %s cphy - %d ls-le - %d emb-data - %d\n",
			pattern, phy, ls_le, emb_data);

	tpg_config->tpg_ng.virtual_channel_id = port->virtual_channel_id;
	tpg_config->tpg_ng.datatype = port->core_format->img_dt;
	tpg_config->tpg_ng.stream_id = port->stream_id;

	flags |= (ls_le == true) ? NVCSI_TPG_FLAG_ENABLE_LS_LE : 0;
	flags |= (emb_data == true) ? NVCSI_TPG_FLAG_EMBEDDED_PATTERN_CONFIG_INFO : 0;
	flags |= (phy == true) ? NVCSI_TPG_FLAG_PHY_MODE_CPHY : 0;

	strncpy(param_name, pattern, PARAM_STRING_LENGTH);
	param_name[PARAM_STRING_LENGTH - 1] = '\0';
	temp = strstrip(param_name);

	if (strcmp(temp, "PATCH") == 0) {
		flags |= NVCSI_TPG_FLAG_PATCH_MODE;
		patch_pattern_tpg_settings(port, tpg_config);
	} else if (strcmp(temp, "SINE") == 0) {
		flags |= NVCSI_TPG_FLAG_SINE_MODE;
		sine_pattern_tpg_settings(port, tpg_config);
	} else {
		dev_err(NULL, "Error: Incorrect pattern - %s\n", pattern);
		return -EINVAL;
	}

	tpg_config->tpg_ng.flags = flags;

	tpg_config->tpg_ng.initial_frame_number = 1;
	tpg_config->tpg_ng.maximum_frame_number = 32768;
	tpg_config->tpg_ng.image_width = port->format.width;
	tpg_config->tpg_ng.image_height = port->format.height;

	tpg_config->tpg_ng.brightness_gain_ratio =
		CAPTURE_CSI_STREAM_TPG_GAIN_RATIO_NONE;

	tpg_config->tpg_ng.embedded_lines_top = 0;
	tpg_config->tpg_ng.embedded_line_width = 0;
	tpg_config->tpg_ng.embedded_lines_bottom = 0;
	if (emb_data) {
		tpg_config->tpg_ng.embedded_lines_top = 1;
		tpg_config->tpg_ng.embedded_line_width = 32;
		tpg_config->tpg_ng.embedded_lines_bottom = 0;
		/* Least significant 8 bits of flags */
		tpg_config->tpg_ng.emb_data_spare_0 = (uint8_t)(flags & 0xFF);
		/* Most significant 8 bits of flags */
		tpg_config->tpg_ng.emb_data_spare_1 =
			(uint8_t)((flags >> 8) & 0xFF);
	}

	return 0;
}

static int __init tpg_probe_t19x(void)
{
	struct tegra_csi_device *mc_csi = tegra_get_mc_csi();
	struct tegra_mc_vi *mc_vi = tegra_get_mc_vi();
	int err = 0;
	int i = 0;
	unsigned int table_size = ARRAY_SIZE(tegra19x_csi_tpg_frmfmt);
	const u8 chip_id = tegra_get_chip_id();

	if (!mc_vi || !mc_csi)
		return -EINVAL;

	if (chip_id == TEGRA194) {
		mc_csi->get_tpg_settings = get_tpg_settings_t19x;
		mc_csi->tpg_gain_ctrl = false;
		mc_csi->tpg_emb_data_config = false;
	} else if (chip_id == TEGRA234) {
		mc_csi->get_tpg_settings = get_tpg_settings_t23x;
		mc_csi->tpg_gain_ctrl = false;
		mc_csi->tpg_emb_data_config = emb_data;
	} else {
		dev_err(mc_csi->dev, "%s invalid chip-id : %d\n",
				__func__, chip_id);
		return -EINVAL;
	}

	dev_info(mc_csi->dev, "%s\n", __func__);
	mc_vi->csi = mc_csi;
	/* Init CSI related media controller interface */
	frmfmt_table = devm_kzalloc(mc_csi->dev,
			table_size * sizeof(struct tpg_frmfmt), GFP_KERNEL);
	if (!frmfmt_table)
		return -ENOMEM;

	mc_csi->tpg_frmfmt_table_size = table_size;
	memcpy(frmfmt_table, tegra19x_csi_tpg_frmfmt,
		table_size * sizeof(struct tpg_frmfmt));

	if (override_frmfmt) {
		for (i = 0; i < table_size; i++)
			frmfmt_table[i].framerate = framerate;
	}
	mc_csi->tpg_frmfmt_table = frmfmt_table;

	err = tpg_csi_media_controller_init(mc_csi, TEGRA_VI_PG_PATCH);
	if (err)
		return -EINVAL;
	err = tpg_vi_media_controller_init(mc_vi, TEGRA_VI_PG_PATCH);
	if (err)
		goto vi_init_err;

	err = tpg_create_debugfs(mc_csi);
	if (err)
		goto debugfs_init_err;

	return err;
debugfs_init_err:
	tpg_remove_debugfs(mc_csi);
vi_init_err:
	tpg_csi_media_controller_cleanup(mc_csi);
	dev_err(mc_csi->dev, "%s error\n", __func__);
	return err;
}
static void __exit tpg_remove_t19x(void)
{
	struct tegra_csi_device *mc_csi = tegra_get_mc_csi();
	struct tegra_mc_vi *mc_vi = tegra_get_mc_vi();

	if (!mc_vi || !mc_csi)
		return;

	dev_info(mc_csi->dev, "%s\n", __func__);
	tpg_remove_debugfs(mc_csi);
	tpg_csi_media_controller_cleanup(mc_csi);
	tpg_vi_media_controller_cleanup(mc_vi);

	mc_csi->tpg_frmfmt_table = NULL;
	mc_csi->tpg_frmfmt_table_size = 0;
	devm_kfree(mc_csi->dev, frmfmt_table);
}

module_init(tpg_probe_t19x);
module_exit(tpg_remove_t19x);
MODULE_LICENSE("GPL v2");
