/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION. All rights reserved.
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

#include <linux/delay.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/ktime.h>
#include <linux/debugfs.h>

#include <nvgpu/lock.h>
#include <nvgpu/nvgpu_init.h>

#include "module.h"
#include "platform_gk20a.h"
#include "pci_power.h"

#define PCI_DEV_NAME_MAX	64

struct pci_power_stats {
	u64 power_ons;
	s64 power_on_lat_ns_min;
	s64 power_on_lat_ns_max;
	s64 power_on_lat_ns_avg;
	u64 power_offs;
	s64 power_off_lat_ns_min;
	s64 power_off_lat_ns_max;
	s64 power_off_lat_ns_avg;
};

static struct dentry *pci_power_stats_dbgfs_dentry;

struct nvgpu_pci_power {
	struct	list_head list;
	struct	nvgpu_mutex mutex;
	struct	nvgpu_pci_gpios gpios;
	struct	pci_dev *pci_dev;
	char	pci_dev_name[PCI_DEV_NAME_MAX];
	void	*pci_cookie;
	struct	pci_power_stats stats;
	bool	can_pci_gc_off;
};

static struct list_head nvgpu_pci_power_devs =
	LIST_HEAD_INIT(nvgpu_pci_power_devs);

static struct nvgpu_pci_power *nvgpu_pci_get_pci_power(const char *dev_name)
{
	struct nvgpu_pci_power *pp, *tmp_pp;

	list_for_each_entry_safe(pp, tmp_pp, &nvgpu_pci_power_devs, list) {
		if (!strcmp(dev_name, pp->pci_dev_name))
			return pp;
	}
	return NULL;
}

int nvgpu_pci_add_pci_power(struct pci_dev *pdev)
{
	struct gk20a_platform *platform;
	struct nvgpu_pci_power *pp;

	if (!pdev)
		return -EINVAL;

	pp = nvgpu_pci_get_pci_power(dev_name(&pdev->dev));
	if (pp) {
		pp->pci_dev = pdev;
		return 0;
	}

	pp = kzalloc(sizeof(*pp), GFP_KERNEL);
	if (!pp)
		return -ENOMEM;

	nvgpu_mutex_init(&pp->mutex);
	pp->pci_dev = pdev;
	strlcpy(pp->pci_dev_name,
		dev_name(&pdev->dev), PCI_DEV_NAME_MAX);

	platform = pci_get_drvdata(pdev);
	pp->can_pci_gc_off = platform->can_pci_gc_off;

	list_add(&pp->list, &nvgpu_pci_power_devs);

	return 0;
}

static void nvgpu_free_pci_gpios(struct nvgpu_pci_gpios *pgpios);

static int nvgpu_pci_remove_pci_power(struct nvgpu_pci_power *pp)
{
	list_del(&pp->list);
	nvgpu_free_pci_gpios(&pp->gpios);
	kfree(pp);
	return 0;
}

static ssize_t probed_gpus_show(struct device_driver *drv, char *buf)
{
	struct nvgpu_pci_power *pp, *tmp_pp;
	ssize_t count = 0;

	list_for_each_entry_safe(pp, tmp_pp, &nvgpu_pci_power_devs, list) {
		count += snprintf(buf, NVGPU_CPU_PAGE_SIZE - count, "pci-%s\t%s\n",
				  pp->pci_dev_name,
				  pp->pci_dev ? "PoweredOn" : "PoweredOff");
	}
	return count;
}

static DRIVER_ATTR_RO(probed_gpus);

int nvgpu_pci_clear_pci_power(const char *dev_name)
{
	struct nvgpu_pci_power *pp, *tmp_pp;

	list_for_each_entry_safe(pp, tmp_pp, &nvgpu_pci_power_devs, list) {
		if (!strcmp(dev_name, pp->pci_dev_name)) {
			pp->pci_dev = NULL;
			return 0;
		}
	}
	return -ENODEV;
}

static int nvgpu_pci_parse_dev_name(const char *buf, size_t count, char *name)
{
	int domain, bus, device, func;
	int ret;

	/* DDDD:BB:DD.F for domain:bus:device.function */
	ret = sscanf(buf, "%x:%x:%x.%x", &domain, &bus, &device, &func);
	if (ret < 4)
		return -EINVAL;

	(void) snprintf(name, PCI_DEV_NAME_MAX, "%04x:%02x:%02x.%1x",
		 domain, bus, device, func);

	return 0;
}

