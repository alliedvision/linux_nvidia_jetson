/*
 * cdi_tsc.c - CDI TSC Signal Generation Driver.
 *
 * Copyright (c) 2021-2022 NVIDIA CORPORATION.  All rights reserved.
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

#include <asm/types.h>
#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/lcm.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm.h>

#define TSC_TICKS_PER_HZ			(31250000ULL)
#define TSC_NS_PER_TICK				(32)
#define NS_PER_MS				(1000000U)

#define TSC_MTSCCNTCV0				(0x10)
#define TSC_MTSCCNTCV0_CV			GENMASK(31, 0)

#define TSC_MTSCCNTCV1				(0x14)
#define TSC_MTSCCNTCV1_CV			GENMASK(31, 0)

#define TSC_GENX_CTRL				(0x00)
#define TSC_GENX_CTRL_RST			(0x00)
#define TSC_GENX_CTRL_INITIAL_VAL		BIT(1)
#define TSC_GENX_CTRL_ENABLE			BIT(0)

#define TSC_GENX_START0				(0x04)
#define TSC_GENX_START0_LSB_VAL			GENMASK(31, 0)

#define TSC_GENX_START1				(0x08)
#define TSC_GENX_START1_MSB_VAL			GENMASK(23, 0)

#define TSC_GENX_STATUS				(0x0C)
#define TSC_GENX_STATUS_INTERRUPT_STATUS	BIT(6)
#define TSC_GENX_STATUS_VALUE			BIT(5)
#define TSC_GENX_STATUS_EDGE_ID			GENMASK(4, 2)
#define TSC_GENX_STATUS_RUNNING			BIT(1)
#define TSC_GENX_STATUS_WAITING			BIT(0)

#define TSC_GENX_EDGE0				(0x18)
#define TSC_GENX_EDGE1				(0x1C)
#define TSC_GENX_EDGE2				(0x20)
#define TSC_GENX_EDGE3				(0x24)
#define TSC_GENX_EDGE4				(0x28)
#define TSC_GENX_EDGE5				(0x2C)
#define TSC_GENX_EDGE6				(0x30)
#define TSC_GENX_EDGE7				(0x34)

#define TSC_GENX_EDGEX_INTERRUPT_EN		BIT(31)
#define TSC_GENX_EDGEX_STOP			BIT(30)
#define TSC_GENX_EDGEX_TOGGLE			BIT(29)
#define TSC_GENX_EDGEX_LOOP			BIT(28)
#define TSC_GENX_EDGEX_OFFSET			GENMASK(27, 0)

/* Time (ms) offset for the TSC signal generators */
#define TSC_GENX_START_OFFSET_MS		(100)

/**
 * struct tsc_signal_controller_features: TSC signal controller SW feature support.
 * @rational_locking:
 *   @enforced: Generator periods must be derived from a common base.
 *   @max_freq_hz_lcm: Maximum frequency (hz) of the common base generator period.
 * @offset:
 *   @enabled: Allow generators to offset their signal from the start of their period.
 */
struct tsc_signal_controller_features {
	struct {
		bool enforced;
		u32 max_freq_hz_lcm;
	} rational_locking;
	struct {
		bool enabled;
	} offset;
};

/**
 * struct tsc_signal_generator : Generator context.
 * @base: ioremapped register base.
 * @of: Generator device node.
 * @config:
 *   @freq_hz: Frequency (hz) of the generator.
 *   @duty_cycle: Duty cycle (%) of the generator.
 *   @offset_ms: Offset (ms) to shift the signal by.
 * @debugfs:
 *   @regset_ro: Debug FS read-only register set.
 * @list: List node
 */
struct tsc_signal_generator {
	void __iomem *base;
	struct device_node *of;
	struct {
		u32 freq_hz;
		u32 duty_cycle;
		u32 offset_ms;
	} config;
	struct {
		struct debugfs_regset32 regset_ro;
	} debugfs;
	struct list_head list;
};

/**
 * struct tsc_signal_controller : Controller context
 * @dev: device.
 * @base: ioremapped register base.
 * @debugfs:
 *   @d: dentry to debugfs directory.
 * @features: Feature support for the controller.
 * @generators: Linked list of child generators.
 */
struct tsc_signal_controller {
	struct device *dev;
	void __iomem *base;
	struct {
		struct dentry *d;
	} debugfs;
	const struct tsc_signal_controller_features *features;
	struct list_head generators;
};

static const struct tsc_signal_controller_features tegra234_tsc_features = {
	.rational_locking = {
		.enforced = true,
		.max_freq_hz_lcm = 120,
	},
	.offset = {
		.enabled = true,
	},
};

