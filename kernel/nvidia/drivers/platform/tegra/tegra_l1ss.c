/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/tegra-ivc.h>
#include <linux/tegra-safety-ivc.h>
#include <linux/tegra-ivc-instance.h>
#include <linux/tegra-hsp.h>
#include <linux/tegra_l1ss_ioctl.h>
#include <linux/tegra_l1ss_kernel_interface.h>
#include <linux/wait.h>

#include "tegra_l1ss.h"
#include "tegra_l1ss_cmd_resp_exec_config.h"
#include "tegra_l1ss_cmd_resp_l2_interface.h"

#define MAX_DEV 1

struct l1ss_data *ldata;
struct class *l1ss_class;
int dev_major;
static cmd_resp_look_up_ex cmd_resp_lookup_table[CMDRESPL1_N_CLASSES]
						[CMDRESPL1_MAX_CMD_IN_CLASS] = {
						 CMDRESPL1_L2_CLASS0,
						 CMDRESPL1_L2_CLASS1,
						 CMDRESPL1_L2_CLASS2
						 };


static int l1ss_open(struct inode *inode, struct file *file);
static int l1ss_release(struct inode *inode, struct file *file);
static long l1ss_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

static const struct file_operations l1ss_fops = {
	.owner		= THIS_MODULE,
	.open		= l1ss_open,
	.release	= l1ss_release,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= l1ss_ioctl,
#endif
	.unlocked_ioctl = l1ss_ioctl,
};

int l1ss_cmd_resp_send_frame(const cmdresp_frame_ex_t *p_cmd_pkt,
			     nv_guard_3lss_layer_t layer_id,
			     struct l1ss_data *ldata)
{
	struct tegra_safety_ivc_chan *ivc_ch = NULL;
	int ret = 0;

	mutex_lock(&ldata->safety_ivc->wlock);

	if (atomic_read(&ldata->safety_ivc->ivc_ready) == 1 &&
			layer_id == CMDRESPEXEC_L2_LAYER_ID) {
		ivc_ch = tegra_safety_get_ivc_chan_from_str(ldata->safety_ivc,
							    "cmdresp");
		if (!ivc_ch) {
			pr_err("L1SS: Failed to get cmdresp IVC channel\n");
			ret = -EINVAL;
			goto unlock_mutex;
		}

		ret = tegra_ivc_write(&ivc_ch->ivc, p_cmd_pkt,
					sizeof(cmdresp_frame_ex_t));
		if (ret < 0) {
			pr_err("%s: IVC write failed\n", __func__);
			goto unlock_mutex;
		}
		ret = 0;
	}

unlock_mutex:
	mutex_unlock(&ldata->safety_ivc->wlock);
	return ret;
}

static int l1ss_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	add_uevent_var(env, "DEVMODE=%#o", 0666);
	return 0;
}

static int l1ss_process_request(nv_guard_request_t *req,
				struct l1ss_data *ldata) {

	int ret = 0;

	PDEBUG("Command = %d\n", req->srv_id_cmd);
	switch (req->srv_id_cmd) {
	case NVGUARD_SERVICESTATUS_NOTIFICATION:
		ret = user_send_service_status_notification(&req->srv_status,
							CMDRESPEXEC_L2_LAYER_ID,
							ldata);
		break;
	case NVGUARD_SEND_ISTMSG:
		ret = user_send_ist_mesg(&req->user_msg,
							CMDRESPEXEC_L2_LAYER_ID,
							ldata);
		break;
	case NVGUARD_PHASE_NOTIFICATION:
		ret = user_send_phase_notify(ldata, CMDRESPEXEC_L2_LAYER_ID,
				req->phase);
		break;
	default:
		PDEBUG("cmd = %d not implemented\n", req->srv_id_cmd);
	}

	return ret;
}

