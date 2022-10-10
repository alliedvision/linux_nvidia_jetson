// SPDX-License-Identifier: GPL-2.0-only
/*
 * MAXIM DP Serializer driver for MAXIM GMSL Serializers
 *
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
 */

#include <linux/device.h>
#include <linux/fwnode.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/driver.h>
#include <linux/i2c.h>
#include <linux/i2c-mux.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_graph.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/of_gpio.h>
#include <linux/workqueue.h>
#include <linux/of_device.h>
#include <linux/of.h>

#define MAX_GMSL_DP_SER_REG_13			0xD

#define MAX_GMSL_DP_SER_CTRL3			0x13
#define MAX_GMSL_DP_SER_CTRL3_LOCK_MASK		(1 << 3)
#define MAX_GMSL_DP_SER_CTRL3_LOCK_VAL		(1 << 3)

#define MAX_GMSL_DP_SER_INTR8			0x20
#define MAX_GMSL_DP_SER_INTR8_MASK		(1 << 0)
#define MAX_GMSL_DP_SER_INTR8_VAL		0x1

#define MAX_GMSL_DP_SER_INTR9			0x21
#define MAX_GMSL_DP_SER_LOSS_OF_LOCK_FLAG	(1 << 0)

#define MAX_GMSL_DP_SER_LINK_CTRL_PHY_A		0x29
#define MAX_GMSL_DP_SER_LINK_CTRL_A_MASK	(1 << 0)

#define MAX_GMSL_DP_SER_LCTRL2_A		0x2A
#define MAX_GMSL_DP_SER_LCTRL2_B		0x34
#define MAX_GMSL_DP_SER_LCTRL2_LOCK_MASK	(1 << 0)
#define MAX_GMSL_DP_SER_LCTRL2_LOCK_VAL		0x1

#define MAX_GMSL_DP_SER_LINK_CTRL_PHY_B		0x33
#define MAX_GMSL_DP_SER_LINK_CTRL_B_MASK	(1 << 0)

#define MAX_GMSL_DP_SER_VID_TX_X		0x100
#define MAX_GMSL_DP_SER_VID_TX_Y		0x110
#define MAX_GMSL_DP_SER_VID_TX_Z		0x120
#define MAX_GMSL_DP_SER_VID_TX_U		0x130
#define MAX_GMSL_DP_SER_ENABLE_LINK_A		0x0
#define MAX_GMSL_DP_SER_ENABLE_LINK_B		0x1
#define MAX_GMSL_DP_SER_ENABLE_LINK_AB		0x2

#define MAX_GMSL_DP_SER_VID_TX_MASK		(1 << 0)
#define MAX_GMSL_DP_SER_VID_TX_LINK_MASK	(3 << 1)
#define MAX_GMSL_DP_SER_LINK_SEL_SHIFT_VAL	0x1

#define MAX_GMSL_DP_SER_PHY_EDP_0_CTRL0_B0	0x6064
#define MAX_GMSL_DP_SER_PHY_EDP_0_CTRL0_B1	0x6065
#define MAX_GMSL_DP_SER_PHY_EDP_1_CTRL0_B0	0x6164
#define MAX_GMSL_DP_SER_PHY_EDP_1_CTRL0_B1	0x6165
#define MAX_GMSL_DP_SER_PHY_EDP_2_CTRL0_B0	0x6264
#define MAX_GMSL_DP_SER_PHY_EDP_2_CTRL0_B1	0x6265
#define MAX_GMSL_DP_SER_PHY_EDP_3_CTRL0_B0	0x6364
#define MAX_GMSL_DP_SER_PHY_EDP_3_CTRL0_B1	0x6365

#define MAX_GMSL_DP_SER_DPRX_TRAIN		0x641A
#define MAX_GMSL_DP_SER_DPRX_TRAIN_STATE_MASK	(0xF << 4)
#define MAX_GMSL_DP_SER_DPRX_TRAIN_STATE_VAL	0xF0

#define MAX_GMSL_DP_SER_LINK_ENABLE		0x7000
#define MAX_GMSL_DP_SER_LINK_ENABLE_MASK	(1 << 0)

#define MAX_GMSL_DP_SER_MISC_CONFIG_B1		0x7019
#define MAX_GMSL_DP_SER_MISC_CONFIG_B1_MASK	(1 << 0)
#define MAX_GMSL_DP_SER_MISC_CONFIG_B1_VAL	0x1
#define MAX_GMSL_DP_SER_MAX_LINK_COUNT		0x7070
#define MAX_GMSL_DP_SER_MAX_LINK_RATE		0x7074

