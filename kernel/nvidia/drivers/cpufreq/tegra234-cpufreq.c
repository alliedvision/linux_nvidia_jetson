/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/interconnect.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/platform/tegra/mc_utils.h>
#include <linux/version.h>
#include <soc/tegra/bpmp.h>
#include <soc/tegra/cpufreq_cpu_emc_table.h>
#include <dt-bindings/interconnect/tegra_icc_id.h>

#define KHZ				1000
#define REF_CLK_MHZ			408 /* 408 MHz */
#define US_DELAY			500
#define US_DELAY_MIN			2
#define CPUFREQ_TBL_STEP_HZ		(50 * KHZ * KHZ)
#define MAX_CNT				~0U
#define MAX_CORES_PER_CLUSTER		4
#define SCRATCH_FREQ_CORE_BASE		0x2000
#define SCRATCH_FREQ_CORE_REG(mpidr)	(mpidr * 8)
#define CLUSTER_ACTMON_BASE(cl)		(0x30000 + (cl * 0x10000) + 0x9000)
#define CORE_ACTMON_REG(core)		(core * 8)
#define NDIV_MASK			0x1FF

/* cpufreq transisition latency */
#define TEGRA_CPUFREQ_TRANSITION_LATENCY (300 * 1000) /* unit in nanoseconds */

#define LOOP_FOR_EACH_CLUSTER(cl)	for (cl = 0; \
						cl < MAX_CLUSTERS; cl++)

enum cluster {
	CLUSTER0,
	CLUSTER1,
	CLUSTER2,
	MAX_CLUSTERS,
};

struct tegra234_cpufreq_data {
	void __iomem *regs;
	size_t num_clusters;
	struct icc_path **icc_handle;
	struct cpufreq_frequency_table **tables;
	struct cpumask *cl_cpu_mask;
	struct mrq_cpu_ndiv_limits_response *ndiv_limits;
	bool bypass_icc;
};

struct tegra_cpu_ctr {
	u32 cpu;
	u32 delay;
	u32 coreclk_cnt, last_coreclk_cnt;
	u32 refclk_cnt, last_refclk_cnt;
};

struct read_counters_work {
	struct work_struct work;
	struct tegra_cpu_ctr c;
};

struct mpidr {
	uint32_t cl;
	uint32_t cpu;
};

static struct workqueue_struct *read_counters_wq;

static struct cpu_emc_mapping *cpu_emc_map_ptr;

static void get_mpidr_id(void *id)
{
	u64 mpidr = read_cpuid_mpidr() & MPIDR_HWID_BITMASK;

	((struct mpidr *)id)->cl = MPIDR_AFFINITY_LEVEL(mpidr, 2);
	((struct mpidr *)id)->cpu = MPIDR_AFFINITY_LEVEL(mpidr, 1);
}

static inline u32 map_ndiv_to_freq(struct mrq_cpu_ndiv_limits_response
				   *nltbl, u16 ndiv)
{
	return (nltbl->ref_clk_hz / KHZ) * ndiv / (nltbl->pdiv * nltbl->mdiv);
}

static void tegra_read_counters(struct work_struct *work)
{
	struct tegra234_cpufreq_data *data = cpufreq_get_driver_data();
	struct read_counters_work *read_counters_work;
	struct tegra_cpu_ctr *c;
	struct mpidr id;
	void __iomem *actmon_reg;
	u64 val;

	/*
	 * ref_clk_counter(32 bit counter) runs on constant clk,
	 * pll_p(408MHz).
	 * It will take = 2 ^ 32 / 408 MHz to overflow ref clk counter
	 *              = 10526880 usec = 10.527 sec to overflow
	 *
	 * Like wise core_clk_counter(32 bit counter) runs on core clock.
	 * It's synchronized to crab_clk (cpu_crab_clk) which runs at
	 * freq of cluster. Assuming max cluster clock ~2000MHz,
	 * It will take = 2 ^ 32 / 2000 MHz to overflow core clk counter
	 *              = ~2.147 sec to overflow
	 */
	read_counters_work = container_of(work, struct read_counters_work,
					  work);
	c = &read_counters_work->c;
	get_mpidr_id((void *)&id);
	actmon_reg = data->regs + (CLUSTER_ACTMON_BASE(id.cl)
						+ CORE_ACTMON_REG(id.cpu));

	val = readq(actmon_reg);
	c->last_refclk_cnt = upper_32_bits(val);
	c->last_coreclk_cnt = lower_32_bits(val);
	udelay(c->delay);
	val = readq(actmon_reg);
	c->refclk_cnt = upper_32_bits(val);
	c->coreclk_cnt = lower_32_bits(val);
}

