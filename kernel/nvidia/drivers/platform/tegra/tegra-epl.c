/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/**
 * @file tegra-epl.c
 * @brief <b> epl driver to communicate with eplcom daemon</b>
 *
 * This file will register as client driver so that EPL client can
 * report SW error to FSI using HSP mailbox from user space
 */

/* ==================[Includes]============================================= */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/dma-buf.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/mailbox_client.h>
#include <linux/sched/signal.h>
#include "linux/tegra-epl.h"
#include "uapi/linux/tegra-epl.h"
#include <linux/pm.h>

/*Timeout in millisec*/
#define TIMEOUT		1000

/*32bit data Length*/
#define MAX_LEN	4

/* Macro indicating misc register width */
#define MISC_REG_WIDTH    4U

/* Macro indiciating total number of Misc Sw generic errors in Misc EC */
#define NUM_SW_GENERIC_ERR 5U

/* Macro for Misc register access, because of guardword check could not
 * include the hw headers here.
 */

/* Macro for Misc EC mission error status register address */
#define MISC_EC_ERRSLICE0_MISSIONERR_STATUS_0   0x024e0038U

/* Macro for Misc registers */

#define MISCREG_MISC_EC_ERR0_SW_ERR_CODE_0      0x00110000U
#define MISCREG_MISC_EC_ERR0_SW_ERR_ASSERT_0    0x00110004U

#define MISCREG_MISC_EC_ERR1_SW_ERR_CODE_0      0x00120000U
#define MISCREG_MISC_EC_ERR1_SW_ERR_ASSERT_0    0x00120004U

#define MISCREG_MISC_EC_ERR2_SW_ERR_CODE_0      0x00130000U
#define MISCREG_MISC_EC_ERR2_SW_ERR_ASSERT_0    0x00130004U

#define MISCREG_MISC_EC_ERR3_SW_ERR_CODE_0      0x00140000U
#define MISCREG_MISC_EC_ERR3_SW_ERR_ASSERT_0    0x00140004U

#define MISCREG_MISC_EC_ERR4_SW_ERR_CODE_0      0x00150000U
#define MISCREG_MISC_EC_ERR4_SW_ERR_ASSERT_0    0x00150004U

/* =================[Data types]======================================== */

/*Data type for mailbox client and channel details*/
struct epl_hsp_sm {
	struct mbox_client client;
	struct mbox_chan *chan;
};

/*Data type for accessing TOP2 HSP */
struct epl_hsp {
	struct epl_hsp_sm tx;
	struct device dev;
};

/* Data type to store Misc Sw Generic error configuration */
struct epl_misc_sw_err_cfg {
	uint32_t err_code_phyaddr;
	uint32_t err_assert_phy_addr;
	void __iomem *err_code_va;
	void __iomem *err_assert_va;
	const char *dev_configured;
	uint8_t ec_err_idx;
};

/* =================[GLOBAL variables]================================== */
static ssize_t device_file_ioctl(
		struct file *, unsigned int cmd, unsigned long arg);

static int device_file_major_number;
static const char device_name[] = "epdaemon";

static struct platform_device *pdev_local;

static struct epl_hsp *epl_hsp_v;

static void __iomem *mission_err_status_va;

static bool isAddrMappOk = true;

static struct epl_misc_sw_err_cfg miscerr_cfg[NUM_SW_GENERIC_ERR] = {
	{
		.err_code_phyaddr = MISCREG_MISC_EC_ERR0_SW_ERR_CODE_0,
		.err_assert_phy_addr = MISCREG_MISC_EC_ERR0_SW_ERR_ASSERT_0,
		.ec_err_idx = 24U,
	},
	{
		.err_code_phyaddr = MISCREG_MISC_EC_ERR1_SW_ERR_CODE_0,
		.err_assert_phy_addr = MISCREG_MISC_EC_ERR1_SW_ERR_ASSERT_0,
		.ec_err_idx = 25U,
	},
	{
		.err_code_phyaddr = MISCREG_MISC_EC_ERR2_SW_ERR_CODE_0,
		.err_assert_phy_addr = MISCREG_MISC_EC_ERR2_SW_ERR_ASSERT_0,
		.ec_err_idx = 26U,
	},
	{
		.err_code_phyaddr = MISCREG_MISC_EC_ERR3_SW_ERR_CODE_0,
		.err_assert_phy_addr = MISCREG_MISC_EC_ERR3_SW_ERR_ASSERT_0,
		.ec_err_idx = 27U,
	},
	{
		.err_code_phyaddr = MISCREG_MISC_EC_ERR4_SW_ERR_CODE_0,
		.err_assert_phy_addr = MISCREG_MISC_EC_ERR4_SW_ERR_ASSERT_0,
		.ec_err_idx = 28U,
	}
};

