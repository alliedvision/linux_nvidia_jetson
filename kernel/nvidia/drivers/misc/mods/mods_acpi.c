// SPDX-License-Identifier: GPL-2.0
/*
 * This file is part of NVIDIA MODS kernel driver.
 *
 * Copyright (c) 2008-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA MODS kernel driver is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * NVIDIA MODS kernel driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NVIDIA MODS kernel driver.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "mods_internal.h"

#include <linux/acpi.h>
#include <linux/device.h>
#include <acpi/acpi.h>
#include <acpi/acpi_bus.h>

static acpi_status mods_acpi_find_acpi_handler(acpi_handle,
					       u32,
					       void *,
					       void **);

/*********************
 * PRIVATE FUNCTIONS *
 *********************/

struct acpi_dev_children_in_data {
	u32         expected_acpi_ids_buf[4];
	u32         num_expected_acpi_ids;
	u32         acpi_dev_mask;
	acpi_handle dev_handle;
};

typedef int (*dev_children_fptr)(struct mods_client *,
				 u32,
				 acpi_handle,
				 void *);

static int acpi_store_dev_children(struct mods_client *client,
				   u32                 curr_acpi_id,
				   acpi_handle         dev_handle,
				   void               *out_data)
{
	struct MODS_GET_ACPI_DEV_CHILDREN *acpi_out_data
			= (struct MODS_GET_ACPI_DEV_CHILDREN *)out_data;

	if (acpi_out_data->num_children >= ACPI_MAX_DEV_CHILDREN) {
		cl_error("ACPI: output buffer too small to store all children\n");
		return -ENOBUFS;
	}

	acpi_out_data->children[acpi_out_data->num_children] = curr_acpi_id;
	++acpi_out_data->num_children;

	return OK;
}

static int acpi_compare_dev_id(struct mods_client *client,
			       u32                 curr_acpi_id,
			       acpi_handle         dev_handle,
			       void               *out_data)
{
	u32                               i;
	struct acpi_dev_children_in_data *dev_child_data
			= (struct acpi_dev_children_in_data *)out_data;

	for (i = 0; i < dev_child_data->num_expected_acpi_ids; ++i) {
		if ((dev_child_data->expected_acpi_ids_buf[i] & dev_child_data->acpi_dev_mask)
			== (curr_acpi_id & dev_child_data->acpi_dev_mask)) {
			dev_child_data->dev_handle = dev_handle;
			/*
			 * returning unique error code to signal caller that value is found
			 */
			return -EALREADY;
		}
	}
	return -ENODEV;
}

static int acpi_get_dev_children(struct mods_client *client,
				 dev_children_fptr   fptr,
				 acpi_handle         dev_handle,
				 void               *out_data)
{
	int                 err     = OK;
	acpi_status         status;
	struct acpi_device *device  = NULL;
	struct list_head   *node    = NULL;
	struct list_head   *next    = NULL;

	LOG_ENT();

	status = acpi_bus_get_device(dev_handle, &device);
	if (ACPI_FAILURE(status) || !device) {
		cl_error("ACPI: device for fetching device children not found\n");
		LOG_EXT();
		return -EINVAL;
	}

	list_for_each_safe(node, next, &device->children) {
		unsigned long long device_id = 0;

		struct acpi_device *dev =
			list_entry(node, struct acpi_device, node);

		status = acpi_evaluate_integer(dev->handle,
					       "_ADR",
					       NULL,
					       &device_id);
		if (ACPI_FAILURE(status))
			/* Couldnt query device_id for this device */
			continue;

		err = fptr(client, device_id, dev->handle, out_data);
		if (err == -EALREADY) {
			err = OK;
			break;
		}
	}

	LOG_EXT();
	return err;
}


/* store handle if found. */
static void mods_acpi_handle_init(struct mods_client *client,
				  char               *method_name,
				  acpi_handle        *handler)
{
	MODS_ACPI_WALK_NAMESPACE(ACPI_TYPE_ANY,
				 ACPI_ROOT_OBJECT,
				 ACPI_UINT32_MAX,
				 mods_acpi_find_acpi_handler,
				 method_name,
				 handler);

	if (!(*handler)) {
		cl_debug(DEBUG_ACPI,
			 "ACPI method %s not found\n",
			 method_name);
	}
}