/*
 * Return instantaneous cpu speed
 * Instantaneous freq is calculated as -
 * -Takes sample on every query of getting the freq.
 *	- Read core and ref clock counters;
 *	- Delay for X us
 *	- Read above cycle counters again
 *	- Calculates freq by subtracting current and previous counters
 *	  divided by the delay time or eqv. of ref_clk_counter in delta time
 *	- Return Kcycles/second, freq in KHz
 *
 *	delta time period = x sec
 *			  = delta ref_clk_counter / (408 * 10^6) sec
 *	freq in Hz = cycles/sec
 *		   = (delta cycles / x sec
 *		   = (delta cycles * 408 * 10^6) / delta ref_clk_counter
 *	in KHz	   = (delta cycles * 408 * 10^3) / delta ref_clk_counter
 *
 * @cpu - logical cpu whose freq to be updated
 * Returns freq in KHz on success, 0 if cpu is offline
 */
static unsigned int tegra234_get_speed_common(u32 cpu, u32 delay)
{
	struct read_counters_work read_counters_work;
	struct tegra_cpu_ctr c;
	u32 delta_refcnt;
	u32 delta_ccnt;
	u32 rate_mhz;

	/*
	 * udelay() is required to reconstruct cpu frequency over an
	 * observation window. Using workqueue to call udelay() with
	 * interrupts enabled.
	 */
	memset(&read_counters_work, 0, sizeof(struct read_counters_work));
	read_counters_work.c.cpu = cpu;
	read_counters_work.c.delay = delay;
	INIT_WORK_ONSTACK(&read_counters_work.work, tegra_read_counters);
	queue_work_on(cpu, read_counters_wq, &read_counters_work.work);
	flush_work(&read_counters_work.work);
	c = read_counters_work.c;

	if (c.coreclk_cnt < c.last_coreclk_cnt)
		delta_ccnt = c.coreclk_cnt + (MAX_CNT - c.last_coreclk_cnt);
	else
		delta_ccnt = c.coreclk_cnt - c.last_coreclk_cnt;
	if (!delta_ccnt)
		return 0;

	/* ref clock is 32 bits */
	if (c.refclk_cnt < c.last_refclk_cnt)
		delta_refcnt = c.refclk_cnt + (MAX_CNT - c.last_refclk_cnt);
	else
		delta_refcnt = c.refclk_cnt - c.last_refclk_cnt;
	if (!delta_refcnt) {
		pr_debug("cpufreq: %d is idle, delta_refcnt: 0\n", cpu);
		return 0;
	}
	rate_mhz = ((unsigned long)(delta_ccnt * REF_CLK_MHZ)) / delta_refcnt;

	return (rate_mhz * KHZ); /* in KHz */
}

static unsigned int tegra234_get_speed(u32 cpu)
{
	return tegra234_get_speed_common(cpu, US_DELAY);
}

static int tegra234_cpufreq_init(struct cpufreq_policy *policy)
{
	struct tegra234_cpufreq_data *data = cpufreq_get_driver_data();
	struct mpidr id;
	void __iomem *scratch_freq_core_reg;
	uint32_t mpidr_id, ndiv;

	smp_call_function_single(policy->cpu, get_mpidr_id, &id, true);

	if (id.cl >= data->num_clusters)
		return -EINVAL;

	/* boot freq */
	mpidr_id = (id.cl * MAX_CORES_PER_CLUSTER) + id.cpu;
	scratch_freq_core_reg = SCRATCH_FREQ_CORE_REG(mpidr_id) +
				SCRATCH_FREQ_CORE_BASE + data->regs;
	ndiv = readl(scratch_freq_core_reg) & NDIV_MASK;
	policy->cur = map_ndiv_to_freq(&data->ndiv_limits[id.cl], ndiv);

	/* set same policy for all cpus in a cluster */
	cpumask_copy(policy->cpus, &(data->cl_cpu_mask[id.cl]));

	policy->freq_table = data->tables[id.cl];
	policy->cpuinfo.transition_latency = TEGRA_CPUFREQ_TRANSITION_LATENCY;
	policy->driver_data = data->regs; /* MMCRAB base */

	return 0;
}

