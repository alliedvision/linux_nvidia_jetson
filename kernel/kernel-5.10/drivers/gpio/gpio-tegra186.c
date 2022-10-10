// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2022 NVIDIA Corporation
 *
 * Author: Thierry Reding <treding@nvidia.com>
 */

#include <linux/gpio/driver.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#include <dt-bindings/gpio/tegra186-gpio.h>
#include <dt-bindings/gpio/tegra194-gpio.h>
#include <dt-bindings/gpio/tegra234-gpio.h>
#include <dt-bindings/gpio/tegra239-gpio.h>

/* security registers */
#define TEGRA186_GPIO_CTL_SCR 0x0c
#define  TEGRA186_GPIO_CTL_SCR_SEC_WEN BIT(28)
#define  TEGRA186_GPIO_CTL_SCR_SEC_REN BIT(27)

#define TEGRA186_GPIO_INT_ROUTE_MAPPING(p, x) (0x14 + (p) * 0x20 + (x) * 4)

#define GPIO_VM_REG				0x00
#define GPIO_VM_RW				0x03
#define GPIO_SCR_REG				0x04
#define GPIO_SCR_DIFF				0x08
#define GPIO_SCR_BASE_DIFF			0x40
#define GPIO_SCR_SEC_WEN			BIT(28)
#define GPIO_SCR_SEC_REN			BIT(27)
#define GPIO_SCR_SEC_G1W			BIT(9)
#define GPIO_SCR_SEC_G1R			BIT(1)
#define GPIO_FULL_ACCESS			(GPIO_SCR_SEC_WEN | \
						 GPIO_SCR_SEC_REN | \
						 GPIO_SCR_SEC_G1R | \
						 GPIO_SCR_SEC_G1W)
#define GPIO_SCR_SEC_ENABLE			(GPIO_SCR_SEC_WEN | \
						 GPIO_SCR_SEC_REN)

/* control registers */
#define TEGRA186_GPIO_ENABLE_CONFIG 0x00
#define  TEGRA186_GPIO_ENABLE_CONFIG_ENABLE BIT(0)
#define  TEGRA186_GPIO_ENABLE_CONFIG_OUT BIT(1)
#define  TEGRA186_GPIO_ENABLE_CONFIG_TRIGGER_TYPE_NONE (0x0 << 2)
#define  TEGRA186_GPIO_ENABLE_CONFIG_TRIGGER_TYPE_LEVEL (0x1 << 2)
#define  TEGRA186_GPIO_ENABLE_CONFIG_TRIGGER_TYPE_SINGLE_EDGE (0x2 << 2)
#define  TEGRA186_GPIO_ENABLE_CONFIG_TRIGGER_TYPE_DOUBLE_EDGE (0x3 << 2)
#define  TEGRA186_GPIO_ENABLE_CONFIG_TRIGGER_TYPE_MASK (0x3 << 2)
#define  TEGRA186_GPIO_ENABLE_CONFIG_TRIGGER_LEVEL BIT(4)
#define  TEGRA186_GPIO_ENABLE_CONFIG_DEBOUNCE BIT(5)
#define  TEGRA186_GPIO_ENABLE_CONFIG_INTERRUPT BIT(6)
#define  TEGRA186_GPIO_ENABLE_CONFIG_TIMESTAMP_FUNC BIT(7)

#define TEGRA186_GPIO_DEBOUNCE_CONTROL 0x04
#define  TEGRA186_GPIO_DEBOUNCE_CONTROL_THRESHOLD(x) ((x) & 0xff)

#define TEGRA186_GPIO_INPUT 0x08
#define  TEGRA186_GPIO_INPUT_HIGH BIT(0)

#define TEGRA186_GPIO_OUTPUT_CONTROL 0x0c
#define  TEGRA186_GPIO_OUTPUT_CONTROL_FLOATED BIT(0)

#define TEGRA186_GPIO_OUTPUT_VALUE 0x10
#define  TEGRA186_GPIO_OUTPUT_VALUE_HIGH BIT(0)

#define TEGRA186_GPIO_INTERRUPT_CLEAR 0x14

#define TEGRA186_GPIO_INTERRUPT_STATUS(x) (0x100 + (x) * 4)

/******************** GTE Registers ******************************/

#define GTE_GPIO_TECTRL				0x0
#define GTE_GPIO_TETSCH				0x4
#define GTE_GPIO_TETSCL				0x8
#define GTE_GPIO_TESRC				0xC
#define GTE_GPIO_TECCV				0x10
#define GTE_GPIO_TEPCV				0x14
#define GTE_GPIO_TEENCV				0x18
#define GTE_GPIO_TECMD				0x1C
#define GTE_GPIO_TESTATUS			0x20
#define GTE_GPIO_SLICE0_TETEN			0x40
#define GTE_GPIO_SLICE0_TETDIS			0x44
#define GTE_GPIO_SLICE1_TETEN			0x60
#define GTE_GPIO_SLICE1_TETDIS			0x64
#define GTE_GPIO_SLICE2_TETEN			0x80
#define GTE_GPIO_SLICE2_TETDIS			0x84

#define GTE_GPIO_TECTRL_ENABLE_SHIFT		0
#define GTE_GPIO_TECTRL_ENABLE_MASK		0x1
#define GTE_GPIO_TECTRL_ENABLE_DISABLE		0x0
#define GTE_GPIO_TECTRL_ENABLE_ENABLE		0x1

#define GTE_GPIO_TESRC_SLICE_SHIFT		16
#define GTE_GPIO_TESRC_SLICE_DEFAULT_MASK	0xFF

#define GTE_GPIO_TECMD_CMD_POP			0x1

#define GTE_GPIO_TESTATUS_OCCUPANCY_SHIFT	8
#define GTE_GPIO_TESTATUS_OCCUPANCY_MASK	0xFF

#define AON_GPIO_SLICE1_MAP			0x3000
#define AON_GPIO_SLICE2_MAP			0xFFFFFFF
#define AON_GPIO_SLICE1_INDEX			1
#define AON_GPIO_SLICE2_INDEX			2
#define BASE_ADDRESS_GTE_GPIO_SLICE0		0x40
#define BASE_ADDRESS_GTE_GPIO_SLICE1		0x60
#define BASE_ADDRESS_GTE_GPIO_SLICE2		0x80

#define GTE_GPIO_SLICE_SIZE (BASE_ADDRESS_GTE_GPIO_SLICE1 - \
			     BASE_ADDRESS_GTE_GPIO_SLICE0)

/* AON GPIOS are mapped to only slice 1 and slice 2 */
/* GTE Interrupt connections. For slice 1 */
#define NV_AON_GTE_SLICE1_IRQ_LIC0   0
#define NV_AON_GTE_SLICE1_IRQ_LIC1   1
#define NV_AON_GTE_SLICE1_IRQ_LIC2   2
#define NV_AON_GTE_SLICE1_IRQ_LIC3   3
#define NV_AON_GTE_SLICE1_IRQ_APBERR 4
#define NV_AON_GTE_SLICE1_IRQ_GPIO   5
#define NV_AON_GTE_SLICE1_IRQ_WAKE0  6
#define NV_AON_GTE_SLICE1_IRQ_PMC    7
#define NV_AON_GTE_SLICE1_IRQ_DMIC   8
#define NV_AON_GTE_SLICE1_IRQ_PM     9
#define NV_AON_GTE_SLICE1_IRQ_FPUINT 10
#define NV_AON_GTE_SLICE1_IRQ_AOVC   11
#define NV_AON_GTE_SLICE1_IRQ_GPIO_28 12
#define NV_AON_GTE_SLICE1_IRQ_GPIO_29 13
#define NV_AON_GTE_SLICE1_IRQ_GPIO_30 14
#define NV_AON_GTE_SLICE1_IRQ_GPIO_31 15
#define NV_AON_GTE_SLICE1_IRQ_GPIO_32 16
#define NV_AON_GTE_SLICE1_IRQ_GPIO_33 17
#define NV_AON_GTE_SLICE1_IRQ_GPIO_34 18
#define NV_AON_GTE_SLICE1_IRQ_GPIO_35 19
#define NV_AON_GTE_SLICE1_IRQ_GPIO_36 20
#define NV_AON_GTE_SLICE1_IRQ_GPIO_37 21
#define NV_AON_GTE_SLICE1_IRQ_GPIO_38 22
#define NV_AON_GTE_SLICE1_IRQ_GPIO_39 23
#define NV_AON_GTE_SLICE1_IRQ_GPIO_40 24
#define NV_AON_GTE_SLICE1_IRQ_GPIO_41 25
#define NV_AON_GTE_SLICE1_IRQ_GPIO_42 26
#define NV_AON_GTE_SLICE1_IRQ_GPIO_43 27

