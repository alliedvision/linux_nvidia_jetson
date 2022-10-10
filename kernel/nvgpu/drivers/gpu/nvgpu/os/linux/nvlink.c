/*
 * Copyright (c) 2018-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/mutex.h>
#include <linux/io.h>
#ifdef CONFIG_NVGPU_NVLINK
#include <nvlink/common/tegra-nvlink.h>
#endif

#include <nvgpu/gk20a.h>
#include <nvgpu/nvlink.h>
#include <nvgpu/nvlink_minion.h>
#include <nvgpu/enabled.h>
#include <nvgpu/firmware.h>

#ifdef CONFIG_NVGPU_NVLINK
int nvgpu_nvlink_enumerate(struct gk20a *g)
{
	struct nvlink_device *ndev = (struct nvlink_device *) g->nvlink.priv;

	if (!ndev)
		return -ENODEV;

	return nvlink_enumerate(ndev);
}

int nvgpu_nvlink_train(struct gk20a *g, u32 link_id, bool from_off)
{
	struct nvlink_device *ndev = (struct nvlink_device *) g->nvlink.priv;

	if (!ndev)
		return -ENODEV;

	/* Check if the link is connected */
	if (!g->nvlink.links[link_id].remote_info.is_connected)
		return -ENODEV;

	if (from_off)
		return nvlink_transition_intranode_conn_off_to_safe(ndev);

	return nvlink_train_intranode_conn_safe_to_hs(ndev);
}

void nvgpu_nvlink_free_minion_used_mem(struct gk20a *g,
					struct nvgpu_firmware *nvgpu_minion_fw)
{
	struct nvlink_device *ndev = (struct nvlink_device *) g->nvlink.priv;
	struct minion_hdr *minion_hdr = &ndev->minion_hdr;

	nvgpu_kfree(g, minion_hdr->app_code_offsets);
	nvgpu_kfree(g, minion_hdr->app_code_sizes);
	nvgpu_kfree(g, minion_hdr->app_data_offsets);
	nvgpu_kfree(g, minion_hdr->app_data_sizes);

	if (nvgpu_minion_fw) {
		nvgpu_release_firmware(g, nvgpu_minion_fw);
		ndev->minion_img = NULL;
	}
}

/*
 * Load minion FW
 */
int nvgpu_nvlink_minion_load_ucode(struct gk20a *g,
					struct nvgpu_firmware *nvgpu_minion_fw)
{
	int err = 0;
	struct nvlink_device *ndev = (struct nvlink_device *) g->nvlink.priv;
	struct minion_hdr *minion_hdr = &ndev->minion_hdr;
	u32 data_idx = 0;
	u32 app = 0;

	nvgpu_log_fn(g, " ");

	/* Read ucode header */
	minion_hdr->os_code_offset = nvgpu_nvlink_minion_extract_word(
							nvgpu_minion_fw,
							data_idx);
	data_idx += 4;
	minion_hdr->os_code_size = nvgpu_nvlink_minion_extract_word(
							nvgpu_minion_fw,
							data_idx);
	data_idx += 4;
	minion_hdr->os_data_offset = nvgpu_nvlink_minion_extract_word(
							nvgpu_minion_fw,
							data_idx);
	data_idx += 4;
	minion_hdr->os_data_size = nvgpu_nvlink_minion_extract_word(
							nvgpu_minion_fw,
							data_idx);
	data_idx += 4;
	minion_hdr->num_apps = nvgpu_nvlink_minion_extract_word(
							nvgpu_minion_fw,
							data_idx);
	data_idx += 4;

	nvgpu_log(g, gpu_dbg_nvlink,
		"MINION Ucode Header Info:");
	nvgpu_log(g, gpu_dbg_nvlink,
		"-------------------------");
	nvgpu_log(g, gpu_dbg_nvlink,
		"  - OS Code Offset = %u", minion_hdr->os_code_offset);
	nvgpu_log(g, gpu_dbg_nvlink,
		"  - OS Code Size = %u", minion_hdr->os_code_size);
	nvgpu_log(g, gpu_dbg_nvlink,
		"  - OS Data Offset = %u", minion_hdr->os_data_offset);
	nvgpu_log(g, gpu_dbg_nvlink,
		"  - OS Data Size = %u", minion_hdr->os_data_size);
	nvgpu_log(g, gpu_dbg_nvlink,
		"  - Num Apps = %u", minion_hdr->num_apps);

