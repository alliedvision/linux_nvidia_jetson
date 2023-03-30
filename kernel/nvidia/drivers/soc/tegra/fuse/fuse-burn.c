// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016-2022, NVIDIA CORPORATION.  All rights reserved.
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <soc/tegra/fuse.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/thermal.h>
#include <linux/version.h>

#include <soc/tegra/pmc.h>
#include <soc/tegra/tegra_bpmp.h>
#include <soc/tegra/bpmp_abi.h>

#define TEGRA_FUSE_CTRL				0x0
#define TEGRA_FUSE_CTRL_CMD_READ		0x1
#define TEGRA_FUSE_CTRL_CMD_WRITE		0x2
#define TEGRA_FUSE_CTRL_CMD_SENSE		0x3
#define TEGRA_FUSE_CTRL_CMD_MASK		0x3
#define TEGRA_FUSE_CTRL_STATE_IDLE		0x4
#define TEGRA_FUSE_CTRL_STATE_MASK		0x1f
#define TEGRA_FUSE_CTRL_STATE_SHIFT		16
#define TEGRA_FUSE_CTRL_PD			BIT(26)
#define TEGRA_FUSE_CTRL_SENSE_DONE		BIT(30)
#define TEGRA_FUSE_ADDR				0x4
#define TEGRA_FUSE_RDATA			0x8
#define TEGRA_FUSE_WDATA			0xc
#define TEGRA_FUSE_TIME_PGM2			0x1c
#define TEGRA_FUSE_PRIV2INTFC_START		0x20
#define TEGRA_FUSE_PRIV2INTFC_SDATA		0x1
#define TEGRA_FUSE_PRIV2INTFC_SKIP_RECORDS	0x2
#define TEGRA_FUSE_DISABLE_REG_PROG		0x2c
#define TEGRA_FUSE_WRITE_ACCESS_SW		0x30
#define TEGRA_FUSE_OPT_TPC_DISABLE		0x20c
#define TEGRA_FUSE_SLAM				0x84
#define TEGRA_FUSE_SLAM_LOCK			(0x1 << 31)

#define TEGRA_FUSE_ENABLE_PRGM_OFFSET		0
#define TEGRA_FUSE_ENABLE_PRGM_REDUND_OFFSET	1
#define TEGRA_FUSE_BURN_MAX_FUSES		30

#define TEGRA_FUSE_ODM_PRODUCTION_MODE		0xa0
#define H2_START_MACRO_BIT_INDEX		2167
#define H2_END_MACRO_BIT_INDEX			3326

#define FPERM_R					0440
#define FPERM_RW				0660

#define TEGRA_FUSE_SHUTDOWN_LIMIT_MODIFIER	2000

struct fuse_burn_data {
	char *name;
	u32 start_offset;
	u32 start_bit;
	u32 size_bits;
	u32 reg_offset;
	bool is_redundant;
	bool is_big_endian;
	bool redundant_war;
	struct device_attribute attr;
};

struct tegra_fuse_hw_feature {
	bool power_down_mode;
	bool mirroring_support;
	bool hw_mutex_support;
	bool has_power_switch;
	int pgm_time;
	struct fuse_burn_data burn_data[TEGRA_FUSE_BURN_MAX_FUSES];
};

struct tegra_fuse_burn_dev {
	struct device *dev;
	struct tegra_fuse_hw_feature *hw;
	struct clk *pgm_clk;
	u32 pgm_width;
	struct thermal_zone_device *tz;
	s32 min_temp;
	s32 max_temp;
	u32 thermal_zone;
};

static DEFINE_MUTEX(fuse_lock);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
static u64 chip_uid;
#endif

static void fuse_state_wait_for_idle(void)
{
	u32 reg;
	u32 idle;

	do {
		tegra_fuse_control_read(TEGRA_FUSE_CTRL, &reg);
		idle = reg & (TEGRA_FUSE_CTRL_STATE_MASK
				<< TEGRA_FUSE_CTRL_STATE_SHIFT);
		udelay(1);
	} while (idle != (TEGRA_FUSE_CTRL_STATE_IDLE
				<< TEGRA_FUSE_CTRL_STATE_SHIFT));
}

static u32 fuse_cmd_read(u32 addr)
{
	u32 reg;

	fuse_state_wait_for_idle();
	tegra_fuse_control_write(addr, TEGRA_FUSE_ADDR);
	tegra_fuse_control_read(TEGRA_FUSE_CTRL, &reg);
	reg &= ~TEGRA_FUSE_CTRL_CMD_MASK;
	reg |= TEGRA_FUSE_CTRL_CMD_READ;
	tegra_fuse_control_write(reg, TEGRA_FUSE_CTRL);
	fuse_state_wait_for_idle();
	tegra_fuse_control_read(TEGRA_FUSE_RDATA, &reg);

	return reg;
}

static void fuse_cmd_write(u32 value, u32 addr)
{
	u32 reg;

	fuse_state_wait_for_idle();
	tegra_fuse_control_write(addr, TEGRA_FUSE_ADDR);
	tegra_fuse_control_write(value, TEGRA_FUSE_WDATA);

	tegra_fuse_control_read(TEGRA_FUSE_CTRL, &reg);
	reg &= ~TEGRA_FUSE_CTRL_CMD_MASK;
	reg |= TEGRA_FUSE_CTRL_CMD_WRITE;
	tegra_fuse_control_write(reg, TEGRA_FUSE_CTRL);
	fuse_state_wait_for_idle();
	fuse_cmd_read(addr);
}

static u32 tegra_fuse_calculate_parity(u32 val)
{
	u32 i, p = 0;

	for (i = 0; i < 32; i++)
		p ^= ((val >> i) & 1);

	return p;
}

static inline int tegra_fuse_acquire_burn_lock(
			struct tegra_fuse_burn_dev *fuse_dev)
{
	u32 reg;
	int num_retries = 3;

