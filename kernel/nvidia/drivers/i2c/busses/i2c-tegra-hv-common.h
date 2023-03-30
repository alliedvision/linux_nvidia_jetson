/*
 * Copyright (c) 2015-2019, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __I2C_LIB_HV_H__
#define __I2C_LIB_HV_H__

#ifdef CONFIG_TEGRA_HV_MANAGER
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/tegra-ivc.h>
#include <linux/list.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>

/*
 * struct i2c_virt_msg - an I2C transaction segment
 * @addr: Slave address, either seven or ten bits.  When this is a ten
 *  bit address, I2C_M_TEN must be set in @flags and the adapter
 *  must support I2C_FUNC_10BIT_ADDR.
 * @flags: I2C_M_RD is handled by all adapters.  No other flags may be
 *  provided unless the adapter exported the relevant I2C_FUNC_*
 *  flags through i2c_check_functionality().
 * @len: Number of data bytes in @buf being read from or written to the
 *  I2C slave address.  For read transactions where I2C_M_RECV_LEN
 *  is set, the caller guarantees that this buffer can hold up to
 *  32 bytes in addition to the initial length byte sent by the
 *  slave (plus, if used, the SMBus PEC); and this value will be
 *  incremented by the number of block data bytes received.
 * @buf: The buffer into which data is read, or from which it's written.
*/

struct i2c_virt_msg {
	__u16 addr;	/* slave address			*/
	__u16 flags;
#define I2C_M_TEN		0x0010	/* this is a ten bit chip address */
#define I2C_M_RD		0x0001	/* read data, from slave to master */
#define I2C_M_STOP		0x8000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_NOSTART		0x4000	/* if I2C_FUNC_NOSTART */
#define I2C_M_REV_DIR_ADDR	0x2000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_IGNORE_NAK	0x1000	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_NO_RD_ACK		0x0800	/* if I2C_FUNC_PROTOCOL_MANGLING */
#define I2C_M_RECV_LEN		0x0400	/* length will be first received byte */
	__u16 len;		/* msg length				*/
	__u8 *buf;
};

struct i2c_ivc_msg_common {
	uint32_t err;
	int32_t count;
	uint32_t s_marker;
	uint32_t e_marker;
	int32_t comm_chan_id;
	uint32_t controller_instance;
};

struct i2c_ivc_frame {
	struct i2c_ivc_msg_common hdr;
	struct i2c_virt_msg virt_msg_array[];
};

#define I2C_IVC_FRAME_GET_FIRST_MSG_PTR(frame_ptr)  \
                (&(frame_ptr->virt_msg_array[0]))

#define I2C_IVC_FRAME_GET_NEXT_MSG_PTR(current_msg_ptr)  \
                ((struct i2c_virt_msg *)((char *)&current_msg_ptr->buf + current_msg_ptr->len))

typedef void (*i2c_isr_handler)(void *context);

struct tegra_hv_i2c_comm_chan;

int hv_i2c_transfer(struct i2c_ivc_frame *frame, struct tegra_hv_i2c_comm_chan
		*comm_chan, phys_addr_t base,
		struct i2c_msg *msgs, int count);
void hv_i2c_comm_chan_cleanup(struct tegra_hv_i2c_comm_chan *comm_chan);
void hv_i2c_comm_chan_free(struct tegra_hv_i2c_comm_chan *comm_chan);
void hv_i2c_comm_suspend(struct tegra_hv_i2c_comm_chan *comm_chan);
void hv_i2c_comm_resume(struct tegra_hv_i2c_comm_chan *comm_chan);
void *hv_i2c_comm_init(struct device *dev, i2c_isr_handler handler,
		void *data);
void tegra_hv_i2c_poll_cleanup(struct tegra_hv_i2c_comm_chan *comm_chan);
int hv_i2c_comm_chan_transfer_size(struct tegra_hv_i2c_comm_chan *comm_chan);

#ifdef I2C_DEBUG
void print_msg(struct i2c_msg *msg);
void print_frame(struct i2c_ivc_frame *i2c_ivc_frame, int num);
#endif

#define MAX_COMM_CHANS  10


enum i2c_rx_state_t {
	I2C_RX_INIT,
	I2C_RX_PENDING,
};

#define HV_I2C_FLAGS_HIGHSPEED_MODE	(1<<22)
#define HV_I2C_FLAGS_CONT_ON_NAK	(1<<21)
#define HV_I2C_FLAGS_SEND_START_BYTE	(1<<20)
#define HV_I2C_FLAGS_10BIT_ADDR		(1<<18)
#define HV_I2C_FLAGS_IE_ENABLE		(1<<17)
#define HV_I2C_FLAGS_REPEAT_START	(1<<16)
#define HV_I2C_FLAGS_CONTINUE_XFER	(1<<15)



#define I2C_IVC_COMMON_HEADER_LEN	sizeof(struct i2c_ivc_msg_common)

#define i2c_ivc_start_marker(_msg_ptr)	(_msg_ptr->hdr.s_marker)
#define i2c_ivc_end_marker(_msg_ptr)	(_msg_ptr->hdr.e_marker)
#define i2c_ivc_chan_id(_msg_ptr)	(_msg_ptr->hdr.comm_chan_id)
#define i2c_ivc_controller_instance(_msg_ptr)	\
					(_msg_ptr->hdr.controller_instance)
#define i2c_ivc_error_field(_msg_ptr)	(_msg_ptr->hdr.err)
#define i2c_ivc_count_field(_msg_ptr)	(_msg_ptr->hdr.count)

struct tegra_hv_i2c_comm_dev;

/* channel is virtual abstraction over a single ivc queue
 * each channel holds messages that are independent of other channels
 * we allocate one channel per i2c adapter as these can operate in parallel
 */
struct tegra_hv_i2c_comm_chan {
	struct tegra_hv_ivc_cookie *ivck;
	struct device *dev;
	int id;
	i2c_isr_handler handler;
	void *data;
	void *rcvd_data;
	enum i2c_rx_state_t rx_state;
	struct tegra_hv_i2c_comm_dev *hv_comm_dev;
	spinlock_t lock;
	int count;
};

struct tegra_hv_i2c_comm_dev {
	uint32_t queue_id;
	struct tegra_hv_ivc_cookie *ivck;
	spinlock_t ivck_tx_lock;
	spinlock_t lock;
	struct hlist_node list;
	struct work_struct work;
	struct tegra_hv_i2c_comm_chan *hv_comm_chan[MAX_COMM_CHANS];
};
#endif
#endif
