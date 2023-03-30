/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
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
#include <nvgpu/timers.h>
#include <nvgpu/falcon.h>
#include <nvgpu/io.h>
#include <nvgpu/soc.h>
#include <nvgpu/static_analysis.h>

#include "falcon_sw_gk20a.h"
#ifdef CONFIG_NVGPU_DGPU
#include "falcon_sw_tu104.h"
#endif
#include "falcon_sw_ga10b.h"

#if defined(CONFIG_NVGPU_NON_FUSA) && defined(CONFIG_NVGPU_NEXT)
#include <nvgpu_next_falcon.h>
#endif

static bool is_falcon_valid(struct nvgpu_falcon *flcn)
{
	if (flcn == NULL) {
		return false;
	}

	if (!flcn->is_falcon_supported) {
		nvgpu_err(flcn->g, "Falcon %d not supported", flcn->flcn_id);
		return false;
	}

	return true;
}

u32 nvgpu_falcon_readl(struct nvgpu_falcon *flcn, u32 offset)
{
	return nvgpu_readl(flcn->g,
			   nvgpu_safe_add_u32(flcn->flcn_base, offset));
}

void nvgpu_falcon_writel(struct nvgpu_falcon *flcn,
				       u32 offset, u32 val)
{
	nvgpu_writel(flcn->g, nvgpu_safe_add_u32(flcn->flcn_base, offset), val);
}

int nvgpu_falcon_reset(struct nvgpu_falcon *flcn)
{
	struct gk20a *g;
	int status = 0;

	if (!is_falcon_valid(flcn)) {
		return -EINVAL;
	}

	g = flcn->g;

	if (flcn->flcn_engine_dep_ops.reset_eng != NULL) {
		/* falcon & engine reset */
		status = flcn->flcn_engine_dep_ops.reset_eng(g);
	} else {
		g->ops.falcon.reset(flcn);
	}

	if (status == 0) {
		status = nvgpu_falcon_mem_scrub_wait(flcn);
	}

	return status;
}

int nvgpu_falcon_wait_for_halt(struct nvgpu_falcon *flcn, unsigned int timeout)
{
	struct nvgpu_timeout to;
	struct gk20a *g;
	int status = 0;

	if (!is_falcon_valid(flcn)) {
		return -EINVAL;
	}

	g = flcn->g;

	nvgpu_timeout_init_cpu_timer(g, &to, timeout);

	do {
		if (g->ops.falcon.is_falcon_cpu_halted(flcn)) {
			break;
		}

		nvgpu_udelay(10);
	} while (nvgpu_timeout_expired(&to) == 0);

	if (nvgpu_timeout_peek_expired(&to)) {
		status = -ETIMEDOUT;
	}

	return status;
}

int nvgpu_falcon_wait_idle(struct nvgpu_falcon *flcn)
{
	struct nvgpu_timeout timeout;
	struct gk20a *g;

	if (!is_falcon_valid(flcn)) {
		return -EINVAL;
	}

	g = flcn->g;

	nvgpu_timeout_init_retry(g, &timeout, 2000);

	/* wait for falcon idle */
	do {
		if (g->ops.falcon.is_falcon_idle(flcn)) {
			break;
		}

		if (nvgpu_timeout_expired_msg(&timeout,
			"waiting for falcon idle") != 0) {
			return -ETIMEDOUT;
		}

		nvgpu_usleep_range(100, 200);
	} while (true);

	return 0;
}

int nvgpu_falcon_mem_scrub_wait(struct nvgpu_falcon *flcn)
{
	struct nvgpu_timeout timeout;
	/**
	 * Delay depends on memory size and pwr_clk
	 * delay = (MAX {IMEM_SIZE, DMEM_SIZE} * 64 + 1) / pwr_clk
	 * Timeout set is 1msec & status check at interval 10usec
	 */
	const u32 mem_scrubbing_max_timeout = 1000U;
	const u32 mem_scrubbing_default_timeout = 10U;
	struct gk20a *g;
	int status = 0;

	if (!is_falcon_valid(flcn)) {
		return -EINVAL;
	}

	g = flcn->g;

	/* check IMEM/DMEM scrubbing complete status */
	nvgpu_timeout_init_retry(g, &timeout,
				 mem_scrubbing_max_timeout /
				 mem_scrubbing_default_timeout);

	do {
		if (g->ops.falcon.is_falcon_scrubbing_done(flcn)) {
			break;
		}
		nvgpu_udelay(mem_scrubbing_default_timeout);
	} while (nvgpu_timeout_expired(&timeout) == 0);

	if (nvgpu_timeout_peek_expired(&timeout)) {
		status = -ETIMEDOUT;
	}

	return status;
}