	while (num_retries--) {
		tegra_fuse_control_read(TEGRA_FUSE_SLAM, &reg);

		if (reg & TEGRA_FUSE_SLAM_LOCK) {
			/* If mutex is still acquired after 3 retries.
			 * Return -EPERM.
			 */
			if (!num_retries) {
				dev_err(fuse_dev->dev,
					"fuse burn already in progress.\n");
				return -EPERM;
			}
			udelay(10);
			continue;
		}
		break;
	}

	/* Acquire mutex by writing 1 into the LOCK bit. */
	tegra_fuse_control_write(reg | TEGRA_FUSE_SLAM_LOCK, TEGRA_FUSE_SLAM);
	dev_info(fuse_dev->dev, "acquired write lock\n");

	return 0;
}

static inline void tegra_fuse_release_burn_lock(
			struct tegra_fuse_burn_dev *fuse_dev)
{
	u32 reg;

	tegra_fuse_control_read(TEGRA_FUSE_SLAM, &reg);

	/* Release the mutex by clearing LOCK bit. */
	if (reg & TEGRA_FUSE_SLAM_LOCK)
		tegra_fuse_control_write(reg & ~TEGRA_FUSE_SLAM_LOCK,
					 TEGRA_FUSE_SLAM);

	dev_info(fuse_dev->dev, "released write lock\n");
}

static bool tegra_fuse_is_fuse_burn_allowed(struct fuse_burn_data *data)
{
	u32 reg = 0;
	int ret;

	/* If odm_production_mode(security mode) fuse is burnt, then
	 * only allow odm reserved/lock to burn
	 */
	ret = tegra_fuse_readl(TEGRA_FUSE_ODM_PRODUCTION_MODE, &reg);
	if (!ret) {
		if (reg) {
			if (!strncmp(data->name, "reserved_odm", 12))
				return true;
			if (!strcmp(data->name, "odm_lock"))
				return true;
			return false;
		}
	}

	return true;
}

static int tegra_fuse_form_burn_data(struct fuse_burn_data *data,
		u32 *input_data, u32 *burn_data, u32 *burn_mask)
{
	int nbits = data->size_bits;
	int start_bit = data->start_bit;
	int i, offset, loops;
	int src_bit = 0;
	u32 val;

	for (offset = 0, loops = 0; nbits > 0; offset++, nbits -= loops) {
		val = *input_data;
		loops = min(nbits, 32 - start_bit);
		for (i = 0; i < loops; i++) {
			burn_mask[offset] |= BIT(start_bit + i);
			if (val & BIT(src_bit))
				burn_data[offset] |= BIT(start_bit + i);
			else
				burn_data[offset] &= ~BIT(start_bit + i);
			src_bit++;
			if (src_bit == 32) {
				input_data++;
				val = *input_data;
				src_bit = 0;
			}
		}
		start_bit = 0;
	}

	return offset;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
static int tegra_fuse_get_shutdown_limit(struct tegra_fuse_burn_dev *fuse_dev,
					 int *shutdown_limit)
{
	struct mrq_thermal_host_to_bpmp_request req;
	union mrq_thermal_bpmp_to_host_response reply;
	int err = 0;

	memset(&req, 0, sizeof(req));
	req.type = CMD_THERMAL_GET_THERMTRIP;
	req.get_thermtrip.zone = fuse_dev->thermal_zone;

	err = tegra_bpmp_send_receive(MRQ_THERMAL, &req, sizeof(req), &reply,
				      sizeof(reply));
	if (err)
		goto out;

	*shutdown_limit = reply.get_thermtrip.thermtrip;
out:
	return err;
}
#endif

static int tegra_fuse_is_temp_under_range(struct tegra_fuse_burn_dev *fuse_dev)
{
	int temp, ret = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	int shutdown_limit = 0;
#endif

	/* Check if temperature is under permissible range */
	ret = thermal_zone_get_temp(fuse_dev->tz, &temp);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0)
	if (!ret && (temp < fuse_dev->min_temp || temp > fuse_dev->max_temp)) {
		dev_err(fuse_dev->dev, "temp-%d is not under range\n", temp);
		return -EPERM;
	}
#else
	if (ret)
		goto out;

	if (temp < fuse_dev->min_temp ||
		temp > fuse_dev->max_temp) {
		dev_err(fuse_dev->dev, "temp-%d is not under range\n",
				temp);
		ret = -EPERM;
		goto out;
	}

	if (!fuse_dev->thermal_zone)
		goto out;

	ret = tegra_fuse_get_shutdown_limit(fuse_dev, &shutdown_limit);
	if (ret) {
		dev_err(fuse_dev->dev, "unable to get shutdown limit: %d\n",
			 ret);
		ret = -EPERM;
		goto out;
	}

	/* Check if current temperature is 2C degrees below shutdown limit*/
	if (temp > (shutdown_limit - TEGRA_FUSE_SHUTDOWN_LIMIT_MODIFIER)) {
		dev_err(fuse_dev->dev, "temp-%d close to shutdown limit\n",
			temp);
		ret = -EPERM;
	}
out:
#endif
	return ret;
}

static void tegra_fuse_set_pd(bool pd)
{
	u32 reg;

	tegra_fuse_control_read(TEGRA_FUSE_CTRL, &reg);

	if (pd && !(reg & TEGRA_FUSE_CTRL_PD)) {
		udelay(1);
		reg |= TEGRA_FUSE_CTRL_PD;
		tegra_fuse_control_write(reg, TEGRA_FUSE_CTRL);
	} else if (!pd && (reg & TEGRA_FUSE_CTRL_PD)) {
		reg &= ~TEGRA_FUSE_CTRL_PD;
		tegra_fuse_control_write(reg, TEGRA_FUSE_CTRL);
		tegra_fuse_control_read(TEGRA_FUSE_CTRL, &reg);
		udelay(1);
	}
}