static void set_cpu_ndiv(int cpu, void __iomem *freq_base, unsigned int ndiv)
{
	struct mpidr id;
	void __iomem *scratch_freq_core_reg;
	uint32_t mpidr_id;

	smp_call_function_single(cpu, get_mpidr_id, &id, true);
	mpidr_id = (id.cl * MAX_CORES_PER_CLUSTER) + id.cpu;
	scratch_freq_core_reg = SCRATCH_FREQ_CORE_REG(mpidr_id) + freq_base;
	writel(ndiv, scratch_freq_core_reg);
}

/* Set emc clock by referring cpu_to_emc freq mapping */
static void set_cpufreq_to_emcfreq(enum cluster cl, uint32_t cluster_freq)
{
	struct tegra234_cpufreq_data *data = cpufreq_get_driver_data();
	unsigned long emc_freq_khz;
	unsigned long emc_freq_kbps;

	if (!data->icc_handle[cl] || data->bypass_icc)
		return;

	emc_freq_khz = tegra_cpu_to_emc_freq(cluster_freq, cpu_emc_map_ptr);

	emc_freq_kbps = emc_freq_to_bw(emc_freq_khz);

	icc_set_bw(data->icc_handle[cl], 0, emc_freq_kbps);
	pr_debug("cluster %d, emc freq(KHz): %lu cluster_freq(KHz): %u\n",
		 cl, emc_freq_khz, cluster_freq);
}

static int tegra234_cpufreq_set_target(struct cpufreq_policy *policy,
				       unsigned int index)
{
	struct cpufreq_frequency_table *tbl = policy->freq_table + index;
	void __iomem *freq_base = policy->driver_data + SCRATCH_FREQ_CORE_BASE;
	unsigned int ndiv = tbl->driver_data;
	struct mpidr id;
	int cpu;

	for_each_cpu_and(cpu, policy->cpus, cpu_online_mask)
		set_cpu_ndiv(cpu, freq_base, ndiv);

	if (cpu_emc_map_ptr) {
		smp_call_function_single(policy->cpu, get_mpidr_id, &id, true);
		set_cpufreq_to_emcfreq(id.cl, tbl->frequency);
	}

	return 0;
}

static struct cpufreq_driver tegra234_cpufreq_driver = {
	.name = "tegra234",
	.flags = CPUFREQ_STICKY | CPUFREQ_CONST_LOOPS |
		CPUFREQ_NEED_INITIAL_FREQ_CHECK | CPUFREQ_IS_COOLING_DEV,
	.verify = cpufreq_generic_frequency_table_verify,
	.target_index = tegra234_cpufreq_set_target,
	.get = tegra234_get_speed,
	.init = tegra234_cpufreq_init,
	.attr = cpufreq_generic_attr,
};

static void tegra234_cpufreq_free_resources(void)
{
	struct tegra234_cpufreq_data *data = cpufreq_get_driver_data();
	enum cluster cl;

	destroy_workqueue(read_counters_wq);

	LOOP_FOR_EACH_CLUSTER(cl) {
		if (data->icc_handle[cl])
			icc_put(data->icc_handle[cl]);
	}

	kfree(cpu_emc_map_ptr);
}

