// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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
 *
 * nv_acsl_core.c -- ACSL (ADSP Component Shim Layer) Kernel Driver
 */

#include <linux/module.h>

#include "nv_acsl.h"
#include "nv_acsl_ipc.h"

static inline void log_buf_info(struct device *dev, const char *func,
	uint8_t COMP_ID, uint8_t PORT, uint8_t buff_indx, bool block)
{
#ifdef BUF_PRINTS
	struct acsl_drv *drv = dev_get_drvdata(dev);
	struct csm_sm_state_t *csm_sm = drv->csm_sm;

	dev_info(dev, "%s: PORT:%d, Comp.ID:%d\n", func, PORT, COMP_ID);
	dev_info(dev, "I.RBI:%d, I.ABI:%d, mI.RBI:%d, mI.ABI:%d\n",
		csm_sm->rel_buf_index[IN_PORT][COMP_ID],
		csm_sm->acq_buf_index[IN_PORT][COMP_ID],
		drv->m_rel_buf_index[IN_PORT][COMP_ID],
		drv->m_acq_buf_index[IN_PORT][COMP_ID]);
	dev_info(dev, "O.RBI:%d, O.ABI:%d, mO.RBI:%d, mO.ABI:%d\n",
		csm_sm->rel_buf_index[OUT_PORT][COMP_ID],
		csm_sm->acq_buf_index[OUT_PORT][COMP_ID],
		drv->m_rel_buf_index[OUT_PORT][COMP_ID],
		drv->m_acq_buf_index[OUT_PORT][COMP_ID]);
	dev_info(dev, "User.BI:%d bBlock:%d\n", buff_indx, block);
#endif
}

static inline uint32_t buffer_index_wrap(uint32_t index)
{
	return index & (MAX_PORT_BUFF - 1);
}

static bool is_free_index_avail(struct acsl_drv *drv,
	uint8_t COMP_ID, uint8_t PORT, bool block)
{
	struct device *dev = drv->dev;
	struct csm_sm_state_t *csm_sm = drv->csm_sm;
	status_t ret;

	bool wait_for_buffers = false;

	if (!drv->append_init_input_buff[COMP_ID] && PORT == IN_PORT) {
		dev_info(dev, "Initial free buffer avail, COMP_ID:%d\n",
			COMP_ID);
		return true;
	}

	if (PORT == IN_PORT) {
		if ((drv->m_rel_buf_index[PORT][COMP_ID] -
			drv->m_acq_buf_index[PORT][COMP_ID]) >=
				MAX_PORT_BUFF)
			wait_for_buffers = true;
	} else if (PORT == OUT_PORT) {
		if (drv->m_rel_buf_index[PORT][COMP_ID] ==
			drv->m_acq_buf_index[PORT][COMP_ID])
			wait_for_buffers = true;
	}

	if (wait_for_buffers) {
		log_buf_info(dev, __func__, COMP_ID, PORT, 0, block);

		if (block) {
			mutex_lock(&drv->port_lock[PORT][COMP_ID]);
			reinit_completion(&drv->buff_complete[PORT][COMP_ID]);
			mutex_unlock(&drv->port_lock[PORT][COMP_ID]);
			ret = wait_for_completion_timeout(
				&drv->buff_complete[PORT][COMP_ID],
				msecs_to_jiffies(ACSL_TIMEOUT));
			if (ret <= 0)
				goto log;
			return true;
		} else {
			return false;
		}
	}

	return true;

log:
	dev_err(dev, "timeout occur on PORT:%d COMP_ID:%d\n",
		PORT, COMP_ID);
	dev_info(dev, "%s: PORT:%d, Comp.ID:%d\n",
		__func__, PORT, COMP_ID);
	dev_info(dev, "I.RBI:%d, I.ABI:%d, mI.RBI:%d, mI.ABI:%d\n",
		csm_sm->rel_buf_index[IN_PORT][COMP_ID],
		csm_sm->acq_buf_index[IN_PORT][COMP_ID],
		drv->m_rel_buf_index[IN_PORT][COMP_ID],
		drv->m_acq_buf_index[IN_PORT][COMP_ID]);
	dev_info(dev, "O.RBI:%d, O.ABI:%d, mO.RBI:%d, mO.ABI:%d\n",
		csm_sm->rel_buf_index[OUT_PORT][COMP_ID],
		csm_sm->acq_buf_index[OUT_PORT][COMP_ID],
		drv->m_rel_buf_index[OUT_PORT][COMP_ID],
		drv->m_acq_buf_index[OUT_PORT][COMP_ID]);
	dev_info(dev, "bBlock:%d\n",  block);
	return false;
}