static int tegra_fuse_pre_burn_process(struct tegra_fuse_burn_dev *fuse_dev)
{
	u32 off_0_val, off_1_val, reg;
	int ret;

	if (fuse_dev->tz) {
		ret = tegra_fuse_is_temp_under_range(fuse_dev);
		if (ret)
			return ret;
	}

	/* Check if fuse burn is disabled */
	reg = tegra_fuse_control_read(TEGRA_FUSE_DISABLE_REG_PROG, &reg);
	if (reg) {
		dev_err(fuse_dev->dev, "Fuse register programming disabled\n");
		return -EIO;
	}

	if (fuse_dev->hw->hw_mutex_support) {
		ret = tegra_fuse_acquire_burn_lock(fuse_dev);
		if (ret)
			return ret;
	}

	/* Enable fuse register write access */
	tegra_fuse_control_write(0, TEGRA_FUSE_WRITE_ACCESS_SW);

	/* Disable power down mode */
	if (fuse_dev->hw->power_down_mode)
		tegra_fuse_set_pd(false);

	if (fuse_dev->pgm_width)
		tegra_fuse_control_write(fuse_dev->pgm_width,
				      TEGRA_FUSE_TIME_PGM2);

	if (fuse_dev->hw->mirroring_support)
		tegra_pmc_fuse_disable_mirroring();

	if (fuse_dev->hw->has_power_switch)
		tegra_pmc_fuse_control_ps18_latch_set();

	/* Enable fuse program */
	off_0_val = fuse_cmd_read(TEGRA_FUSE_ENABLE_PRGM_OFFSET);
	off_1_val = fuse_cmd_read(TEGRA_FUSE_ENABLE_PRGM_REDUND_OFFSET);
	off_0_val = 0x1 & ~off_0_val;
	off_1_val = 0x1 & ~off_1_val;
	fuse_cmd_write(off_0_val, TEGRA_FUSE_ENABLE_PRGM_OFFSET);
	fuse_cmd_write(off_1_val, TEGRA_FUSE_ENABLE_PRGM_REDUND_OFFSET);

	return 0;
}

static void tegra_fuse_post_burn_process(struct tegra_fuse_burn_dev *fuse_dev)
{
	u32 reg;
	u32 sense_done;

	/* burned fuse values can take effect without reset by below steps*/
	reg = TEGRA_FUSE_PRIV2INTFC_SDATA | TEGRA_FUSE_PRIV2INTFC_SKIP_RECORDS;
	tegra_fuse_control_write(reg, TEGRA_FUSE_PRIV2INTFC_START);
	fuse_state_wait_for_idle();
	do {
		udelay(1);
		tegra_fuse_control_read(TEGRA_FUSE_CTRL, &reg);
		sense_done = reg & TEGRA_FUSE_CTRL_SENSE_DONE;
	} while (!sense_done);

	/* Enable power down mode */
	if (fuse_dev->hw->power_down_mode)
		tegra_fuse_set_pd(true);

	if (fuse_dev->hw->has_power_switch)
		tegra_pmc_fuse_control_ps18_latch_clear();

	if (fuse_dev->hw->mirroring_support)
		tegra_pmc_fuse_enable_mirroring();

	if (fuse_dev->hw->hw_mutex_support)
		tegra_fuse_release_burn_lock(fuse_dev);

	/* Disable fuse register write access */
	tegra_fuse_control_write(1, TEGRA_FUSE_WRITE_ACCESS_SW);
}

static int tegra_fuse_burn_fuse(struct tegra_fuse_burn_dev *fuse_dev,
	struct fuse_burn_data *fuse_data, u32 *input_data)
{
	u32 reg, burn_data[17] = {0}, burn_mask[17] = {0};
	int fuse_addr = fuse_data->start_offset;
	int is_redundant = fuse_data->is_redundant;
	int i;
	int num_words;
	int ret;

	ret = tegra_fuse_pre_burn_process(fuse_dev);
	if (ret)
		return ret;

	/* Form burn data */
	num_words = tegra_fuse_form_burn_data(fuse_data, input_data,
					      burn_data, burn_mask);

	/* Burn the fuse */
	for (i = 0; i < num_words; i++) {
		reg = fuse_cmd_read(fuse_addr);
		burn_data[i] = (burn_data[i] & ~reg) & burn_mask[i];
		if (burn_data[i]) {
			fuse_cmd_write(burn_data[i], fuse_addr);
			if (is_redundant)
				fuse_cmd_write(burn_data[i], fuse_addr + 1);
		}
		if (is_redundant)
			fuse_addr += 2;
		else
			fuse_addr += 1;
	}

	tegra_fuse_post_burn_process(fuse_dev);

	return 0;
}

static void tegra_fuse_get_fuse(struct tegra_fuse_burn_dev *fuse_dev,
	struct fuse_burn_data *data, u32 *macro_buf)
{
	int start_bit = data->start_bit;
	int nbits = data->size_bits;
	int offset = data->start_offset;
	bool is_redundant = data->is_redundant;
	bool redundant_war = data->redundant_war;
	int bit_position = 0;
	int i, loops;
	u32 actual_val, redun_val = 0;

	/* Disable power down mode */
	if (fuse_dev->hw->power_down_mode)
		tegra_fuse_set_pd(false);

	do {
		actual_val = fuse_cmd_read(offset);
		if (is_redundant)
			redun_val = fuse_cmd_read(offset + 1);
		loops = min(nbits, 32 - start_bit);
		for (i = 0; i < loops; i++) {
			if (actual_val & (BIT(start_bit + i)))
				*macro_buf |= BIT(bit_position);
			/* If redundant WAR enable, skip redun_val */
			if (is_redundant && !redundant_war) {
				if (redun_val & (BIT(start_bit + i)))
					*macro_buf |= BIT(bit_position);
			}
			bit_position++;
			if (bit_position == 32) {
				macro_buf++;
				bit_position = 0;
			}
		}
		nbits -= loops;
		if (is_redundant)
			offset += 2;
		else
			offset += 1;
		start_bit = 0;
	} while (nbits > 0);

	/* Enable power down mode */
	if (fuse_dev->hw->power_down_mode)
		tegra_fuse_set_pd(true);
}