static const struct debugfs_reg32 tsc_signal_generator_debugfs_regset[] = {
	{
		.name = "status",
		.offset = TSC_GENX_STATUS,
	},
};
#define TSC_SIG_GEN_DEBUGFS_REGSET_SIZE ARRAY_SIZE(tsc_signal_generator_debugfs_regset)

static inline void cdi_tsc_generator_writel(struct tsc_signal_generator *generator, u32 reg, u32 val)
{
	writel(val, generator->base + reg);
}

static inline u32 cdi_tsc_generator_readl(struct tsc_signal_generator *generator, u32 reg)
{
	return readl(generator->base + reg);
}

static inline u32 cdi_tsc_controller_readl(struct tsc_signal_controller *controller, u32 reg)
{
	return readl(controller->base + reg);
}

static u32 cdi_tsc_find_max_freq_hz_lcm(const struct tsc_signal_controller *controller)
{
	struct tsc_signal_generator *generator;
	u32 running_lcm = 0;

	list_for_each_entry(generator, &controller->generators, list) {
		running_lcm = lcm_not_zero(generator->config.freq_hz, running_lcm);
	}

	return running_lcm;
}

static int cdi_tsc_find_and_add_generators(struct tsc_signal_controller *controller)
{
	struct tsc_signal_generator *generator;
	struct device_node *np;
	struct resource res;
	const char *node_status;
	int err;

	for_each_child_of_node(controller->dev->of_node, np) {
		err = of_property_read_string(np, "status", &node_status);
		if (err != 0) {
			dev_err(controller->dev, "Failed to read generator status: %d\n", err);
			return err;
		}
		if (strcmp("okay", node_status) != 0) {
			dev_dbg(controller->dev, "Generator %s disabled - skipping\n", np->full_name);
			continue;
		}

		dev_dbg(controller->dev, "Generator found: %s\n", np->full_name);

		generator = devm_kzalloc(controller->dev, sizeof(*generator), GFP_KERNEL);
		if (!generator)
			return -ENOMEM;

		generator->of = np;
		INIT_LIST_HEAD(&generator->list);

		if (of_address_to_resource(np, 0, &res))
			return -EINVAL;

		generator->base = devm_ioremap_resource(controller->dev, &res);
		if (IS_ERR(generator->base))
			return PTR_ERR(generator->base);

		err = of_property_read_u32(np, "freq_hz", &generator->config.freq_hz);
		if (err != 0) {
			dev_err(controller->dev, "Failed to read generator frequency: %d\n", err);
			return err;
		}

		if (generator->config.freq_hz == 0) {
			dev_err(controller->dev, "Frequency must be non-zero\n");
			return -EINVAL;
		}

		err = of_property_read_u32(np, "duty_cycle", &generator->config.duty_cycle);
		if (err != 0) {
			dev_err(controller->dev, "Failed to read generator duty cycle: %d\n", err);
			return err;
		}
		if (generator->config.duty_cycle >= 100) {
			dev_err(controller->dev, "Duty cycle must be < 100%%\n");
			return -EINVAL;
		}

		if (controller->features->offset.enabled) {
			err = of_property_read_u32(np, "offset_ms", &generator->config.offset_ms);
			if (err != 0) {
				dev_err(controller->dev, "Failed to read generator offset: %d\n", err);
				return err;
			}
		}

		list_add_tail(&generator->list, &controller->generators);
		dev_dbg(controller->dev, "Generator %s added to controller\n", np->full_name);
	}

	return 0;
}

