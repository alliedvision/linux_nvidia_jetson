// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2021, NVIDIA CORPORATION, All rights reserved.
 *
 * Simple PWM based backlight control, board code has to setup
 * 1) pin configuration so PWM waveforms can output
 * 2) platform_data being correctly configured
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/pwm.h>
#include <linux/pwm_backlight.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include "board-panel.h"

static void pwm_backlight_power_on(struct pwm_bl_data *pb)
{
	struct pwm_state state;
	int err;

	pwm_get_state(pb->pwm, &state);
	if (pb->enabled)
		return;

	err = regulator_enable(pb->power_supply);
	if (err < 0)
		dev_err(pb->dev, "failed to enable power supply\n");

	state.enabled = true;
	pwm_apply_state(pb->pwm, &state);

	if (pb->post_pwm_on_delay)
		msleep(pb->post_pwm_on_delay);

	if (pb->enable_gpio)
		gpiod_set_value_cansleep(pb->enable_gpio, 1);

	pb->enabled = true;
}

static void pwm_backlight_power_off(struct pwm_bl_data *pb)
{
	struct pwm_state state;

	pwm_get_state(pb->pwm, &state);
	if (!pb->enabled)
		return;

	if (pb->enable_gpio)
		gpiod_set_value_cansleep(pb->enable_gpio, 0);

	if (pb->pwm_off_delay)
		msleep(pb->pwm_off_delay);

	state.enabled = false;
	state.duty_cycle = 0;
	pwm_apply_state(pb->pwm, &state);

	regulator_disable(pb->power_supply);
	pb->enabled = false;
}

static int compute_duty_cycle(struct pwm_bl_data *pb, int brightness)
{
	unsigned int lth = pb->lth_brightness;
	struct pwm_state state;
	u64 duty_cycle;

	pwm_get_state(pb->pwm, &state);

	if (pb->levels)
		duty_cycle = pb->levels[brightness];
	else
		duty_cycle = brightness;

	duty_cycle *= state.period - lth;
	do_div(duty_cycle, pb->scale);

	return duty_cycle + lth;
}

static int pwm_backlight_update_status(struct backlight_device *bl)
{
	struct pwm_bl_data *pb = bl_get_data(bl);
	int brightness = backlight_get_brightness(bl);
	struct pwm_state state;

	if (pb->notify)
		brightness = pb->notify(pb->dev, brightness);

	if (brightness > 0) {
		pwm_get_state(pb->pwm, &state);
		state.duty_cycle = compute_duty_cycle(pb, brightness);
		pwm_apply_state(pb->pwm, &state);
		pwm_backlight_power_on(pb);
	} else {
		pwm_backlight_power_off(pb);
	}

	if (pb->notify_after)
		pb->notify_after(pb->dev, brightness);

	return 0;
}

static int pwm_backlight_check_fb(struct backlight_device *bl,
				  struct fb_info *info)
{
	struct pwm_bl_data *pb = bl_get_data(bl);

	return !pb->check_fb || pb->check_fb(pb->dev, info);
}

static const struct backlight_ops pwm_backlight_ops = {
	.update_status	= pwm_backlight_update_status,
	.check_fb	= pwm_backlight_check_fb,
};

static int pwm_backlight_notify(struct device *dev, int brightness)
{
	struct backlight_device *bl = dev_get_drvdata(dev);
	struct backlight_device_brightness_info bl_info;

	bl_info.dev = dev;
	bl_info.brightness = brightness;

	return backlight_device_notifier_call_chain(bl,
			BACKLIGHT_DEVICE_PRE_BRIGHTNESS_CHANGE,
			(void *)&bl_info);
}

static void pwm_backlight_notify_after(struct device *dev, int brightness)
{
	struct backlight_device *bl = dev_get_drvdata(dev);
	struct backlight_device_brightness_info bl_info;

	bl_info.dev = dev;
	bl_info.brightness = brightness;

	backlight_device_notifier_call_chain(bl,
			BACKLIGHT_DEVICE_POST_BRIGHTNESS_CHANGE,
			(void *)&bl_info);
}

#ifdef CONFIG_OF
#define PWM_LUMINANCE_SHIFT	16
#define PWM_LUMINANCE_SCALE	(1 << PWM_LUMINANCE_SHIFT) /* luminance scale */

