/*
 * Copyright (C) 2014-2022, NVIDIA CORPORATION. All rights reserved.
 *
 * Hypervisor interfaces
 *
 * This header is BSD licensed so anyone can use the definitions to implement
 * compatible drivers/servers.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of NVIDIA CORPORATION nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NVIDIA CORPORATION OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __VMM_SYSCALLS_H__
#define __VMM_SYSCALLS_H__

#include <soc/tegra/virt/tegra_hv_sysmgr.h>

#define HVC_NR_READ_STAT		1
#define HVC_NR_READ_IVC			2
#define HVC_NR_READ_GID			3
#define HVC_NR_RAISE_IRQ		4
#define HVC_NR_READ_NGUESTS		5
#define HVC_NR_READ_IPA_PA		6
#define HVC_NR_READ_GUEST_STATE		7
#define HVC_NR_READ_HYP_INFO		9
#define HVC_NR_GUEST_RESET		10
#define HVC_NR_SYSINFO_IPA		13
#define HVC_NR_TRACE_GET_EVENT_MASK		0x8003U
#define HVC_NR_TRACE_SET_EVENT_MASK		0x8004U

#define GUEST_PRIMARY		0
#define GUEST_IVC_SERVER	0
#define HVC_NR_CPU_FREQ		0xC6000022

#define NGUESTS_MAX 16

#ifndef __ASSEMBLY__

#if defined(__KERNEL__)
#include <linux/types.h>
#endif

struct tegra_hv_queue_data {
	uint32_t	id;	/* IVC id */
	uint32_t	peers[2];
	uint32_t	size;
	uint32_t	nframes;
	uint32_t	frame_size;
	uint32_t	offset;
	uint16_t	irq, raise_irq;
	uint64_t	trap_ipa; /** @brief IO address used to notify peer endpoint */
	uint64_t	msi_ipa; /** @brief MSI address used to notify peer endpoint */
};

struct ivc_mempool {
	uint64_t pa;
	uint64_t size;
	uint32_t id;
	uint32_t peer_vmid;
};

struct ivc_shared_area {
	uint64_t pa;
	uint64_t size;
	uint32_t guest;
	uint16_t free_irq_start;
	uint16_t free_irq_count;
};

struct ivc_info_page {
	uint32_t nr_queues;
	uint32_t nr_areas;
	uint32_t nr_mempools;
	uint32_t padding; /**< @brief reserved for internal use */
			// IMPORTANT: Padding is needed to align
			// sizeof(struct ivc_info_page ) to 64 bits
	uint64_t trap_region_base_ipa; /**< @brief MMIO trap region start address */
	uint64_t trap_region_size; /**< @brief MMIO trap region size */
	uint64_t trap_ipa_stride; /**< @brief MMIO trap IPA stride size */
	uint64_t msi_region_base_ipa; /**< @brief MMIO msi region start address */
	uint64_t msi_region_size; /**< @brief MMIO msi region size */
	uint64_t msi_ipa_stride; /**< @brief MMIO msi IPA stride size */

	/* The actual length of this array is nr_areas. */
	struct ivc_shared_area areas[];

	/*
	 * Following the shared array is an array of queue data structures with
	 * an entry per queue that is assigned to the guest. This  array is
	 * terminated by an entry with no frames.
	 *
	 * struct tegra_hv_queue_data queue_data[nr_queues];
	 */

	/*
	 * Following the queue data array is an array of mempool structures
	 * with an entry per mempool assigned to the guest.
	 *
	 * struct ivc_mempool[nr_mempools];
	 */
};

static inline struct ivc_shared_area *ivc_shared_area_addr(
		const struct ivc_info_page *info, uint32_t area_num)
{
	return ((struct ivc_shared_area *) (((uintptr_t) info) + sizeof(*info)))
		+ area_num;
}

static inline const struct tegra_hv_queue_data *ivc_info_queue_array(
		const struct ivc_info_page *info)
{
	return (struct tegra_hv_queue_data *)&info->areas[info->nr_areas];
}

static inline const struct ivc_mempool *ivc_info_mempool_array(
		const struct ivc_info_page *info)
{
	return (struct ivc_mempool *)
			&ivc_info_queue_array(info)[info->nr_queues];
}

struct hyp_ipa_pa_info {
	uint64_t base;       /* base of contiguous pa region */
	uint64_t offset;     /* offset for requested ipa address */
	uint64_t size;       /* size of pa region */
};

#define HVC_MAX_VCPU 64

struct trapped_access {
	uint64_t ipa;
	uint32_t size;
	int32_t write_not_read;
	uint64_t data;
	uint32_t guest_id;
};

struct hyp_server_page {
	/* guest reset protocol */
	uint32_t guest_reset_virq;

	/* boot delay offsets per VM needed by monitor partition */
	uint32_t boot_delay[NGUESTS_MAX];

	/* hypervisor trace log */
	uint64_t log_ipa;
	uint32_t log_size;

	/* secure-hypervisor trace log */
	uint64_t secure_log_ipa;
	uint32_t secure_log_size;

	/* PCT data */
	uint64_t pct_ipa;
	uint64_t pct_size;

