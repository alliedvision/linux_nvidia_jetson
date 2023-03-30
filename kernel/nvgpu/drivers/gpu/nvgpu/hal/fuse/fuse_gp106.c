/*
 * GP106 FUSE
 *
 * Copyright (c) 2017-2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <nvgpu/enabled.h>
#include <nvgpu/io.h>
#include <nvgpu/gk20a.h>
#include <nvgpu/pmu/clk/clk.h>

#include "fuse_gp106.h"

#include <nvgpu/hw/gp106/hw_fuse_gp106.h>

u32 gp106_fuse_read_vin_cal_fuse_rev(struct gk20a *g)
{
	return fuse_vin_cal_fuse_rev_data_v(
		nvgpu_readl(g, fuse_vin_cal_fuse_rev_r()));
}

static int gp106_compute_slope_intercept_data(struct gk20a *g,
				u32 vin_id, u32 *slope, u32 *intercept,
				u32 data, u32 gpc0interceptdata){

	u32 interceptdata = 0U;
	u32 slopedata = 0U;
	u32 gpc0data = 0U;
	u32 gpc0slopedata = 0U;
	bool error_status = false;

	/* read gpc0 irrespective of vin id */
	gpc0data = nvgpu_readl(g, fuse_vin_cal_gpc0_r());
	if (gpc0data == U32_MAX) {
		return -EINVAL;
	}

	switch (vin_id) {
	case CTRL_CLK_VIN_ID_GPC0:
		break;

	case CTRL_CLK_VIN_ID_GPC1:
	case CTRL_CLK_VIN_ID_GPC2:
	case CTRL_CLK_VIN_ID_GPC3:
	case CTRL_CLK_VIN_ID_GPC4:
	case CTRL_CLK_VIN_ID_GPC5:
	case CTRL_CLK_VIN_ID_SYS:
	case CTRL_CLK_VIN_ID_XBAR:
	case CTRL_CLK_VIN_ID_LTC:
		interceptdata = (fuse_vin_cal_gpc1_delta_icpt_int_data_v(data) <<
				 fuse_vin_cal_gpc1_delta_icpt_frac_data_s()) +
				fuse_vin_cal_gpc1_delta_icpt_frac_data_v(data);
		interceptdata = (interceptdata * 1000U) >>
				fuse_vin_cal_gpc1_delta_icpt_frac_data_s();
		slopedata = (fuse_vin_cal_gpc1_delta_slope_int_data_v(data)) *
					1000U;
		break;

	default:
		error_status = true;
		break;
	}

	if (error_status == true) {
		return -EINVAL;
	}
	if (fuse_vin_cal_gpc1_delta_icpt_sign_data_v(data) != 0U) {
		*intercept = gpc0interceptdata - interceptdata;
	} else {
		*intercept = gpc0interceptdata + interceptdata;
	}

	/* slope */
	gpc0slopedata = (fuse_vin_cal_gpc0_slope_int_data_v(gpc0data) <<
			     fuse_vin_cal_gpc0_slope_frac_data_s()) +
			    fuse_vin_cal_gpc0_slope_frac_data_v(gpc0data);
	gpc0slopedata = (gpc0slopedata * 1000U) >>
			    fuse_vin_cal_gpc0_slope_frac_data_s();

	if (fuse_vin_cal_gpc1_delta_slope_sign_data_v(data) != 0U) {
		*slope = gpc0slopedata - slopedata;
	} else {
		*slope = gpc0slopedata + slopedata;
	}
	return 0;
}