/*
 * CIE lightness to PWM conversion.
 *
 * The CIE 1931 lightness formula is what actually describes how we perceive
 * light:
 *          Y = (L* / 903.3)           if L* ≤ 8
 *          Y = ((L* + 16) / 116)^3    if L* > 8
 *
 * Where Y is the luminance, the amount of light coming out of the screen, and
 * is a number between 0.0 and 1.0; and L* is the lightness, how bright a human
 * perceives the screen to be, and is a number between 0 and 100.
 *
 * The following function does the fixed point maths needed to implement the
 * above formula.
 */
static u64 cie1931(unsigned int lightness)
{
	u64 retval;

	/*
	 * @lightness is given as a number between 0 and 1, expressed
	 * as a fixed-point number in scale
	 * PWM_LUMINANCE_SCALE. Convert to a percentage, still
	 * expressed as a fixed-point number, so the above formulas
	 * can be applied.
	 */
	lightness *= 100;
	if (lightness <= (8 * PWM_LUMINANCE_SCALE)) {
		retval = DIV_ROUND_CLOSEST(lightness * 10, 9033);
	} else {
		retval = (lightness + (16 * PWM_LUMINANCE_SCALE)) / 116;
		retval *= retval * retval;
		retval += 1ULL << (2*PWM_LUMINANCE_SHIFT - 1);
		retval >>= 2*PWM_LUMINANCE_SHIFT;
	}

	return retval;
}

/*
 * Create a default correction table for PWM values to create linear brightness
 * for LED based backlights using the CIE1931 algorithm.
 */
static
int pwm_backlight_brightness_default(struct device *dev,
				     struct platform_pwm_backlight_data *data,
				     unsigned int period)
{
	unsigned int i;
	u64 retval;

	/*
	 * Once we have 4096 levels there's little point going much higher...
	 * neither interactive sliders nor animation benefits from having
	 * more values in the table.
	 */
	data->max_brightness =
		min((int)DIV_ROUND_UP(period, fls(period)), 4096);

	data->levels = devm_kcalloc(dev, data->max_brightness,
				    sizeof(*data->levels), GFP_KERNEL);
	if (!data->levels)
		return -ENOMEM;

	/* Fill the table using the cie1931 algorithm */
	for (i = 0; i < data->max_brightness; i++) {
		retval = cie1931((i * PWM_LUMINANCE_SCALE) /
				 data->max_brightness) * period;
		retval = DIV_ROUND_CLOSEST_ULL(retval, PWM_LUMINANCE_SCALE);
		if (retval > UINT_MAX)
			return -EINVAL;
		data->levels[i] = (unsigned int)retval;
	}

	data->dft_brightness = data->max_brightness / 2;
	data->max_brightness--;

	return 0;
}

static int pwm_backlight_parse_dt(struct device *dev,
				  struct platform_pwm_backlight_data *data,
				  const char *blnode_compatible,
				  struct device_node **target_bl_node)
{
	struct device_node *node = dev->of_node;
	unsigned int num_levels = 0;
	unsigned int levels_count;
	unsigned int num_steps = 0;
	struct device_node *bl_node = NULL;
	struct device_node *compat_node = NULL;
	struct property *prop;
	const __be32 *p;
	u32 u;
	unsigned int *table;
	int length;
	u32 value;
	int ret = 0;
	int n_bl_measured = 0;

	if (!node)
		return -ENODEV;

	/* If there's compat_node which is contained in
	 * backlight parent node, that means, there are
	 * multi pwm-bl device nodes and right one is
	 * chosen, with blnode_compatible */
	if (blnode_compatible)
		compat_node = of_find_compatible_node(node, NULL,
			blnode_compatible);

	if (!blnode_compatible || !compat_node)
		bl_node = node;
	else
		bl_node = compat_node;

	*target_bl_node = bl_node;

	/*
	 * These values are optional and set as 0 by default, the out values
	 * are modified only if a valid u32 value can be decoded.
	 */
	of_property_read_u32(node, "post-pwm-on-delay-ms",
			     &data->post_pwm_on_delay);
	of_property_read_u32(node, "pwm-off-delay-ms", &data->pwm_off_delay);

