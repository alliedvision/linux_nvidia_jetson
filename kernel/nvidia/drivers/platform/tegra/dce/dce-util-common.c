/*
 * Copyright (c) 2019-2023, NVIDIA CORPORATION & AFFILIATES.  All rights reserved.
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
#include <dce.h>
#include <dce-util-common.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/firmware.h>

/**
 * dce_writel - Dce io function to perform MMIO writes
 *
 * @d : Pointer to tegra_dce struct.
 * @r : register offset from dce_base.
 * @v : value to be written
 *
 * Return : Void
 */
void dce_writel(struct tegra_dce *d, u32 r, u32 v)
{
	struct dce_device *d_dev = dce_device_from_dce(d);

	if (unlikely(!d_dev->regs))
		dce_err(d, "DCE Register Space not IOMAPed to CPU");
	else
		writel(v, d_dev->regs + r);
}

/**
 * dce_readl - Dce io function to perform MMIO reads
 *
 * @d : Pointer to tegra_dce struct.
 * @r : register offset from dce_base.
 *
 * Return : the read value
 */
u32 dce_readl(struct tegra_dce *d, u32 r)
{
	u32 v = 0xffffffff;

	struct dce_device *d_dev = dce_device_from_dce(d);

	if (unlikely(!d_dev->regs))
		dce_err(d, "DCE Register Space not IOMAPed to CPU");
	else
		v = readl(d_dev->regs + r);
	/*TODO : Add error check here */
	return v;
}

/**
 * dce_writel_check - Performs MMIO writes and checks if the writes
 *			are actaully correct.
 *
 * @d : Pointer to tegra_dce struct.
 * @r : register offset from dce_base.
 * @v : value to be written
 *
 * Return : Void
 */
void dce_writel_check(struct tegra_dce *d, u32 r, u32 v)
{
	/* TODO : Write and read back to check */
}

/**
 * dce_io_exists - Dce io function to check if the registers are mapped
 *			to CPU correctly
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : True if mapped.
 */
bool dce_io_exists(struct tegra_dce *d)
{
	struct dce_device *d_dev = dce_device_from_dce(d);

	return d_dev->regs != NULL;
}

/**
 * dce_io_valid_regs - Dce io function to check if the requested offset is
 *			within the range of CPU mapped MMIO range.
 *
 * @d : Pointer to tegra_dce struct.
 * @r : register offset from dce_base.
 *
 * Return : True if offset within range.
 */
bool dce_io_valid_reg(struct tegra_dce *d, u32 r)
{
	/* TODO : Implement range check here. Returning true for now*/
	return true;
}

/**
 * dce_kzalloc - Function to allocate contiguous kernel memory
 *
 * @d : Pointer to tegra_dce struct.
 * @size_t : Size of the memory to be allocated
 * @dma_flag: True if allocated memory should be DMAable
 *
 * Return : CPU Mapped Address if successful else NULL.
 */
void *dce_kzalloc(struct tegra_dce *d, size_t size, bool dma_flag)
{
	void *alloc;
	u32 flags = GFP_KERNEL;

	if (dma_flag)
		flags |= __GFP_DMA;

	alloc = kzalloc(size, flags);

	return alloc;
}

/**
 * dce_kfree - Frees an alloc from dce_kzalloc
 *
 * @d : Pointer to tegra_dce struct.
 * @addr : Address of the object to free.
 *
 * Return : void
 */
void dce_kfree(struct tegra_dce *d, void *addr)
{
	kfree(addr);
}

/**
 * dce_request_firmware - Reads the fw into memory.
 *
 * @d : Pointer to tegra_dce struct.
 * @fw_name : Name of the fw.
 *
 * Return : Pointer to dce_firmware if successful else NULL.
 */
