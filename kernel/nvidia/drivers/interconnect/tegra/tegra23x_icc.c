/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/interconnect-provider.h>
#include <linux/platform/tegra/mc_utils.h>
#include <linux/version.h>
#include <dt-bindings/interconnect/tegra_icc_id.h>
#include "tegra_icc.h"
#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 15, 0)
#include <soc/tegra/fuse.h>
#endif

/* iso buckets for is_mode_possible algorithm */
#define NUM_ISO_CLIENT_TYPES	4
#define HZ_TO_KHZ_MULT		1000

#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 15, 0)
/* all units in kBps */
static void classify_bw_reqs(struct icc_provider *provider,
		struct mrq_bwmgr_request *req,
		uint32_t *max_floor_kbps, uint32_t *init_bw_floor)
{
	struct icc_node *n;
	struct tegra_icc_node *tn;

	list_for_each_entry(n, &provider->nodes, node_list) {
		tn = n->data;
		switch (tn->type) {
		case TEGRA_ICC_NISO:
			req->bwmgr_rate_req.sum_niso_bw += n->avg_bw;
			*max_floor_kbps = max(*max_floor_kbps, n->peak_bw);
			break;
		case TEGRA_ICC_ISO_DISPLAY:
			req->bwmgr_rate_req.isobw_reqs[0].id =
							TEGRA_ICC_DISPLAY;
			req->bwmgr_rate_req.isobw_reqs[0].iso_bw += n->avg_bw;
			*init_bw_floor = n->peak_bw;
			break;
		case TEGRA_ICC_ISO_VI:
			req->bwmgr_rate_req.isobw_reqs[1].id = TEGRA_ICC_VI;
			req->bwmgr_rate_req.isobw_reqs[1].iso_bw += n->avg_bw;
			break;
		case TEGRA_ICC_ISO_AUDIO:
			req->bwmgr_rate_req.isobw_reqs[2].id = TEGRA_ICC_APE;
			req->bwmgr_rate_req.isobw_reqs[2].iso_bw += n->avg_bw;
			break;
		case TEGRA_ICC_ISO_VIFAL:
			req->bwmgr_rate_req.isobw_reqs[3].id = TEGRA_ICC_VIFAL;
			req->bwmgr_rate_req.isobw_reqs[3].iso_bw += n->avg_bw;
			break;
		default:
			break;
		}
	}
	req->bwmgr_rate_req.num_iso_clients = NUM_ISO_CLIENT_TYPES;

	/* VI cannot tolerate DVFS, request max dram floor when VI is active */
	if (req->bwmgr_rate_req.isobw_reqs[1].iso_bw)
		*max_floor_kbps = UINT_MAX;
}

static uint32_t get_bw(struct mrq_bwmgr_request *req,
					enum tegra_icc_client_type type)
{
	uint32_t i;
	uint32_t bw = 0;

	switch (type) {
	case TEGRA_ICC_ISO_DISPLAY:
		for (i = 0; i < NUM_ISO_CLIENT_TYPES; i++) {
			if (req->bwmgr_rate_req.isobw_reqs[i].id ==
							TEGRA_ICC_DISPLAY)
				bw = req->bwmgr_rate_req.isobw_reqs[i].iso_bw;
		}
		break;
	default:
		/* return overall bw */
		for (i = 0; i < NUM_ISO_CLIENT_TYPES; i++)
			bw += req->bwmgr_rate_req.isobw_reqs[i].iso_bw;
		bw += req->bwmgr_rate_req.sum_niso_bw;
		break;
	}
	return bw;
}

static int calculate_la_ptsa(int id, uint32_t bw, uint32_t init_bw_floor,
	struct mrq_iso_client_response *resp, struct tegra_icc_provider *tp)
{
	struct mrq_iso_client_request req = {0};
	int ret;

	req.cmd = CMD_ISO_CLIENT_CALCULATE_LA;
	req.calculate_la_req.id = (uint32_t)id;
	req.calculate_la_req.bw = bw;
	req.calculate_la_req.init_bw_floor = init_bw_floor;

	memset(&tp->msg, 0, sizeof(struct tegra_bpmp_message));
	tp->msg.mrq = MRQ_ISO_CLIENT;
	tp->msg.tx.data = &req;
	tp->msg.tx.size = sizeof(req);
	tp->msg.rx.data = resp;
	tp->msg.rx.size = sizeof(struct mrq_iso_client_response);