	/* determine the number of brightness levels */
	prop = of_find_property(bl_node, "brightness-levels", &length);
	if (!prop) {
		/* if brightness levels array is not defined,
		 * parse max brightness and default brightness,
		 * directly.
		 */
		ret = of_property_read_u32(bl_node, "max-brightness",
					   &value);
		if (ret < 0) {
			pr_info("fail to parse max-brightness\n");
			goto fail_parse_dt;
		}

		data->max_brightness = value;

#if defined(CONFIG_ANDROID) && defined(CONFIG_TEGRA_COMMON)
		if (get_androidboot_mode_charger())
			ret = of_property_read_u32(bl_node,
						   "default-charge-brightness",
						   &value);
		else
#endif
		ret = of_property_read_u32(bl_node, "default-brightness",
					   &value);
		if (ret < 0) {
			pr_info("fail to parse default-brightness\n");
			goto fail_parse_dt;
		}

		data->dft_brightness = value;
	} else {
		size_t size = 0;
		int item_counts;
		unsigned int i, j, n = 0;
		item_counts = length / sizeof(u32);
		if (item_counts > 0)
			size = sizeof(*data->levels) * item_counts;

		data->levels = devm_kzalloc(dev, size, GFP_KERNEL);
		if (!data->levels) {
			ret = -ENOMEM;
			goto fail_parse_dt;
		}

		ret = of_property_read_u32_array(bl_node,
						 "brightness-levels",
						 data->levels,
						 item_counts);
		if (ret < 0) {
			pr_info("fail to parse brightness-levels\n");
			goto fail_parse_dt;
		}

		/*
		 * default-brightness-level: the default brightness level
		 * (index into the array defined by the "brightness-levels"
		 * property)
		 */
		ret = of_property_read_u32(bl_node,
					   "default-brightness-level",
					   &value);
		if (ret < 0) {
			pr_info("fail to parse default-brightness-level\n");
			goto fail_parse_dt;
		}

		/*
		 * This property is optional, if is set enables linear
		 * interpolation between each of the values of brightness levels
		 * and creates a new pre-computed table.
		 */
		of_property_read_u32(node, "num-interpolated-steps",
				     &num_steps);

		/*
		 * Make sure that there is at least two entries in the
		 * brightness-levels table, otherwise we can't interpolate
		 * between two points.
		 */
		if (num_steps) {
			if (data->max_brightness < 2) {
				dev_err(dev, "can't interpolate\n");
				return -EINVAL;
			}

			/*
			 * Recalculate the number of brightness levels, now
			 * taking in consideration the number of interpolated
			 * steps between two levels.
			 */
			for (i = 0; i < data->max_brightness - 1; i++) {
				if ((data->levels[i + 1] - data->levels[i]) /
				   num_steps)
					num_levels += num_steps;
				else
					num_levels++;
			}
			num_levels++;
			dev_dbg(dev, "new number of brightness levels: %d\n",
				num_levels);

			/*
			 * Create a new table of brightness levels with all the
			 * interpolated steps.
			 */
			size = sizeof(*table) * num_levels;
			table = devm_kzalloc(dev, size, GFP_KERNEL);
			if (!table)
				return -ENOMEM;

			/* Fill the interpolated table. */
			levels_count = 0;
			for (i = 0; i < data->max_brightness - 1; i++) {
				value = data->levels[i];
				n = (data->levels[i + 1] - value) / num_steps;
				if (n > 0) {
					for (j = 0; j < num_steps; j++) {
						table[levels_count] = value;
						value += n;
						levels_count++;
					}
				} else {
					table[levels_count] = data->levels[i];
					levels_count++;
				}
			}
			table[levels_count] = data->levels[i];

			/*
			 * As we use interpolation lets remove current
			 * brightness levels table and replace for the
			 * new interpolated table.
			 */
			devm_kfree(dev, data->levels);
			data->levels = table;

			/*
			 * Reassign max_brightness value to the new total number
			 * of brightness levels.
			 */
			data->max_brightness = num_levels;
		}

		data->dft_brightness = data->levels[value];
		data->max_brightness = data->levels[item_counts - 1];
	}

	value = 0;
	ret = of_property_read_u32(bl_node, "lth-brightness",
		&value);
	data->lth_brightness = (unsigned int)value;

	data->pwm_gpio = of_get_named_gpio(bl_node, "pwm-gpio", 0);

