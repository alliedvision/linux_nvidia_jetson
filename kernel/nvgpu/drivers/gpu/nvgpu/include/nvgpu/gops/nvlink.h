/*
 * Copyright (c) 2020-2022, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef NVGPU_GOPS_NVLINK_H
#define NVGPU_GOPS_NVLINK_H

#ifdef CONFIG_NVGPU_DGPU
/* API */
struct gops_nvlink_link_mode_transitions {
	int (*setup_pll)(struct gk20a *g,
			unsigned long link_mask);
	int (*data_ready_en)(struct gk20a *g,
			unsigned long link_mask, bool sync);
	u32 (*get_link_state)(struct gk20a *g, u32 link_id);
	enum nvgpu_nvlink_link_mode (*get_link_mode)(
			struct gk20a *g,
			u32 link_id);
	int (*set_link_mode)(struct gk20a *g, u32 link_id,
			enum nvgpu_nvlink_link_mode mode);
	u32 (*get_rx_sublink_state)(struct gk20a *g,
			u32 link_id);
	u32 (*get_tx_sublink_state)(struct gk20a *g,
			u32 link_id);
	enum nvgpu_nvlink_sublink_mode (*get_sublink_mode)(
			struct gk20a *g, u32 link_id,
			bool is_rx_sublink);
	int (*set_sublink_mode)(struct gk20a *g, u32 link_id,
			bool is_rx_sublink,
			enum nvgpu_nvlink_sublink_mode mode);
};

struct gops_nvlink_minion {
	u32 (*base_addr)(struct gk20a *g);
	bool (*is_running)(struct gk20a *g);
	int (*is_boot_complete)(struct gk20a *g,
				bool *boot_cmplte);
	u32 (*get_dlcmd_ordinal)(struct gk20a *g,
			enum nvgpu_nvlink_minion_dlcmd dlcmd);
	int (*send_dlcmd)(struct gk20a *g, u32 link_id,
			enum nvgpu_nvlink_minion_dlcmd dlcmd,
			bool sync);
	void (*clear_intr)(struct gk20a *g);
	void (*init_intr)(struct gk20a *g);
	void (*enable_link_intr)(struct gk20a *g, u32 link_id,
						bool enable);
	void (*falcon_isr)(struct gk20a *g);
	void (*isr)(struct gk20a *g);
	bool (*is_debug_mode)(struct gk20a *g);
};

struct gops_nvlink_intr {
	void (*init_link_err_intr)(struct gk20a *g, u32 link_id);
	void (*enable_link_err_intr)(struct gk20a *g,
				u32 link_id, bool enable);
	void (*isr)(struct gk20a *g);
};

struct gops_nvlink {
	int (*init)(struct gk20a *g);
	u32 (*get_link_reset_mask)(struct gk20a *g);
	int (*discover_link)(struct gk20a *g);
	int (*rxdet)(struct gk20a *g, u32 link_id);
	void (*get_connected_link_mask)(u32 *link_mask);
	void (*set_sw_errata)(struct gk20a *g, u32 link_id);
	int (*configure_ac_coupling)(struct gk20a *g,
		unsigned long mask, bool sync);
	void (*prog_alt_clk)(struct gk20a *g);
	void (*clear_link_reset)(struct gk20a *g, u32 link_id);
	void (*enable_link_an0)(struct gk20a *g, u32 link_id);
	/* API */
	struct gops_nvlink_link_mode_transitions link_mode_transitions;
	int (*reg_init)(struct gk20a *g);
	struct gops_nvlink_minion minion;
	struct gops_nvlink_intr intr;
};
#endif

#endif /* NVGPU_GOPS_NVLINK_H */
