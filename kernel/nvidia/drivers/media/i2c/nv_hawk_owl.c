/*
 * ar0234.c - ar0234 sensor driver
 *
 * Copyright (c) 2018-2022, NVIDIA CORPORATION.  All rights reserved.
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
#define DEBUG 0
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/module.h>

#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include <media/max9295.h>
#include <media/max9296.h>

#include <media/tegracam_core.h>
#include "hawk_owl_mode_tbls.h"
#include <linux/ktime.h>

#define CHANNEL_N 13
#define MAX_RADIAL_COEFFICIENTS         6
#define MAX_TANGENTIAL_COEFFICIENTS     2
#define MAX_FISHEYE_COEFFICIENTS        6
#define CAMERA_MAX_SN_LENGTH            32
#define MAX_RLS_COLOR_CHANNELS          4
#define MAX_RLS_BREAKPOINTS             6

extern int max96712_write_reg_Dser(int slaveAddr,int channel,
		u16 addr, u8 val);

extern int max96712_read_reg_Dser(int slaveAddr,int channel,
		                u16 addr, unsigned int *val);

#define AR0234_MIN_GAIN         (1)
#define AR0234_MAX_GAIN         (8)
#define AR0234_MAX_GAIN_REG     (0x40)
#define AR0234_DEFAULT_FRAME_LENGTH    (1224)
#define AR0234_COARSE_TIME_SHS1_ADDR    0x3012
#define AR0234_ANALOG_GAIN    0x3060

static const struct of_device_id ar0234_of_match[] = {
	{.compatible = "nvidia,ar0234_hawk_owl",},
	{ },
};
MODULE_DEVICE_TABLE(of, ar0234_of_match);

static const u32 ctrl_cid_list[] = {
	TEGRA_CAMERA_CID_GAIN,
	TEGRA_CAMERA_CID_EXPOSURE,
	TEGRA_CAMERA_CID_EXPOSURE_SHORT,
	TEGRA_CAMERA_CID_FRAME_RATE,
	TEGRA_CAMERA_CID_EEPROM_DATA,
	TEGRA_CAMERA_CID_HDR_EN,
	TEGRA_CAMERA_CID_SENSOR_MODE_ID,
	TEGRA_CAMERA_CID_STEREO_EEPROM,
};

// Coefficients as per distortion model (wide FOV) being used
typedef struct
{
	// Radial coefficients count
	u32 coeff_count;
	// Radial coefficients
	float k[MAX_FISHEYE_COEFFICIENTS];
	// 0 -> equidistant, 1 -> equisolid, 2 -> orthographic, 3 -> stereographic
	u32 mapping_type;
} fisheye_lens_distortion_coeff;

// Coefficients as per distortion model being used
typedef struct
{
	// Radial coefficients count
	u32 radial_coeff_count;
	// Radial coefficients
	float k[MAX_RADIAL_COEFFICIENTS];
	// Tangential coefficients count
	u32 tangential_coeff_count;
	// Tangential coefficients
	float p[MAX_TANGENTIAL_COEFFICIENTS];
} polynomial_lens_distortion_coeff;

/*
 * Stereo Eeprom Data
 */
typedef struct
{
	// Width and height of image in pixels
	u32 width, height;
	// Focal length in pixels
	float fx, fy;
	float skew;
	// Principal point (optical center) in pixels
	float cx, cy;
	/*
	 * Structure for distortion coefficients as per the model being used
	 * 0: pinhole, assuming polynomial distortion
	 * 1: fisheye, assuming fisheye distortion)
	 * 2: ocam (omini-directional)
	 */
	u32 distortion_type;
	union distortion_coefficients {
		polynomial_lens_distortion_coeff poly;
		fisheye_lens_distortion_coeff fisheye;
	} dist_coeff;
} camera_intrinsics;

/*
 * Extrinsic parameters shared by camera and IMU.
 * All rotation + translation with respect to the same reference point
 */
typedef struct
{
	/*
	 * Rotation parameter expressed in Rodrigues notation
	 * angle = sqrt(rx^2+ry^2+rz^2)
	 * unit axis = [rx,ry,rz]/angle
	 */
	float rx, ry, rz;
	// Translation parameter from one camera to another parameter
	float tx, ty, tz;
} camera_extrinsics;

typedef struct
{
	// 3D vector to add to accelerometer readings
	float linear_acceleration_bias[3];
	// 3D vector to add to gyroscope readings
	float angular_velocity_bias[3];
	// gravity acceleration
	float gravity_acceleration[3];
	// Extrinsic structure for IMU device
	camera_extrinsics extr;
	// Noise model parameters
	float update_rate;
	float linear_acceleration_noise_density;
	float linear_acceleration_random_walk;
	float angular_velocity_noise_density;
	float angular_velocity_random_walk;
} imu_params;