static ssize_t tegra_fuse_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct platform_device *pdev = container_of(dev,
			struct platform_device, dev);
	struct tegra_fuse_burn_dev *fuse_dev = platform_get_drvdata(pdev);
	struct fuse_burn_data *data;
	char str[9];
	u32 *macro_buf;
	int num_words, i;
	__be32 val;

	data = container_of(attr, struct fuse_burn_data, attr);
	num_words = DIV_ROUND_UP(data->size_bits, 32);
	macro_buf = kcalloc(num_words, sizeof(*macro_buf), GFP_KERNEL);
	if (!macro_buf) {
		dev_err(dev, "buffer allocation failed\n");
		return -ENOMEM;
	}

	mutex_lock(&fuse_lock);
	tegra_fuse_get_fuse(fuse_dev, data, macro_buf);
	mutex_unlock(&fuse_lock);
	strcpy(buf, "0x");
	if (data->is_big_endian) {
		for (i = 0; i < num_words; i++) {
			val = cpu_to_be32(macro_buf[i]);
			sprintf(str, "%08x", val);
			strcat(buf, str);
		}
	} else {
		while (num_words--) {
			sprintf(str, "%08x", macro_buf[num_words]);
			strcat(buf, str);
		}
	}
	strcat(buf, "\n");
	kfree(macro_buf);

	return strlen(buf);
}

static ssize_t tegra_fuse_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct platform_device *pdev = container_of(dev,
			struct platform_device, dev);
	struct tegra_fuse_burn_dev *fuse_dev = platform_get_drvdata(pdev);
	struct fuse_burn_data *fuse_data;
	int len = count;
	int num_nibbles;
	u32 input_data[17] = {0};
	__be32 temp_data[17] = {0};
	char str[9] = {0};
	int copy_cnt, copy_idx;
	int burn_idx = 0, idx;
	int ret;

	fuse_data = container_of(attr, struct fuse_burn_data, attr);
	if (!tegra_fuse_is_fuse_burn_allowed(fuse_data)) {
		dev_err(dev, "security mode fuse is burnt, burn not allowed\n");
		return -EPERM;
	}

	num_nibbles = DIV_ROUND_UP(fuse_data->size_bits, 4);

	if (*buf == 'x') {
		len--;
		buf++;
	}
	if (*buf == '0' && (*(buf + 1) == 'x' || *(buf + 1) == 'X')) {
		len -= 2;
		buf += 2;
	}
	len--;
	if (len > num_nibbles) {
		dev_err(dev, "input data too long, max is %d characters\n",
			num_nibbles);
		return -EINVAL;
	}

	for (burn_idx = 0; len; burn_idx++) {
		copy_idx = len > 8 ? (len - 8) : 0;
		copy_cnt = len > 8 ? 8 : len;
		memset(str, 0, sizeof(str));
		strncpy(str, buf + copy_idx, copy_cnt);
		ret = kstrtouint(str, 16, &input_data[burn_idx]);
		if (ret)
			return ret;
		len -= copy_cnt;
	}

	if (fuse_data->is_big_endian) {
		for (idx = --burn_idx, copy_cnt = 0; idx >= 0;
				idx--, copy_cnt++)
			temp_data[copy_cnt] = cpu_to_be32(input_data[idx]);
		memcpy(input_data, temp_data, sizeof(input_data));
	}

	pm_stay_awake(fuse_dev->dev);
	mutex_lock(&fuse_lock);
	ret = tegra_fuse_burn_fuse(fuse_dev, fuse_data, input_data);
	mutex_unlock(&fuse_lock);
	pm_relax(fuse_dev->dev);
	if (ret)
		return ret;

	return count;
}