static acpi_status mods_acpi_find_acpi_handler(
	acpi_handle handle,
	u32 nest_level,
	void *dummy1,
	void **dummy2
)
{
	acpi_handle acpi_method_handler_temp;

	if (!acpi_get_handle(handle, dummy1, &acpi_method_handler_temp))
		*dummy2 = acpi_method_handler_temp;

	return OK;
}

static int mods_extract_acpi_object(struct mods_client *client,
				    char               *method,
				    union acpi_object  *obj,
				    u8                **buf,
				    u8                 *buf_end)
{
	int err = OK;

	switch (obj->type) {

	case ACPI_TYPE_BUFFER:
		if (obj->buffer.length == 0) {
			cl_error(
				"empty ACPI output buffer from ACPI method %s\n",
				method);
			err = -EINVAL;
		} else if (obj->buffer.length <= buf_end-*buf) {
			u32 size = obj->buffer.length;

			memcpy(*buf, obj->buffer.pointer, size);
			*buf += size;
		} else {
			cl_error("output buffer too small for ACPI method %s\n",
				 method);
			err = -EINVAL;
		}
		break;

	case ACPI_TYPE_INTEGER:
		if (buf_end - *buf >= 4) {
			if (obj->integer.value > 0xFFFFFFFFU) {
				cl_error(
					"integer value from ACPI method %s out of range\n",
					method);
				err = -EINVAL;
			} else {
				memcpy(*buf, &obj->integer.value, 4);
				*buf += 4;
			}
		} else {
			cl_error("output buffer too small for ACPI method %s\n",
				 method);
			err = -EINVAL;
		}
		break;

	case ACPI_TYPE_PACKAGE:
		if (obj->package.count == 0) {
			cl_error(
				"empty ACPI output package from ACPI method %s\n",
				method);
			err = -EINVAL;
		} else {
			union acpi_object *elements = obj->package.elements;
			u32                size     = 0;
			u32                i;

			for (i = 0; i < obj->package.count; i++) {
				u8 *old_buf = *buf;
				u32 new_size;

				err = mods_extract_acpi_object(client,
							       method,
							       &elements[i],
							       buf,
							       buf_end);
				if (err)
					break;

				new_size = *buf - old_buf;

				if (size == 0) {
					size = new_size;
				} else if (size != new_size) {
					cl_error(
						"ambiguous package element size from ACPI method %s\n",
						method);
					err = -EINVAL;
				}
			}
		}
		break;

	case ACPI_TYPE_LOCAL_REFERENCE:
		if (obj->reference.actual_type == ACPI_TYPE_POWER) {
			memcpy(*buf, &obj->reference.handle,
					sizeof(obj->reference.handle));
			*buf += sizeof(obj->reference.handle);
		} else {
			cl_error("Unsupported ACPI reference type\n");
			err = -EINVAL;
		}
		break;

	default:
		cl_error("unsupported ACPI output type 0x%02x from method %s\n",
			 (unsigned int)obj->type,
			 method);
		err = -EINVAL;
		break;

	}
	return err;
}

