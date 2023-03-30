/*
 * Copyright (c) 2016-2019, NVIDIA CORPORATION.  All rights reserved.
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
#include <linux/compiler.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/slab.h>
#include <linux/wait.h>

#include <linux/tegra-hsp.h>
#include <dt-bindings/soc/nvidia,tegra186-hsp.h>

#define NV(p) "nvidia," #p

struct tegra_hsp {
	void __iomem *base;
	struct reset_control *reset;
	spinlock_t lock;
	resource_size_t start;
	u8 n_sm;
	u8 n_as;
	u8 n_ss;
	u8 n_db;
	u8 n_si;
	bool mbox_ie;
};

struct tegra_hsp_sm {
	struct device dev;
	void __iomem *reg;
	int irq;
	u8 si_index;
	u8 ie_shift;
	u8 index;
	u8 per_sm_ie;
};

struct tegra_hsp_sm_rx {
	tegra_hsp_sm_notify full_notify;
	struct tegra_hsp_sm sm;
};

struct tegra_hsp_sm_tx {
	tegra_hsp_sm_notify empty_notify;
	struct tegra_hsp_sm sm;
};

struct tegra_hsp_ss {
	struct device dev;
	void __iomem *reg;
};

#define TEGRA_HSP_IR			(0x304)
#define TEGRA_HSP_IE(si)		(0x100 + (4 * (si)))
#define TEGRA_HSP_IE_SM_EMPTY(sm)	(0x1u << (sm))
#define TEGRA_HSP_IE_SM_FULL(sm)	(0x100u << (sm))
#define TEGRA_HSP_IE_SM_FULL_SHIFT	0x8u
#define TEGRA_HSP_IE_DB(db)		(0x10000u << (db))
#define TEGRA_HSP_IE_AS(as)		(0x1000000u << (as))
#define TEGRA_HSP_DIMENSIONING		0x380
#define TEGRA_HSP_SM(sm)		(0x10000 + (0x8000 * (sm)))
#define TEGRA_HSP_SS(n_sm, ss) \
	(0x10000 + (0x8000 * (n_sm) + (0x10000 * (ss))))
#define TEGRA_HSP_SM_FULL		0x80000000u

#define TEGRA_HSP_SM_IE_FULL		0x4u
#define TEGRA_HSP_SM_IE_EMPTY		0x8u

static void __iomem *tegra_hsp_reg(struct device *dev, u32 offset)
{
	struct tegra_hsp *hsp = dev_get_drvdata(dev);

	return hsp->base + offset;
}

static void __iomem *tegra_hsp_ie_reg(const struct tegra_hsp_sm *sm)
{
	return tegra_hsp_reg(sm->dev.parent, TEGRA_HSP_IE(sm->si_index));
}

static void __iomem *tegra_hsp_ir_reg(const struct tegra_hsp_sm *sm)
{
	return tegra_hsp_reg(sm->dev.parent, TEGRA_HSP_IR);
}

static inline bool tegra_hsp_irq_is_shared(const struct tegra_hsp_sm *sm)
{
	return (sm->irq >= 0) && (sm->si_index != 0xff);
}

static inline bool tegra_hsp_sm_is_empty(const struct tegra_hsp_sm *sm)
{
	return (sm->ie_shift < TEGRA_HSP_IE_SM_FULL_SHIFT);
}

static void tegra_hsp_irq_suspend(struct tegra_hsp_sm *sm)
{
	unsigned long flags;

	if (tegra_hsp_irq_is_shared(sm)) {
		struct tegra_hsp *hsp = dev_get_drvdata(sm->dev.parent);
		void __iomem *reg = tegra_hsp_ie_reg(sm);

		spin_lock_irqsave(&hsp->lock, flags);
		writel(readl(reg) & ~(1u << sm->ie_shift), reg);
		spin_unlock_irqrestore(&hsp->lock, flags);
	}
}

static void tegra_hsp_irq_resume(struct tegra_hsp_sm *sm)
{
	unsigned long flags;

	if (tegra_hsp_irq_is_shared(sm)) {
		struct tegra_hsp *hsp = dev_get_drvdata(sm->dev.parent);
		void __iomem *reg = tegra_hsp_ie_reg(sm);

		spin_lock_irqsave(&hsp->lock, flags);
		writel(readl(reg) | (1u << sm->ie_shift), reg);
		spin_unlock_irqrestore(&hsp->lock, flags);
	}
}

static void tegra_hsp_enable_per_sm_irq(struct tegra_hsp_sm *sm,
					int irq)
{
	if (sm->per_sm_ie != 0)
		writel(1, sm->reg + sm->per_sm_ie);
	else if (tegra_hsp_irq_is_shared(sm))
		tegra_hsp_irq_resume(sm);
	else if (!(irq < 0))
		enable_irq(irq); /* APE HSP uses internal interrupts */
}

