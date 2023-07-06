// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/dma-buf.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/mailbox_client.h>
#include <linux/tegra-epl.h>
#include <linux/pm.h>
#include <linux/kthread.h>
#include <linux/mutex.h>

/* Timeout in milliseconds */
#define TIMEOUT		5U

/* 32bit data Length */
#define MAX_LEN	4

/* Macro indicating total number of Misc Sw generic errors in Misc EC */
#define NUM_SW_GENERIC_ERR 5U

/* Error index offset in mission status register */
#define ERROR_INDEX_OFFSET	24U

enum handshake_state {
	HANDSHAKE_PENDING,
	HANDSHAKE_FAILED,
	HANDSHAKE_DONE
};

/* Data type for mailbox client and channel details */
struct epl_hsp_sm {
	struct mbox_client client;
	struct mbox_chan *chan;
};

/* Data type for accessing TOP2 HSP */
struct epl_hsp {
	struct epl_hsp_sm tx;
	struct device dev;
};

/* Data type to store Misc Sw Generic error configuration */
struct epl_misc_sw_err_cfg {
	void __iomem *err_code_va;
	void __iomem *err_assert_va;
	const char *dev_configured;
};

static int device_file_major_number;
static const char device_name[] = "epdaemon";

static struct platform_device *pdev_local;

static struct epl_hsp *epl_hsp_v;

static void __iomem *mission_err_status_va;

static bool isAddrMappOk = true;

static struct epl_misc_sw_err_cfg miscerr_cfg[NUM_SW_GENERIC_ERR];

/* State of FSI handshake */
static enum handshake_state hs_state = HANDSHAKE_PENDING;
static DEFINE_MUTEX(hs_state_mutex);

static struct task_struct *fsi_handshake_thread;

static void tegra_hsp_tx_empty_notify(struct mbox_client *cl,
					 void *data, int empty_value)
{
	pr_debug("EPL: TX empty callback came\n");
}

static int tegra_hsp_mb_init(struct device *dev)
{
	int err;

	epl_hsp_v = devm_kzalloc(dev, sizeof(*epl_hsp_v), GFP_KERNEL);
	if (!epl_hsp_v)
		return -ENOMEM;

	epl_hsp_v->tx.client.dev = dev;
	epl_hsp_v->tx.client.tx_block = true;
	epl_hsp_v->tx.client.tx_tout = TIMEOUT;
	epl_hsp_v->tx.client.tx_done = tegra_hsp_tx_empty_notify;

	epl_hsp_v->tx.chan = mbox_request_channel_byname(&epl_hsp_v->tx.client,
							"epl-tx");
	if (IS_ERR(epl_hsp_v->tx.chan)) {
		err = PTR_ERR(epl_hsp_v->tx.chan);
		dev_err(dev, "failed to get tx mailbox: %d\n", err);
		return err;
	}

	return 0;
}

static ssize_t device_file_ioctl(
			struct file *fp, unsigned int cmd, unsigned long arg)
{
	uint32_t lData[MAX_LEN];
	int ret;

	if (copy_from_user(lData, (void __user *)arg,
				 MAX_LEN * sizeof(uint32_t)))
		return -EACCES;

	switch (cmd) {

	case EPL_REPORT_ERROR_CMD:
		mutex_lock(&hs_state_mutex);
		if (hs_state == HANDSHAKE_DONE)
			ret = mbox_send_message(epl_hsp_v->tx.chan, (void *) lData);
		else
			ret = -ENODEV;
		mutex_unlock(&hs_state_mutex);

		break;
	default:
		return -EINVAL;
	}

	return ret;
}