#define MAX_GMSL_DP_SER_LOCAL_EDID		0x7084

#define MAX_GMSL_DP_SER_I2C_SPEED_CAPABILITY		0x70A4
#define MAX_GMSL_DP_SER_I2C_SPEED_CAPABILITY_MASK	(0x3F << 0)
#define MAX_GMSL_DP_SER_I2C_SPEED_CAPABILITY_100KBPS	0x8

#define MAX_GMSL_DP_SER_MST_PAYLOAD_ID_0	0x7904
#define MAX_GMSL_DP_SER_MST_PAYLOAD_ID_1	0x7908
#define MAX_GMSL_DP_SER_MST_PAYLOAD_ID_2	0x790C
#define MAX_GMSL_DP_SER_MST_PAYLOAD_ID_3	0x7910

#define MAX_GMSL_DP_SER_TX3_0		0xA3
#define MAX_GMSL_DP_SER_TX3_1		0xA7
#define MAX_GMSL_DP_SER_TX3_2		0xAB
#define MAX_GMSL_DP_SER_TX3_3		0xAF

#define MAX_GMSL_ARRAY_SIZE		4


struct max_gmsl_dp_ser_source {
	struct fwnode_handle *fwnode;
};

static const struct regmap_config max_gmsl_dp_ser_i2c_regmap = {
	.reg_bits = 16,
	.val_bits = 8,
};

struct max_gmsl_dp_ser_priv {
	struct i2c_client *client;
	struct gpio_desc *gpiod_pwrdn;
	u8 dprx_lane_count;
	u8 dprx_link_rate;
	struct mutex mutex;
	struct regmap *regmap;
	struct work_struct work;
	struct delayed_work delay_work;
	struct workqueue_struct *wq;
	int ser_errb;
	unsigned int ser_irq;
	bool enable_mst;
	u8 mst_payload_ids[MAX_GMSL_ARRAY_SIZE];
	u8 gmsl_stream_ids[MAX_GMSL_ARRAY_SIZE];
	u8 gmsl_link_select[MAX_GMSL_ARRAY_SIZE];
	bool link_a_is_enabled;
	bool link_b_is_enabled;
};

static int max_gmsl_dp_ser_read(struct max_gmsl_dp_ser_priv *priv, int reg)
{
	int ret, val = 0;

	ret = regmap_read(priv->regmap, reg, &val);
	if (ret < 0)
		dev_err(&priv->client->dev,
			"%s: register 0x%02x read failed (%d)\n",
			__func__, reg, ret);
	return val;
}

static int max_gmsl_dp_ser_write(struct max_gmsl_dp_ser_priv *priv, u32 reg, u8 val)
{
	int ret;

	ret = regmap_write(priv->regmap, reg, val);
	if (ret < 0)
		dev_err(&priv->client->dev,
			"%s: register 0x%02x write failed (%d)\n",
			__func__, reg, ret);
	return ret;
}

/* static api to update given value */
static inline void max_gmsl_dp_ser_update(struct max_gmsl_dp_ser_priv *priv,
					  u32 reg, u32 mask, u8 val)
{
	u8 update_val;

	update_val = max_gmsl_dp_ser_read(priv, reg);
	update_val = ((update_val & (~mask)) | (val & mask));
	max_gmsl_dp_ser_write(priv, reg, update_val);
}