static ssize_t tegra_fuse_calc_h2_code(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	u32 startrowindex, startbitindex;
	u32 endrowindex, endbitindex;
	u32 bitindex;
	u32 rowindex;
	u32 rowdata;
	u32 hammingvalue = 0;
	u32 pattern = 0x7ff;
	u32 parity;
	u32 hammingcode;

	startrowindex = H2_START_MACRO_BIT_INDEX / 32;
	startbitindex = H2_START_MACRO_BIT_INDEX % 32;
	endrowindex = H2_END_MACRO_BIT_INDEX / 32;
	endbitindex = H2_END_MACRO_BIT_INDEX % 32;

	mutex_lock(&fuse_lock);
	for (rowindex = startrowindex; rowindex <= endrowindex; rowindex++) {
		rowdata = fuse_cmd_read(rowindex);
		for (bitindex = 0; bitindex < 32; bitindex++) {
			pattern++;
			if ((rowindex == startrowindex) &&
			    (bitindex < startbitindex))
				continue;
			if ((rowindex == endrowindex) &&
			    (bitindex > endbitindex))
				continue;
			if ((rowdata >> bitindex) & 0x1)
				hammingvalue ^= pattern;
		}
	}
	mutex_unlock(&fuse_lock);
	parity = tegra_fuse_calculate_parity(hammingvalue);
	hammingcode = hammingvalue | (1 << 12) | ((parity ^ 1) << 13);
	sprintf(buf, "0x%08x\n", hammingcode);

	return strlen(buf);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
#define TEGRA210_INT_CID 5
#define TEGRA186_INT_CID 6
#define TEGRA194_INT_CID 7
#define TEGRA234_INT_CID 8

#define FUSE_OPT_VENDOR_CODE		0x100
#define FUSE_OPT_VENDOR_CODE_MASK	0xf
#define FUSE_OPT_FAB_CODE		0x104
#define FUSE_OPT_FAB_CODE_MASK		0x3f
#define FUSE_OPT_LOT_CODE_0		0x108
#define FUSE_OPT_LOT_CODE_1		0x10c
#define FUSE_OPT_WAFER_ID		0x110
#define FUSE_OPT_WAFER_ID_MASK		0x3f
#define FUSE_OPT_X_COORDINATE		0x114
#define FUSE_OPT_X_COORDINATE_MASK	0x1ff
#define FUSE_OPT_Y_COORDINATE		0x118
#define FUSE_OPT_Y_COORDINATE_MASK	0x1ff

static unsigned long long tegra_chip_uid(void)
{

	u64 uid = 0ull;
	u32 reg;
	u32 cid;
	u32 vendor;
	u32 fab;
	u32 lot;
	u32 wafer;
	u32 x;
	u32 y;
	u32 i;

	/*
	 * This used to be so much easier in prior chips. Unfortunately, there
	 * is no one-stop shopping for the unique id anymore. It must be
	 *  constructed from various bits of information burned into the fuses
	 *  during the manufacturing process. The 64-bit unique id is formed
	 *  by concatenating several bit fields. The notation used for the
	 *  various fields is <fieldname:size_in_bits> with the UID composed
	 *  thusly:
	 *
	 *  <CID:4><VENDOR:4><FAB:6><LOT:26><WAFER:6><X:9><Y:9>
	 *
	 * Where:
	 *
	 *	Field    Bits  Position Data
	 *	-------  ----  -------- ----------------------------------------
	 *	CID        4     60     Chip id
	 *	VENDOR     4     56     Vendor code
	 *	FAB        6     50     FAB code
	 *	LOT       26     24     Lot code (5-digit base-36-coded-decimal,
	 *				re-encoded to 26 bits binary)
	 *	WAFER      6     18     Wafer id
	 *	X          9      9     Wafer X-coordinate
	 *	Y          9      0     Wafer Y-coordinate
	 *	-------  ----
	 *	Total     64
	 */

	reg = tegra_get_chip_id();
	switch (reg) {
	case TEGRA210:
		cid = TEGRA210_INT_CID;
		break;
	case TEGRA186:
		cid = TEGRA186_INT_CID;
		break;
	case TEGRA194:
		cid = TEGRA194_INT_CID;
		break;
	case TEGRA234:
		cid = TEGRA234_INT_CID;
		break;
	default:
		cid = 0;
		break;
	};

	tegra_fuse_readl(FUSE_OPT_VENDOR_CODE, &reg);
	vendor = reg & FUSE_OPT_VENDOR_CODE_MASK;
	tegra_fuse_readl(FUSE_OPT_FAB_CODE, &reg);
	fab = reg & FUSE_OPT_FAB_CODE_MASK;

	/* Lot code must be re-encoded from a 5 digit base-36 'BCD' number
	 * to a binary number.
	 */
	lot = 0;
	tegra_fuse_readl(FUSE_OPT_LOT_CODE_0, &reg);
	reg = reg << 2;

	for (i = 0; i < 5; ++i) {
		u32 digit = (reg & 0xFC000000) >> 26;

		WARN_ON(digit >= 36);
		lot *= 36;
		lot += digit;
		reg <<= 6;
	}

	tegra_fuse_readl(FUSE_OPT_WAFER_ID, &reg);
	wafer = reg & FUSE_OPT_WAFER_ID_MASK;
	tegra_fuse_readl(FUSE_OPT_X_COORDINATE, &reg);
	x = reg & FUSE_OPT_X_COORDINATE_MASK;
	tegra_fuse_readl(FUSE_OPT_Y_COORDINATE, &reg);
	y = reg & FUSE_OPT_Y_COORDINATE_MASK;

	uid = ((unsigned long long)cid  << 60ull)
	    | ((unsigned long long)vendor << 56ull)
	    | ((unsigned long long)fab << 50ull)
	    | ((unsigned long long)lot << 24ull)
	    | ((unsigned long long)wafer << 18ull)
	    | ((unsigned long long)x << 9ull)
	    | ((unsigned long long)y << 0ull);
	return uid;
}

static ssize_t tegra_fuse_read_ecid(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	sprintf(buf, "%llu\n", tegra_chip_uid());
	return strlen(buf);
}
#endif

static ssize_t tegra_fuse_read_opt_tpc_disable(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	u32 reg = 0;
	int retval;

	retval = tegra_fuse_readl(TEGRA_FUSE_OPT_TPC_DISABLE, &reg);

	if (unlikely(retval)) {
		dev_err(dev, "sysfs read failed\n");
		return -EINVAL;
	}

	sprintf(buf, "0x%x\n", reg);
	return strlen(buf);
}

#define FUSE_BURN_DATA(fname, m_off, sbit, size, c_off, is_red, is_be)	\
	{								\
		.name = #fname,						\
		.start_offset = m_off,					\
		.start_bit = sbit,					\
		.size_bits = size,					\
		.reg_offset = c_off,					\
		.is_redundant = is_red,					\
		.is_big_endian = is_be,					\
		.redundant_war = false,					\
		.attr.show = tegra_fuse_show,				\
		.attr.store = tegra_fuse_store,				\
		.attr.attr.name = #fname,				\
		.attr.attr.mode = 0660,					\
	}
#define FUSE_SYSFS_DATA(fname, show_func, store_func, _mode)		\
	{								\
		.name = #fname,						\
		.attr.show = show_func,					\
		.attr.store = store_func,				\
		.attr.attr.name = #fname,				\
		.attr.attr.mode = _mode,				\
	}

static struct tegra_fuse_hw_feature tegra210_fuse_chip_data = {
	.power_down_mode = true,
	.mirroring_support = true,
	.hw_mutex_support = false,
	.has_power_switch = true,
	.pgm_time = 5,
	.burn_data = {
		FUSE_BURN_DATA(reserved_odm0, 0x2e, 17, 32, 0xc8, true, false),
		FUSE_BURN_DATA(reserved_odm1, 0x30, 17, 32, 0xcc, true, false),
		FUSE_BURN_DATA(reserved_odm2, 0x32, 17, 32, 0xd0, true, false),
		FUSE_BURN_DATA(reserved_odm3, 0x34, 17, 32, 0xd4, true, false),
		FUSE_BURN_DATA(reserved_odm4, 0x36, 17, 32, 0xd8, true, false),
		FUSE_BURN_DATA(reserved_odm5, 0x38, 17, 32, 0xdc, true, false),
		FUSE_BURN_DATA(reserved_odm6, 0x3a, 17, 32, 0xe0, true, false),
		FUSE_BURN_DATA(reserved_odm7, 0x3c, 17, 32, 0xe4, true, false),
		FUSE_BURN_DATA(odm_lock, 0, 6, 4, 0x8, true, false),
		FUSE_BURN_DATA(device_key, 0x2a, 20, 32, 0xb4, true, false),
		FUSE_BURN_DATA(arm_jtag_disable, 0x0, 12, 1, 0xb8, true, false),
		FUSE_BURN_DATA(odm_production_mode, 0x0, 11, 1, 0xa0, true, false),
		FUSE_BURN_DATA(sec_boot_dev_cfg, 0x2c, 20, 16, 0xbc, true, false),
		FUSE_BURN_DATA(sec_boot_dev_sel, 0x2e, 4, 3, 0xc0, true, false),
		FUSE_BURN_DATA(secure_boot_key, 0x22, 20, 128, 0xa4, true, false),
		FUSE_BURN_DATA(public_key, 0xc, 6, 256, 0x64, true, false),
		FUSE_BURN_DATA(pkc_disable, 0x52, 7, 1, 0x168, true, false),
		FUSE_BURN_DATA(debug_authentication, 0x5a, 19, 5, 0x1e4, true, false),
		FUSE_BURN_DATA(aid, 0x67, 2, 32, 0x1f8, false, false),
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		FUSE_SYSFS_DATA(ecid, tegra_fuse_read_ecid, NULL, FPERM_R),
#endif
		{},
	},
};

static struct tegra_fuse_hw_feature tegra186_fuse_chip_data = {
	.power_down_mode = true,
	.mirroring_support = true,
	.hw_mutex_support = false,
	.has_power_switch = true,
	.pgm_time = 5,
	.burn_data = {
		FUSE_BURN_DATA(reserved_odm0, 0x2, 2, 32, 0xc8, true, false),
		FUSE_BURN_DATA(reserved_odm1, 0x4, 2, 32, 0xcc, true, false),
		FUSE_BURN_DATA(reserved_odm2, 0x6, 2, 32, 0xd0, true, false),
		FUSE_BURN_DATA(reserved_odm3, 0x8, 2, 32, 0xd4, true, false),
		FUSE_BURN_DATA(reserved_odm4, 0xa, 2, 32, 0xd8, true, false),
		FUSE_BURN_DATA(reserved_odm5, 0xc, 2, 32, 0xdc, true, false),
		FUSE_BURN_DATA(reserved_odm6, 0xe, 2, 32, 0xe0, true, false),
		FUSE_BURN_DATA(reserved_odm7, 0x10, 2, 32, 0xe4, true, false),
		FUSE_BURN_DATA(odm_lock, 0, 6, 4, 0x8, true, false),
		FUSE_BURN_DATA(arm_jtag_disable, 0x0, 12, 1, 0xb8, true, false),
		FUSE_BURN_DATA(odm_production_mode, 0x0, 11, 1, 0xa0, true, false),
		FUSE_BURN_DATA(debug_authentication, 0x5a, 0, 5, 0x1e4, true, false),
		FUSE_BURN_DATA(boot_security_info, 0x0, 16, 6, 0x168, true, false),
		FUSE_BURN_DATA(secure_boot_key, 0x4b, 23, 128, 0xa4, false, true),
		FUSE_BURN_DATA(public_key, 0x43, 23, 256, 0x64, false, true),
		FUSE_BURN_DATA(kek0, 0x59, 22, 128, 0x2c0, false, true),
		FUSE_BURN_DATA(kek1, 0x5d, 22, 128, 0x2d0, false, true),
		FUSE_BURN_DATA(kek2, 0x61, 22, 128, 0x2e0, false, true),
		FUSE_BURN_DATA(odm_info, 0x50, 31, 16, 0x19c, false, false),
		FUSE_BURN_DATA(odm_h2, 0x67, 31, 14, 0x33c, false, false),
		FUSE_SYSFS_DATA(calc_h2, tegra_fuse_calc_h2_code, NULL,
				FPERM_RW),
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		FUSE_SYSFS_DATA(ecid, tegra_fuse_read_ecid, NULL, FPERM_R),
#endif
		{},
	},
};

static struct tegra_fuse_hw_feature tegra210b01_fuse_chip_data = {
	.power_down_mode = true,
	.mirroring_support = true,
	.hw_mutex_support = false,
	.has_power_switch = true,
	.pgm_time = 5,
	.burn_data = {
		FUSE_BURN_DATA(reserved_odm0, 0x62, 27, 32, 0xc8, true, false),
		FUSE_BURN_DATA(reserved_odm1, 0x64, 27, 32, 0xcc, true, false),
		FUSE_BURN_DATA(reserved_odm2, 0x66, 27, 32, 0xd0, true, false),
		FUSE_BURN_DATA(reserved_odm3, 0x68, 27, 32, 0xd4, true, false),
		FUSE_BURN_DATA(reserved_odm4, 0x6a, 27, 32, 0xd8, true, false),
		FUSE_BURN_DATA(reserved_odm5, 0x6c, 27, 32, 0xdc, true, false),
		FUSE_BURN_DATA(reserved_odm6, 0x6e, 27, 32, 0xe0, true, false),
		FUSE_BURN_DATA(reserved_odm7, 0x70, 27, 32, 0xe4, true, false),
		FUSE_BURN_DATA(odm_lock, 0, 6, 16, 0x8, true, false),
		FUSE_BURN_DATA(device_key, 0x5e, 30, 32, 0xb4, true, false),
		FUSE_BURN_DATA(arm_jtag_disable, 0x0, 24, 1, 0xb8, true, false),
		FUSE_BURN_DATA(odm_production_mode, 0, 23, 1, 0xa0, true, false),
		FUSE_BURN_DATA(secure_boot_key, 0x56, 30, 128, 0xa4, true, false),
		FUSE_BURN_DATA(public_key, 0x40, 15, 256, 0x64, true, false),
		FUSE_BURN_DATA(boot_security_info, 0x8c, 18, 8, 0x168, true, false),
		FUSE_BURN_DATA(debug_authentication, 0, 26, 5, 0x1e4, true, false),
		FUSE_BURN_DATA(odm_info, 0x92, 15, 16, 0x19c, true, false),
		FUSE_BURN_DATA(kek, 0x1e, 0, 128, 0xd0, true, false),
		FUSE_BURN_DATA(bek, 0x26, 0, 128, 0xe0, true, false),
		FUSE_BURN_DATA(aid, 0xa5, 2, 32, 0x1f8, false, false),
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		FUSE_SYSFS_DATA(ecid, tegra_fuse_read_ecid, NULL, FPERM_R),
#endif
		{},
	},
};

static struct tegra_fuse_hw_feature tegra194_fuse_chip_data = {
	.power_down_mode = true,
	.mirroring_support = true,
	.hw_mutex_support = false,
	.has_power_switch = true,
	.pgm_time = 5,
	.burn_data = {
		FUSE_BURN_DATA(reserved_odm0, 0x2, 2, 32, 0xc8, true, false),
		FUSE_BURN_DATA(reserved_odm1, 0x4, 2, 32, 0xcc, true, false),
		FUSE_BURN_DATA(reserved_odm2, 0x6, 2, 32, 0xd0, true, false),
		FUSE_BURN_DATA(reserved_odm3, 0x8, 2, 32, 0xd4, true, false),
		FUSE_BURN_DATA(reserved_odm4, 0xa, 2, 32, 0xd8, true, false),
		FUSE_BURN_DATA(reserved_odm5, 0xc, 2, 32, 0xdc, true, false),
		FUSE_BURN_DATA(reserved_odm6, 0xe, 2, 32, 0xe0, true, false),
		FUSE_BURN_DATA(reserved_odm7, 0x10, 2, 32, 0xe4, true, false),
		FUSE_BURN_DATA(reserved_odm8, 0x16, 26, 32, 0x420, true, false),
		FUSE_BURN_DATA(reserved_odm9, 0x18, 26, 32, 0x424, true, false),
		FUSE_BURN_DATA(reserved_odm10, 0x1a, 26, 32, 0x428, true, false),
		FUSE_BURN_DATA(reserved_odm11, 0x1c, 26, 32, 0x42c, true, false),
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0)
		FUSE_BURN_DATA(reserved_sw, 0x65, 25, 24, 0xc0, false, false),
#endif
		FUSE_BURN_DATA(odm_lock, 0, 6, 4, 0x8, true, false),
		FUSE_BURN_DATA(arm_jtag_disable, 0x0, 12, 1, 0xb8, true, false),
		FUSE_BURN_DATA(odm_production_mode, 0, 11, 1, 0xa0, true, false),
		FUSE_BURN_DATA(secure_boot_key, 0x61, 1, 128, 0xa4, false, true),
		FUSE_BURN_DATA(public_key, 0x59, 1, 256, 0x64, false, true),
		FUSE_BURN_DATA(boot_security_info, 0x66, 21, 16, 0x168, false, false),
		FUSE_BURN_DATA(debug_authentication, 0, 20, 5, 0x1e4, true, false),
		FUSE_BURN_DATA(odm_info, 0x67, 5, 16, 0x19c, false, false),
		FUSE_BURN_DATA(pdi, 0x40, 17, 64, 0x300, false, false),
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		FUSE_BURN_DATA(opt_customer_optin_fuse, 0x7e, 6, 1, 0x4a8,
			       false, false),
		FUSE_BURN_DATA(odmid, 0x7b, 30, 64, 0x308, false, false),
#endif
		FUSE_BURN_DATA(kek0, 0x6f, 30, 128, 0x2c0, false, true),
		FUSE_BURN_DATA(kek1, 0x73, 30, 128, 0x2d0, false, true),
		FUSE_BURN_DATA(kek2, 0x77, 30, 128, 0x2e0, false, true),
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		FUSE_SYSFS_DATA(ecid, tegra_fuse_read_ecid, NULL, FPERM_R),
#endif
		FUSE_SYSFS_DATA(opt_tpc_disable,
				tegra_fuse_read_opt_tpc_disable, NULL, FPERM_R),
		{},
	},
};