static void tegra_hsp_disable_per_sm_irq(struct tegra_hsp_sm *sm)
{
	if (sm->per_sm_ie != 0)
		writel(0, sm->reg + sm->per_sm_ie);
	else if (tegra_hsp_irq_is_shared(sm))
		tegra_hsp_irq_suspend(sm);
	else
		disable_irq_nosync(sm->irq);
}

static inline bool tegra_hsp_irq_is_set(struct tegra_hsp_sm *sm)
{
	if (sm->per_sm_ie != 0)
		return (readl(sm->reg + sm->per_sm_ie) & 1) != 0;
	else if (tegra_hsp_irq_is_shared(sm))
		return (readl(tegra_hsp_ir_reg(sm)) &
			readl(tegra_hsp_ie_reg(sm)) &
			(1u << sm->ie_shift)) != 0;
	else
		return true;
}

static irqreturn_t tegra_hsp_full_isr(int irq, void *data)
{
	struct tegra_hsp_sm *sm = data;
	struct tegra_hsp_sm_rx *sm_rx;

	u32 value = readl(sm->reg);

	if (!(value & TEGRA_HSP_SM_FULL))
		return IRQ_NONE;

	if (!tegra_hsp_irq_is_set(sm))
		return IRQ_NONE;

	/* Empty the mailbox and clear the interrupt */
	writel(0, sm->reg);

	value &= ~TEGRA_HSP_SM_FULL;

	sm_rx = container_of(sm, struct tegra_hsp_sm_rx, sm);
	sm_rx->full_notify(dev_get_drvdata(&sm->dev), value);

	return IRQ_HANDLED;
}

static irqreturn_t tegra_hsp_empty_isr(int irq, void *data)
{
	struct tegra_hsp_sm *sm = data;
	struct tegra_hsp_sm_tx *sm_tx;

	u32 value = readl(sm->reg);

	if ((value & TEGRA_HSP_SM_FULL))
		return IRQ_NONE;

	if (!tegra_hsp_irq_is_set(sm))
		return IRQ_NONE;

	tegra_hsp_disable_per_sm_irq(sm);

	sm_tx = container_of(sm, struct tegra_hsp_sm_tx, sm);
	sm_tx->empty_notify(dev_get_drvdata(&sm->dev), value);

	return IRQ_HANDLED;
}

static int tegra_hsp_get_shared_irq(struct tegra_hsp_sm *sm,
		irq_handler_t handler)
{
	struct platform_device *pdev = to_platform_device(sm->dev.parent);
	struct tegra_hsp *hsp = dev_get_drvdata(sm->dev.parent);
	int ret = -ENODEV;
	unsigned long flags = IRQF_ONESHOT | IRQF_SHARED | IRQF_PROBE_SHARED;
	unsigned i;

	for (i = 0; i < hsp->n_si; i++) {
		char irqname[8];

		sprintf(irqname, "shared%X", i);
		sm->irq = platform_get_irq_byname(pdev, irqname);
		if (sm->irq < 0)
			continue;

		sm->si_index = i;

		ret = request_threaded_irq(sm->irq, NULL, handler, flags,
						dev_name(&sm->dev), sm);
		if (ret)
			continue;

		dev_dbg(&sm->dev, "using shared IRQ %u (%d)\n", i, sm->irq);

		tegra_hsp_enable_per_sm_irq(sm, -EPERM);

		/* Update interrupt masks (for shared interrupts only) */
		tegra_hsp_irq_resume(sm);

		return 0;
	}

	if (ret != -EPROBE_DEFER)
		dev_err(&sm->dev, "cannot get shared IRQ: %d\n", ret);
	return ret;
}

static int tegra_hsp_get_sm_irq(struct tegra_hsp_sm *sm,
		irq_handler_t handler)
{
	struct platform_device *pdev = to_platform_device(sm->dev.parent);
	unsigned long flags = IRQF_ONESHOT | IRQF_SHARED;
	char name[7];

	/* Look for dedicated internal IRQ */
	sprintf(name,
		tegra_hsp_sm_is_empty(sm) ? "empty%X" : "full%X",
		sm->index);
	sm->irq = platform_get_irq_byname(pdev, name);
	if (!(sm->irq < 0)) {
		sm->si_index = 0xff;

		if (request_threaded_irq(sm->irq, NULL, handler, flags,
					dev_name(&sm->dev), sm) == 0) {
			tegra_hsp_enable_per_sm_irq(sm, -EPERM);
			return 0;
		}
	}

	/* Look for a free shared IRQ */
	return tegra_hsp_get_shared_irq(sm, handler);
}

static void tegra_hsp_irq_free(struct tegra_hsp_sm *sm)
{
	if (sm->irq < 0)
		return;
	tegra_hsp_irq_suspend(sm);
	free_irq(sm->irq, sm);
}