static void l1ss_workqueue_function(struct work_struct *work)
{
	struct l1ss_data *ldata =
		container_of(work, struct l1ss_data, work);
	struct l1ss_req_node *t = NULL;
	nv_guard_request_t *req = NULL;

	spin_lock(&ldata->slock);
	t = ldata->head;

	while (t) {
		req = t->req;
		ldata->head = t->next;
		spin_unlock(&ldata->slock);

		l1ss_process_request(req, ldata);
		kfree(t->req);
		kfree(t);

		spin_lock(&ldata->slock);
		t = ldata->head;
	}
	spin_unlock(&ldata->slock);
}

int l1ss_init(struct tegra_safety_ivc *safety_ivc)
{
	int err;

	ldata = kmalloc(sizeof(struct l1ss_data), GFP_KERNEL);
	if (ldata == NULL)
		return -1;
	spin_lock_init(&ldata->slock);
	ldata->wq = alloc_workqueue("l1ss", WQ_HIGHPRI, 0);
	ldata->head = NULL;
	INIT_WORK(&ldata->work, l1ss_workqueue_function);
	init_waitqueue_head(&ldata->cmd.notify_waitq);
	atomic_set(&ldata->cmd.notify_registered, 0);

	ldata->cmd_resp_lookup_table = cmd_resp_lookup_table;

	err = alloc_chrdev_region(&ldata->dev, 0, MAX_DEV, "l1ss");

	ldata->dev_major = MAJOR(ldata->dev);

	ldata->l1ss_class = class_create(THIS_MODULE, "l1ss");
	ldata->l1ss_class->dev_uevent = l1ss_uevent;

	cdev_init(&ldata->cdev, &l1ss_fops);
	ldata->cdev.owner = THIS_MODULE;

	cdev_add(&ldata->cdev, MKDEV(ldata->dev_major, 0), 1);

	device_create(ldata->l1ss_class, NULL, MKDEV(ldata->dev_major, 0),
		      NULL, "l1ss-%d", 0);

	ldata->safety_ivc = safety_ivc;
	safety_ivc->ldata = ldata;

	return 0;
}

int l1ss_exit(struct tegra_safety_ivc *safety_ivc)
{
	struct l1ss_data *ldata;

	if (safety_ivc && safety_ivc->ldata) {
		ldata = safety_ivc->ldata;
	} else {
		PDEBUG("%s(%d) no ldata present\n", __func__, __LINE__);
		return 0;
	}

	destroy_workqueue(ldata->wq);
	cdev_del(&ldata->cdev);
	device_destroy(l1ss_class, MKDEV(dev_major, 0));
	class_destroy(l1ss_class);
	unregister_chrdev_region(MKDEV(dev_major, 0), MINORMASK);

	kfree(ldata);
	ldata = NULL;

	PDEBUG("Device exit\n");
	return 0;
}

static int l1ss_open(struct inode *inode, struct file *file)
{
	struct l1ss_data *ldata = NULL;

	PDEBUG("Device open\n");

	ldata = container_of(inode->i_cdev, struct l1ss_data, cdev);
	file->private_data = ldata;

	if (ldata == NULL)
		return -1;

	return 0;
}

static int l1ss_release(struct inode *inode, struct file *file)
{

	PDEBUG("Device close\n");
	ldata = container_of(inode->i_cdev, struct l1ss_data, cdev);
	if (ldata) {
		l1ss_class = ldata->l1ss_class;
		dev_major = ldata->dev_major;
	} else {
		PDEBUG("ldata is NULL\n");
	}

	return 0;
}

/*
 * Helper functions to update/fetch data from CmdResp Header
 *
 * Deviated MISRA Rule 5.9 (Advisory)
 * Declaring an internal linkage function with identifier "l_get_dest_class_id".
 *
 * Reason:
 * False Positive
 */
inline void  l_get_dest_class_id(const cmdresp_header_t *pHeader, uint8_t *ClassId)
{
	uint16_t l_cmd_opcode = pHeader->cmd_opcode;
	*ClassId = (uint8_t)(((l_cmd_opcode & CMDRESPL2_DEST_CLASS_ID_MASK)
				>>CMDRESPL2_DEST_CLASS_ID_SHIFT)
				& (uint8_t)UINT8_MAX);
}