static int cdi_tsc_program_generator_edges(struct tsc_signal_controller *controller)
{
	struct tsc_signal_generator *generator;
	u32 max_freq_hz_lcm = 0;

	/*
	 * If rational locking is enforced (e.g. a 30Hz & 60Hz signal must align every two periods
	 * w.r.t. the 60Hz signal) edges will be derived from whole-number multiples of the LCM of
	 * all generator frequencies belonging to this controller.
	 *
	 * If rational locking is _not_ enforced then generator edges will be independently
	 * derived based on their configured frequency.
	 */
	if (controller->features->rational_locking.enforced) {
		max_freq_hz_lcm = cdi_tsc_find_max_freq_hz_lcm(controller);
		if (max_freq_hz_lcm > controller->features->rational_locking.max_freq_hz_lcm) {
			dev_err(controller->dev,
				"Highest common frequency of %u hz exceeds maximum allowed (%u hz)\n",
				max_freq_hz_lcm,
				controller->features->rational_locking.max_freq_hz_lcm);
			return -EINVAL;
		}
	}

	list_for_each_entry(generator, &controller->generators, list) {
		u32 ticks_in_period = 0;
		u32 ticks_active = 0;
		u32 ticks_inactive = 0;

		if (controller->features->rational_locking.enforced) {
			ticks_in_period = DIV_ROUND_CLOSEST(TSC_TICKS_PER_HZ, max_freq_hz_lcm);
			ticks_in_period *= max_freq_hz_lcm / generator->config.freq_hz;
		} else {
			ticks_in_period = DIV_ROUND_CLOSEST(TSC_TICKS_PER_HZ, generator->config.freq_hz);
		}

		ticks_active = mult_frac(ticks_in_period, generator->config.duty_cycle, 100);
		ticks_inactive = ticks_in_period - ticks_active;

		cdi_tsc_generator_writel(generator, TSC_GENX_EDGE0,
			TSC_GENX_EDGEX_TOGGLE |
			FIELD_PREP(TSC_GENX_EDGEX_OFFSET, ticks_active));

		cdi_tsc_generator_writel(generator, TSC_GENX_EDGE1,
			TSC_GENX_EDGEX_TOGGLE |
			TSC_GENX_EDGEX_LOOP |
			FIELD_PREP(TSC_GENX_EDGEX_OFFSET, ticks_inactive));
	}

	return 0;
}

static void cdi_tsc_program_generator_start_values(struct tsc_signal_controller *controller)
{
	const u32 relative_ticks_to_start = mult_frac(
		TSC_GENX_START_OFFSET_MS, NS_PER_MS, TSC_NS_PER_TICK);

	const u32 current_ticks_lo = FIELD_GET(TSC_MTSCCNTCV0_CV,
		cdi_tsc_controller_readl(controller, TSC_MTSCCNTCV0));
	const u32 current_ticks_hi = FIELD_GET(TSC_MTSCCNTCV1_CV,
		cdi_tsc_controller_readl(controller, TSC_MTSCCNTCV1));

	const u64 current_ticks = ((u64)current_ticks_hi << 32) | current_ticks_lo;

	struct tsc_signal_generator *generator;

	list_for_each_entry(generator, &controller->generators, list) {
		u64 absolute_ticks_to_start = current_ticks + relative_ticks_to_start;

		if (controller->features->offset.enabled && (generator->config.offset_ms != 0)) {
			absolute_ticks_to_start += mult_frac(generator->config.offset_ms, NS_PER_MS, TSC_NS_PER_TICK);
		}

		cdi_tsc_generator_writel(generator, TSC_GENX_START0,
			FIELD_PREP(TSC_GENX_START0_LSB_VAL, lower_32_bits(absolute_ticks_to_start)));

		cdi_tsc_generator_writel(generator, TSC_GENX_START1,
			FIELD_PREP(TSC_GENX_START1_MSB_VAL, upper_32_bits(absolute_ticks_to_start)));
	}
}

static bool cdi_tsc_generator_is_running(struct tsc_signal_generator *generator)
{
	const u32 status = cdi_tsc_generator_readl(generator, TSC_GENX_STATUS);

	return FIELD_GET(TSC_GENX_STATUS_RUNNING, status) == 1;
}

static bool cdi_tsc_generator_is_waiting(struct tsc_signal_generator *generator)
{
	const u32 status = cdi_tsc_generator_readl(generator, TSC_GENX_STATUS);

	return FIELD_GET(TSC_GENX_STATUS_WAITING, status) == 1;
}

static inline bool cdi_tsc_generator_is_idle(struct tsc_signal_generator *generator)
{
	return !cdi_tsc_generator_is_running(generator) &&
		!cdi_tsc_generator_is_waiting(generator);
}

static int cdi_tsc_start_generators(struct tsc_signal_controller *controller)
{
	struct tsc_signal_generator *generator;
	int err;

	/* A generator must be idle (e.g. neither running nor waiting) before starting */
	list_for_each_entry(generator, &controller->generators, list) {
		if (!cdi_tsc_generator_is_idle(generator)) {
			dev_err(controller->dev, "Generator %s is not idle\n", generator->of->full_name);
			return -EBUSY;
		}
	}

	err = cdi_tsc_program_generator_edges(controller);
	if (err != 0)
		return err;

	cdi_tsc_program_generator_start_values(controller);

	/* Start the generators */
	list_for_each_entry(generator, &controller->generators, list) {
		cdi_tsc_generator_writel(generator, TSC_GENX_CTRL,
			TSC_GENX_CTRL_INITIAL_VAL | TSC_GENX_CTRL_ENABLE);
	}

	return 0;
}