/** CPU -> ADSP */
static int append_buf_to_csm(struct acsl_drv *drv, uint8_t PORT,
	uint8_t COMP_ID)
{
	struct csm_sm_state_t *csm_sm = drv->csm_sm;
	struct device *dev = drv->dev;
	status_t ret = 0;

	csm_sm->acq_buf_index[PORT][COMP_ID] =
		drv->m_rel_buf_index[PORT][COMP_ID];

	if (IN_PORT == PORT) {
		while ((ret = nvadsp_mbox_send(&drv->csm_mbox_buf_in_recv,
			COMP_ID, NVADSP_MBOX_SMSG, 0, ACSL_TIMEOUT)) != 0) {
			dev_warn(dev, "%s: INPUT: Warn: Mbx Send is failed ret:%d\n",
				__func__, ret);
			msleep(1000);
		}
	} else if (OUT_PORT == PORT) {
		while ((ret = nvadsp_mbox_send(&drv->csm_mbox_buf_out_recv,
			COMP_ID, NVADSP_MBOX_SMSG, 0, ACSL_TIMEOUT)) != 0) {
			dev_warn(dev, "%s: OUTPUT: Warn: Mbx Send is failed ret:%d\n",
				__func__, ret);
			msleep(1000);
		}
	}

	log_buf_info(dev, __func__, COMP_ID, PORT, 0, 0);
	return ret;
}

/* Call back ADSP -> CPU */
static void release_buf_from_csm(struct acsl_drv *drv, uint8_t PORT,
	uint8_t COMP_ID)
{
	struct csm_sm_state_t *csm_sm = drv->csm_sm;

	drv->m_acq_buf_index[PORT][COMP_ID] =
		csm_sm->rel_buf_index[PORT][COMP_ID];
	drv->m_acq_buf_index[PORT][COMP_ID]++;
}

/* CSM mailbox message handler */
static status_t csm_buff_in_msg_handler(uint32_t msg, void *data)
{
	struct acsl_drv *drv = data;
	uint8_t COMP_ID;

	COMP_ID = msg;
	release_buf_from_csm(drv, IN_PORT, COMP_ID);
	complete_all(&drv->buff_complete[IN_PORT][COMP_ID]);

	return 0;
}

static void send_message(struct acsl_drv *drv, struct acsl_csm_args_t *csm_args)
{
	struct csm_sm_state_t *csm_sm = drv->csm_sm;
	struct device *dev = drv->dev;
	union csm_message_t message;
	status_t ret, i;

	message.msgq_msg.size = csm_args->size;

	for (i = 0; i < csm_args->size; i++)
		message.msg.payload[i] = csm_args->payload[i];

	while ((ret = msgq_queue_message(&csm_sm->recv_msgq.msgq,
			&message.msgq_msg)) != 0) {
		dev_warn(dev, "%s: Warn: msgq is failed(ret: %d)\n",
			__func__, ret);
		msleep(1000);
	}
}

/* CSM mailbox message handler */
static status_t csm_buff_out_msg_handler(uint32_t msg, void *data)
{
	struct acsl_drv *drv = data;
	uint8_t COMP_ID;

	COMP_ID = msg;
	release_buf_from_csm(drv, OUT_PORT, COMP_ID);
	complete_all(&drv->buff_complete[OUT_PORT][COMP_ID]);

	return 0;
}
/*  Deinit csm app and close mailboxes */
void csm_app_deinit(struct acsl_drv *drv)
{
	uint8_t i, j;

	nvadsp_mbox_close(&drv->csm_mbox_send);
	nvadsp_mbox_close(&drv->csm_mbox_recv);
	nvadsp_mbox_close(&drv->csm_mbox_buf_in_send);
	nvadsp_mbox_close(&drv->csm_mbox_buf_out_send);
	nvadsp_mbox_close(&drv->csm_mbox_buf_in_recv);
	nvadsp_mbox_close(&drv->csm_mbox_buf_out_recv);

	for (j = 0; j < MAX_COMP; j++) {
		for (i = 0; i < MAX_PORTS; i++) {
			mutex_destroy(&drv->port_lock[i][j]);
			complete_all(&drv->buff_complete[i][j]);
		}
	}
	nvadsp_app_unload(drv->csm_app_handle);
}