/*File operations*/
const static struct file_operations epl_driver_fops = {
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
		devm_kfree(dev, epl_hsp_v);
		return err;
	}

	return 0;
}

static void epl_unregister_device(void)
{
	if (device_file_major_number != 0)
		unregister_chrdev(device_file_major_number, device_name);
}

static ssize_t device_file_ioctl(
			struct file *fp, unsigned int cmd, unsigned long arg)
{
	uint32_t lData[MAX_LEN];
	int ret;

	if (copy_from_user(lData, (uint8_t *)arg,
				 MAX_LEN * sizeof(uint32_t)))
		return -EACCES;

	switch (cmd) {

	case EPL_REPORT_ERROR_CMD:
		ret = mbox_send_message(epl_hsp_v->tx.chan,
						(void *) lData);
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

		mask = (1U << (miscerr_cfg[err_number].ec_err_idx % 32U));
		mission_err_status = readl(mission_err_status_va);

		if ((mission_err_status & mask) != 0U)
			*status = false;
		else
			*status = true;

		ret = 0;
	}

	return ret;
}
EXPORT_SYMBOL(epl_get_misc_ec_err_status);

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
EXPORT_SYMBOL(epl_report_misc_ec_error);

int epl_report_error(struct epl_error_report_frame error_report)
{
	int ret = -EINVAL;

	if (epl_hsp_v == NULL)
		return -ENODEV;

	ret = mbox_send_message(epl_hsp_v->tx.chan, (void *)&error_report);

	return ret < 0 ? ret : 0;
}
EXPORT_SYMBOL(epl_report_error);

static int __maybe_unused epl_client_suspend(struct device *dev)
{
	pr_debug("tegra-epl: suspend called\n");
	return 0;
}

static int __maybe_unused epl_client_resume(struct device *dev)
{
	pr_debug("tegra-epl: resume called\n");
	return 0;
}
static SIMPLE_DEV_PM_OPS(epl_client_pm, epl_client_suspend, epl_client_resume);

static const struct of_device_id epl_client_dt_match[] = {
	{ .compatible = "nvidia,tegra234-epl-client"},
	{}
};

MODULE_DEVICE_TABLE(of, epl_client_dt_match);

static int epl_client_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	const struct device_node *np = dev->of_node;
	int iterator = 0;
	char name[32] = "client-misc-sw-generic-err";

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
				ioremap(miscerr_cfg[iterator].err_code_phyaddr, MISC_REG_WIDTH);
			miscerr_cfg[iterator].err_assert_va =
				ioremap(miscerr_cfg[iterator].err_assert_phy_addr, MISC_REG_WIDTH);

			if ((miscerr_cfg[iterator].err_code_va == NULL) ||
					(miscerr_cfg[iterator].err_assert_va == NULL)) {
				isAddrMappOk = false;
				ret = -1;
				pr_info("epl: error in mapping misc err register for err #%d\n",
						iterator);
			}
		} else {
			pr_info("Misc Sw Generic Err %d not configured for any client\n", iterator);
		}
	}

	mission_err_status_va = ioremap(MISC_EC_ERRSLICE0_MISSIONERR_STATUS_0, MISC_REG_WIDTH);
	if (mission_err_status_va == NULL) {
		ret = -1;
		isAddrMappOk = false;
		pr_info("epl: error in mapping mission error status register\n");
	}

	return ret;
}

static int epl_client_remove(struct platform_device *pdev)
{
	int iterator = 0;

	epl_unregister_device();
	devm_kfree(&pdev->dev, epl_hsp_v);
	iounmap(mission_err_status_va);
	for (iterator = 0; iterator < NUM_SW_GENERIC_ERR; iterator++) {
		iounmap(miscerr_cfg[iterator].err_code_va);
		iounmap(miscerr_cfg[iterator].err_assert_va);
	}
	return 0;
}

static struct platform_driver epl_client = {
	.driver         = {
	.name   = "epl_client",
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.of_match_table = of_match_ptr(epl_client_dt_match),
		.pm = pm_ptr(&epl_client_pm),
	},
	.probe          = epl_client_probe,
	.remove         = epl_client_remove,
};

module_platform_driver(epl_client);

MODULE_DESCRIPTION("tegra: Error Propagation Library driver");
MODULE_AUTHOR("Prashant Shaw <pshaw@nvidia.com>");
MODULE_LICENSE("GPL v2");
