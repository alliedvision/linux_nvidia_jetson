// SPDX-License-Identifier: GPL-2.0-only

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/version.h>
#include "bmi_iio.h"

enum BMI_ATTR {
	BMI_ATTR_PART,
	BMI_ATTR_MATRIX,
	BMI_ATTR_DEV_STATE,
	BMI_ATTR_DEV_ERR,
	BMI_ATTR_DUMP_REGS,
};

static ssize_t bmi_iio_attr_show(struct device *dev,
				 struct device_attribute *attr, char *buf);
static ssize_t bmi_iio_attr_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count);

static IIO_DEVICE_ATTR(part, 0444,
		       bmi_iio_attr_show, NULL, BMI_ATTR_PART);
static IIO_DEVICE_ATTR(dev_state, 0444,
		       bmi_iio_attr_show, NULL, BMI_ATTR_DEV_STATE);
static IIO_DEVICE_ATTR(dev_err, 0444,
		       bmi_iio_attr_show, NULL, BMI_ATTR_DEV_ERR);
static IIO_DEVICE_ATTR(dump_regs, 0444,
		       bmi_iio_attr_show, NULL, BMI_ATTR_DUMP_REGS);

static IIO_DEVICE_ATTR(mount_matrix, 0444,
		       bmi_iio_attr_show, bmi_iio_attr_store, BMI_ATTR_MATRIX);
static
IIO_CONST_ATTR(in_accel_sampling_frequency_available,
	       "12.5 25 50 100 200 400 800 1600");
static
IIO_CONST_ATTR(in_anglvel_sampling_frequency_available,
	       "100 200 400 1000 2000");

static
IIO_CONST_ATTR(in_accel_scale_available,
	       "0.000897 0.001795 0.003591 0.007182");
static
IIO_CONST_ATTR(in_anglvel_scale_available,
	       "0.001065 0.000532 0.000266 0.000133 0.000066");

static struct attribute *bmi_iio_attrs[] = {
	&iio_dev_attr_part.dev_attr.attr,
	&iio_dev_attr_mount_matrix.dev_attr.attr,
	&iio_dev_attr_dev_state.dev_attr.attr,
	&iio_dev_attr_dev_err.dev_attr.attr,
	&iio_dev_attr_dump_regs.dev_attr.attr,
};

static struct attribute *bmi_iio_accel_attrs[] = {
	&iio_const_attr_in_accel_sampling_frequency_available.dev_attr.attr,
	&iio_const_attr_in_accel_scale_available.dev_attr.attr,
	NULL
};

static struct attribute *bmi_iio_gyro_attrs[] = {
	&iio_const_attr_in_anglvel_sampling_frequency_available.dev_attr.attr,
	&iio_const_attr_in_anglvel_scale_available.dev_attr.attr,
	NULL
};

/* array size is same for accl and gyro */
#define ATTR_SZ_DEVICE	ARRAY_SIZE(bmi_iio_gyro_attrs)

enum bmi088_accel_scan_axis {
	BMI088_SCAN_ACCEL_X,
	BMI088_SCAN_ACCEL_Y,
	BMI088_SCAN_ACCEL_Z,
	BMI088_SCAN_ACCEL_TS,
};

enum bmi088_gyro_scan_axis {
	BMI088_SCAN_GYRO_X,
	BMI088_SCAN_GYRO_Y,
	BMI088_SCAN_GYRO_Z,
	BMI088_SCAN_GYRO_TS,
};

#define NUM_CHANNELS	3

#define BMI088_CHANNEL(_type, _axis, _index) {			\
	.type = _type,						\
	.modified = 1,						\
	.channel2 = IIO_MOD_##_axis,				\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),		\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE) |  \
		BIT(IIO_CHAN_INFO_SAMP_FREQ),			\
	.scan_index = _index,					\
	.scan_type = {						\
		.sign = 's',					\
		.realbits = 16,					\
		.storagebits = 16,				\
		.endianness = IIO_LE,				\
	},							\
}

#define IIO_CHAN_HW_TIMESTAMP(_si) {					\
	.type = IIO_TIMESTAMP,						\
	.channel = -1,							\
	.scan_index = _si,						\
	.scan_type = {							\
		.sign = 'u',						\
		.realbits = 64,						\
		.storagebits = 64,					\
		},							\
}

static const struct iio_chan_spec bmi088_acc_channels[] = {
	BMI088_CHANNEL(IIO_ACCEL, X, BMI088_SCAN_ACCEL_X),
	BMI088_CHANNEL(IIO_ACCEL, Y, BMI088_SCAN_ACCEL_Y),
	BMI088_CHANNEL(IIO_ACCEL, Z, BMI088_SCAN_ACCEL_Z),
	IIO_CHAN_HW_TIMESTAMP(BMI088_SCAN_ACCEL_TS),
};

