// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, NVIDIA CORPORATION. All rights reserved.
 */

#include <linux/cpufreq.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/platform/tegra/emc_bwmgr.h>

#include <soc/tegra/bpmp.h>
#include <soc/tegra/bpmp-abi.h>
#include <soc/tegra/cpufreq_cpu_emc_table.h>

#define KHZ                     		1000
#define EDVD_CORE_VOLT_FREQ(core)		(0x20 + (core) * 0x4)
#define EDVD_CORE_VOLT_FREQ_F_SHIFT		0
#define EDVD_CORE_VOLT_FREQ_F_MASK		0xffff
#define EDVD_CORE_VOLT_FREQ_V_SHIFT		16

struct tegra186_cpufreq_cluster_info {
	unsigned long offset;
	int cpus[4];
	unsigned int bpmp_cluster_id;
};

#define NO_CPU -1
static const struct tegra186_cpufreq_cluster_info tegra186_clusters[] = {
	/* Denver cluster */
	{
		.offset = SZ_64K * 7,
		.cpus = { 1, 2, NO_CPU, NO_CPU },
		.bpmp_cluster_id = 0,
	},
	/* A57 cluster */
	{
		.offset = SZ_64K * 6,
		.cpus = { 0, 3, 4, 5 },
		.bpmp_cluster_id = 1,
	},
};

struct tegra186_cpufreq_cluster {
	const struct tegra186_cpufreq_cluster_info *info;
	struct cpufreq_frequency_table *table;
	u32 ref_clk_khz;
	u32 div;
	struct tegra_bwmgr_client *bwmgr;
};

struct tegra186_cpufreq_data {
	void __iomem *regs;

	size_t num_clusters;
	struct tegra186_cpufreq_cluster *clusters;
	bool bypass_bwmgr_mode;
};

static struct cpu_emc_mapping dflt_t186_cpu_emc_mapping[] = {
	{ 450000,   408000},
	{ 800000,   660000},
	{1400000, UINT_MAX},
	{}, /* termination entry */
};

static struct cpu_emc_mapping *cpu_emc_mapping_dt;

static int tegra186_cpufreq_init(struct cpufreq_policy *policy)
{
	struct tegra186_cpufreq_data *data = cpufreq_get_driver_data();
	unsigned int i;

	for (i = 0; i < data->num_clusters; i++) {
		struct tegra186_cpufreq_cluster *cluster = &data->clusters[i];
		const struct tegra186_cpufreq_cluster_info *info =
			cluster->info;
		int core;

		for (core = 0; core < ARRAY_SIZE(info->cpus); core++) {
			if (info->cpus[core] == policy->cpu)
				break;
		}
		if (core == ARRAY_SIZE(info->cpus))
			continue;

		policy->driver_data =
			data->regs + info->offset + EDVD_CORE_VOLT_FREQ(core);
		policy->freq_table = cluster->table;

		if (!cluster->bwmgr) {
			cluster->bwmgr = tegra_bwmgr_register(i);
			if (IS_ERR_OR_NULL(cluster->bwmgr)) {
				pr_warn("cpufreq: fail to register with bwmgr");
				pr_warn("for cluster %d\n", i);
				return -ENODEV;
			}
		}
		break;
	}

	policy->cpuinfo.transition_latency = 300 * 1000;

	return 0;
}

/* Set emc clock by referring cpu_to_emc freq mapping */
static void tegra186_set_cpufreq_to_emcfreq(u32 cl, uint32_t cluster_freq)
{
	struct tegra186_cpufreq_data *data = cpufreq_get_driver_data();
	unsigned long emc_freq;

	if (data->bypass_bwmgr_mode)
		return;

	emc_freq = tegra_cpu_to_emc_freq(cluster_freq, cpu_emc_mapping_dt);
	if (!emc_freq)
		return;

	tegra_bwmgr_set_emc(data->clusters[cl].bwmgr, emc_freq * KHZ,
			    TEGRA_BWMGR_SET_EMC_FLOOR);
	pr_debug("cluster %d, emc freq(KHz): %lu cluster_freq(KHz): %u\n",
		 cl, emc_freq, cluster_freq);
}