int nvgpu_falcon_wait_for_nvriscv_brom_completion(struct nvgpu_falcon *flcn)
{
	struct gk20a *g;
	u32 timeoutms = 0;
	u32 retcode = 0;
	int err = 0;

	if (!is_falcon_valid(flcn)) {
		return -EINVAL;
	}

	g = flcn->g;

	if (nvgpu_platform_is_silicon(g)) {
		timeoutms = NVRISCV_BR_COMPLETION_TIMEOUT_SILICON_MS;
	} else {
		timeoutms = NVRISCV_BR_COMPLETION_TIMEOUT_NON_SILICON_MS;
	}

	do {
		retcode = g->ops.falcon.get_brom_retcode(flcn);
		if (g->ops.falcon.check_brom_passed(retcode)) {
			break;
		}

		if (g->ops.falcon.check_brom_failed(retcode)) {
			err = -ENOTRECOVERABLE;
			nvgpu_err(g, "Falcon-%d RISCV BROM Failed", flcn->flcn_id);
			goto exit;
		}

		if (timeoutms <= 0) {
			nvgpu_err(g, "Falcon-%d RISCV BROM timed out, limit: %d ms",
					flcn->flcn_id, timeoutms);
			err =  -ETIMEDOUT;
			goto exit;
		}

		nvgpu_msleep(NVRISCV_BR_COMPLETION_POLLING_TIME_INTERVAL_MS);
		timeoutms -= NVRISCV_BR_COMPLETION_POLLING_TIME_INTERVAL_MS;

	} while (true);

	nvgpu_falcon_dbg(flcn->g, "Falcon-%d RISCV BROM passed",
			flcn->flcn_id);

exit:
	g->ops.falcon.dump_brom_stats(flcn);
	return err;
}


static int falcon_memcpy_params_check(struct nvgpu_falcon *flcn,
		u32 offset, u32 size, enum falcon_mem_type mem_type, u8 port)
{
	struct gk20a *g = flcn->g;
	u32 mem_size = 0;
	int ret = -EINVAL;

	if (size == 0U) {
		nvgpu_err(g, "size is zero");
		goto exit;
	}

	if ((offset & 0x3U) != 0U) {
		nvgpu_err(g, "offset (0x%08x) not 4-byte aligned", offset);
		goto exit;
	}

	if (port >= g->ops.falcon.get_ports_count(flcn, mem_type)) {
		nvgpu_err(g, "invalid port %u", (u32) port);
		goto exit;
	}

	mem_size = g->ops.falcon.get_mem_size(flcn, mem_type);

	if (!((offset < mem_size) && ((offset + size) <= mem_size))) {
		nvgpu_err(g, "flcn-id 0x%x, copy overflow ",
			flcn->flcn_id);
		nvgpu_err(g, "total size 0x%x, offset 0x%x, copy size 0x%x",
			mem_size, offset, size);
		ret = -EINVAL;
		goto exit;
	}

	ret = 0;

exit:
	return ret;
}

int nvgpu_falcon_copy_to_dmem(struct nvgpu_falcon *flcn,
	u32 dst, u8 *src, u32 size, u8 port)
{
	int status = -EINVAL;
	struct gk20a *g;

	if (!is_falcon_valid(flcn)) {
		return -EINVAL;
	}

	g = flcn->g;

	if (falcon_memcpy_params_check(flcn, dst, size, MEM_DMEM, port) != 0) {
		nvgpu_err(g, "incorrect parameters");
		goto exit;
	}

	nvgpu_mutex_acquire(&flcn->dmem_lock);
	status = g->ops.falcon.copy_to_dmem(flcn, dst, src, size, port);
	nvgpu_mutex_release(&flcn->dmem_lock);

exit:
	return status;
}