static int mods_eval_acpi_method(struct mods_client           *client,
				 struct MODS_EVAL_ACPI_METHOD *p,
				 struct mods_pci_dev_2        *pdevice,
				 u32                           acpi_id)
{
	int err = OK;
	int i;
	acpi_status status;
	struct acpi_object_list input;
	struct acpi_buffer output = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *acpi_method = NULL;
	union acpi_object acpi_params[ACPI_MAX_ARGUMENT_NUMBER];
	acpi_handle acpi_method_handler = NULL;
	struct pci_dev *dev = NULL;

	LOG_ENT();

	if (p->argument_count >= ACPI_MAX_ARGUMENT_NUMBER) {
		cl_error("invalid argument count for ACPI call\n");
		LOG_EXT();
		return -EINVAL;
	}

	if (pdevice) {
		cl_debug(DEBUG_ACPI,
			 "ACPI %s for dev %04x:%02x:%02x.%x\n",
			 p->method_name,
			 pdevice->domain,
			 pdevice->bus,
			 pdevice->device,
			 pdevice->function);

		err = mods_find_pci_dev(client, pdevice, &dev);
		if (unlikely(err)) {
			if (err == -ENODEV)
				cl_error(
					"ACPI: dev %04x:%02x:%02x.%x not found\n",
					pdevice->domain,
					pdevice->bus,
					pdevice->device,
					pdevice->function);
			LOG_EXT();
			return err;
		}
		acpi_method_handler = MODS_ACPI_HANDLE(&dev->dev);
	} else {
		cl_debug(DEBUG_ACPI, "ACPI %s\n", p->method_name);
		mods_acpi_handle_init(client,
				      p->method_name,
				      &acpi_method_handler);
	}

	if (acpi_id != ACPI_MODS_IGNORE_ACPI_ID) {
		struct acpi_dev_children_in_data in_data = { {}, 0, 0, NULL};

		in_data.expected_acpi_ids_buf[0] = acpi_id;
		in_data.num_expected_acpi_ids = 1;
		in_data.acpi_dev_mask = 0xffffffff;

		err = acpi_get_dev_children(client, acpi_compare_dev_id,
				acpi_method_handler, (void *)&in_data);
		if (err) {
			pci_dev_put(dev);
			LOG_EXT();
			return -EINVAL;
		}
		acpi_method_handler = in_data.dev_handle;
	}

	if (!acpi_method_handler) {
		cl_debug(DEBUG_ACPI,
			 "ACPI: handle for %s not found\n",
			 p->method_name);
		pci_dev_put(dev);
		LOG_EXT();
		return -EINVAL;
	}
	cl_debug(DEBUG_ACPI,
		 "ACPI: found %s (id = 0x%x) on dev %04x:%02x:%02x.%x\n",
		 p->method_name,
		 (unsigned int)acpi_id,
		 pdevice ? pdevice->domain : 0U,
		 pdevice ? pdevice->bus : 0U,
		 pdevice ? pdevice->device : 0U,
		 pdevice ? pdevice->function : 0U);

	input.count = p->argument_count;
	input.pointer = acpi_params;

	for (i = 0; i < p->argument_count; i++) {
		switch (p->argument[i].type) {
		case ACPI_MODS_TYPE_INTEGER: {
			acpi_params[i].integer.type = ACPI_TYPE_INTEGER;
			acpi_params[i].integer.value
				= p->argument[i].integer.value;
			break;
		}
		case ACPI_MODS_TYPE_BUFFER: {
			acpi_params[i].buffer.type = ACPI_TYPE_BUFFER;
			acpi_params[i].buffer.length
				= p->argument[i].buffer.length;
			acpi_params[i].buffer.pointer
				= p->in_buffer + p->argument[i].buffer.offset;
			break;
		}
		case ACPI_MODS_TYPE_METHOD: {
			memcpy(&acpi_method_handler,
					&p->argument[i].method.handle,
					sizeof(acpi_method_handler));

			if (!acpi_method_handler) {
				cl_error("ACPI: Invalid reference handle 0\n");
				pci_dev_put(dev);
				LOG_EXT();
				return -EINVAL;
			}

			if (i != p->argument_count - 1) {
				cl_error("ACPI: Invalid argument count\n");
				pci_dev_put(dev);
				LOG_EXT();
				return -EINVAL;
			}

			--input.count;
			break;
		}
		default: {
			cl_error("unsupported ACPI argument type\n");
			pci_dev_put(dev);
			LOG_EXT();
			return -EINVAL;
		}
		}
	}

	status = acpi_evaluate_object(acpi_method_handler,
				      pdevice ? p->method_name : NULL,
				      &input,
				      &output);

	if (ACPI_FAILURE(status)) {
		cl_info("ACPI method %s failed\n", p->method_name);
		pci_dev_put(dev);
		LOG_EXT();
		return -EINVAL;
	}

	acpi_method = output.pointer;
	if (!acpi_method) {
		cl_error("missing output from ACPI method %s\n",
			 p->method_name);
		err = -EINVAL;
	} else {
		u8 *buf = p->out_buffer;

		err = mods_extract_acpi_object(client,
					       p->method_name,
					       acpi_method,
					       &buf,
					       buf+sizeof(p->out_buffer));
		p->out_data_size = err ? 0 : (buf - p->out_buffer);
	}

	kfree(output.pointer);
	pci_dev_put(dev);
	LOG_EXT();
	return err;
}

