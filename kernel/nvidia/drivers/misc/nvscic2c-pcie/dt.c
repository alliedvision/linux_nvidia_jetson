// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#define pr_fmt(fmt)	"nvscic2c-pcie: dt: " fmt

#include <linux/errno.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pci_ids.h>
#include <linux/platform_device.h>
#include <linux/printk.h>

#include "common.h"
#include "module.h"

#define COMPATIBLE_EPC_PROP_VAL		("nvidia,tegra-nvscic2c-pcie-epc")
#define COMPATIBLE_EPF_PROP_VAL		("nvidia,tegra-nvscic2c-pcie-epf")
#define HOST1X_PHANDLE_PROP_NAME	("nvidia,host1x")
#define EDMA_PHANDLE_PROP_NAME		("nvidia,pcie-edma")
#define PCI_DEV_ID_PROP_NAME		("nvidia,pci-dev-id")
#define BAR_WIN_SZ_PROP_NAME		("nvidia,bar-win-size")
#define BOARD_ID_PROP_NAME		("nvidia,board-id")
#define SOC_ID_PROP_NAME		("nvidia,soc-id")
#define CNTRLR_ID_PROP_NAME		("nvidia,cntrlr-id")
#define ENDPOINT_DB_PROP_NAME		("nvidia,endpoint-db")
#define MAX_PROP_LEN			(1024)
#define FRAME_SZ_ALIGN			(64)

#define MAX_FRAME_SZ			(SZ_32K)
#define MAX_NFRAMES			(64)
#define MIN_BAR_WIN_SZ			(SZ_64M)

/*
 * Debug only.
 */
static void
dt_print(struct driver_param_t *drv_param)
{
	u32 i = 0;
	struct node_info_t *local_node = &drv_param->local_node;
	struct node_info_t *peer_node = &drv_param->peer_node;

	pr_debug("dt parsing leads to:\n");
	pr_debug("\tdriver mode  = (%s)\n",
		 ((drv_param->drv_mode == DRV_MODE_EPC) ? ("epc") : ("epf")));
	pr_debug("\tpci dev id   = 0x%x\n", drv_param->pci_dev_id);
	pr_debug("\tNode information\n");
	pr_debug("\t\tlocal board id = %u\n", local_node->board_id);
	pr_debug("\t\tpeer board id  = %u\n", peer_node->board_id);
	pr_debug("\t\tlocal soc id   = %u\n", local_node->soc_id);
	pr_debug("\t\tpeer soc id    = %u\n", peer_node->soc_id);
	pr_debug("\t\tlocal pcie cntrlr id = %u\n", local_node->cntrlr_id);
	pr_debug("\t\tpeer pcie cntrlr id  = %u\n", peer_node->cntrlr_id);
	if (drv_param->drv_mode == DRV_MODE_EPF)
		pr_debug("\tbar win size = 0x%x\n", drv_param->bar_win_size);
	pr_debug("\ttotal endpoints	= (%u)\n", drv_param->nr_endpoint);
	for (i = 0; i < drv_param->nr_endpoint; i++) {
		struct endpoint_prop_t *prop = NULL;

		prop = &drv_param->endpoint_props[i];
		pr_debug("\t\t(%s)::\n", prop->name);
		pr_debug("\t\t\tnframes   = (%02u) frame_size=(%08u)",
			 prop->nframes, prop->frame_sz);
	}
	pr_debug("dt parsing ends\n");
}

/*
 * helper function to tokenize the string with caller provided
 * delimiter.
 */
static char *
tokenize(char **input, const char *delim)
{
	/* skipping args check - internal api.*/

	char *token = NULL;

	token = strsep(input, delim);
	if (!token) {
		pr_err("Error parsing endpoint name\n");
	} else {
		/* remove any whitespaces. */
		token = strim(token);
		if (!token)
			pr_err("Error trimming endpoint name\n");
	}

	return token;
}

/*
 * helper function to tokenize the string with caller provided
 * delimiter and provide the sting->uint8_t value.
 *
 * @param input is an in,out parameter.
 *
 */
static int
tokenize_u8(char **input, const char *delim,
	    u32 base, u8 *value)
{
	int ret = 0;
	char *token = NULL;

	/* skipping args check - internal api.*/

	token = tokenize(input, delim);
	if (!token)
		ret = -ENODATA;
	else
		ret = kstrtou8(token, base, value);

	return ret;
}