typedef struct {
	// Image height
	u16 image_height;
	// Image width
	u16 image_width;
	// Number of color channels
	u8 n_channels;
	// Coordinate x of center point
	float rls_x0[MAX_RLS_COLOR_CHANNELS];
	// Coordinate y of center point
	float rls_y0[MAX_RLS_COLOR_CHANNELS];
	// Ellipse xx parameter
	double ekxx[MAX_RLS_COLOR_CHANNELS];
	// Ellipse xy parameter
	double ekxy[MAX_RLS_COLOR_CHANNELS];
	// Ellipse yx parameter
	double ekyx[MAX_RLS_COLOR_CHANNELS];
	// Ellipse yy parameter
	double ekyy[MAX_RLS_COLOR_CHANNELS];
	// Number of breakpoints in LSC radial transfer function
	u8 rls_n_points;
	// LSC radial transfer function X
	float rls_rad_tf_x[MAX_RLS_COLOR_CHANNELS][MAX_RLS_BREAKPOINTS];
	// LSC radial transfer function Y
	float rls_rad_tf_y[MAX_RLS_COLOR_CHANNELS][MAX_RLS_BREAKPOINTS];
	// LSC radial transfer function slope
	float rls_rad_tf_slope[MAX_RLS_COLOR_CHANNELS][MAX_RLS_BREAKPOINTS];
	// rScale parameter
	u8 r_scale;
} radial_lsc_params;


typedef struct
{
	// Intrinsic structure for  camera device
	camera_intrinsics cam_intr;

	// Extrinsic structure for camera device
	camera_extrinsics cam_extr;

	// Flag for IMU availability
	u8 imu_present;

	// Intrinsic structure for IMU
	imu_params imu;

	// HAWK module serial number
	u8 serial_number[CAMERA_MAX_SN_LENGTH];

	// Radial Lens Shading Correction parameters
	radial_lsc_params rls;
} NvCamSyncSensorCalibData;

typedef struct
{
	/**
	 * EEPROM layout version
	 */
	u32 version;
	/**
	 * Factory Blob Flag, to set when factory flashed and reset to 0
	 * when user modified
	 */
	u32 factory_data;
	/**
	 * Intrinsic structure for camera device
	 */
	camera_intrinsics left_cam_intr;
	camera_intrinsics right_cam_intr;
	/**
	 * Extrinsic structure for camera device
	 */
	camera_extrinsics cam_extr;
	/**
	 * Flag for IMU 0-absent, 1-present
	 */
	u8 imu_present;
	/**
	 * Intrinsic structure for IMU
	 */
	imu_params imu;

	// HAWK module serial number
	u8 serial_number[CAMERA_MAX_SN_LENGTH];

	// Radial Lens Shading Correction parameters
	radial_lsc_params left_rls;
	radial_lsc_params right_rls;
} LiEeprom_Content_Struct;

struct ar0234 {
	struct camera_common_eeprom_data eeprom[AR0234_EEPROM_NUM_BLOCKS];
	u8                              eeprom_buf[AR0234_EEPROM_SIZE];
	struct i2c_client	*i2c_client;
	const struct i2c_device_id *id;
	struct v4l2_subdev	*subdev;
	u32	frame_length;
	struct camera_common_data	*s_data;
	struct tegracam_device		*tc_dev;
	u32                             channel;
	u32 	sync_sensor_index;
	NvCamSyncSensorCalibData EepromCalib;
};

static const struct regmap_config sensor_regmap_config = {
	.reg_bits = 16,
	.val_bits = 16,
	.cache_type = REGCACHE_RBTREE,
};

static inline void ar0234_get_coarse_time_regs_shs1(ar0234_reg *regs,
		u16 coarse_time)
{
	regs->addr = AR0234_COARSE_TIME_SHS1_ADDR;
	regs->val = (coarse_time) & 0xffff;

}

static inline void ar0234_get_gain_reg(ar0234_reg *regs,
		u16 gain)
{
	regs->addr = AR0234_ANALOG_GAIN;
	regs->val = (gain) & 0xffff;

}

static int test_mode;
module_param(test_mode, int, 0644);

static inline int ar0234_read_reg(struct camera_common_data *s_data,
		u16 addr, u16 *val)
{
	int err = 0;
	u32 reg_val = 0;

	err = regmap_read(s_data->regmap, addr, &reg_val);
	*val = reg_val & 0xFFFF;

	return err;
}

static int ar0234_write_reg(struct camera_common_data *s_data,
		u16 addr, u16 val)
{
	int err;
	struct device *dev = s_data->dev;

	err = regmap_write(s_data->regmap, addr, val);

	if (err)
		dev_err(dev, "%s:i2c write failed, 0x%x = %x\n",
				__func__, addr, val);
	return err;
}