	of_property_for_each_u32(bl_node, "bl-measured", prop, p, u)
		n_bl_measured++;
	if (n_bl_measured > 0) {
		data->bl_measured = devm_kzalloc(dev,
			sizeof(*data->bl_measured) * n_bl_measured, GFP_KERNEL);
		if (!data->bl_measured) {
			pr_err("bl_measured memory allocation failed\n");
			ret = -ENOMEM;
			goto fail_parse_dt;
		}
		n_bl_measured = 0;
		of_property_for_each_u32(bl_node,
			"bl-measured", prop, p, u)
			data->bl_measured[n_bl_measured++] = u;
	}

	/* label, if specified in DT, will be used as device name */
	of_property_read_string(node, "label", &data->name);

	of_node_put(compat_node);
	return 0;

fail_parse_dt:
	of_node_put(compat_node);
	return ret;
}

static const struct of_device_id pwm_backlight_of_match[] = {
	{ .compatible = "pwm-backlight" },
	{ }
};

MODULE_DEVICE_TABLE(of, pwm_backlight_of_match);
#else
static int pwm_backlight_parse_dt(struct device *dev,
				  struct platform_pwm_backlight_data *data,
				  const char *blnode_compatible,
				  struct device_node **target_bl_node)
{
	return -ENODEV;
}

static
int pwm_backlight_brightness_default(struct device *dev,
				     struct platform_pwm_backlight_data *data,
				     unsigned int period)
{
	return -ENODEV;
}
#endif

static bool pwm_backlight_is_linear(struct platform_pwm_backlight_data *data)
{
	unsigned int nlevels = data->max_brightness + 1;
	unsigned int min_val = data->levels[0];
	unsigned int max_val = data->levels[nlevels - 1];
	/*
	 * Multiplying by 128 means that even in pathological cases such
	 * as (max_val - min_val) == nlevels the error at max_val is less
	 * than 1%.
	 */
	unsigned int slope = (128 * (max_val - min_val)) / nlevels;
	unsigned int margin = (max_val - min_val) / 20; /* 5% */
	int i;

	for (i = 1; i < nlevels; i++) {
		unsigned int linear_value = min_val + ((i * slope) / 128);
		unsigned int delta = abs(linear_value - data->levels[i]);

		if (delta > margin)
			return false;
	}

	return true;
}

static int pwm_backlight_initial_power_state(const struct pwm_bl_data *pb)
{
	struct device_node *node = pb->dev->of_node;
	bool active = true;

	/*
	 * If the enable GPIO is present, observable (either as input
	 * or output) and off then the backlight is not currently active.
	 * */
	if (pb->enable_gpio && gpiod_get_value_cansleep(pb->enable_gpio) == 0)
		active = false;

	if (!regulator_is_enabled(pb->power_supply))
		active = false;

	if (!pwm_is_enabled(pb->pwm))
		active = false;

	/*
	 * Synchronize the enable_gpio with the observed state of the
	 * hardware.
	 */
	if (pb->enable_gpio)
		gpiod_direction_output(pb->enable_gpio, active);

	/*
	 * Do not change pb->enabled here! pb->enabled essentially
	 * tells us if we own one of the regulator's use counts and
	 * right now we do not.
	 */

	/* Not booted with device tree or no phandle link to the node */
	if (!node || !node->phandle)
		return FB_BLANK_UNBLANK;

	/*
	 * If the driver is probed from the device tree and there is a
	 * phandle link pointing to the backlight node, it is safe to
	 * assume that another driver will enable the backlight at the
	 * appropriate time. Therefore, if it is disabled, keep it so.
	 */
	return active ? FB_BLANK_UNBLANK: FB_BLANK_POWERDOWN;
}