int nvgpu_falcon_copy_to_imem(struct nvgpu_falcon *flcn,
	u32 dst, u8 *src, u32 size, u8 port, bool sec, u32 tag)
{
	int status = -EINVAL;
	struct gk20a *g;

	if (!is_falcon_valid(flcn)) {
		return -EINVAL;
	}

	g = flcn->g;

	if (falcon_memcpy_params_check(flcn, dst, size, MEM_IMEM, port) != 0) {
		nvgpu_err(g, "incorrect parameters");
		goto exit;
	}

	nvgpu_mutex_acquire(&flcn->imem_lock);
	status = g->ops.falcon.copy_to_imem(flcn, dst, src,
					    size, port, sec, tag);
	nvgpu_mutex_release(&flcn->imem_lock);

exit:
	return status;
}

u32 nvgpu_falcon_mailbox_read(struct nvgpu_falcon *flcn, u32 mailbox_index)
{
	struct gk20a *g;
	u32 data = 0;

	if (!is_falcon_valid(flcn)) {
		return 0;
	}

	g = flcn->g;

	if (mailbox_index >= FALCON_MAILBOX_COUNT) {
		nvgpu_err(g, "incorrect mailbox id %d", mailbox_index);
		goto exit;
	}

	data = g->ops.falcon.mailbox_read(flcn, mailbox_index);

exit:
	return data;
}

void nvgpu_falcon_mailbox_write(struct nvgpu_falcon *flcn, u32 mailbox_index,
		u32 data)
{
	struct gk20a *g;

	if (!is_falcon_valid(flcn)) {
		return;
	}

	g = flcn->g;

	if (mailbox_index >= FALCON_MAILBOX_COUNT) {
		nvgpu_err(g, "incorrect mailbox id %d", mailbox_index);
		goto exit;
	}

	g->ops.falcon.mailbox_write(flcn, mailbox_index, data);

exit:
	return;
}

int nvgpu_falcon_hs_ucode_load_bootstrap(struct nvgpu_falcon *flcn, u32 *ucode,
	u32 *ucode_header)
{
	struct gk20a *g;
	u32 sec_imem_dest = 0U;
	int err = 0;

	if (!is_falcon_valid(flcn)) {
		return -EINVAL;
	}

	g = flcn->g;

	/* falcon reset */
	err = nvgpu_falcon_reset(flcn);
	if (err != 0) {
		nvgpu_err(g, "nvgpu_falcon_reset() failed err=%d", err);
		return err;
	}

	/* setup falcon apertures, boot-config */
	if (flcn->flcn_engine_dep_ops.setup_bootstrap_config != NULL) {
		flcn->flcn_engine_dep_ops.setup_bootstrap_config(flcn->g);
	}

	/* Copy Non Secure IMEM code */
	err = nvgpu_falcon_copy_to_imem(flcn, 0U,
		(u8 *)&ucode[ucode_header[OS_CODE_OFFSET] >> 2U],
		ucode_header[OS_CODE_SIZE], 0U, false,
		GET_IMEM_TAG(ucode_header[OS_CODE_OFFSET]));
	if (err != 0) {
		nvgpu_err(g, "HS ucode non-secure code to IMEM failed");
		goto exit;
	}

	/* Put secure code after non-secure block */
	sec_imem_dest = GET_NEXT_BLOCK(ucode_header[OS_CODE_SIZE]);

	err = nvgpu_falcon_copy_to_imem(flcn, sec_imem_dest,
		(u8 *)&ucode[ucode_header[APP_0_CODE_OFFSET] >> 2U],
		ucode_header[APP_0_CODE_SIZE], 0U, true,
		GET_IMEM_TAG(ucode_header[APP_0_CODE_OFFSET]));
	if (err != 0) {
		nvgpu_err(g, "HS ucode secure code to IMEM failed");
		goto exit;
	}

	/* load DMEM: ensure that signatures are patched */
	err = nvgpu_falcon_copy_to_dmem(flcn, 0U, (u8 *)&ucode[
		ucode_header[OS_DATA_OFFSET] >> 2U],
		ucode_header[OS_DATA_SIZE], 0U);
	if (err != 0) {
		nvgpu_err(g, "HS ucode data copy to DMEM failed");
		goto exit;
	}

	/*
	 * Write non-zero value to mailbox register which is updated by
	 * HS bin to denote its return status.
	 */
	nvgpu_falcon_mailbox_write(flcn, FALCON_MAILBOX_0, 0xdeadbeefU);

	/* set BOOTVEC to start of non-secure code */
	g->ops.falcon.bootstrap(flcn, 0U);

exit:
	return err;
}

u32 nvgpu_falcon_get_id(struct nvgpu_falcon *flcn)
{
	return flcn->flcn_id;
}