/*
 * Deviated MISRA Rule 5.9 (Advisory)
 * Declaring an internal linkage function with identifier "l_set_dest_class_id".
 *
 * Reason:
 * False Positive
 */
inline void l_set_dest_class_id(cmdresp_header_t *const pHeader, uint8_t ClassId)
{
	uint16_t *l_pcmd_opcode = &(pHeader->cmd_opcode);
	uint16_t l_cmd_opcode = pHeader->cmd_opcode;
	uint16_t lClassId = (uint16_t)(ClassId);

	l_cmd_opcode = (((l_cmd_opcode) & (~(CMDRESPL2_DEST_CLASS_ID_MASK)))|
			(uint16_t)((lClassId << CMDRESPL2_DEST_CLASS_ID_SHIFT) &
				(uint16_t)UINT16_MAX)) & (uint16_t)UINT16_MAX;
	*l_pcmd_opcode = l_cmd_opcode;
}

/*
 * Deviated MISRA Rule 5.9 (Advisory)
 * Declaring an internal linkage function with identifier "l_get_cmd_id".
 *
 * Reason:
 * False Positive
 */
inline void l_get_cmd_id(const cmdresp_header_t *pHeader, uint8_t *CmdId)
{
	uint16_t l_cmd_opcode = pHeader->cmd_opcode;

	*CmdId = (uint8_t)(((l_cmd_opcode & CMDRESPL2_CMD_ID_MASK)
				>>CMDRESPL2_CMD_ID_SHIFT)&(uint8_t)UINT8_MAX);
}

/*
 * Deviated MISRA Rule 5.9 (Advisory)
 * Declaring an internal linkage function with identifier "l_set_cmd_id".
 *
 * Reason:
 * False Positive
 **/
inline void l_set_cmd_id(cmdresp_header_t *const pHeader, uint8_t CmdId)
{
	uint16_t *l_pcmd_opcode = &(pHeader->cmd_opcode);
	uint16_t l_cmd_opcode = pHeader->cmd_opcode;

	l_cmd_opcode = (((l_cmd_opcode) & (~(CMDRESPL2_CMD_ID_MASK)))|
			((uint16_t)(((uint16_t)CmdId << CMDRESPL2_CMD_ID_SHIFT)&
				(uint16_t)UINT16_MAX))) & (uint16_t)UINT16_MAX;
	*l_pcmd_opcode = l_cmd_opcode;
}

/*
 * Deviated MISRA Rule 5.9 (Advisory)
 * Declaring an internal linkage function with identifier "l_get_dest_id".
 *
 * Reason:
 * False Positive
 */
inline void l_get_dest_id(const cmdresp_header_t *pHeader, uint8_t *CmdDestId)
{
	*CmdDestId = pHeader->dest;
}

/*
 * Deviated MISRA Rule 5.9 (Advisory)
 * Declaring an internal linkage function with identifier "l_set_dest_id".
 *
 * Reason:
 * False Positive
 */
inline void l_set_dest_id(cmdresp_header_t *const pHeader, uint8_t DestId)
{
	pHeader->dest = DestId;
}

/*
 * Deviated MISRA Rule 5.9 (Advisory)
 * Declaring an internal linkage function with identifier "l_get_src_id".
 *
 * Reason:
 * False Positive
 */
inline void l_get_src_id(const cmdresp_header_t *pHeader, uint8_t *CmdSrcId)
{
	*CmdSrcId = pHeader->src;
}

/*
 * Deviated MISRA Rule 5.9 (Advisory)
 * Declaring an internal linkage function with identifier "l_set_src_id".
 *
 * Reason:
 * False Positive
 */
inline void l_set_src_id(cmdresp_header_t *const pHeader, uint8_t CmdSrcId)
{
	pHeader->src = CmdSrcId;
}

/*
 * Deviated MISRA Rule 5.9 (Advisory)
 * Declaring an internal linkage function with identifier "l_is_resp_flag_set".
 *
 * Reason:
 * False Positive
 */