static int pwm_backlight_probe(struct platform_device *pdev)
{
	struct platform_pwm_backlight_data *data = dev_get_platdata(&pdev->dev);
	struct device_node *np = pdev->dev.of_node;
	struct platform_pwm_backlight_data defdata;
	struct backlight_properties props;
	struct backlight_device *bl;
	struct device_node *node = pdev->dev.of_node;
	struct pwm_bl_data *pb;
	struct pwm_state state;
	unsigned int i;
	struct device_node *target_bl_node = NULL;

	int ret;
	const char *blnode_compatible = NULL;

	if (!np && !pdev->dev.platform_data) {
		dev_err(&pdev->dev, "no platform data for pwm_bl\n");
		return -ENOENT;
	}

	if (np) {
		struct pwm_bl_data_dt_ops *pops;
		tegra_pwm_bl_ops_register(&pdev->dev);
		pops = (struct pwm_bl_data_dt_ops *)platform_get_drvdata(pdev);
		memset(&defdata, 0, sizeof(defdata));
		if (pops) {
			defdata.init = pops->init;
			defdata.notify = pops->notify;
			defdata.notify_after = pops->notify_after;
			defdata.check_fb = pops->check_fb;
			defdata.exit = pops->exit;
			blnode_compatible = pops->blnode_compatible;
		} else {
			defdata.notify = pwm_backlight_notify;
			defdata.notify_after = pwm_backlight_notify_after;
		}

		ret = pwm_backlight_parse_dt(&pdev->dev, &defdata,
			blnode_compatible, &target_bl_node);
		if (ret < 0) {
			dev_err(&pdev->dev, "fail to find platform data\n");
			return ret;
		}
		data = &defdata;

		/* initialize dev drv data */
		platform_set_drvdata(pdev, NULL);
	}

	if (data->init) {
		ret = data->init(&pdev->dev);
		if (ret < 0)
			return ret;
	}

	pb = devm_kzalloc(&pdev->dev, sizeof(*pb), GFP_KERNEL);
	if (!pb) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	pb->notify = data->notify;
	pb->notify_after = data->notify_after;
	pb->bl_measured = data->bl_measured;
	pb->check_fb = data->check_fb;
	pb->exit = data->exit;
	pb->dev = &pdev->dev;
	pb->pwm_gpio = data->pwm_gpio;
	pb->enabled = false;
	pb->post_pwm_on_delay = data->post_pwm_on_delay;
	pb->pwm_off_delay = data->pwm_off_delay;

	pb->enable_gpio = devm_gpiod_get_optional(&pdev->dev, "enable",
						  GPIOD_ASIS);
	if (IS_ERR(pb->enable_gpio)) {
		ret = PTR_ERR(pb->enable_gpio);
		goto err_alloc;
	}

	pb->power_supply = devm_regulator_get(&pdev->dev, "power");
	if (IS_ERR(pb->power_supply)) {
		ret = PTR_ERR(pb->power_supply);
		goto err_alloc;
	}

	pb->pwm = devm_pwm_get(&pdev->dev, NULL);
	if (IS_ERR(pb->pwm) && PTR_ERR(pb->pwm) != -EPROBE_DEFER) {
		pb->pwm = of_pwm_get(&pdev->dev, target_bl_node, NULL);
		if (IS_ERR(pb->pwm) && !node) {
			dev_err(&pdev->dev, "unable to request PWM, trying legacy API\n");
			pb->legacy = true;
			pb->pwm = pwm_request(data->pwm_id, "pwm-backlight");
		}
	}

	if (IS_ERR(pb->pwm)) {
		ret = PTR_ERR(pb->pwm);
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "unable to request PWM\n");
		goto err_alloc;
	}

	dev_dbg(&pdev->dev, "got pwm for backlight\n");

	/* Sync up PWM state. */
	pwm_init_state(pb->pwm, &state);

	/*
	 * The DT case will not set pwm_period_ns. Instead, it stores the
	 * period, parsed from the DT, in the PWM device. In other words,
	 * the 2nd argument of pwms property indicates pwm_period in
	 * nonoseconds. For the non-DT case, set the period from
	 * platform data.
	 */
	if (!state.period && (data->pwm_period_ns > 0))
		state.period = data->pwm_period_ns;

	ret = pwm_apply_state(pb->pwm, &state);
	if (ret) {
		dev_err(&pdev->dev, "failed to apply initial PWM state: %d\n",
			ret);
		goto err_alloc;
	}

	memset(&props, 0, sizeof(struct backlight_properties));

	if (data->levels) {
		pb->levels = data->levels;

		/*
		 * For the DT case, only when brightness levels is defined
		 * data->levels is filled. For the non-DT case, data->levels
		 * can come from platform data, however is not usual.
		 */
		for (i = 0; i <= data->max_brightness; i++)
			if (data->levels[i] > pb->scale)
				pb->scale = data->levels[i];

		if (pwm_backlight_is_linear(data))
			props.scale = BACKLIGHT_SCALE_LINEAR;
		else
			props.scale = BACKLIGHT_SCALE_NON_LINEAR;
	} else if (!data->max_brightness) {
		/*
		 * If no brightness levels are provided and max_brightness is
		 * not set, use the default brightness table. For the DT case,
		 * max_brightness is set to 0 when brightness levels is not
		 * specified. For the non-DT case, max_brightness is usually
		 * set to some value.
		 */

		/* Get the PWM period (in nanoseconds) */
		pwm_get_state(pb->pwm, &state);

		ret = pwm_backlight_brightness_default(&pdev->dev, data,
						       state.period);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"failed to setup default brightness table\n");
			goto err_alloc;
		}

		for (i = 0; i <= data->max_brightness; i++) {
			if (data->levels[i] > pb->scale)
				pb->scale = data->levels[i];

			pb->levels = data->levels;
		}

		props.scale = BACKLIGHT_SCALE_NON_LINEAR;
	} else {
		/*
		 * That only happens for the non-DT case, where platform data
		 * sets the max_brightness value.
		 */
		pb->scale = data->max_brightness;
	}

	pb->lth_brightness = data->lth_brightness * (div_u64(state.period,
				pb->scale));

	props.type = BACKLIGHT_RAW;
	props.max_brightness = data->max_brightness;

	if (gpio_is_valid(pb->pwm_gpio)) {
		ret = gpio_request(pb->pwm_gpio, "disp_bl");
		if (ret)
			dev_err(&pdev->dev, "backlight gpio request failed\n");
	}

	bl = backlight_device_register(data->name ? data->name :
					dev_name(&pdev->dev), &pdev->dev,
					pb, &pwm_backlight_ops, &props);
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		ret = PTR_ERR(bl);
		if (pb->legacy)
			pwm_free(pb->pwm);
		goto err_alloc;
	}

	if (data->dft_brightness > data->max_brightness) {
		dev_warn(&pdev->dev,
			 "invalid dft brightness: %u, using max one %u\n",
			 data->dft_brightness, data->max_brightness);
		data->dft_brightness = data->max_brightness;
	}

	platform_set_drvdata(pdev, bl);
	bl->props.brightness = data->dft_brightness;
	bl->props.power = pwm_backlight_initial_power_state(pb);
	backlight_update_status(bl);

	if (gpio_is_valid(pb->pwm_gpio))
		gpio_free(pb->pwm_gpio);

	return 0;

