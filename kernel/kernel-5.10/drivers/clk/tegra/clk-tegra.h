/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2020, NVIDIA CORPORATION.  All rights reserved.
 */

#ifndef __TEGRA_CLK_DOWNSTREAM_H
#define __TEGRA_CLK_DOWNSTREAM_H

struct tegra_clk_skipper {
	struct clk_hw hw;
	void __iomem *reg;
	spinlock_t *lock;
};
#define to_clk_skipper(_hw) container_of(_hw, struct tegra_clk_skipper, hw)

struct clk *tegra_clk_register_skipper(const char *name,
		const char *parent_name, void __iomem *reg,
		unsigned long flags, spinlock_t *lock);

enum shared_bus_users_mode {
	SHARED_FLOOR = 0,
	SHARED_BW,
	SHARED_CEILING,
	SHARED_CEILING_BUT_ISO,
	SHARED_AUTO,
	SHARED_OVERRIDE,
};

#define TEGRA_CLK_SHARED_MAGIC	0x18ce213d

#define TEGRA_SHARED_BUS_RATE_LIMIT	BIT(0)
#define TEGRA_SHARED_BUS_RETENTION	BIT(1)
#define TEGRA_SHARED_BUS_RACE_TO_SLEEP	BIT(2)
#define TEGRA_SHARED_BUS_ROUND_PASS_THRU	BIT(3)
#define TEGRA_SHARED_BUS_EMC_NATIVE	BIT(4)

struct clk_div_sel {
	struct clk_hw *src;
	u32 div; /* stored as a 7.1 divider */
	unsigned long rate;
};

struct tegra_clk_cbus_shared {
	u32			magic;
	struct clk_hw		hw;
	struct list_head	shared_bus_list;
	struct clk		*shared_bus_backup;
	u32			flags;
	unsigned long		min_rate;
	unsigned long		max_rate;
	unsigned long		users_default_rate;
	bool			rate_update_started;
	bool			rate_updating;
	bool			rate_propagating;
	int			(*bus_update)(struct tegra_clk_cbus_shared *);
	struct clk_hw		*top_user;
	struct clk_hw		*slow_user;
	struct clk		*top_clk;
	unsigned long		override_rate;
	union {
		struct {
			struct clk_hw	*mux_clk;
			struct clk_hw	*div_clk;
			struct clk_hw	*pclk;
			struct clk_hw	*hclk;
			struct clk_hw	*sclk_low;
			struct clk_hw	*sclk_high;
			struct clk_hw	*apb_bus;
			struct clk_hw	*ahb_bus;
			unsigned long	threshold;
			int round_table_size;
			bool fallback;
			struct clk_div_sel *round_table;
		} system;
		struct {
			struct list_head	node;
			bool			enabled;
			unsigned long		rate;
			struct clk		*client;
			u32			client_div;
			enum shared_bus_users_mode mode;
			struct clk		*inputs[2];
		} shared_bus_user;
	} u;
};

#define to_clk_cbus_shared(_hw) \
			container_of(_hw, struct tegra_clk_cbus_shared, hw)

struct clk *tegra_clk_register_shared(const char *name,
		const char **parent, u8 num_parents, unsigned long flags,
		enum shared_bus_users_mode mode, const char *client);
struct clk *tegra_clk_register_cbus(const char *name,
		const char *parent, unsigned long flags,
		const char *backup, unsigned long min_rate,
		unsigned long max_rate);
struct clk *tegra_clk_register_gbus(const char *name,
		const char *parent, unsigned long flags,
		unsigned long min_rate, unsigned long max_rate);
struct clk *tegra_clk_register_shared_master(const char *name,
		const char *parent, unsigned long flags,
		unsigned long min_rate, unsigned long max_rate);
struct clk *tegra_clk_register_sbus_cmplx(const char *name,
		const char *parent, const char *mux_clk, const char *div_clk,
		unsigned long flags, const char *pclk, const char *hclk,
		const char *sclk_low, const char *sclk_high,
		unsigned long threshold, unsigned long min_rate,
		unsigned long max_rate);
struct clk *tegra_clk_register_cascade_master(const char *name,
		const char *parent, const char *topclkname,
		unsigned long flags);
struct clk *tegra_clk_register_shared_connect(const char *name,
		const char **parent, u8 num_parents, unsigned long flags,
		enum shared_bus_users_mode mode, const char *client);
void tegra_shared_clk_init(struct tegra_clk *tegra_clks);
void tegra210b01_pll_init(void __iomem *car, void __iomem *pmc,
			  unsigned long osc, unsigned long ref,
			  struct clk **clks);
void tegra210b01_audio_clk_init(void __iomem *clk_base,
				void __iomem *pmc_base,
				struct tegra_clk *tegra_clks);
void tegra210b01_super_clk_init(void __iomem *clk_base,
				void __iomem *pmc_base,
				struct tegra_clk *tegra_clks);
int tegra210b01_init_pllu(void);
struct tegra_clk_pll_params *tegra210b01_get_pllp_params(void);
struct tegra_clk_pll_params *tegra210b01_get_pllc4_params(void);
const struct clk_div_table *tegra210b01_get_pll_vco_post_div_table(void);
void tegra210b01_clock_table_init(struct clk **clks);
void tegra210b01_adjust_clks(struct tegra_clk *tegra_clks);

extern bool div1_5_not_allowed;

#endif /* __TEGRA_CLK_DOWNSTREAM_H */