static char *nvgpu_pci_gpio_name(int g)
{
	switch (g) {
	case PCI_GPIO_VBAT_PWR_ON:
		return "PCI_GPIO_VBAT_PWR_ON";
	case PCI_GPIO_PRSNT2:
		return "PCI_GPIO_PRSNT2*";
	case PCI_GPIO_PRSNT1:
		return "PCI_GPIO_PRSNT1*";
	case PCI_GPIO_PWR_ON:
		return "PCI_GPIO_PWR_ON";
	case PCI_GPIO_PG:
		return "PCI_GPIO_PG";
	}
	return "INVALID_PCI_GPIO";
}

static void nvgpu_dump_pci_gpios(struct nvgpu_pci_gpios *pgpios, const char *f)
{
	int is_in, val, i;
	struct gpio_desc *gd;

	pr_debug("nvgpu gpio status in %s:\n", f);

	for (i = 0; i < PCI_GPIO_MAX; i++) {
		if (pgpios->gpios[i] == 0) {
			pr_debug("%d. %-25s: gpio not requested\n",
				 i, nvgpu_pci_gpio_name(i));
			continue;
		}

		gd = gpio_to_desc(pgpios->gpios[i]);
		if (gd) {
			is_in = gpiod_get_direction(gd);
			val = gpiod_get_value_cansleep(gd);

			pr_debug("%d. %-25s gpio-%-3d dir=%s val=%s\n",
				 i, nvgpu_pci_gpio_name(i), pgpios->gpios[i],
				 is_in ? "in " : "out",
				 val >= 0 ? (val != 0 ? "hi" : "lo") : "?  ");

		} else {
			pr_debug("%d. %-25s invalid gpio desc\n",
				 i, nvgpu_pci_gpio_name(i));
		}
	}
}

static void nvgpu_free_pci_gpios(struct nvgpu_pci_gpios *pgpios)
{
	int i;

	for (i = 0; i < PCI_GPIO_MAX; i++) {
		if (pgpios->gpios[i]) {
			gpio_free(pgpios->gpios[i]);
			pgpios->gpios[i] = 0;
		}
	}
}

static int nvgpu_request_pci_gpios(struct nvgpu_pci_gpios *pgpios)
{
	struct device_node *np;
	int i, ret, gpio;

	if (pgpios->gpios[0])
		return 0;

	np = of_find_node_by_name(NULL, "nvgpu");
	if (!np) {
		ret = -ENOENT;
		goto err;
	}

	for (i = 0; i < PCI_GPIO_MAX; i++) {
		gpio = of_get_named_gpio(np, "nvgpu-pci-gpios", i);
		if (gpio < 0) {
			ret = gpio;
			goto err;
		}

		ret = gpio_request(gpio, "pci-gpio");
		if (ret)
			goto err;

		pgpios->gpios[i] = gpio;
	}

	nvgpu_dump_pci_gpios(pgpios, __func__);

	of_node_put(np);
	return 0;
err:
	of_node_put(np);
	nvgpu_free_pci_gpios(pgpios);
	return ret;
}

static int nvgpu_disable_pci_rail(struct nvgpu_pci_gpios *pgpios)
{
	int pci_vbat_pwr_on_gpio = pgpios->gpios[PCI_GPIO_VBAT_PWR_ON];

	gpio_set_value(pci_vbat_pwr_on_gpio, 0);

	mdelay(PCI_VBAR_PWR_OFF_DELAY_MS);
	return 0;
}

static int nvgpu_check_pci_power_good(struct nvgpu_pci_gpios *pgpios)
{
	int pci_pg = pgpios->gpios[PCI_GPIO_PG];

	return gpio_get_value(pci_pg) != 1 ? -EINVAL : 0;
}

static int nvgpu_enable_pci_rail(struct nvgpu_pci_gpios *pgpios)
{
	int pci_vbat_pwr_on_gpio = pgpios->gpios[PCI_GPIO_VBAT_PWR_ON];

	gpio_set_value(pci_vbat_pwr_on_gpio, 1);

	mdelay(PCI_VBAR_PWR_ON_DELAY_MS);
	return 0;
}

static int nvgpu_deassert_pci_pwr_on(struct nvgpu_pci_gpios *pgpios)
{
	int pci_pwr_on = pgpios->gpios[PCI_GPIO_PWR_ON];

	gpio_set_value(pci_pwr_on, 0);

	mdelay(PCI_PWR_OFF_DELAY_MS);
	return 0;
}