/* GTE Interrupt connections. For slice 2 */
#define NV_AON_GTE_SLICE2_IRQ_GPIO_0 0
#define NV_AON_GTE_SLICE2_IRQ_GPIO_1 1
#define NV_AON_GTE_SLICE2_IRQ_GPIO_2 2
#define NV_AON_GTE_SLICE2_IRQ_GPIO_3 3
#define NV_AON_GTE_SLICE2_IRQ_GPIO_4 4
#define NV_AON_GTE_SLICE2_IRQ_GPIO_5 5
#define NV_AON_GTE_SLICE2_IRQ_GPIO_6 6
#define NV_AON_GTE_SLICE2_IRQ_GPIO_7 7
#define NV_AON_GTE_SLICE2_IRQ_GPIO_8 8
#define NV_AON_GTE_SLICE2_IRQ_GPIO_9 9
#define NV_AON_GTE_SLICE2_IRQ_GPIO_10 10
#define NV_AON_GTE_SLICE2_IRQ_GPIO_11 11
#define NV_AON_GTE_SLICE2_IRQ_GPIO_12 12
#define NV_AON_GTE_SLICE2_IRQ_GPIO_13 13
#define NV_AON_GTE_SLICE2_IRQ_GPIO_14 14
#define NV_AON_GTE_SLICE2_IRQ_GPIO_15 15
#define NV_AON_GTE_SLICE2_IRQ_GPIO_16 16
#define NV_AON_GTE_SLICE2_IRQ_GPIO_17 17
#define NV_AON_GTE_SLICE2_IRQ_GPIO_18 18
#define NV_AON_GTE_SLICE2_IRQ_GPIO_19 19
#define NV_AON_GTE_SLICE2_IRQ_GPIO_20 20
#define NV_AON_GTE_SLICE2_IRQ_GPIO_21 21
#define NV_AON_GTE_SLICE2_IRQ_GPIO_22 22
#define NV_AON_GTE_SLICE2_IRQ_GPIO_23 23
#define NV_AON_GTE_SLICE2_IRQ_GPIO_24 24
#define NV_AON_GTE_SLICE2_IRQ_GPIO_25 25
#define NV_AON_GTE_SLICE2_IRQ_GPIO_26 26
#define NV_AON_GTE_SLICE2_IRQ_GPIO_27 27


/**************************************************************/

struct tegra_gpio_port {
	const char *name;
	unsigned int bank;
	unsigned int port;
	unsigned int pins;
};

struct tegra186_pin_range {
	unsigned int offset;
	const char *group;
};

struct tegra_gpio_soc {
	const struct tegra_gpio_port *ports;
	unsigned int num_ports;
	const char *name;
	unsigned int instance;
	unsigned int num_irqs_per_bank;
	bool is_hw_ts_sup;
	bool do_vm_check;
	const struct tegra186_pin_range *pin_ranges;
	unsigned int num_pin_ranges;
	const char *pinmux;
	const struct tegra_gte_info *gte_info;
	int gte_npins;
};

struct tegra_gpio_saved_register {
	bool restore_needed;
	u32 val;
	u32 conf;
	u32 out;
};

struct tegra_gpio {
	struct gpio_chip gpio;
	struct irq_chip intc;
	unsigned int num_irq;
	unsigned int *irq;

	const struct tegra_gpio_soc *soc;
	unsigned int num_irqs_per_bank;
	unsigned int num_banks;
	unsigned int gte_enable;
	bool use_timestamp;

	void __iomem *secure;
	void __iomem *base;
	void __iomem *gte_regs;
	struct tegra_gpio_saved_register *gpio_rval;
};

/*************************** GTE related code ********************/

struct tegra_gte_info {
	uint32_t pin_num;
	uint32_t slice;
	uint32_t slice_bit;
};

/* Structure to maintain all information about the AON GPIOs
 * that can be supported
 */
static struct tegra_gte_info tegra194_gte_info[] = {
	/* pin_num, slice, slice_bit*/
	[0]  = {11, 2, NV_AON_GTE_SLICE2_IRQ_GPIO_0},
	[1]  = {10, 2, NV_AON_GTE_SLICE2_IRQ_GPIO_1},
	[2]  = {9,  2, NV_AON_GTE_SLICE2_IRQ_GPIO_2},
	[3]  = {8,  2, NV_AON_GTE_SLICE2_IRQ_GPIO_3},
	[4]  = {7,  2, NV_AON_GTE_SLICE2_IRQ_GPIO_4},
	[5]  = {6,  2, NV_AON_GTE_SLICE2_IRQ_GPIO_5},
	[6]  = {5,  2, NV_AON_GTE_SLICE2_IRQ_GPIO_6},
	[7]  = {4,  2, NV_AON_GTE_SLICE2_IRQ_GPIO_7},
	[8]  = {3,  2, NV_AON_GTE_SLICE2_IRQ_GPIO_8},
	[9]  = {2,  2, NV_AON_GTE_SLICE2_IRQ_GPIO_9},
	[10] = {1,  2, NV_AON_GTE_SLICE2_IRQ_GPIO_10},
	[11] = {0,  2, NV_AON_GTE_SLICE2_IRQ_GPIO_11},
	[12] = {26, 2, NV_AON_GTE_SLICE2_IRQ_GPIO_12},
	[13] = {25, 2, NV_AON_GTE_SLICE2_IRQ_GPIO_13},
	[14] = {24, 2, NV_AON_GTE_SLICE2_IRQ_GPIO_14},
	[15] = {23, 2, NV_AON_GTE_SLICE2_IRQ_GPIO_15},
	[16] = {22, 2, NV_AON_GTE_SLICE2_IRQ_GPIO_16},
	[17] = {21, 2, NV_AON_GTE_SLICE2_IRQ_GPIO_17},
	[18] = {20, 2, NV_AON_GTE_SLICE2_IRQ_GPIO_18},
	[19] = {19, 2, NV_AON_GTE_SLICE2_IRQ_GPIO_19},
	[20] = {18, 2, NV_AON_GTE_SLICE2_IRQ_GPIO_20},
	[21] = {17, 2, NV_AON_GTE_SLICE2_IRQ_GPIO_21},
	[22] = {16, 2, NV_AON_GTE_SLICE2_IRQ_GPIO_22},
	[23] = {38, 2, NV_AON_GTE_SLICE2_IRQ_GPIO_23},
	[24] = {37, 2, NV_AON_GTE_SLICE2_IRQ_GPIO_24},
	[25] = {36, 2, NV_AON_GTE_SLICE2_IRQ_GPIO_25},
	[26] = {35, 2, NV_AON_GTE_SLICE2_IRQ_GPIO_26},
	[27] = {34, 2, NV_AON_GTE_SLICE2_IRQ_GPIO_27},
	[28] = {33, 1, NV_AON_GTE_SLICE1_IRQ_GPIO_28},
	[29] = {32, 1, NV_AON_GTE_SLICE1_IRQ_GPIO_29},
};

static inline u32 tegra_gte_readl(struct tegra_gpio *tgi, u32 reg)
{
	return __raw_readl(tgi->gte_regs + reg);
}

static inline void tegra_gte_writel(struct tegra_gpio *tgi, u32 reg,
		u32 val)
{
	__raw_writel(val, tgi->gte_regs + reg);
}

static void tegra_gte_flush_fifo(struct tegra_gpio *tgi)
{
	/* Check if FIFO is empty */
	while ((tegra_gte_readl(tgi, GTE_GPIO_TESTATUS) >>
		GTE_GPIO_TESTATUS_OCCUPANCY_SHIFT) &
		GTE_GPIO_TESTATUS_OCCUPANCY_MASK) {
		/* Pop this entry, go to next */
		tegra_gte_writel(tgi, GTE_GPIO_TECMD, GTE_GPIO_TECMD_CMD_POP);
	}
}

u64 tegra_gte_read_fifo(struct tegra_gpio *tgi, u32 offset)
{
	u32 src_slice;
	u32 tsh, tsl;
	u64 ts = 0;
	u32 precv, curcv, xorcv;
	u32 aon_bits;
	u32 bit_index = 0;

	/* Check if FIFO is empty */
	while ((tegra_gte_readl(tgi, GTE_GPIO_TESTATUS) >>
		GTE_GPIO_TESTATUS_OCCUPANCY_SHIFT) &
		GTE_GPIO_TESTATUS_OCCUPANCY_MASK) {
		src_slice = (tegra_gte_readl(tgi, GTE_GPIO_TESRC) >>
				GTE_GPIO_TESRC_SLICE_SHIFT) &
			GTE_GPIO_TESRC_SLICE_DEFAULT_MASK;

		if (src_slice == AON_GPIO_SLICE1_INDEX ||
		    src_slice == AON_GPIO_SLICE2_INDEX) {
			precv = tegra_gte_readl(tgi, GTE_GPIO_TEPCV);
			curcv = tegra_gte_readl(tgi, GTE_GPIO_TECCV);

			/* Save TSC high and low 32 bits value */
			tsh = tegra_gte_readl(tgi, GTE_GPIO_TETSCH);
			tsl = tegra_gte_readl(tgi, GTE_GPIO_TETSCL);

			/* TSC countre as 64 bits */
			ts  = (((uint64_t)tsh << 32) | tsl);

			xorcv = precv ^ curcv;
			if (src_slice == AON_GPIO_SLICE1_INDEX)
				aon_bits = xorcv & AON_GPIO_SLICE1_MAP;
			else
				aon_bits = xorcv & AON_GPIO_SLICE2_MAP;

			bit_index = ffs(aon_bits) - 1;
		}
		/* Pop this entry, go to next */
		tegra_gte_writel(tgi, GTE_GPIO_TECMD, GTE_GPIO_TECMD_CMD_POP);
		tegra_gte_readl(tgi, GTE_GPIO_TESRC);
	}

	return (tgi->soc->gte_info[bit_index].pin_num == offset) ? ts : 0;
}