static struct cpufreq_frequency_table *
init_freq_table(struct platform_device *pdev, struct tegra_bpmp *bpmp,
	unsigned int cluster_id, struct mrq_cpu_ndiv_limits_response *resp)
{
	struct cpufreq_frequency_table *freq_table;
	unsigned int num_freqs, ndiv, delta_ndiv;
	struct mrq_cpu_ndiv_limits_request req;
	struct tegra_bpmp_message msg;
	int err;
	u16 freq_table_step_size;
	int index;

	memset(&req, 0, sizeof(req));
	req.cluster_id = cluster_id;

	memset(&msg, 0, sizeof(msg));
	msg.mrq = MRQ_CPU_NDIV_LIMITS;
	msg.tx.data = &req;
	msg.tx.size = sizeof(req);
	msg.rx.data = resp;
	msg.rx.size = sizeof(*resp);

	err = tegra_bpmp_transfer(bpmp, &msg);
	if (err)
		return ERR_PTR(err);
	if (msg.rx.ret == -BPMP_EINVAL) {
		/* Cluster not available */
		return NULL;
	}
	if (msg.rx.ret)
		return ERR_PTR(-EINVAL);

	/*
	 * Make sure frequency table step is a multiple of mdiv to match
	 * vhint table granularity.
	 */
	freq_table_step_size = resp->mdiv *
			DIV_ROUND_UP(CPUFREQ_TBL_STEP_HZ, resp->ref_clk_hz);

	dev_dbg(&pdev->dev, "cluster %d: frequency table step size: %d\n",
		cluster_id, freq_table_step_size);

	delta_ndiv = resp->ndiv_max - resp->ndiv_min;

	if (unlikely(delta_ndiv == 0)) {
		num_freqs = 1;
	} else {
		/* We store both ndiv_min and ndiv_max hence the +1 */
		num_freqs = delta_ndiv / freq_table_step_size + 1;
	}

	num_freqs += (delta_ndiv % freq_table_step_size) ? 1 : 0;

	freq_table = devm_kcalloc(&pdev->dev, num_freqs + 1,
				  sizeof(*freq_table), GFP_KERNEL);
	if (!freq_table)
		return ERR_PTR(-ENOMEM);

	for (index = 0, ndiv = resp->ndiv_min;
			ndiv < resp->ndiv_max;
			index++, ndiv += freq_table_step_size) {
		freq_table[index].driver_data = ndiv;
		freq_table[index].frequency = map_ndiv_to_freq(resp, ndiv);
	}

	freq_table[index].driver_data = resp->ndiv_max;
	freq_table[index++].frequency = map_ndiv_to_freq(resp, resp->ndiv_max);
	freq_table[index].frequency = CPUFREQ_TABLE_END;

	return freq_table;
}

#ifdef CONFIG_DEBUG_FS
static struct dentry *tegra_cpufreq_debugfs_root;

static int tegra_cpufreq_debug_init(void)
{
	tegra_cpufreq_debugfs_root = debugfs_create_dir("tegra_cpufreq", NULL);
	if (!tegra_cpufreq_debugfs_root)
		return -ENOMEM;

	if (!tegra_debugfs_create_cpu_emc_map(tegra_cpufreq_debugfs_root,
					cpu_emc_map_ptr))
		goto err_out;

	return 0;

err_out:
	return -EINVAL;
}

static void tegra_cpufreq_debug_exit(void)
{
	debugfs_remove_recursive(tegra_cpufreq_debugfs_root);
}
#endif