int epl_get_misc_ec_err_status(struct device *dev, uint8_t err_number, bool *status)
{
	int ret = -EINVAL;

	if (dev && status && (err_number < NUM_SW_GENERIC_ERR)) {
		uint32_t mission_err_status = 0U;
		uint32_t mask = 0U;
		const char *dev_str;

		if (miscerr_cfg[err_number].dev_configured == NULL || isAddrMappOk == false)
			return -ENODEV;

		dev_str = dev_driver_string(dev);

		if (strcmp(dev_str, miscerr_cfg[err_number].dev_configured) != 0)
			return -EACCES;

		mask = (1U << ((ERROR_INDEX_OFFSET + err_number) % 32U));
		mission_err_status = readl(mission_err_status_va);

		if ((mission_err_status & mask) != 0U)
			*status = false;
		else
			*status = true;

		ret = 0;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(epl_get_misc_ec_err_status);

int epl_report_misc_ec_error(struct device *dev, uint8_t err_number,
	uint32_t sw_error_code)
{
	int ret = -EINVAL;
	bool status = false;

	ret = epl_get_misc_ec_err_status(dev, err_number, &status);

	if (ret != 0)
		return ret;

	if (status == false)
		return -EAGAIN;

	/* Updating error code */
	writel(sw_error_code, miscerr_cfg[err_number].err_code_va);

	/* triggering SW generic error */
	writel(0x1U, miscerr_cfg[err_number].err_assert_va);

	return 0;
}
EXPORT_SYMBOL_GPL(epl_report_misc_ec_error);

int epl_report_error(struct epl_error_report_frame error_report)
{
	int ret = -EINVAL;

	mutex_lock(&hs_state_mutex);
	if (epl_hsp_v == NULL || hs_state != HANDSHAKE_DONE) {
		mutex_unlock(&hs_state_mutex);
		return -ENODEV;
	}
	mutex_unlock(&hs_state_mutex);

	ret = mbox_send_message(epl_hsp_v->tx.chan, (void *)&error_report);

	return ret < 0 ? ret : 0;
}
EXPORT_SYMBOL_GPL(epl_report_error);

static int epl_client_fsi_handshake(void *arg)
{
	mutex_lock(&hs_state_mutex);

	if (epl_hsp_v) {
		int ret;
		const uint32_t handshake_data[] = {0x45504C48, 0x414E4453, 0x48414B45,
			0x44415441};
		const uint8_t max_retries = 3;
		uint8_t count = 0;

		do {
			ret = mbox_send_message(epl_hsp_v->tx.chan, (void *) handshake_data);

			if (ret < 0) {
				hs_state = HANDSHAKE_FAILED;
				count++;
			} else {
				hs_state = HANDSHAKE_DONE;
				break;
			}
		} while (count < max_retries || kthread_should_stop());
	}

	if (hs_state == HANDSHAKE_FAILED)
		pr_warn("epl_client: handshake with FSI failed\n");
	else
		pr_info("epl_client: handshake done with FSI\n");

	mutex_unlock(&hs_state_mutex);

	return 0;
}

static int __maybe_unused epl_client_suspend(struct device *dev)
{
	pr_debug("tegra-epl: suspend called\n");

	mutex_lock(&hs_state_mutex);
	hs_state = HANDSHAKE_PENDING;
	mutex_unlock(&hs_state_mutex);

	return 0;
}

static int __maybe_unused epl_client_resume(struct device *dev)
{
	pr_debug("tegra-epl: resume called\n");

	fsi_handshake_thread = kthread_run(epl_client_fsi_handshake, NULL, "fsi-hs");

	if (IS_ERR(fsi_handshake_thread))
		return PTR_ERR(fsi_handshake_thread);

	return 0;
}
static SIMPLE_DEV_PM_OPS(epl_client_pm, epl_client_suspend, epl_client_resume);

static const struct of_device_id epl_client_dt_match[] = {
	{ .compatible = "nvidia,tegra234-epl-client"},
	{}
};

MODULE_DEVICE_TABLE(of, epl_client_dt_match);

/* File operations */
static const struct file_operations epl_driver_fops = {
	.owner   = THIS_MODULE,
	.unlocked_ioctl   = device_file_ioctl,
};

static int epl_register_device(void)
{
	int result = 0;
	struct class *dev_class;

	result = register_chrdev(0, device_name, &epl_driver_fops);
	if (result < 0) {
		pr_err("%s> register_chrdev code = %i\n", device_name, result);
		return result;
	}
	device_file_major_number = result;
	dev_class = class_create(THIS_MODULE, device_name);
	if (dev_class == NULL) {
		pr_err("%s> Could not create class for device\n", device_name);
		goto class_fail;
	}

	if ((device_create(dev_class, NULL,
		MKDEV(device_file_major_number, 0),
			 NULL, device_name)) == NULL) {
		pr_err("%s> Could not create device node\n", device_name);
		goto device_failure;
	}
	return 0;

device_failure:
	class_destroy(dev_class);
class_fail:
	unregister_chrdev(device_file_major_number, device_name);
	return -1;
}

static void epl_unregister_device(void)
{
	if (device_file_major_number != 0)
		unregister_chrdev(device_file_major_number, device_name);
}

static int epl_client_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	const struct device_node *np = dev->of_node;
	int iterator = 0;
	char name[32] = "client-misc-sw-generic-err";

	mutex_lock(&hs_state_mutex);
	hs_state = HANDSHAKE_PENDING;
	mutex_unlock(&hs_state_mutex);

	epl_register_device();
	ret = tegra_hsp_mb_init(dev);
	pdev_local = pdev;

	for (iterator = 0; iterator < NUM_SW_GENERIC_ERR; iterator++) {
		name[26] = (char)(iterator+48U);
		name[27] = '\0';
		if (of_property_read_string(np, name, &miscerr_cfg[iterator].dev_configured) == 0) {
			pr_info("Misc Sw Generic Err #%d configured to client %s\n",
					iterator, miscerr_cfg[iterator].dev_configured);

			/* Mapping registers to process address space */
			miscerr_cfg[iterator].err_code_va =
				devm_platform_ioremap_resource(pdev, (iterator * 2));
			miscerr_cfg[iterator].err_assert_va =
				devm_platform_ioremap_resource(pdev, (iterator * 2) + 1);

			if (IS_ERR(miscerr_cfg[iterator].err_code_va) ||
					IS_ERR(miscerr_cfg[iterator].err_assert_va)) {
				isAddrMappOk = false;
				ret = -1;
				dev_err(&pdev->dev, "error in mapping misc err register for err #%d\n",
						iterator);
			}
		} else {
			pr_info("Misc Sw Generic Err %d not configured for any client\n", iterator);
		}
	}

	mission_err_status_va = devm_platform_ioremap_resource(pdev, NUM_SW_GENERIC_ERR * 2);
	if (IS_ERR(mission_err_status_va)) {
		isAddrMappOk = false;
		dev_err(&pdev->dev, "error in mapping mission error status register\n");
		return PTR_ERR(mission_err_status_va);
	}

	if (ret == 0) {
		fsi_handshake_thread = kthread_run(epl_client_fsi_handshake, NULL, "fsi-hs");

		if (IS_ERR(fsi_handshake_thread))
			return PTR_ERR(fsi_handshake_thread);
	}

	return ret;
}

static int epl_client_remove(struct platform_device *pdev)
{
	epl_unregister_device();
	return 0;
}

static struct platform_driver epl_client = {
	.driver         = {
	.name   = "epl_client",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table = of_match_ptr(epl_client_dt_match),
		.pm = pm_ptr(&epl_client_pm),
	},
	.probe			= epl_client_probe,
	.remove			= epl_client_remove,
};

module_platform_driver(epl_client);

MODULE_DESCRIPTION("tegra: Error Propagation Library driver");
MODULE_AUTHOR("Prashant Shaw <pshaw@nvidia.com>");
MODULE_LICENSE("GPL v2");