inline bool l_is_resp_flag_set(const cmdresp_header_t Header)
{
	uint16_t l_cmd_opcode = Header.cmd_opcode;

	return(((l_cmd_opcode) | (CMDRESPL2_RESPFLAG_SHIFT)) == 0xFFFFU);
}

/*
 * Deviated MISRA Rule 5.9 (Advisory)
 * Declaring an internal linkage function with identifier "l_set_resp_flag".
 *
 * Reason:
 * False Positive
 */
inline void l_set_resp_flag(cmdresp_header_t *const pHeader)
{
	pHeader->cmd_opcode |= CMDRESPL2_RESPFLAG_MASK;
}

/*
 * Deviated MISRA Rule 5.9 (Advisory)
 * Declaring an internal linkage function with identifier "l_set_cmd_msg_id".
 *
 * Reason:
 * False Positive
 */
inline void l_set_cmd_msg_id(cmdresp_header_t *const pHeader, uint16_t CmdMsgId)
{
	pHeader->cmd_opcode &= CMDRESPL2_CMD_ID_RESET;
	pHeader->cmd_opcode |= (CmdMsgId << CMDRESPL2_CMD_ID_SHIFT) &
				(uint16_t)UINT16_MAX;
}

/*
 * Deviated MISRA Rule 5.9 (Advisory)
 * Declaring an internal linkage function with identifier "l_set_src_class_id".
 *
 * Reason:
 * False Positive
 */
inline void l_set_src_class_id(cmdresp_header_t *const pHeader, uint8_t ClassId)
{
	uint16_t *l_pcmd_opcode = &(pHeader->cmd_opcode);
	uint16_t l_cmd_opcode = pHeader->cmd_opcode;

	l_cmd_opcode = (((l_cmd_opcode) & (~(CMDRESPL2_SRC_CLASS_ID_MASK))) |
			(ClassId))&(uint16_t)UINT16_MAX;
	*l_pcmd_opcode = l_cmd_opcode;
}

nv_guard_3lss_layer_t cmd_resp_get_current_layer_id(void)
{
	return 1;
}

void l1ss_get_class_cmd_resp_from_header(cmdresp_header_t *cmdresp_h,
		uint8_t *class, uint8_t *cmd, bool *is_resp)
{
	l_get_cmd_id(cmdresp_h, cmd);
	l_get_dest_class_id(cmdresp_h, class);
	*is_resp = l_is_resp_flag_set(*cmdresp_h);
}

/*
 * @brief Function to set CmdResp Header data members
 *
 * - <b>Description</b>\n
 *      Function to set CmdResp Header data members
 *
 * @param  pHeader(in) Pointer to CmdResp Frame Header
 *
 * @param Class(in)  Destination class Id
 *
 * @param Cmd(in) Command Id
 *
 * @param DestId(in) Destination Layer Id
 *
 * @param IsResp(in) true if response bit needs to be set
 *
 * @return void
 *
 */
void
cmd_resp_update_header(
		cmdresp_header_t *pHeader,
		uint8_t Class,
		uint8_t Cmd,
		uint32_t DestId,
		bool IsResp)
{
	uint8_t lLayerId;
	nv_guard_3lss_layer_t lData = cmd_resp_get_current_layer_id();

	if (lData <= NVGUARD_MAX_LAYERID) {
		lLayerId = (uint8_t)lData;
		l_set_dest_class_id(pHeader, Class);
		l_set_cmd_id(pHeader, Cmd);
		if (DestId <= NVGUARD_MAX_LAYERID) {
			l_set_dest_id(pHeader, (uint8_t)DestId);
			l_set_src_id(pHeader, lLayerId);
			if (true == IsResp)
				l_set_resp_flag(pHeader);
		} else {
			pr_err("L1SS %s(%d) Wrong Layer Id Sent:%d:%d\n",
					__func__, __LINE__, Class, Cmd);
		}
	} else {
		pr_err("L1SS %s(%d) Wrong Layer Id Fetched:%d:%d\n",
				__func__, __LINE__, Class, Cmd);
	}
}

