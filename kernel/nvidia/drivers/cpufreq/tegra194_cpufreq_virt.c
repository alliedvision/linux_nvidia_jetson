/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/version.h>
#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif
#include <linux/tegra-cpufreq.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <soc/tegra/bpmp_abi.h>
#include <soc/tegra/virt/syscalls.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include "tegra194-cpufreq.h"

#define US_DELAY_MIN		20
#define MAX_CPU_PER_CLUSTER	2
#define REF_CLK_MHZ		408 /* 408 MHz */

#define LOOP_FOR_EACH_CPU_OF_CLUSTER(cl) for (cpu_id = (cl * 2); \
					 cpu_id < ((cl+1) * 2); cpu_id++)

#define LOOP_FOR_EACH_CLUSTER(cl)	for (cl = CLUSTER0; \
					cl < MAX_CLUSTERS; cl++)

#define to_cpufreq_obj(x) container_of(x, struct cpufreq_obj, kobj)
#define to_cpufreq_attr(x) container_of(x, struct cpufreq_attribute, attr)

struct cpufreq_obj {
	struct kobject kobj;
	int cpufreq;
};

struct cpufreq_virt {
	struct cpufreq_obj freq_obj;
	uint8_t cpu_id;
	struct mutex read_lock;
};

struct cpufreq_attribute {
	struct attribute attr;
	ssize_t (*show)(struct cpufreq_obj *cpufreq,
			struct cpufreq_attribute *attr,
			char *buf);
	ssize_t (*store)(struct cpufreq_obj *cpufreq,
			struct cpufreq_attribute *attr,
			const char *buf,
			size_t count);
};

static ssize_t get_pct_cpu_id_freq(struct cpufreq_obj *kobj,
				struct cpufreq_attribute *attr,
				char *buf);
static ssize_t set_pct_cpu_id_freq(struct cpufreq_obj *kobj,
				struct cpufreq_attribute *attr,
				const char *buf,
				size_t n);

static struct cpufreq_virt *cpufreq_virt_data;

/* Cluster lock for  ndiv writes */
static struct mutex cl_mlock[MAX_CLUSTERS];

static struct tegra_cpu_ctr cpu_cntr[MAX_CLUSTERS * MAX_CPU_PER_CLUSTER];

static ssize_t cpufreq_attr_show(struct kobject *kobj,
			     struct attribute *attr,
			     char *buf)
{
	struct cpufreq_attribute *attribute;
	struct cpufreq_obj *cpufreq;

	attribute = to_cpufreq_attr(attr);
	cpufreq = to_cpufreq_obj(kobj);

	if (!attribute->show)
		return -EIO;

	return attribute->show(cpufreq, attribute, buf);
}

static ssize_t cpufreq_attr_store(struct kobject *kobj,
			      struct attribute *attr,
			      const char *buf, size_t len)
{
	struct cpufreq_attribute *attribute;
	struct cpufreq_obj *cpufreq;

	attribute = to_cpufreq_attr(attr);
	cpufreq = to_cpufreq_obj(kobj);

	if (!attribute->store)
		return -EIO;

	return attribute->store(cpufreq, attribute, buf, len);
}
static const struct sysfs_ops cpufreq_sysfs_ops = {
	.show = cpufreq_attr_show,
	.store = cpufreq_attr_store,
};

static struct cpufreq_attribute cpufreq_attr =
	 __ATTR(pct_cpu_id_freq, 0600, get_pct_cpu_id_freq,
		set_pct_cpu_id_freq);


static void cpufreq_release(struct kobject *kobj)
{
	kobject_put(kobj);
}

static struct attribute *cpufreq_default_attrs[] = {
	&cpufreq_attr.attr,
	NULL, /* need to NULL terminate the list of attributes */
};

static struct kobj_type cpufreq_ktype = {
	.sysfs_ops = &cpufreq_sysfs_ops,
	.release = cpufreq_release,
	.default_attrs = cpufreq_default_attrs,
};

