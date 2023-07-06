/*
 * SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 */

#include <linux/of.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>

#define DEVICE_NAME "f75308"
#define DEVICE_VID_ADDR 0xC0
#define DEVICE_PID_ADDR 0xC2

#define DEVICE_VID 0x1934

#define DEVICE_PID_64PIN 0x1012
#define DEVICE_PID_48PIN 0x1022
#define DEVICE_PID_28PIN 0x1032

#define F75308_REG_BANK 0x00

/* BANK-0 */
#define F75308_REG_VOLT(nr) (0x30 + (nr)) /* 0 ~ 14 */
#define F75308_REG_TEMP_READ(nr) (0x40 + (nr * 2)) /* 0 ~ 6 */
#define F75308_REG_FAN_READ(nr) (0x80 + (nr * 2)) /* 0 ~ 14 */

#define F75308_MAX_FAN_IN 14
#define F75308_MAX_FAN_CTRL_CNT 11
#define F75308_MAX_FAN_SEG_CNT 5

enum chip {
	f75308a_28,
	f75308b_48,
	f75308c_64,
};

struct f75308_priv {
	struct mutex locker;
	struct i2c_client *client;
	struct device *hwmon_dev;
	enum chip chip_id;
};

static ssize_t f75308_show_temp(struct device *dev,
				struct device_attribute *devattr, char *buf);

static ssize_t f75308_show_in(struct device *dev,
			      struct device_attribute *devattr, char *buf);

static ssize_t f75308_show_fan(struct device *dev,
			       struct device_attribute *devattr, char *buf);

static ssize_t f75308_show_pwm(struct device *dev,
			       struct device_attribute *devattr, char *buf);

static ssize_t f75308_set_pwm(struct device *dev,
			      struct device_attribute *devattr, const char *buf,
			      size_t count);

static ssize_t f75308_show_fan_type(struct device *dev,
				    struct device_attribute *devattr,
				    char *buf);

static ssize_t f75308_set_fan_type(struct device *dev,
				   struct device_attribute *devattr,
				   const char *buf, size_t count);

static ssize_t f75308_show_fan_mode(struct device *dev,
				    struct device_attribute *devattr,
				    char *buf);

static ssize_t f75308_set_fan_mode(struct device *dev,
				   struct device_attribute *devattr,
				   const char *buf, size_t count);

static ssize_t f75308_show_fan_5_seg(struct device *dev,
				     struct device_attribute *devattr,
				     char *buf);

static ssize_t f75308_set_fan_5_seg(struct device *dev,
				    struct device_attribute *devattr,
				    const char *buf, size_t count);

static ssize_t f75308_show_fan_map(struct device *dev,
				   struct device_attribute *devattr, char *buf);

static ssize_t f75308_set_fan_map(struct device *dev,
				  struct device_attribute *devattr,
				  const char *buf, size_t count);

static SENSOR_DEVICE_ATTR(in0_input, 0444, f75308_show_in, NULL, 0);
static SENSOR_DEVICE_ATTR(in1_input, 0444, f75308_show_in, NULL, 1);
static SENSOR_DEVICE_ATTR(in2_input, 0444, f75308_show_in, NULL, 2);
static SENSOR_DEVICE_ATTR(in3_input, 0444, f75308_show_in, NULL, 3);
static SENSOR_DEVICE_ATTR(in4_input, 0444, f75308_show_in, NULL, 4);
static SENSOR_DEVICE_ATTR(in5_input, 0444, f75308_show_in, NULL, 5);
static SENSOR_DEVICE_ATTR(in6_input, 0444, f75308_show_in, NULL, 6);
static SENSOR_DEVICE_ATTR(in7_input, 0444, f75308_show_in, NULL, 7);
static SENSOR_DEVICE_ATTR(in8_input, 0444, f75308_show_in, NULL, 8);
static SENSOR_DEVICE_ATTR(in9_input, 0444, f75308_show_in, NULL, 9);
static SENSOR_DEVICE_ATTR(in10_input, 0444, f75308_show_in, NULL, 10);
static SENSOR_DEVICE_ATTR(in11_input, 0444, f75308_show_in, NULL, 11);
static SENSOR_DEVICE_ATTR(in12_input, 0444, f75308_show_in, NULL, 12);
static SENSOR_DEVICE_ATTR(in13_input, 0444, f75308_show_in, NULL, 13);
static SENSOR_DEVICE_ATTR(in14_input, 0444, f75308_show_in, NULL, 14);

static SENSOR_DEVICE_ATTR(temp_local_input, 0444, f75308_show_temp, NULL, 0);
static SENSOR_DEVICE_ATTR(temp1_input, 0444, f75308_show_temp, NULL, 1);
static SENSOR_DEVICE_ATTR(temp2_input, 0444, f75308_show_temp, NULL, 2);
static SENSOR_DEVICE_ATTR(temp3_input, 0444, f75308_show_temp, NULL, 3);
static SENSOR_DEVICE_ATTR(temp4_input, 0444, f75308_show_temp, NULL, 4);
static SENSOR_DEVICE_ATTR(temp5_input, 0444, f75308_show_temp, NULL, 5);
static SENSOR_DEVICE_ATTR(temp6_input, 0444, f75308_show_temp, NULL, 6);

static SENSOR_DEVICE_ATTR(fan1_input, 0444, f75308_show_fan, NULL, 0);
static SENSOR_DEVICE_ATTR(fan2_input, 0444, f75308_show_fan, NULL, 1);
static SENSOR_DEVICE_ATTR(fan3_input, 0444, f75308_show_fan, NULL, 2);
static SENSOR_DEVICE_ATTR(fan4_input, 0444, f75308_show_fan, NULL, 3);
static SENSOR_DEVICE_ATTR(fan5_input, 0444, f75308_show_fan, NULL, 4);
static SENSOR_DEVICE_ATTR(fan6_input, 0444, f75308_show_fan, NULL, 5);
static SENSOR_DEVICE_ATTR(fan7_input, 0444, f75308_show_fan, NULL, 6);
static SENSOR_DEVICE_ATTR(fan8_input, 0444, f75308_show_fan, NULL, 7);
static SENSOR_DEVICE_ATTR(fan9_input, 0444, f75308_show_fan, NULL, 8);
static SENSOR_DEVICE_ATTR(fan10_input, 0444, f75308_show_fan, NULL, 9);
static SENSOR_DEVICE_ATTR(fan11_input, 0444, f75308_show_fan, NULL, 10);
static SENSOR_DEVICE_ATTR(fan12_input, 0444, f75308_show_fan, NULL, 11);
static SENSOR_DEVICE_ATTR(fan13_input, 0444, f75308_show_fan, NULL, 12);
static SENSOR_DEVICE_ATTR(fan14_input, 0444, f75308_show_fan, NULL, 13);

