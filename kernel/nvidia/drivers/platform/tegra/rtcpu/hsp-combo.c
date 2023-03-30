/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION. All rights reserved.
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

#include "hsp-combo.h"

#include <linux/version.h>

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/tegra-hsp.h>
#include <linux/sched.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
#include <linux/sched/clock.h>
#endif

#include "soc/tegra/camrtc-commands.h"

struct camrtc_hsp_op;

struct camrtc_hsp {
	const struct camrtc_hsp_op *op;
	struct tegra_hsp_sm_rx *rx;
	struct tegra_hsp_sm_tx *tx;
	u32 cookie;
	spinlock_t sendlock;
	struct tegra_hsp_ss *ss;
	void (*group_notify)(struct device *dev, u16 group);
	struct device dev;
	struct mutex mutex;
	struct completion emptied;
	wait_queue_head_t response_waitq;
	atomic_t response;
	long timeout;
};

struct camrtc_hsp_op {
	int (*send)(struct camrtc_hsp *, int msg, long *timeout);
	void (*group_ring)(struct camrtc_hsp *, u16 group);
	int (*sync)(struct camrtc_hsp *, long *timeout);
	int (*resume)(struct camrtc_hsp *, long *timeout);
	int (*suspend)(struct camrtc_hsp *, long *timeout);
	int (*bye)(struct camrtc_hsp *, long *timeout);
	int (*ch_setup)(struct camrtc_hsp *, dma_addr_t iova, long *timeout);
	int (*ping)(struct camrtc_hsp *, u32 data, long *timeout);
	int (*get_fw_hash)(struct camrtc_hsp *, u32 index, long *timeout);
};

static int camrtc_hsp_send(struct camrtc_hsp *camhsp,
		int request, long *timeout)
{
	int ret = camhsp->op->send(camhsp, request, timeout);

	if (ret == -ETIMEDOUT)
		dev_err(&camhsp->dev,
			"request 0x%08x: empty mailbox timeout\n", request);

	return ret;
}

static int camrtc_hsp_recv(struct camrtc_hsp *camhsp,
		int command, long *timeout)
{
	int response;

	*timeout = wait_event_timeout(
		camhsp->response_waitq,
		(response = atomic_xchg(&camhsp->response, -1)) >= 0,
		*timeout);
	if (*timeout <= 0) {
		dev_err(&camhsp->dev,
			"request 0x%08x: response timeout\n", command);
		return -ETIMEDOUT;
	}

	dev_dbg(&camhsp->dev, "request 0x%08x: response 0x%08x\n",
		command, response);

	return response;
}