static int tegra_hsp_sm_suspend(struct device *dev)
{
	struct tegra_hsp_sm *sm = container_of(dev, struct tegra_hsp_sm, dev);

	tegra_hsp_irq_suspend(sm);

	return 0;
}

static int tegra_hsp_sm_resume(struct device *dev)
{
	struct tegra_hsp_sm *sm = container_of(dev, struct tegra_hsp_sm, dev);

	tegra_hsp_irq_resume(sm);

	return 0;
}

static const struct dev_pm_ops tegra_hsp_sm_pm_ops = {
	.suspend_noirq	= tegra_hsp_sm_suspend,
	.resume_noirq	= tegra_hsp_sm_resume,
};

static const struct device_type tegra_hsp_sm_dev_type = {
	.name	= "tegra-hsp-shared-mailbox",
	.pm	= &tegra_hsp_sm_pm_ops,
};

static int tegra_hsp_sm_register(struct device *dev,
		struct tegra_hsp_sm *sm,
		u32 index, u8 ie_shift, u8 per_sm_ie,
		irq_handler_t handler,
		void (*release)(struct device *dev),
		void *data)
{
	struct tegra_hsp *hsp = dev_get_drvdata(dev);
	resource_size_t start;
	int err;

	sm->irq = -1;
	sm->reg = tegra_hsp_reg(dev, TEGRA_HSP_SM(index));
	sm->si_index = 0xff;
	sm->index = index;
	sm->ie_shift = ie_shift;
	sm->per_sm_ie = per_sm_ie;

	sm->dev.parent = dev;
	sm->dev.type = &tegra_hsp_sm_dev_type;
	sm->dev.release = release;
	start = hsp->start + TEGRA_HSP_SM(index);
	dev_set_name(&sm->dev, "%llx.%s:%u.%s",
		(u64)start, "tegra-hsp-sm", index,
		tegra_hsp_sm_is_empty(sm) ? "tx" : "rx");

	dev_set_drvdata(&sm->dev, data);

	err = device_register(&sm->dev);
	if (err) {
		/* This calls sm->dev.release */
		put_device(&sm->dev);
		return err;
	}

	if (handler != NULL) {
		err = tegra_hsp_get_sm_irq(sm, handler);
		if (err)
			device_unregister(&sm->dev);
	}

	return err;
}

static void tegra_hsp_sm_free(struct tegra_hsp_sm *sm)
{
	/*
	 * Make sure that the structure is no longer referenced.
	 * This also implies that callbacks are no longer pending.
	 */
	tegra_hsp_irq_free(sm);
	device_unregister(&sm->dev);
}

static void tegra_hsp_sm_rx_dev_release(struct device *dev)
{
	struct tegra_hsp_sm_rx *sm_rx = container_of(dev,
			struct tegra_hsp_sm_rx, sm.dev);

	kfree(sm_rx);
}

static struct tegra_hsp_sm_rx *tegra_hsp_sm_rx_create(
	struct device *dev, u32 index,
	tegra_hsp_sm_notify full_notify, void *data)
{
	struct tegra_hsp *hsp = dev_get_drvdata(dev);
	struct tegra_hsp_sm_rx *sm_rx;
	irq_handler_t handler = NULL;
	int err;

	if (hsp == NULL)
		return ERR_PTR(-EPROBE_DEFER);
	if (index >= hsp->n_sm)
		return ERR_PTR(-ENODEV);

	sm_rx = kzalloc(sizeof(*sm_rx), GFP_KERNEL);
	if (unlikely(sm_rx == NULL))
		return ERR_PTR(-ENOMEM);

	if (full_notify) {
		sm_rx->full_notify = full_notify;
		handler = tegra_hsp_full_isr;
	}

	err = tegra_hsp_sm_register(dev, &sm_rx->sm,
			index, index + TEGRA_HSP_IE_SM_FULL_SHIFT,
			hsp->mbox_ie ? TEGRA_HSP_SM_IE_FULL : 0,
			handler, tegra_hsp_sm_rx_dev_release, data);
	if (err)
		return ERR_PTR(err);

	return sm_rx;
}