int tegra_gte_enable_ts(struct tegra_gpio *tgi, u32 offset)
{
	u32 val, mask, reg;
	int i = 0;

	if (tgi->gte_enable == 1) {
		dev_err(tgi->gpio.parent, "timestamp is already enabled for gpio\n");
		return -EINVAL;
	}

	/* Configure Timestamping AON GPIO to SLICEx mapping */
	for (i = 0; i < tgi->soc->gte_npins; i++) {
		if (tgi->soc->gte_info[i].pin_num == offset) {
			reg = (tgi->soc->gte_info[i].slice *
			       GTE_GPIO_SLICE_SIZE) + GTE_GPIO_SLICE0_TETEN;
			val = (1 << tgi->soc->gte_info[i].slice_bit);
			tegra_gte_writel(tgi, reg, val);
			break;
		}
	}

	val = tegra_gte_readl(tgi, GTE_GPIO_TECTRL);
	mask = (GTE_GPIO_TECTRL_ENABLE_MASK << GTE_GPIO_TECTRL_ENABLE_SHIFT);
	val &= ~mask;
	val |= (GTE_GPIO_TECTRL_ENABLE_ENABLE << GTE_GPIO_TECTRL_ENABLE_SHIFT);
	tegra_gte_writel(tgi, GTE_GPIO_TECTRL, val);

	tegra_gte_flush_fifo(tgi);

	tgi->gte_enable = 1;

	return 0;
}

int tegra_gte_disable_ts(struct tegra_gpio *tgi, u32 offset)
{
	u32 val, mask;

	if (tgi->gte_enable == 0) {
		dev_err(tgi->gpio.parent, "timestamp is already disabled\n");
		return 0;
	}

	val = tegra_gte_readl(tgi, GTE_GPIO_TECTRL);
	mask = (GTE_GPIO_TECTRL_ENABLE_MASK << GTE_GPIO_TECTRL_ENABLE_SHIFT);
	val &= ~mask;
	val |= (GTE_GPIO_TECTRL_ENABLE_DISABLE << GTE_GPIO_TECTRL_ENABLE_SHIFT);
	tegra_gte_writel(tgi, GTE_GPIO_TECTRL, val);

	/* Disable Slice mapping as well */
	tegra_gte_writel(tgi, (AON_GPIO_SLICE1_INDEX * GTE_GPIO_SLICE_SIZE) +
			GTE_GPIO_SLICE0_TETEN, 0);
	tegra_gte_writel(tgi, (AON_GPIO_SLICE2_INDEX * GTE_GPIO_SLICE_SIZE) +
			GTE_GPIO_SLICE0_TETEN, 0);

	tgi->gte_enable = 0;

	return 0;
}

int tegra_gte_setup(struct tegra_gpio *tgi)
{
	tegra_gte_writel(tgi, GTE_GPIO_TECTRL, 0);
	tgi->gte_enable = 0;

	return 0;
}

/*****************************************************************/

static const struct tegra_gpio_port *
tegra186_gpio_get_port(struct tegra_gpio *gpio, unsigned int *pin)
{
	unsigned int start = 0, i;

	for (i = 0; i < gpio->soc->num_ports; i++) {
		const struct tegra_gpio_port *port = &gpio->soc->ports[i];

		if (*pin >= start && *pin < start + port->pins) {
			*pin -= start;
			return port;
		}

		start += port->pins;
	}

	return NULL;
}

static void __iomem *tegra186_gpio_get_base(struct tegra_gpio *gpio,
					    unsigned int pin)
{
	const struct tegra_gpio_port *port;
	unsigned int offset;

	port = tegra186_gpio_get_port(gpio, &pin);
	if (!port)
		return NULL;

	offset = port->bank * 0x1000 + port->port * 0x200;

	return gpio->base + offset + pin * 0x20;
}

static void __iomem *tegra186_gpio_get_secure(struct tegra_gpio *gpio,
					    unsigned int pin)
{
	const struct tegra_gpio_port *port;
	unsigned int offset;

	port = tegra186_gpio_get_port(gpio, &pin);
	if (!port)
		return NULL;
	offset = port->bank * 0x1000 + port->port * GPIO_SCR_BASE_DIFF;

	return gpio->secure + offset + pin * GPIO_SCR_DIFF;
}

static inline bool gpio_is_accessible(struct tegra_gpio *gpio, u32 pin)
{
	void __iomem *secure;
	u32 val;

	secure = tegra186_gpio_get_secure(gpio, pin);
	if (gpio->soc->do_vm_check) {
		val = __raw_readl(secure + GPIO_VM_REG);
		if ((val & GPIO_VM_RW) != GPIO_VM_RW)
			return false;
	}

	val = __raw_readl(secure + GPIO_SCR_REG);

	if ((val & (GPIO_SCR_SEC_ENABLE)) == 0)
		return true;

	if ((val & (GPIO_FULL_ACCESS)) == GPIO_FULL_ACCESS)
		return true;

	return false;
}

static int tegra186_gpio_get_direction(struct gpio_chip *chip,
				       unsigned int offset)
{
	struct tegra_gpio *gpio = gpiochip_get_data(chip);
	void __iomem *base;
	u32 value;

	if (!gpio_is_accessible(gpio, offset))
		return -EPERM;

	base = tegra186_gpio_get_base(gpio, offset);
	if (WARN_ON(base == NULL))
		return -ENODEV;

	value = readl(base + TEGRA186_GPIO_ENABLE_CONFIG);
	if (value & TEGRA186_GPIO_ENABLE_CONFIG_OUT)
		return GPIO_LINE_DIRECTION_OUT;

	return GPIO_LINE_DIRECTION_IN;
}

static int tegra186_gpio_direction_input(struct gpio_chip *chip,
					 unsigned int offset)
{
	struct tegra_gpio *gpio = gpiochip_get_data(chip);
	void __iomem *base;
	u32 value;
	int ret = 0;

	if (!gpio_is_accessible(gpio, offset))
		return -EPERM;

	base = tegra186_gpio_get_base(gpio, offset);
	if (WARN_ON(base == NULL))
		return -ENODEV;

	value = readl(base + TEGRA186_GPIO_OUTPUT_CONTROL);
	value |= TEGRA186_GPIO_OUTPUT_CONTROL_FLOATED;
	writel(value, base + TEGRA186_GPIO_OUTPUT_CONTROL);

	value = readl(base + TEGRA186_GPIO_ENABLE_CONFIG);
	value |= TEGRA186_GPIO_ENABLE_CONFIG_ENABLE;
	value &= ~TEGRA186_GPIO_ENABLE_CONFIG_OUT;
	writel(value, base + TEGRA186_GPIO_ENABLE_CONFIG);

	ret = pinctrl_gpio_direction_input(chip->base + offset);
	if (ret < 0)
		dev_err(chip->parent,
			"Failed to set input direction: %d\n", ret);
	return ret;
}

static int tegra186_gpio_direction_output(struct gpio_chip *chip,
					  unsigned int offset, int level)
{
	struct tegra_gpio *gpio = gpiochip_get_data(chip);
	void __iomem *base;
	u32 value;
	int ret = 0;

	if (!gpio_is_accessible(gpio, offset))
		return -EPERM;

	/* configure output level first */
	chip->set(chip, offset, level);

	base = tegra186_gpio_get_base(gpio, offset);
	if (WARN_ON(base == NULL))
		return -EINVAL;

	/* set the direction */
	value = readl(base + TEGRA186_GPIO_OUTPUT_CONTROL);
	value &= ~TEGRA186_GPIO_OUTPUT_CONTROL_FLOATED;
	writel(value, base + TEGRA186_GPIO_OUTPUT_CONTROL);

	value = readl(base + TEGRA186_GPIO_ENABLE_CONFIG);
	value |= TEGRA186_GPIO_ENABLE_CONFIG_ENABLE;
	value |= TEGRA186_GPIO_ENABLE_CONFIG_OUT;
	writel(value, base + TEGRA186_GPIO_ENABLE_CONFIG);
	ret = pinctrl_gpio_direction_output(chip->base + offset);

	if (ret < 0)
		dev_err(chip->parent,
			"Failed to set output direction: %d\n", ret);
	return ret;
}

static int tegra_gpio_suspend_configure(struct gpio_chip *chip, unsigned offset,
					enum gpiod_flags dflags)
{
	struct tegra_gpio *gpio = gpiochip_get_data(chip);
	struct tegra_gpio_saved_register *regs;
	void __iomem *base;

	if (!gpio_is_accessible(gpio, offset))
		return -EPERM;