static int tegra186_cpufreq_set_target(struct cpufreq_policy *policy,
				       unsigned int index)
{
	struct cpufreq_frequency_table *tbl = policy->freq_table + index;
	struct tegra186_cpufreq_data *data = cpufreq_get_driver_data();
	void __iomem *edvd_reg = policy->driver_data;
	u32 edvd_val = tbl->driver_data;
	int i;

	writel(edvd_val, edvd_reg);

	for (i = 0; i < data->num_clusters; i++) {
		struct tegra186_cpufreq_cluster *cluster = &data->clusters[i];
		int core;

		for (core = 0; core < ARRAY_SIZE(cluster->info->cpus); core++) {
			if (cluster->info->cpus[core] != policy->cpu)
				continue;
			if (!cluster->bwmgr)
				continue;

			tegra186_set_cpufreq_to_emcfreq(i, tbl->frequency);
			return 0;
		}
	}

	return 0;
}

static unsigned int tegra186_cpufreq_get(unsigned int cpu)
{
	struct tegra186_cpufreq_data *data = cpufreq_get_driver_data();
	struct cpufreq_policy *policy;
	void __iomem *edvd_reg;
	unsigned int i, freq = 0;
	u32 ndiv;

	policy = cpufreq_cpu_get(cpu);
	if (!policy)
		return 0;

	edvd_reg = policy->driver_data;
	ndiv = readl(edvd_reg) & EDVD_CORE_VOLT_FREQ_F_MASK;

	for (i = 0; i < data->num_clusters; i++) {
		struct tegra186_cpufreq_cluster *cluster = &data->clusters[i];
		int core;

		for (core = 0; core < ARRAY_SIZE(cluster->info->cpus); core++) {
			if (cluster->info->cpus[core] != policy->cpu)
				continue;

			freq = (cluster->ref_clk_khz * ndiv) / cluster->div;
			goto out;
		}
	}

out:
	cpufreq_cpu_put(policy);

	return freq;
}

static struct cpufreq_driver tegra186_cpufreq_driver = {
	.name = "tegra186",
	.flags = CPUFREQ_STICKY | CPUFREQ_HAVE_GOVERNOR_PER_POLICY |
			CPUFREQ_NEED_INITIAL_FREQ_CHECK,
	.get = tegra186_cpufreq_get,
	.verify = cpufreq_generic_frequency_table_verify,
	.target_index = tegra186_cpufreq_set_target,
	.init = tegra186_cpufreq_init,
	.attr = cpufreq_generic_attr,
};

static struct cpufreq_frequency_table *init_vhint_table(
	struct platform_device *pdev, struct tegra_bpmp *bpmp,
	struct tegra186_cpufreq_cluster *cluster)
{
	struct cpufreq_frequency_table *table;
	struct mrq_cpu_vhint_request req;
	struct tegra_bpmp_message msg;
	struct cpu_vhint_data *data;
	int err, i, j, num_rates = 0;
	dma_addr_t phys;
	void *virt;

	virt = dma_alloc_coherent(bpmp->dev, sizeof(*data), &phys,
				  GFP_KERNEL);
	if (!virt)
		return ERR_PTR(-ENOMEM);

	data = (struct cpu_vhint_data *)virt;

	memset(&req, 0, sizeof(req));
	req.addr = phys;
	req.cluster_id = cluster->info->bpmp_cluster_id;

	memset(&msg, 0, sizeof(msg));
	msg.mrq = MRQ_CPU_VHINT;
	msg.tx.data = &req;
	msg.tx.size = sizeof(req);

	err = tegra_bpmp_transfer(bpmp, &msg);
	if (err) {
		table = ERR_PTR(err);
		goto free;
	}
	if (msg.rx.ret) {
		table = ERR_PTR(-EINVAL);
		goto free;
	}

	for (i = data->vfloor; i <= data->vceil; i++) {
		u16 ndiv = data->ndiv[i];

		if (ndiv < data->ndiv_min || ndiv > data->ndiv_max)
			continue;

		/* Only store lowest voltage index for each rate */
		if (i > 0 && ndiv == data->ndiv[i - 1])
			continue;

		num_rates++;
	}

	table = devm_kcalloc(&pdev->dev, num_rates + 1, sizeof(*table),
			     GFP_KERNEL);
	if (!table) {
		table = ERR_PTR(-ENOMEM);
		goto free;
	}

	cluster->ref_clk_khz = data->ref_clk_hz / 1000;
	cluster->div = data->pdiv * data->mdiv;