	/* check if the VM is a server or a guest */
	uint32_t is_server_vm;

	/* golden register data */
	uint64_t gr_ipa;
	uint32_t gr_size;

	/* all vm mappings ipa */
	uint64_t mappings_ipa;
};

/* For backwards compatibility, alias the old name for hyp_server_name. */
#define hyp_info_page hyp_server_page

#ifdef CONFIG_ARM64

#define _X3_X17 "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", \
"x13", "x14", "x15", "x16", "x17"

#define _X4_X17 "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", \
"x13", "x14", "x15", "x16", "x17"

static inline int hyp_read_gid(unsigned int *gid)
{
	register uint64_t r0 asm("x0");
	register uint64_t r1 asm("x1");

	asm("hvc %2"
		: "=r"(r0), "=r"(r1)
		: "i"(HVC_NR_READ_GID)
		: "x2", "x3", _X4_X17);

	*gid = r1;
	return (int)r0;
}

static inline int hyp_read_nguests(unsigned int *nguests)
{
	register uint64_t r0 asm("x0");
	register uint64_t r1 asm("x1");

	asm("hvc %2"
		: "=r"(r0), "=r"(r1)
		: "i"(HVC_NR_READ_NGUESTS)
		: "x2", "x3", _X4_X17);

	*nguests = r1;
	return (int)r0;
}

static inline int hyp_read_ivc_info(uint64_t *ivc_info_page_pa)
{
	register uint64_t r0 asm("x0");
	register uint64_t r1 asm("x1");

	asm("hvc %2"
		: "=r"(r0), "=r"(r1)
		: "i"(HVC_NR_READ_IVC)
		: "x2", "x3", _X4_X17);

	*ivc_info_page_pa = r1;
	return (int)r0;
}

static inline int hyp_read_ipa_pa_info(struct hyp_ipa_pa_info *info,
		unsigned int guestid, uint64_t ipa)
{
	register uint64_t r0 asm("x0") = guestid;
	register uint64_t r1 asm("x1") = ipa;
	register uint64_t r2 asm("x2");
	register uint64_t r3 asm("x3");


	asm("hvc %4"
		: "+r"(r0), "+r"(r1), "=r"(r2), "=r"(r3)
		: "i"(HVC_NR_READ_IPA_PA)
		: _X4_X17);

	info->base = r1;
	info->offset = r2;
	info->size = r3;

	return (int)r0;
}

static inline int hyp_raise_irq(unsigned int irq, unsigned int vmid)
{
	register uint64_t r0 asm("x0") = irq;
	register uint64_t r1 asm("x1") = vmid;

	asm volatile("hvc %1"
		: "+r"(r0)
		: "i"(HVC_NR_RAISE_IRQ), "r"(r1)
		: "x2", "x3", _X4_X17);

	return (int)r0;
}

static inline int hyp_read_guest_state(unsigned int vmid, unsigned int *state)
{
	register uint64_t r0 asm("x0") = vmid;
	register uint64_t r1 asm("x1");

	asm("hvc %2"
		: "+r"(r0), "=r"(r1)
		: "i"(HVC_NR_READ_GUEST_STATE)
		: "x2", _X3_X17);

	*state = (unsigned int)r1;
	return (int)r0;
}

static inline int hyp_read_hyp_info(uint64_t *hyp_info_page_pa)
{
	register uint64_t r0 asm("x0");
	register uint64_t r1 asm("x1");

	asm("hvc %2"
		: "=r"(r0), "=r"(r1)
		: "i"(HVC_NR_READ_HYP_INFO)
		: "x2", "x3", _X4_X17);

	*hyp_info_page_pa = r1;
	return (int)r0;
}

static inline int hyp_guest_reset(unsigned int id,
				  struct hyp_sys_state_info *out)
{
	register uint64_t r0 asm("x0") = id;
	register uint64_t r1 asm("x1");
	register uint64_t r2 asm("x2");
	register uint64_t r3 asm("x3");

	asm volatile("hvc %4"
		: "+r"(r0), "=r"(r1),
		  "=r"(r2), "=r"(r3)
		: "i"(HVC_NR_GUEST_RESET)
		: _X4_X17);

	if (out != NULL) {
		out->sys_transition_mask = (uint32_t)r1;
		out->vm_shutdown_mask = (uint32_t)r2;
		out->vm_reboot_mask = (uint32_t)r3;
	}

	return (int)r0;
}

static inline uint64_t hyp_sysinfo_ipa(void)
{
	register uint64_t r0 asm("x0");

	asm("hvc %1"
		: "=r"(r0)
		: "i"(HVC_NR_SYSINFO_IPA)
		: "x1", "x2", "x3", _X4_X17);

	return r0;
}

static inline int hyp_read_freq_feedback(uint64_t *value)
{
	register uint64_t r0 asm("x0") = HVC_NR_CPU_FREQ;
	register uint64_t r1 asm("x1") = 1U;

	asm volatile("hvc #0"
		: "+r"(r0), "+r"(r1)
		:
		: "x2", "x3", _X4_X17);

	if (r0 == 1 &&  value != NULL)
		*value = r1;

	return (int16_t)r0;
}