	base = tegra186_gpio_get_base(gpio, offset);
	if (WARN_ON(base == NULL))
		return -EINVAL;

	regs = &gpio->gpio_rval[offset];
	regs->conf = readl(base + TEGRA186_GPIO_ENABLE_CONFIG),
	regs->out = readl(base + TEGRA186_GPIO_OUTPUT_CONTROL),
	regs->val = readl(base + TEGRA186_GPIO_OUTPUT_VALUE),
	regs->restore_needed = true;

	if (dflags & GPIOD_FLAGS_BIT_DIR_OUT)
		return tegra186_gpio_direction_output(chip, offset,
					dflags & GPIOD_FLAGS_BIT_DIR_VAL);

	return tegra186_gpio_direction_input(chip, offset);
}

static int tegra_gpio_timestamp_control(struct gpio_chip *chip, unsigned offset,
					int enable)
{
	struct tegra_gpio *gpio = gpiochip_get_data(chip);
	void __iomem *base;
	int value;
	int ret;

	base = tegra186_gpio_get_base(gpio, offset);
	if (WARN_ON(base == NULL))
		return -EINVAL;

	if (gpio->use_timestamp) {
		value = readl(base + TEGRA186_GPIO_ENABLE_CONFIG);
		value |= TEGRA186_GPIO_ENABLE_CONFIG_TIMESTAMP_FUNC;
		writel(value, base + TEGRA186_GPIO_ENABLE_CONFIG);
		if (enable)
			ret = tegra_gte_enable_ts(gpio, offset);
		else
			ret = tegra_gte_disable_ts(gpio, offset);
	} else
		ret = -EOPNOTSUPP;

	return ret;
}

static int tegra_gpio_timestamp_read(struct gpio_chip *chip, unsigned offset,
				     u64 *ts)
{
	struct tegra_gpio *tgi = gpiochip_get_data(chip);
	int ret;

	if (tgi->use_timestamp) {
		*ts = tegra_gte_read_fifo(tgi, offset);
		ret = 0;
	} else
		ret = -EOPNOTSUPP;

	return ret;
}

static int tegra186_gpio_get(struct gpio_chip *chip, unsigned int offset)
{
	struct tegra_gpio *gpio = gpiochip_get_data(chip);
	void __iomem *base;
	u32 value;

	base = tegra186_gpio_get_base(gpio, offset);
	if (WARN_ON(base == NULL))
		return -ENODEV;

	value = readl(base + TEGRA186_GPIO_ENABLE_CONFIG);
	if (value & TEGRA186_GPIO_ENABLE_CONFIG_OUT)
		value = readl(base + TEGRA186_GPIO_OUTPUT_VALUE);
	else
		value = readl(base + TEGRA186_GPIO_INPUT);

	return value & BIT(0);
}

static void tegra186_gpio_set(struct gpio_chip *chip, unsigned int offset,
			      int level)
{
	struct tegra_gpio *gpio = gpiochip_get_data(chip);
	void __iomem *base;
	u32 value;

	if (!gpio_is_accessible(gpio, offset))
		return;

	base = tegra186_gpio_get_base(gpio, offset);
	if (WARN_ON(base == NULL))
		return;

	value = readl(base + TEGRA186_GPIO_OUTPUT_VALUE);
	if (level == 0)
		value &= ~TEGRA186_GPIO_OUTPUT_VALUE_HIGH;
	else
		value |= TEGRA186_GPIO_OUTPUT_VALUE_HIGH;

	writel(value, base + TEGRA186_GPIO_OUTPUT_VALUE);
}

static int tegra186_gpio_set_config(struct gpio_chip *chip,
				    unsigned int offset,
				    unsigned long config)
{
	struct tegra_gpio *gpio = gpiochip_get_data(chip);
	u32 debounce, value;
	void __iomem *base;

	base = tegra186_gpio_get_base(gpio, offset);
	if (base == NULL)
		return -ENXIO;

	if (pinconf_to_config_param(config) != PIN_CONFIG_INPUT_DEBOUNCE)
		return -ENOTSUPP;

	debounce = pinconf_to_config_argument(config);

	/*
	 * The Tegra186 GPIO controller supports a maximum of 255 ms debounce
	 * time.
	 */
	if (debounce > 255000)
		return -EINVAL;

	debounce = DIV_ROUND_UP(debounce, USEC_PER_MSEC);

	value = TEGRA186_GPIO_DEBOUNCE_CONTROL_THRESHOLD(debounce);
	writel(value, base + TEGRA186_GPIO_DEBOUNCE_CONTROL);

	value = readl(base + TEGRA186_GPIO_ENABLE_CONFIG);
	value |= TEGRA186_GPIO_ENABLE_CONFIG_DEBOUNCE;
	writel(value, base + TEGRA186_GPIO_ENABLE_CONFIG);

	return 0;
}

static int tegra186_gpio_add_pin_ranges(struct gpio_chip *chip)
{
	struct tegra_gpio *gpio = gpiochip_get_data(chip);
	struct pinctrl_dev *pctldev;
	struct device_node *np;
	unsigned int i, j;
	int err;

	if (!gpio->soc->pinmux || gpio->soc->num_pin_ranges == 0)
		return 0;

	np = of_find_compatible_node(NULL, NULL, gpio->soc->pinmux);
	if (!np)
		return -ENODEV;

	pctldev = of_pinctrl_get(np);
	of_node_put(np);
	if (!pctldev)
		return -EPROBE_DEFER;

	for (i = 0; i < gpio->soc->num_pin_ranges; i++) {
		unsigned int pin = gpio->soc->pin_ranges[i].offset, port;
		const char *group = gpio->soc->pin_ranges[i].group;

		port = pin / 8;
		pin = pin % 8;

		if (port >= gpio->soc->num_ports) {
			dev_warn(chip->parent, "invalid port %u for %s\n",
				 port, group);
			continue;
		}

		for (j = 0; j < port; j++)
			pin += gpio->soc->ports[j].pins;

		err = gpiochip_add_pingroup_range(chip, pctldev, pin, group);
		if (err < 0)
			return err;
	}

	return 0;
}

static int tegra186_gpio_of_xlate(struct gpio_chip *chip,
				  const struct of_phandle_args *spec,
				  u32 *flags)
{
	struct tegra_gpio *gpio = gpiochip_get_data(chip);
	unsigned int port, pin, i, offset = 0;

	if (WARN_ON(chip->of_gpio_n_cells < 2))
		return -EINVAL;

	if (WARN_ON(spec->args_count < chip->of_gpio_n_cells))
		return -EINVAL;

	port = spec->args[0] / 8;
	pin = spec->args[0] % 8;

	if (port >= gpio->soc->num_ports) {
		dev_err(chip->parent, "invalid port number: %u\n", port);
		return -EINVAL;
	}

	for (i = 0; i < port; i++)
		offset += gpio->soc->ports[i].pins;

	if (flags)
		*flags = spec->args[1];

	return offset + pin;
}

#define to_tegra_gpio(x) container_of((x), struct tegra_gpio, gpio)

static void tegra186_irq_ack(struct irq_data *data)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(data);
	struct tegra_gpio *gpio = to_tegra_gpio(gc);
	void __iomem *base;

	base = tegra186_gpio_get_base(gpio, data->hwirq);
	if (WARN_ON(base == NULL))
		return;

	writel(1, base + TEGRA186_GPIO_INTERRUPT_CLEAR);
}

static void tegra186_irq_mask(struct irq_data *data)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(data);
	struct tegra_gpio *gpio = to_tegra_gpio(gc);
	void __iomem *base;
	u32 value;

	base = tegra186_gpio_get_base(gpio, data->hwirq);
	if (WARN_ON(base == NULL))
		return;

	value = readl(base + TEGRA186_GPIO_ENABLE_CONFIG);
	value &= ~TEGRA186_GPIO_ENABLE_CONFIG_INTERRUPT;
	writel(value, base + TEGRA186_GPIO_ENABLE_CONFIG);
}

static void tegra186_irq_unmask(struct irq_data *data)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(data);
	struct tegra_gpio *gpio = to_tegra_gpio(gc);
	void __iomem *base;
	u32 value;

	base = tegra186_gpio_get_base(gpio, data->hwirq);
	if (WARN_ON(base == NULL))
		return;

	value = readl(base + TEGRA186_GPIO_ENABLE_CONFIG);
	value |= TEGRA186_GPIO_ENABLE_CONFIG_INTERRUPT;
	writel(value, base + TEGRA186_GPIO_ENABLE_CONFIG);
}