static int nvgpu_assert_pci_pwr_on(struct nvgpu_pci_gpios *pgpios)
{
	int pci_pwr_on = pgpios->gpios[PCI_GPIO_PWR_ON];

	gpio_set_value(pci_pwr_on, 1);

	mdelay(PCI_PWR_ON_DELAY_MS);
	return 0;
}

static void nvgpu_update_power_on_stats(struct nvgpu_pci_power *pp, s64 time_ns)
{
	struct pci_power_stats *stats = &pp->stats;

	stats->power_ons++;

	if (unlikely(stats->power_on_lat_ns_min == 0)) {
		stats->power_on_lat_ns_min = time_ns;
		stats->power_on_lat_ns_max = time_ns;
		stats->power_on_lat_ns_avg = time_ns;
		return;
	}

	if (time_ns < stats->power_on_lat_ns_min)
		stats->power_on_lat_ns_min = time_ns;

	if (time_ns > stats->power_on_lat_ns_max)
		stats->power_on_lat_ns_max = time_ns;

	stats->power_on_lat_ns_avg =
		(stats->power_on_lat_ns_avg + time_ns) >> 1;
}

static void nvgpu_update_power_off_stats(struct nvgpu_pci_power *pp,
					 s64 time_ns)
{
	struct pci_power_stats *stats = &pp->stats;

	stats->power_offs++;

	if (unlikely(stats->power_off_lat_ns_min == 0)) {
		stats->power_off_lat_ns_min = time_ns;
		stats->power_off_lat_ns_max = time_ns;
		stats->power_off_lat_ns_avg = time_ns;
		return;
	}

	if (time_ns < stats->power_off_lat_ns_min)
		stats->power_off_lat_ns_min = time_ns;

	if (time_ns > stats->power_off_lat_ns_max)
		stats->power_off_lat_ns_max = time_ns;

	stats->power_off_lat_ns_avg =
		(stats->power_off_lat_ns_avg + time_ns) >> 1;
}