/* Initialize csm app and mailboxes  */
status_t csm_app_init(struct acsl_drv *drv)
{
	struct csm_sm_state_t *csm_sm;
	struct device *dev = drv->dev;
	uint8_t i, j;
	status_t ret = 0;

	for (i = 0; i < MAX_COMP; i++) {
		for (j = 0; j < MAX_PORTS; j++) {
			drv->m_acq_buf_index[j][i] = 0;
			drv->m_rel_buf_index[j][i] = 0;
			mutex_init(&drv->port_lock[j][i]);
			init_completion(&drv->buff_complete[j][i]);
		}
	}

	drv->csm_app_handle = nvadsp_app_load("csm_sm", "csm_sm.elf");
	if (!drv->csm_app_handle) {
		ret = -ENODEV;
		goto error;
	}

	drv->csm_app_info = nvadsp_app_init(drv->csm_app_handle, NULL);
	if (!drv->csm_app_info)
		goto error;

	ret = nvadsp_app_start(drv->csm_app_info);
	if (ret)
		goto error;

	csm_sm = (struct csm_sm_state_t *)drv->csm_app_info->mem.shared;
	if (!csm_sm) {
		ret = -ENOMEM;
		goto error;
	}

	ret = nvadsp_mbox_open(&drv->csm_mbox_send, &csm_sm->mbox_csm_send_id,
			"csm_send", NULL, NULL);
	if (ret)
		goto error;

	ret = nvadsp_mbox_open(&drv->csm_mbox_recv, &csm_sm->mbox_csm_recv_id,
			"csm_ack", NULL, NULL);
	if (ret)
		goto error;

	ret = nvadsp_mbox_open(&drv->csm_mbox_buf_in_send,
			&csm_sm->mbox_buf_in_send_id,
			"csm_buff_in_send",
			csm_buff_in_msg_handler, drv);
	if (ret)
		goto error;

	ret = nvadsp_mbox_open(&drv->csm_mbox_buf_out_send,
			&csm_sm->mbox_buf_out_send_id,
			"csm_buff_out_send",
			csm_buff_out_msg_handler, drv);
	if (ret)
		goto error;

	ret = nvadsp_mbox_open(&drv->csm_mbox_buf_in_recv,
			&csm_sm->mbox_buf_in_recv_id,
			"csm_buff_in_recv", NULL, NULL);
	if (ret)
		goto error;

	ret = nvadsp_mbox_open(&drv->csm_mbox_buf_out_recv,
			&csm_sm->mbox_buf_out_recv_id,
			"csm_buff_out_recv", NULL, NULL);
	if (ret)
		goto error;

	dev_info(dev, "csm_recv_id:%d\n", csm_sm->mbox_csm_recv_id);
	dev_info(dev, "csm_send_id:%d\n", csm_sm->mbox_csm_send_id);
	dev_info(dev, "buf_in_send_id:%d\n", csm_sm->mbox_buf_in_send_id);
	dev_info(dev, "buf_out_send_id:%d\n", csm_sm->mbox_buf_out_send_id);
	dev_info(dev, "buf_in_recv_id:%d\n", csm_sm->mbox_buf_in_recv_id);
	dev_info(dev, "buf_out_recv_id:%d\n", csm_sm->mbox_buf_out_recv_id);
	dev_info(dev, "CSM SharedMem %ld\n", sizeof(struct csm_sm_state_t));

	drv->csm_sm = csm_sm;
	return ret;
error:
	dev_err(dev, "%s: failed with ret:%d\n", __func__, ret);
	return ret;
}

