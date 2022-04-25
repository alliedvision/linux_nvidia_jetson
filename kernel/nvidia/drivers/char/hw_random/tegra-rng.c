/*
 * drivers/char/hw_random/tegra-rng.c
 *
 * hwrng dev node for NVIDIA tegra rng hardware
 *
 * Copyright (c) 2021, NVIDIA Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */
#include <linux/module.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/hw_random.h>
#include <crypto/rng.h>

#define MODULE_NAME "tegra-rng"
#define RETRY_TIMEOUT_SECS	2

static int tegra_rng_read(struct hwrng *h, void *data, size_t max, bool wait)
{
	int ret;
	struct crypto_rng *rng;
	unsigned long t;

	if (!wait)
		return 0;

	rng = crypto_alloc_rng("rng1-elp-tegra",
			CRYPTO_ALG_TYPE_RNG, 0);
	if (IS_ERR(rng)) {
		pr_err("crypto_alloc_rng(rng1-elp-tegra) failed: %ld\n",
				PTR_ERR(rng));
		return PTR_ERR(rng);
	}
	t = get_seconds();
	do {
		ret = crypto_rng_get_bytes(rng, data, max);
		if (ret != -EAGAIN)
			break;
		msleep_interruptible(20);
	} while ((get_seconds() - t) <= RETRY_TIMEOUT_SECS);

	/* crypto_rng_get_bytes returns 0 upon success */
	if (ret == 0)
		ret = max;

	crypto_free_rng(rng);
	return ret;
}

static struct hwrng tegra_rng = {
	.name = MODULE_NAME,
	.read = tegra_rng_read,
};

static int __init tegra_rng_init(void)
{
	return hwrng_register(&tegra_rng);
}
module_init(tegra_rng_init);

static void __exit tegra_rng_exit(void)
{
	hwrng_unregister(&tegra_rng);
}
module_exit(tegra_rng_exit);

MODULE_DESCRIPTION("RNG driver for Tegra devices");
MODULE_AUTHOR("Harinarayan Bhatta <hbhatta@nvidia.com>");
MODULE_LICENSE("GPL v2");