struct dce_firmware *dce_request_firmware(struct tegra_dce *d,
					const char *fw_name)
{
	struct device *dev = dev_from_dce(d);
	struct dce_firmware *fw;
	const struct firmware *l_fw;

	fw = dce_kzalloc(d, sizeof(*fw), false);
	if (!fw)
		return NULL;

	if (request_firmware(&l_fw, fw_name, dev) < 0) {
		dce_err(d, "FW Request Failed");
		goto err;
	}

	if (!l_fw)
		goto err;

	/* Make sure the address is aligned to 4K */
	fw->size = l_fw->size;

	fw->size = ALIGN(fw->size + SZ_4K, SZ_4K);
	/**
	 * BUG : Currently overwriting all alignment logic above to blinldy
	 * allocate 2MB FW virtual space. Ideally it should be as per the
	 * actual size of the fw.
	 */
	fw->size = SZ_32M;

	fw->data = dma_alloc_coherent(dev, fw->size,
				      (dma_addr_t *)&fw->dma_handle,
				      GFP_KERNEL);
	if (!fw->data)
		goto err_release;

	memcpy((u8 *)fw->data, (u8 *)l_fw->data, l_fw->size);

	release_firmware(l_fw);

	return fw;

err_release:
	release_firmware(l_fw);
err:
	dce_kfree(d, fw);
	return NULL;
}

/**
 * dce_release_firmware - Reads the fw into memory.
 *
 * @d : Pointer to tegra_dce struct.
 * @fw : Pointer to dce_firmware.
 *
 * Return : void
 */
void dce_release_fw(struct tegra_dce *d, struct dce_firmware *fw)
{
	struct device *dev = dev_from_dce(d);

	if (!fw)
		return;

	dma_free_coherent(dev, fw->size,
			(void *)fw->data,
			(dma_addr_t)fw->dma_handle);

	dce_kfree(d, fw);
}

/**
 * dce_get_phys_stream_id - Gets the physical stream ID to be programmed from
 * platform data.
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : Stream ID Value
 */
u8 dce_get_phys_stream_id(struct tegra_dce *d)
{
	return pdata_from_dce(d)->phys_stream_id;
}

/**
 * dce_get_dce_stream_id - Gets the dce stream ID to be programmed from
 * platform data.
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : Stream ID Value
 */
u8 dce_get_dce_stream_id(struct tegra_dce *d)
{
	return pdata_from_dce(d)->stream_id;
}

/**
 * dce_get_fw_vm_index - Gets the VMIndex for the fw region to be
 * programmed from platform data.
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : VMIndex
 */
u8 dce_get_fw_vm_index(struct tegra_dce *d)
{
	return pdata_from_dce(d)->fw_vmindex;
}

/**
 * dce_get_fw_carveout_id- Gets the carveout ID for the fw region to be
 * programmed from platform data.
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : Carveout Id
 */
u8 dce_get_fw_carveout_id(struct tegra_dce *d)
{
	return pdata_from_dce(d)->fw_carveout_id;
}

/**
 * dce_is_physical_id_valid - Checks if the DCE can use physical stream ID.
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : True if SMMU is disabled.
 */
bool dce_is_physical_id_valid(struct tegra_dce *d)
{
	return pdata_from_dce(d)->use_physical_id;
}

/**
 * dce_get_fw_dce_addr - Gets the 32bit address to be used for
 *				loading	the fw before being converted
 *				by AST into a 40-bit address.
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : 32bit address
 */
u32 dce_get_fw_dce_addr(struct tegra_dce *d)
{
	return pdata_from_dce(d)->fw_dce_addr;
}

/**
 * dce_get_fw_phy_addr - Gets the 40bit address to be used by AST
 *				for loading the fw after converting
 *				the 32bit incoming address.
 *
 * @d : Pointer to tegra_dce struct.
 *
 * This API is to be used only if the memory is being allocated
 * via kzalloc or friends. Do not use this if memory is
 * allocated via dma apis.
 *
 * Return : 64bit address
 */
u64 dce_get_fw_phy_addr(struct tegra_dce *d, struct dce_firmware *fw)
{
	/* Caller should make sure that *fw is valid since this func is
	 * not expected to return any error.
	 */
	return (u64)virt_to_phys((void *)fw->data);
}

/**
 * dce_get_fw_name - Gets the dce fw name from platform data.
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : fw_name
 */
const char *dce_get_fw_name(struct tegra_dce *d)
{
	return pdata_from_dce(d)->fw_name;
}