status_t acsl_csm_cmd_send(struct acsl_drv *drv, uint32_t cmd,
	uint32_t flags, bool block, bool ack)
{
	struct device *dev = drv->dev;
	uint32_t data;
	status_t ret;

	ret = nvadsp_mbox_send(&drv->csm_mbox_recv, cmd,
			NVADSP_MBOX_SMSG, flags, ACSL_TIMEOUT);
	if (ret) {
		dev_err(dev, "%s: failed with ret:%d\n", __func__, ret);
		return ret;
	}

	if (ack) {
		ret = nvadsp_mbox_recv(&drv->csm_mbox_send, &data,
				block, ACSL_TIMEOUT);
		if (ret)
			dev_err(dev, "CSM mailbox recv timed out\n");
		switch (data) {
		case ACK:
			break;
		default:
			dev_err(dev,
				"failed to recv ACK\n");
			ret = -EINVAL;
			break;
		}
	}

	return ret;
}

uint8_t acsl_acq_buf(struct acsl_drv *drv, struct acsl_buf_args_t *buf_args, uint8_t PORT)
{
	struct device *dev = drv->dev;
	uint8_t COMP_ID, buff_indx = UINT8_MAX;

	COMP_ID = buf_args->comp_id;
	log_buf_info(dev, __func__, COMP_ID, PORT, buff_indx,
		buf_args->block);

	if (is_free_index_avail(drv, COMP_ID, PORT, buf_args->block)) {
		mutex_lock(&drv->port_lock[PORT][COMP_ID]);
		buff_indx =
			buffer_index_wrap(drv->m_rel_buf_index[PORT][COMP_ID]);
		mutex_unlock(&drv->port_lock[PORT][COMP_ID]);
		dev_dbg(dev, "%s: Comp_ID:%d, PORT:%d, buff_indx:%d\n",
			__func__, COMP_ID, PORT, buff_indx);
	}

	return buff_indx;
}

uint8_t acsl_rel_buf(struct acsl_drv *drv, struct acsl_buf_args_t *buf_args, uint8_t PORT)
{
	struct device *dev = drv->dev;
	status_t ret;
	uint8_t COMP_ID, buf_index = buf_args->buf_index;

	COMP_ID = buf_args->comp_id;
	log_buf_info(dev, __func__, COMP_ID, PORT, buf_index,
		buf_args->block);
	mutex_lock(&drv->port_lock[PORT][COMP_ID]);
	buf_index = buffer_index_wrap(drv->m_rel_buf_index[PORT][COMP_ID]);
	drv->m_rel_buf_index[PORT][COMP_ID]++;
	ret = append_buf_to_csm(drv, PORT, COMP_ID);
	if (ret)
		buf_index = UINT8_MAX;
	mutex_unlock(&drv->port_lock[PORT][COMP_ID]);

	drv->append_init_input_buff[COMP_ID] = true;

	return buf_index;
}

status_t acsl_comp_close(struct acsl_drv *drv, struct acsl_csm_args_t *csm_args)
{
	struct device *dev = drv->dev;
	status_t ret;
	uint8_t COMP_ID = csm_args->comp_id;

	send_message(drv, csm_args);

	ret = acsl_csm_cmd_send(drv, CSM_COMP_CLOSE_CMD, 0, true, true);
	if (ret) {
		dev_err(dev, "%s: failed with ret:%d\n", __func__, ret);
		return ret;
	}

	complete_all(&drv->buff_complete[IN_PORT][COMP_ID]);
	complete_all(&drv->buff_complete[OUT_PORT][COMP_ID]);

	return ret;
}

status_t acsl_comp_open(struct acsl_drv *drv, struct acsl_csm_args_t *csm_args)
{
	send_message(drv, csm_args);

	return acsl_csm_cmd_send(drv, CSM_COMP_OPEN_CMD, 0, true, true);
}

status_t acsl_intf_close(struct acsl_drv *drv, struct acsl_csm_args_t *csm_args)
{

	send_message(drv, csm_args);

	return acsl_csm_cmd_send(drv, CSM_INTF_CLOSE_CMD, 0, true, true);
}

status_t acsl_intf_open(struct acsl_drv *drv, struct acsl_csm_args_t *csm_args)
{
	send_message(drv, csm_args);

	return acsl_csm_cmd_send(drv, CSM_INTF_OPEN_CMD, 0, true, true);
}

status_t acsl_open(struct acsl_drv *drv, struct acsl_csm_args_t *csm_args)
{
	send_message(drv, csm_args);

	return acsl_csm_cmd_send(drv, CSM_INIT_CMD, 0, true, true);
}

status_t acsl_close(struct acsl_drv *drv)
{
	return acsl_csm_cmd_send(drv, CSM_DEINIT_CMD, 0, false, false);
}