	for (i = data->vfloor, j = 0; i <= data->vceil; i++) {
		struct cpufreq_frequency_table *point;
		u16 ndiv = data->ndiv[i];
		u32 edvd_val = 0;

		if (ndiv < data->ndiv_min || ndiv > data->ndiv_max)
			continue;

		/* Only store lowest voltage index for each rate */
		if (i > 0 && ndiv == data->ndiv[i - 1])
			continue;

		edvd_val |= i << EDVD_CORE_VOLT_FREQ_V_SHIFT;
		edvd_val |= ndiv << EDVD_CORE_VOLT_FREQ_F_SHIFT;

		point = &table[j++];
		point->driver_data = edvd_val;
		point->frequency = (cluster->ref_clk_khz * ndiv) / cluster->div;
	}

	table[j].frequency = CPUFREQ_TABLE_END;

free:
	dma_free_coherent(bpmp->dev, sizeof(*data), virt, phys);

	return table;
}

static int tegra186_cpufreq_probe(struct platform_device *pdev)
{
	struct tegra186_cpufreq_data *data;
	struct device_node *dn = NULL;
	struct tegra_bpmp *bpmp;
	unsigned int i = 0, err;

	dn = pdev->dev.of_node;
	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->clusters = devm_kcalloc(&pdev->dev, ARRAY_SIZE(tegra186_clusters),
				      sizeof(*data->clusters), GFP_KERNEL);
	if (!data->clusters)
		return -ENOMEM;

	data->num_clusters = ARRAY_SIZE(tegra186_clusters);

	bpmp = tegra_bpmp_get(&pdev->dev);
	if (IS_ERR(bpmp))
		return PTR_ERR(bpmp);

	data->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(data->regs)) {
		err = PTR_ERR(data->regs);
		goto put_bpmp;
	}

	cpu_emc_mapping_dt = tegra_cpufreq_cpu_emc_map_dt_init(dn);
	if (!cpu_emc_mapping_dt) {
		cpu_emc_mapping_dt = dflt_t186_cpu_emc_mapping;
		pr_info("CPU EMC frequency map table from default setting\n");
	}

	for (i = 0; i < data->num_clusters; i++) {
		struct tegra186_cpufreq_cluster *cluster = &data->clusters[i];

		cluster->info = &tegra186_clusters[i];
		cluster->table = init_vhint_table(pdev, bpmp, cluster);
		if (IS_ERR(cluster->table)) {
			err = PTR_ERR(cluster->table);
			goto put_bpmp;
		}
	}

	tegra186_cpufreq_driver.driver_data = data;

	err = cpufreq_register_driver(&tegra186_cpufreq_driver);

put_bpmp:
	tegra_bpmp_put(bpmp);

	return err;
}

static int tegra186_cpufreq_remove(struct platform_device *pdev)
{
	cpufreq_unregister_driver(&tegra186_cpufreq_driver);

	return 0;
}

static const struct of_device_id tegra186_cpufreq_of_match[] = {
	{ .compatible = "nvidia,tegra186-ccplex-cluster", },
	{ }
};
MODULE_DEVICE_TABLE(of, tegra186_cpufreq_of_match);

#ifdef CONFIG_PM_SLEEP
static int tegra186_cpufreq_suspend(struct device *dev)
{
	struct tegra186_cpufreq_data *data = cpufreq_get_driver_data();

	data->bypass_bwmgr_mode = true;
	return 0;
}

static int tegra186_cpufreq_resume(struct device *dev)
{
	struct tegra186_cpufreq_data *data = cpufreq_get_driver_data();

	data->bypass_bwmgr_mode = false;
	return 0;
}
#endif

static const struct dev_pm_ops tegra186_cpufreq_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(tegra186_cpufreq_suspend, tegra186_cpufreq_resume)
};

static struct platform_driver tegra186_cpufreq_platform_driver = {
	.driver = {
		.name = "tegra186-cpufreq",
		.of_match_table = tegra186_cpufreq_of_match,
		.pm = &tegra186_cpufreq_pm_ops,
	},
	.probe = tegra186_cpufreq_probe,
	.remove = tegra186_cpufreq_remove,
};
module_platform_driver(tegra186_cpufreq_platform_driver);

MODULE_AUTHOR("Mikko Perttunen <mperttunen@nvidia.com>");
MODULE_DESCRIPTION("NVIDIA Tegra186 cpufreq driver");
MODULE_LICENSE("GPL v2");
