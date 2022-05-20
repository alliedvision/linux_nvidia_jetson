/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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

#include <nvgpu/gk20a.h>
#include <nvgpu/enabled.h>
#include <nvgpu/nvlink_probe.h>

int nvgpu_nvlink_probe(struct gk20a *g)
{
#ifdef CONFIG_NVGPU_NVLINK
	int err;

	err = nvgpu_nvlink_setup_ndev(g);
	if (err != 0) {
		return err;
	}

	err = nvgpu_nvlink_read_dt_props(g);
	if (err != 0) {
		goto free_ndev;
	}

	err = nvgpu_nvlink_init_ops(g);
	if (err != 0) {
		goto free_ndev;
	}

	/* Register device with core driver*/
	err = nvgpu_nvlink_register_device(g);
	if (err != 0) {
		nvgpu_err(g, "failed on nvlink device registration");
		goto free_ndev;
	}

	/* Register link with core driver */
	err = nvgpu_nvlink_register_link(g);
	if (err != 0) {
		nvgpu_err(g, "failed on nvlink link registration");
		goto unregister_ndev;
	}

	/* Enable NVLINK support */
	nvgpu_set_enabled(g, NVGPU_SUPPORT_NVLINK, true);
	return 0;

unregister_ndev:
	err = nvgpu_nvlink_unregister_device(g);

free_ndev:
	nvgpu_kfree(g, g->nvlink.priv);
	g->nvlink.priv = NULL;
	return err;
#else
	return -ENODEV;
#endif
}