static void dce_print(const char *func_name, int line,
			enum dce_log_type type, const char *log)
{
#define DCE_LOG_FMT	"dce: %15s:%-4d %s\n"

	switch (type) {
	case DCE_DEBUG:
		pr_debug(DCE_LOG_FMT, func_name, line, log);
		break;
	case DCE_INFO:
		pr_info(DCE_LOG_FMT, func_name, line, log);
		break;
	case DCE_WARNING:
		pr_warn(DCE_LOG_FMT, func_name, line, log);
		break;
	case DCE_ERROR:
		pr_err(DCE_LOG_FMT, func_name, line, log);
		break;
	}
#undef DCE_LOG_FMT
}

__printf(5, 6)
void dce_log_msg(struct tegra_dce *d, const char *func_name, int line,
			enum dce_log_type type, const char *fmt, ...)
{

#define BUF_LEN 100

	char log[BUF_LEN];
	va_list args;

	va_start(args, fmt);
	(void) vsnprintf(log, BUF_LEN, fmt, args);
	va_end(args);

	dce_print(func_name, line, type, log);
}

/**
 * dce_cond_init - Initialize a condition variable
 *
 * @cond - The condition variable to initialize
 *
 * Initialize a condition variable before using it.
 */
int dce_cond_init(struct dce_cond *cond)
{
	init_waitqueue_head(&cond->wq);
	cond->initialized = true;

	return 0;
}

/**
 * dce_cond_destroy - Destroy a condition variable
 *
 * @cond - The condition variable to destroy
 */
void dce_cond_destroy(struct dce_cond *cond)
{
	cond->initialized = false;
}

/**
 * dce_cond_signal - Signal a condition variable
 *
 * @cond - The condition variable to signal
 *
 * Wake up a waiter for a condition variable to check if its condition has been
 * satisfied.
 *
 * The waiter is using an uninterruptible wait.
 */
void dce_cond_signal(struct dce_cond *cond)
{
	WARN_ON(!cond->initialized);

	wake_up(&cond->wq);
}

/**
 * dce_cond_signal_interruptible - Signal a condition variable
 *
 * @cond - The condition variable to signal
 *
 * Wake up a waiter for a condition variable to check if its condition has been
 * satisfied.
 *
 * The waiter is using an interruptible wait.
 */
void dce_cond_signal_interruptible(struct dce_cond *cond)
{
	WARN_ON(!cond->initialized);

	wake_up_interruptible(&cond->wq);
}

/**
 * dce_cond_broadcast - Signal all waiters of a condition variable
 *
 * @cond - The condition variable to signal
 *
 * Wake up all waiters for a condition variable to check if their conditions
 * have been satisfied.
 *
 * The waiters are using an uninterruptible wait.
 */
int dce_cond_broadcast(struct dce_cond *cond)
{
	if (!cond->initialized)
		return -EINVAL;

	wake_up_all(&cond->wq);

	return 0;
}

/**
 * dce_cond_broadcast_interruptible - Signal all waiters of a condition
 * variable
 *
 * @cond - The condition variable to signal
 *
 * Wake up all waiters for a condition variable to check if their conditions
 * have been satisfied.
 *
 * The waiters are using an interruptible wait.
 */
int dce_cond_broadcast_interruptible(struct dce_cond *cond)
{
	if (!cond->initialized)
		return -EINVAL;

	wake_up_interruptible_all(&cond->wq);

	return 0;
}

/**
 * dce_thread_proxy - Function to be passed to kthread.
 *
 * @thread_data : Pointer to actual dce_thread struct
 *
 * Return  : Ruturns the return value of the function to be run.
 */
static int dce_thread_proxy(void *thread_data)
{
	struct dce_thread *thread = thread_data;
	int ret = thread->fn(thread->data);

	thread->running = false;
	return ret;
}

/**
 * dce_thread_create - Create and run a new thread.
 *
 * @thread - thread structure to use
 * @data - data to pass to threadfn
 * @threadfn - Thread function
 * @name - name of the thread
 *
 * Create a thread and run threadfn in it. The thread stays alive as long as
 * threadfn is running. As soon as threadfn returns the thread is destroyed.
 *
 * threadfn needs to continuously poll dce_thread_should_stop() to determine
 * if it should exit.
 */