static struct tegra_fuse_hw_feature tegra234_fuse_chip_data = {
	.power_down_mode = true,
	.mirroring_support = true,
	.hw_mutex_support = true,
	.has_power_switch = true,
	.pgm_time = 5,
	.burn_data = {
		FUSE_BURN_DATA(reserved_odm0, 0x2, 2, 32, 0xc8, true, false),
		FUSE_BURN_DATA(reserved_odm1, 0x4, 2, 32, 0xcc, true, false),
		FUSE_BURN_DATA(reserved_odm2, 0x6, 2, 32, 0xd0, true, false),
		FUSE_BURN_DATA(reserved_odm3, 0x10, 0, 32, 0xd4, true, false),
		FUSE_BURN_DATA(reserved_odm4, 0xc, 0, 32, 0xd8, true, false),
		FUSE_BURN_DATA(reserved_odm5, 0xe, 0, 32, 0xdc, true, false),
		FUSE_BURN_DATA(reserved_odm6, 0xe, 2, 32, 0xe0, true, false),
		FUSE_BURN_DATA(reserved_odm7, 0x10, 2, 32, 0xe4, true, false),
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0)
		FUSE_BURN_DATA(reserved_sw, 0xc6, 0, 24, 0xc0, false, false),
#endif

		FUSE_BURN_DATA(odm_lock, 0, 5, 4, 0x8, true, false),
		FUSE_BURN_DATA(public_key, 0xbc, 21, 512, 0x64, false, true),
		FUSE_BURN_DATA(boot_security_info, 0xc7, 0, 32, 0x168, false, false),
		FUSE_BURN_DATA(debug_authentication, 0, 16, 5, 0x1e4, true, false),
		FUSE_BURN_DATA(odm_info, 0xc7, 9, 16, 0x19c, false, false),
		FUSE_BURN_DATA(pdi, 0x62, 29, 64, 0x300, false, false),
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		FUSE_BURN_DATA(opt_customer_optin_fuse, 0xca, 7, 1, 0x4a8,
				false, false),
		FUSE_BURN_DATA(odmid, 0xc9, 0, 64, 0x308, false, false),
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
		FUSE_SYSFS_DATA(ecid, tegra_fuse_read_ecid, NULL, FPERM_R),
#endif
		FUSE_SYSFS_DATA(opt_tpc_disable,
				tegra_fuse_read_opt_tpc_disable, NULL, FPERM_R),
		{},
	},
};

