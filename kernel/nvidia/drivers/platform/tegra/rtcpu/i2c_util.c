/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>

#define WR_VALUE _IOW(1, 0, struct i2c_data*)
#define RD_VALUE _IOR(1, 1, struct i2c_data*)
#define ERROR_CHECK _IOR(1, 0, struct error_check*)
#define NUMBER_BUS 2
#define NUMBER_DEVICE 10
#define NUMBER_REGISTER 10
#define ERR_MSG_MAX_LEN 100

/*
 *Error #01 : Failed to write data from user arg to the kernel variable.
 *Error #02 : Failed to write data from kernel variable to the user arg.
 *Error #03 : The IOCTL cmd is wrong.
 *Error #04 : Failed to write error check to user arg.
 *
 *Error A : Error allocating MAJOR number.
 *Error B : Error adding the Device to the system.
 *Error C : Error creating the struct class.
 *Error D : Error creating the Device.
 */

struct i2c_data {
	int bus_number;
	int device_address;
	int register_start_address;
	int number_of_registers;
	int value[NUMBER_REGISTER];
};

struct error_check {
	int success;
	char error_message[ERR_MSG_MAX_LEN];
};

static int bus_device_register_arr[NUMBER_BUS][NUMBER_DEVICE][NUMBER_REGISTER];
static struct i2c_data i2c_data_main;
static struct error_check error_check_data;
static dev_t i2c_util_dev;
static struct class *i2c_util_dev_class;
static struct cdev i2c_util_cdev;

static int __init i2c_util_init(void);
static void __exit i2c_util_exit(void);
static int i2c_util_standard_open(struct inode *inode, struct file *file);
static int i2c_util_standard_release(struct inode *inode,
	struct file *file);
static ssize_t i2c_util_standard_read(struct file *filp, char __user *buf,
	size_t len, loff_t *off);
static ssize_t i2c_util_standard_write(struct file *filp, const char __user *buf,
	size_t len, loff_t *off);
static long i2c_util_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg);
static int i2c_util_write_to_memory(void);
static int i2c_util_read_from_memory(void);
static void i2c_util_write(const struct i2c_data __user *arg);
static void i2c_util_read(struct i2c_data __user *arg);
static void i2c_util_error_check(struct error_check __user *arg);

static const struct file_operations i2c_util_fops = {
	.owner = THIS_MODULE,
	.open = i2c_util_standard_open,
	.release = i2c_util_standard_release,
	.write = i2c_util_standard_write,
	.read = i2c_util_standard_read,
	.unlocked_ioctl = i2c_util_ioctl,
};

static void i2c_util_printStruct(struct i2c_data *i2c_data_sec)
{
	int start_index = i2c_data_sec->register_start_address;
	int end_index = i2c_data_sec->register_start_address +
		i2c_data_sec->number_of_registers;
	int value_index = 0;

	pr_info("*************** Struct Data ***************\n");
	pr_info("Bus Number             : %d\n", i2c_data_sec->bus_number);
	pr_info("Device Address         : %d\n", i2c_data_sec->device_address);
	pr_info("Register Start Address : %d\n",
		i2c_data_sec->register_start_address);
	pr_info("Number of Registers    : %d\n",
		i2c_data_sec->number_of_registers);
	while (start_index < end_index) {
		pr_info("Reg(%d) Value           : %d\n", start_index,
			i2c_data_sec->value[value_index]);
		start_index++;
		value_index++;
	}
	pr_info("*******************************************\n\n");
}

static int verify_i2c_data(void)
{
	int max_reg = 0;

	error_check_data.success = 0;

	if (i2c_data_main.bus_number < 0 ||
		i2c_data_main.bus_number >= NUMBER_BUS) {
		strcpy(error_check_data.error_message,
			"Error : Invalid Bus Number.");
		return 0;
	}
	if (i2c_data_main.device_address < 0 ||
		i2c_data_main.device_address >= NUMBER_DEVICE) {
		strcpy(error_check_data.error_message,
			"Error : Invalid Device Address.");
		return 0;
	}
	if (i2c_data_main.register_start_address < 0 ||
		i2c_data_main.register_start_address >= NUMBER_DEVICE) {
		strcpy(error_check_data.error_message,
			"Error : Invalid Register Start Address.");
		return 0;
	}

	max_reg = NUMBER_REGISTER - i2c_data_main.register_start_address;
	if (i2c_data_main.number_of_registers < 0 ||
		i2c_data_main.number_of_registers > max_reg) {
		strcpy(error_check_data.error_message,
			"Error : Invalid Number of registers.");
		return 0;
	}
	error_check_data.success = 1;
	strcpy(error_check_data.error_message, "No Error");

	return 1;
}

static int i2c_util_write_to_memory(void)
{
	int start_index, end_index, value_index;

	if (!verify_i2c_data())
		return 0;

	start_index = i2c_data_main.register_start_address;
	end_index = i2c_data_main.register_start_address +
		i2c_data_main.number_of_registers;
	value_index = 0;

	while (start_index < end_index) {
		bus_device_register_arr[i2c_data_main.bus_number]
			[i2c_data_main.device_address][start_index] =
			i2c_data_main.value[value_index];
		start_index++;
		value_index++;
	}

	return 1;
}