static void max_gmsl_dp_ser_mst_setup(struct max_gmsl_dp_ser_priv *priv)
{
	int i;
	static const int max_mst_payload_id_reg[] = {
		MAX_GMSL_DP_SER_MST_PAYLOAD_ID_0,
		MAX_GMSL_DP_SER_MST_PAYLOAD_ID_1,
		MAX_GMSL_DP_SER_MST_PAYLOAD_ID_2,
		MAX_GMSL_DP_SER_MST_PAYLOAD_ID_3,
	};
	static const int max_gmsl_stream_id_regs[] = {
		MAX_GMSL_DP_SER_TX3_0,
		MAX_GMSL_DP_SER_TX3_1,
		MAX_GMSL_DP_SER_TX3_2,
		MAX_GMSL_DP_SER_TX3_3,
	};

	/* enable MST by programming MISC_CONFIG_B1 reg  */
	max_gmsl_dp_ser_update(priv, MAX_GMSL_DP_SER_MISC_CONFIG_B1,
			       MAX_GMSL_DP_SER_MISC_CONFIG_B1_MASK,
			       MAX_GMSL_DP_SER_MISC_CONFIG_B1_VAL);

	/* program MST payload IDs */
	for (i = 0; i < ARRAY_SIZE(priv->mst_payload_ids); i++) {
		max_gmsl_dp_ser_write(priv, max_mst_payload_id_reg[i],
				      priv->mst_payload_ids[i]);
	}

	/* Program stream IDs */
	for (i = 0; i < ARRAY_SIZE(priv->gmsl_stream_ids); i++) {
		max_gmsl_dp_ser_write(priv, max_gmsl_stream_id_regs[i],
				      priv->gmsl_stream_ids[i]);
	}
}

static void max_gmsl_dp_ser_setup(struct max_gmsl_dp_ser_priv *priv)
{
	int i;
	u8 gmsl_link_select_value = 0;
	static const int max_gmsl_ser_vid_tx_regs[] = {
		MAX_GMSL_DP_SER_VID_TX_X,
		MAX_GMSL_DP_SER_VID_TX_Y,
		MAX_GMSL_DP_SER_VID_TX_Z,
		MAX_GMSL_DP_SER_VID_TX_U,
	};

	max_gmsl_dp_ser_write(priv, MAX_GMSL_DP_SER_PHY_EDP_0_CTRL0_B0, 0x0f);
	max_gmsl_dp_ser_write(priv, MAX_GMSL_DP_SER_PHY_EDP_0_CTRL0_B1, 0x0f);
	max_gmsl_dp_ser_write(priv, MAX_GMSL_DP_SER_PHY_EDP_1_CTRL0_B0, 0x0f);
	max_gmsl_dp_ser_write(priv, MAX_GMSL_DP_SER_PHY_EDP_1_CTRL0_B1, 0x0f);
	max_gmsl_dp_ser_write(priv, MAX_GMSL_DP_SER_PHY_EDP_2_CTRL0_B0, 0x0f);
	max_gmsl_dp_ser_write(priv, MAX_GMSL_DP_SER_PHY_EDP_2_CTRL0_B1, 0x0f);
	max_gmsl_dp_ser_write(priv, MAX_GMSL_DP_SER_PHY_EDP_3_CTRL0_B0, 0x0f);
	max_gmsl_dp_ser_write(priv, MAX_GMSL_DP_SER_PHY_EDP_3_CTRL0_B1, 0x0f);

	max_gmsl_dp_ser_write(priv, MAX_GMSL_DP_SER_LOCAL_EDID, 0x1);

	/* Disable MST Mode */
	max_gmsl_dp_ser_write(priv, MAX_GMSL_DP_SER_MISC_CONFIG_B1, 0x0);

	max_gmsl_dp_ser_write(priv, MAX_GMSL_DP_SER_MAX_LINK_RATE,
			      priv->dprx_link_rate);

	max_gmsl_dp_ser_write(priv, MAX_GMSL_DP_SER_MAX_LINK_COUNT,
			      priv->dprx_lane_count);

	for (i = 0; i < MAX_GMSL_ARRAY_SIZE; i++) {
		gmsl_link_select_value = (priv->gmsl_link_select[i] <<
					  MAX_GMSL_DP_SER_LINK_SEL_SHIFT_VAL);
		max_gmsl_dp_ser_update(priv, max_gmsl_ser_vid_tx_regs[i],
				       MAX_GMSL_DP_SER_VID_TX_LINK_MASK,
				       gmsl_link_select_value);
	}

	max_gmsl_dp_ser_update(priv, MAX_GMSL_DP_SER_I2C_SPEED_CAPABILITY,
			       MAX_GMSL_DP_SER_I2C_SPEED_CAPABILITY_MASK,
			       MAX_GMSL_DP_SER_I2C_SPEED_CAPABILITY_100KBPS);

	if (priv->enable_mst)
		max_gmsl_dp_ser_mst_setup(priv);
}

static bool max_gmsl_dp_ser_check_dups(u8 *ids)
{
	int i = 0, j = 0;

	/* check if IDs are unique */
	for (i = 0; i < MAX_GMSL_ARRAY_SIZE; i++) {
		for (j = i + 1; j < MAX_GMSL_ARRAY_SIZE; j++) {
			if (ids[i] == ids[j])
				return false;
		}
	}

	return true;
}