static int tegra186_irq_set_type(struct irq_data *data, unsigned int type)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(data);
	struct tegra_gpio *gpio = to_tegra_gpio(gc);
	void __iomem *base;
	u32 value;

	base = tegra186_gpio_get_base(gpio, data->hwirq);
	if (WARN_ON(base == NULL))
		return -ENODEV;

	value = readl(base + TEGRA186_GPIO_ENABLE_CONFIG);
	value &= ~TEGRA186_GPIO_ENABLE_CONFIG_TRIGGER_TYPE_MASK;
	value &= ~TEGRA186_GPIO_ENABLE_CONFIG_TRIGGER_LEVEL;

	switch (type & IRQ_TYPE_SENSE_MASK) {
	case IRQ_TYPE_NONE:
		break;

	case IRQ_TYPE_EDGE_RISING:
		value |= TEGRA186_GPIO_ENABLE_CONFIG_TRIGGER_TYPE_SINGLE_EDGE;
		value |= TEGRA186_GPIO_ENABLE_CONFIG_TRIGGER_LEVEL;
		break;

	case IRQ_TYPE_EDGE_FALLING:
		value |= TEGRA186_GPIO_ENABLE_CONFIG_TRIGGER_TYPE_SINGLE_EDGE;
		break;

	case IRQ_TYPE_EDGE_BOTH:
		value |= TEGRA186_GPIO_ENABLE_CONFIG_TRIGGER_TYPE_DOUBLE_EDGE;
		break;

	case IRQ_TYPE_LEVEL_HIGH:
		value |= TEGRA186_GPIO_ENABLE_CONFIG_TRIGGER_TYPE_LEVEL;
		value |= TEGRA186_GPIO_ENABLE_CONFIG_TRIGGER_LEVEL;
		break;

	case IRQ_TYPE_LEVEL_LOW:
		value |= TEGRA186_GPIO_ENABLE_CONFIG_TRIGGER_TYPE_LEVEL;
		break;

	default:
		return -EINVAL;
	}

	writel(value, base + TEGRA186_GPIO_ENABLE_CONFIG);

	if ((type & IRQ_TYPE_EDGE_BOTH) == 0)
		irq_set_handler_locked(data, handle_level_irq);
	else
		irq_set_handler_locked(data, handle_edge_irq);

	if (data->parent_data)
		return irq_chip_set_type_parent(data, type);

	return 0;
}

static int tegra186_irq_set_wake(struct irq_data *data, unsigned int on)
{
	if (data->parent_data)
		return irq_chip_set_wake_parent(data, on);

	return 0;
}

static void tegra186_gpio_irq(struct irq_desc *desc)
{
	struct tegra_gpio *gpio = irq_desc_get_handler_data(desc);
	struct irq_domain *domain = gpio->gpio.irq.domain;
	struct irq_chip *chip = irq_desc_get_chip(desc);
	unsigned int parent = irq_desc_get_irq(desc);
	unsigned int i, j, offset = 0;

	chained_irq_enter(chip, desc);

	for (i = 0; i < gpio->soc->num_ports; i++) {
		const struct tegra_gpio_port *port = &gpio->soc->ports[i];
		unsigned int pin, irq;
		unsigned long value;
		void __iomem *base;

		base = gpio->base + port->bank * 0x1000 + port->port * 0x200;

		for (j = 0; j < gpio->num_irqs_per_bank; j++) {
			if (parent == gpio->irq[port->bank * gpio->num_irqs_per_bank + j])
				break;
		}

		if (j == gpio->num_irqs_per_bank)
			goto skip;

		value = readl(base + TEGRA186_GPIO_INTERRUPT_STATUS(1));

		for_each_set_bit(pin, &value, port->pins) {
			irq = irq_find_mapping(domain, offset + pin);
			if (WARN_ON(irq == 0))
				continue;

			generic_handle_irq(irq);
		}

skip:
		offset += port->pins;
	}

	chained_irq_exit(chip, desc);
}

static int tegra186_gpio_irq_domain_translate(struct irq_domain *domain,
					      struct irq_fwspec *fwspec,
					      unsigned long *hwirq,
					      unsigned int *type)
{
	struct tegra_gpio *gpio = gpiochip_get_data(domain->host_data);
	unsigned int port, pin, i, offset = 0;

	if (WARN_ON(gpio->gpio.of_gpio_n_cells < 2))
		return -EINVAL;

	if (WARN_ON(fwspec->param_count < gpio->gpio.of_gpio_n_cells))
		return -EINVAL;

	port = fwspec->param[0] / 8;
	pin = fwspec->param[0] % 8;

	if (port >= gpio->soc->num_ports)
		return -EINVAL;

	for (i = 0; i < port; i++)
		offset += gpio->soc->ports[i].pins;

	*type = fwspec->param[1] & IRQ_TYPE_SENSE_MASK;
	*hwirq = offset + pin;

	return 0;
}

static void *tegra186_gpio_populate_parent_fwspec(struct gpio_chip *chip,
						 unsigned int parent_hwirq,
						 unsigned int parent_type)
{
	struct tegra_gpio *gpio = gpiochip_get_data(chip);
	struct irq_fwspec *fwspec;

	fwspec = kmalloc(sizeof(*fwspec), GFP_KERNEL);
	if (!fwspec)
		return NULL;

	fwspec->fwnode = chip->irq.parent_domain->fwnode;
	fwspec->param_count = 3;
	fwspec->param[0] = gpio->soc->instance;
	fwspec->param[1] = parent_hwirq;
	fwspec->param[2] = parent_type;

	return fwspec;
}

static int tegra186_gpio_child_to_parent_hwirq(struct gpio_chip *chip,
					       unsigned int hwirq,
					       unsigned int type,
					       unsigned int *parent_hwirq,
					       unsigned int *parent_type)
{
	*parent_hwirq = chip->irq.child_offset_to_irq(chip, hwirq);
	*parent_type = type;

	return 0;
}

static unsigned int tegra186_gpio_child_offset_to_irq(struct gpio_chip *chip,
						      unsigned int offset)
{
	struct tegra_gpio *gpio = gpiochip_get_data(chip);
	unsigned int i;

	for (i = 0; i < gpio->soc->num_ports; i++) {
		if (offset < gpio->soc->ports[i].pins)
			break;

		offset -= gpio->soc->ports[i].pins;
	}

	return offset + i * 8;
}

static const struct of_device_id tegra186_pmc_of_match[] = {
	{ .compatible = "nvidia,tegra186-pmc" },
	{ .compatible = "nvidia,tegra194-pmc" },
	{ .compatible = "nvidia,tegra234-pmc" },
	{ /* sentinel */ }
};

static void tegra186_gpio_init_route_mapping(struct tegra_gpio *gpio)
{
	struct device *dev = gpio->gpio.parent;
	unsigned int i, j;
	u32 value;

	for (i = 0; i < gpio->soc->num_ports; i++) {
		const struct tegra_gpio_port *port = &gpio->soc->ports[i];
		unsigned int offset, p = port->port;
		void __iomem *base;

		base = gpio->secure + port->bank * 0x1000 + 0x800;

		value = readl(base + TEGRA186_GPIO_CTL_SCR);

		/*
		 * For controllers that haven't been locked down yet, make
		 * sure to program the default interrupt route mapping.
		 */
		if ((value & TEGRA186_GPIO_CTL_SCR_SEC_REN) == 0 &&
		    (value & TEGRA186_GPIO_CTL_SCR_SEC_WEN) == 0) {
			/*
			 * On Tegra194 and later, each pin can be routed to one or more
			 * interrupts.
			 */
			for (j = 0; j < gpio->num_irqs_per_bank; j++) {
				dev_dbg(dev, "programming default interrupt routing for port %s\n",
					port->name);

				offset = TEGRA186_GPIO_INT_ROUTE_MAPPING(p, j);

				/*
				 * By default we only want to route GPIO pins to IRQ 0. This works
				 * only under the assumption that we're running as the host kernel
				 * and hence all GPIO pins are owned by Linux.
				 *
				 * For cases where Linux is the guest OS, the hypervisor will have
				 * to configure the interrupt routing and pass only the valid
				 * interrupts via device tree.
				 */

				if (j == 0) {
					value = readl(base + offset);
					value = BIT(port->pins) - 1;
					writel(value, base + offset);
				}
			}
		}
	}
}

static unsigned int tegra186_gpio_irqs_per_bank(struct tegra_gpio *gpio)
{
	struct device *dev = gpio->gpio.parent;

	if (gpio->num_irq > gpio->num_banks) {
		if (gpio->num_irq % gpio->num_banks != 0)
			goto error;
	}

	if (gpio->num_irq < gpio->num_banks)
		goto error;

	gpio->num_irqs_per_bank = gpio->num_irq / gpio->num_banks;

	if (gpio->num_irqs_per_bank > gpio->soc->num_irqs_per_bank)
		goto error;

	return 0;

error:
	dev_err(dev, "invalid number of interrupts (%u) for %u banks\n",
		gpio->num_irq, gpio->num_banks);
	return -EINVAL;
}