	/* Allocate offset/size arrays for all the ucode apps */
	minion_hdr->app_code_offsets = nvgpu_kcalloc(g,
						minion_hdr->num_apps,
						sizeof(u32));
	if (!minion_hdr->app_code_offsets) {
		nvgpu_err(g, "Couldn't allocate MINION app_code_offsets array");
		return -ENOMEM;
	}

	minion_hdr->app_code_sizes = nvgpu_kcalloc(g,
						minion_hdr->num_apps,
						sizeof(u32));
	if (!minion_hdr->app_code_sizes) {
		nvgpu_err(g, "Couldn't allocate MINION app_code_sizes array");
		return -ENOMEM;
	}

	minion_hdr->app_data_offsets = nvgpu_kcalloc(g,
						minion_hdr->num_apps,
						sizeof(u32));
	if (!minion_hdr->app_data_offsets) {
		nvgpu_err(g, "Couldn't allocate MINION app_data_offsets array");
		return -ENOMEM;
	}

	minion_hdr->app_data_sizes = nvgpu_kcalloc(g,
						minion_hdr->num_apps,
						sizeof(u32));
	if (!minion_hdr->app_data_sizes) {
		nvgpu_err(g, "Couldn't allocate MINION app_data_sizes array");
		return -ENOMEM;
	}

	/* Get app code offsets and sizes */
	for (app = 0; app < minion_hdr->num_apps; app++) {
		minion_hdr->app_code_offsets[app] =
				nvgpu_nvlink_minion_extract_word(
							nvgpu_minion_fw,
							data_idx);
		data_idx += 4;
		minion_hdr->app_code_sizes[app] =
				nvgpu_nvlink_minion_extract_word(
							nvgpu_minion_fw,
							data_idx);
		data_idx += 4;

		nvgpu_log(g, gpu_dbg_nvlink,
			"  - App Code:");
		nvgpu_log(g, gpu_dbg_nvlink,
			"      - App #%d: Code Offset = %u, Code Size = %u",
			app,
			minion_hdr->app_code_offsets[app],
			minion_hdr->app_code_sizes[app]);
	}

	/* Get app data offsets and sizes */
	for (app = 0; app < minion_hdr->num_apps; app++) {
		minion_hdr->app_data_offsets[app] =
				nvgpu_nvlink_minion_extract_word(
							nvgpu_minion_fw,
							data_idx);
		data_idx += 4;
		minion_hdr->app_data_sizes[app] =
				nvgpu_nvlink_minion_extract_word(
							nvgpu_minion_fw,
							data_idx);
		data_idx += 4;

		nvgpu_log(g, gpu_dbg_nvlink,
			"  - App Data:");
		nvgpu_log(g, gpu_dbg_nvlink,
			"      - App #%d: Data Offset = %u, Data Size = %u",
			app,
			minion_hdr->app_data_offsets[app],
			minion_hdr->app_data_sizes[app]);
	}

	minion_hdr->ovl_offset = nvgpu_nvlink_minion_extract_word(
							nvgpu_minion_fw,
							data_idx);
	data_idx += 4;
	minion_hdr->ovl_size = nvgpu_nvlink_minion_extract_word(
							nvgpu_minion_fw,
							data_idx);
	data_idx += 4;

	ndev->minion_img = &(nvgpu_minion_fw->data[data_idx]);
	minion_hdr->ucode_data_size = nvgpu_minion_fw->size - data_idx;

