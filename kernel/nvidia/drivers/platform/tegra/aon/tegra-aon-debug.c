/*
 * Copyright (c) 2015-2022, NVIDIA CORPORATION. All rights reserved.
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
 * You should have eeceived a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/tegra-aon.h>
#include <linux/mailbox_client.h>
#include <linux/uaccess.h>
#include <linux/completion.h>
#include <linux/jiffies.h>
#include <linux/firmware.h>
#include <linux/tegra-firmwares.h>
#include <linux/tegra-cache.h>

#include <aon.h>

#include "aon-ivc-dbg-messages.h"

#define AON_REQUEST_MASK	0xF
#define AON_REQUESTS_TOTAL	(AON_REQUEST_TYPE_MAX + 1)

#define TX_BLOCK_PERIOD 20

#define AON_ROOT	0
#define AON_MODS	1
#define AON_ADCC	2

#define IVC_DBG_CH_FRAME_SIZE 64
#define MODS_DEFAULT_VAL	  0xFFFF
#define MODS_DEFAULT_LOOPS	  10
#define MODS_DEFAULT_CHANS	  0x1
#define MODS_BASIC_TEST		  0x0
#define MODS_DMA_MEM2MEM	  0x1
#define MODS_DMA_IO2MEM		  0x2
#define MODS_ADCC_SINGLE	  0x3
#define MODS_ADCC_CONT		  0x4

#define ADCC_MODE_SINGLE_SHOT 1
#define ADCC_MODE_CONT		  0
#define ADCC_CLK_SRC_OSC	  0
#define ADCC_CLK_SRC_PLLP	  1

#define AONFW_BOOT	1

struct tegra_aondbg {
	struct device *dev;
	struct tegra_aon *aon;
	struct mbox_client cl;
	struct mbox_chan *mbox;
	struct dentry *aon_root;
	bool supports_adcc;
};

static struct tegra_aondbg aondbg_dev;

struct aon_dbgfs_node {
	char *name;
	u32 id;
	u8 pdr_id;
	mode_t mode;
	struct completion *wait_on;
	const struct file_operations *fops;
	char data[IVC_DBG_CH_FRAME_SIZE];
};

struct dbgfs_dir {
	const char *name;
	struct dentry *dir;
	struct dbgfs_dir *parent;
};

static struct dbgfs_dir aon_dbgfs_dirs[] = {
	{.name = "aon", .parent = NULL},
	{.name = "aon_mods", .parent = &aon_dbgfs_dirs[AON_ROOT]},
	{.name = "adcc", .parent = &aon_dbgfs_dirs[AON_MODS]}
};

static u32 mods_result		 = MODS_DEFAULT_VAL;
static u32 mods_dma_chans	 = MODS_DEFAULT_CHANS;
static u32 mods_adcc_chans	 = MODS_DEFAULT_CHANS;
static u32 mods_case		 = MODS_BASIC_TEST;
static u32 mods_loops		 = MODS_DEFAULT_LOOPS;
static u32 mods_adcc_smpl_dur	 = 16;
static u32 mods_adcc_avg_window  = 1024;
static u32 mods_adcc_clk_src	 = ADCC_CLK_SRC_OSC;
static u64 mods_adcc_chans_data;
static u64 mods_adcc_dac_lb_data;

static unsigned int completion_timeout = 50;

static bool aon_boot_done;

static DEFINE_MUTEX(aon_mutex);
static DEFINE_SPINLOCK(mods);
static DEFINE_SPINLOCK(completion);
static DEFINE_SPINLOCK(loops);
static DEFINE_SPINLOCK(mods_dma);
static DEFINE_SPINLOCK(mods_adcc);

static struct aon_dbgfs_node aon_nodes[];

static void set_mods_result(u32 result)
{
	spin_lock(&mods);
	mods_result = result;
	spin_unlock(&mods);
}

static unsigned int get_mods_result(void)
{
	u32 val;

	spin_lock(&mods);
	val = mods_result;
	spin_unlock(&mods);

	return val;
}

static void set_completion_timeout(unsigned int timeout)
{
	spin_lock(&completion);
	completion_timeout = timeout;
	spin_unlock(&completion);
}

static unsigned int get_completion_timeout(void)
{
	unsigned int val;

	spin_lock(&completion);
	val = completion_timeout;
	spin_unlock(&completion);

	return val;
}

static void set_mods_loops(u32 count)
{
	spin_lock(&loops);
	mods_loops = count;
	spin_unlock(&loops);
}

static unsigned int get_mods_loops(void)
{
	unsigned int val;

	spin_lock(&loops);
	val = mods_loops;
	spin_unlock(&loops);

	return val;
}

static void set_mods_dma_chans(u32 dma_chans)
{
	spin_lock(&mods_dma);
	mods_dma_chans = dma_chans;
	spin_unlock(&mods_dma);
}

static unsigned int get_mods_dma_chans(void)
{
	unsigned int val;

	spin_lock(&mods_dma);
	val = mods_dma_chans;
	spin_unlock(&mods_dma);

	return val;
}

static void set_mods_adcc_chans(u64 chans)
{
	spin_lock(&mods_adcc);
	mods_adcc_chans = (u32) (chans & 0xFFFFFFFFUL);
	spin_unlock(&mods_adcc);
}

static unsigned int get_mods_adcc_chans(void)
{
	unsigned int val;

	spin_lock(&mods_adcc);
	val = mods_adcc_chans;
	spin_unlock(&mods_adcc);

	return val;
}

static void set_mods_adcc_smpl_dur(u64 dur)
{
	spin_lock(&mods_adcc);
	mods_adcc_smpl_dur = (u32)(dur & 0xFFFFFFFFUL);
	spin_unlock(&mods_adcc);
}

static unsigned int get_mods_adcc_smpl_dur(void)
{
	unsigned int val;

	spin_lock(&mods_adcc);
	val = mods_adcc_smpl_dur;
	spin_unlock(&mods_adcc);

	return val;
}

static void set_mods_adcc_avg_window(u64 avg)
{
	spin_lock(&mods_adcc);
	mods_adcc_avg_window = (u32) (avg & 0xFFFFFFFFUL);
	spin_unlock(&mods_adcc);
}

static unsigned int get_mods_adcc_avg_window(void)
{
	unsigned int val;

	spin_lock(&mods_adcc);
	val = mods_adcc_avg_window;
	spin_unlock(&mods_adcc);

	return val;
}

static void set_mods_adcc_clk_src(u64 src)
{
	spin_lock(&mods_adcc);
	mods_adcc_clk_src = (u32)(src & 0xFFFFFFFFUL);
	spin_unlock(&mods_adcc);
}

static u64 get_mods_adcc_chans_data(void)
{
	u32 val;

	spin_lock(&mods_adcc);
	val = (u32)(mods_adcc_chans_data & 0xFFFFFFFFUL);
	spin_unlock(&mods_adcc);

	return val;
}

static void set_mods_adcc_chans_data(u64 adcc_data)
{
	spin_lock(&mods_adcc);
	mods_adcc_chans_data = adcc_data;
	spin_unlock(&mods_adcc);
}

static u64 get_mods_adcc_dac_lb_data(void)
{
	u64 val;

	spin_lock(&mods_adcc);
	val = mods_adcc_dac_lb_data;
	spin_unlock(&mods_adcc);

	return val;
}

static void set_mods_adcc_dac_lb_data(u64 lb_data)
{
	spin_lock(&mods_adcc);
	mods_adcc_dac_lb_data = lb_data;
	spin_unlock(&mods_adcc);
}

static unsigned int get_mods_adcc_clk_src(void)
{
	unsigned int val;

	spin_lock(&mods_adcc);
	val = mods_adcc_clk_src;
	spin_unlock(&mods_adcc);

	return val;
}

static void aon_create_mods_req(struct aon_dbg_request *req, u32 data)
{
	switch (data) {
	case MODS_BASIC_TEST:
		break;
	case MODS_DMA_MEM2MEM:
	case MODS_DMA_IO2MEM:
		req->data.mods_req.dma_chans = get_mods_dma_chans();
		break;
	case MODS_ADCC_SINGLE:
		req->data.mods_req.adcc.chans = get_mods_adcc_chans();
		req->data.mods_req.adcc.mode = ADCC_MODE_SINGLE_SHOT;
		req->data.mods_req.adcc.sampling_dur = get_mods_adcc_smpl_dur();
		req->data.mods_req.adcc.avg_window = get_mods_adcc_avg_window();
		req->data.mods_req.adcc.clk_src = get_mods_adcc_clk_src();
		req->data.mods_req.adcc.lb_data = get_mods_adcc_dac_lb_data();
		break;
	case MODS_ADCC_CONT:
		req->data.mods_req.adcc.chans = get_mods_adcc_chans();
		req->data.mods_req.adcc.mode = ADCC_MODE_CONT;
		req->data.mods_req.adcc.sampling_dur = get_mods_adcc_smpl_dur();
		req->data.mods_req.adcc.avg_window = get_mods_adcc_avg_window();
		req->data.mods_req.adcc.clk_src = get_mods_adcc_clk_src();
		req->data.mods_req.adcc.lb_data = get_mods_adcc_dac_lb_data();
		break;
	}
}

static struct aon_dbg_response *aon_create_ivc_dbg_req(u32 request,
							   u32 flag,
							   u32 data)
{
	struct tegra_aondbg *aondbg = &aondbg_dev;
	struct aon_dbg_request req;
	struct aon_dbg_response *resp;
	struct tegra_aon_mbox_msg msg;
	int ret = 0;
	unsigned int timeout = 0;

	if (aondbg->dev == NULL)
		return (void *)-EPROBE_DEFER;

	req.req_type = request & AON_REQUEST_MASK;
	switch (req.req_type) {
	case AON_MODS_CASE:
		req.data.mods_req.loops = get_mods_loops();
		req.data.mods_req.mods_case = data;
		aon_create_mods_req(&req, data);
		break;
	case AON_MODS_CRC:
	case AON_PING:
		break;
	case AON_QUERY_TAG:
		break;
	default:
		dev_err(aondbg->dev, "Invalid aon dbg request\n");
		return ERR_PTR(-EINVAL);
	}

	msg.length = sizeof(struct aon_dbg_request);
	msg.data = (void *)&req;
	ret = mbox_send_message(aondbg->mbox, (void *)&msg);
	if (ret < 0) {
		dev_err(aondbg->dev, "mbox_send_message failed\n");
		return ERR_PTR(ret);
	}
	timeout = get_completion_timeout();
	ret = wait_for_completion_timeout(aon_nodes[req.req_type].wait_on,
				msecs_to_jiffies(timeout));
	if (!ret) {
		dev_err(aondbg->dev, "No response\n");
		return ERR_PTR(-ETIMEDOUT);
	}
	resp = (void *)aon_nodes[req.req_type].data;
	if (resp->resp_type > AON_REQUEST_TYPE_MAX) {
		dev_err(aondbg->dev, "Invalid aon dbg response\n");
		return ERR_PTR(-EIO);
	}
	if (resp->status != AON_DBG_STATUS_OK) {
		dev_err(aondbg->dev, "Request failed\n");
		return ERR_PTR(-EIO);
	}

	return resp;
}

static int load_aon_fw(struct tegra_aon *aon)
{
	const struct firmware *fw;
	struct aon_platform_data *pdata = NULL;
	int ret;

	pdata = dev_get_drvdata(aon->dev);

	ret = request_firmware(&fw, pdata->fw_name, aon->dev);
	if (ret)
		goto exit;

	memcpy((u8 *)aon->fw->data, (u8 *)fw->data, fw->size);
	release_firmware(fw);
	tegra_flush_cache_all();

exit:
	return ret;
}

static int boot_aonfw(struct tegra_aon *aon)
{
	int err = 0;

	err = tegra_aon_reset(aon);
	if (err)
		goto exit;

	err = tegra_aon_ipc_init(aon);
	if (err)
		goto exit;

exit:
	return err;
}

static int aon_boot_show(void *data, u64 *val)
{
	struct tegra_aon *aon = aondbg_dev.aon;

	if (aon->ast_config_complete &&
		aon->reset_complete && aon->load_complete)
		*val = 1;

	return 0;
}

static int aon_boot_store(void *data, u64 val)
{
	struct tegra_aon *aon = aondbg_dev.aon;
	int ret = 0;


	switch (val) {
	case AONFW_BOOT:
		if (!aon_boot_done) {
			ret = tegra_aon_ast_config(aon);
			if (ret) {
				dev_err(aon->dev, "AST config failed\n");
				goto exit;
			}

			ret = load_aon_fw(aon);
			if (ret) {
				dev_err(aon->dev, "AON fw load failed\n");
				goto exit;
			}

			ret = boot_aonfw(aon);
			aon_boot_done = (ret == 0) ? true : false;
		}
		break;

	default:
		ret = -EINVAL;
		break;
	}

exit:
	return ret;
}

DEFINE_SIMPLE_ATTRIBUTE(aon_boot_fops, aon_boot_show,
			aon_boot_store, "%lld\n");

static ssize_t aon_get_fwtag(u32 context, char **data)
{
	struct aon_dbg_response *resp;
	int ret = 0;

	*data = NULL;
	resp = aon_create_ivc_dbg_req(context, READ, 0);
	if (IS_ERR(resp))
		ret = PTR_ERR(resp);
	else
		*data = resp->data.tag_resp.tag;

	return ret;
}

static int aon_tag_show(struct seq_file *file, void *param)
{
	char *data;
	int ret;

	mutex_lock(&aon_mutex);
	ret = aon_get_fwtag(*(u32 *)file->private, &data);
	if (ret >= 0)
		seq_printf(file, "%s\n", data);
	mutex_unlock(&aon_mutex);

	return ret;
}

static ssize_t aon_version_show(struct device *dev, char *buf, size_t size)
{
	char *data;
	int ret = 0;

	mutex_lock(&aon_mutex);
	ret = aon_get_fwtag(AON_QUERY_TAG, &data);
	if (ret < 0)
		ret = snprintf(buf, size, "error retrieving version: %d", ret);
	else
		ret = snprintf(buf, size, "%s", data ? data : "unavailable");
	mutex_unlock(&aon_mutex);

	return ret;
}


static int aon_tag_open(struct inode *inode, struct file *file)
{
	return single_open(file, aon_tag_show, inode->i_private);
}

static const struct file_operations aon_tag_fops = {
	.open = aon_tag_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

static int __aon_do_ping(void)
{
	struct aon_dbg_response *resp;
	int challenge = 8;
	int ret = 0;

	resp = aon_create_ivc_dbg_req(AON_PING, READ, challenge);
	if (IS_ERR(resp)) {
		ret = PTR_ERR(resp);
		goto exit;
	}

	ret = resp->data.ping_resp.reply;
	if (ret != challenge * 2)
		ret = -EINVAL;

exit:
	return ret;
}

static int aon_ping_show(void *data, u64 *val)
{
	ktime_t tm;
	int ret = 0;

	mutex_lock(&aon_mutex);
	tm = ktime_get();
	ret = __aon_do_ping();
	tm = ktime_sub(ktime_get(), tm);
	*val = (ret < 0) ? tm : ret;
	mutex_unlock(&aon_mutex);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(aon_ping_fops, aon_ping_show,
			NULL, "%lld\n");

static int aon_mods_loops_show(void *data, u64 *val)
{
	*val = get_mods_loops();

	return 0;
}

static int aon_mods_loops_store(void *data, u64 val)
{
	set_mods_loops(val);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(aon_mods_loops_fops, aon_mods_loops_show,
			aon_mods_loops_store, "%lld\n");

static int aon_mods_case_show(void *data, u64 *val)
{
	*val = mods_case;

	return 0;
}

static int aon_mods_case_store(void *data, u64 val)
{
	struct aon_dbg_response *resp;
	int ret = 0;
	int i;
	u64 adcc_data = 0U;
	u64 ch_data = 0U;

	if (val > MODS_ADCC_CONT) {
		ret = -1;
		dev_err(aondbg_dev.dev, "Invalid mods case\n");
		goto out;
	}

	if (val > MODS_DMA_IO2MEM && val <= MODS_ADCC_CONT) {
		if (!aondbg_dev.supports_adcc) {
			ret = -1;
			dev_err(aondbg_dev.dev, "no adcc on this platform\n");
			goto out;
		}
	}

	mutex_lock(&aon_mutex);
	set_mods_result(MODS_DEFAULT_VAL);
	resp = aon_create_ivc_dbg_req(*(u32 *)data, WRITE, val);
	if (IS_ERR(resp)) {
		ret = PTR_ERR(resp);
	} else {
		set_mods_result(resp->status);
		if (val == MODS_ADCC_SINGLE || val == MODS_ADCC_CONT) {
			adcc_data = 0U;
			for (i = 0; i < ADCC_NCHANS; i++) {
				ch_data = resp->data.adcc_resp.ch_data[i];
				adcc_data |= (ch_data & 0x3FFU) << (i * 10);
			}
			set_mods_adcc_chans_data(adcc_data);
		}
	}
	mutex_unlock(&aon_mutex);

out:
	return ret;
}

DEFINE_SIMPLE_ATTRIBUTE(aon_mods_case_fops, aon_mods_case_show,
			aon_mods_case_store, "%lld\n");

static int aon_mods_result_show(void *data, u64 *val)
{
	*val = get_mods_result();

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(aon_mods_result_fops, aon_mods_result_show,
			NULL, "%lld\n");

static int aon_mods_crc_show(void *data, u64 *val)
{
	struct aon_dbg_response *resp;
	int ret = 0;

	mutex_lock(&aon_mutex);
	resp = aon_create_ivc_dbg_req(*(u32 *)data, READ, 0);
	if (IS_ERR(resp))
		ret = PTR_ERR(resp);
	else
		*val = resp->data.crc_resp.crc;
	mutex_unlock(&aon_mutex);

	return ret;
}

DEFINE_SIMPLE_ATTRIBUTE(aon_mods_crc_fops, aon_mods_crc_show,
			NULL, "%llx\n");

static int aon_mods_dma_show(void *data, u64 *val)
{
	*val = get_mods_dma_chans();

	return 0;
}

static int aon_mods_dma_store(void *data, u64 val)
{
	set_mods_dma_chans(val);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(aon_mods_dma_fops, aon_mods_dma_show,
			aon_mods_dma_store, "%lld\n");

static int aon_mods_adcc_chans_show(void *data, u64 *val)
{
	*val = get_mods_adcc_chans();

	return 0;
}

static int aon_mods_adcc_chans_store(void *data, u64 val)
{
	set_mods_adcc_chans(val);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(aon_mods_adcc_chans_fops, aon_mods_adcc_chans_show,
			aon_mods_adcc_chans_store, "%lld\n");

static int aon_mods_adcc_smpl_show(void *data, u64 *val)
{
	*val = get_mods_adcc_smpl_dur();

	return 0;
}

static int aon_mods_adcc_smpl_store(void *data, u64 val)
{
	set_mods_adcc_smpl_dur(val);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(aon_mods_adcc_smpl_fops, aon_mods_adcc_smpl_show,
			aon_mods_adcc_smpl_store, "%lld\n");

static int aon_mods_adcc_avg_show(void *data, u64 *val)
{
	*val = get_mods_adcc_avg_window();

	return 0;
}

static int aon_mods_adcc_avg_store(void *data, u64 val)
{
	set_mods_adcc_avg_window(val);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(aon_mods_adcc_avg_fops, aon_mods_adcc_avg_show,
			aon_mods_adcc_avg_store, "%lld\n");

static int aon_mods_adcc_clk_show(void *data, u64 *val)
{
	*val = get_mods_adcc_clk_src();

	return 0;
}

static int aon_mods_adcc_clk_store(void *data, u64 val)
{
	set_mods_adcc_clk_src(val);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(aon_mods_adcc_clk_fops, aon_mods_adcc_clk_show,
			aon_mods_adcc_clk_store, "%lld\n");

static int aon_mods_adcc_data_show(void *data, u64 *val)
{
	*val = get_mods_adcc_chans_data();

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(aon_mods_adcc_data_fops, aon_mods_adcc_data_show,
			NULL, "%lld\n");

static int aon_mods_adcc_dac_show(void *data, u64 *val)
{
	*val = get_mods_adcc_dac_lb_data();

	return 0;
}

static int aon_mods_adcc_dac_store(void *data, u64 val)
{
	set_mods_adcc_dac_lb_data(val);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(aon_mods_adcc_dac_fops, aon_mods_adcc_dac_show,
			aon_mods_adcc_dac_store, "%lld\n");

static int aon_timeout_show(void *data, u64 *val)
{
	*val = get_completion_timeout();

	return 0;
}

static int aon_timeout_store(void *data, u64 val)
{
	set_completion_timeout(val);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(aon_timeout_fops, aon_timeout_show,
			aon_timeout_store, "%lld\n");

static struct aon_dbgfs_node aon_nodes[] = {
	{.name = "boot", .id = AON_BOOT, .pdr_id = AON_ROOT,
			.mode = 0644, .fops = &aon_boot_fops, },
	{.name = "loops", .pdr_id = AON_MODS,
			.mode = 0644, .fops = &aon_mods_loops_fops, },
	{.name = "result", .id = AON_MODS_RESULT, .pdr_id = AON_MODS,
			.mode = 0444, .fops = &aon_mods_result_fops,},
	{.name = "crc", .id = AON_MODS_CRC, .pdr_id = AON_MODS,
			.mode = 0444, .fops = &aon_mods_crc_fops,},
	{.name = "case", .id = AON_MODS_CASE, .pdr_id = AON_MODS,
			.mode = 0644, .fops = &aon_mods_case_fops,},
	{.name = "dma_channels", .pdr_id = AON_MODS,
			.mode = 0644, .fops = &aon_mods_dma_fops,},
	{.name = "adcc_chans", .pdr_id = AON_ADCC,
			.mode = 0644, .fops = &aon_mods_adcc_chans_fops,},
	{.name = "sampling_dur", .pdr_id = AON_ADCC,
			.mode = 0644, .fops = &aon_mods_adcc_smpl_fops,},
	{.name = "avg_window", .pdr_id = AON_ADCC,
			.mode = 0644, .fops = &aon_mods_adcc_avg_fops,},
	{.name = "clk_src", .pdr_id = AON_ADCC,
			.mode = 0644, .fops = &aon_mods_adcc_clk_fops,},
	{.name = "adcc_data", .pdr_id = AON_ADCC,
			.mode = 0644, .fops = &aon_mods_adcc_data_fops,},
	{.name = "dac", .pdr_id = AON_ADCC,
			.mode = 0644, .fops = &aon_mods_adcc_dac_fops,},
	{.name = "ping", .id = AON_PING, .pdr_id = AON_ROOT,
			.mode = 0644, .fops = &aon_ping_fops,},
	{.name = "tag", .id = AON_QUERY_TAG, .pdr_id = AON_ROOT,
			.mode = 0644, .fops = &aon_tag_fops,},
	{.name = "completion_timeout", .pdr_id = AON_ROOT,
			.mode = 0644, .fops = &aon_timeout_fops,},
};

static void tegra_aondbg_recv_msg(struct mbox_client *cl, void *rx_msg)
{
	struct tegra_aon_mbox_msg *msg;
	struct aon_dbg_response *resp;

	msg = (struct tegra_aon_mbox_msg *)rx_msg;
	resp = (void *)msg->data;
	if (resp->resp_type > AON_REQUEST_TYPE_MAX) {
		dev_err(aondbg_dev.dev,
			"Multiple request types in 1 response\n");
		return;
	}
	memcpy(aon_nodes[resp->resp_type].data, msg->data,
					IVC_DBG_CH_FRAME_SIZE);
	complete(aon_nodes[resp->resp_type].wait_on);
}

static int aon_dbg_init(struct tegra_aondbg *aon)
{
	struct dentry *d;
	struct dentry *parent_dir;
	struct dbgfs_dir *dbgdir;
	int i;

	d = debugfs_create_dir(aon_dbgfs_dirs[AON_ROOT].name, NULL);
	if (IS_ERR_OR_NULL(d))
		goto clean;

	aon_dbgfs_dirs[AON_ROOT].dir = d;
	aon->aon_root = d;

	for (i = 1; i < ARRAY_SIZE(aon_dbgfs_dirs); i++) {
		dbgdir = &aon_dbgfs_dirs[i];
		d = debugfs_create_dir(dbgdir->name, dbgdir->parent->dir);
		if (IS_ERR_OR_NULL(d))
			goto clean;

		dbgdir->dir = d;
	}

	for (i = 0; i < ARRAY_SIZE(aon_nodes); i++) {
		parent_dir = aon_dbgfs_dirs[aon_nodes[i].pdr_id].dir;
		d = debugfs_create_file(aon_nodes[i].name,
					aon_nodes[i].mode,
					parent_dir,
					&aon_nodes[i].id,
					aon_nodes[i].fops);
		if (IS_ERR_OR_NULL(d))
			goto clean;
	}

	return 0;

clean:
	debugfs_remove_recursive(aon->aon_root);
	return PTR_ERR(d);
}

int tegra_aon_debugfs_create(struct tegra_aon *aon)
{
	struct tegra_aondbg *aondbg = &aondbg_dev;
	struct device *dev = aon->dev;
	struct device_node *np = dev->of_node;
	int count;
	int ret = 0;
	int i;

	if (!debugfs_initialized()) {
		ret = -ENODEV;
		goto exit;
	}

	if (np == NULL) {
		dev_err(dev, "tegra-aondbg: DT data required.\n");
		ret = -EINVAL;
		goto exit;
	}

	count = of_count_phandle_with_args(np, "mboxes", "#mbox-cells");
	if (count != 1) {
		dev_err(dev, "incorrect mboxes property in '%pOF'\n", np);
		ret = -EINVAL;
		goto exit;
	}

	if (of_property_read_bool(np, NV("adcc")))
		aondbg->supports_adcc = true;
	else
		aondbg->supports_adcc = false;

	aondbg->dev = aon->dev;
	aondbg->aon = aon;
	aondbg->cl.dev = aon->dev;
	aondbg->cl.tx_block = true;
	aondbg->cl.tx_tout = TX_BLOCK_PERIOD;
	aondbg->cl.knows_txdone = false;
	aondbg->cl.rx_callback = tegra_aondbg_recv_msg;
	aondbg->mbox = mbox_request_channel(&aondbg->cl, 0);
	if (IS_ERR(aondbg->mbox)) {
		ret = PTR_ERR(aondbg->mbox);
		if (ret != -EPROBE_DEFER) {
			dev_warn(dev,
				 "can't get mailbox channel (%d)\n", ret);
		}
		goto exit;
	}
	dev_dbg(dev, "aondbg->mbox = %p\n", aondbg->mbox);

	for (i = 0; i < ARRAY_SIZE(aon_nodes); i++) {
		aon_nodes[i].wait_on = devm_kzalloc(aon->dev,
							sizeof(struct completion),
							GFP_KERNEL);
		if (!aon_nodes[i].wait_on) {
			dev_err(dev, "out of memory.\n");
			ret = -ENOMEM;
			goto exit;
		}
		init_completion(aon_nodes[i].wait_on);
	}

	ret = aon_dbg_init(aondbg);
	if (ret) {
		dev_err(dev, "failed to create debugfs nodes.\n");
		goto exit;
	}

	devm_tegrafw_register(dev, "aon", TFW_NORMAL, aon_version_show, NULL);

exit:
	return ret;
}

void tegra_aon_debugfs_remove(struct tegra_aon *aon)
{
	struct tegra_aondbg *aondbg = &aondbg_dev;

	mbox_free_channel(aondbg->mbox);
	debugfs_remove_recursive(aondbg->aon_root);
}