static int max_gmsl_read_lock(struct max_gmsl_dp_ser_priv *priv,
			      u32 reg_addr, u32 mask,
			      u32 expected_value)
{
	u8 reg_data;

	reg_data = max_gmsl_dp_ser_read(priv, reg_addr);
	if ((reg_data & mask) == expected_value)
		return 0;

	return -1;
}

static irqreturn_t max_gsml_dp_ser_irq_handler(int irq, void *dev_id)
{
	struct max_gmsl_dp_ser_priv *priv = dev_id;
	int ret = 0;
	struct device *dev = &priv->client->dev;

	ret = max_gmsl_dp_ser_read(priv, MAX_GMSL_DP_SER_INTR9);
	if (ret & MAX_GMSL_DP_SER_LOSS_OF_LOCK_FLAG)
		dev_dbg(dev, "%s: Fault due to GMSL Link Loss\n", __func__);

	dev_dbg(dev, "%s: Sticky bit LOSS_OF_LOCK_FLAG cleared\n", __func__);

	return IRQ_HANDLED;
}

static void tegra_poll_gmsl_training_lock(struct work_struct *work)
{
	struct delayed_work *dwork = container_of(work,
					struct delayed_work, work);
	struct max_gmsl_dp_ser_priv *priv = container_of(dwork,
					struct max_gmsl_dp_ser_priv, delay_work);
	int ret = 0;

	ret = max_gmsl_read_lock(priv, MAX_GMSL_DP_SER_CTRL3,
				 MAX_GMSL_DP_SER_CTRL3_LOCK_MASK,
				 MAX_GMSL_DP_SER_CTRL3_LOCK_VAL);
	if (ret < 0) {
		dev_dbg(&priv->client->dev, "GMSL Lock is not set\n");
		goto reschedule;
	}

	if (priv->link_a_is_enabled) {
		ret = max_gmsl_read_lock(priv, MAX_GMSL_DP_SER_LCTRL2_A,
					 MAX_GMSL_DP_SER_LCTRL2_LOCK_MASK,
					 MAX_GMSL_DP_SER_LCTRL2_LOCK_VAL);
		if (ret < 0) {
			dev_dbg(&priv->client->dev, "GMSL Lock set failed for Link A\n");
			goto reschedule;
		}
	}

	if (priv->link_b_is_enabled) {
		ret = max_gmsl_read_lock(priv, MAX_GMSL_DP_SER_LCTRL2_B,
					 MAX_GMSL_DP_SER_LCTRL2_LOCK_MASK,
					 MAX_GMSL_DP_SER_LCTRL2_LOCK_VAL);
		if (ret < 0) {
			dev_dbg(&priv->client->dev, "GMSL Lock set failed for Link B\n");
			goto reschedule;
		}
	}

	ret = max_gmsl_read_lock(priv, MAX_GMSL_DP_SER_DPRX_TRAIN,
				 MAX_GMSL_DP_SER_DPRX_TRAIN_STATE_MASK,
				 MAX_GMSL_DP_SER_DPRX_TRAIN_STATE_VAL);
	if (ret < 0) {
		dev_dbg(&priv->client->dev,
			"DP Link tranining hasn't completed\n");
		goto reschedule;
	}

	max_gmsl_dp_ser_update(priv, MAX_GMSL_DP_SER_VID_TX_X,
			       MAX_GMSL_DP_SER_VID_TX_MASK, 0x1);
	max_gmsl_dp_ser_update(priv, MAX_GMSL_DP_SER_VID_TX_Y,
			       MAX_GMSL_DP_SER_VID_TX_MASK, 0x1);
	max_gmsl_dp_ser_update(priv, MAX_GMSL_DP_SER_VID_TX_Z,
			       MAX_GMSL_DP_SER_VID_TX_MASK, 0x1);
	max_gmsl_dp_ser_update(priv, MAX_GMSL_DP_SER_VID_TX_U,
			       MAX_GMSL_DP_SER_VID_TX_MASK, 0x1);

	return;

reschedule:
	queue_delayed_work(priv->wq,
			   &priv->delay_work, msecs_to_jiffies(500));
}