static SENSOR_DEVICE_ATTR(pwm1, 0644, f75308_show_pwm, f75308_set_pwm, 0);
static SENSOR_DEVICE_ATTR(pwm2, 0644, f75308_show_pwm, f75308_set_pwm, 1);
static SENSOR_DEVICE_ATTR(pwm3, 0644, f75308_show_pwm, f75308_set_pwm, 2);
static SENSOR_DEVICE_ATTR(pwm4, 0644, f75308_show_pwm, f75308_set_pwm, 3);
static SENSOR_DEVICE_ATTR(pwm5, 0644, f75308_show_pwm, f75308_set_pwm, 4);
static SENSOR_DEVICE_ATTR(pwm6, 0644, f75308_show_pwm, f75308_set_pwm, 5);
static SENSOR_DEVICE_ATTR(pwm7, 0644, f75308_show_pwm, f75308_set_pwm, 6);
static SENSOR_DEVICE_ATTR(pwm8, 0644, f75308_show_pwm, f75308_set_pwm, 7);
static SENSOR_DEVICE_ATTR(pwm9, 0644, f75308_show_pwm, f75308_set_pwm, 8);
static SENSOR_DEVICE_ATTR(pwm10, 0644, f75308_show_pwm, f75308_set_pwm, 9);
static SENSOR_DEVICE_ATTR(pwm11, 0644, f75308_show_pwm, f75308_set_pwm, 10);

static SENSOR_DEVICE_ATTR(fan1_type, 0644, f75308_show_fan_type,
			  f75308_set_fan_type, 0);
static SENSOR_DEVICE_ATTR(fan2_type, 0644, f75308_show_fan_type,
			  f75308_set_fan_type, 1);
static SENSOR_DEVICE_ATTR(fan3_type, 0644, f75308_show_fan_type,
			  f75308_set_fan_type, 2);
static SENSOR_DEVICE_ATTR(fan4_type, 0644, f75308_show_fan_type,
			  f75308_set_fan_type, 3);
static SENSOR_DEVICE_ATTR(fan5_type, 0644, f75308_show_fan_type,
			  f75308_set_fan_type, 4);
static SENSOR_DEVICE_ATTR(fan6_type, 0644, f75308_show_fan_type,
			  f75308_set_fan_type, 5);
static SENSOR_DEVICE_ATTR(fan7_type, 0644, f75308_show_fan_type,
			  f75308_set_fan_type, 6);
static SENSOR_DEVICE_ATTR(fan8_type, 0644, f75308_show_fan_type,
			  f75308_set_fan_type, 7);
static SENSOR_DEVICE_ATTR(fan9_type, 0644, f75308_show_fan_type,
			  f75308_set_fan_type, 8);
static SENSOR_DEVICE_ATTR(fan10_type, 0644, f75308_show_fan_type,
			  f75308_set_fan_type, 9);
static SENSOR_DEVICE_ATTR(fan11_type, 0644, f75308_show_fan_type,
			  f75308_set_fan_type, 10);

static SENSOR_DEVICE_ATTR(fan1_mode, 0644, f75308_show_fan_mode,
			  f75308_set_fan_mode, 0);
static SENSOR_DEVICE_ATTR(fan2_mode, 0644, f75308_show_fan_mode,
			  f75308_set_fan_mode, 1);
static SENSOR_DEVICE_ATTR(fan3_mode, 0644, f75308_show_fan_mode,
			  f75308_set_fan_mode, 2);
static SENSOR_DEVICE_ATTR(fan4_mode, 0644, f75308_show_fan_mode,
			  f75308_set_fan_mode, 3);
static SENSOR_DEVICE_ATTR(fan5_mode, 0644, f75308_show_fan_mode,
			  f75308_set_fan_mode, 4);
static SENSOR_DEVICE_ATTR(fan6_mode, 0644, f75308_show_fan_mode,
			  f75308_set_fan_mode, 5);
static SENSOR_DEVICE_ATTR(fan7_mode, 0644, f75308_show_fan_mode,
			  f75308_set_fan_mode, 6);
static SENSOR_DEVICE_ATTR(fan8_mode, 0644, f75308_show_fan_mode,
			  f75308_set_fan_mode, 7);
static SENSOR_DEVICE_ATTR(fan9_mode, 0644, f75308_show_fan_mode,
			  f75308_set_fan_mode, 8);
static SENSOR_DEVICE_ATTR(fan10_mode, 0644, f75308_show_fan_mode,
			  f75308_set_fan_mode, 9);
static SENSOR_DEVICE_ATTR(fan11_mode, 0644, f75308_show_fan_mode,
			  f75308_set_fan_mode, 10);

static SENSOR_DEVICE_ATTR(fan1_5_seg, 0644, f75308_show_fan_5_seg,
			  f75308_set_fan_5_seg, 0);
static SENSOR_DEVICE_ATTR(fan2_5_seg, 0644, f75308_show_fan_5_seg,
			  f75308_set_fan_5_seg, 1);
static SENSOR_DEVICE_ATTR(fan3_5_seg, 0644, f75308_show_fan_5_seg,
			  f75308_set_fan_5_seg, 2);
static SENSOR_DEVICE_ATTR(fan4_5_seg, 0644, f75308_show_fan_5_seg,
			  f75308_set_fan_5_seg, 3);
static SENSOR_DEVICE_ATTR(fan5_5_seg, 0644, f75308_show_fan_5_seg,
			  f75308_set_fan_5_seg, 4);
static SENSOR_DEVICE_ATTR(fan6_5_seg, 0644, f75308_show_fan_5_seg,
			  f75308_set_fan_5_seg, 5);
static SENSOR_DEVICE_ATTR(fan7_5_seg, 0644, f75308_show_fan_5_seg,
			  f75308_set_fan_5_seg, 6);
static SENSOR_DEVICE_ATTR(fan8_5_seg, 0644, f75308_show_fan_5_seg,
			  f75308_set_fan_5_seg, 7);