static int camrtc_hsp_sendrecv(struct camrtc_hsp *camhsp,
		int command, long *timeout)
{
	int ret = camrtc_hsp_send(camhsp, command, timeout);

	if (ret == 0)
		ret = camrtc_hsp_recv(camhsp, command, timeout);

	return ret;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
static struct device_node *of_get_compatible_child(
	const struct device_node *parent,
	const char *compatible)
{
	struct device_node *child;

	for_each_child_of_node(parent, child) {
		if (of_device_is_compatible(child, compatible))
			break;
	}

	return child;
}
#endif

/* ---------------------------------------------------------------------- */
/* Protocol nvidia,tegra-camrtc-hsp-vm */

static void camrtc_hsp_rx_full_notify(void *data, u32 msg)
{
	struct camrtc_hsp *camhsp = data;
	u32 status, group;

	if (!IS_ERR_OR_NULL(camhsp->ss)) {
		status = tegra_hsp_ss_status(camhsp->ss);
		dev_dbg(&camhsp->dev, "notify sm=0x%08x ss=0x%04x\n", msg, status);
		status &= CAMRTC_HSP_SS_FW_MASK;
		tegra_hsp_ss_clr(camhsp->ss, status);
	} else {
		/* Notify all groups */
		status = CAMRTC_HSP_SS_FW_MASK;
	}

	status >>= CAMRTC_HSP_SS_FW_SHIFT;
	group = status & CAMRTC_HSP_SS_IVC_MASK;

	if (group != 0)
		camhsp->group_notify(camhsp->dev.parent, (u16)group);

	/* Other interrupt bits are ignored for now */

	if (CAMRTC_HSP_MSG_ID(msg) == CAMRTC_HSP_IRQ) {
		/* We are done here */
	} else if (CAMRTC_HSP_MSG_ID(msg) < CAMRTC_HSP_HELLO) {
		/* Rest of the unidirectional messages are now ignored */
		dev_info(&camhsp->dev, "unknown message 0x%08x\n", msg);
	} else {
		atomic_set(&camhsp->response, msg);
		wake_up(&camhsp->response_waitq);
	}
}

static void camrtc_hsp_tx_empty_notify(void *data, u32 empty_value)
{
	struct camrtc_hsp *camhsp = data;

	(void)empty_value;	/* ignored */

	complete(&camhsp->emptied);
}

static int camrtc_hsp_vm_send(struct camrtc_hsp *camhsp,
		int request, long *timeout);
static void camrtc_hsp_vm_group_ring(struct camrtc_hsp *camhsp, u16 group);
static void camrtc_hsp_vm_send_irqmsg(struct camrtc_hsp *camhsp);
static int camrtc_hsp_vm_sync(struct camrtc_hsp *camhsp, long *timeout);
static int camrtc_hsp_vm_hello(struct camrtc_hsp *camhsp, long *timeout);
static int camrtc_hsp_vm_protocol(struct camrtc_hsp *camhsp, long *timeout);
static int camrtc_hsp_vm_resume(struct camrtc_hsp *camhsp, long *timeout);
static int camrtc_hsp_vm_suspend(struct camrtc_hsp *camhsp, long *timeout);
static int camrtc_hsp_vm_bye(struct camrtc_hsp *camhsp, long *timeout);
static int camrtc_hsp_vm_ch_setup(struct camrtc_hsp *camhsp,
		dma_addr_t iova, long *timeout);
static int camrtc_hsp_vm_ping(struct camrtc_hsp *camhsp,
		u32 data, long *timeout);
static int camrtc_hsp_vm_get_fw_hash(struct camrtc_hsp *camhsp,
		u32 index, long *timeout);

static const struct camrtc_hsp_op camrtc_hsp_vm_ops = {
	.send = camrtc_hsp_vm_send,
	.group_ring = camrtc_hsp_vm_group_ring,
	.sync = camrtc_hsp_vm_sync,
	.resume = camrtc_hsp_vm_resume,
	.suspend = camrtc_hsp_vm_suspend,
	.bye = camrtc_hsp_vm_bye,
	.ping = camrtc_hsp_vm_ping,
	.ch_setup = camrtc_hsp_vm_ch_setup,
	.get_fw_hash = camrtc_hsp_vm_get_fw_hash,
};

static int camrtc_hsp_vm_send(struct camrtc_hsp *camhsp,
		int request, long *timeout)
{
	for (;;) {
		unsigned long flags;

		spin_lock_irqsave(&camhsp->sendlock, flags);

		if (tegra_hsp_sm_tx_is_empty(camhsp->tx)) {
			atomic_set(&camhsp->response, -1);
			tegra_hsp_sm_tx_write(camhsp->tx, (u32)request);
			spin_unlock_irqrestore(&camhsp->sendlock, flags);

			return 0;
		}

		spin_unlock_irqrestore(&camhsp->sendlock, flags);

		if (*timeout <= 0)
			return -ETIMEDOUT;

		/*
		 * The reinit_completion() resets the completion to 0.
		 *
		 * The tegra_hsp_sm_tx_enable_notify() guarantees that
		 * the empty notify gets called at least once even if the
		 * mailbox was already empty, so no empty events are missed
		 * even if the mailbox gets emptied between the calls to
		 * reinit_completion() and enable_empty_notify().
		 *
		 * The tegra_hsp_sm_tx_enable_notify() may or may not
		 * do reference counting (on APE it does, elsewhere it does
		 * not). If the mailbox is initially empty, the emptied is
		 * already complete()d here, and the code ends up enabling
		 * empty notify twice, and when the mailbox gets empty,
		 * emptied gets complete() twice, and we always run the loop
		 * one extra time.
		 *
		 * Note that the complete() call in empty notifier
		 * callback lets only one waiting task to run. The
		 * mailbox exchange is protected by a mutex, so only
		 * one task can be waiting here.
		 */
		reinit_completion(&camhsp->emptied);

		tegra_hsp_sm_tx_enable_notify(camhsp->tx);

		*timeout = wait_for_completion_timeout(
			&camhsp->emptied, *timeout);
	}
}

static void camrtc_hsp_vm_group_ring(struct camrtc_hsp *camhsp,
		u16 group)
{
	if (!IS_ERR_OR_NULL(camhsp->ss)) {
		u32 status = (u32)(group & CAMRTC_HSP_SS_IVC_MASK);

		status <<= CAMRTC_HSP_SS_VM_SHIFT;

		tegra_hsp_ss_set(camhsp->ss, status);
	}

	camrtc_hsp_vm_send_irqmsg(camhsp);
}

static void camrtc_hsp_vm_send_irqmsg(struct camrtc_hsp *camhsp)
{
	u32 irqmsg = CAMRTC_HSP_MSG(CAMRTC_HSP_IRQ, 1);
	unsigned long flags;

	spin_lock_irqsave(&camhsp->sendlock, flags);

	if (tegra_hsp_sm_tx_is_empty(camhsp->tx))
		tegra_hsp_sm_tx_write(camhsp->tx, irqmsg);

	spin_unlock_irqrestore(&camhsp->sendlock, flags);
}

static int camrtc_hsp_vm_sendrecv(struct camrtc_hsp *camhsp,
		int request, long *timeout)
{
	int response = camrtc_hsp_sendrecv(camhsp, request, timeout);

	if (response < 0)
		return response;

	if (CAMRTC_HSP_MSG_ID(request) != CAMRTC_HSP_MSG_ID(response)) {
		dev_err(&camhsp->dev,
			"request 0x%08x mismatch with response 0x%08x\n",
			request, response);
		return -EIO;
	}

	/* Return the 24-bit parameter only */
	return CAMRTC_HSP_MSG_PARAM(response);
}

static int camrtc_hsp_vm_sync(struct camrtc_hsp *camhsp, long *timeout)
{
	int response = camrtc_hsp_vm_hello(camhsp, timeout);

	if (response >= 0) {
		camhsp->cookie = response;
		response = camrtc_hsp_vm_protocol(camhsp, timeout);
	}

	return response;
}

static u32 camrtc_hsp_vm_cookie(void)
{
	u32 value = CAMRTC_HSP_MSG_PARAM(sched_clock() >> 5U);

	if (value == 0)
		value++;

	return value;
}

static int camrtc_hsp_vm_hello(struct camrtc_hsp *camhsp, long *timeout)
{
	int request = CAMRTC_HSP_MSG(CAMRTC_HSP_HELLO, camrtc_hsp_vm_cookie());
	int response = camrtc_hsp_send(camhsp, request, timeout);

	if (response < 0)
		return response;

	for (;;) {
		response = camrtc_hsp_recv(camhsp, request, timeout);

		/* Wait until we get the HELLO message we sent */
		if (response == request)
			break;

		/* ...or timeout */
		if (response < 0)
			break;
	}

	return response;
}

static int camrtc_hsp_vm_protocol(struct camrtc_hsp *camhsp, long *timeout)
{
	int request = CAMRTC_HSP_MSG(CAMRTC_HSP_PROTOCOL,
			RTCPU_DRIVER_SM6_VERSION);

	return camrtc_hsp_vm_sendrecv(camhsp, request, timeout);
}

static int camrtc_hsp_vm_resume(struct camrtc_hsp *camhsp, long *timeout)
{
	int request = CAMRTC_HSP_MSG(CAMRTC_HSP_RESUME, camhsp->cookie);

	return camrtc_hsp_vm_sendrecv(camhsp, request, timeout);
}

static int camrtc_hsp_vm_suspend(struct camrtc_hsp *camhsp, long *timeout)
{
	u32 request = CAMRTC_HSP_MSG(CAMRTC_HSP_SUSPEND, 0);

	return camrtc_hsp_vm_sendrecv(camhsp, request, timeout);
}

static int camrtc_hsp_vm_bye(struct camrtc_hsp *camhsp, long *timeout)
{
	u32 request = CAMRTC_HSP_MSG(CAMRTC_HSP_BYE, 0);

	camhsp->cookie = 0U;

	return camrtc_hsp_vm_sendrecv(camhsp, request, timeout);
}

static int camrtc_hsp_vm_ch_setup(struct camrtc_hsp *camhsp,
		dma_addr_t iova, long *timeout)
{
	u32 request = CAMRTC_HSP_MSG(CAMRTC_HSP_CH_SETUP, iova >> 8);

	return camrtc_hsp_vm_sendrecv(camhsp, request, timeout);
}

static int camrtc_hsp_vm_ping(struct camrtc_hsp *camhsp, u32 data,
		long *timeout)
{
	u32 request = CAMRTC_HSP_MSG(CAMRTC_HSP_PING, data);

	return camrtc_hsp_vm_sendrecv(camhsp, request, timeout);
}

static int camrtc_hsp_vm_get_fw_hash(struct camrtc_hsp *camhsp, u32 index,
		long *timeout)
{
	u32 request = CAMRTC_HSP_MSG(CAMRTC_HSP_FW_HASH, index);

	return camrtc_hsp_vm_sendrecv(camhsp, request, timeout);
}

static int camrtc_hsp_vm_probe(struct camrtc_hsp *camhsp)
{
	struct device_node *np = camhsp->dev.parent->of_node;
	int err = -ENOTSUPP;
	const char *obtain = "";

	np = of_get_compatible_child(np, "nvidia,tegra-camrtc-hsp-vm");
	if (!of_device_is_available(np)) {
		of_node_put(np);
		dev_err(&camhsp->dev, "no hsp protocol \"%s\"\n",
			"nvidia,tegra-camrtc-hsp-vm");
		return -ENOTSUPP;
	}

	camhsp->ss = of_tegra_hsp_ss_by_name(np, obtain = "vm-ss");
	if (IS_ERR(camhsp->ss)) {
		err = PTR_ERR(camhsp->ss);
		if (err != -ENODATA && err != -EINVAL)
			goto fail;
		dev_info(&camhsp->dev,
			 "operating without shared semaphores.\n");
	}

	camhsp->rx = of_tegra_hsp_sm_rx_by_name(np, obtain = "vm-rx",
			camrtc_hsp_rx_full_notify, camhsp);
	if (IS_ERR(camhsp->rx)) {
		err = PTR_ERR(camhsp->rx);
		goto fail;
	}

	camhsp->tx = of_tegra_hsp_sm_tx_by_name(np, obtain = "vm-tx",
			camrtc_hsp_tx_empty_notify, camhsp);
	if (IS_ERR(camhsp->rx)) {
		err = PTR_ERR(camhsp->rx);
		goto fail;
	}

	camhsp->dev.of_node = np;
	camhsp->op = &camrtc_hsp_vm_ops;
	dev_set_name(&camhsp->dev, "%s:%s",
		dev_name(camhsp->dev.parent), camhsp->dev.of_node->name);
	dev_dbg(&camhsp->dev, "probed\n");

	return 0;

fail:
	if (err != -EPROBE_DEFER) {
		dev_err(&camhsp->dev, "%s: failed to obtain %s: %d\n",
			np->name, obtain, err);
	}
	of_node_put(np);
	return err;
}

/* ---------------------------------------------------------------------- */
/* Public interface */

void camrtc_hsp_group_ring(struct camrtc_hsp *camhsp,
		u16 group)
{
	if (!WARN_ON(camhsp == NULL))
		camhsp->op->group_ring(camhsp, group);
}
EXPORT_SYMBOL(camrtc_hsp_group_ring);

/*
 * Synchronize the HSP
 */
int camrtc_hsp_sync(struct camrtc_hsp *camhsp)
{
	long timeout;
	int response;

	if (WARN_ON(camhsp == NULL))
		return -EINVAL;

	timeout = camhsp->timeout;
	mutex_lock(&camhsp->mutex);
	response = camhsp->op->sync(camhsp, &timeout);
	mutex_unlock(&camhsp->mutex);

	return response;
}
EXPORT_SYMBOL(camrtc_hsp_sync);

/*
 * Resume: resume the firmware
 */
int camrtc_hsp_resume(struct camrtc_hsp *camhsp)
{
	long timeout;
	int response;

	if (WARN_ON(camhsp == NULL))
		return -EINVAL;

	timeout = camhsp->timeout;
	mutex_lock(&camhsp->mutex);
	response = camhsp->op->resume(camhsp, &timeout);
	mutex_unlock(&camhsp->mutex);

	return response;
}
EXPORT_SYMBOL(camrtc_hsp_resume);

/*
 * Suspend: set firmware to idle.
 */
int camrtc_hsp_suspend(struct camrtc_hsp *camhsp)
{
	long timeout;
	int response;

	if (WARN_ON(camhsp == NULL))
		return -EINVAL;

	timeout = camhsp->timeout;
	mutex_lock(&camhsp->mutex);
	response = camhsp->op->suspend(camhsp, &timeout);
	mutex_unlock(&camhsp->mutex);

	if (response != 0)
		dev_WARN(&camhsp->dev, "PM_SUSPEND failed: 0x%08x\n",
			response);

	return response <= 0 ? response : -EIO;
}
EXPORT_SYMBOL(camrtc_hsp_suspend);

/*
 * Bye: tell firmware that VM mappings are going away
 */
int camrtc_hsp_bye(struct camrtc_hsp *camhsp)
{
	long timeout;
	int response;

	if (WARN_ON(camhsp == NULL))
		return -EINVAL;

	timeout = camhsp->timeout;
	mutex_lock(&camhsp->mutex);
	response = camhsp->op->bye(camhsp, &timeout);
	mutex_unlock(&camhsp->mutex);

	if (response != 0)
		dev_WARN(&camhsp->dev, "BYE failed: 0x%08x\n", response);

	return response;
}
EXPORT_SYMBOL(camrtc_hsp_bye);

int camrtc_hsp_ch_setup(struct camrtc_hsp *camhsp, dma_addr_t iova)
{
	long timeout;
	int response;

	if (WARN_ON(camhsp == NULL))
		return -EINVAL;

	if (iova >= BIT_ULL(32) || (iova & 0xffU) != 0) {
		dev_WARN(&camhsp->dev,
			"CH_SETUP invalid iova: 0x%08llx\n", iova);
		return -EINVAL;
	}

	timeout = camhsp->timeout;
	mutex_lock(&camhsp->mutex);
	response = camhsp->op->ch_setup(camhsp, iova, &timeout);
	mutex_unlock(&camhsp->mutex);

	if (response > 0)
		dev_dbg(&camhsp->dev, "CH_SETUP failed: 0x%08x\n", response);

	return response;
}
EXPORT_SYMBOL(camrtc_hsp_ch_setup);

int camrtc_hsp_ping(struct camrtc_hsp *camhsp, u32 data, long timeout)
{
	long left = timeout;
	int response;

	if (WARN_ON(camhsp == NULL))
		return -EINVAL;

	if (left == 0L)
		left = camhsp->timeout;

	mutex_lock(&camhsp->mutex);
	response = camhsp->op->ping(camhsp, data, &left);
	mutex_unlock(&camhsp->mutex);

	return response;
}
EXPORT_SYMBOL(camrtc_hsp_ping);

int camrtc_hsp_get_fw_hash(struct camrtc_hsp *camhsp,
		u8 hash[], size_t hash_size)
{
	int i;
	int ret = 0;
	long timeout;

	if (WARN_ON(camhsp == NULL))
		return -EINVAL;

	memset(hash, 0, hash_size);
	timeout = camhsp->timeout;
	mutex_lock(&camhsp->mutex);

	for (i = 0; i < hash_size; i++) {
		int value = camhsp->op->get_fw_hash(camhsp, i, &timeout);

		if (value < 0 || value > 255) {
			dev_warn(&camhsp->dev,
				"FW_HASH failed: 0x%08x\n", value);
			ret = value < 0 ? value : -EIO;
			goto fail;
		}

		hash[i] = value;
	}

fail:
	mutex_unlock(&camhsp->mutex);

	return ret;
}
EXPORT_SYMBOL(camrtc_hsp_get_fw_hash);

static const struct device_type camrtc_hsp_combo_dev_type = {
	.name	= "camrtc-hsp-protocol",
};

static void camrtc_hsp_combo_dev_release(struct device *dev)
{
	struct camrtc_hsp *camhsp = container_of(dev, struct camrtc_hsp, dev);

	if (!IS_ERR_OR_NULL(camhsp->rx))
		tegra_hsp_sm_rx_free(camhsp->rx);
	if (!IS_ERR_OR_NULL(camhsp->tx))
		tegra_hsp_sm_tx_free(camhsp->tx);
	if (!IS_ERR_OR_NULL(camhsp->ss))
		tegra_hsp_ss_free(camhsp->ss);

	of_node_put(dev->of_node);
	kfree(camhsp);
}

static int camrtc_hsp_probe(struct camrtc_hsp *camhsp)
{
	int ret;

	ret = camrtc_hsp_vm_probe(camhsp);
	if (ret != -ENOTSUPP)
		return ret;

	return -ENODEV;
}

struct camrtc_hsp *camrtc_hsp_create(
	struct device *dev,
	void (*group_notify)(struct device *dev, u16 group),
	long cmd_timeout)
{
	struct camrtc_hsp *camhsp;
	int ret = -EINVAL;

	camhsp = kzalloc(sizeof(*camhsp), GFP_KERNEL);
	if (camhsp == NULL)
		return ERR_PTR(-ENOMEM);

	camhsp->dev.parent = dev;
	camhsp->group_notify = group_notify;
	camhsp->timeout = cmd_timeout;
	mutex_init(&camhsp->mutex);
	spin_lock_init(&camhsp->sendlock);
	init_waitqueue_head(&camhsp->response_waitq);
	init_completion(&camhsp->emptied);
	atomic_set(&camhsp->response, -1);

	camhsp->dev.type = &camrtc_hsp_combo_dev_type;
	camhsp->dev.release = camrtc_hsp_combo_dev_release;
	device_initialize(&camhsp->dev);

	dev_set_name(&camhsp->dev, "%s:%s", dev_name(dev), "hsp");

	pm_runtime_no_callbacks(&camhsp->dev);
	pm_runtime_enable(&camhsp->dev);

	ret = camrtc_hsp_probe(camhsp);
	if (ret < 0)
		goto fail;

	ret = device_add(&camhsp->dev);
	if (ret < 0)
		goto fail;

	dev_set_drvdata(&camhsp->dev, camhsp);

	return camhsp;

fail:
	camrtc_hsp_free(camhsp);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(camrtc_hsp_create);

void camrtc_hsp_free(struct camrtc_hsp *camhsp)
{
	if (IS_ERR_OR_NULL(camhsp))
		return;

	pm_runtime_disable(&camhsp->dev);

	if (dev_get_drvdata(&camhsp->dev) != NULL)
		device_unregister(&camhsp->dev);
	else
		put_device(&camhsp->dev);
}
EXPORT_SYMBOL(camrtc_hsp_free);