static int max_gmsl_dp_ser_init(struct device *dev)
{
	struct max_gmsl_dp_ser_priv *priv;
	struct i2c_client *client;
	int ret = 0;

	client = to_i2c_client(dev);
	priv = i2c_get_clientdata(client);

	priv->gpiod_pwrdn = devm_gpiod_get_optional(&client->dev, "enable",
						    GPIOD_OUT_HIGH);
	if (IS_ERR(priv->gpiod_pwrdn)) {
		dev_err(dev, "%s: gpiopwrdn is not enabled\n", __func__);
		return PTR_ERR(priv->gpiod_pwrdn);
	}
	gpiod_set_consumer_name(priv->gpiod_pwrdn, "max_gmsl_dp_ser-pwrdn");

	/* Drive PWRDNB pin high to power up the serializer */
	gpiod_set_value_cansleep(priv->gpiod_pwrdn, 1);

	/* Wait ~2ms for powerup to complete */
	usleep_range(2000, 2200);

	/*
	 * Write RESET_LINK = 1 (for both Phy A, 0x29, and Phy B, 0x33)
	 * within 10ms
	 */
	max_gmsl_dp_ser_update(priv, MAX_GMSL_DP_SER_LINK_CTRL_PHY_A,
			       MAX_GMSL_DP_SER_LINK_CTRL_A_MASK, 0x1);
	max_gmsl_dp_ser_update(priv, MAX_GMSL_DP_SER_LINK_CTRL_PHY_B,
			       MAX_GMSL_DP_SER_LINK_CTRL_B_MASK, 0x1);

	/*
	 * Disable video output on the GMSL link by setting VID_TX_EN = 0
	 * for Pipe X, Y, Z and U
	 */
	max_gmsl_dp_ser_update(priv, MAX_GMSL_DP_SER_VID_TX_X,
			       MAX_GMSL_DP_SER_VID_TX_MASK, 0x0);
	max_gmsl_dp_ser_update(priv, MAX_GMSL_DP_SER_VID_TX_Y,
			       MAX_GMSL_DP_SER_VID_TX_MASK, 0x0);
	max_gmsl_dp_ser_update(priv, MAX_GMSL_DP_SER_VID_TX_Z,
			       MAX_GMSL_DP_SER_VID_TX_MASK, 0x0);
	max_gmsl_dp_ser_update(priv, MAX_GMSL_DP_SER_VID_TX_U,
			       MAX_GMSL_DP_SER_VID_TX_MASK, 0x0);

	/*
	 * Set LINK_ENABLE=0 (0x7000) to force the DP HPD
	 * pin low to hold off DP link training and
	 * SOC video
	 */
	max_gmsl_dp_ser_update(priv, MAX_GMSL_DP_SER_LINK_ENABLE,
			       MAX_GMSL_DP_SER_LINK_ENABLE_MASK, 0x0);

	max_gmsl_dp_ser_setup(priv);

	/*
	 * Write RESET_LINK = 0 (for both Phy A, 0x29, and Phy B, 0x33)
	 * to initiate the GMSL link lock process.
	 */
	if (priv->link_a_is_enabled)
		max_gmsl_dp_ser_update(priv,
				       MAX_GMSL_DP_SER_LINK_CTRL_PHY_A,
				       MAX_GMSL_DP_SER_LINK_CTRL_A_MASK,
				       0x0);
	if (priv->link_b_is_enabled)
		max_gmsl_dp_ser_update(priv,
				       MAX_GMSL_DP_SER_LINK_CTRL_PHY_B,
				       MAX_GMSL_DP_SER_LINK_CTRL_B_MASK,
				       0x0);

	/*
	 * Set LINK_ENABLE = 1 (0x7000) to enable SOC DP link training,
	 * enable SOC video output to the serializer.
	 */
	max_gmsl_dp_ser_update(priv, MAX_GMSL_DP_SER_LINK_ENABLE,
			       MAX_GMSL_DP_SER_LINK_ENABLE_MASK, 0x1);

	queue_delayed_work(priv->wq, &priv->delay_work,
			   msecs_to_jiffies(500));

	return ret;
}

static int max_gmsl_dp_ser_parse_mst_props(struct i2c_client *client,
					   struct max_gmsl_dp_ser_priv *priv)
{
	struct device *dev = &priv->client->dev;
	struct device_node *ser = dev->of_node;
	int err = 0;
	bool ret;