static int mods_acpi_get_ddc(struct mods_client         *client,
			     struct MODS_ACPI_GET_DDC_2 *p,
			     struct mods_pci_dev_2      *pci_device)
{
	int                               err;
	acpi_status                       status;
	union acpi_object                *ddc;
	union acpi_object                 ddc_arg0       = { ACPI_TYPE_INTEGER };
	struct acpi_object_list           input          = { 1, &ddc_arg0 };
	u32                               i;
	acpi_handle                       dev_handle     = NULL;
	acpi_handle                       lcd_dev_handle = NULL;
	struct pci_dev                   *dev            = NULL;
	bool                              data_found     = false;
	struct acpi_dev_children_in_data  in_data        = { {}, 0, 0, NULL };

	LOG_ENT();

	cl_debug(DEBUG_ACPI,
		 "ACPI _DDC (EDID) for dev %04x:%02x:%02x.%x\n",
		 pci_device->domain,
		 pci_device->bus,
		 pci_device->device,
		 pci_device->function);

	err = mods_find_pci_dev(client, pci_device, &dev);
	if (unlikely(err)) {
		if (err == -ENODEV)
			cl_error("ACPI: dev %04x:%02x:%02x.%x not found\n",
				 pci_device->domain,
				 pci_device->bus,
				 pci_device->device,
				 pci_device->function);
		LOG_EXT();
		return err;
	}

	dev_handle = MODS_ACPI_HANDLE(&dev->dev);
	if (!dev_handle) {
		cl_debug(DEBUG_ACPI, "ACPI: handle for _DDC not found\n");
		pci_dev_put(dev);
		LOG_EXT();
		return -EINVAL;
	}

	/*
	 * List of supported display's (panels)
	 * Reference: https://uefi.org/sites/default/files/resources/ACPI_6_1.pdf
	 * Section: Table B-390 Video Output Device Attributes
	 */
	in_data.expected_acpi_ids_buf[0] = 0x0110;
	in_data.expected_acpi_ids_buf[1] = 0x0118;
	in_data.expected_acpi_ids_buf[2] = 0x0400;
	in_data.expected_acpi_ids_buf[3] = 0xa450;
	in_data.num_expected_acpi_ids = 4;
	in_data.acpi_dev_mask = 0xffff;

	err = acpi_get_dev_children(client, acpi_compare_dev_id,
			dev_handle, (void *)&in_data);
	if (err) {
		pci_dev_put(dev);
		LOG_EXT();
		return -EINVAL;
	}
	lcd_dev_handle = in_data.dev_handle;

	if (lcd_dev_handle == NULL) {
		cl_error("ACPI: LCD not found for dev %04x:%02x:%02x.%x\n",
			 p->device.domain,
			 p->device.bus,
			 p->device.device,
			 p->device.function);
		pci_dev_put(dev);
		LOG_EXT();
		return -EINVAL;
	}

	cl_debug(DEBUG_ACPI,
		 "ACPI: Found LCD on dev %04x:%02x:%02x.%x\n",
		 p->device.domain,
		 p->device.bus,
		 p->device.device,
		 p->device.function);

	/*
	 * As per ACPI Spec 3.0:
	 * ARG0 is size of EDID buffer in 128-byte blocks.
	 */
	err = -EINVAL;
	for (i = 4; i >= 1; --i) {
		struct acpi_buffer output = { ACPI_ALLOCATE_BUFFER, NULL };

		ddc_arg0.integer.value = i;
		status = acpi_evaluate_object(lcd_dev_handle,
					      "_DDC",
					      &input,
					      &output);
		if (ACPI_SUCCESS(status)) {
			ddc = output.pointer;
			if (ddc && (ddc->type != ACPI_TYPE_BUFFER))
				continue;

			if (!ddc || ddc->buffer.length == 0) {
				cl_error("unsupported ACPI output type\n");
			} else if (ddc->buffer.length > sizeof(p->out_buffer)) {
				cl_error(
					"output buffer too small for ACPI method _DDC (EDID)\n");
			} else {
				p->out_data_size = ddc->buffer.length;
				memcpy(p->out_buffer,
					   ddc->buffer.pointer,
					   p->out_data_size);
				err = OK;
				data_found = true;
			}

			kfree(ddc);

			break;
		}
	}