static const struct of_device_id tegra_fuse_burn_match[] = {
	{
		.compatible = "nvidia,tegra210-efuse-burn",
		.data = &tegra210_fuse_chip_data,
	}, {
		.compatible = "nvidia,tegra186-efuse-burn",
		.data = &tegra186_fuse_chip_data,
	}, {
		.compatible = "nvidia,tegra210b01-efuse-burn",
		.data = &tegra210b01_fuse_chip_data,
	}, {
		.compatible = "nvidia,tegra194-efuse-burn",
		.data = &tegra194_fuse_chip_data,
	}, {
		.compatible = "nvidia,tegra234-efuse-burn",
		.data = &tegra234_fuse_chip_data,
	}, {},
};

static void tegra_fuse_parse_dt(struct tegra_fuse_burn_dev *fuse_dev,
		struct device_node *np)
{
	int n_entries;

	n_entries = of_property_count_u32_elems(np, "nvidia,temp-range");
	if (n_entries == 2) {
		of_property_read_u32_index(np, "nvidia,temp-range",
				0, &fuse_dev->min_temp);
		of_property_read_u32_index(np, "nvidia,temp-range",
				1, &fuse_dev->max_temp);
	} else {
		dev_dbg(fuse_dev->dev, "invalid fuse-temp range entries\n");
	}
}

