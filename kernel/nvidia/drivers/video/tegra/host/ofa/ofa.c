/*
 * Tegra OFA Module Support
 *
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/nvhost.h>
#include <linux/iopoll.h>
#include "bus_client.h"
#include "ofa/ofa.h"

#define OFA_IDLE_TIMEOUT_DEFAULT	100000	/* 100 milliseconds */
#define OFA_IDLE_CHECK_PERIOD		10	/* 10 usec */

int ofa_safety_ram_init(struct platform_device *pdev)
{
	int ret;
	int val = 0;
	void __iomem *addr = get_aperture(pdev, 0) +
				ofa_safety_ram_init_done_r();

	host1x_writel(pdev, ofa_safety_ram_init_req_r(), 0x1);

	ret = readl_poll_timeout(addr, val, (val == 1), OFA_IDLE_CHECK_PERIOD,
				OFA_IDLE_TIMEOUT_DEFAULT);
	if (ret)
		dev_err(&pdev->dev, "Ofa safety ram init timeout! val=0x%x\n",
			val);

	return ret;
}