	if (!data_found)
		cl_error("ACPI method _DDC (EDID) failed\n");

	pci_dev_put(dev);
	LOG_EXT();
	return err;
}

/*************************
 * ESCAPE CALL FUNCTIONS *
 *************************/

int esc_mods_eval_acpi_method(struct mods_client           *client,
			      struct MODS_EVAL_ACPI_METHOD *p)
{
	return mods_eval_acpi_method(client, p, NULL, ACPI_MODS_IGNORE_ACPI_ID);
}

int esc_mods_eval_dev_acpi_method_3(struct mods_client                 *client,
				    struct MODS_EVAL_DEV_ACPI_METHOD_3 *p)
{
	return mods_eval_acpi_method(client,
				     &p->method,
				     &p->device,
				     p->acpi_id);
}

int esc_mods_eval_dev_acpi_method_2(struct mods_client                 *client,
				    struct MODS_EVAL_DEV_ACPI_METHOD_2 *p)
{
	return mods_eval_acpi_method(client,
				     &p->method,
				     &p->device,
				     ACPI_MODS_IGNORE_ACPI_ID);
}

int esc_mods_eval_dev_acpi_method(struct mods_client               *client,
				  struct MODS_EVAL_DEV_ACPI_METHOD *p)
{
	struct mods_pci_dev_2 device = {0};

	device.domain		= 0;
	device.bus		= p->device.bus;
	device.device		= p->device.device;
	device.function		= p->device.function;
	return mods_eval_acpi_method(client, &p->method, &device,
				     ACPI_MODS_IGNORE_ACPI_ID);
}

int esc_mods_acpi_get_ddc_2(struct mods_client         *client,
			    struct MODS_ACPI_GET_DDC_2 *p)
{
	return mods_acpi_get_ddc(client, p, &p->device);
}

int esc_mods_acpi_get_ddc(struct mods_client       *client,
			  struct MODS_ACPI_GET_DDC *p)
{
	struct MODS_ACPI_GET_DDC_2 *pp     = (struct MODS_ACPI_GET_DDC_2 *) p;
	struct mods_pci_dev_2       device = {0};

	device.domain	= 0;
	device.bus	= p->device.bus;
	device.device	= p->device.device;
	device.function	= p->device.function;

	return mods_acpi_get_ddc(client, pp, &device);
}

int esc_mods_get_acpi_dev_children(struct mods_client                *client,
				   struct MODS_GET_ACPI_DEV_CHILDREN *p)
{
	int             err;
	struct pci_dev *dev        = NULL;
	acpi_handle     dev_handle = NULL;

	LOG_ENT();

	cl_debug(DEBUG_ACPI,
		 "ACPI: failed to get children for dev %04x:%02x:%02x.%x\n",
		 p->device.domain,
		 p->device.bus,
		 p->device.device,
		 p->device.function);

	err = mods_find_pci_dev(client, &p->device, &dev);
	if (unlikely(err)) {
		if (err == -ENODEV)
			cl_error("ACPI: dev %04x:%02x:%02x.%x not found\n",
				 p->device.domain,
				 p->device.bus,
				 p->device.device,
				 p->device.function);
		LOG_EXT();
		return err;
	}

	dev_handle = MODS_ACPI_HANDLE(&dev->dev);
	if (!dev_handle) {
		cl_error("ACPI: handle for fetching device children not found\n");
		pci_dev_put(dev);
		LOG_EXT();
		return -EINVAL;
	}

	p->num_children = 0;
	err = acpi_get_dev_children(client, acpi_store_dev_children,
			dev_handle, (void *)p);

	if (err) {
		cl_error("ACPI: failed to get children for dev %04x:%02x:%02x.%x\n",
			 p->device.domain,
			 p->device.bus,
			 p->device.device,
			 p->device.function);
	}

	pci_dev_put(dev);
	LOG_EXT();
	return err;
}