/*
 * helper function to tokenize the string with caller provided
 * delimiter and provide the sting->u32 value.
 *
 * @param input is an in,out parameter.
 *
 */
static int
tokenize_u32(char **input, const char *delim,
	     u32 base, u32 *value)
{
	int ret = 0;
	char *token = NULL;

	/* skipping args check - internal api.*/

	token = tokenize(input, delim);
	if (!token)
		ret = -ENODATA;
	else
		ret = kstrtou32(token, base, value);

	return ret;
}

/* find a compatible node carrying the pci_dev_id.*/
static struct device_node*
find_compatible_node(const char *compatible, u32 pci_dev_id)
{
	int ret = 0;
	u32 ret_id = 0;
	struct device_node *dn = NULL;
	struct device_node *dn_found = NULL;

	/* look all device nodes with matching compatible and pci-dev-id.*/
	while ((dn = of_find_compatible_node(dn, NULL, compatible)) != NULL) {
		if (of_device_is_available(dn) == false)
			continue;

		ret = of_property_read_u32(dn, PCI_DEV_ID_PROP_NAME, &ret_id);
		if (ret < 0) {
			pr_err("Failed to read: (%s) from device node: (%s)\n",
			       PCI_DEV_ID_PROP_NAME, dn->name);
			of_node_put(dn);
			goto err;
		}

		if (ret_id == pci_dev_id) {
			if (dn_found) {
				ret = -EINVAL;
				pr_err("pci-dev-id: (0x%x) first repeated in:(%s)\n",
				       ret_id, dn->name);
				of_node_put(dn);
				goto err;
			} else {
				dn_found = dn;
			}
		}
	}

	if (!dn_found) {
		ret = -EINVAL;
		pr_err("Matching pci-dev-id: (0x%x) not found\n", pci_dev_id);
		goto err;
	}

	return dn_found;

err:
	return ERR_PTR(ret);
}

/* Parse the host1x phandle and create host1x pdev.*/
static int
parse_host1x_phandle(struct driver_param_t *drv_param)
{
	int ret = 0;
	struct device_node *np = NULL;

	np = drv_param->pdev->dev.of_node;

	drv_param->host1x_np =
			of_parse_phandle(np, HOST1X_PHANDLE_PROP_NAME, 0);
	if (!drv_param->host1x_np) {
		ret = -EINVAL;
		pr_err("Error parsing host1x phandle property: (%s)\n",
		       HOST1X_PHANDLE_PROP_NAME);
	} else {
		drv_param->host1x_pdev =
				of_find_device_by_node(drv_param->host1x_np);
		if (!drv_param->host1x_pdev) {
			ret = -ENODEV;
			pr_err("Host1x device not available\n");
		}
	}

	return ret;
}

/* Parse the pcie-edma phandle.*/
static int
parse_edma_phandle(struct driver_param_t *drv_param)
{
	int ret = 0;
	struct device_node *np = NULL;

	np = drv_param->pdev->dev.of_node;

	drv_param->edma_np = of_parse_phandle(np, EDMA_PHANDLE_PROP_NAME, 0);
	if (!drv_param->edma_np) {
		ret = -EINVAL;
		pr_err("Error parsing pcie-edma phandle property: (%s)\n",
		       EDMA_PHANDLE_PROP_NAME);
	}

	return ret;
}

/* Parse the pci device id.*/
static int
parse_pci_dev_id(struct driver_param_t *drv_param)
{
	int ret = 0;
	struct device_node *np = NULL;

	np = drv_param->pdev->dev.of_node;

	ret = of_property_read_u32(np, PCI_DEV_ID_PROP_NAME,
				   &drv_param->pci_dev_id);
	if (ret) {
		pr_err("Error parsing pci dev id prop:(%s)\n",
		       PCI_DEV_ID_PROP_NAME);
		goto err;
	}

	/* validate.*/
	if (drv_param->pci_dev_id != PCI_DEVICE_ID_NVIDIA_C2C_1 &&
	    drv_param->pci_dev_id != PCI_DEVICE_ID_NVIDIA_C2C_2 &&
	    drv_param->pci_dev_id != PCI_DEVICE_ID_NVIDIA_C2C_3) {
		pr_err("Invalid value for property: (%s)\n",
		       PCI_DEV_ID_PROP_NAME);
		goto err;
	}

err:
	return ret;
}