static int ar0234_write_table(struct ar0234 *priv,
		const struct index_reg_8 table[])
{
	struct tegracam_device *tc_dev = priv->tc_dev;
	struct device *dev = tc_dev->dev;
	int i = 0;
	int ret = 0;
	int retry = 5;
	int retry_seraddr = 0x84;

	dev_dbg(dev, "%s: channel %d, ", __func__, priv->channel);
	while (table[i].source != 0x00) {
		if (table[i].source == 0x06) {
			retry = 1;

			if (table[i].addr == AR0234_TABLE_WAIT_MS)
			{
				dev_err(dev, "%s: sleep 500\n", __func__);
				msleep(table[i].val);
				i++;
				continue;
			}
retry_sensor:
			ret = ar0234_write_reg(priv->s_data, table[i].addr, table[i].val);
			if (ret) {
				retry--;
				if (retry > 0) {
					dev_warn(dev, "ar0234_write_reg: try %d\n", retry);
					msleep(4);
					goto retry_sensor;
				}
				return -1;
			} else {
				if (0x301a == table[i].addr || 0x3060 == table[i].addr)
					msleep(100);
			}
		} else {
			retry = 5;
			if (priv->channel == CHANNEL_N)
			{
				i++;
				continue;
			}
retry_serdes:
			/* To handle sync sensor i2c trans */
			if (2 == priv->sync_sensor_index)
				ret = max96712_write_reg_Dser(table[i].source,
						0/*channel-num*/, table[i].addr, (u8)table[i].val);
			else
				ret = max96712_write_reg_Dser(table[i].source,
					priv->channel, table[i].addr, (u8)table[i].val);
			/* To handle ser address change from 0x80 to 0x84
			 * after link enable at deser*/
			if (ret && (0x80 == table[i].source)) {
				ret = max96712_write_reg_Dser(retry_seraddr,
					  priv->channel, table[i].addr, (u8)table[i].val);
			}
			if (ret && (table[i].addr != 0x0000))
			{
				retry--;
				if (retry > 0) {
					dev_warn(dev, "max96712_write_reg_Dser: try %d\n", retry);
					msleep(4);
					goto retry_serdes;
				}
				return -1;
			}
			if (0x0010 == table[i].addr || 0x0000 == table[i].addr ||
					0x0006 == table[i].addr || 0x0018 == table[i].addr)
				msleep(300);
			else
				msleep(10);
		}
		i++;
	}
	return 0;
}

static int ar0234_hawk_owl_i2ctrans(struct ar0234 *priv)
{
	int err = 0;

	if ((1 == priv->channel)) { /* Quad link */
		err = ar0234_write_table(priv, i2c_address_trans_owl);
		if (err) {
			pr_err("%s: Failed to do i2c address trans..\n",__func__);
		} else {
			pr_err("%s: Successfully done I2c address trans..\n",__func__);
		}
	} else if ((!priv->channel) || (2 == priv->sync_sensor_index)) { /* Dual link */
		err = ar0234_write_table(priv, i2c_address_trans_hawk);
		if (err) {
			pr_err("%s: Failed to do i2c address trans..\n",__func__);
		} else {
			pr_err("%s: Successfully done I2c address trans..\n",__func__);
		}
	}
	return err;
}

static int ar0234_enable_pwdn_gpio(struct camera_common_data *s_data)
{
	int err = 0;
	struct camera_common_power_rail *pw = s_data->power;
	struct ar0234 *priv = (struct ar0234 *) s_data->priv;
	static int haw_flag = 0, owl_flag = 0;

	if (pw->pwdn_gpio > 0)
		gpio_set_value(pw->pwdn_gpio, 1);

	/* Serializer i2c address trans */
	if ((1 == priv->channel) && !owl_flag) {
		err = ar0234_write_table(priv, i2c_address_trans_owl_ser);
                if (err) {
                        pr_err("%s: Failed to do i2c address trans..\n",__func__);
                } else {
                        pr_err("%s: Successfully done I2c address trans..\n",__func__);
                }
		owl_flag++;

	} else if ((!priv->channel) && !haw_flag) {
		err = ar0234_write_table(priv, i2c_address_trans_hawk_ser);
                if (err) {
                        pr_err("%s: Failed to do i2c address trans..\n",__func__);
                } else {
                        pr_err("%s: Successfully done I2c address trans..\n",__func__);
                }
		haw_flag++;
	}

	return err;
}

static int ar0234_power_on(struct camera_common_data *s_data)
{
	int err = 0;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct device *dev = s_data->dev;
	struct ar0234 *priv = (struct ar0234 *) s_data->priv;

	if (pdata && pdata->power_on) {
		err = pdata->power_on(pw);
		if (err)
			dev_err(dev, "%s failed.\n", __func__);
		else
			pw->state = SWITCH_ON;
		return err;
	}

	if (pw->reset_gpio > 0)
		gpio_set_value(pw->reset_gpio, 1);

	usleep_range(1000, 2000);

	pw->state = SWITCH_ON;

	/*i2c address trans for Hawk & Owl*/
	err = ar0234_hawk_owl_i2ctrans(priv);
	return 0;
}