#define DBG_POWER_STAT_SEQ_PRINTF(s, stat)	\
	seq_printf(s, "%20s:%15lld\n", #stat, pp->stats.stat)

static int debugfs_pci_power_stats_show(struct seq_file *s, void *unused)
{
	struct nvgpu_pci_power *pp, *tmp_pp;

	list_for_each_entry_safe(pp, tmp_pp, &nvgpu_pci_power_devs, list) {
		seq_printf(s, "PCI GPU (%s) Power Stats:\n", pp->pci_dev_name);
		DBG_POWER_STAT_SEQ_PRINTF(s, power_ons);
		DBG_POWER_STAT_SEQ_PRINTF(s, power_on_lat_ns_min);
		DBG_POWER_STAT_SEQ_PRINTF(s, power_on_lat_ns_max);
		DBG_POWER_STAT_SEQ_PRINTF(s, power_on_lat_ns_avg);
		DBG_POWER_STAT_SEQ_PRINTF(s, power_offs);
		DBG_POWER_STAT_SEQ_PRINTF(s, power_off_lat_ns_min);
		DBG_POWER_STAT_SEQ_PRINTF(s, power_off_lat_ns_max);
		DBG_POWER_STAT_SEQ_PRINTF(s, power_off_lat_ns_avg);
	}

	return 0;
}

static int debugfs_pci_power_stats_open(struct inode *i, struct file *f)
{
	return single_open(f, debugfs_pci_power_stats_show, &i->i_private);
}

static const struct file_operations debug_power_stats_fops = {
	.open		= debugfs_pci_power_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

#if (!IS_ENABLED(CONFIG_PCIE_TEGRA_DW) && !IS_ENABLED(CONFIG_PCIE_TEGRA)) || \
	((LINUX_VERSION_CODE <= KERNEL_VERSION(4, 14, 0)) &&	\
	 !IS_ENABLED(CONFIG_ARCH_TEGRA_19x_SOC)) ||	\
	((LINUX_VERSION_CODE > KERNEL_VERSION(4, 14, 0)) &&	\
	 !IS_ENABLED(CONFIG_ARCH_TEGRA_194_SOC)) ||	\
	LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0)
void *tegra_pcie_detach_controller(struct pci_dev *pdev)
{
	pr_err("nvgpu: detach pci controller not available\n");
	return NULL;
}

int tegra_pcie_attach_controller(void *cookie)
{
	pr_err("nvgpu: attach pci controller not available\n");
	return -EINVAL;
}
#endif

static int nvgpu_detach_pci_gpu(struct nvgpu_pci_power *pp)
{
	struct pci_dev *pdev = pp->pci_dev;
	void *pci_cookie;
	int ret = 0;

	pci_cookie = tegra_pcie_detach_controller(pdev);

	if (IS_ERR(pci_cookie)) {
		ret = PTR_ERR(pci_cookie);
		pr_err("nvgpu: detaching PCIe controller failed (%d)\n", ret);
		return ret;
	}

	pp->pci_cookie = pci_cookie;
	return 0;
}

static int nvgpu_attach_pci_gpu(struct nvgpu_pci_power *pp)
{
	void *pci_cookie = pp->pci_cookie;
	int ret = 0;

	if (pci_cookie == NULL) {
		pr_err("nvgpu: Invalid pci cookie\n");
		return -EINVAL;
	}

	ret = tegra_pcie_attach_controller(pci_cookie);
	if (ret)
		pr_err("nvgpu: attaching PCIe controller failed (%d)\n", ret);

	return ret;
}

static int nvgpu_pci_gpu_power_on(char *dev_name)
{
	struct nvgpu_pci_power *pp;
	struct nvgpu_pci_gpios *pgpios;
	ktime_t time_start;
	s64 time_ns;
	int ret;

	pp = nvgpu_pci_get_pci_power(dev_name);
	if (!pp) {
		pr_err("nvgpu: no pci dev by name: %s\n", dev_name);
		return -ENODEV;
	}

	if (pp->can_pci_gc_off == false) {
		pr_err("nvgpu: gc-off not enabled for pdev: %s\n", dev_name);
		return -EPERM;
	}

	time_start = ktime_get();

	nvgpu_mutex_acquire(&pp->mutex);

	pgpios = &pp->gpios;

	ret = nvgpu_request_pci_gpios(pgpios);
	if (ret) {
		pr_err("nvgpu: request pci gpios failed\n");
		goto out;
	}

	ret = nvgpu_enable_pci_rail(pgpios);
	if (ret) {
		pr_err("nvgpu: enable pci rail failed\n");
		goto out;
	}

	ret = nvgpu_assert_pci_pwr_on(pgpios);
	if (ret) {
		pr_err("nvgpu: assert pci pwr on failed\n");
		goto out;
	}

	ret = nvgpu_check_pci_power_good(pgpios);
	if (ret) {
		pr_err("nvgpu: pci power is no good\n");
		goto out;
	}

	ret = nvgpu_attach_pci_gpu(pp);
	if (ret) {
		pr_err("nvgpu: attach pci gpu failed\n");
		goto out;
	}

	nvgpu_dump_pci_gpios(pgpios, __func__);

	nvgpu_mutex_release(&pp->mutex);

	time_ns = ktime_to_ns(ktime_sub(ktime_get(), time_start));
	nvgpu_update_power_on_stats(pp, time_ns);
	return 0;
out:
	nvgpu_mutex_release(&pp->mutex);
	return ret;
}

static int nvgpu_pci_gpu_power_off(char *dev_name)
{
	struct nvgpu_pci_power *pp;
	struct nvgpu_pci_gpios *pgpios;
	struct device *dev;
	struct gk20a *g;
	ktime_t time_start;
	s64 time_ns;
	int ret;

	pp = nvgpu_pci_get_pci_power(dev_name);
	if (!pp) {
		pr_err("nvgpu: no pci dev by name: %s\n", dev_name);
		return -ENODEV;
	}

	if (pp->can_pci_gc_off == false) {
		pr_err("nvgpu: gc-off not enabled for pdev: %s\n", dev_name);
		return -EPERM;
	}

	time_start = ktime_get();

	nvgpu_mutex_acquire(&pp->mutex);

	dev = &pp->pci_dev->dev;
	g = get_gk20a(dev);
	pgpios = &pp->gpios;

	nvgpu_start_gpu_idle(g);

	ret = nvgpu_wait_for_gpu_idle(g);
	if (ret) {
		pr_err("nvgpu: wait for gpu idle failed\n");
		goto out;
	}

	ret = nvgpu_request_pci_gpios(pgpios);
	if (ret) {
		pr_err("nvgpu: request pci gpios failed\n");
		goto out;
	}

	ret = nvgpu_detach_pci_gpu(pp);
	if (ret) {
		pr_err("nvgpu: detach pci gpu failed\n");
		goto out;
	}

	ret = nvgpu_deassert_pci_pwr_on(pgpios);
	if (ret) {
		pr_err("nvgpu: deassert pci pwr on failed\n");
		goto out;
	}

	ret = nvgpu_disable_pci_rail(pgpios);
	if (ret) {
		pr_err("nvgpu: disable pci rail failed\n");
		goto out;
	}

	nvgpu_dump_pci_gpios(pgpios, __func__);

	nvgpu_mutex_release(&pp->mutex);

	time_ns = ktime_to_ns(ktime_sub(ktime_get(), time_start));
	nvgpu_update_power_off_stats(pp, time_ns);
	return 0;
out:
	nvgpu_mutex_release(&pp->mutex);
	return ret;
}

int nvgpu_pci_set_powerstate(char *dev_name, int powerstate)
{
	int ret = 0;

	switch (powerstate) {
	case NVGPU_POWER_ON:
		ret = nvgpu_pci_gpu_power_on(dev_name);
		break;

	case NVGPU_POWER_OFF:
		ret = nvgpu_pci_gpu_power_off(dev_name);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static ssize_t poweroff_store(struct device_driver *drv,
			      const char *buf, size_t count)
{
	struct nvgpu_pci_power *pp;
	char dev_name[PCI_DEV_NAME_MAX];
	int ret;

	ret = nvgpu_pci_parse_dev_name(buf, count, dev_name);
	if (ret)
		return ret;

	pp = nvgpu_pci_get_pci_power(dev_name);
	if (!pp)
		return -ENODEV;

	ret = nvgpu_pci_set_powerstate(dev_name, NVGPU_POWER_OFF);
	if (ret) {
		pr_err("nvgpu: GPU(%s) POWER OFF failed\n", dev_name);
		return ret;
	}

	pr_debug("nvgpu: GPU(%s) POWER OFF done\n", dev_name);
	return count;
}

static DRIVER_ATTR_WO(poweroff);

static ssize_t poweron_store(struct device_driver *drv,
			     const char *buf, size_t count)
{
	struct nvgpu_pci_power *pp;
	char dev_name[PCI_DEV_NAME_MAX];
	int ret;

	ret = nvgpu_pci_parse_dev_name(buf, count, dev_name);
	if (ret)
		return ret;

	pp = nvgpu_pci_get_pci_power(dev_name);
	if (!pp)
		return -ENODEV;

	ret = nvgpu_pci_set_powerstate(dev_name, NVGPU_POWER_ON);
	if (ret) {
		pr_err("nvgpu: GPU(%s) POWER ON failed\n", dev_name);
		return ret;
	}

	pr_debug("nvgpu: GPU(%s) POWER ON done\n", dev_name);
	return count;
}

static DRIVER_ATTR_WO(poweron);

int __init nvgpu_pci_power_init(struct pci_driver *nvgpu_pci_driver)
{
	struct device_driver *driver = &nvgpu_pci_driver->driver;
	struct dentry *d;
	int ret;

	ret = driver_create_file(driver, &driver_attr_probed_gpus);
	if (ret)
		goto err_probed_gpus;

	d = debugfs_create_file("tegra_nvgpu_pci_power_stats", 0400,
				NULL, NULL, &debug_power_stats_fops);
	if (!d) {
		ret = -ENOENT;
		goto err_power_stats;
	}
	pci_power_stats_dbgfs_dentry = d;

	ret = driver_create_file(driver, &driver_attr_poweroff);
	if (ret)
		goto err_poweroff;

	ret = driver_create_file(driver, &driver_attr_poweron);
	if (ret)
		goto err_poweron;

	return 0;

err_poweron:
	driver_remove_file(driver, &driver_attr_poweroff);
err_poweroff:
	debugfs_remove(d);
	pci_power_stats_dbgfs_dentry = NULL;
err_power_stats:
	driver_remove_file(driver, &driver_attr_probed_gpus);
err_probed_gpus:
	return ret;
}

void __exit nvgpu_pci_power_exit(struct pci_driver *nvgpu_pci_driver)
{
	struct device_driver *driver = &nvgpu_pci_driver->driver;

	driver_remove_file(driver, &driver_attr_poweroff);
	driver_remove_file(driver, &driver_attr_poweron);

	debugfs_remove(pci_power_stats_dbgfs_dentry);

	driver_remove_file(driver, &driver_attr_probed_gpus);
}

void __exit nvgpu_pci_power_cleanup(void)
{
	struct nvgpu_pci_power *pp, *tmp_pp;

	list_for_each_entry_safe(pp, tmp_pp, &nvgpu_pci_power_devs, list)
		nvgpu_pci_remove_pci_power(pp);
}