struct tegra_hsp_sm_rx *of_tegra_hsp_sm_rx_by_name(
	struct device_node *np, char const *name,
	tegra_hsp_sm_notify full_notify,
	void *data)
{
	struct platform_device *pdev;
	int err, index, number;
	struct of_phandle_args smspec;
	struct tegra_hsp_sm_rx *sm_rx;

	index = of_property_match_string(np, "mbox-names", name);
	err = of_parse_phandle_with_args(np, "mboxes", "#mbox-cells",
			index, &smspec);

	if (index < 0) {
		index = of_property_match_string(np,
				"nvidia,hsp-mailbox-names", name);
		err = of_parse_phandle_with_fixed_args(np,
				"nvidia,hsp-mailboxes", 1, index, &smspec);
		smspec.args[1] = TEGRA_HSP_SM_RX(smspec.args[0]);
		smspec.args[0] = TEGRA_HSP_MBOX_TYPE_SM;
		smspec.args_count = 2;
	}

	if (index < 0) {
		index = of_property_match_string(np,
				"nvidia,hsp-shared-mailbox-names", name);
		err = of_parse_phandle_with_fixed_args(np,
				"nvidia,hsp-shared-mailbox", 1,
				index, &smspec);
		smspec.args[1] = TEGRA_HSP_SM_RX(smspec.args[0]);
		smspec.args[0] = TEGRA_HSP_MBOX_TYPE_SM;
		smspec.args_count = 2;
	}

	if (err)
		return ERR_PTR(err);
	if (smspec.args_count < 2)
		return ERR_PTR(-ENODEV);
	if (smspec.args[0] != TEGRA_HSP_MBOX_TYPE_SM)
		return ERR_PTR(-ENODEV);
	if ((smspec.args[1] & ~TEGRA_HSP_SM_MASK) != TEGRA_HSP_SM_FLAG_RX)
		return ERR_PTR(-ENODEV);

	number = smspec.args[1] & TEGRA_HSP_SM_MASK;

	pdev = of_find_device_by_node(smspec.np);
	of_node_put(smspec.np);

	if (pdev == NULL)
		return ERR_PTR(-EPROBE_DEFER);

	sm_rx = tegra_hsp_sm_rx_create(&pdev->dev, number, full_notify, data);
	platform_device_put(pdev);
	return sm_rx;
}
EXPORT_SYMBOL(of_tegra_hsp_sm_rx_by_name);

/**
 * tegra_hsp_sm_rx_free - free a Tegra HSP mailbox
 */
void tegra_hsp_sm_rx_free(struct tegra_hsp_sm_rx *rx)
{
	if (!IS_ERR_OR_NULL(rx))
		tegra_hsp_sm_free(&rx->sm);
}
EXPORT_SYMBOL(tegra_hsp_sm_rx_free);

/**
 * tegra_hsp_sm_rx_is_empty - test if mailbox has been emptied
 *
 * @rx: receive mailbox
 *
 * Return: true if mailbox is empty, false otherwise.
 */
bool tegra_hsp_sm_rx_is_empty(const struct tegra_hsp_sm_rx *rx)
{
	if ((readl(rx->sm.reg) & TEGRA_HSP_SM_FULL) == 0)
		return true;

	/* Ensure any pending full ISR invocation has emptied the mailbox */
	synchronize_irq(rx->sm.irq);

	return (readl(rx->sm.reg) & TEGRA_HSP_SM_FULL) == 0;
}
EXPORT_SYMBOL(tegra_hsp_sm_rx_is_empty);

static void tegra_hsp_sm_tx_dev_release(struct device *dev)
{
	struct tegra_hsp_sm_tx *sm_tx = container_of(dev,
			struct tegra_hsp_sm_tx, sm.dev);

	kfree(sm_tx);
}

static struct tegra_hsp_sm_tx *tegra_hsp_sm_tx_create(
	struct device *dev, u32 index,
	tegra_hsp_sm_notify empty_notify,
	void *data)
{
	struct tegra_hsp *hsp = dev_get_drvdata(dev);
	struct tegra_hsp_sm_tx *sm_tx;
	irq_handler_t handler = NULL;
	int err;

	if (hsp == NULL)
		return ERR_PTR(-EPROBE_DEFER);
	if (index >= hsp->n_sm)
		return ERR_PTR(-ENODEV);

	sm_tx = kzalloc(sizeof(*sm_tx), GFP_KERNEL);
	if (unlikely(sm_tx == NULL))
		return ERR_PTR(-ENOMEM);

	if (empty_notify) {
		sm_tx->empty_notify = empty_notify;
		handler = tegra_hsp_empty_isr;
	}

	err = tegra_hsp_sm_register(dev, &sm_tx->sm,
			index, index,
			hsp->mbox_ie ? TEGRA_HSP_SM_IE_EMPTY : 0,
			handler, tegra_hsp_sm_tx_dev_release, data);
	if (err)
		return ERR_PTR(err);

	return sm_tx;
}