static SENSOR_DEVICE_ATTR(fan9_5_seg, 0644, f75308_show_fan_5_seg,
			  f75308_set_fan_5_seg, 8);
static SENSOR_DEVICE_ATTR(fan10_5_seg, 0644, f75308_show_fan_5_seg,
			  f75308_set_fan_5_seg, 9);
static SENSOR_DEVICE_ATTR(fan11_5_seg, 0644, f75308_show_fan_5_seg,
			  f75308_set_fan_5_seg, 10);

static SENSOR_DEVICE_ATTR(fan1_map, 0644, f75308_show_fan_map,
			  f75308_set_fan_map, 0);
static SENSOR_DEVICE_ATTR(fan2_map, 0644, f75308_show_fan_map,
			  f75308_set_fan_map, 1);
static SENSOR_DEVICE_ATTR(fan3_map, 0644, f75308_show_fan_map,
			  f75308_set_fan_map, 2);
static SENSOR_DEVICE_ATTR(fan4_map, 0644, f75308_show_fan_map,
			  f75308_set_fan_map, 3);
static SENSOR_DEVICE_ATTR(fan5_map, 0644, f75308_show_fan_map,
			  f75308_set_fan_map, 4);
static SENSOR_DEVICE_ATTR(fan6_map, 0644, f75308_show_fan_map,
			  f75308_set_fan_map, 5);
static SENSOR_DEVICE_ATTR(fan7_map, 0644, f75308_show_fan_map,
			  f75308_set_fan_map, 6);
static SENSOR_DEVICE_ATTR(fan8_map, 0644, f75308_show_fan_map,
			  f75308_set_fan_map, 7);
static SENSOR_DEVICE_ATTR(fan9_map, 0644, f75308_show_fan_map,
			  f75308_set_fan_map, 8);
static SENSOR_DEVICE_ATTR(fan10_map, 0644, f75308_show_fan_map,
			  f75308_set_fan_map, 9);
static SENSOR_DEVICE_ATTR(fan11_map, 0644, f75308_show_fan_map,
			  f75308_set_fan_map, 10);

static struct attribute *f75308a_28_attributes[] = {
	&sensor_dev_attr_temp_local_input.dev_attr.attr,
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_temp2_input.dev_attr.attr,
	&sensor_dev_attr_temp3_input.dev_attr.attr,

	&sensor_dev_attr_fan1_input.dev_attr.attr,
	&sensor_dev_attr_fan2_input.dev_attr.attr,
	&sensor_dev_attr_fan3_input.dev_attr.attr,
	&sensor_dev_attr_fan4_input.dev_attr.attr,

	&sensor_dev_attr_in0_input.dev_attr.attr,
	&sensor_dev_attr_in1_input.dev_attr.attr,
	&sensor_dev_attr_in2_input.dev_attr.attr,
	&sensor_dev_attr_in3_input.dev_attr.attr,
	&sensor_dev_attr_in4_input.dev_attr.attr,
	&sensor_dev_attr_in5_input.dev_attr.attr,

	&sensor_dev_attr_pwm1.dev_attr.attr,
	&sensor_dev_attr_pwm2.dev_attr.attr,
	&sensor_dev_attr_pwm3.dev_attr.attr,
	&sensor_dev_attr_pwm4.dev_attr.attr,

	&sensor_dev_attr_fan1_type.dev_attr.attr,
	&sensor_dev_attr_fan2_type.dev_attr.attr,
	&sensor_dev_attr_fan3_type.dev_attr.attr,
	&sensor_dev_attr_fan4_type.dev_attr.attr,

	&sensor_dev_attr_fan1_mode.dev_attr.attr,
	&sensor_dev_attr_fan2_mode.dev_attr.attr,
	&sensor_dev_attr_fan3_mode.dev_attr.attr,
	&sensor_dev_attr_fan4_mode.dev_attr.attr,

	&sensor_dev_attr_fan1_map.dev_attr.attr,
	&sensor_dev_attr_fan2_map.dev_attr.attr,
	&sensor_dev_attr_fan3_map.dev_attr.attr,
	&sensor_dev_attr_fan4_map.dev_attr.attr,

	&sensor_dev_attr_fan1_5_seg.dev_attr.attr,
	&sensor_dev_attr_fan2_5_seg.dev_attr.attr,
	&sensor_dev_attr_fan3_5_seg.dev_attr.attr,
	&sensor_dev_attr_fan4_5_seg.dev_attr.attr,

	NULL
};

static struct attribute *f75308b_48_attributes[] = {
	&sensor_dev_attr_temp_local_input.dev_attr.attr,
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_temp2_input.dev_attr.attr,
	&sensor_dev_attr_temp3_input.dev_attr.attr,
	&sensor_dev_attr_temp4_input.dev_attr.attr,
	&sensor_dev_attr_temp5_input.dev_attr.attr,

	&sensor_dev_attr_fan1_input.dev_attr.attr,
	&sensor_dev_attr_fan2_input.dev_attr.attr,
	&sensor_dev_attr_fan3_input.dev_attr.attr,
	&sensor_dev_attr_fan4_input.dev_attr.attr,
	&sensor_dev_attr_fan5_input.dev_attr.attr,
	&sensor_dev_attr_fan6_input.dev_attr.attr,
	&sensor_dev_attr_fan7_input.dev_attr.attr,
	&sensor_dev_attr_fan8_input.dev_attr.attr,
	&sensor_dev_attr_fan9_input.dev_attr.attr,

	&sensor_dev_attr_in0_input.dev_attr.attr,
	&sensor_dev_attr_in1_input.dev_attr.attr,
	&sensor_dev_attr_in2_input.dev_attr.attr,
	&sensor_dev_attr_in3_input.dev_attr.attr,
	&sensor_dev_attr_in4_input.dev_attr.attr,
	&sensor_dev_attr_in5_input.dev_attr.attr,
	&sensor_dev_attr_in6_input.dev_attr.attr,
	&sensor_dev_attr_in7_input.dev_attr.attr,
	&sensor_dev_attr_in8_input.dev_attr.attr,
	&sensor_dev_attr_in9_input.dev_attr.attr,
	&sensor_dev_attr_in10_input.dev_attr.attr,