int gp106_fuse_read_vin_cal_slope_intercept_fuse(struct gk20a *g,
					     u32 vin_id, u32 *slope,
					     u32 *intercept)
{
	u32 data = 0U;
	u32 gpc0data;
	u32 gpc0interceptdata;
	bool error_status = false;
	int status = 0;

	/* read gpc0 irrespective of vin id */
	gpc0data = nvgpu_readl(g, fuse_vin_cal_gpc0_r());
	if (gpc0data == U32_MAX) {
		return -EINVAL;
	}

	switch (vin_id) {
	case CTRL_CLK_VIN_ID_GPC0:
		break;

	case CTRL_CLK_VIN_ID_GPC1:
		data = nvgpu_readl(g, fuse_vin_cal_gpc1_delta_r());
		break;

	case CTRL_CLK_VIN_ID_GPC2:
		data = nvgpu_readl(g, fuse_vin_cal_gpc2_delta_r());
		break;

	case CTRL_CLK_VIN_ID_GPC3:
		data = nvgpu_readl(g, fuse_vin_cal_gpc3_delta_r());
		break;

	case CTRL_CLK_VIN_ID_GPC4:
		data = nvgpu_readl(g, fuse_vin_cal_gpc4_delta_r());
		break;

	case CTRL_CLK_VIN_ID_GPC5:
		data = nvgpu_readl(g, fuse_vin_cal_gpc5_delta_r());
		break;

	case CTRL_CLK_VIN_ID_SYS:
	case CTRL_CLK_VIN_ID_XBAR:
	case CTRL_CLK_VIN_ID_LTC:
		data = nvgpu_readl(g, fuse_vin_cal_shared_delta_r());
		break;

	default:
		error_status = true;
		break;
	}

	if (error_status == true) {
		return -EINVAL;
	}
	if (data == U32_MAX) {
		return -EINVAL;
	}

	gpc0interceptdata = (fuse_vin_cal_gpc0_icpt_int_data_v(gpc0data) <<
			     fuse_vin_cal_gpc0_icpt_frac_data_s()) +
			    fuse_vin_cal_gpc0_icpt_frac_data_v(gpc0data);
	gpc0interceptdata = (gpc0interceptdata * 1000U) >>
			    fuse_vin_cal_gpc0_icpt_frac_data_s();

	status = gp106_compute_slope_intercept_data(g, vin_id, slope,
			intercept, data, gpc0interceptdata);
	if (status != 0) {
		return -EINVAL;
	}
	return status;
}


int gp106_fuse_read_vin_cal_gain_offset_fuse(struct gk20a *g,
					     u32 vin_id, s8 *gain,
					     s8 *offset)
{
	u32 reg_val = 0;
	u32 data = 0;
	bool error_status = false;

	switch (vin_id) {
	case CTRL_CLK_VIN_ID_GPC0:
		reg_val = nvgpu_readl(g, fuse_vin_cal_gpc0_r());
		break;

	case CTRL_CLK_VIN_ID_GPC1:
		reg_val = nvgpu_readl(g, fuse_vin_cal_gpc1_delta_r());
		break;

	case CTRL_CLK_VIN_ID_GPC2:
		reg_val = nvgpu_readl(g, fuse_vin_cal_gpc2_delta_r());
		break;

	case CTRL_CLK_VIN_ID_GPC3:
		reg_val = nvgpu_readl(g, fuse_vin_cal_gpc3_delta_r());
		break;

	case CTRL_CLK_VIN_ID_GPC4:
		reg_val = nvgpu_readl(g, fuse_vin_cal_gpc4_delta_r());
		break;

	case CTRL_CLK_VIN_ID_GPC5:
		reg_val = nvgpu_readl(g, fuse_vin_cal_gpc5_delta_r());
		break;

	case CTRL_CLK_VIN_ID_SYS:
	case CTRL_CLK_VIN_ID_XBAR:
	case CTRL_CLK_VIN_ID_LTC:
		reg_val = nvgpu_readl(g, fuse_vin_cal_shared_delta_r());
		break;

	default:
		error_status = true;
		break;
	}

	if (error_status == true) {
		return -EINVAL;
	}
	if (reg_val == U32_MAX) {
		return -EINVAL;
	}
	data = (reg_val >> 16U) & 0x1fU;
	*gain = (s8)data;
	data = reg_val & 0x7fU;
	*offset = (s8)data;

	return 0;
}