struct tegra_hsp_sm_tx *of_tegra_hsp_sm_tx_by_name(
	struct device_node *np, char const *name,
	tegra_hsp_sm_notify notify,
	void *data)
{
	struct platform_device *pdev;
	int err, index, number;
	struct of_phandle_args smspec;
	struct tegra_hsp_sm_tx *sm_tx;

	index = of_property_match_string(np, "mbox-names", name);
	err = of_parse_phandle_with_args(np, "mboxes", "#mbox-cells",
			index, &smspec);

	if (index < 0) {
		index = of_property_match_string(np,
				"nvidia,hsp-mailbox-names", name);
		err = of_parse_phandle_with_fixed_args(np,
				"nvidia,hsp-mailboxes", 1, index, &smspec);
		smspec.args[1] = TEGRA_HSP_SM_TX(smspec.args[0]);
		smspec.args[0] = TEGRA_HSP_MBOX_TYPE_SM;
		smspec.args_count = 2;
	}

	if (index < 0) {
		index = of_property_match_string(np,
			"nvidia,hsp-shared-mailbox-names", name);
		err = of_parse_phandle_with_fixed_args(np,
			"nvidia,hsp-shared-mailbox", 1, index, &smspec);
		/* pair of the numbered shared-mailbox */
		smspec.args[1] = TEGRA_HSP_SM_TX(smspec.args[0] ^ 1);
		smspec.args[0] = TEGRA_HSP_MBOX_TYPE_SM;
		smspec.args_count = 2;
	}

	if (err)
		return ERR_PTR(err);

	if (smspec.args_count < 2)
		return ERR_PTR(-ENODEV);
	if (smspec.args[0] != TEGRA_HSP_MBOX_TYPE_SM)
		return ERR_PTR(-ENODEV);
	if ((smspec.args[1] & ~TEGRA_HSP_SM_MASK) != TEGRA_HSP_SM_FLAG_TX)
		return ERR_PTR(-ENODEV);

	number = smspec.args[1] & TEGRA_HSP_SM_MASK;

	pdev = of_find_device_by_node(smspec.np);
	of_node_put(smspec.np);

	if (pdev == NULL)
		return ERR_PTR(-EPROBE_DEFER);

	sm_tx = tegra_hsp_sm_tx_create(&pdev->dev, number, notify, data);
	platform_device_put(pdev);
	return sm_tx;
}
EXPORT_SYMBOL(of_tegra_hsp_sm_tx_by_name);

/**
 * tegra_hsp_sm_tx_free - free a Tegra HSP mailbox
 */
void tegra_hsp_sm_tx_free(struct tegra_hsp_sm_tx *tx)
{
	if (!IS_ERR_OR_NULL(tx))
		tegra_hsp_sm_free(&tx->sm);
}
EXPORT_SYMBOL(tegra_hsp_sm_tx_free);

/**
 * tegra_hsp_sm_tx_is_empty - test if mailbox has been emptied
 *
 * @tx: transmit mailbox
 *
 * Return: true if mailbox is empty, false otherwise.
 */
bool tegra_hsp_sm_tx_is_empty(const struct tegra_hsp_sm_tx *tx)
{
	return (readl(tx->sm.reg) & TEGRA_HSP_SM_FULL) == 0;
}
EXPORT_SYMBOL(tegra_hsp_sm_tx_is_empty);

/**
 * tegra_hsp_sm_tx_write - fill a Tegra HSP shared mailbox
 *
 * @tx: shared mailbox
 * @value: value to fill mailbox with (only 31-bits low order bits are used)
 *
 * This writes a value to the transmit side mailbox.
 */
void tegra_hsp_sm_tx_write(const struct tegra_hsp_sm_tx *tx, u32 value)
{
	writel(TEGRA_HSP_SM_FULL | value, tx->sm.reg);
}
EXPORT_SYMBOL(tegra_hsp_sm_tx_write);

/**
 * tegra_hsp_sm_tx_enable_notify - enable empty notification from mailbox
 *
 * @tx: shared mailbox
 *
 * This enables one-shot empty notify from the transmit side mailbox.
 */
void tegra_hsp_sm_tx_enable_notify(struct tegra_hsp_sm_tx *tx)
{
	if (tx->empty_notify != NULL)
		tegra_hsp_enable_per_sm_irq(&tx->sm, tx->sm.irq);
}
EXPORT_SYMBOL(tegra_hsp_sm_tx_enable_notify);

/*
 * Shared semaphore devices
 */
static const struct device_type tegra_hsp_ss_dev_type = {
	.name	= "tegra-hsp-shared-semaphore",
};

static void tegra_hsp_ss_dev_release(struct device *dev)
{
	struct tegra_hsp_ss *ss = container_of(dev, struct tegra_hsp_ss, dev);

	kfree(ss);
}

static struct tegra_hsp_ss *tegra_hsp_ss_get(
	struct device *dev, u32 index)
{
	struct tegra_hsp *hsp = dev_get_drvdata(dev);
	struct tegra_hsp_ss *ss;
	resource_size_t start;
	int err;

	if (hsp == NULL)
		return ERR_PTR(-EPROBE_DEFER);
	if (index >= hsp->n_ss)
		return ERR_PTR(-ENODEV);

	ss = kzalloc(sizeof(*ss), GFP_KERNEL);
	if (unlikely(ss == NULL))
		return ERR_PTR(-ENOMEM);