static int ar0234_power_off(struct camera_common_data *s_data)
{
	int err = 0;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct device *dev = s_data->dev;

	dev_err(dev, "%s:\n", __func__);
	if (pdata && pdata->power_off) {
		err = pdata->power_off(pw);
		if (!err)
			goto power_off_done;
		else
			dev_err(dev, "%s failed.\n", __func__);
		return err;
	}

power_off_done:
	pw->state = SWITCH_OFF;

	return 0;
}

static int ar0234_power_get(struct tegracam_device *tc_dev)
{
	struct device *dev = tc_dev->dev;
	struct camera_common_data *s_data = tc_dev->s_data;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	const char *mclk_name;
	const char *parentclk_name;
	struct clk *parent;
	int err = 0;

	mclk_name = pdata->mclk_name ?
		pdata->mclk_name : "cam_mclk1";
	pw->mclk = devm_clk_get(dev, mclk_name);
	if (IS_ERR(pw->mclk)) {
		dev_err(dev, "unable to get clock %s\n", mclk_name);
		return PTR_ERR(pw->mclk);
	}

	parentclk_name = pdata->parentclk_name;
	if (parentclk_name) {
		parent = devm_clk_get(dev, parentclk_name);
		if (IS_ERR(parent)) {
			dev_err(dev, "unable to get parent clcok %s",
					parentclk_name);
		} else
			clk_set_parent(pw->mclk, parent);
	}
	if (!err) {
		pw->reset_gpio = pdata->reset_gpio;
		pw->af_gpio = pdata->af_gpio;
		pw->pwdn_gpio = pdata->pwdn_gpio;
	}

	pw->state = SWITCH_OFF;

	return err;
}

static int ar0234_power_put(struct tegracam_device *tc_dev)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct camera_common_power_rail *pw = s_data->power;

	if (unlikely(!pw))
		return -EFAULT;

	return 0;
}

static int ar0234_set_group_hold(struct tegracam_device *tc_dev, bool val)
{
	struct device *dev = tc_dev->dev;
	int err = 0;

	if (err) {
		dev_err(dev,
				"%s: Group hold control error\n", __func__);
		return err;
	}

	return 0;
}

static int ar0234_set_gain(struct tegracam_device *tc_dev, s64 val)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct device *dev = tc_dev->dev;
	ar0234_reg reg_list[1];
	int err;
	u16 gain = (u16)val;
	u16 gain_reg = 0;

	if (val < 200) {
		gain_reg = (32 * (1000 - (100000/gain)))/1000;
	} else if (val < 400 && val >= 200) {
		gain = gain /2;
		gain_reg = (16 * (1000 - (100000/gain)))/1000 * 2;
		gain_reg = gain_reg + 0x10;
	} else if (val < 800 && val >= 400) {
		gain = gain / 4;
		gain_reg = (32 * (1000 - (100000/gain)))/1000;
		gain_reg = gain_reg + 0x20;
	} else if (val < 1600 && val >= 800) {
		gain = gain /8;
		gain_reg = (16 * (1000 - (100000/gain)))/1000 * 2;
		gain_reg = gain_reg + 0x30;
	} else if (val >= 1600) {
		gain_reg = 0x40;
	}

	if (gain > AR0234_MAX_GAIN_REG)
		gain = AR0234_MAX_GAIN_REG;
	ar0234_get_gain_reg(reg_list, gain_reg);
	err = ar0234_write_reg(s_data, reg_list[0].addr,
			reg_list[0].val);
	if (err)
		goto fail;
	return 0;

fail:
	dev_err(dev, "%s: GAIN control error\n", __func__);
	return err;
}

static int ar0234_set_frame_rate(struct tegracam_device *tc_dev, s64 val)
{
	struct ar0234 *priv = (struct ar0234 *)tegracam_get_privdata(tc_dev);

	if (val == 30000000) {  //30fps full resolution
		max96712_write_reg_Dser(0x52, priv->channel, 0x04A5, 0x35);
		max96712_write_reg_Dser(0x52, priv->channel, 0x04A6, 0xB7);
		max96712_write_reg_Dser(0x52, priv->channel, 0x04A7, 0x0C);
		priv->frame_length = 0xC20;
	} else if (val == 60000000) {//60fps full resolution
		max96712_write_reg_Dser(0x52, priv->channel, 0x04A5, 0x9A);
		max96712_write_reg_Dser(0x52, priv->channel, 0x04A6, 0x5B);
		max96712_write_reg_Dser(0x52, priv->channel, 0x04A7, 0x06);
		priv->frame_length = 0x610;
	} else if (val == 120000000) {//120fps binning resolution
		max96712_write_reg_Dser(0x52, priv->channel, 0x04A5, 0xCD);
		max96712_write_reg_Dser(0x52, priv->channel, 0x04A6, 0x2D);
		max96712_write_reg_Dser(0x52, priv->channel, 0x04A7, 0x03);
		priv->frame_length = 0x308;
	}

	return 0;
}