err_alloc:
	if (data->exit)
		data->exit(&pdev->dev);
	return ret;
}

static int pwm_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct pwm_bl_data *pb = bl_get_data(bl);

	backlight_device_unregister(bl);
	pwm_backlight_power_off(pb);

	if (pb->exit)
		pb->exit(&pdev->dev);
	if (pb->legacy)
		pwm_free(pb->pwm);

	return 0;
}

static void pwm_backlight_shutdown(struct platform_device *pdev)
{
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct pwm_bl_data *pb = bl_get_data(bl);

	pwm_backlight_power_off(pb);
}

#ifdef CONFIG_PM_SLEEP
static int pwm_backlight_suspend(struct device *dev)
{
	struct backlight_device *bl = dev_get_drvdata(dev);
	struct pwm_bl_data *pb = bl_get_data(bl);

	if (pb->notify)
		pb->notify(pb->dev, 0);

	pwm_backlight_power_off(pb);

	if (pb->notify_after)
		pb->notify_after(pb->dev, 0);

	return 0;
}

static int pwm_backlight_resume(struct device *dev)
{
	struct backlight_device *bl = dev_get_drvdata(dev);

	backlight_update_status(bl);

	return 0;
}
#endif

static const struct dev_pm_ops pwm_backlight_pm_ops = {
#ifdef CONFIG_PM_SLEEP
	.suspend = pwm_backlight_suspend,
	.resume = pwm_backlight_resume,
	.poweroff = pwm_backlight_suspend,
	.restore = pwm_backlight_resume,
#endif
};

static struct platform_driver pwm_backlight_driver = {
	.driver		= {
		.name		= "pwm-backlight",
		.pm		= &pwm_backlight_pm_ops,
		.of_match_table	= of_match_ptr(pwm_backlight_of_match),
	},
	.probe		= pwm_backlight_probe,
	.remove		= pwm_backlight_remove,
	.shutdown	= pwm_backlight_shutdown,
};

module_platform_driver(pwm_backlight_driver);

MODULE_DESCRIPTION("PWM based Backlight Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:pwm-backlight");