static int tegra_fuse_burn_probe(struct platform_device *pdev)
{
	struct tegra_fuse_burn_dev *fuse_dev;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *tz_np;
	int i, ret;

	fuse_dev = devm_kzalloc(&pdev->dev, sizeof(*fuse_dev), GFP_KERNEL);
	if (!fuse_dev)
		return -ENOMEM;

	fuse_dev->hw = (struct tegra_fuse_hw_feature *)of_device_get_match_data(
			&pdev->dev);
	if (!fuse_dev->hw) {
		dev_err(&pdev->dev, "No hw data provided\n");
		return -EINVAL;
	}

	/* Since T210, we support the bit offset and we will have redundant fuse
	 * for some of the fuse. But one exception(AID fuse) is not redundant.
	 * Unfortunately, some legacy kernel(eg. Kernel v3.10) will assume the
	 * AID fuse as redundant and read the fuse value in redundant way, from
	 * address X and address X+2, which should be address X and address X+1
	 * instead.
	 * To align the platform we release with legacy kernel and client, add
	 * the "redundant-aid-war" for reading the same value as in the past.
	 * Or the inconsistent value may cause an issue in some case.
	 */
	if (of_property_read_bool(np, "nvidia,redundant-aid-war")) {
		for (i = 0; i < ARRAY_SIZE(fuse_dev->hw->burn_data) &&
				fuse_dev->hw->burn_data[i].name != NULL; i++)
			if (!strcmp(fuse_dev->hw->burn_data[i].name, "aid")) {
				fuse_dev->hw->burn_data[i].is_redundant = true;
				fuse_dev->hw->burn_data[i].redundant_war =
					true;
			}
	}

	fuse_dev->pgm_clk = devm_clk_get(&pdev->dev, "clk_m");
	if (IS_ERR(fuse_dev->pgm_clk)) {
		if (PTR_ERR(fuse_dev->pgm_clk) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "failed to get clk_m err\n");
		return PTR_ERR(fuse_dev->pgm_clk);
	}
	fuse_dev->pgm_width = DIV_ROUND_UP(
			clk_get_rate(fuse_dev->pgm_clk) *
			fuse_dev->hw->pgm_time,
			1000 * 1000);

	fuse_dev->dev = &pdev->dev;
	platform_set_drvdata(pdev, fuse_dev);
	mutex_init(&fuse_lock);
	for (i = 0; i < ARRAY_SIZE(fuse_dev->hw->burn_data) &&
			fuse_dev->hw->burn_data[i].name != NULL; i++) {
		ret = sysfs_create_file(&pdev->dev.kobj,
					&fuse_dev->hw->burn_data[i].attr.attr);
		if (ret) {
			dev_err(&pdev->dev, "sysfs create failed %d\n", ret);
			return ret;
		}
	}
	WARN(sysfs_create_link(&platform_bus.kobj, &pdev->dev.kobj,
			"tegra-fuse"), "Unable to create symlink\n");

	device_init_wakeup(fuse_dev->dev, true);

	if (of_property_read_u32(np, "thermal-zone", &fuse_dev->thermal_zone))
		dev_info(fuse_dev->dev, "shutdown limit check disabled\n");

	tz_np = of_parse_phandle(np, "nvidia,tz", 0);
	if (tz_np) {
		fuse_dev->tz = thermal_zone_get_zone_by_node(tz_np);
		if (IS_ERR(fuse_dev->tz))
			dev_dbg(&pdev->dev, "temp zone node not available\n");
		else
			tegra_fuse_parse_dt(fuse_dev, np);
	}

	dev_info(&pdev->dev, "Fuse burn driver initialized\n");
	return 0;
}

static struct platform_driver tegra_fuse_burn_driver = {
	.driver = {
		.name = "tegra-fuse-burn",
		.of_match_table = tegra_fuse_burn_match,
	},
	.probe = tegra_fuse_burn_probe,
};
module_platform_driver(tegra_fuse_burn_driver);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
static int get_chip_uid(char *val, const struct kernel_param *kp)
{
	chip_uid = tegra_chip_uid();
	return param_get_ulong(val, kp);
}

static struct kernel_param_ops tegra_chip_uid_ops = {
	.get = get_chip_uid,
};
module_param_cb(tegra_chip_uid, &tegra_chip_uid_ops, &chip_uid, 0444);
#endif