	priv->enable_mst = of_property_read_bool(ser,
						 "enable-mst");
	if (priv->enable_mst)
		dev_info(dev, "%s: MST mode enabled:\n", __func__);
	else
		dev_info(dev, "%s: MST mode not enabled:\n", __func__);

	if (priv->enable_mst) {
		err = of_property_read_variable_u8_array(ser,
							 "mst-payload-ids",
							 priv->mst_payload_ids, 1,
							 ARRAY_SIZE(priv->mst_payload_ids));

		if (err < 0) {
			dev_info(dev,
				 "%s: MST Payload prop not found or invalid\n",
				 __func__);
			return -EINVAL;
		}

		ret = max_gmsl_dp_ser_check_dups(priv->mst_payload_ids);
		if (!ret) {
			dev_err(dev, "%s: payload IDs are not unique\n",
				__func__);
			return -EINVAL;
		}

		err = of_property_read_variable_u8_array(ser,
							 "gmsl-stream-ids",
							 priv->gmsl_stream_ids, 1,
							 ARRAY_SIZE(priv->gmsl_stream_ids));
		if (err < 0) {
			dev_info(dev,
				 "%s: GMSL Stream ID property not found or invalid\n",
				 __func__);
			return -EINVAL;
		}

		ret = max_gmsl_dp_ser_check_dups(priv->gmsl_stream_ids);
		if (!ret) {
			dev_err(dev, "%s: stream IDs are not unique\n",
				__func__);
			return -EINVAL;
		}
	}

	return 0;
}

static int max_gmsl_dp_ser_parse_dt(struct i2c_client *client,
				    struct max_gmsl_dp_ser_priv *priv)
{
	struct device *dev = &priv->client->dev;
	struct device_node *ser = dev->of_node;
	int err = 0, i;
	u32 val = 0;

	dev_info(dev, "%s: parsing serializer device tree:\n", __func__);

	err = of_property_read_u32(ser, "dprx-lane-count", &val);
	if (err) {
		if (err == -EINVAL) {
			dev_info(dev, "%s: - dprx-lane-count property not found\n",
				 __func__);
			/* default value: 4 */
			priv->dprx_lane_count = 4;
			dev_info(dev, "%s: dprx-lane-count set to default val: 4\n",
				 __func__);
		} else {
			return err;
		}
	} else {
		/* set dprx-lane-count */
		priv->dprx_lane_count = val;
		dev_info(dev, "%s: - dprx-lane-count %i\n", __func__, val);
	}

	err = of_property_read_u32(ser, "dprx-link-rate", &val);
	if (err) {
		if (err == -EINVAL) {
			dev_info(dev, "%s: - dprx-link-rate property not found\n",
				 __func__);
			/* default value: 0x1E */
			priv->dprx_link_rate = 0x1E;
			dev_info(dev, "%s: dprx-link-rate set to default val: 0x1E\n",
				 __func__);
		} else {
			return err;
		}
	} else {
		/* set dprx-link-rate*/
		priv->dprx_link_rate = val;
		dev_info(dev, "%s: - dprx-link-rate %i\n", __func__, val);
	}

	err = of_property_read_variable_u8_array(ser, "gmsl-link-select",
						 priv->gmsl_link_select, 1,
						 ARRAY_SIZE(priv->gmsl_link_select));
	if (err < 0) {
		dev_info(dev,
			 "%s: GMSL Link select property not found or invalid\n",
			 __func__);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(priv->gmsl_link_select); i++) {
		if ((priv->gmsl_link_select[i] == MAX_GMSL_DP_SER_ENABLE_LINK_A)
		    || (priv->gmsl_link_select[i] ==
		    MAX_GMSL_DP_SER_ENABLE_LINK_AB)) {
			priv->link_a_is_enabled = true;
		} else if ((priv->gmsl_link_select[i] == MAX_GMSL_DP_SER_ENABLE_LINK_B)
		    || (priv->gmsl_link_select[i] ==
		    MAX_GMSL_DP_SER_ENABLE_LINK_AB)) {
			priv->link_b_is_enabled = true;
		} else {
			dev_info(dev,
				 "%s: GMSL Link select values are invalid\n",
				 __func__);
			return -EINVAL;
		}
	}

	err = max_gmsl_dp_ser_parse_mst_props(client, priv);
	if (err != 0) {
		dev_err(dev, "%s: error parsing MST props\n", __func__);
		return -EFAULT;
	}