bool nvgpu_falcon_is_falcon2_enabled(struct nvgpu_falcon *flcn)
{
	return flcn->is_falcon2_enabled ? true : false;
}

bool nvgpu_falcon_is_feature_supported(struct nvgpu_falcon *flcn,
		u32 feature)
{
	return nvgpu_test_bit(feature, (void *)&flcn->fuse_settings);
}

struct nvgpu_falcon *nvgpu_falcon_get_instance(struct gk20a *g, u32 flcn_id)
{
	struct nvgpu_falcon *flcn = NULL;

	switch (flcn_id) {
	case FALCON_ID_PMU:
		flcn = &g->pmu_flcn;
		break;
	case FALCON_ID_FECS:
		flcn = &g->fecs_flcn;
		break;
	case FALCON_ID_GPCCS:
		flcn = &g->gpccs_flcn;
		break;
	case FALCON_ID_GSPLITE:
		flcn = &g->gsp_flcn;
		break;
#ifdef CONFIG_NVGPU_DGPU
	case FALCON_ID_NVDEC:
		flcn = &g->nvdec_flcn;
		break;
	case FALCON_ID_SEC2:
		flcn = &g->sec2.flcn;
		break;
	case FALCON_ID_MINION:
		flcn = &g->minion_flcn;
		break;
#endif
	default:
		nvgpu_err(g, "Invalid/Unsupported falcon ID %x", flcn_id);
		break;
	};

	return flcn;
}

static int falcon_sw_chip_init(struct gk20a *g, struct nvgpu_falcon *flcn)
{
	u32 ver = nvgpu_safe_add_u32(g->params.gpu_arch, g->params.gpu_impl);
	int err = 0;

	switch (ver) {
#ifdef CONFIG_NVGPU_NON_FUSA
	case GK20A_GPUID_GM20B:
	case GK20A_GPUID_GM20B_B:
		gk20a_falcon_sw_init(flcn);
		break;
	case NVGPU_GPUID_GP10B:
		gk20a_falcon_sw_init(flcn);
		break;
#ifdef CONFIG_NVGPU_DGPU
	case NVGPU_GPUID_TU104:
	case NVGPU_GPUID_GA100:
		tu104_falcon_sw_init(flcn);
		break;
#endif /* CONFIG_NVGPU_DGPU */
#endif /* CONFIG_NVGPU_NON_FUSA */
	case NVGPU_GPUID_GA10B:
		ga10b_falcon_sw_init(flcn);
		break;
	case NVGPU_GPUID_GV11B:
		gk20a_falcon_sw_init(flcn);
		break;
	default:
#if defined(CONFIG_NVGPU_NON_FUSA) && defined(CONFIG_NVGPU_NEXT)
		err = nvgpu_next_falcon_sw_init(g, flcn);
		if (err != 0)
#endif
		{
			err = -ENODEV;
			nvgpu_err(g, "no support for GPUID %x", ver);
		}
		break;
	}

	return err;
}

int nvgpu_falcon_sw_init(struct gk20a *g, u32 flcn_id)
{
	struct nvgpu_falcon *flcn = NULL;
	int err = 0;

	flcn = nvgpu_falcon_get_instance(g, flcn_id);
	if (flcn == NULL) {
		return -ENODEV;
	}

	flcn->flcn_id = flcn_id;
	flcn->g = g;

	/* call SW init methods to assign flcn base & support of a falcon */
	err = falcon_sw_chip_init(g, flcn);
	if (err != 0) {
		nvgpu_err(g, "Chip specific falcon sw init failed %d", err);
		return err;
	}

	nvgpu_mutex_init(&flcn->imem_lock);
	nvgpu_mutex_init(&flcn->dmem_lock);

#ifdef CONFIG_NVGPU_DGPU
	if (flcn->emem_supported) {
		nvgpu_mutex_init(&flcn->emem_lock);
	}
#endif

	return 0;
}

void nvgpu_falcon_sw_free(struct gk20a *g, u32 flcn_id)
{
	struct nvgpu_falcon *flcn = NULL;

	flcn = nvgpu_falcon_get_instance(g, flcn_id);
	if (flcn == NULL) {
		return;
	}

	if (flcn->is_falcon_supported) {
		flcn->is_falcon_supported = false;
	} else {
		nvgpu_log_info(g, "falcon 0x%x not supported on %s",
			flcn->flcn_id, g->name);
		return;
	}

#ifdef CONFIG_NVGPU_DGPU
	if (flcn->emem_supported) {
		nvgpu_mutex_destroy(&flcn->emem_lock);
	}
#endif
	nvgpu_mutex_destroy(&flcn->dmem_lock);
	nvgpu_mutex_destroy(&flcn->imem_lock);
}