	&sensor_dev_attr_pwm1.dev_attr.attr,
	&sensor_dev_attr_pwm2.dev_attr.attr,
	&sensor_dev_attr_pwm3.dev_attr.attr,
	&sensor_dev_attr_pwm4.dev_attr.attr,
	&sensor_dev_attr_pwm5.dev_attr.attr,
	&sensor_dev_attr_pwm6.dev_attr.attr,
	&sensor_dev_attr_pwm7.dev_attr.attr,

	&sensor_dev_attr_fan1_type.dev_attr.attr,
	&sensor_dev_attr_fan2_type.dev_attr.attr,
	&sensor_dev_attr_fan3_type.dev_attr.attr,
	&sensor_dev_attr_fan4_type.dev_attr.attr,
	&sensor_dev_attr_fan5_type.dev_attr.attr,
	&sensor_dev_attr_fan6_type.dev_attr.attr,
	&sensor_dev_attr_fan7_type.dev_attr.attr,

	&sensor_dev_attr_fan1_mode.dev_attr.attr,
	&sensor_dev_attr_fan2_mode.dev_attr.attr,
	&sensor_dev_attr_fan3_mode.dev_attr.attr,
	&sensor_dev_attr_fan4_mode.dev_attr.attr,
	&sensor_dev_attr_fan5_mode.dev_attr.attr,
	&sensor_dev_attr_fan6_mode.dev_attr.attr,
	&sensor_dev_attr_fan7_mode.dev_attr.attr,

	&sensor_dev_attr_fan1_map.dev_attr.attr,
	&sensor_dev_attr_fan2_map.dev_attr.attr,
	&sensor_dev_attr_fan3_map.dev_attr.attr,
	&sensor_dev_attr_fan4_map.dev_attr.attr,
	&sensor_dev_attr_fan5_map.dev_attr.attr,
	&sensor_dev_attr_fan6_map.dev_attr.attr,
	&sensor_dev_attr_fan7_map.dev_attr.attr,

	&sensor_dev_attr_fan1_5_seg.dev_attr.attr,
	&sensor_dev_attr_fan2_5_seg.dev_attr.attr,
	&sensor_dev_attr_fan3_5_seg.dev_attr.attr,
	&sensor_dev_attr_fan4_5_seg.dev_attr.attr,
	&sensor_dev_attr_fan5_5_seg.dev_attr.attr,
	&sensor_dev_attr_fan6_5_seg.dev_attr.attr,
	&sensor_dev_attr_fan7_5_seg.dev_attr.attr,

	NULL
};

static struct attribute *f75308c_64_attributes[] = {
	&sensor_dev_attr_temp_local_input.dev_attr.attr,
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_temp2_input.dev_attr.attr,
	&sensor_dev_attr_temp3_input.dev_attr.attr,
	&sensor_dev_attr_temp4_input.dev_attr.attr,
	&sensor_dev_attr_temp5_input.dev_attr.attr,
	&sensor_dev_attr_temp6_input.dev_attr.attr,

	&sensor_dev_attr_fan1_input.dev_attr.attr,
	&sensor_dev_attr_fan2_input.dev_attr.attr,
	&sensor_dev_attr_fan3_input.dev_attr.attr,
	&sensor_dev_attr_fan4_input.dev_attr.attr,
	&sensor_dev_attr_fan5_input.dev_attr.attr,
	&sensor_dev_attr_fan6_input.dev_attr.attr,
	&sensor_dev_attr_fan7_input.dev_attr.attr,
	&sensor_dev_attr_fan8_input.dev_attr.attr,
	&sensor_dev_attr_fan9_input.dev_attr.attr,
	&sensor_dev_attr_fan10_input.dev_attr.attr,
	&sensor_dev_attr_fan11_input.dev_attr.attr,
	&sensor_dev_attr_fan12_input.dev_attr.attr,
	&sensor_dev_attr_fan13_input.dev_attr.attr,
	&sensor_dev_attr_fan14_input.dev_attr.attr,

	&sensor_dev_attr_in0_input.dev_attr.attr,
	&sensor_dev_attr_in1_input.dev_attr.attr,
	&sensor_dev_attr_in2_input.dev_attr.attr,
	&sensor_dev_attr_in3_input.dev_attr.attr,
	&sensor_dev_attr_in4_input.dev_attr.attr,
	&sensor_dev_attr_in5_input.dev_attr.attr,
	&sensor_dev_attr_in6_input.dev_attr.attr,
	&sensor_dev_attr_in7_input.dev_attr.attr,
	&sensor_dev_attr_in8_input.dev_attr.attr,
	&sensor_dev_attr_in9_input.dev_attr.attr,
	&sensor_dev_attr_in10_input.dev_attr.attr,
	&sensor_dev_attr_in11_input.dev_attr.attr,
	&sensor_dev_attr_in12_input.dev_attr.attr,
	&sensor_dev_attr_in13_input.dev_attr.attr,
	&sensor_dev_attr_in14_input.dev_attr.attr,

	&sensor_dev_attr_pwm1.dev_attr.attr,
	&sensor_dev_attr_pwm2.dev_attr.attr,
	&sensor_dev_attr_pwm3.dev_attr.attr,
	&sensor_dev_attr_pwm4.dev_attr.attr,
	&sensor_dev_attr_pwm5.dev_attr.attr,
	&sensor_dev_attr_pwm6.dev_attr.attr,
	&sensor_dev_attr_pwm7.dev_attr.attr,
	&sensor_dev_attr_pwm8.dev_attr.attr,
	&sensor_dev_attr_pwm9.dev_attr.attr,
	&sensor_dev_attr_pwm10.dev_attr.attr,
	&sensor_dev_attr_pwm11.dev_attr.attr,

	&sensor_dev_attr_fan1_type.dev_attr.attr,
	&sensor_dev_attr_fan2_type.dev_attr.attr,
	&sensor_dev_attr_fan3_type.dev_attr.attr,
	&sensor_dev_attr_fan4_type.dev_attr.attr,
	&sensor_dev_attr_fan5_type.dev_attr.attr,
	&sensor_dev_attr_fan6_type.dev_attr.attr,
	&sensor_dev_attr_fan7_type.dev_attr.attr,
	&sensor_dev_attr_fan8_type.dev_attr.attr,
	&sensor_dev_attr_fan9_type.dev_attr.attr,
	&sensor_dev_attr_fan10_type.dev_attr.attr,
	&sensor_dev_attr_fan11_type.dev_attr.attr,