static int tegra186_gpio_probe(struct platform_device *pdev)
{
	unsigned int i, j, offset;
	struct gpio_irq_chip *irq;
	struct tegra_gpio *gpio;
	struct device_node *np;
	struct resource *res;
	char **names;
	int err;
	int ret;
	int value;
	void __iomem *base;

	gpio = devm_kzalloc(&pdev->dev, sizeof(*gpio), GFP_KERNEL);
	if (!gpio)
		return -ENOMEM;

	gpio->soc = of_device_get_match_data(&pdev->dev);
	gpio->gpio.label = gpio->soc->name;
	gpio->gpio.parent = &pdev->dev;

	gpio->secure = devm_platform_ioremap_resource_byname(pdev, "security");
	if (IS_ERR(gpio->secure))
		return PTR_ERR(gpio->secure);

	/* count the number of banks in the controller */
	for (i = 0; i < gpio->soc->num_ports; i++)
		if (gpio->soc->ports[i].bank > gpio->num_banks)
			gpio->num_banks = gpio->soc->ports[i].bank;

	gpio->num_banks++;

	gpio->base = devm_platform_ioremap_resource_byname(pdev, "gpio");
	if (IS_ERR(gpio->base))
		return PTR_ERR(gpio->base);

	gpio->gpio_rval = devm_kzalloc(&pdev->dev, gpio->soc->num_ports * 8 *
				      sizeof(*gpio->gpio_rval), GFP_KERNEL);
	if (!gpio->gpio_rval)
		return -ENOMEM;

	np = pdev->dev.of_node;
	if (!np) {
		dev_err(&pdev->dev, "No valid device node, probe failed\n");
		return -EINVAL;
	}

	gpio->use_timestamp = of_property_read_bool(np, "use-timestamp");

	if (gpio->use_timestamp) {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "gte");
		if (!res) {
			dev_err(&pdev->dev, "Missing gte MEM resource\n");
			return -ENODEV;
		}
		gpio->gte_regs = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(gpio->gte_regs)) {
			ret = PTR_ERR(gpio->gte_regs);
			dev_err(&pdev->dev,
				"Failed to iomap for gte: %d\n", ret);
			return ret;
		}
	}

	err = platform_irq_count(pdev);
	if (err < 0)
		return err;

	gpio->num_irq = err;

	err = tegra186_gpio_irqs_per_bank(gpio);
	if (err < 0)
		return err;

	gpio->irq = devm_kcalloc(&pdev->dev, gpio->num_irq, sizeof(*gpio->irq),
				 GFP_KERNEL);
	if (!gpio->irq)
		return -ENOMEM;

	for (i = 0; i < gpio->num_irq; i++) {
		err = platform_get_irq(pdev, i);
		if (err < 0)
			return err;

		gpio->irq[i] = err;
	}


	gpio->gpio.request = gpiochip_generic_request;
	gpio->gpio.free = gpiochip_generic_free;
	gpio->gpio.get_direction = tegra186_gpio_get_direction;
	gpio->gpio.direction_input = tegra186_gpio_direction_input;
	gpio->gpio.direction_output = tegra186_gpio_direction_output;
	gpio->gpio.get = tegra186_gpio_get,
	gpio->gpio.set = tegra186_gpio_set;
	gpio->gpio.set_config = tegra186_gpio_set_config;
	gpio->gpio.timestamp_control = tegra_gpio_timestamp_control;
	gpio->gpio.timestamp_read = tegra_gpio_timestamp_read;
	gpio->gpio.suspend_configure = tegra_gpio_suspend_configure;
	gpio->gpio.add_pin_ranges = tegra186_gpio_add_pin_ranges;

	gpio->gpio.base = -1;

	for (i = 0; i < gpio->soc->num_ports; i++)
		gpio->gpio.ngpio += gpio->soc->ports[i].pins;

	names = devm_kcalloc(gpio->gpio.parent, gpio->gpio.ngpio,
			     sizeof(*names), GFP_KERNEL);
	if (!names)
		return -ENOMEM;

	for (i = 0, offset = 0; i < gpio->soc->num_ports; i++) {
		const struct tegra_gpio_port *port = &gpio->soc->ports[i];
		char *name;

		for (j = 0; j < port->pins; j++) {
			name = devm_kasprintf(gpio->gpio.parent, GFP_KERNEL,
					      "P%s.%02x", port->name, j);
			if (!name)
				return -ENOMEM;

			names[offset + j] = name;
		}

		offset += port->pins;
	}

	gpio->gpio.names = (const char * const *)names;

	gpio->gpio.of_node = pdev->dev.of_node;
	gpio->gpio.of_gpio_n_cells = 2;
	gpio->gpio.of_xlate = tegra186_gpio_of_xlate;

	gpio->intc.name = pdev->dev.of_node->name;
	gpio->intc.irq_ack = tegra186_irq_ack;
	gpio->intc.irq_mask = tegra186_irq_mask;
	gpio->intc.irq_unmask = tegra186_irq_unmask;
	gpio->intc.irq_set_type = tegra186_irq_set_type;
	gpio->intc.irq_set_wake = tegra186_irq_set_wake;

	irq = &gpio->gpio.irq;
	irq->chip = &gpio->intc;
	irq->fwnode = of_node_to_fwnode(pdev->dev.of_node);
	irq->child_to_parent_hwirq = tegra186_gpio_child_to_parent_hwirq;
	irq->populate_parent_alloc_arg = tegra186_gpio_populate_parent_fwspec;
	irq->child_offset_to_irq = tegra186_gpio_child_offset_to_irq;
	irq->child_irq_domain_ops.translate = tegra186_gpio_irq_domain_translate;
	irq->handler = handle_simple_irq;
	irq->default_type = IRQ_TYPE_NONE;
	irq->parent_handler = tegra186_gpio_irq;
	irq->parent_handler_data = gpio;
	irq->num_parents = gpio->num_irq;


	/*
	* To simplify things, use a single interrupt per bank for now. Some
	* chips support up to 8 interrupts per bank, which can be useful to
	* distribute the load and decrease the processing latency for GPIOs
	* but it also requires a more complicated interrupt routing than we
	* currently program.
	*/
	if (gpio->num_irqs_per_bank > 1) {
		irq->parents = devm_kcalloc(&pdev->dev, gpio->num_banks,
						sizeof(*irq->parents), GFP_KERNEL);
		if (!irq->parents)
			return -ENOMEM;

		for (i = 0; i < gpio->num_banks; i++)
			irq->parents[i] = gpio->irq[i * gpio->num_irqs_per_bank];

		irq->num_parents = gpio->num_banks;
	} else {
		irq->num_parents = gpio->num_irq;
		irq->parents = gpio->irq;
	}

	if (gpio->soc->num_irqs_per_bank > 1)
		tegra186_gpio_init_route_mapping(gpio);

	np = of_find_matching_node(NULL, tegra186_pmc_of_match);
	if (!of_device_is_available(np))
		np = of_irq_find_parent(pdev->dev.of_node);

	if (of_device_is_available(np)) {
		irq->parent_domain = irq_find_host(np);
		of_node_put(np);

		if (!irq->parent_domain)
			return -EPROBE_DEFER;
	}


	irq->map = devm_kcalloc(&pdev->dev, gpio->gpio.ngpio,
				sizeof(*irq->map), GFP_KERNEL);
	if (!irq->map)
		return -ENOMEM;

	for (i = 0, offset = 0; i < gpio->soc->num_ports; i++) {
		const struct tegra_gpio_port *port = &gpio->soc->ports[i];

		for (j = 0; j < port->pins; j++)
			irq->map[offset + j] = irq->parents[port->bank];

		offset += port->pins;
	}

	platform_set_drvdata(pdev, gpio);

	err = devm_gpiochip_add_data(&pdev->dev, &gpio->gpio, gpio);
	if (err < 0)
		return err;

	if (gpio->soc->is_hw_ts_sup) {
		for (i = 0, offset = 0; i < gpio->soc->num_ports; i++) {
			const struct tegra_gpio_port *port =
							&gpio->soc->ports[i];

			for (j = 0; j < port->pins; j++) {
				base = tegra186_gpio_get_base(gpio, offset + j);
				if (WARN_ON(base == NULL))
					return -EINVAL;

				value = readl(base +
					      TEGRA186_GPIO_ENABLE_CONFIG);
				value |=
				TEGRA186_GPIO_ENABLE_CONFIG_TIMESTAMP_FUNC;
				writel(value,
				       base + TEGRA186_GPIO_ENABLE_CONFIG);
			}
			offset += port->pins;
		}
	}

	if (gpio->use_timestamp)
		tegra_gte_setup(gpio);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tegra_gpio_resume_early(struct device *dev)
{
	struct tegra_gpio *gpio = dev_get_drvdata(dev);
	struct tegra_gpio_saved_register *regs;
	unsigned offset = 0U;
	void __iomem *base;
	int i;

	base = tegra186_gpio_get_base(gpio, offset);
	if (WARN_ON(base == NULL))
		return -EINVAL;

	for (i = 0; i < gpio->gpio.ngpio; i++) {
		regs = &gpio->gpio_rval[i];
		if (!regs->restore_needed)
			continue;

		regs->restore_needed = false;

		writel(regs->val,  base + TEGRA186_GPIO_OUTPUT_VALUE);
		writel(regs->out,  base + TEGRA186_GPIO_OUTPUT_CONTROL);
		writel(regs->conf, base + TEGRA186_GPIO_ENABLE_CONFIG);
	}

	return 0;
}

static int tegra_gpio_suspend_late(struct device *dev)
{
	struct tegra_gpio *gpio = dev_get_drvdata(dev);

	return of_gpiochip_suspend(&gpio->gpio);
}