void nvgpu_falcon_set_irq(struct nvgpu_falcon *flcn, bool enable,
	u32 intr_mask, u32 intr_dest)
{
	struct gk20a *g;

	if (!is_falcon_valid(flcn)) {
		return;
	}

	g = flcn->g;

	if (!flcn->is_interrupt_enabled) {
		nvgpu_warn(g, "Interrupt not supported on flcn 0x%x ",
			flcn->flcn_id);
		return;
	}

	g->ops.falcon.set_irq(flcn, enable, intr_mask, intr_dest);
}

int nvgpu_falcon_get_mem_size(struct nvgpu_falcon *flcn,
			      enum falcon_mem_type type, u32 *size)
{
	if (!is_falcon_valid(flcn)) {
		return -EINVAL;
	}

	*size = flcn->g->ops.falcon.get_mem_size(flcn, type);

	return 0;
}

#ifdef CONFIG_NVGPU_DGPU
int nvgpu_falcon_copy_from_emem(struct nvgpu_falcon *flcn,
	u32 src, u8 *dst, u32 size, u8 port)
{
	struct nvgpu_falcon_engine_dependency_ops *flcn_dops;
	int status = -EINVAL;
	struct gk20a *g;

	if (!is_falcon_valid(flcn)) {
		return -EINVAL;
	}

	g = flcn->g;
	flcn_dops = &flcn->flcn_engine_dep_ops;

	if (flcn_dops->copy_from_emem != NULL) {
		nvgpu_mutex_acquire(&flcn->emem_lock);
		status = flcn_dops->copy_from_emem(g, src, dst, size, port);
		nvgpu_mutex_release(&flcn->emem_lock);
	} else {
		nvgpu_warn(g, "Invalid op on falcon 0x%x ",
			flcn->flcn_id);
		goto exit;
	}

exit:
	return status;
}

int nvgpu_falcon_copy_to_emem(struct nvgpu_falcon *flcn,
	u32 dst, u8 *src, u32 size, u8 port)
{
	struct nvgpu_falcon_engine_dependency_ops *flcn_dops;
	int status = -EINVAL;
	struct gk20a *g;

	if (!is_falcon_valid(flcn)) {
		return -EINVAL;
	}

	g = flcn->g;
	flcn_dops = &flcn->flcn_engine_dep_ops;

	if (flcn_dops->copy_to_emem != NULL) {
		nvgpu_mutex_acquire(&flcn->emem_lock);
		status = flcn_dops->copy_to_emem(g, dst, src, size, port);
		nvgpu_mutex_release(&flcn->emem_lock);
	} else {
		nvgpu_warn(g, "Invalid op on falcon 0x%x ",
			flcn->flcn_id);
		goto exit;
	}

exit:
	return status;
}
#endif

#ifdef CONFIG_NVGPU_FALCON_DEBUG
void nvgpu_falcon_dump_stats(struct nvgpu_falcon *flcn)
{
	if (!is_falcon_valid(flcn)) {
		return;
	}

	flcn->g->ops.falcon.dump_falcon_stats(flcn);
}
#endif

#if defined(CONFIG_NVGPU_FALCON_DEBUG) || defined(CONFIG_NVGPU_FALCON_NON_FUSA)
int nvgpu_falcon_copy_from_dmem(struct nvgpu_falcon *flcn,
	u32 src, u8 *dst, u32 size, u8 port)
{
	int status = -EINVAL;
	struct gk20a *g;

	if (!is_falcon_valid(flcn)) {
		return -EINVAL;
	}

	g = flcn->g;

	if (falcon_memcpy_params_check(flcn, src, size, MEM_DMEM, port) != 0) {
		nvgpu_err(g, "incorrect parameters");
		goto exit;
	}

	nvgpu_mutex_acquire(&flcn->dmem_lock);
	status = g->ops.falcon.copy_from_dmem(flcn, src, dst, size, port);
	nvgpu_mutex_release(&flcn->dmem_lock);

exit:
	return status;
}
#endif