static int ar0234_set_exposure(struct tegracam_device *tc_dev, s64 val)
{
	struct ar0234 *priv = (struct ar0234 *)tegracam_get_privdata(tc_dev);
	struct camera_common_data *s_data = tc_dev->s_data;
	const struct sensor_mode_properties *mode =
		&s_data->sensor_props.sensor_modes[s_data->mode];
	ar0234_reg reg_list[1];
	int err;
	u32 coarse_time;
	u32 shs1;

	if (priv->frame_length == 0)
		priv->frame_length = AR0234_DEFAULT_FRAME_LENGTH;


	coarse_time = mode->signal_properties.pixel_clock.val *
		val / mode->image_properties.line_length /
		mode->control_properties.exposure_factor;

	if (coarse_time > priv->frame_length)
		coarse_time = priv->frame_length;
	shs1 = coarse_time;
	/* 0 and 1 are prohibited */
	if (shs1 < 2)
		shs1 = 2;

	ar0234_get_coarse_time_regs_shs1(reg_list, shs1);
	err = ar0234_write_reg(priv->s_data, reg_list[0].addr,
			reg_list[0].val);
	if (err)
		goto fail;

	return 0;

fail:
	dev_err(&priv->i2c_client->dev,
			"%s: set coarse time error\n", __func__);
	return err;
}

static int ar0234_fill_string_ctrl(struct tegracam_device *tc_dev,
		struct v4l2_ctrl *ctrl)
{
	struct ar0234 *priv = tc_dev->priv;
	int i, ret;

	switch (ctrl->id) {
		case TEGRA_CAMERA_CID_EEPROM_DATA:
			for (i = 0; i < AR0234_EEPROM_SIZE; i++) {
				ret = sprintf(&ctrl->p_new.p_char[i*2], "%02x",
						priv->eeprom_buf[i]);
				if (ret < 0)
					return -EINVAL;
			}
			break;
		default:
			return -EINVAL;
	}
	ctrl->p_cur.p_char = ctrl->p_new.p_char;

	return 0;
}

static int ar0234_fill_eeprom(struct tegracam_device *tc_dev,
				struct v4l2_ctrl *ctrl)
{
	struct ar0234 *priv = tc_dev->priv;
	LiEeprom_Content_Struct tmp;
	u32 test = 0;

	switch (ctrl->id) {
		case TEGRA_CAMERA_CID_STEREO_EEPROM:
			memset(&(priv->EepromCalib), 0, sizeof(NvCamSyncSensorCalibData));
			memset(ctrl->p_new.p, 0, sizeof(NvCamSyncSensorCalibData));
			memcpy(&tmp, priv->eeprom_buf, sizeof(LiEeprom_Content_Struct));

			if (priv->sync_sensor_index == 1) {
				priv->EepromCalib.cam_intr =  tmp.left_cam_intr;
			} else if (priv->sync_sensor_index == 2) {
				priv->EepromCalib.cam_intr =  tmp.right_cam_intr;
			} else {
				priv->EepromCalib.cam_intr =  tmp.left_cam_intr;
			}
			priv->EepromCalib.cam_extr = tmp.cam_extr;
			priv->EepromCalib.imu_present = tmp.imu_present;
			priv->EepromCalib.imu = tmp.imu;
			memcpy(priv->EepromCalib.serial_number, tmp.serial_number,
				CAMERA_MAX_SN_LENGTH);

			if (priv->sync_sensor_index == 1)
				priv->EepromCalib.rls = tmp.left_rls;
			else if (priv->sync_sensor_index == 2)
				priv->EepromCalib.rls = tmp.right_rls;
			else
				priv->EepromCalib.rls = tmp.left_rls;

			memcpy(ctrl->p_new.p, (u8 *)&(priv->EepromCalib),
					sizeof(NvCamSyncSensorCalibData));
			break;
		default:
			return -EINVAL;
	}

	memcpy(&test, &(priv->EepromCalib.cam_intr.fx), 4);

	ctrl->p_cur.p = ctrl->p_new.p;

	return 0;
}

static struct tegracam_ctrl_ops ar0234_ctrl_ops = {
	.numctrls = ARRAY_SIZE(ctrl_cid_list),
	.ctrl_cid_list = ctrl_cid_list,
	.string_ctrl_size = {AR0234_EEPROM_STR_SIZE},
	.compound_ctrl_size = {sizeof(NvCamSyncSensorCalibData)},
	.set_gain = ar0234_set_gain,
	.set_exposure = ar0234_set_exposure,
	.set_exposure_short = ar0234_set_exposure,
	.set_frame_rate = ar0234_set_frame_rate,
	.set_group_hold = ar0234_set_group_hold,
	.fill_string_ctrl = ar0234_fill_string_ctrl,
	.fill_compound_ctrl = ar0234_fill_eeprom,
};