static const struct dev_pm_ops tegra_gpio_pm = {
	.suspend_late = tegra_gpio_suspend_late,
	.resume_early = tegra_gpio_resume_early,
};
#define TEGRA_GPIO_PM		&tegra_gpio_pm
#else
#define TEGRA_GPIO_PM		NULL
#endif

static int tegra186_gpio_remove(struct platform_device *pdev)
{
	return 0;
}

#define TEGRA186_MAIN_GPIO_PORT(_name, _bank, _port, _pins)	\
	[TEGRA186_MAIN_GPIO_PORT_##_name] = {			\
		.name = #_name,					\
		.bank = _bank,					\
		.port = _port,					\
		.pins = _pins,					\
	}

static const struct tegra_gpio_port tegra186_main_ports[] = {
	TEGRA186_MAIN_GPIO_PORT( A, 2, 0, 7),
	TEGRA186_MAIN_GPIO_PORT( B, 3, 0, 7),
	TEGRA186_MAIN_GPIO_PORT( C, 3, 1, 7),
	TEGRA186_MAIN_GPIO_PORT( D, 3, 2, 6),
	TEGRA186_MAIN_GPIO_PORT( E, 2, 1, 8),
	TEGRA186_MAIN_GPIO_PORT( F, 2, 2, 6),
	TEGRA186_MAIN_GPIO_PORT( G, 4, 1, 6),
	TEGRA186_MAIN_GPIO_PORT( H, 1, 0, 7),
	TEGRA186_MAIN_GPIO_PORT( I, 0, 4, 8),
	TEGRA186_MAIN_GPIO_PORT( J, 5, 0, 8),
	TEGRA186_MAIN_GPIO_PORT( K, 5, 1, 1),
	TEGRA186_MAIN_GPIO_PORT( L, 1, 1, 8),
	TEGRA186_MAIN_GPIO_PORT( M, 5, 3, 6),
	TEGRA186_MAIN_GPIO_PORT( N, 0, 0, 7),
	TEGRA186_MAIN_GPIO_PORT( O, 0, 1, 4),
	TEGRA186_MAIN_GPIO_PORT( P, 4, 0, 7),
	TEGRA186_MAIN_GPIO_PORT( Q, 0, 2, 6),
	TEGRA186_MAIN_GPIO_PORT( R, 0, 5, 6),
	TEGRA186_MAIN_GPIO_PORT( T, 0, 3, 4),
	TEGRA186_MAIN_GPIO_PORT( X, 1, 2, 8),
	TEGRA186_MAIN_GPIO_PORT( Y, 1, 3, 7),
	TEGRA186_MAIN_GPIO_PORT(BB, 2, 3, 2),
	TEGRA186_MAIN_GPIO_PORT(CC, 5, 2, 4),
};

static const struct tegra_gpio_soc tegra186_main_soc = {
	.num_ports = ARRAY_SIZE(tegra186_main_ports),
	.ports = tegra186_main_ports,
	.name = "tegra186-gpio",
	.instance = 0,
	.num_irqs_per_bank = 1,
};

#define TEGRA186_AON_GPIO_PORT(_name, _bank, _port, _pins)	\
	[TEGRA186_AON_GPIO_PORT_##_name] = {			\
		.name = #_name,					\
		.bank = _bank,					\
		.port = _port,					\
		.pins = _pins,					\
	}

static const struct tegra_gpio_port tegra186_aon_ports[] = {
	TEGRA186_AON_GPIO_PORT( S, 0, 1, 5),
	TEGRA186_AON_GPIO_PORT( U, 0, 2, 6),
	TEGRA186_AON_GPIO_PORT( V, 0, 4, 8),
	TEGRA186_AON_GPIO_PORT( W, 0, 5, 8),
	TEGRA186_AON_GPIO_PORT( Z, 0, 7, 4),
	TEGRA186_AON_GPIO_PORT(AA, 0, 6, 8),
	TEGRA186_AON_GPIO_PORT(EE, 0, 3, 3),
	TEGRA186_AON_GPIO_PORT(FF, 0, 0, 5),
};

static const struct tegra_gpio_soc tegra186_aon_soc = {
	.num_ports = ARRAY_SIZE(tegra186_aon_ports),
	.ports = tegra186_aon_ports,
	.name = "tegra186-gpio-aon",
	.instance = 1,
	.num_irqs_per_bank = 1,
};

#define TEGRA194_MAIN_GPIO_PORT(_name, _bank, _port, _pins)	\
	[TEGRA194_MAIN_GPIO_PORT_##_name] = {			\
		.name = #_name,					\
		.bank = _bank,					\
		.port = _port,					\
		.pins = _pins,					\
	}

static const struct tegra_gpio_port tegra194_main_ports[] = {
	TEGRA194_MAIN_GPIO_PORT( A, 1, 2, 8),
	TEGRA194_MAIN_GPIO_PORT( B, 4, 7, 2),
	TEGRA194_MAIN_GPIO_PORT( C, 4, 3, 8),
	TEGRA194_MAIN_GPIO_PORT( D, 4, 4, 4),
	TEGRA194_MAIN_GPIO_PORT( E, 4, 5, 8),
	TEGRA194_MAIN_GPIO_PORT( F, 4, 6, 6),
	TEGRA194_MAIN_GPIO_PORT( G, 4, 0, 8),
	TEGRA194_MAIN_GPIO_PORT( H, 4, 1, 8),
	TEGRA194_MAIN_GPIO_PORT( I, 4, 2, 5),
	TEGRA194_MAIN_GPIO_PORT( J, 5, 1, 6),
	TEGRA194_MAIN_GPIO_PORT( K, 3, 0, 8),
	TEGRA194_MAIN_GPIO_PORT( L, 3, 1, 4),
	TEGRA194_MAIN_GPIO_PORT( M, 2, 3, 8),
	TEGRA194_MAIN_GPIO_PORT( N, 2, 4, 3),
	TEGRA194_MAIN_GPIO_PORT( O, 5, 0, 6),
	TEGRA194_MAIN_GPIO_PORT( P, 2, 5, 8),
	TEGRA194_MAIN_GPIO_PORT( Q, 2, 6, 8),
	TEGRA194_MAIN_GPIO_PORT( R, 2, 7, 6),
	TEGRA194_MAIN_GPIO_PORT( S, 3, 3, 8),
	TEGRA194_MAIN_GPIO_PORT( T, 3, 4, 8),
	TEGRA194_MAIN_GPIO_PORT( U, 3, 5, 1),
	TEGRA194_MAIN_GPIO_PORT( V, 1, 0, 8),
	TEGRA194_MAIN_GPIO_PORT( W, 1, 1, 2),
	TEGRA194_MAIN_GPIO_PORT( X, 2, 0, 8),
	TEGRA194_MAIN_GPIO_PORT( Y, 2, 1, 8),
	TEGRA194_MAIN_GPIO_PORT( Z, 2, 2, 8),
	TEGRA194_MAIN_GPIO_PORT(FF, 3, 2, 2),
	TEGRA194_MAIN_GPIO_PORT(GG, 0, 0, 2)
};

static const struct tegra_gpio_soc tegra194_main_soc = {
	.num_ports = ARRAY_SIZE(tegra194_main_ports),
	.ports = tegra194_main_ports,
	.name = "tegra194-gpio",
	.instance = 0,
	.num_irqs_per_bank = 8,
	.do_vm_check = true,
};

#define TEGRA194_AON_GPIO_PORT(_name, _bank, _port, _pins)	\
	[TEGRA194_AON_GPIO_PORT_##_name] = {			\
		.name = #_name,					\
		.bank = _bank,					\
		.port = _port,					\
		.pins = _pins,					\
	}

static const struct tegra_gpio_port tegra194_aon_ports[] = {
	TEGRA194_AON_GPIO_PORT(AA, 0, 3, 8),
	TEGRA194_AON_GPIO_PORT(BB, 0, 4, 4),
	TEGRA194_AON_GPIO_PORT(CC, 0, 1, 8),
	TEGRA194_AON_GPIO_PORT(DD, 0, 2, 3),
	TEGRA194_AON_GPIO_PORT(EE, 0, 0, 7)
};

static const struct tegra_gpio_soc tegra194_aon_soc = {
	.num_ports = ARRAY_SIZE(tegra194_aon_ports),
	.ports = tegra194_aon_ports,
	.name = "tegra194-gpio-aon",
	.gte_info = tegra194_gte_info,
	.gte_npins = ARRAY_SIZE(tegra194_gte_info),
	.instance = 1,
	.num_irqs_per_bank = 8,
	.is_hw_ts_sup = true,
	.do_vm_check = false,
};

#define TEGRA234_MAIN_GPIO_PORT(_name, _bank, _port, _pins)	\
	[TEGRA234_MAIN_GPIO_PORT_##_name] = {			\
		.name = #_name,					\
		.bank = _bank,					\
		.port = _port,					\
		.pins = _pins,					\
	}