static const struct iio_chan_spec bmi088_gyro_channels[] = {
	BMI088_CHANNEL(IIO_ANGL_VEL, X, BMI088_SCAN_GYRO_X),
	BMI088_CHANNEL(IIO_ANGL_VEL, Y, BMI088_SCAN_GYRO_Y),
	BMI088_CHANNEL(IIO_ANGL_VEL, Z, BMI088_SCAN_GYRO_Z),
	IIO_CHAN_HW_TIMESTAMP(BMI088_SCAN_GYRO_TS),
};

struct bmi_iio_state {
	void *client;
	struct device *dev;
	struct iio_fn_dev *fn_dev;
	struct sensor_cfg *cfg;
	struct iio_chan_spec *chs;
	struct attribute *attrs[ARRAY_SIZE(bmi_iio_attrs) + ATTR_SZ_DEVICE];
	struct attribute_group attr_group;
	struct iio_info info;
	u64 ts;

	struct {
		s16 chan[NUM_CHANNELS];
		u64 timestamp __aligned(8);
	} data;
};

int bmi_iio_push_buf(struct iio_dev *indio_dev, unsigned char *data, u64 ts)
{
	struct bmi_iio_state *st;
	int ret = 0;
	int bit;
	__le16 *sample = (__le16 *)data;

	if (!indio_dev || !data)
		return -EINVAL;

	st = iio_priv(indio_dev);
	if (!st)
		return -EINVAL;

	memset(&st->data, 0, sizeof(st->data));

	for_each_set_bit(bit, indio_dev->active_scan_mask,
			 indio_dev->masklength) {
		st->data.chan[bit] = sample[bit];
	}

	if (iio_buffer_enabled(indio_dev)) {
		ret = iio_push_to_buffers_with_timestamp(indio_dev, &st->data, ts);
		if (!ret) {
			if (ts)
				st->ts = ts;
		}
	}

	if (ts) {
		if (unlikely((ts - st->ts) < 0)) {
			dev_err(st->dev, "%s ts_diff=%lld\n",
				st->cfg->name, ts - st->ts);
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(bmi_iio_push_buf);

static int bmi_iio_enable(struct iio_dev *indio_dev, bool en)
{
	struct bmi_iio_state *st = iio_priv(indio_dev);
	int enable = 0;
	int bit;

	if (!en)
		return st->fn_dev->enable(st->client, st->cfg->snsr_id,
					  0, true);

	if (indio_dev->num_channels > 1) {
		for_each_set_bit(bit, indio_dev->active_scan_mask,
				 indio_dev->masklength)
			enable |= 1 << bit;
	} else {
		enable = 1;
	}


	return st->fn_dev->enable(st->client, st->cfg->snsr_id, enable, true);
}

static ssize_t bmi_iio_attr_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_iio_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	char *str;
	s8 matrix[9];
	unsigned int new;
	int ret;

	if (!indio_dev || !st || !this_attr)
		return -EINVAL;

	mutex_lock(&indio_dev->mlock);

	if (*st->fn_dev->sts & (BMI_STS_SHUTDOWN | BMI_STS_SUSPEND)) {
		mutex_unlock(&indio_dev->mlock);
		return -EPERM;
	}

	switch (this_attr->address) {
	case BMI_ATTR_MATRIX:
		for (new = 0; new < sizeof(matrix); new++) {
			str = strsep((char **)&buf, " \0");
			if (str != NULL) {
				ret = kstrtos8(str, 10, &matrix[new]);
				if (ret)
					break;

				if (matrix[new] < -1 || matrix[new] > 1) {
					ret = -EINVAL;
					break;
				}
			} else {
				ret = -EINVAL;
				break;
			}
		}

		if (new == sizeof(matrix))
			memcpy(st->cfg->matrix, matrix,
			       sizeof(st->cfg->matrix));
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&indio_dev->mlock);

	return ret;
}

static ssize_t bmi_iio_attr_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmi_iio_state *st = iio_priv(indio_dev);
	struct iio_dev_attr *this_attr = to_iio_dev_attr(attr);
	ssize_t t = 0;
	unsigned int i;

	if (!indio_dev || !st || !this_attr)
		return -EINVAL;

	switch (this_attr->address) {
	case BMI_ATTR_PART:
		return snprintf(buf, PAGE_SIZE, "%s %s\n",
				st->cfg->part, st->cfg->name);

	case BMI_ATTR_MATRIX:
		mutex_lock(&indio_dev->mlock);
		for (i = 0; i < 8; i++)
			t += snprintf(buf + t, PAGE_SIZE - t, "%hhd,",
				      st->cfg->matrix[i]);
		t += snprintf(buf + t, PAGE_SIZE - t, "%hhd\n",
			      st->cfg->matrix[i]);
		mutex_unlock(&indio_dev->mlock);

		return t;

	case BMI_ATTR_DEV_ERR:
		if (!st->fn_dev->read_err)
			return -EINVAL;

		return st->fn_dev->read_err(st->client, st->cfg->snsr_id, buf);

	case BMI_ATTR_DEV_STATE:
		return snprintf(buf, PAGE_SIZE, "dev state=%u\n",
				*st->fn_dev->sts);

	case BMI_ATTR_DUMP_REGS:
		if (!st->fn_dev->regs)
			return -EINVAL;

		return st->fn_dev->regs(st->client, st->cfg->snsr_id, buf);

	default:
		return -EINVAL;
	}

	return -EINVAL;
}

static int bmi_iio_attr(struct iio_dev *indio_dev, const char *name)
{
	struct bmi_iio_state *st;

	if (!indio_dev || !name)
		return 0;

	st = iio_priv(indio_dev);

	memcpy(st->attrs, bmi_iio_attrs, sizeof(bmi_iio_attrs));
	if (!strcmp(st->cfg->name, "accelerometer"))
		memcpy(&st->attrs[ARRAY_SIZE(bmi_iio_attrs)],
		       bmi_iio_accel_attrs, sizeof(bmi_iio_accel_attrs));
	else if (!strcmp(st->cfg->name, "gyroscope"))
		memcpy(&st->attrs[ARRAY_SIZE(bmi_iio_attrs)],
		       bmi_iio_gyro_attrs, sizeof(bmi_iio_gyro_attrs));
	else
		return 0;

	st->attr_group.attrs = st->attrs;

	return 0;
}

static inline int bmi_iio_check_enable(struct bmi_iio_state *st)
{
	if (!st->fn_dev->enable)
		return -EINVAL;

	return st->fn_dev->enable(st->client, st->cfg->snsr_id, -1, false);
}

static int bmi_iio_read_raw(struct iio_dev *indio_dev,
			    struct iio_chan_spec const *chan, int *val,
			    int *val2, long mask)
{
	struct bmi_iio_state *st = iio_priv(indio_dev);
	int ret;

	mutex_lock(&indio_dev->mlock);
	if (*st->fn_dev->sts & (BMI_STS_SHUTDOWN | BMI_STS_SUSPEND)) {
		mutex_unlock(&indio_dev->mlock);
		return -EPERM;
	}

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = bmi_iio_check_enable(st);
		if (ret < 0)
			goto out;

		if (!st->fn_dev->get_data) {
			ret = -EINVAL;
			goto out;
		}

		ret = st->fn_dev->get_data(st->client, st->cfg->snsr_id,
					   chan->channel2, val);
		if (!ret)
			ret = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SAMP_FREQ:
		if (!st->fn_dev->freq_read) {
			ret = -EINVAL;
			goto out;
		}

		ret = st->fn_dev->freq_read(st->client, st->cfg->snsr_id,
					    val, val2);
		if (!ret)
			ret = st->cfg->float_significance;
		break;
	case IIO_CHAN_INFO_SCALE:
		*val = st->cfg->scale.ival;
		*val2 = st->cfg->scale.fval;

		ret = st->cfg->float_significance;
		break;
	default:
		ret = -EINVAL;
		break;
	}

out:
	mutex_unlock(&indio_dev->mlock);

	return ret;
}