	&sensor_dev_attr_fan1_mode.dev_attr.attr,
	&sensor_dev_attr_fan2_mode.dev_attr.attr,
	&sensor_dev_attr_fan3_mode.dev_attr.attr,
	&sensor_dev_attr_fan4_mode.dev_attr.attr,
	&sensor_dev_attr_fan5_mode.dev_attr.attr,
	&sensor_dev_attr_fan6_mode.dev_attr.attr,
	&sensor_dev_attr_fan7_mode.dev_attr.attr,
	&sensor_dev_attr_fan8_mode.dev_attr.attr,
	&sensor_dev_attr_fan9_mode.dev_attr.attr,
	&sensor_dev_attr_fan10_mode.dev_attr.attr,
	&sensor_dev_attr_fan11_mode.dev_attr.attr,

	&sensor_dev_attr_fan1_map.dev_attr.attr,
	&sensor_dev_attr_fan2_map.dev_attr.attr,
	&sensor_dev_attr_fan3_map.dev_attr.attr,
	&sensor_dev_attr_fan4_map.dev_attr.attr,
	&sensor_dev_attr_fan5_map.dev_attr.attr,
	&sensor_dev_attr_fan6_map.dev_attr.attr,
	&sensor_dev_attr_fan7_map.dev_attr.attr,
	&sensor_dev_attr_fan8_map.dev_attr.attr,
	&sensor_dev_attr_fan9_map.dev_attr.attr,
	&sensor_dev_attr_fan10_map.dev_attr.attr,
	&sensor_dev_attr_fan11_map.dev_attr.attr,

	&sensor_dev_attr_fan1_5_seg.dev_attr.attr,
	&sensor_dev_attr_fan2_5_seg.dev_attr.attr,
	&sensor_dev_attr_fan3_5_seg.dev_attr.attr,
	&sensor_dev_attr_fan4_5_seg.dev_attr.attr,
	&sensor_dev_attr_fan5_5_seg.dev_attr.attr,
	&sensor_dev_attr_fan6_5_seg.dev_attr.attr,
	&sensor_dev_attr_fan7_5_seg.dev_attr.attr,
	&sensor_dev_attr_fan8_5_seg.dev_attr.attr,
	&sensor_dev_attr_fan9_5_seg.dev_attr.attr,
	&sensor_dev_attr_fan10_5_seg.dev_attr.attr,
	&sensor_dev_attr_fan11_5_seg.dev_attr.attr,

	NULL
};

static const struct attribute_group f75308a_28_group = {
	.attrs = f75308a_28_attributes,
};

static const struct attribute_group f75308b_48_group = {
	.attrs = f75308b_48_attributes,
};

static const struct attribute_group f75308c_64_group = {
	.attrs = f75308c_64_attributes,
};

static const struct attribute_group *f75308a_28_groups[] = {
	&f75308a_28_group,
	NULL,
};

static const struct attribute_group *f75308b_48_groups[] = {
	&f75308b_48_group,
	NULL,
};

static const struct attribute_group *f75308c_64_groups[] = {
	&f75308c_64_group,
	NULL,
};

static const struct attribute_group **f75308_groups[] = {
	f75308a_28_groups,
	f75308b_48_groups,
	f75308c_64_groups,
};

static int f75308_read8(struct i2c_client *client, u8 reg)
{
	return i2c_smbus_read_byte_data(client, reg);
}

static int f75308_write8(struct i2c_client *client, u8 reg, u8 value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}

static int f75308_write_mask8(struct i2c_client *client, u8 reg, u8 mask,
			      u8 value)
{
	int status;

	status = f75308_read8(client, reg);
	if (status < 0)
		return status;

	status = (status & ~mask) | (value & mask);

	return f75308_write8(client, reg, (u8)status);
}

static inline u16 f75308_read16(struct i2c_client *client, u8 reg)
{
	return (i2c_smbus_read_byte_data(client, reg) << 8) |
	       i2c_smbus_read_byte_data(client, reg + 1);
}

static ssize_t f75308_show_temp(struct device *dev,
				struct device_attribute *devattr, char *buf)
{
	struct f75308_priv *priv = dev_get_drvdata(dev);
	struct i2c_client *client = priv->client;
	int nr = to_sensor_dev_attr_2(devattr)->index;
	int status, deci, frac, data;

	mutex_lock(&priv->locker);

	status = f75308_write8(client, F75308_REG_BANK, 0);
	if (status)
		goto err;

	status = f75308_read8(client, F75308_REG_TEMP_READ(nr) + 0);
	if (status < 0)
		goto err;
	deci = status;

	status = f75308_read8(client, F75308_REG_TEMP_READ(nr) + 1);
	if (status < 0)
		goto err;
	frac = status;

	data = deci * 1000 + (frac >> 5) * 125;
	mutex_unlock(&priv->locker);

	dev_dbg(dev, "%s: nr:%d deci:%d frac:%d, data:%d\n", __func__, nr, deci,
		frac, data);
	return sprintf(buf, "%d\n", data);

err:
	mutex_unlock(&priv->locker);
	return status;
}

static ssize_t f75308_show_in(struct device *dev,
			      struct device_attribute *devattr, char *buf)
{
	struct f75308_priv *priv = dev_get_drvdata(dev);
	struct i2c_client *client = priv->client;
	int nr = to_sensor_dev_attr_2(devattr)->index;
	int status = 0, data;

	mutex_lock(&priv->locker);

	status = f75308_write8(client, F75308_REG_BANK, 0);
	if (status)
		goto err;

	data = f75308_read8(client, F75308_REG_VOLT(nr));
	if (data < 0) {
		status = data;
		goto err;
	}

	data *= 8;
	mutex_unlock(&priv->locker);

	return sprintf(buf, "%d\n", data);

err:
	mutex_unlock(&priv->locker);
	return status;
}

static ssize_t f75308_show_fan(struct device *dev,
			       struct device_attribute *devattr, char *buf)
{
	struct f75308_priv *priv = dev_get_drvdata(dev);
	struct i2c_client *client = priv->client;
	int nr = to_sensor_dev_attr_2(devattr)->index;
	int status, lsb, msb, data;

	mutex_lock(&priv->locker);

	status = f75308_write8(client, F75308_REG_BANK, 0);
	if (status)
		goto err;

	status = f75308_read8(client, F75308_REG_FAN_READ(nr) + 0);
	if (status < 0)
		goto err;
	msb = status;

	status = f75308_read8(client, F75308_REG_FAN_READ(nr) + 1);
	if (status < 0)
		goto err;
	lsb = status;