#ifdef CONFIG_NVGPU_FALCON_NON_FUSA
int nvgpu_falcon_bootstrap(struct nvgpu_falcon *flcn, u32 boot_vector)
{
	if (!is_falcon_valid(flcn)) {
		return -EINVAL;
	}

	flcn->g->ops.falcon.bootstrap(flcn, boot_vector);

	return 0;
}

int nvgpu_falcon_clear_halt_intr_status(struct nvgpu_falcon *flcn,
	unsigned int timeout)
{
	struct nvgpu_timeout to;
	struct gk20a *g;
	int status = 0;

	if (!is_falcon_valid(flcn)) {
		return -EINVAL;
	}

	g = flcn->g;

	nvgpu_timeout_init_cpu_timer(g, &to, timeout);

	do {
		if (g->ops.falcon.clear_halt_interrupt_status(flcn)) {
			break;
		}

		nvgpu_udelay(1);
	} while (nvgpu_timeout_expired(&to) == 0);

	if (nvgpu_timeout_peek_expired(&to)) {
		status = -ETIMEDOUT;
	}

	return status;
}

int nvgpu_falcon_copy_from_imem(struct nvgpu_falcon *flcn,
	u32 src, u8 *dst, u32 size, u8 port)
{
	int status = -EINVAL;
	struct gk20a *g;

	if (!is_falcon_valid(flcn)) {
		return -EINVAL;
	}

	g = flcn->g;

	if (falcon_memcpy_params_check(flcn, src, size, MEM_IMEM, port) != 0) {
		nvgpu_err(g, "incorrect parameters");
		goto exit;
	}

	nvgpu_mutex_acquire(&flcn->imem_lock);
	status = g->ops.falcon.copy_from_imem(flcn, src, dst, size, port);
	nvgpu_mutex_release(&flcn->imem_lock);

exit:
	return status;
}

static void falcon_print_mem(struct nvgpu_falcon *flcn, u32 src,
	u32 size, enum falcon_mem_type mem_type)
{
	u32 buff[64] = {0};
	u32 total_block_read = 0;
	u32 byte_read_count = 0;
	struct gk20a *g;
	u32 i = 0;
	int status = 0;

	g = flcn->g;

	if (falcon_memcpy_params_check(flcn, src, size, mem_type, 0) != 0) {
		nvgpu_err(g, "incorrect parameters");
		return;
	}

	nvgpu_falcon_dbg(g, " offset 0x%x  size %d bytes", src, size);

	total_block_read = size >> 8;
	do {
		byte_read_count =
			(total_block_read != 0U) ? (u32)sizeof(buff) : size;

		if (byte_read_count == 0U) {
			break;
		}

		if (mem_type == MEM_DMEM) {
			status = nvgpu_falcon_copy_from_dmem(flcn, src,
				(u8 *)buff, byte_read_count, 0);
		} else {
			status = nvgpu_falcon_copy_from_imem(flcn, src,
				(u8 *)buff, byte_read_count, 0);
		}

		if (status != 0) {
			nvgpu_err(g, "MEM print failed");
			break;
		}

		for (i = 0U; i < (byte_read_count >> 2U); i += 4U) {
			nvgpu_falcon_dbg(g, "0x%04x: 0x%08x 0x%08x 0x%08x 0x%08x",
				src + (i << 2U), buff[i], buff[i+1U],
				buff[i+2U], buff[i+3U]);
		}

		src += byte_read_count;
		size -= byte_read_count;
	} while (total_block_read-- != 0U);
}

void nvgpu_falcon_print_dmem(struct nvgpu_falcon *flcn, u32 src, u32 size)
{
	if (!is_falcon_valid(flcn)) {
		return;
	}

	nvgpu_falcon_dbg(flcn->g, " PRINT DMEM ");
	falcon_print_mem(flcn, src, size, MEM_DMEM);
}

void nvgpu_falcon_print_imem(struct nvgpu_falcon *flcn, u32 src, u32 size)
{
	if (!is_falcon_valid(flcn)) {
		return;
	}

	nvgpu_falcon_dbg(flcn->g, " PRINT IMEM ");
	falcon_print_mem(flcn, src, size, MEM_IMEM);
}

void nvgpu_falcon_get_ctls(struct nvgpu_falcon *flcn, u32 *sctl, u32 *cpuctl)
{
	if (!is_falcon_valid(flcn)) {
		return;
	}

	flcn->g->ops.falcon.get_falcon_ctls(flcn, sctl, cpuctl);
}
#endif