static int bmi_iio_write_raw(struct iio_dev *indio_dev,
			     struct iio_chan_spec const *chan, int val,
			     int val2, long mask)
{
	struct bmi_iio_state *st = iio_priv(indio_dev);
	int ret = 0;

	mutex_lock(&indio_dev->mlock);
	if (*st->fn_dev->sts & (BMI_STS_SHUTDOWN | BMI_STS_SUSPEND)) {
		mutex_unlock(&indio_dev->mlock);
		return -EPERM;
	}

	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		if (!st->fn_dev->freq_write) {
			ret = -EINVAL;
			goto out;
		}

		ret = st->fn_dev->freq_write(st->client, st->cfg->snsr_id,
					     val, val2);
		break;

	case IIO_CHAN_INFO_SCALE:
		if (!st->fn_dev->scale_write) {
			ret = -EINVAL;
			goto out;
		}

		ret = st->fn_dev->scale_write(st->client, st->cfg->snsr_id,
					      val, val2);
		break;

	default:
		ret = -EINVAL;
		break;
	}

out:
	mutex_unlock(&indio_dev->mlock);

	return ret;
}

static int bmi_iio_buffer_preenable(struct iio_dev *indio_dev)
{
	struct bmi_iio_state *st = iio_priv(indio_dev);

	if (*st->fn_dev->sts & (BMI_STS_SHUTDOWN | BMI_STS_SUSPEND))
		return -EINVAL;

	return 0;
}