	dev_dbg(dev, "%s: nr: %d, msb: %x, lsb: %x\n", __func__, nr, msb, lsb);

	if (msb == 0x1f && lsb == 0xff)
		data = 0;
	else if (msb || lsb)
		data = 1500000 / (msb * 256 + lsb);
	else
		data = 0;

	mutex_unlock(&priv->locker);

	return sprintf(buf, "%d\n", data);

err:
	mutex_unlock(&priv->locker);
	return status;
}

static ssize_t f75308_show_pwm(struct device *dev,
			       struct device_attribute *devattr, char *buf)
{
	struct f75308_priv *priv = dev_get_drvdata(dev);
	struct i2c_client *client = priv->client;
	int nr = to_sensor_dev_attr_2(devattr)->index;
	int status = 0, data;

	mutex_lock(&priv->locker);

	status = f75308_write8(client, F75308_REG_BANK, 0);
	if (status)
		goto done;

	data = f75308_read8(client, 0xa0 + nr);
	if (data < 0) {
		status = data;
		goto done;
	}

	status = sprintf(buf, "%d\n", data);

done:
	mutex_unlock(&priv->locker);
	return status;
}

static ssize_t f75308_set_pwm(struct device *dev,
			      struct device_attribute *devattr, const char *buf,
			      size_t count)
{
	struct f75308_priv *priv = dev_get_drvdata(dev);
	struct i2c_client *client = priv->client;
	int nr = to_sensor_dev_attr_2(devattr)->index;
	int status = 0, fan_mode, pwm;

	status = kstrtoint(buf, 0, &pwm);
	if (status)
		return status;

	pwm = clamp_val(pwm, 0, 255);

	mutex_lock(&priv->locker);

	/* check whether fan mode is in manual duty mode */
	status = f75308_write8(client, F75308_REG_BANK, 4);
	if (status)
		goto done;

	fan_mode = f75308_read8(client, 0x74 + nr / 4);
	if (fan_mode < 0) {
		status = fan_mode;
		goto done;
	}

	fan_mode = fan_mode >> ((nr % 4) * 2);
	fan_mode = fan_mode & 0x03;
	if (fan_mode != 0x03) {
		dev_err(dev, "%s: Only manual_duty mode supports PWM write!\n",
			__func__);
		status = -EOPNOTSUPP;
		goto done;
	}

	status = f75308_write8(client, F75308_REG_BANK, 5);
	if (status)
		goto done;

	status = f75308_write8(client, 0x11 + nr * 0x10, pwm);
done:
	mutex_unlock(&priv->locker);
	return status ? status : count;
}

static ssize_t f75308_show_fan_type(struct device *dev,
				    struct device_attribute *devattr, char *buf)
{
	struct f75308_priv *priv = dev_get_drvdata(dev);
	struct i2c_client *client = priv->client;
	int nr = to_sensor_dev_attr_2(devattr)->index;
	int status = 0, data;

	mutex_lock(&priv->locker);

	status = f75308_write8(client, F75308_REG_BANK, 4);
	if (status)
		goto done;

	data = f75308_read8(client, 0x70 + nr / 4);
	if (data < 0) {
		status = data;
		goto done;
	}

	data = data >> ((nr % 4) * 2);
	data = data & 0x03;

	switch (data) {
	case 0:
		status = sprintf(buf, "pwm\n");
		break;
	case 1:
		status = sprintf(buf, "linear\n");
		break;
	case 2:
		status = sprintf(buf, "pwm_opendrain\n");
		break;

	default:
	case 3:
		status = sprintf(buf, "%s: invalid data: nr: %d, data: %xh\n",
				 __func__, nr, data);
		break;
	}

done:
	mutex_unlock(&priv->locker);
	return status;
}

static int __f75308_set_fan_type(struct i2c_client *client, int nr,
				 const char *buf)
{
	int status, data, shift;

	status = f75308_write8(client, F75308_REG_BANK, 4);
	if (status)
		return status;

	if (!strncmp(buf, "pwm_opendrain", strlen("pwm_opendrain"))) {
		data = 0x02;
	} else if (!strncmp(buf, "linear", strlen("linear"))) {
		data = 0x01;
	} else if (!strncmp(buf, "pwm", strlen("pwm"))) {
		data = 0x00;
	} else {
		dev_err(&client->dev,
			"%s: support only pwm/linear/pwm_opendrain\n",
			__func__);
		return -EINVAL;
	}

	shift = ((nr % 4) * 2);

	return f75308_write_mask8(client, 0x70 + nr / 4, 3 << shift,
				  data << shift);
}

static ssize_t f75308_set_fan_type(struct device *dev,
				   struct device_attribute *devattr,
				   const char *buf, size_t count)
{
	struct f75308_priv *priv = dev_get_drvdata(dev);
	struct i2c_client *client = priv->client;
	int nr = to_sensor_dev_attr_2(devattr)->index;
	int status = 0;

	mutex_lock(&priv->locker);
	status = __f75308_set_fan_type(client, nr, buf);
	mutex_unlock(&priv->locker);

	if (status)
		return status;

	return count;
}

static ssize_t f75308_show_fan_mode(struct device *dev,
				    struct device_attribute *devattr, char *buf)
{
	struct f75308_priv *priv = dev_get_drvdata(dev);
	struct i2c_client *client = priv->client;
	int nr = to_sensor_dev_attr_2(devattr)->index;
	int status = 0, data;

	mutex_lock(&priv->locker);

	status = f75308_write8(client, F75308_REG_BANK, 4);
	if (status)
		goto done;

	data = f75308_read8(client, 0x74 + nr / 4);
	if (data < 0) {
		status = data;
		goto done;
	}

	data = data >> ((nr % 4) * 2);
	data = data & 0x03;

	switch (data) {
	case 0:
		status = sprintf(buf, "auto_rpm\n");
		break;
	case 1:
		status = sprintf(buf, "auto_duty\n");
		break;
	case 2:
		status = sprintf(buf, "manual_rpm\n");
		break;
	case 3:
		status = sprintf(buf, "manual_duty\n");
		break;
	default:
		status = sprintf(buf, "%s: invalid data: nr: %d, data: %xh\n",
				 __func__, nr, data);
		break;
	}

done:
	mutex_unlock(&priv->locker);
	return status;
}