static struct camera_common_pdata *ar0234_parse_dt(struct tegracam_device *tc_dev)
{
	struct device *dev = tc_dev->dev;
	struct device_node *node = dev->of_node;
	struct camera_common_pdata *board_priv_pdata;
	const struct of_device_id *match;
	int err;
	int gpio = 0;

	if (!node)
		return NULL;

	match = of_match_device(ar0234_of_match, dev);
	if (!match) {
		dev_err(dev, "Failed to find matching dt id\n");
		return NULL;
	}

	board_priv_pdata = devm_kzalloc(dev, sizeof(*board_priv_pdata), GFP_KERNEL);

	err = of_property_read_string(node, "mclk",
			&board_priv_pdata->mclk_name);
	if (err)
		dev_err(dev, "mclk not in DT\n");

	board_priv_pdata->reset_gpio = of_get_named_gpio(node,
			"reset-gpios", 0);
	gpio_direction_output(board_priv_pdata->reset_gpio, 1);

	board_priv_pdata->pwdn_gpio = of_get_named_gpio(node,
			"pwdn-gpios", 0);

	gpio_direction_output(board_priv_pdata->pwdn_gpio, 1);
	gpio = of_get_named_gpio(node,
			"pwr-gpios", 0);

	gpio_direction_output(gpio, 1);

	board_priv_pdata->has_eeprom =
		of_property_read_bool(node, "has-eeprom");
	return board_priv_pdata;
}

static int ar0234_set_mode(struct tegracam_device *tc_dev)
{
	struct ar0234 *priv = (struct ar0234 *)tegracam_get_privdata(tc_dev);
	struct camera_common_data *s_data = tc_dev->s_data;
	struct device *dev = tc_dev->dev;
	const struct of_device_id *match;
	int err;

	match = of_match_device(ar0234_of_match, dev);
	if (!match) {
		dev_err(dev, "Failed to find matching dt id\n");
		return -EINVAL;
	}

	err = ar0234_write_table(priv, mode_table[AR0234_MODE_STOP_STREAM]);
	if (err)
		return err;

	if (s_data->mode_prop_idx < 0)
		return -EINVAL;
	dev_err(dev, "%s: mode index:%d\n", __func__,s_data->mode_prop_idx);
	err = ar0234_write_table(priv, mode_table[s_data->mode_prop_idx]);
	if (err)
		return err;

	return 0;
}

static int ar0234_start_streaming(struct tegracam_device *tc_dev)
{
	struct ar0234 *priv = (struct ar0234 *)tegracam_get_privdata(tc_dev);
	int err;

	if (test_mode) {
		err = ar0234_write_table(priv,
				mode_table[AR0234_MODE_TEST_PATTERN]);
		if (err)
			return err;
	}


	err = ar0234_write_table(priv,
			mode_table[AR0234_MODE_START_STREAM]);
	if (err)
		return err;

	return 0;

}

static int ar0234_stop_streaming(struct tegracam_device *tc_dev)
{
	struct ar0234 *priv = (struct ar0234 *)tegracam_get_privdata(tc_dev);
	int err;

	err = ar0234_write_table(priv, mode_table[AR0234_MODE_STOP_STREAM]);
	if (err)
		return err;

	return 0;
}

static struct camera_common_sensor_ops ar0234_common_ops = {
	.numfrmfmts = ARRAY_SIZE(ar0234_frmfmt),
	.frmfmt_table = ar0234_frmfmt,
	.power_on = ar0234_power_on,
	.power_off = ar0234_power_off,
	.parse_dt = ar0234_parse_dt,
	.power_get = ar0234_power_get,
	.power_put = ar0234_power_put,
	.set_mode = ar0234_set_mode,
	.start_streaming = ar0234_start_streaming,
	.stop_streaming = ar0234_stop_streaming,
};

static int ar0234_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_err(&client->dev, "%s:\n", __func__);

	return 0;
}

static int ar0234_eeprom_device_release(struct ar0234 *priv)
{
	int i;

	for (i = 0; i < AR0234_EEPROM_NUM_BLOCKS; i++) {
		if (priv->eeprom[i].i2c_client != NULL) {
			i2c_unregister_device(priv->eeprom[i].i2c_client);
			priv->eeprom[i].i2c_client = NULL;
		}
	}

	return 0;
}

static const struct v4l2_subdev_internal_ops ar0234_subdev_internal_ops = {
	.open = ar0234_open,
};