	ret = tegra_bpmp_transfer(tp->bpmp_dev, &tp->msg);
	if (ret < 0) {
		return ret;
	} else if (tp->msg.rx.ret < 0) {
		pr_err("%s failed for (%d) bw = %ukBps\n", __func__, id, bw);
		return -EINVAL;
	}

	return ret;
}

static int set_la_ptsa(int id, uint32_t bw,
		uint32_t final_bw_floor, struct tegra_icc_provider *tp)
{
	struct mrq_iso_client_request req = {0};
	int ret;

	req.cmd = CMD_ISO_CLIENT_SET_LA;
	req.set_la_req.id = (uint32_t)id;
	req.set_la_req.bw = bw;
	req.set_la_req.final_bw_floor = final_bw_floor;

	memset(&tp->msg, 0, sizeof(struct tegra_bpmp_message));
	tp->msg.mrq = MRQ_ISO_CLIENT;
	tp->msg.tx.data = &req;
	tp->msg.tx.size = sizeof(req);
	tp->msg.rx.data = NULL;
	tp->msg.rx.size = 0;

	ret = tegra_bpmp_transfer(tp->bpmp_dev, &tp->msg);
	if (ret < 0) {
		return ret;
	} else if (tp->msg.rx.ret < 0) {
		pr_err("%s failed for (%d) bw = %ukBps\n", __func__, id, bw);
		return -EINVAL;
	}

	return ret;
}

static unsigned long determine_rate(struct mrq_bwmgr_response *br,
	struct mrq_iso_client_response *ir, struct tegra_icc_provider *tp,
	uint32_t max_floor_kbps)
{
	unsigned long clk_rate = tp->min_rate / HZ_TO_KHZ_MULT;
	unsigned long max_floor_khz = 0;

	/* units kHz */
	clk_rate = max_t(unsigned long, clk_rate,
				br->bwmgr_rate_resp.iso_rate_min);
	clk_rate = max_t(unsigned long, clk_rate,
				br->bwmgr_rate_resp.total_rate_min);
	clk_rate = max_t(unsigned long, clk_rate,
				ir->calculate_la_resp.la_rate_floor);
	clk_rate = max_t(unsigned long, clk_rate,
			ir->calculate_la_resp.iso_client_only_rate);
	max_floor_khz = emc_bw_to_freq(max_floor_kbps);
	max_floor_khz = min(max_floor_khz, tp->max_rate / HZ_TO_KHZ_MULT);
	clk_rate = max(clk_rate, max_floor_khz);
	clk_rate = min(clk_rate, tp->max_rate / HZ_TO_KHZ_MULT);
	clk_rate = min(clk_rate, tp->cap_rate / HZ_TO_KHZ_MULT);
	clk_rate *= HZ_TO_KHZ_MULT; /* kHz to Hz*/

	return clk_rate;
}

