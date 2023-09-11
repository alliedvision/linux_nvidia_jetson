/*
 * MC StreamID configuration
 *
 * Copyright (c) 2015-2023, NVIDIA CORPORATION.  All rights reserved.
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

#define pr_fmt(fmt)	"%s(): " fmt, __func__

#include <linux/err.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
#include <soc/tegra/tegra-sid-override.h>
#endif

#include <linux/platform/tegra/tegra-mc-sid.h>
#include <dt-bindings/memory/tegra-swgroup.h>

#define TO_MC_SID_STREAMID_SECURITY_CONFIG(addr)	(addr + sizeof(u32))

static LIST_HEAD(sid_override_list);

struct tegra_mc_sid_override {
	struct list_head list;
	void __iomem *addr;
	unsigned long sid;
};

struct tegra_mc_sid {
	struct device *dev;
	void __iomem *base;
	void __iomem *sid_base;
	const struct tegra_mc_sid_soc_data *soc_data;
	struct dentry *debugfs_root;
};

static struct tegra_mc_sid *mc_sid;

/*
 * Return a string with the name associated with the passed StreamID.
 */
const char *tegra_mc_get_sid_name(int sid)
{
	int i;
	struct sid_to_oids *entry;

	if (!mc_sid) {
		pr_err("mc-sid isn't populated yet\n");
		goto end;
	}

	for (i = 0; i < mc_sid->soc_data->nsid_to_oids; i++) {
		entry = &mc_sid->soc_data->sid_to_oids[i];

		if (entry->sid == sid) {
			if (!entry->name)
				pr_err("Entry is missing name\n");
			return entry->name;
		}
	}

end:
	if (sid > TEGRA_SID_PASSTHROUGH)
		return "Invalid SID";
	else
		return "Unassigned SID";
}

static void __mc_override_sid(int sid, int oid, enum mc_overrides ord)
{
	struct tegra_mc_sid_override *entry;
	volatile void __iomem *addr;
	u32 val;
	int offs = mc_sid->soc_data->sid_override_reg[oid].offs;

	BUG_ON(oid >= mc_sid->soc_data->max_oids);

	addr = TO_MC_SID_STREAMID_SECURITY_CONFIG(mc_sid->sid_base + offs);
	val = readl_relaxed(addr);

	if (!(val & SCEW_STREAMID_OVERRIDE)
		&& (val & SCEW_STREAMID_WRITE_ACCESS_DISABLED))
		return;

	addr = mc_sid->sid_base + offs;
	writel_relaxed(sid, addr);

	pr_debug("override sid=%d oid=%d ord=%d at offset=%x\n",
		 sid, oid, ord, offs);

	/* Check if this override exists in the list */
	list_for_each_entry(entry, &sid_override_list, list) {
		if (entry->addr != mc_sid->sid_base + offs)
			continue;
		/* Upon hit, Update existing list if mismatch */
		if (entry->sid != sid)
			entry->sid = sid;
		return;
	}

	/* Add this override to the list for resume() */
	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	entry->addr = mc_sid->sid_base + offs;
	entry->sid = sid;
	list_add_tail(&entry->list, &sid_override_list);
}

void platform_override_streamid(int sid, struct device *dev)
{
	int i;

	if (!mc_sid || !mc_sid->sid_base) {
		pr_err("mc-sid isn't populated\n");
		return;
	}

	for (i = 0; i < mc_sid->soc_data->nsid_to_oids; i++) {
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
		struct of_phandle_args args;
		unsigned int index = 0;
#endif
		struct sid_to_oids *conf;
		int j;

		conf = &mc_sid->soc_data->sid_to_oids[i];
		BUG_ON(conf->noids > MAX_OIDS_IN_SID);

		if (sid != conf->sid)
			continue;
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
		while (!of_parse_phandle_with_args(dev->of_node, "interconnects",
						   "#interconnect-cells", index, &args)) {
			if (args.args_count != 0) {
				if (conf->client_id == args.args[0]) {
					for (j = 0; j < conf->noids; j++)
						__mc_override_sid(sid, conf->oid[j], conf->ord);
				}
			}
			index++;
		}
#else
		for (j = 0; j < conf->noids; j++)
			__mc_override_sid(sid, conf->oid[j], conf->ord);
#endif
	}
}