	ss->reg = tegra_hsp_reg(dev, TEGRA_HSP_SS(hsp->n_sm, index));
	ss->dev.parent = dev;
	ss->dev.type = &tegra_hsp_ss_dev_type;
	ss->dev.release = tegra_hsp_ss_dev_release;
	start = hsp->start + TEGRA_HSP_SS(hsp->n_sm, index);
	dev_set_name(&ss->dev, "%llx.%s:%u", (u64)start, "tegra-hsp-ss", index);

	err = device_register(&ss->dev);
	if (err) {
		/* This calls ss->dev.release */
		put_device(&ss->dev);
		return ERR_PTR(err);
	}

	return ss;
}

/**
 * of_tegra_hsp_ss_by_name - request a Tegra HSP shared semaphore from DT.
 *
 * @np: device node
 * @name: shared semaphore entry name
 *
 * Looks up a shared semaphore in device tree by name. The device tree
 * node needs the properties nvidia,hsp-shared-semaphores and
 * nvidia-hsp-shared-semaphore-names.
 */
struct tegra_hsp_ss *of_tegra_hsp_ss_by_name(
	struct device_node *np, char const *name)
{
	struct platform_device *pdev;
	struct tegra_hsp_ss *ss;
	struct of_phandle_args smspec;
	int err;
	int index;

	index = of_property_match_string(np, "mbox-names", name);

	if (index < 0) {
		index = of_property_match_string(np,
				"nvidia,hsp-shared-semaphore-names", name);
		if (index < 0)
			return ERR_PTR(index);
		err = of_parse_phandle_with_fixed_args(np,
				"nvidia,hsp-shared-semaphores", 1,
				index, &smspec);
		if (err)
			return ERR_PTR(err);
		index = smspec.args[0];
	} else {
		err = of_parse_phandle_with_args(np, "mboxes", "#mbox-cells",
				index, &smspec);
		if (err < 0)
			return ERR_PTR(err);
		if (smspec.args_count < 2)
			return ERR_PTR(-ENODEV);
		if (smspec.args[0] != TEGRA_HSP_MBOX_TYPE_SS)
			return ERR_PTR(-ENODEV);
		index = smspec.args[1];
	}

	pdev = of_find_device_by_node(smspec.np);
	of_node_put(smspec.np);

	if (pdev == NULL)
		return ERR_PTR(-EPROBE_DEFER);

	ss = tegra_hsp_ss_get(&pdev->dev, index);
	platform_device_put(pdev);
	return ss;
}
EXPORT_SYMBOL(of_tegra_hsp_ss_by_name);

/**
 * tegra_hsp_ss_free - free a Tegra HSP shared semaphore
 */
void tegra_hsp_ss_free(struct tegra_hsp_ss *ss)
{
	if (!IS_ERR_OR_NULL(ss))
		device_unregister(&ss->dev);
}
EXPORT_SYMBOL(tegra_hsp_ss_free);

/**
 * tegra_hsp_ss_status - Read status of a Tegra HSP shared semaphore
 *
 * @ss: shared semaphore
 *
 * Returns the current status of shared semaphore.
 *
 * NOTE: The shared semaphore should not rely on value 0xDEAD1001
 * being set, any read of shared semaphore with status 0xDEAD1001
 * results in value 0 being read instead. See http://nvbugs/200395605
 * for more details.
 */
u32 tegra_hsp_ss_status(const struct tegra_hsp_ss *ss)
{
	return readl(ss->reg);
}
EXPORT_SYMBOL(tegra_hsp_ss_status);

/**
 * tegra_hsp_ss_set - Set bits on a Tegra HSP shared semaphore
 *
 * @ss: shared semaphore
 * @bits: bits to set
 *
 * The 1 bits in `bits` are set on semaphore status.
 *
 * NOTE: The shared semaphore should not rely on value 0xDEAD1001
 * being set, any read of shared semaphore with value 0xDEAD1001
 * results in value 0 being read instead. See http://nvbugs/200395605
 * for more details.
 */
void tegra_hsp_ss_set(const struct tegra_hsp_ss *ss, u32 bits)
{
	writel(bits, ss->reg + 4u);
}
EXPORT_SYMBOL(tegra_hsp_ss_set);

/**
 * tegra_hsp_ss_clr - Clear bits on a Tegra HSP shared semaphore
 *
 * @ss: shared semaphore
 * @bits: bits to clr
 *
 * The 1 bits in `bits` are cleared on semaphore status.
 *
 * NOTE: The shared semaphore should not rely on value 0xDEAD1001
 * being set, any read of shared semaphore with value 0xDEAD1001
 * results in value 0 being read instead. See http://nvbugs/200395605
 * for more details.
 */
void tegra_hsp_ss_clr(const struct tegra_hsp_ss *ss, u32 bits)
{
	writel(bits, ss->reg + 8u);
}
EXPORT_SYMBOL(tegra_hsp_ss_clr);