int dce_thread_create(struct dce_thread *thread,
		void *data,
		int (*threadfn)(void *data), const char *name)
{
	struct task_struct *task = kthread_create(dce_thread_proxy,
			thread, name);
	if (IS_ERR(task))
		return PTR_ERR(task);

	thread->task = task;
	thread->fn = threadfn;
	thread->data = data;
	thread->running = true;
	wake_up_process(task);
	return 0;
};

/**
 * dce_thread_stop - Destroy or request to destroy a thread
 *
 * @thread - thread to stop
 *
 * Request a thread to stop by setting dce_thread_should_stop() to
 * true and wait for thread to exit.
 */
void dce_thread_stop(struct dce_thread *thread)
{
	/*
	 * Threads waiting on wq's should have dce_thread_should_stop()
	 * as one of its wakeup condition. This allows the thread to be woken
	 * up when kthread_stop() is invoked and does not require an additional
	 * callback to wakeup the sleeping thread.
	 */
	if (thread->task) {
		kthread_stop(thread->task);
		thread->task = NULL;
	}
};

/**
 * dce_thread_should_stop - Query if thread should stop
 *
 * @thread
 *
 * Return true if thread should exit. Can be run only in the thread's own
 * context and with the thread as parameter.
 */
bool dce_thread_should_stop(struct dce_thread *thread)
{
	return kthread_should_stop();
};

/**
 * dce_thread_is_running - Query if thread is running
 *
 * @thread
 *
 * Return true if thread is started.
 */
bool dce_thread_is_running(struct dce_thread *thread)
{
	return READ_ONCE(thread->running);
};

/**
 * dce_thread_join - join a thread to reclaim resources
 * after it has exited
 *
 * @thread - thread to join
 *
 */
void dce_thread_join(struct dce_thread *thread)
{
	while (READ_ONCE(thread->running))
		usleep_range(10000, 20000);
};

/**
 * dce_get_nxt_pow_of_2 : get next power of 2 number for a given number
 *
 * @addr : Address of given number
 * @nbits : bits in given number
 *
 * Return : unsigned long next power of 2 value
 */
unsigned long dce_get_nxt_pow_of_2(unsigned long *addr, u8 nbits)
{
	u8 l_bit = 0;
	u8 bit_index = 0;
	unsigned long val;

	val = *addr;
	if (val == 0)
		return 0;

	bit_index = find_first_bit(addr, nbits);

	while (bit_index && (bit_index < nbits)) {
		l_bit = bit_index;
		bit_index = find_next_bit(addr, nbits, bit_index + 1);
	}

	if (BIT(l_bit) < val) {
		l_bit += 1UL;
		val = BIT(l_bit);
	}

	return val;
}

/*
 * dce_schedule_work : schedule work in global highpri workqueue
 *
 * @work : dce work to be scheduled
 *
 * Return : void
 */
void dce_schedule_work(struct dce_work_struct *work)
{
	queue_work(system_highpri_wq, &work->work);
}

/*
 * dce_work_handle_fn : handler function for scheduled dce-work
 *
 * @work : Pointer to the scheduled work
 *
 * Return : void
 */
void dce_work_handle_fn(struct work_struct *work)
{
	struct dce_work_struct *dce_work = container_of(work,
							struct dce_work_struct,
							work);

	if (dce_work->dce_work_fn != NULL)
		dce_work->dce_work_fn(dce_work->d);
}

/*
 * dce_init_work : Init dce work structure
 *
 * @d : Pointer to tegra_dce struct.
 * @work : Pointer to dce work structure
 * @work_fn : worker function to be called
 *
 * Return : 0 if successful
 */
int dce_init_work(struct tegra_dce *d,
		   struct dce_work_struct *work,
		   void (*work_fn)(struct tegra_dce *d))
{
	work->d = d;
	work->dce_work_fn = work_fn;

	INIT_WORK(&work->work, dce_work_handle_fn);

	return 0;
}