static int ar0234_eeprom_device_init(struct ar0234 *priv)
{
	struct camera_common_pdata *pdata =  priv->s_data->pdata;
	char *dev_name = "eeprom_ar0234";
	static struct regmap_config eeprom_regmap_config = {
		.reg_bits = 8,
		.val_bits = 8
	};
	int i;
	int err;
	static int eeprom_addr = AR0234_EEPROM_ADDRESS;

	if (!pdata->has_eeprom)
		return -EINVAL;

	for (i = 0; i < AR0234_EEPROM_NUM_BLOCKS; i++) {
		priv->eeprom[i].adap = i2c_get_adapter(
				priv->i2c_client->adapter->nr);
		memset(&priv->eeprom[i].brd, 0, sizeof(priv->eeprom[i].brd));
		strncpy(priv->eeprom[i].brd.type, dev_name,
				sizeof(priv->eeprom[i].brd.type));

		if (priv->sync_sensor_index == 1) {
				priv->eeprom[i].brd.addr = eeprom_addr + i;
		}
		else if (priv->sync_sensor_index == 2)
			priv->eeprom[i].brd.addr = AR0234_EEPROM_ADDRESS_R + i;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
		priv->eeprom[i].i2c_client = i2c_new_device(
				priv->eeprom[i].adap, &priv->eeprom[i].brd);
#else
		priv->eeprom[i].i2c_client = i2c_new_client_device(
				priv->eeprom[i].adap, &priv->eeprom[i].brd);
#endif
		if (!priv->eeprom[i].i2c_client) {
			pr_err("%s: Failed to probe EEPORM at addr = 0x%x \n",
					__func__, priv->eeprom[i].brd.addr);
			return 0;
		}
		priv->eeprom[i].regmap = devm_regmap_init_i2c(
				priv->eeprom[i].i2c_client, &eeprom_regmap_config);
		if (IS_ERR(priv->eeprom[i].regmap)) {
			err = PTR_ERR(priv->eeprom[i].regmap);
			ar0234_eeprom_device_release(priv);
			return err;
		}
	}
	/* Move to next EEPROM addr */
	eeprom_addr += 2;
	return 0;
}

static int ar0234_read_eeprom(struct ar0234 *priv)
{
	int err, i;

	for (i = 0; i < AR0234_EEPROM_NUM_BLOCKS; i++) {
		err = regmap_bulk_read(priv->eeprom[i].regmap, 0,
				&priv->eeprom_buf[i * AR0234_EEPROM_BLOCK_SIZE],
				AR0234_EEPROM_BLOCK_SIZE);
		if (err) {
			return err;
		}
	}
#if DEBUG
	/* Print added, Remove later */
	for (i = 0; i < AR0234_EEPROM_BLOCK_SIZE; i+=8) {
		pr_err("%02x %02x %02x %02x %02x %02x %02x %02x",priv->eeprom_buf[i+0],\
			priv->eeprom_buf[i+1],priv->eeprom_buf[i+2],priv->eeprom_buf[i+3],\
			priv->eeprom_buf[i+4],priv->eeprom_buf[i+5],priv->eeprom_buf[i+6],
			priv->eeprom_buf[i+7]);
		pr_err("\n");
	}
#endif
	return 0;
}

static int ar0234_board_setup(struct ar0234 *priv)
{
	struct camera_common_data *s_data = priv->s_data;
	struct device *dev = s_data->dev;
	bool eeprom_ctrl = 0;
	int err = 0;

	dev_err(dev, "%s++\n", __func__);

	/* eeprom interface */
	err = ar0234_eeprom_device_init(priv);
	if (err && s_data->pdata->has_eeprom)
		dev_err(dev,
				"Failed to allocate eeprom reg map: %d\n", err);
	eeprom_ctrl = !err;
	err = camera_common_mclk_enable(s_data);
	if (err) {
		dev_err(dev,
				"Error %d turning on mclk\n", err);
		return err;
	}

	if (eeprom_ctrl) {
		err = ar0234_read_eeprom(priv);
		if (err) {
			dev_err(dev,
					"Error %d reading eeprom\n", err);
			goto error;
		}
	}

error:
	ar0234_power_off(s_data);
	camera_common_mclk_disable(s_data);
	return err;
}