static int bmi_iio_buffer_postenable(struct iio_dev *indio_dev)
{
	return bmi_iio_enable(indio_dev, true);
}

static int bmi_iio_buffer_postdisable(struct iio_dev *indio_dev)
{
	return bmi_iio_enable(indio_dev, false);
}

static const struct iio_buffer_setup_ops bmi_iio_buffer_setup_ops = {
	.preenable = &bmi_iio_buffer_preenable,
	.postenable = &bmi_iio_buffer_postenable,
	.postdisable = &bmi_iio_buffer_postdisable,
};

void bmi_iio_remove(struct iio_dev *indio_dev)
{
	(void)indio_dev;
}
EXPORT_SYMBOL_GPL(bmi_iio_remove);

static int __bmi_08x_iio_init(struct iio_dev *indio_dev,
			      struct bmi_iio_state *st)
{
	struct iio_buffer *buffer;
	int ret;

	ret = bmi_iio_attr(indio_dev, st->cfg->name);
	if (ret) {
		dev_err(st->dev, "bmi_iio_attr ERR=%d\n", ret);
		return ret;
	}

	if (!strcmp(st->cfg->name, "accelerometer")) {
		indio_dev->channels = bmi088_acc_channels;
		indio_dev->num_channels = ARRAY_SIZE(bmi088_acc_channels);
	} else if (!strcmp(st->cfg->name, "gyroscope")) {
		indio_dev->channels = bmi088_gyro_channels;
		indio_dev->num_channels = ARRAY_SIZE(bmi088_gyro_channels);
	} else {
		return -EINVAL;
	}

	indio_dev->modes = INDIO_DIRECT_MODE | INDIO_BUFFER_SOFTWARE;
	indio_dev->dev.parent = st->dev;
	indio_dev->name = st->cfg->name;
	st->info.attrs = &st->attr_group;
	st->info.read_raw = &bmi_iio_read_raw;
	st->info.write_raw = &bmi_iio_write_raw;
	indio_dev->info = &st->info;
	indio_dev->setup_ops = &bmi_iio_buffer_setup_ops;
	buffer = devm_iio_kfifo_allocate(st->dev);
	if (!buffer) {
		dev_err(st->dev, "devm_iio_kfifo_allocate ERR\n");
		return -ENOMEM;
	}

	iio_device_attach_buffer(indio_dev, buffer);

	ret = devm_iio_device_register(st->dev, indio_dev);
	if (ret) {
		dev_err(st->dev, "iio_device_register ERR\n");
		return ret;
	}

	return 0;
}

int bmi_08x_iio_init(struct iio_dev **handle, void *dev_client,
		     struct device *dev, struct iio_fn_dev *fn_dev,
		     struct sensor_cfg *snsr_cfg)
{
	struct iio_dev *indio_dev;
	struct bmi_iio_state *st;
	int ret;

	if (!snsr_cfg)
		return -ENODEV;

	if (snsr_cfg->snsr_id < 0) {
		/* device has been disabled */
		if (snsr_cfg->name)
			dev_info(dev, "%s disabled\n", snsr_cfg->name);
		else
			dev_info(dev, "device disabled\n");

		return -ENODEV;
	}

	indio_dev = devm_iio_device_alloc(dev, sizeof(*st));
	if (!indio_dev) {
		dev_err(dev, "iio_device_alloc ERR\n");
		return -ENOMEM;
	}

	st = iio_priv(indio_dev);
	st->client = dev_client;
	st->dev = dev;
	st->fn_dev = fn_dev;

	st->cfg = snsr_cfg;
	ret = __bmi_08x_iio_init(indio_dev, st);
	if (ret) {
		if (st->cfg->name)
			dev_err(st->dev, "%s snsr_id=%d EXIT ERR=%d\n",
				st->cfg->name, st->cfg->snsr_id, ret);
		else
			dev_err(st->dev, "snsr_id=%d EXIT ERR=%d\n",
				st->cfg->snsr_id, ret);

		return ret;
	}

	*handle = indio_dev;

	dev_info(st->dev, "iio %s done\n", st->cfg->name);

	return ret;
}
EXPORT_SYMBOL_GPL(bmi_08x_iio_init);

MODULE_AUTHOR("NVIDIA Corporation");
MODULE_DESCRIPTION("BMI088 IIO driver");
MODULE_LICENSE("GPL v2");