static inline int hyp_read_freq_request(uint64_t *value)
{
	register uint64_t r0 asm("x0") = HVC_NR_CPU_FREQ;
	register uint64_t r1 asm("x1") = 0U;

	asm volatile("hvc #0"
		: "+r"(r0), "+r"(r1)
		:
		: "x2", "x3", _X4_X17);

	if (r0 == 1 &&  value != 0)
		*value = r1;

	return (int16_t)r0;
}

static inline int hyp_write_freq_request(uint64_t value)
{
	register uint64_t r0 asm("x0") = HVC_NR_CPU_FREQ;
	register uint64_t r1 asm("x1") = 2U;
	register uint64_t r2 asm("x2") = value;

	asm volatile("hvc #0"
		: "+r"(r0)
		: "r"(r1), "r"(r2)
		: "x3", _X4_X17);

	return (int16_t)r0;
}

static inline int hyp_pct_cpu_id_read_freq_feedback(uint8_t cpu_id,
							uint64_t *value)
{
	register uint64_t r0 asm("x0") = HVC_NR_CPU_FREQ;
	register uint64_t r1 asm("x1") = 4U;
	register uint64_t r2 asm("x2") = cpu_id;

	asm volatile("hvc #0"
		: "+r"(r0), "+r"(r1)
		: "r"(r2)
		: "x3", _X4_X17);

	if (r0 == 1 &&  value != 0)
		*value = r1;

	return (int16_t)r0;

}

static inline int hyp_pct_cpu_id_read_freq_request(uint8_t cpu_id,
							uint64_t *value)
{
	register uint64_t r0 asm("x0") = HVC_NR_CPU_FREQ;
	register uint64_t r1 asm("x1") = 3U;
	register uint64_t r2 asm("x2") = cpu_id;

	asm volatile("hvc #0"
		: "+r"(r0), "+r"(r1)
		: "r"(r2)
		: "x3", _X4_X17);

	if (r0 == 1 &&  value != 0)
		*value = r1;

	return (int16_t)r0;
}

static inline int hyp_pct_cpu_id_write_freq_request(uint8_t cpu_id,
							uint64_t value)
{
	register uint64_t r0 asm("x0") = HVC_NR_CPU_FREQ;
	register uint64_t r1 asm("x1") = 5U;
	register uint64_t r2 asm("x2") = value;
	register uint64_t r3 asm("x3") = cpu_id;

	asm volatile("hvc #0"
		: "+r"(r0)
		: "r"(r1), "r"(r2), "r"(r3)
		: _X4_X17);

	return (int16_t)r0;
}

static inline uint8_t hyp_get_cpu_count(void)
{
	register uint64_t r0 asm("x0") = HVC_NR_CPU_FREQ;
	register uint64_t r1 asm("x1") = 6U;

	asm volatile("hvc #0"
		: "+r"(r0), "+r"(r1)
		:
		: "x2", "x3", _X4_X17);

	if (r0 == 1)
		return r1;

	return 0;
}

static __attribute__((always_inline)) inline void hyp_call44(uint16_t id,
			uint64_t args[4])
{
		register uint64_t x0 asm("x0") = args[0];
		register uint64_t x1 asm("x1") = args[1];
		register uint64_t x2 asm("x2") = args[2];
		register uint64_t x3 asm("x3") = args[3];

		asm volatile("HVC %[imm16]"
			: "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3)
			:
			[imm16] "i"(((uint32_t)id)));

		args[0] = x0;
		args[1] = x1;
		args[2] = x2;
		args[3] = x3;
}

static inline int hyp_trace_get_mask(uint64_t *value)
{
	uint64_t args[4] = { 0U, 0U, 0U, 0U };

	hyp_call44(HVC_NR_TRACE_GET_EVENT_MASK, args);
	if (args[0] == 0U)
		*value = args[1];

	return (int) args[0];
}

static inline int hyp_trace_set_mask(uint64_t mask)
{
	uint64_t args[4] = { mask, 0U, 0U, 0U };

	hyp_call44(HVC_NR_TRACE_SET_EVENT_MASK, args);
	return (int) args[0];
}

#undef _X3_X17
#undef _X4_X17

#else

int hyp_read_gid(unsigned int *gid);
int hyp_read_nguests(unsigned int *nguests);
int hyp_read_ivc_info(uint64_t *ivc_info_page_pa);
int hyp_read_ipa_pa_info(struct hyp_ipa_pa_info *info, int guestid,
		uint64_t ipa);
int hyp_raise_irq(unsigned int irq, unsigned int vmid);
uint64_t hyp_sysinfo_ipa(void);

/* ASM prototypes */
extern int hvc_read_gid(void *);
extern int hvc_read_ivc_info(int *);
extern int hvc_read_ipa_pa_info(void *, int guestid, uint64_t ipa);
extern int hvc_read_nguests(void *);
extern int hvc_raise_irq(unsigned int irq, unsigned int vmid);

#endif /* CONFIG_ARCH_ARM64 */

#endif /* !__ASSEMBLY__ */

#endif /* __VMM_SYSCALLS_H__ */