static int ar0234_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct tegracam_device *tc_dev;
	struct ar0234 *priv;
	const char *str;
	int err;
	static int hawk_flag = 0, owl_flag = 0;

	dev_info(dev, "probing v4l2 sensor.\n");

	if (!IS_ENABLED(CONFIG_OF) || !node)
		return -EINVAL;

	priv = devm_kzalloc(dev, sizeof(struct ar0234), GFP_KERNEL);
	if (!priv) {
		dev_err(dev, "unable to allocate memory!\n");
		return -ENOMEM;
	}
	tc_dev = devm_kzalloc(dev,
			sizeof(struct tegracam_device), GFP_KERNEL);
	if (!tc_dev)
		return -ENOMEM;

	err = of_property_read_string(node, "channel", &str);
	if (err)
		dev_err(dev, "channel not found\n");
	priv->channel = str[0] - 'a';
	dev_err(dev, "%s: channel %d\n", __func__, priv->channel);


	err = of_property_read_u32(node, "sync_sensor_index",
			&priv->sync_sensor_index);
	if (err)
		dev_err(dev, "sync name index not in DT\n");

	priv->i2c_client = tc_dev->client = client;
	tc_dev->dev = dev;
	strncpy(tc_dev->name, "ar0234", sizeof(tc_dev->name));
	tc_dev->dev_regmap_config = &sensor_regmap_config;
	tc_dev->sensor_ops = &ar0234_common_ops;
	tc_dev->v4l2sd_internal_ops = &ar0234_subdev_internal_ops;
	tc_dev->tcctrl_ops = &ar0234_ctrl_ops;

	err = tegracam_device_register(tc_dev);
	if (err) {
		dev_err(dev, "tegra camera driver registration failed\n");
		return err;
	}
	priv->tc_dev = tc_dev;
	priv->s_data = tc_dev->s_data;
	priv->subdev = &tc_dev->s_data->subdev;
	tegracam_set_privdata(tc_dev, (void *)priv);

	/*Power down gpio enable */
	ar0234_enable_pwdn_gpio(tc_dev->s_data);
	ar0234_power_on(tc_dev->s_data);
	msleep(100);

	err = ar0234_write_table(priv, mode_table[AR0234_MODE_STOP_STREAM]);
	if (err) {
		dev_err(&client->dev,"ar0234 detect error\n");
		return err;
	}
	msleep(100);
	/* Deser/ser programming */
	if ((1 == priv->channel) && (!owl_flag)) {
		err = ar0234_write_table(priv, mode_table[AR0234_MODE_Owl_Dser_Ser]);
		if (err)
			pr_err("%s: Failed to do OWL mode table..\n",__func__);
		else
			pr_err("%s: Successfully done OWL mode table ..\n",__func__);
		owl_flag++;
	}
	if ((!priv->channel) && !hawk_flag) {
		err = ar0234_write_table(priv, mode_table[AR0234_MODE_Hawk_Dser_Ser]);
		if (err)
			pr_err("%s: Failed to do Hawk  mode table..\n",__func__);
		else
			pr_err("%s: Successfully done Hawk mode table ..\n",__func__);
		hawk_flag++;
	}

	/* i2c address trans for EEPROM access */
	if ((1 == priv->channel)) {
		err = ar0234_write_table(priv, i2c_address_trans_owl_eeprom);
		if (err) {
			dev_err(&client->dev,"Owl camera Eeprom i2c address trans error\n");
			return err;
		} else {
			dev_err(&client->dev,"Owl camera Eeprom i2c address trans success!!!\n");
		}

	} else if ((!priv->channel)) {
		err = ar0234_write_table(priv, i2c_address_trans_hawk_eeprom);
		if (err) {
			dev_err(&client->dev,"Hawk camera Eeprom i2c address trans error\n");
			return err;
		} else {
			dev_err(&client->dev,"Hawk camera Eeprom i2c address trans success!!!\n");
		}
	}

	/*Fixme: EEPROM is not enabled for Hawk */
	if (/*(!priv->channel) ||*/ (1 == priv->channel)) {
		err = ar0234_board_setup(priv);
		if (err) {
			dev_err(dev, "board setup failed\n");
			return err;
		}
	}

	/* re-i2c address trans for Hawk & Owl sensors */
	if ((1 == priv->channel)) {
		err = ar0234_write_table(priv, i2c_address_trans_owl);
		if (err) {
			dev_err(&client->dev,"Owl camera Eeprom i2c address trans back error\n");
			return err;
		} else {
			dev_err(&client->dev,"Owl camera Eeprom i2c address trans back success!!!\n");
		}
	} else if ((!priv->channel)) {
		err = ar0234_write_table(priv, i2c_address_trans_hawk);
		if (err) {
			dev_err(&client->dev,"Hawk camera Eeprom i2c address trans back error\n");
			return err;
		} else {
			dev_err(&client->dev,"Hawk camera Eeprom i2c address trans back success!!!\n");
		}
	}

	err = tegracam_v4l2subdev_register(tc_dev, true);
	if (err) {
	}

	dev_err(&client->dev, "Detected AR0234 sensor\n");

	return 0;
}

static int ar0234_remove(struct i2c_client *client)
{
	struct camera_common_data *s_data = to_camera_common_data(&client->dev);
	struct ar0234 *priv = (struct ar0234 *)s_data->priv;

	tegracam_v4l2subdev_unregister(priv->tc_dev);
	tegracam_device_unregister(priv->tc_dev);
	ar0234_eeprom_device_release(priv);

	return 0;
}

static const struct i2c_device_id ar0234_id[] = {
	{ "ar0234", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, ar0234_id);

static struct i2c_driver ar0234_i2c_driver = {
	.driver = {
		.name = "ar0234",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ar0234_of_match),
	},
	.probe = ar0234_probe,
	.remove = ar0234_remove,
	.id_table = ar0234_id,
};

module_i2c_driver(ar0234_i2c_driver);

MODULE_DESCRIPTION("Media Controller driver for Sony AR0234");
MODULE_AUTHOR("NVIDIA Corporation");
MODULE_AUTHOR("Praveen AC <pac@nvidia.com");
MODULE_LICENSE("GPL v2");