static int
validate_node_information(struct driver_param_t *drv_param)
{
	struct node_info_t *local_node = NULL;
	struct node_info_t *peer_node = NULL;

	local_node = &drv_param->local_node;
	peer_node = &drv_param->peer_node;

	if (local_node->board_id >= MAX_BOARDS ||
	    peer_node->board_id >= MAX_BOARDS) {
		pr_err("Board Ids must be in the range [0, %u]\n", MAX_BOARDS);
		return -EINVAL;
	}
	if (local_node->soc_id >= MAX_SOCS ||
	    peer_node->soc_id >= MAX_SOCS) {
		pr_err("SoC Ids must be in the range [0, %u]\n", MAX_SOCS);
		return -EINVAL;
	}
	if (local_node->cntrlr_id >= MAX_PCIE_CNTRLRS ||
	    peer_node->cntrlr_id >= MAX_PCIE_CNTRLRS) {
		pr_err("PCIe controller Ids must be in the range [0, %u]\n",
		       MAX_PCIE_CNTRLRS);
		return -EINVAL;
	}

	/*
	 * From the node information, we must have either
	 * one of the three properties different between
	 * local and peer.
	 * Same board, same SoC, different controller
	 * Same board, different SoC, same controller
	 * likewise.
	 *
	 * Essentially the tuple of board+soc+cntrlr shouldn't
	 * be same for local and peer.
	 */
	if (local_node->board_id == peer_node->board_id &&
	    local_node->soc_id == peer_node->soc_id &&
	    local_node->cntrlr_id == peer_node->cntrlr_id)
		return -EINVAL;

	return 0;
}

/* Parse the node information: board, soc, controller information.*/
static int
parse_node_info(struct driver_param_t *drv_param)
{
	int ret = 0;
	struct device_node *np = NULL;
	struct node_info_t *local_node = NULL;
	struct node_info_t *peer_node = NULL;

	np = drv_param->pdev->dev.of_node;
	local_node = &drv_param->local_node;
	peer_node = &drv_param->peer_node;

	/* board-id: local and peer.*/
	ret = of_property_read_u32_index(np, BOARD_ID_PROP_NAME, 0,
					 &local_node->board_id);
	if (ret == 0) {
		ret = of_property_read_u32_index(np, BOARD_ID_PROP_NAME, 1,
						 &peer_node->board_id);
	}
	if (ret) {
		pr_err("Error parsing board id prop:(%s) information\n",
		       BOARD_ID_PROP_NAME);
		goto err;
	}

	/* soc-id: local and peer.*/
	ret = of_property_read_u32_index(np, SOC_ID_PROP_NAME, 0,
					 &local_node->soc_id);
	if (ret == 0) {
		ret = of_property_read_u32_index(np, SOC_ID_PROP_NAME, 1,
						 &peer_node->soc_id);
	}
	if (ret) {
		pr_err("Error parsing soc id prop:(%s) information\n",
		       SOC_ID_PROP_NAME);
		goto err;
	}

	/* pcie controller-id: local and peer.*/
	ret = of_property_read_u32_index(np, CNTRLR_ID_PROP_NAME, 0,
					 &local_node->cntrlr_id);
	if (ret == 0) {
		ret = of_property_read_u32_index(np, CNTRLR_ID_PROP_NAME, 1,
						 &peer_node->cntrlr_id);
	}
	if (ret) {
		pr_err("Error parsing pcie controller id prop:(%s) information\n",
		       CNTRLR_ID_PROP_NAME);
		goto err;
	}

	ret = validate_node_information(drv_param);
	if (ret) {
		pr_err("Node information for board:soc:cntrlr is not sane\n");
		goto err;
	}

err:
	return ret;
}