/*
 * Shared mailbox pairs
 */
static struct tegra_hsp_sm_pair *tegra_hsp_sm_pair_request(
	struct device *dev, u32 index,
	tegra_hsp_sm_notify full_notify,
	tegra_hsp_sm_notify empty_notify,
	void *data)
{
	struct tegra_hsp_sm_pair *pair;
	void *err_ptr;

	pair = kzalloc(sizeof(*pair), GFP_KERNEL);
	if (unlikely(pair == NULL))
		return ERR_PTR(-ENOMEM);

	pair->rx = tegra_hsp_sm_rx_create(dev, index, full_notify, data);
	if (IS_ERR(pair->rx)) {
		err_ptr = pair->rx;
		kfree(pair);
		return err_ptr;
	}

	pair->tx = tegra_hsp_sm_tx_create(dev, index ^ 1, empty_notify, data);
	if (IS_ERR(pair->tx)) {
		err_ptr = pair->tx;
		tegra_hsp_sm_free(&pair->rx->sm);
		kfree(pair);
		return err_ptr;
	}

	return pair;
}

/**
 * of_tegra_sm_pair_request - request a Tegra HSP shared mailbox pair from DT.
 *
 * @np: device node
 * @index: mailbox pair entry offset in the DT property
 *
 * Looks up a shared mailbox pair in device tree by index. The device node
 * needs a nvidia,hsp-shared-mailbox property, containing pairs of
 * OF phandle and mailbox number. The OF phandle points to the Tegra HSP
 * platform device. The mailbox number refers to the consumer side mailbox.
 * The producer side mailbox is the other one in the same (even-odd) pair.
 */
struct tegra_hsp_sm_pair *of_tegra_hsp_sm_pair_request(
	const struct device_node *np, u32 index,
	tegra_hsp_sm_notify full, tegra_hsp_sm_notify empty, void *data)
{
	struct platform_device *pdev;
	struct tegra_hsp_sm_pair *pair;
	struct of_phandle_args smspec;
	int err;

	err = of_parse_phandle_with_fixed_args(np, NV(hsp-shared-mailbox), 1,
						index, &smspec);
	if (err)
		return ERR_PTR(err);

	pdev = of_find_device_by_node(smspec.np);
	index = smspec.args[0];
	of_node_put(smspec.np);

	if (pdev == NULL)
		return ERR_PTR(-EPROBE_DEFER);

	pair = tegra_hsp_sm_pair_request(&pdev->dev, index, full, empty, data);
	platform_device_put(pdev);
	return pair;
}
EXPORT_SYMBOL(of_tegra_hsp_sm_pair_request);

/**
 * of_tegra_sm_pair_by_name - request a Tegra HSP shared mailbox pair from DT.
 *
 * @np: device node
 * @name: mailbox pair entry name
 *
 * Looks up a shared mailbox pair in device tree by name. The device node needs
 * nvidia,hsp-shared-mailbox and nvidia-hsp-shared-mailbox-names properties.
 */
struct tegra_hsp_sm_pair *of_tegra_hsp_sm_pair_by_name(
	struct device_node *np, char const *name,
	tegra_hsp_sm_notify full, tegra_hsp_sm_notify empty, void *data)
{
	/* If match fails, index will be -1 and parse_phandles fails */
	int index = of_property_match_string(np,
			NV(hsp-shared-mailbox-names), name);

	return of_tegra_hsp_sm_pair_request(np, index, full, empty, data);
}
EXPORT_SYMBOL(of_tegra_hsp_sm_pair_by_name);

/**
 * tegra_hsp_sm_pair_free - free a Tegra HSP shared mailbox pair.
 */
void tegra_hsp_sm_pair_free(struct tegra_hsp_sm_pair *pair)
{
	if (IS_ERR_OR_NULL(pair))
		return;

	tegra_hsp_sm_free(&pair->rx->sm);
	tegra_hsp_sm_free(&pair->tx->sm);

	kfree(pair);
}
EXPORT_SYMBOL(tegra_hsp_sm_pair_free);

/**
 * tegra_hsp_sm_pair_write - fill a Tegra HSP shared mailbox
 *
 * @pair: shared mailbox pair
 * @value: value to fill mailbox with (only 31-bits low order bits are used)
 *
 * This writes a value to the producer side mailbox of a mailbox pair.
 */
void tegra_hsp_sm_pair_write(const struct tegra_hsp_sm_pair *pair,
				u32 value)
{
	tegra_hsp_sm_tx_write(pair->tx, value);
}
EXPORT_SYMBOL(tegra_hsp_sm_pair_write);

/**
 * tegra_hsp_sm_pair_is_empty - test is mailbox pair is empty
 *
 * @pair: shared mailbox pair
 *
 * Return: true if both mailboxes are empty, false otherwise.
 */