static int tegra23x_icc_set(struct icc_node *src, struct icc_node *dst)
{
	struct icc_provider *provider = src->provider;
	struct tegra_icc_provider *tp = to_tegra_provider(provider);
	int ret = 0;
	struct mrq_bwmgr_request bwmgr_req = {0};
	struct mrq_bwmgr_response bwmgr_resp = {0};
	uint32_t max_floor_kbps = 0;
	uint32_t init_bw_floor = 0; /* used for display */
	uint32_t final_bw_floor = 0; /* used for display */
	struct mrq_iso_client_response iso_client_resp = {0};
	unsigned long clk_rate = 0;
	struct tegra_icc_node *tn = src->data;
	uint32_t iso_bw_disp = 0;
	uint32_t sum_bw = 0;
	unsigned long cap_req = 0;
	unsigned long cap_khz = tp->cap_rate / HZ_TO_KHZ_MULT;

	if (!tegra_platform_is_silicon())
		return 0;

	classify_bw_reqs(provider, &bwmgr_req, &max_floor_kbps, &init_bw_floor);

	/* nvpmodel emc cap request */
	if (src->id == TEGRA_ICC_NVPMODEL) {
		/* remove existing cap by increasing max rate to bpmp Fmax */
		ret = clk_set_max_rate(tp->dram_clk, UINT_MAX);
		if (ret) {
			pr_err("clk_set_max_rate failed %d\n", ret);
			return ret;
		}

		/* round the requested clock cap rate */
		/* note: the peak_bw 0 is considered an uncapped request */
		cap_req = emc_bw_to_freq(src->peak_bw) * HZ_TO_KHZ_MULT;
		cap_req = cap_req ? cap_req : UINT_MAX;
		clk_rate = clk_round_rate(tp->dram_clk, cap_req);

		/* apply the rounded clock cap rate */
		ret = clk_set_max_rate(tp->dram_clk, clk_rate);
		if (ret) {
			pr_err("clk_set_max_rate fail %d\n", ret);
			return ret;
		}

		tp->cap_rate = clk_rate;
		tp->max_rate = tp->cap_rate;
		return 0;
	}

	/* calculate_la MRQ for disp client */
	if (tn->type == TEGRA_ICC_ISO_DISPLAY) {
		iso_bw_disp = get_bw(&bwmgr_req, TEGRA_ICC_ISO_DISPLAY);

		if (iso_bw_disp) {
			ret = calculate_la_ptsa(src->id, iso_bw_disp,
					init_bw_floor, &iso_client_resp, tp);
			if (ret) {
				pr_err("calculate_la failed %d\n", ret);
				return ret;
			}
			tp->last_disp_la_floor =
				iso_client_resp.calculate_la_resp.la_rate_floor;
		} else
			tp->last_disp_la_floor = 0;
	} else
		iso_client_resp.calculate_la_resp.la_rate_floor =
							tp->last_disp_la_floor;

	sum_bw = get_bw(&bwmgr_req, TEGRA_ICC_NONE);
	if (sum_bw || (tp->rate != tp->min_rate)) {
		/* bwmgr MRQ */
		bwmgr_req.cmd = CMD_BWMGR_CALC_RATE;
		memset(&tp->msg, 0, sizeof(struct tegra_bpmp_message));
		tp->msg.mrq = MRQ_BWMGR;
		tp->msg.tx.data = &bwmgr_req;
		tp->msg.tx.size = sizeof(bwmgr_req);
		tp->msg.rx.data = &bwmgr_resp;
		tp->msg.rx.size = sizeof(bwmgr_resp);

		ret = tegra_bpmp_transfer(tp->bpmp_dev, &tp->msg);
		if (ret < 0) {
			return ret;
		} else if (tp->msg.rx.ret < 0) {
			pr_err("bwmgr req failed for %d\n", src->id);
			return -EINVAL;
		}
	}

	/* check determined rate is < emc_cap for iso */
	if ((tn->type != TEGRA_ICC_NISO) &&
		((bwmgr_resp.bwmgr_rate_resp.iso_rate_min > cap_khz) ||
		(iso_client_resp.calculate_la_resp.la_rate_floor > cap_khz) ||
		(iso_client_resp.calculate_la_resp.iso_client_only_rate >
		cap_khz))) {
		pr_err("iso req failed due to emc_cap %lu\n", cap_khz);
		return -EINVAL;
	}

	clk_rate = determine_rate(&bwmgr_resp, &iso_client_resp,
					tp, max_floor_kbps);

	/* clk_set_rate */
	if (clk_rate && (clk_rate != tp->rate)) {
		ret = clk_set_rate(tp->dram_clk, clk_rate);
		if (ret) {
			pr_err("clk_set_rate failed %d\n", ret);
			return ret;
		}
		tp->rate = clk_rate;
	}

	/* set_la MRQ for display client */
	if ((tn->type == TEGRA_ICC_ISO_DISPLAY) && iso_bw_disp) {
		final_bw_floor =
			max(iso_client_resp.calculate_la_resp.la_rate_floor,
			iso_client_resp.calculate_la_resp.iso_client_only_rate);

		final_bw_floor = emc_freq_to_bw(final_bw_floor);
		ret = set_la_ptsa(src->id, src->avg_bw, final_bw_floor, tp);
		if (ret) {
			pr_err("set_la_ptsa failed %d\n", ret);
			return ret;
		}
	}

	return ret;
}

static int tegra23x_icc_get_init_bw(struct icc_node *node, u32 *avg, u32 *peak)
{
	/*
	 * These are required to maintain sane values before
	 * every client can request bw during boot. Not supported.
	 */
	*avg = 0;
	*peak = 0;

	return 0;
}

const struct tegra_icc_ops tegra23x_icc_ops = {
	.plat_icc_set = tegra23x_icc_set,
	.plat_icc_aggregate = icc_std_aggregate,
	.plat_icc_get_bw = tegra23x_icc_get_init_bw,
};
#else
const struct tegra_icc_ops tegra23x_icc_ops = {0};
#endif