static int __f75308_set_fan_mode(struct i2c_client *client, int nr,
				 const char *buf)
{
	int status, data, shift;

	status = f75308_write8(client, F75308_REG_BANK, 4);
	if (status)
		return status;

	if (!strncmp(buf, "manual_rpm", strlen("manual_rpm"))) {
		data = 0x02;
	} else if (!strncmp(buf, "manual_duty", strlen("manual_duty"))) {
		data = 0x03;
	} else if (!strncmp(buf, "auto_rpm", strlen("auto_rpm"))) {
		data = 0x00;
	} else if (!strncmp(buf, "auto_duty", strlen("auto_duty"))) {
		data = 0x01;
	} else {
		dev_err(&client->dev,
			"%s: support only manual_rpm/manual_duty/auto_rpm/auto_duty\n",
			__func__);

		return -EINVAL;
	}

	shift = ((nr % 4) * 2);

	return f75308_write_mask8(client, 0x74 + nr / 4, 3 << shift,
				  data << shift);
}

static ssize_t f75308_set_fan_mode(struct device *dev,
				   struct device_attribute *devattr,
				   const char *buf, size_t count)
{
	struct f75308_priv *priv = dev_get_drvdata(dev);
	struct i2c_client *client = priv->client;
	int nr = to_sensor_dev_attr_2(devattr)->index;
	int status;

	mutex_lock(&priv->locker);
	status = __f75308_set_fan_mode(client, nr, buf);
	mutex_unlock(&priv->locker);

	if (status)
		return status;

	return count;
}

static ssize_t f75308_show_fan_5_seg(struct device *dev,
				     struct device_attribute *devattr,
				     char *buf)
{
	struct f75308_priv *priv = dev_get_drvdata(dev);
	struct i2c_client *client = priv->client;
	int nr = to_sensor_dev_attr_2(devattr)->index;
	int status = 0, data[5], i, tmp;

	mutex_lock(&priv->locker);

	status = f75308_write8(client, F75308_REG_BANK, 5);
	if (status)
		goto done;

	for (i = 0; i < 5; ++i) {
		data[i] = f75308_read8(client, 0x18 + nr * 0x10 + i);
		if (data[i] < 0) {
			status = data[i];
			goto done;
		}

		tmp = data[i] * 100 / 255;
		dev_dbg(dev, "%s: reg: %x, data: %x, %d%%\n", __func__,
			0x18 + nr * 0x10 + i, data[i], tmp);
		data[i] = tmp;
	}

	status = sprintf(buf, "%d%% %d%% %d%% %d%% %d%%\n", data[0], data[1],
			 data[2], data[3], data[4]);
done:
	mutex_unlock(&priv->locker);
	return status;
}

static int __f75308_set_fan_5_seg(struct i2c_client *client, int nr,
				  int data[5])
{
	int status, i, tmp;

	for (i = 0; i < 5; ++i) {
		if (data[i] > 100 || data[i] < 0)
			return -EINVAL;
	}

	status = f75308_write8(client, F75308_REG_BANK, 5);
	if (status)
		return status;

	for (i = 0; i < 5; ++i) {
		tmp = 255 * data[i] / 100;

		status = f75308_write8(client, 0x18 + nr * 0x10 + i, (u8)tmp);
		if (status)
			return status;

		dev_dbg(&client->dev, "%s: reg: %x, data: %x\n", __func__,
			0x18 + nr * 0x10 + i, tmp);
	}

	return 0;
}

static ssize_t f75308_set_fan_5_seg(struct device *dev,
				    struct device_attribute *devattr,
				    const char *buf, size_t count)
{
	struct f75308_priv *priv = dev_get_drvdata(dev);
	struct i2c_client *client = priv->client;
	int nr = to_sensor_dev_attr_2(devattr)->index;
	int status, data[5], i;
	u8 *p;

	mutex_lock(&priv->locker);

	for (i = 0; i < 5; ++i) {
		p = strsep((char **)&buf, " ");
		if (!p) {
			count = -EINVAL;
			goto done;
		}

		status = kstrtoint(p, 0, &data[i]);
		if (status) {
			count = status;
			goto done;
		}

		if (data[i] > 100 || data[i] < 0) {
			count = -EINVAL;
			goto done;
		}
	}

	status = __f75308_set_fan_5_seg(client, nr, data);
	if (status)
		count = status;

done:
	mutex_unlock(&priv->locker);
	return count;
}

static ssize_t f75308_show_fan_map(struct device *dev,
				   struct device_attribute *devattr, char *buf)
{
	struct f75308_priv *priv = dev_get_drvdata(dev);
	struct i2c_client *client = priv->client;
	int nr = to_sensor_dev_attr_2(devattr)->index;
	int status = 0, data;

	mutex_lock(&priv->locker);

	status = f75308_write8(client, F75308_REG_BANK, 4);
	if (status)
		goto done;

	data = f75308_read8(client, 0x50 + nr);
	if (data < 0) {
		status = data;
		goto done;
	}

	dev_dbg(dev, "%s: idx: %d, data: %x\n", __func__, nr, data);
	status = sprintf(buf, "%d\n", data);
done:
	mutex_unlock(&priv->locker);
	return status;
}

static ssize_t f75308_set_fan_map(struct device *dev,
				  struct device_attribute *devattr,
				  const char *buf, size_t count)
{
	struct f75308_priv *priv = dev_get_drvdata(dev);
	struct i2c_client *client = priv->client;
	int nr = to_sensor_dev_attr_2(devattr)->index;
	int status = 0, data;

	status = kstrtoint(buf, 0, &data);
	if (status) {
		count = status;
		goto done;
	}

	mutex_lock(&priv->locker);

	status = f75308_write8(client, F75308_REG_BANK, 4);
	if (status)
		goto done;

	status = f75308_write8(client, 0x50 + nr, data);
	if (status)
		goto done;

	status = count;
	dev_dbg(dev, "%s: idx: %d, data: %x\n", __func__, nr, data);
done:
	mutex_unlock(&priv->locker);

	return status;
}

static int f75308_get_devid(struct i2c_client *client, enum chip *chipid)
{
	u16 vendid, pid;
	int status;

	status = f75308_write8(client, F75308_REG_BANK, 0);
	if (status)
		return status;

	vendid = f75308_read16(client, DEVICE_VID_ADDR);
	pid = f75308_read16(client, DEVICE_PID_ADDR);
	if (vendid != DEVICE_VID)
		return -ENODEV;

	if (pid == DEVICE_PID_64PIN)
		*chipid = f75308c_64;
	else if (pid == DEVICE_PID_48PIN)
		*chipid = f75308b_48;
	else if (pid == DEVICE_PID_28PIN)
		*chipid = f75308a_28;
	else
		return -ENODEV;

	return 0;
}

