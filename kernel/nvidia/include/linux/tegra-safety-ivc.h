/*
 * Copyright (c) 2016-2021, NVIDIA CORPORATION, All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_TEGRA_SAFETY_IVC_H_
#define _LINUX_TEGRA_SAFETY_IVC_H_

#include <linux/tegra-ivc.h>
#include <linux/tegra-ivc-instance.h>

#define SAFETY_CONF_IVC_L2SS_READY     (SAFETY_CONF_IVC_READY << 24)
#define SAFETY_CONF(id, value)          ((SAFETY_CONF_ ## id << 24) | (value))
#define SAFETY_CONF_GET_ID(value)       (((value) >> 24) & 0x7f)
#define SAFETY_CONF_GET_VALUE(value)    ((value) & 0xffffff)
#define TEGRA_SAFETY_SM_CMDRESP_CH     0
#define TEGRA_SAFETY_IVC_READ_TIMEOUT	(2 * HZ)

enum {
	SAFETY_CONF_IVC_READY = 1,
};

#define MAX_SAFETY_CHANNELS		5

struct safety_ast_region {
	u8 ast_id;
	u32 slave_base;
	size_t size;
	void *base;
	dma_addr_t dma;
	struct device dev;
};

struct tegra_safety_ivc {
	struct safety_ast_region region;
	struct tegra_hsp_sm_pair *ivc_pair;
	struct {
		wait_queue_head_t response_waitq;
		wait_queue_head_t empty_waitq;
		atomic_t response;
		atomic_t emptied;
	} cmd;
	struct tegra_safety_ivc_chan *ivc_chan[MAX_SAFETY_CHANNELS];
	atomic_t ivc_ready;
	struct work_struct work;
	struct workqueue_struct *wq;
	struct mutex rlock;
	struct mutex wlock;
	struct l1ss_data *ldata;
};

struct tegra_safety_ivc_chan {
	struct ivc ivc;
	char *name;
	struct tegra_safety_ivc *safety_ivc;
};

int tegra_safety_dev_init(struct device *dev, int index);
void tegra_safety_dev_exit(struct device *dev, int index);
void tegra_safety_dev_notify(void);
struct tegra_safety_ivc_chan *tegra_safety_get_ivc_chan_from_str(
		struct tegra_safety_ivc *safety_ivc,
		char *ch_name);

#define CMDRESP_PAYLOAD_SIZE     56U
#define CMDRESP_PAYLOAD_EX_SIZE  248U

#pragma pack(push, 1)
typedef struct {
	uint8_t src;
	uint8_t dest;
	uint16_t cmd_opcode;
	uint8_t reserve;
} cmdresp_header_t;

typedef struct {
	uint16_t e2ecf1_crc; /*Profile 5 E2E Header*/
	uint8_t e2ecf2; /*E2E Profile 5 Counter*/
	cmdresp_header_t header; /*Structure containing Cmd Address*/
	uint8_t data[CMDRESP_PAYLOAD_SIZE]; /*Array containing CmdResp Payload*/
} cmdresp_frame_t;

typedef struct {
	uint16_t e2ecf1_crc; /*Profile 5 E2E Header*/
	uint8_t e2ecf2; /*E2E Profile 5 Counter*/
	cmdresp_header_t header; /*Structure containing Cmd Address*/
	uint8_t data[CMDRESP_PAYLOAD_EX_SIZE]; /*CmdResp Payload*/
} cmdresp_frame_ex_t;
#pragma pack(pop)

#define CMDRESP_MAX_ACTIVE_LIST_COUNT	20U
#define CMDRESP_CMD_FRAME_EX_SIZE	256

#endif