static int tegra234_cpufreq_probe(struct platform_device *pdev)
{
	struct tegra234_cpufreq_data *data;
	struct tegra_bpmp *bpmp;
	struct resource *res;
	struct device_node *dn;
	int err, i, cpu, cl;
	const int icc_id_array[MAX_CLUSTERS] = {
		TEGRA_ICC_CPU_CLUSTER0,
		TEGRA_ICC_CPU_CLUSTER1,
		TEGRA_ICC_CPU_CLUSTER2
	};

	bpmp = tegra_bpmp_get(&pdev->dev);
	if (IS_ERR(bpmp))
		return -EPROBE_DEFER;

	dn = pdev->dev.of_node;
	cpu_emc_map_ptr = tegra_cpufreq_cpu_emc_map_dt_init(dn);
	if (!cpu_emc_map_ptr)
		dev_info(&pdev->dev, "cpu_emc_map not present\n");

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto err_free_map_ptr;
	}

	data->num_clusters = MAX_CLUSTERS;
	data->tables = devm_kcalloc(&pdev->dev, data->num_clusters,
				    sizeof(*data->tables), GFP_KERNEL);
	if (!data->tables) {
		err = -ENOMEM;
		goto err_free_map_ptr;
	}

	data->ndiv_limits = devm_kcalloc(&pdev->dev, data->num_clusters,
				    sizeof(*data->ndiv_limits), GFP_KERNEL);
	if (!data->ndiv_limits) {
		err = -ENOMEM;
		goto err_free_map_ptr;
	}

	data->icc_handle = devm_kcalloc(&pdev->dev, data->num_clusters,
				   sizeof(*data->icc_handle), GFP_KERNEL);
	if (!data->icc_handle) {
		err = -ENOMEM;
		goto err_free_map_ptr;
	}

	data->cl_cpu_mask = devm_kcalloc(&pdev->dev, data->num_clusters,
				   sizeof(*data->cl_cpu_mask), GFP_KERNEL);
	if (!data->cl_cpu_mask) {
		err = -ENOMEM;
		goto err_free_map_ptr;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	data->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(data->regs)) {
		err = PTR_ERR(data->regs);
		goto err_free_map_ptr;
	}

	platform_set_drvdata(pdev, data);

	/* set cluster cpu mask */
	for_each_possible_cpu(cpu) {
		cl = MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 2);
		cpumask_set_cpu(cpu, &(data->cl_cpu_mask[cl]));
	}

	read_counters_wq = alloc_workqueue("read_counters_wq", __WQ_LEGACY, 1);
	if (!read_counters_wq) {
		dev_err(&pdev->dev, "fail to create_workqueue\n");
		err = -EINVAL;
		goto err_free_map_ptr;
	}

	for (i = 0; i < data->num_clusters; i++) {
		data->tables[i] = init_freq_table(pdev, bpmp, i,
							&data->ndiv_limits[i]);
		if (IS_ERR(data->tables[i])) {
			err = PTR_ERR(data->tables[i]);
			goto err_free_res;
		}

		data->icc_handle[i] = icc_get(&pdev->dev,
					icc_id_array[i], TEGRA_ICC_MASTER);
		if (IS_ERR_OR_NULL(data->icc_handle[i])) {
			dev_err(&pdev->dev, "cpufreq icc register failed\n");
			data->icc_handle[i] = NULL;
		}
	}

#ifdef CONFIG_DEBUG_FS
	err = tegra_cpufreq_debug_init();
	if (err) {
		pr_err("tegra234-cpufreq: failed to create debugfs nodes\n");
		goto err_free_res;
	}
#endif

	tegra234_cpufreq_driver.driver_data = data;

	err = cpufreq_register_driver(&tegra234_cpufreq_driver);
	if (!err)
		goto put_bpmp;

err_free_res:
	tegra234_cpufreq_free_resources();
err_free_map_ptr:
	if (cpu_emc_map_ptr)
		kfree(cpu_emc_map_ptr);
put_bpmp:
	tegra_bpmp_put(bpmp);
	return err;
}

static int tegra234_cpufreq_remove(struct platform_device *pdev)
{
#ifdef CONFIG_DEBUG_FS
	tegra_cpufreq_debug_exit();
#endif
	cpufreq_unregister_driver(&tegra234_cpufreq_driver);
	tegra234_cpufreq_free_resources();

	return 0;
}

static const struct of_device_id tegra234_cpufreq_of_match[] = {
	{ .compatible = "nvidia,t234-cpufreq", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, tegra234_cpufreq_of_match);

#ifdef CONFIG_PM_SLEEP
static int tegra234_cpufreq_suspend(struct device *dev)
{
	struct tegra234_cpufreq_data *data = cpufreq_get_driver_data();

	data->bypass_icc = true;
	return 0;
}

static int tegra234_cpufreq_resume(struct device *dev)
{
	struct tegra234_cpufreq_data *data = cpufreq_get_driver_data();

	data->bypass_icc = false;
	return 0;
}
#endif

static const struct dev_pm_ops tegra234_cpufreq_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(tegra234_cpufreq_suspend, tegra234_cpufreq_resume)
};

static struct platform_driver tegra234_ccplex_driver = {
	.driver = {
		.name = "tegra234-cpufreq",
		.of_match_table = tegra234_cpufreq_of_match,
		.pm = &tegra234_cpufreq_pm_ops,
	},
	.probe = tegra234_cpufreq_probe,
	.remove = tegra234_cpufreq_remove,
};
module_platform_driver(tegra234_ccplex_driver);

MODULE_AUTHOR("Sanjay Chandrashekara <sanjayc@nvidia.com>");
MODULE_DESCRIPTION("NVIDIA Tegra234 cpufreq driver");
MODULE_LICENSE("GPL v2");