#if defined(CONFIG_DEBUG_FS)

enum { ORD, SEC, MAX_REGS_TYPE};

static const char * const mc_regs_type[] = { "ord", "sec", };

static int mc_reg32_debugfs_set(void *data, u64 val)
{
	writel(val, data);
	return 0;
}

static int mc_reg32_debugfs_get(void *data, u64 *val)
{
	*val = readl(data);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(mc_reg32_debugfs_fops,
			mc_reg32_debugfs_get,
			mc_reg32_debugfs_set, "%08llx\n");

static void tegra_mc_sid_create_debugfs(void)
{
	int i, j;

	mc_sid->debugfs_root = debugfs_create_dir("tegra_mc_sid", NULL);
	if (!mc_sid->debugfs_root)
		return;

	for (i = 0; i < MAX_REGS_TYPE; i++) {
		void __iomem *base;
		struct dentry *dent;

		if (i == SEC)
			base = mc_sid->sid_base + sizeof(u32);
		else
			base = mc_sid->sid_base;

		dent = debugfs_create_dir(mc_regs_type[i],
						mc_sid->debugfs_root);
		if (!dent)
			continue;

		for (j = 0; j < mc_sid->soc_data->nsid_override_reg; j++) {
			void *addr;

			addr = base +
				mc_sid->soc_data->sid_override_reg[j].offs;
			debugfs_create_file(
				mc_sid->soc_data->sid_override_reg[j].name,
				S_IRUGO | S_IWUSR, dent, addr,
				&mc_reg32_debugfs_fops);
		}
	}
}

static void tegra_mc_sid_remove_debugfs(void)
{
	debugfs_remove_recursive(mc_sid->debugfs_root);
}
#else
static inline void tegra_mc_sid_create_debugfs(void)
{
}
static void tegra_mc_sid_remove_debugfs(void)
{
}
#endif	/* CONFIG_DEBUG_FS */

int tegra_mc_sid_probe(struct platform_device *pdev,
			const struct tegra_mc_sid_soc_data *soc_data)
{
	struct resource *res;
	static void __iomem *addr;

	mc_sid = devm_kzalloc(&pdev->dev, sizeof(*mc_sid), GFP_KERNEL);
	if (!mc_sid)
		return -ENOMEM;

	mc_sid->dev = &pdev->dev;
	mc_sid->soc_data = soc_data;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(addr))
		return PTR_ERR(addr);

	mc_sid->sid_base = addr;

	tegra_mc_sid_create_debugfs();

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_mc_sid_probe);

int tegra_mc_sid_remove(struct platform_device *pdev)
{
	struct tegra_mc_sid_override *entry, *next;

	tegra_mc_sid_remove_debugfs();
	list_for_each_entry_safe(entry, next, &sid_override_list, list) {
		list_del(&entry->list);
		kfree(entry);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_mc_sid_remove);

int tegra_mc_sid_resume_early(struct device *dev)
{
	struct tegra_mc_sid_override *entry;

	/* Restore all saved overrides */
	list_for_each_entry(entry, &sid_override_list, list)
		writel_relaxed(entry->sid, entry->addr);

	return 0;
}
EXPORT_SYMBOL_GPL(tegra_mc_sid_resume_early);

MODULE_DESCRIPTION("MC StreamID configuration");
MODULE_AUTHOR("Hiroshi DOYU <hdoyu@nvidia.com>, Pritesh Raithatha <praithatha@nvidia.com>");
MODULE_LICENSE("GPL v2");