static ssize_t set_pct_cpu_id_freq(struct cpufreq_obj *kobj,
				struct cpufreq_attribute *attr,
				const char *buf, size_t count)
{
	uint16_t ndiv;
	uint32_t rate;
	uint64_t val;
	struct mrq_cpu_ndiv_limits_response *nltbl  = NULL;
	struct cpufreq_virt *cpufreq = NULL;
	enum cluster cl;
	uint8_t cpu_id;
	int16_t ret = 0;

	if (kobj) {
		cpufreq = container_of(kobj, struct cpufreq_virt, freq_obj);
		if (!cpufreq) {
			pr_err("Failed to set CPU clock: invalid kobj\n");
			return -EINVAL;
		}
	}

	ret = kstrtouint(buf, 10, &rate);
	if (ret < 0)
		return ret;

	/* CPU number enumeration is considered from PCT configuration
	 * which is logical cpu 0 to  maximum 8.
	 */
	cl = (enum cluster)(cpufreq->cpu_id/2);
	nltbl = get_ndiv_limits(cl);

	if (!nltbl) {
		pr_err("Failed to get Ndiv limits for cl:%d\n", cl);
		return -EINVAL;
	}

	if (!nltbl->ref_clk_hz) {
		pr_err("Failed to set CPU clock: invalid ref clk\n");
		return -EINVAL;
	}
	ndiv = map_freq_to_ndiv(nltbl, rate);
	ndiv = clamp_ndiv(nltbl, ndiv);
	val = (uint64_t)ndiv;

	mutex_lock(&cl_mlock[cl]);
	LOOP_FOR_EACH_CPU_OF_CLUSTER(cl) {
		if (hyp_pct_cpu_id_write_freq_request(cpu_id, val) != 1) {
			ret = -EINVAL;
			break;
		}
	}
	mutex_unlock(&cl_mlock[cl]);
	return count;
}

static struct tegra_cpu_ctr *tegra_read_counters_pct_id(uint8_t cpu)
{
	uint64_t val = 0;
	struct tegra_cpu_ctr *c;

	mutex_lock(&(cpufreq_virt_data[cpu].read_lock));
	c = &cpu_cntr[cpu];
	hyp_pct_cpu_id_read_freq_feedback(cpu, &val);
	c->last_refclk_cnt = (uint32_t)(val & 0xffffffff);
	c->last_coreclk_cnt = (uint32_t) (val >> 32);
	udelay(c->delay);
	hyp_pct_cpu_id_read_freq_feedback(cpu, &val);
	c->refclk_cnt = (uint32_t)(val & 0xffffffff);
	c->coreclk_cnt = (uint32_t) (val >> 32);
	mutex_unlock(&(cpufreq_virt_data[cpu].read_lock));

	return c;
}

static ssize_t get_pct_cpu_id_freq(struct cpufreq_obj *kobj,
				struct cpufreq_attribute *attr,
				char *buf)
{
	uint32_t delta_ccnt = 0;
	uint32_t delta_refcnt = 0;
	unsigned long rate_mhz = 0;
	struct tegra_cpu_ctr *c;
	struct cpufreq_virt *cpufreq = NULL;

	if (kobj) {
		cpufreq = container_of(kobj, struct cpufreq_virt, freq_obj);
		if (!cpufreq) {
			pr_err("Failed to set CPU clock: invalid kobj\n");
			return -EINVAL;
		}
	}
	c = tegra_read_counters_pct_id(cpufreq->cpu_id);
	delta_ccnt = c->coreclk_cnt - c->last_coreclk_cnt;
	if (!delta_ccnt)
		goto err_out;

	/* ref clock is 32 bits */
	delta_refcnt = c->refclk_cnt - c->last_refclk_cnt;
	if (!delta_refcnt)
		goto err_out;
	rate_mhz = ((unsigned long) delta_ccnt * REF_CLK_MHZ) / delta_refcnt;
err_out:
	return sprintf(buf, "%u\n", (uint32_t)(rate_mhz * 1000)); /* in KHz */
}

void cpufreq_hv_init(struct kobject *kobj)
{
	uint16_t i;
	enum cluster cl;
	int16_t retval;
	uint8_t cpu_name[5] = {'c', 'p', 'u'};
	uint8_t cpu_count = hyp_get_cpu_count();

	if (cpu_count == 0)
		cpu_count = MAX_CLUSTERS * MAX_CPU_PER_CLUSTER;

	if (cpu_count > (MAX_CLUSTERS * MAX_CPU_PER_CLUSTER))
		pr_err("%s: Invalid cpu count:%d\n", __func__, cpu_count);

	if (kobj == NULL) {
		pr_err("%s:kobj is NULL\n", __func__);
		return;
	}

	cpufreq_virt_data = kcalloc(cpu_count, sizeof(struct cpufreq_virt),
								GFP_KERNEL);
	if (!cpufreq_virt_data) {
		pr_err("%s: Memory allocation failed\n", __func__);
		return;
	}

	LOOP_FOR_EACH_CLUSTER(cl) {
		mutex_init(&cl_mlock[cl]);
	}

	for (i = 0; i < cpu_count; i++) {
		cpu_name[3] = i + '0';
		cpu_name[4] = '\0';

		mutex_init(&cpufreq_virt_data[i].read_lock);
		cpu_cntr[i].delay = US_DELAY_MIN;
		cpufreq_virt_data[i].cpu_id = i;
		retval = kobject_init_and_add(
				&cpufreq_virt_data[i].freq_obj.kobj,
				&cpufreq_ktype, kobj, "%s",
				cpu_name);
		if (retval) {
			kobject_put(&cpufreq_virt_data[i].freq_obj.kobj);
			return;
		}
	}
}