	return 0;
}

static int max_gmsl_dp_ser_probe(struct i2c_client *client)
{
	struct max_gmsl_dp_ser_priv *priv;
	struct device *dev;
	struct device_node *ser = client->dev.of_node;
	int ret;

	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	if (priv == NULL)
		return -ENOMEM;

	mutex_init(&priv->mutex);

	priv->client = client;
	i2c_set_clientdata(client, priv);

	priv->regmap = devm_regmap_init_i2c(client, &max_gmsl_dp_ser_i2c_regmap);
	if (IS_ERR(priv->regmap))
		return PTR_ERR(priv->regmap);

	dev = &priv->client->dev;

	ret = max_gmsl_dp_ser_read(priv, MAX_GMSL_DP_SER_REG_13);
	if (ret != 0) {
		dev_info(dev, "%s: MAXIM Serializer detected\n", __func__);
	} else {
		dev_err(dev, "%s: MAXIM Serializer Not detected\n", __func__);
		return -ENODEV;
	}

	ret = max_gmsl_dp_ser_parse_dt(client, priv);
	if (ret < 0) {
		dev_err(dev, "%s: error parsing device tree\n", __func__);
		return -EFAULT;
	}

	priv->wq = alloc_workqueue("tegra_poll_gmsl_training_lock", WQ_HIGHPRI, 0);
	INIT_DELAYED_WORK(&priv->delay_work, tegra_poll_gmsl_training_lock);

	ret = max_gmsl_dp_ser_init(&client->dev);
	if (ret < 0) {
		dev_err(dev, "%s: dp serializer init failed\n", __func__);
		return -EFAULT;
	}

	ret = max_gmsl_dp_ser_read(priv, MAX_GMSL_DP_SER_INTR9);
	if (ret < 0) {
		dev_err(dev, "%s: INTR9 register read failed\n", __func__);
		return -EFAULT;
	}
	/* enable INTR8.LOSS_OF_LOCK_OEN */
	max_gmsl_dp_ser_update(priv, MAX_GMSL_DP_SER_INTR8,
			       MAX_GMSL_DP_SER_INTR8_MASK,
			       MAX_GMSL_DP_SER_INTR8_VAL);

	priv->ser_errb = of_get_named_gpio(ser, "ser-errb", 0);

	ret = devm_gpio_request_one(&client->dev, priv->ser_errb,
				    GPIOF_DIR_IN, "GPIO_MAXIM_SER");
	if (ret < 0) {
		dev_err(dev, "%s: GPIO request failed\n ret: %d",
			__func__, ret);
		return ret;
	}

	if (gpio_is_valid(priv->ser_errb)) {
		priv->ser_irq = gpio_to_irq(priv->ser_errb);
		ret = request_threaded_irq(priv->ser_irq, NULL,
					   max_gsml_dp_ser_irq_handler,
					   IRQF_TRIGGER_FALLING
					   | IRQF_ONESHOT, "SER", priv);
		if (ret < 0) {
			dev_err(dev, "%s: Unable to register IRQ handler ret: %d\n",
				__func__, ret);
			return ret;
		}

	}
	return ret;
}

static int max_gmsl_dp_ser_remove(struct i2c_client *client)
{
	struct max_gmsl_dp_ser_priv *priv = i2c_get_clientdata(client);

	i2c_unregister_device(client);
	gpiod_set_value_cansleep(priv->gpiod_pwrdn, 0);

	return 0;
}

static const struct of_device_id max_gmsl_dp_ser_dt_ids[] = {
	{ .compatible = "maxim,max_gmsl_dp_ser" },
	{},
};
MODULE_DEVICE_TABLE(of, max_gmsl_dp_ser_dt_ids);

static struct i2c_driver max_gmsl_dp_ser_i2c_driver = {
	.driver	= {
		.name		= "max_gmsl_dp_ser",
		.of_match_table	= of_match_ptr(max_gmsl_dp_ser_dt_ids),
	},
	.probe_new	= max_gmsl_dp_ser_probe,
	.remove		= max_gmsl_dp_ser_remove,
};

module_i2c_driver(max_gmsl_dp_ser_i2c_driver);

MODULE_DESCRIPTION("Maxim DP GMSL Serializer Driver");
MODULE_AUTHOR("Vishwaroop");
MODULE_LICENSE("GPL");