bool tegra_hsp_sm_pair_is_empty(const struct tegra_hsp_sm_pair *pair)
{
	return tegra_hsp_sm_rx_is_empty(pair->rx) &&
		tegra_hsp_sm_tx_is_empty(pair->tx);
}
EXPORT_SYMBOL(tegra_hsp_sm_pair_is_empty);

/**
 * tegra_hsp_sm_pair_enable_empty_notify - enable mailbox empty notification
 *
 * @pair: mailbox pair
 *
 * This enables one-shot empty notify from the transmit side mailbox.
 */
void tegra_hsp_sm_pair_enable_empty_notify(struct tegra_hsp_sm_pair *pair)
{
	return tegra_hsp_sm_tx_enable_notify(pair->tx);
}
EXPORT_SYMBOL(tegra_hsp_sm_pair_enable_empty_notify);

static int tegra_hsp_suspend(struct device *dev)
{
	struct tegra_hsp *hsp = dev_get_drvdata(dev);
	int ret = 0;

	if (!IS_ERR(hsp->reset))
		ret = reset_control_assert(hsp->reset);

	return ret;
}

static int tegra_hsp_resume(struct device *dev)
{
	struct tegra_hsp *hsp = dev_get_drvdata(dev);
	int ret = 0;

	if (!IS_ERR(hsp->reset))
		ret = reset_control_deassert(hsp->reset);

	return ret;
}

static const struct dev_pm_ops tegra_hsp_pm_ops = {
	.suspend_noirq	= tegra_hsp_suspend,
	.resume_noirq	= tegra_hsp_resume,
};

static const struct of_device_id tegra_hsp_of_match[] = {
	{ .compatible = NV(tegra186-hsp), },
	{ },
};

static int tegra_hsp_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct resource *r;
	struct tegra_hsp *hsp;
	u32 reg;

	if (np == NULL)
		return -ENXIO;

	hsp = devm_kzalloc(&pdev->dev, sizeof(*hsp), GFP_KERNEL);
	if (unlikely(hsp == NULL))
		return -ENOMEM;

	platform_set_drvdata(pdev, hsp);

	spin_lock_init(&hsp->lock);

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (r == NULL)
		return -EINVAL;

	if (resource_size(r) < 0x10000) {
		dev_err(&pdev->dev, "memory range too short\n");
		return -EINVAL;
	}

	hsp->base = devm_ioremap(&pdev->dev, r->start, resource_size(r));
	if (hsp->base == NULL)
		return -ENOMEM;

	/* devm_reset_control_get() fails indistinctly with -EPROBE_DEFER */
	hsp->reset = of_reset_control_get(pdev->dev.of_node, "hsp");
	if (hsp->reset == ERR_PTR(-EPROBE_DEFER))
		return -EPROBE_DEFER;

	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);

	hsp->start = r->start;

	reg = readl(tegra_hsp_reg(&pdev->dev, TEGRA_HSP_DIMENSIONING));
	hsp->n_sm = reg & 0xf;
	hsp->n_ss = (reg >> 4) & 0xf;
	hsp->n_as = (reg >> 8) & 0xf;
	hsp->n_db = (reg >> 12) & 0xf;
	hsp->n_si = (reg >> 16) & 0xf;
	hsp->mbox_ie = of_property_read_bool(pdev->dev.of_node, NV(mbox-ie));

	pm_runtime_put(&pdev->dev);

	if ((resource_size(r) >> 16) < (1 + (hsp->n_sm / 2) + hsp->n_ss +
					hsp->n_as + (hsp->n_db > 0))) {
		dev_err(&pdev->dev, "memory range too short\n");
		return -EINVAL;
	}

	return 0;
}

static __exit int tegra_hsp_remove(struct platform_device *pdev)
{
	struct tegra_hsp *hsp = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
	reset_control_put(hsp->reset);
	return 0;
}

static struct platform_driver tegra_hsp_driver = {
	.probe	= tegra_hsp_probe,
	.remove	= __exit_p(tegra_hsp_remove),
	.driver = {
		.name	= "tegra186-hsp",
		.owner	= THIS_MODULE,
		.suppress_bind_attrs = true,
		.of_match_table = of_match_ptr(tegra_hsp_of_match),
		.pm	= &tegra_hsp_pm_ops,
	},
};

static int __init tegra18_hsp_init(void)
{
	int ret;

	ret = platform_driver_register(&tegra_hsp_driver);
	if (ret)
		return ret;

	return 0;
}
subsys_initcall(tegra18_hsp_init);

static void __exit tegra18_hsp_exit(void)
{
}
module_exit(tegra18_hsp_exit);
MODULE_AUTHOR("Remi Denis-Courmont <remid@nvidia.com>");
MODULE_DESCRIPTION("NVIDIA Tegra 186 HSP driver");
MODULE_LICENSE("GPL");