/* Return 0 if detection is successful, -ENODEV otherwise */
static int f75308_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;
	enum chip chipid;
	const char *name;
	int status = 0;

	status = f75308_get_devid(client, &chipid);
	if (status)
		return status;

	if (chipid == f75308a_28)
		name = "F75308AR";
	else if (chipid == f75308b_48)
		name = "F75308BD";
	else if (chipid == f75308c_64)
		name = "F75308CU";
	else
		return -ENODEV;

	dev_info(&adapter->dev, "%s: found %s with addr %x on %s\n", __func__,
		 name, info->addr, adapter->name);
	strlcpy(info->type, name, I2C_NAME_SIZE);

	return 0;
}

static int f75308_init(struct i2c_client *client)
{
	struct f75308_priv *priv = dev_get_drvdata(&client->dev);
	int status, tmp;

	// check f75308a_28 mapping is default
	if (priv->chip_id == f75308a_28) {
		status = f75308_write8(client, F75308_REG_BANK, 4);
		if (status)
			goto err;

		tmp = f75308_read8(client, 0x53);
		if (tmp < 0)
			return tmp;

		if (tmp == 0x04) {
			// re-mapping FAN4 to T0
			status = f75308_write8(client, 0x53, 0);
			if (status)
				goto err;
		}

		status = f75308_write8(client, F75308_REG_BANK, 0);
		if (status)
			goto err;
	}

	return 0;

err:
	return status;
}

static int f75308_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device_node *child, *np = client->dev.of_node;
	struct property *prop;
	struct f75308_priv *priv;
	int status, seg5[5];
	const char *val_str;
	const __be32 *p;
	int val, reg_idx, i;

	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	mutex_init(&priv->locker);
	priv->client = client;
	dev_set_drvdata(&client->dev, priv);

	if (np)
		dev_dbg(&client->dev, "%s: np name: %s, full name: %s\n",
			__func__, np->name, np->full_name);

	status = f75308_get_devid(client, &priv->chip_id);
	if (status) {
		dev_err(&client->dev, "%s: f75308_get_devid error: %d\n",
			__func__, status);
		goto destroy_lock;
	}

	status = f75308_init(client);
	if (status) {
		dev_err(&client->dev, "%s: f75308_init error: %d\n", __func__,
			status);
		goto destroy_lock;
	}

	for_each_child_of_node(np, child) {
		dev_dbg(&client->dev, "%s: child name: %s, full name: %s\n",
			__func__, child->name, child->full_name);

		if (of_property_read_u32(child, "reg", &reg_idx)) {
			dev_err(&client->dev, "missing reg property of %pOFn\n",
				child);
			status = -EINVAL;
			goto put_child;

		} else {
			dev_dbg(&client->dev, "%s: reg_idx: %d\n", __func__,
				reg_idx);
		}

		if (of_property_read_string(child, "type", &val_str)) {
			dev_err(&client->dev, "read type failed or no type\n");
		} else {
			dev_dbg(&client->dev, "%s: type: %s\n", __func__,
				val_str);

			status =
				__f75308_set_fan_type(client, reg_idx, val_str);
			if (status)
				goto put_child;
		}

		if (of_property_read_string(child, "duty", &val_str)) {
			dev_err(&client->dev, "read duty failed or no duty\n");
		} else {
			dev_dbg(&client->dev, "%s: duty: %s\n", __func__,
				val_str);

			status =
				__f75308_set_fan_mode(client, reg_idx, val_str);
			if (status)
				goto put_child;
		}

		i = 0;
		of_property_for_each_u32(child, "5seg", prop, p, val) {
			dev_dbg(&client->dev, "%s: 5seg: i: %d, val: %d\n",
				__func__, i, val);
			seg5[i] = val;
			i++;
		}

		if (i == 5) {
			status = __f75308_set_fan_5_seg(client, reg_idx, seg5);
			if (status)
				goto put_child;
		}
	}

	priv->hwmon_dev = devm_hwmon_device_register_with_groups(
		&client->dev, DEVICE_NAME, priv, f75308_groups[priv->chip_id]);
	if (IS_ERR(priv->hwmon_dev)) {
		status = PTR_ERR(priv->hwmon_dev);
		goto put_child;
	}

	dev_info(&client->dev, "Finished f75308 probing\n");
	return 0;

put_child:
	of_node_put(child);
destroy_lock:
	mutex_destroy(&priv->locker);
	return status;
}

static int f75308_remove(struct i2c_client *client)
{
	struct f75308_priv *priv = dev_get_drvdata(&client->dev);

	mutex_destroy(&priv->locker);
	return 0;
}

static const unsigned short f75308_addr[] = {
	0x58 >> 1,	0x5A >> 1, 0x5C >> 1, 0x5E >> 1,
	0x98 >> 1,	0x9A >> 1, 0x9C >> 1, 0x9E >> 1,
	I2C_CLIENT_END,
};

static const struct i2c_device_id f75308_id[] = { { "F75308CU", f75308c_64 },
						  { "F75308BD", f75308b_48 },
						  { "F75308AR", f75308a_28 },
						  {} };

MODULE_DEVICE_TABLE(i2c, f75308_id);

#ifdef CONFIG_OF
static const struct of_device_id f75308_match_table[] = {
	{ .compatible = "fintek,f75308" },
	{},
};
MODULE_DEVICE_TABLE(of, f75308_match_table);
#else
#define f75308_match_table NULL
#endif

static struct i2c_driver f75308_driver = {
	.class = I2C_CLASS_HWMON,
	.driver = {
		.name = DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(f75308_match_table),
	},

	.detect = f75308_detect,
	.probe = f75308_probe,
	.remove = f75308_remove,
	.address_list = f75308_addr,
	.id_table = f75308_id,
};

module_i2c_driver(f75308_driver);

MODULE_AUTHOR("Ji-Ze Hong (Peter Hong) <hpeter+linux_kernel@gmail.com>");
MODULE_AUTHOR("Yi-Wei Wang <yiweiw@nvidia.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("F75308 hardware monitoring driver");