	nvgpu_log(g, gpu_dbg_nvlink,
		"  - Overlay Offset = %u", minion_hdr->ovl_offset);
	nvgpu_log(g, gpu_dbg_nvlink,
		"  - Overlay Size = %u", minion_hdr->ovl_size);
	nvgpu_log(g, gpu_dbg_nvlink,
		"  - Ucode Data Size = %u", minion_hdr->ucode_data_size);

	/* Copy Non Secure IMEM code */
	err = nvgpu_falcon_copy_to_imem(&g->minion_flcn, 0,
		(u8 *)&ndev->minion_img[minion_hdr->os_code_offset],
		minion_hdr->os_code_size, 0, false,
		GET_IMEM_TAG(minion_hdr->os_code_offset));

	if (err != 0) {
		goto exit;
	}

	/* Copy Non Secure DMEM code */
	err = nvgpu_falcon_copy_to_dmem(&g->minion_flcn, 0,
		(u8 *)&ndev->minion_img[minion_hdr->os_data_offset],
		minion_hdr->os_data_size, 0);

	if (err != 0) {
		goto exit;
	}

	/* Load the apps securely */
	for (app = 0; app < minion_hdr->num_apps; app++) {
		u32 app_code_start = minion_hdr->app_code_offsets[app];
		u32 app_code_size = minion_hdr->app_code_sizes[app];
		u32 app_data_start = minion_hdr->app_data_offsets[app];
		u32 app_data_size = minion_hdr->app_data_sizes[app];

		if (app_code_size) {
			err = nvgpu_falcon_copy_to_imem(&g->minion_flcn,
				app_code_start,
				(u8 *)&ndev->minion_img[app_code_start],
				app_code_size, 0, true,
				GET_IMEM_TAG(app_code_start));

			if (err != 0) {
				goto exit;
			}
		}

		if (app_data_size) {
			err = nvgpu_falcon_copy_to_dmem(&g->minion_flcn,
				app_data_start,
				(u8 *)&ndev->minion_img[app_data_start],
				app_data_size, 0);

			if (err != 0) {
				goto exit;
			}
		}
	}

exit:
	return err;
}

void nvgpu_mss_nvlink_init_credits(struct gk20a *g)
{
		/* MSS_NVLINK_1_BASE */
		void __iomem *soc1 = ioremap(0x01f20010, 4096);
		/* MSS_NVLINK_2_BASE */
		void __iomem *soc2 = ioremap(0x01f40010, 4096);
		/* MSS_NVLINK_3_BASE */
		void __iomem *soc3 = ioremap(0x01f60010, 4096);
		/* MSS_NVLINK_4_BASE */
		void __iomem *soc4 = ioremap(0x01f80010, 4096);
		u32 val;

		nvgpu_log(g, gpu_dbg_info, "init nvlink soc credits");

		val = readl_relaxed(soc1);
		writel_relaxed(val, soc1);
		val = readl_relaxed(soc1 + 4);
		writel_relaxed(val, soc1 + 4);

		val = readl_relaxed(soc2);
		writel_relaxed(val, soc2);
		val = readl_relaxed(soc2 + 4);
		writel_relaxed(val, soc2 + 4);

		val = readl_relaxed(soc3);
		writel_relaxed(val, soc3);
		val = readl_relaxed(soc3 + 4);
		writel_relaxed(val, soc3 + 4);

		val = readl_relaxed(soc4);
		writel_relaxed(val, soc4);
		val = readl_relaxed(soc4 + 4);
		writel_relaxed(val, soc4 + 4);
}
#endif /* CONFIG_NVGPU_NVLINK */

int nvgpu_nvlink_deinit(struct gk20a *g)
{
#ifdef CONFIG_NVGPU_NVLINK
	struct nvlink_device *ndev = g->nvlink.priv;
	int err;

	if (!nvgpu_is_enabled(g, NVGPU_SUPPORT_NVLINK))
		return -ENODEV;

	err = nvlink_shutdown(ndev);
	if (err) {
		nvgpu_err(g, "failed to shut down nvlink");
		return err;
	}

	nvgpu_nvlink_remove(g);

	return 0;
#endif
	return -ENODEV;
}