/* Parse the bar-window-size.*/
static int
parse_bar_win_size(struct driver_param_t *drv_param)
{
	int ret = 0;
	struct device_node *np = NULL;

	np = drv_param->pdev->dev.of_node;

	/* bar-win-size should be checked only when running as epf.*/
	ret = of_property_read_u32(np, BAR_WIN_SZ_PROP_NAME,
				   &drv_param->bar_win_size);
	if (drv_param->drv_mode == DRV_MODE_EPF) {
		if (ret) {
			ret = -EINVAL;
			pr_err("Error parsing bar window size prop:(%s)\n",
			       BAR_WIN_SZ_PROP_NAME);
		}
	} else {
		/* success is not expected for EPC.*/
		if (ret == 0) {
			ret = -EINVAL;
			pr_err("Property (%s): must be present only with (%s)\n",
			       BAR_WIN_SZ_PROP_NAME, COMPATIBLE_EPF_PROP_VAL);
			goto err;
		}
		/* proceed, as error is expected with EPC (node absent).*/
		ret = 0;
		goto err;
	}

	/* validate. */
	if (!drv_param->bar_win_size) {
		ret = -EINVAL;
		pr_err("Invalid BAR window size: (%u)\n",
		       drv_param->bar_win_size);
		goto err;
	}
	if (drv_param->bar_win_size & (drv_param->bar_win_size - 1)) {
		ret = -EINVAL;
		pr_err("BAR window size: (%u) not a power of 2\n",
		       drv_param->bar_win_size);
		goto err;
	}
	if (drv_param->bar_win_size < MIN_BAR_WIN_SZ) {
		ret = -EINVAL;
		pr_err("BAR window size: (%u) less than minimum: (%u)\n",
		       drv_param->bar_win_size, MIN_BAR_WIN_SZ);
		goto err;
	}
err:
	return ret;
}

/*
 * helper function to validate per-endpoint parameters:
 * nframes and frame_size primarily.
 *
 * Add more when required (probably crypto, eDMA, etc.)
 */
static int
validate_endpoint_prop(struct endpoint_prop_t *prop)
{
	int ret = 0;

	/* skipping args check - internal api.*/

	if ((prop->name[0] == '\0')) {
		ret = -EINVAL;
		pr_err("Endpoint must have a name\n");
	} else if (prop->nframes == 0) {
		ret = -EINVAL;
		pr_err("(%s): Invalid number of frames\n", prop->name);
	} else if (prop->frame_sz == 0) {
		ret = -EINVAL;
		pr_err("(%s): Invalid frame size\n", prop->name);
	} else if ((prop->frame_sz & (FRAME_SZ_ALIGN - 1)) != 0) {
		ret = -EINVAL;
		pr_err("(%s): Frame size unaligned to (%u)\n",
		       prop->name, FRAME_SZ_ALIGN);
	} else if (prop->frame_sz > MAX_FRAME_SZ) {
		ret = -EINVAL;
		pr_err("(%s): Frame size greater than: (%u)\n",
		       prop->name, (MAX_FRAME_SZ));
	} else if (prop->nframes > MAX_NFRAMES) {
		ret = -EINVAL;
		pr_err("(%s): Number of frames greater than: (%u)\n",
		       prop->name, (MAX_NFRAMES));
	}

	return ret;
}

/*
 * Parse all the endpoint information available in DT property
 * of nvscic2c-pcie dt node.
 */
static int
parse_endpoint_db(struct driver_param_t *drv_param)
{
	int ret = 0;
	u8 nr_endpoint = 0;
	struct device_node *np = NULL;

	np = drv_param->pdev->dev.of_node;

	ret = of_property_count_strings(np, ENDPOINT_DB_PROP_NAME);
	if (ret < 0) {
		pr_err("Failed to query endpoint count from property: (%s)\n",
		       ENDPOINT_DB_PROP_NAME);
		return -EFAULT;
	}
	nr_endpoint = ret;

	if (nr_endpoint == 0) {
		ret = -EINVAL;
		pr_err("No endpoint information in property: (%s)\n",
		       ENDPOINT_DB_PROP_NAME);
		goto err;
	} else if (nr_endpoint > MAX_ENDPOINTS) {
		ret = -EINVAL;
		pr_err("Invalid endpoint count:(%u) from property: (%s)\n",
		       nr_endpoint, ENDPOINT_DB_PROP_NAME);
		goto err;

	} else {
		u8 i = 0;
		char *inp = NULL;
		u32 base = 10;
		const char *entry = NULL;
		struct property *prop = NULL;
		char entry_dup[MAX_PROP_LEN] = {0};

		/* for each endpoint entry in endpointdb.*/
		of_property_for_each_string(np, ENDPOINT_DB_PROP_NAME,
					    prop, entry) {
			char *name = NULL;
			struct endpoint_prop_t *ep_prop =
				&drv_param->endpoint_props[i];

			/*
			 * per endpoint entry in endpointdb is longer than
			 * expected.
			 */
			if (strlen(entry) > (MAX_PROP_LEN - 1)) {
				ret = -EINVAL;
				pr_err("Endpoint entry invalid\n");
				break;
			}
			memcpy(entry_dup, entry, (strlen(entry)));
			inp = &entry_dup[0];

			/* parse endpoint name.*/
			name = tokenize(&inp, ",");
			if (!name) {
				ret = -EFAULT;
				pr_err("Error parsing endpoint name\n");
				break;
			}
			if (strlen(name) > (NAME_MAX - 1)) {
				ret = -EINVAL;
				pr_err("Endpoint name: (%s) long, max char:(%u)\n",
				       name, (NAME_MAX - 1));
				break;
			}
			strcpy(ep_prop->name, name);

			/* parse number of frames.*/
			ret = tokenize_u8(&inp, ",", base, &ep_prop->nframes);
			if (ret) {
				pr_err("Error parsing token nframes\n");
				break;
			}

			/* parse size of each frame.*/
			ret = tokenize_u32(&inp, ",", base, &ep_prop->frame_sz);
			if (ret) {
				pr_err("Error parsing token frame_sz\n");
				break;
			}

			/* validate some basic properties of endpoint.*/
			ret = validate_endpoint_prop(ep_prop);
			if (ret) {
				pr_err("(%s): endpoint has invalid properties\n",
				       ep_prop->name);
				break;
			}

			/* all okay, assign the id.*/
			ep_prop->id = i;
			i++;
		}
	}

	/* all okay.*/
	drv_param->nr_endpoint = nr_endpoint;

err:
	return ret;
}