static int cdi_tsc_stop_generators(struct tsc_signal_controller *controller)
{
	struct tsc_signal_generator *generator;

	list_for_each_entry(generator, &controller->generators, list) {
		cdi_tsc_generator_writel(generator, TSC_GENX_CTRL, TSC_GENX_CTRL_RST);

		/* Ensure the generator has stopped */
		if (!cdi_tsc_generator_is_idle(generator)) {
			dev_err(controller->dev, "Generator %s failed to stop\n",
				generator->of->full_name);
			return -EIO;
		}
	}

	return 0;
}

#ifdef CONFIG_DEBUG_FS
static int cdi_tsc_debugfs_init(struct tsc_signal_controller *controller)
{
	struct tsc_signal_generator *generator;

	controller->debugfs.d =
		debugfs_create_dir(controller->dev->of_node->full_name, NULL);
	if (IS_ERR(controller->debugfs.d))
		return PTR_ERR(controller->debugfs.d);

	list_for_each_entry(generator, &controller->generators, list) {
		generator->debugfs.regset_ro.regs = tsc_signal_generator_debugfs_regset;
		generator->debugfs.regset_ro.nregs = TSC_SIG_GEN_DEBUGFS_REGSET_SIZE;
		generator->debugfs.regset_ro.base = generator->base;

		debugfs_create_regset32(
				generator->of->full_name,
				0400,
				controller->debugfs.d,
				&generator->debugfs.regset_ro);
	}

	return 0;
}

static void cdi_tsc_debugfs_remove(struct tsc_signal_controller *controller)
{
	debugfs_remove_recursive(controller->debugfs.d);
	controller->debugfs.d = NULL;
}
#endif

static int cdi_tsc_probe(struct platform_device *pdev)
{
	struct tsc_signal_controller *controller;
	struct resource *res;
	int err;

	dev_info(&pdev->dev, "CDI TSC probing...\n");

	controller = devm_kzalloc(&pdev->dev, sizeof(*controller), GFP_KERNEL);
	if (!controller)
		return -ENOMEM;

	controller->dev = &pdev->dev;
	controller->features = of_device_get_match_data(&pdev->dev);
	if (controller->features == NULL) {
		dev_err(controller->dev, "No controller feature table found\n");
		return -ENODEV;
	}

	INIT_LIST_HEAD(&controller->generators);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	controller->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(controller->base))
		return PTR_ERR(controller->base);

	platform_set_drvdata(pdev, controller);

	err = cdi_tsc_find_and_add_generators(controller);
	if (err != 0)
		return err;

#ifdef CONFIG_DEBUG_FS
	err = cdi_tsc_debugfs_init(controller);
	if (err != 0)
		return err;
#endif

	return cdi_tsc_start_generators(controller);
}

static int cdi_tsc_remove(struct platform_device *pdev)
{
	struct tsc_signal_controller *controller = platform_get_drvdata(pdev);

#ifdef CONFIG_DEBUG_FS
	cdi_tsc_debugfs_remove(controller);
#endif
	return cdi_tsc_stop_generators(controller);
}

static int __maybe_unused cdi_tsc_suspend(struct device *dev)
{
	struct tsc_signal_controller *controller = dev_get_drvdata(dev);

	return cdi_tsc_stop_generators(controller);
}

static int __maybe_unused cdi_tsc_resume(struct device *dev)
{
	struct tsc_signal_controller *controller = dev_get_drvdata(dev);

	return cdi_tsc_start_generators(controller);
}

static const struct of_device_id cdi_tsc_of_match[] = {
	{ .compatible = "nvidia,tegra234-cdi-tsc", .data = &tegra234_tsc_features },
	{ },
};
MODULE_DEVICE_TABLE(of, cdi_tsc_of_match);

static SIMPLE_DEV_PM_OPS(cdi_tsc_pm, cdi_tsc_suspend, cdi_tsc_resume);

static struct platform_driver cdi_tsc_driver = {
	.driver = {
		.name = "cdi_tsc",
		.owner = THIS_MODULE,
		.of_match_table = cdi_tsc_of_match,
		.pm = &cdi_tsc_pm,
	},
	.probe = cdi_tsc_probe,
	.remove = cdi_tsc_remove,
};
module_platform_driver(cdi_tsc_driver);

MODULE_AUTHOR("Ian Kaszubski <ikaszubski@nvidia.com>");
MODULE_DESCRIPTION("CDI TSC Signal Generation Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:cdi_tsc");