static const struct tegra_gpio_port tegra234_main_ports[] = {
	TEGRA234_MAIN_GPIO_PORT(A, 0, 0, 8),
	TEGRA234_MAIN_GPIO_PORT(B, 0, 3, 1),
	TEGRA234_MAIN_GPIO_PORT(C, 5, 1, 8),
	TEGRA234_MAIN_GPIO_PORT(D, 5, 2, 4),
	TEGRA234_MAIN_GPIO_PORT(E, 5, 3, 8),
	TEGRA234_MAIN_GPIO_PORT(F, 5, 4, 6),
	TEGRA234_MAIN_GPIO_PORT(G, 4, 0, 8),
	TEGRA234_MAIN_GPIO_PORT(H, 4, 1, 8),
	TEGRA234_MAIN_GPIO_PORT(I, 4, 2, 7),
	TEGRA234_MAIN_GPIO_PORT(J, 5, 0, 6),
	TEGRA234_MAIN_GPIO_PORT(K, 3, 0, 8),
	TEGRA234_MAIN_GPIO_PORT(L, 3, 1, 4),
	TEGRA234_MAIN_GPIO_PORT(M, 2, 0, 8),
	TEGRA234_MAIN_GPIO_PORT(N, 2, 1, 8),
	TEGRA234_MAIN_GPIO_PORT(P, 2, 2, 8),
	TEGRA234_MAIN_GPIO_PORT(Q, 2, 3, 8),
	TEGRA234_MAIN_GPIO_PORT(R, 2, 4, 6),
	TEGRA234_MAIN_GPIO_PORT(X, 1, 0, 8),
	TEGRA234_MAIN_GPIO_PORT(Y, 1, 1, 8),
	TEGRA234_MAIN_GPIO_PORT(Z, 1, 2, 8),
	TEGRA234_MAIN_GPIO_PORT(AC, 0, 1, 8),
	TEGRA234_MAIN_GPIO_PORT(AD, 0, 2, 4),
	TEGRA234_MAIN_GPIO_PORT(AE, 3, 3, 2),
	TEGRA234_MAIN_GPIO_PORT(AF, 3, 4, 4),
	TEGRA234_MAIN_GPIO_PORT(AG, 3, 2, 8)
};

static const struct tegra_gpio_soc tegra234_main_soc = {
	.num_ports = ARRAY_SIZE(tegra234_main_ports),
	.ports = tegra234_main_ports,
	.name = "tegra234-gpio",
	.instance = 0,
	.num_irqs_per_bank = 8,
	.do_vm_check = true,
};

#define TEGRA234_AON_GPIO_PORT(_name, _bank, _port, _pins)	\
	[TEGRA234_AON_GPIO_PORT_##_name] = {			\
		.name = #_name,					\
		.bank = _bank,					\
		.port = _port,					\
		.pins = _pins,					\
	}

static const struct tegra_gpio_port tegra234_aon_ports[] = {
	TEGRA234_AON_GPIO_PORT(AA, 0, 4, 8),
	TEGRA234_AON_GPIO_PORT(BB, 0, 5, 4),
	TEGRA234_AON_GPIO_PORT(CC, 0, 2, 8),
	TEGRA234_AON_GPIO_PORT(DD, 0, 3, 3),
	TEGRA234_AON_GPIO_PORT(EE, 0, 0, 8),
	TEGRA234_AON_GPIO_PORT(GG, 0, 1, 1)
};

static const struct tegra_gpio_soc tegra234_aon_soc = {
	.num_ports = ARRAY_SIZE(tegra234_aon_ports),
	.ports = tegra234_aon_ports,
	.name = "tegra234-gpio-aon",
	.instance = 1,
	.num_irqs_per_bank = 8,
	.is_hw_ts_sup = true,
	.do_vm_check = false,
};

#define TEGRA239_MAIN_GPIO_PORT(_name, _bank, _port, _pins)	\
	[TEGRA239_MAIN_GPIO_PORT_##_name] = {			\
		.name = #_name,					\
		.bank = _bank,					\
		.port = _port,					\
		.pins = _pins,					\
	}

static const struct tegra_gpio_port tegra239_main_ports[] = {
	TEGRA239_MAIN_GPIO_PORT(A, 0, 0, 8),
	TEGRA239_MAIN_GPIO_PORT(B, 0, 1, 5),
	TEGRA239_MAIN_GPIO_PORT(C, 0, 2, 8),
	TEGRA239_MAIN_GPIO_PORT(D, 0, 3, 8),
	TEGRA239_MAIN_GPIO_PORT(E, 0, 4, 4),
	TEGRA239_MAIN_GPIO_PORT(F, 0, 5, 8),
	TEGRA239_MAIN_GPIO_PORT(G, 0, 6, 8),
	TEGRA239_MAIN_GPIO_PORT(H, 0, 7, 6),
	TEGRA239_MAIN_GPIO_PORT(J, 1, 0, 8),
	TEGRA239_MAIN_GPIO_PORT(K, 1, 1, 4),
	TEGRA239_MAIN_GPIO_PORT(L, 1, 2, 8),
	TEGRA239_MAIN_GPIO_PORT(M, 1, 3, 8),
	TEGRA239_MAIN_GPIO_PORT(N, 1, 4, 3),
	TEGRA239_MAIN_GPIO_PORT(P, 1, 5, 8),
	TEGRA239_MAIN_GPIO_PORT(Q, 1, 6, 3),
	TEGRA239_MAIN_GPIO_PORT(R, 2, 0, 8),
	TEGRA239_MAIN_GPIO_PORT(S, 2, 1, 8),
	TEGRA239_MAIN_GPIO_PORT(T, 2, 2, 8),
	TEGRA239_MAIN_GPIO_PORT(U, 2, 3, 6),
	TEGRA239_MAIN_GPIO_PORT(V, 2, 4, 2),
	TEGRA239_MAIN_GPIO_PORT(W, 3, 0, 8),
	TEGRA239_MAIN_GPIO_PORT(X, 3, 1, 2)
};

static const struct tegra_gpio_soc tegra239_main_soc = {
	.num_ports = ARRAY_SIZE(tegra239_main_ports),
	.ports = tegra239_main_ports,
	.name = "tegra239-gpio",
	.instance = 0,
	.num_irqs_per_bank = 8,
	.do_vm_check = true,
};

#define TEGRA239_AON_GPIO_PORT(_name, _bank, _port, _pins)	\
	[TEGRA239_AON_GPIO_PORT_##_name] = {			\
		.name = #_name,					\
		.bank = _bank,					\
		.port = _port,					\
		.pins = _pins,					\
	}

static const struct tegra_gpio_port tegra239_aon_ports[] = {
	TEGRA239_AON_GPIO_PORT(AA, 0, 0, 8),
	TEGRA239_AON_GPIO_PORT(BB, 0, 1, 1),
	TEGRA239_AON_GPIO_PORT(CC, 0, 2, 8),
	TEGRA239_AON_GPIO_PORT(DD, 0, 3, 8),
	TEGRA239_AON_GPIO_PORT(EE, 0, 4, 6),
	TEGRA239_AON_GPIO_PORT(FF, 0, 5, 8),
	TEGRA239_AON_GPIO_PORT(GG, 0, 6, 8),
	TEGRA239_AON_GPIO_PORT(HH, 0, 7, 4)
};

static const struct tegra_gpio_soc tegra239_aon_soc = {
	.num_ports = ARRAY_SIZE(tegra239_aon_ports),
	.ports = tegra239_aon_ports,
	.name = "tegra239-gpio-aon",
	.instance = 1,
	.num_irqs_per_bank = 8,
	.is_hw_ts_sup = true,
	.do_vm_check = false,
};

static const struct of_device_id tegra186_gpio_of_match[] = {
	{
		.compatible = "nvidia,tegra186-gpio",
		.data = &tegra186_main_soc
	}, {
		.compatible = "nvidia,tegra186-gpio-aon",
		.data = &tegra186_aon_soc
	}, {
		.compatible = "nvidia,tegra194-gpio",
		.data = &tegra194_main_soc
	}, {
		.compatible = "nvidia,tegra194-gpio-aon",
		.data = &tegra194_aon_soc
	}, {
		.compatible = "nvidia,tegra234-gpio",
		.data = &tegra234_main_soc
	}, {
		.compatible = "nvidia,tegra234-gpio-aon",
		.data = &tegra234_aon_soc
	}, {
		.compatible = "nvidia,tegra239-gpio",
		.data = &tegra239_main_soc
	}, {
		.compatible = "nvidia,tegra239-gpio-aon",
		.data = &tegra239_aon_soc
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, tegra186_gpio_of_match);

static struct platform_driver tegra186_gpio_driver = {
	.driver = {
		.name = "tegra186-gpio",
		.of_match_table = tegra186_gpio_of_match,
		.pm = TEGRA_GPIO_PM,
	},
	.probe = tegra186_gpio_probe,
	.remove = tegra186_gpio_remove,
};
module_platform_driver(tegra186_gpio_driver);

MODULE_DESCRIPTION("NVIDIA Tegra186 GPIO controller driver");
MODULE_AUTHOR("Thierry Reding <treding@nvidia.com>");
MODULE_LICENSE("GPL v2");