int tegra_safety_handle_cmd(cmdresp_frame_ex_t *cmd_resp,
			    struct l1ss_data *ldata)
{
	cmdresp_header_t *cmdresp_h = &(cmd_resp->header);
	uint8_t src;
	uint8_t dest;
	uint8_t cmd;
	uint8_t dest_class;
	bool is_resp = false;

	l_get_src_id(cmdresp_h, &src);
	l_get_cmd_id(cmdresp_h, &cmd);
	l_get_dest_id(cmdresp_h, &dest);
	l_get_dest_class_id(cmdresp_h, &dest_class);
	is_resp = l_is_resp_flag_set(*cmdresp_h);

	PDEBUG("srcID %d destID %d cmdID %d ClassID %d is_resp=%d\n",
			src, dest, cmd, dest_class, is_resp);

	if (dest_class <  CMDRESPL1_N_CLASSES && cmd <
			CMDRESPL1_MAX_CMD_IN_CLASS) {
		cmd_resp_look_up_ex cmd_entry =
				ldata->cmd_resp_lookup_table[dest_class][cmd];

		if (cmd_entry.cmd == cmd) {
			if (is_resp)
				cmd_entry.resp_call_back(cmd_resp, ldata);
			else
				cmd_entry.cmd_call_back(cmd_resp, ldata);
		} else {
			PDEBUG(
			"%s(%d)bad cmd entry class=%d cmd=%d cmd_entry=%d\n",
				__func__, __LINE__,
				dest_class, cmd, cmd_entry.cmd);
		}
	} else {
		PDEBUG("%s(%d) bad class or cmd received class=%d cmd=%d\n",
				__func__, __LINE__, dest_class, cmd);
	}
	return 0;
}

static long l1ss_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct l1ss_data *ldata = NULL;
	nv_guard_request_t *req = NULL;

	ldata = (struct l1ss_data *)file->private_data;

	switch (cmd) {
	case L1SS_CLIENT_REQUEST:
		PDEBUG("L1SS_CLIENT_REQUEST\n");

		req = kmalloc(sizeof(nv_guard_request_t), GFP_KERNEL);
		if (req == NULL) {
			pr_err("Failed to allocate memory");
			return -1;
		}
		if (copy_from_user(req, (nv_guard_request_t *)arg,
				   sizeof(nv_guard_request_t))) {
			pr_err("Failed copy_from_user");
			return -EACCES;
		}

		l1ss_submit_rq(req, true);
		kfree(req);
		break;
	default:
		PDEBUG("unknown command");
		break;
	}

	return 0;
}

/*Below function can be called from interrupt context. So no sleep*/
int l1ss_submit_rq(nv_guard_request_t *req, bool can_sleep)
{
	struct l1ss_req_node *n = NULL;
	struct l1ss_req_node *t = NULL;

	if (can_sleep)
		n = kmalloc(sizeof(struct l1ss_req_node), GFP_KERNEL);
	else
		n = kmalloc(sizeof(struct l1ss_req_node), GFP_ATOMIC);
	if (n == NULL) {
		pr_err("Failed to allocate memory");
		return -1;
	}
	n->next = NULL;
	if (can_sleep)
		n->req = kmalloc(sizeof(nv_guard_request_t), GFP_KERNEL);
	else
		n->req = kmalloc(sizeof(nv_guard_request_t), GFP_ATOMIC);
	if (n->req == NULL) {
		pr_err("Failed to allocate memory");
		kfree(n);
		return -1;
	}
	memcpy(n->req, req, sizeof(nv_guard_request_t));

	spin_lock(&ldata->slock);
	if (ldata->head == NULL) {
		ldata->head = n;
	} else {
		t = ldata->head;
		while (t->next)
			t = t->next;
		t->next = n;
	}
	spin_unlock(&ldata->slock);
	queue_work(ldata->wq, &ldata->work);

	return 0;

}
EXPORT_SYMBOL_GPL(l1ss_submit_rq);