/*
 * Look-up device tree node for the compatible string. Check for the
 * pci-dev-id within the compatible node, if more than one such node found also
 * return error.
 */
int
dt_parse(u32 pci_dev_id, enum drv_mode_t drv_mode,
	 struct driver_param_t *drv_param)
{
	int ret = 0;
	char *compatible = NULL;
	struct device_node *dn = NULL;

	if (WARN_ON(!pci_dev_id))
		return -EINVAL;

	if (WARN_ON(!drv_param))
		return -EINVAL;

	if (drv_mode == DRV_MODE_EPC)
		compatible = COMPATIBLE_EPC_PROP_VAL;
	else if (drv_mode == DRV_MODE_EPF)
		compatible = COMPATIBLE_EPF_PROP_VAL;
	else
		return -EINVAL;

	dn = find_compatible_node(compatible, pci_dev_id);
	if (IS_ERR_OR_NULL(dn)) {
		ret = -EINVAL;
		goto err;
	}

	memset(drv_param, 0x0, sizeof(*drv_param));
	drv_param->drv_mode = drv_mode;

	/* dn may not have refcount, by doing this we exlpicitly have one.*/
	drv_param->pdev = of_find_device_by_node(dn);
	if (!drv_param->pdev) {
		pr_err("Failed to find platform device for: (0x%x)\n",
		       pci_dev_id);
		goto err;
	}
	drv_param->of_node = drv_param->pdev->dev.of_node;

	ret = parse_host1x_phandle(drv_param);
	if (ret)
		goto err;

	ret = parse_edma_phandle(drv_param);
	if (ret)
		goto err;

	ret = parse_pci_dev_id(drv_param);
	if (ret)
		goto err;

	ret = parse_node_info(drv_param);
	if (ret)
		goto err;

	ret = parse_bar_win_size(drv_param);
	if (ret)
		goto err;

	ret = parse_endpoint_db(drv_param);
	if (ret)
		goto err;

	/* all okay.*/
	dt_print(drv_param);
	return ret;
err:
	dt_release(drv_param);
	return ret;
}

/*
 * to free-up any memory and decrement ref_count of device nodes
 * accessed.
 */
int
dt_release(struct driver_param_t *drv_param)
{
	int ret = 0;

	if (!drv_param)
		return ret;

	if (drv_param->host1x_pdev) {
		platform_device_put(drv_param->host1x_pdev);
		drv_param->host1x_pdev = NULL;
	}
	if (drv_param->host1x_np) {
		of_node_put(drv_param->host1x_np);
		drv_param->host1x_np = NULL;
	}
	if (drv_param->edma_np) {
		of_node_put(drv_param->edma_np);
		drv_param->edma_np = NULL;
	}
	if (drv_param->pdev) {
		platform_device_put(drv_param->pdev);
		drv_param->pdev = NULL;
	}
	return ret;
}