static int i2c_util_read_from_memory(void)
{
	int start_index, end_index, value_index;

	if (!verify_i2c_data())
		return 0;

	start_index = i2c_data_main.register_start_address;
	end_index = i2c_data_main.register_start_address +
		i2c_data_main.number_of_registers;
	value_index = 0;

	while (start_index < end_index) {
		i2c_data_main.value[value_index] =
			bus_device_register_arr[i2c_data_main.bus_number]
			[i2c_data_main.device_address][start_index];
		start_index++;
		value_index++;
	}

	return 1;
}

static void i2c_util_write(const struct i2c_data __user *arg)
{
	if (copy_from_user(&i2c_data_main, arg, sizeof(i2c_data_main))) {
		error_check_data.success = 0;
		strcpy(error_check_data.error_message,
			"Error : Failed to write data from user arg "
			"to the kernel variable");
		pr_err("Error Code : 01.\n");
		return;
	}
	if (i2c_util_write_to_memory() == 0) {
		pr_info("***** %s ******\n", error_check_data.error_message);
		return;
	}

	pr_info("***** Writing Following Data to Memory ******\n");
	i2c_util_printStruct(&i2c_data_main);
}

static void i2c_util_read(struct i2c_data __user *arg)
{
	if (copy_from_user(&i2c_data_main, arg, sizeof(i2c_data_main))) {
		error_check_data.success = 0;
		strcpy(error_check_data.error_message,
			"Error : Failed to write data from user arg "
			"to the kernel variable.");
		pr_err("Error Code : 01.\n");
		return;
	}
	if (i2c_util_read_from_memory() == 0) {
		pr_info("***** %s ******\n", error_check_data.error_message);
		return;
	}
	if (copy_to_user(arg, &i2c_data_main, sizeof(i2c_data_main))) {
		error_check_data.success = 0;
		strcpy(error_check_data.error_message,
			"Error : Failed to write data from kernel "
			"variable to the user arg.");
		pr_err("Error Code : 02.\n");
		return;
	}

	pr_info("**** Reading Following Data from Memory *****\n");
	i2c_util_printStruct(&i2c_data_main);
}

static void i2c_util_error_check(struct error_check __user *arg)
{
	if (copy_to_user(arg, &error_check_data, sizeof(error_check_data))) {
		error_check_data.success = 0;
		strcpy(error_check_data.error_message,
			"Error : Failed to write data from kernel "
			"variable to the user arg.");
		pr_err("Error Code : 02.\n");
		return;
	}
}

static long i2c_util_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	switch (cmd) {
	case WR_VALUE:
		i2c_util_write((const struct i2c_data __user *) arg);
		break;

	case RD_VALUE:
		i2c_util_read((struct i2c_data __user *) arg);
		break;

	case ERROR_CHECK:
		i2c_util_error_check((struct error_check __user *) arg);
		break;

	default:
		error_check_data.success = 0;
		strcpy(error_check_data.error_message,
			"Error : The IOCTL cmd is wrong.");
		pr_err("Error Code : 03.\n");
		break;
	}

	return 0;
}

static int __init i2c_util_init(void)
{
	if ((alloc_chrdev_region(&i2c_util_dev, 0, 1, "i2c_util_Dev")) < 0) {
		pr_err("Error Code : A.\n");
		return -1;
	}

	pr_info("Major = %d Minor = %d\n", MAJOR(i2c_util_dev),
		MINOR(i2c_util_dev));

	cdev_init(&i2c_util_cdev, &i2c_util_fops);

	if ((cdev_add(&i2c_util_cdev, i2c_util_dev, 1)) < 0) {
		pr_err("Error Code : B.\n");
		goto r_class;
	}

	i2c_util_dev_class = class_create(THIS_MODULE, "i2c_util_class");
	if (i2c_util_dev_class == NULL) {
		pr_err("Error Code : C.\n");
		goto r_class;
	}
	if ((device_create(i2c_util_dev_class, NULL, i2c_util_dev,
		NULL, "i2c_util_device")) == NULL) {
		pr_err("Error Code : D.\n");
		goto r_device;
	}

	pr_info("*********************************************\n");
	pr_info("**** Device Driver Inserted Successfully ****\n");
	pr_info("*********************************************\n\n");

	return 0;

r_device:
	class_destroy(i2c_util_dev_class);
r_class:
	unregister_chrdev_region(i2c_util_dev, 1);

	return -1;
}

static void __exit i2c_util_exit(void)
{
	device_destroy(i2c_util_dev_class, i2c_util_dev);
	class_destroy(i2c_util_dev_class);
	cdev_del(&i2c_util_cdev);
	unregister_chrdev_region(i2c_util_dev, 1);
	pr_info("*********************************************\n");
	pr_info("***** Device Driver Removed Successfully ****\n");
	pr_info("*********************************************\n\n");
}

module_init(i2c_util_init);
module_exit(i2c_util_exit);

/************************ Standard Funtions ************************/
static int i2c_util_standard_open(struct inode *inode, struct file *file)
{
	pr_info("************* Device File Opened ************\n\n");

	return 0;
}

static int i2c_util_standard_release(struct inode *inode, struct file *file)
{
	pr_info("************ Device File Closed *************\n\n");

	return 0;
}

static ssize_t i2c_util_standard_read(struct file *filp, char __user *buf,
	size_t len, loff_t *off)
{
	pr_info("************* Device File Read **************\n\n");

	return 0;
}

static ssize_t i2c_util_standard_write(struct file *filp,
	const char __user *buf, size_t len, loff_t *off)
{
	pr_info("************ Device File Written ************\n\n");

	return len;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sampatlal Jangid <sjangid@nvidia.com>");
MODULE_DESCRIPTION("I2C Debug Util Driver");
