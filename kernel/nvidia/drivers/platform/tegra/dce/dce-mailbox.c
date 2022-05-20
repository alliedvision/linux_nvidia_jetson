/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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
#include <dce-mailbox.h>
#include <dce-util-common.h>
#include <interface/dce-interface.h>
#include <interface/dce-boot-cmds.h>


#define CCPLEX_HSP_IE 1U /* TODO : Have an api to read from platform data */
#define DCE_MAILBOX_FULL_INT_SHIFT 8U

/**
 * dce_hsp_get_irq_sources - gets the interrupt sources.
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : bitmap for mailbox ids that triggered the irqs.
 */
static u32 dce_hsp_get_irq_sources(struct tegra_dce *d)
{
	return (dce_hsp_ie_read(d, CCPLEX_HSP_IE) &
					dce_hsp_ir_read(d));
}

/**
 * dce_mailbox_isr - Isr for mailbox irqs.
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : Void
 */
void dce_mailbox_isr(struct tegra_dce *d)
{
	u8 i = 0;
	u32 value;
	struct dce_mailbox_interface *d_mb;
	u32 irq_sources = dce_hsp_get_irq_sources(d);

	do {
		d_mb = &d->d_mb[i];
		/**
		 * Get the mailbox on which the interrupt
		 * is received.
		 */
		if (irq_sources & (BIT(d_mb->r_mb)
			<< DCE_MAILBOX_FULL_INT_SHIFT)) {
			/**
			 * Read and store the value.
			 *
			 * TODO : Ignore the full interrupt
			 * bit before storing the result.
			 *
			 */
			value = dce_smb_read(d, d_mb->r_mb);
			dce_smb_set(d, 0U, d_mb->r_mb);
			dce_mailbox_store_interface_status(d, value, i);
			d_mb->notify(d, d_mb->notify_data);
		}
		i++;
	} while (i < DCE_MAILBOX_MAX_INTERFACES);
}

/**
 * dce_mailbox_store_interface_status - stores the response
 *				received on a mailbox interface.
 * @d : Pointer to tegra_dce struct.
 * @v : Value to be stored.
 * @id : interface id.
 *
 * Return :Void
 */
void dce_mailbox_store_interface_status(struct tegra_dce *d, u32 v, u8 id)
{
	struct dce_mailbox_interface *d_mb = &d->d_mb[id];

	dce_mutex_lock(&d_mb->lock);
	d_mb->ack_value = v;
	d_mb->valid = true;
	dce_mutex_unlock(&d_mb->lock);
}

/**
 * dce_mailbox_get_interface_status - gets the response
 *					received on mailbox interface.
 * @d : Pointer to tegra_dce struct.
 * @id : Interface id.
 *
 * Return : u32 value
 */
u32 dce_mailbox_get_interface_status(struct tegra_dce *d, u8 id)
{
	struct dce_mailbox_interface *d_mb = &d->d_mb[id];

	if (d_mb->valid)
		return d_mb->ack_value;
	else
		return 0xffffffff;
}

/**
 * dce_mailbox_invalidate_status - renders the response invalid.
 *
 * @d : Pointer to tegra_dce struct.
 * @id : Interface id.
 *
 * Return : void
 */
void dce_mailbox_invalidate_status(struct tegra_dce *d, u8 id)
{
	struct dce_mailbox_interface *d_mb = &d->d_mb[id];

	dce_mutex_lock(&d_mb->lock);
	d_mb->valid = false;
	dce_mutex_unlock(&d_mb->lock);
}

/**
 * dce_mailbox_write_safe - Checks if it's safe to write to
 *						a mailbox register.
 *
 * @d : Pointer to tegra_dce struct.
 * @id : Mailbox ID
 *
 * Return : true if it's safe
 */
static bool dce_mailbox_write_safe(struct tegra_dce *d, u8 id)
{
	unsigned long val;

	val = dce_smb_read(d, id);

	return !(val & BIT(31));
}

/**
 * dce_mailbox_set_full_interrupt - Sets the interrupt tag bit
 *					in the mailbox register
 * @d : Pointer to tegra_dce struct.
 * @id : Mailbox interface id.
 *
 * Return : Void
 */
void dce_mailbox_set_full_interrupt(struct tegra_dce *d, u8 id)
{
	struct dce_mailbox_interface *d_mb;

	d_mb = &d->d_mb[id];

	dce_mutex_lock(&d_mb->lock);

	if (!dce_mailbox_write_safe(d, d_mb->s_mb))
		dce_info(d, "Intr bit set multiple times for MB : [0x%x]",
			 d_mb->s_mb);

	dce_smb_set(d, BIT(31), d_mb->s_mb);

	dce_mutex_unlock(&d_mb->lock);
}

/**
 * dce_mailbox_send_cmd_sync - Sends command via mailbox and waits for ack.
 *
 * @d : Pointer to tegra_dce struct.
 * @cmd : The command to be sent.
 * @interface  : boot or admin interface
 *
 * Return : 0 if successful.
 */
int dce_mailbox_send_cmd_sync(struct tegra_dce *d, u32 cmd, u32 interface)
{
	int ret = 0;
	struct dce_mailbox_interface *d_mb;

	d_mb = &d->d_mb[interface];

	dce_mutex_lock(&d_mb->lock);

	if (!dce_mailbox_write_safe(d, d_mb->s_mb)) {
		dce_err(d, "Previously sent message isn't synced");
		return -1;
	}

	dce_smb_set(d, cmd | BIT(31), d_mb->s_mb);
	d_mb->valid = false;

	dce_mutex_unlock(&d_mb->lock);

	ret = d_mb->dce_mailbox_wait(d);

	return ret;
}

/**
 * dce_mailbox_init_interface - Initializes the mailbox interface.
 *
 * @d : Pointer to tegra_dce struct.
 * @id : Mailbox interface id.
 *
 * Return : 0 if successful
 */
int dce_mailbox_init_interface(struct tegra_dce *d, u8 id, u8 s_mb,
		u8 r_mb, int (*dce_mailbox_wait)(struct tegra_dce *),
		void *notify_data, void (*notify)(struct tegra_dce *, void *))
{
	int ret;
	u64 ie_wr_val;
	struct dce_mailbox_interface *d_mb;

	d_mb = &d->d_mb[id];

	ret = dce_mutex_init(&d_mb->lock);
	if (ret) {
		dce_err(d, "dce lock initialization failed for mailbox");
		goto err_lock_init;
	}

	d_mb->valid = false;

	dce_smb_set_full_ie(d, true, r_mb);

	ie_wr_val = BIT(r_mb) << 8U;
	dce_hsp_ie_write(d, ie_wr_val, CCPLEX_HSP_IE);

	d_mb->s_mb = s_mb;
	d_mb->r_mb = r_mb;

	d_mb->notify = notify;
	d_mb->notify_data = notify_data;

	d_mb->dce_mailbox_wait
		= dce_mailbox_wait;

	return 0;

err_lock_init:
	return ret;
}

/**
 * dce_mailbox_deinit_interface - Releases the resources
 *					associated with boot interface.
 *
 * @d : Pointer to tegra_dce struct.
 *
 * Return : Void
 */
void dce_mailbox_deinit_interface(struct tegra_dce *d, u8 id)
{
	struct dce_mailbox_interface *d_mb;

	d_mb = &d->d_mb[id];

	dce_mutex_destroy(&d_mb->lock);
}
