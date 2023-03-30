/*
 * Copyright (c) 2016-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef DCE_INTERFACE_H
#define DCE_INTERFACE_H

#include <interface/dce-bitops.h>

/*
 * XXX: TODO
 *
 * These should be defined in terms of the HW registers
 */
#define DCE_NUM_SEMA_REGS		4
#define DCE_NUM_MBOX_REGS		8

/*
 * Symbolic definitions of the semaphore registers
 */
typedef uint32_t				hsp_sema_t;

#define DCE_BOOT_SEMA			(hsp_sema_t)0U

/*
 * Definitions for DCE_BOOT_SEMA
 *
 * Used to communicate bits of information between the OS and DCE
 */

/*
 * Bits set by the OS and examined by the R5
 */
#define DCE_BOOT_INT		DCE_BIT(31)	// interrupt when DCE is ready
#define DCE_WAIT_DEBUG		DCE_BIT(30)	// wait in debug loop
#define DCE_SC7_RESUME		DCE_BIT(29)	// resume using saved SC7 state
						// rather than a full restart
#define DCE_OS_BITMASK		(DCE_BOOT_INT | DCE_WAIT_DEBUG | DCE_SC7_RESUME)

/*
 * Bits set by the R5 and examined by the OS
 */
#define DCE_BOOT_TCM_COPY		DCE_BIT(15)	// uCode has copied to TCM
#define DCE_BOOT_HW_INIT		DCE_BIT(14)	// hardware init complete
#define DCE_BOOT_MPU_INIT		DCE_BIT(13)	// MPU initialized
#define DCE_BOOT_CACHE_INIT		DCE_BIT(12)	// cache initialized
#define DCE_BOOT_R5_INIT		DCE_BIT(11)	// R5 initialized
#define DCE_BOOT_DRIVER_INIT		DCE_BIT(10)	// driver init complete
#define DCE_BOOT_MAIN_STARTED		DCE_BIT(9)	// main started
#define DCE_BOOT_TASK_INIT_START	DCE_BIT(8)	// task initialization started
#define DCE_BOOT_TASK_INIT_DONE		DCE_BIT(7)	// task initialization complete

#define DCE_HALTED			DCE_BIT(1)	// uCode has halted
#define DCE_BOOT_COMPLETE		DCE_BIT(0)	// uCode boot has completed

/*
 * Symbolic definitions of the doorbell registers
 */
	typedef uint32_t			hsp_db_t;

/*
 * Symbolic definitions of the mailbox registers (rather than using 0-7)
 */
typedef uint32_t				hsp_mbox_t;

#define DCE_MBOX_FROM_DCE_RM			(hsp_mbox_t)0U	// signal from RM IPC
#define DCE_MBOX_TO_DCE_RM			(hsp_mbox_t)1U	// signal to RM IPC
#define DCE_MBOX_FROM_DCE_RM_EVENT_NOTIFY	(hsp_mbox_t)2U	// signal to DCE for event notification
#define DCE_MBOX_TO_DCE_RM_EVENT_NOTIFY		(hsp_mbox_t)3U	// signal from DCE for event notification IPC
#define DCE_MBOX_FROM_DCE_ADMIN			(hsp_mbox_t)4U	// signal from DCE ADMIN IPC
#define DCE_MBOX_TO_DCE_ADMIN			(hsp_mbox_t)5U	// signal to ADMIN IPC
#define DCE_MBOX_BOOT_CMD			(hsp_mbox_t)6U	// boot commands
#define DCE_MBOX_IRQ				(hsp_mbox_t)7U	// general interrupt/status
/*
 * Generic interrupts & status from the DCE are reported in DCE_MBOX_IRQ
 */
#define DCE_IRQ_PENDING			DCE_BIT(31)// interrupt is pending

#define DCE_IRQ_GET_STATUS_TYPE(_x_)	DCE_EXTRACT(_x_, 30, 27, uint32_t)
#define DCE_IRQ_SET_STATUS_TYPE(_x_)	DCE_INSERT(0U, 30, 27, _x_)

#define DCE_IRQ_STATUS_TYPE_IRQ			0x0		// irq status
#define DCE_IRQ_STATUS_TYPE_BOOT_CMD	0x1		// boot command status

#define NUM_DCE_IRQ_STATUS_TYPES		2

#define DCE_IRQ_GET_STATUS(_x_) DCE_EXTRACT(_x_, 23, 0, uint32_t)
#define DCE_IRQ_SET_STATUS(_x_) DCE_INSERT(0U, 23, 0, _x_)

/*
 * Bits in status field when IRQ_STATUS_TYPE == IRQ_STATUS_TYPE_IRQ
 */
#define DCE_IRQ_READY		DCE_BIT(23)	// DCE is ready
#define DCE_IRQ_LOG_OVERFLOW	DCE_BIT(22)	// trace log overflow
#define DCE_IRQ_LOG_READY	DCE_BIT(21)	// trace log buffers available
#define DCE_IRQ_CRASH_LOG	DCE_BIT(20)	// crash log available
#define DCE_IRQ_ABORT		DCE_BIT(19)	// uCode abort occurred
#define DCE_IRQ_SC7_ENTERED	DCE_BIT(18)	// DCE state saved
						// can be powered off

/*
 * MBOX contents for IPC are the same for all of the mailboxes that are
 * used for signaling IPC.	Not all values will be useful for all mailboxes.
 */
#define DCE_IPC_IRQ_PENDING	DCE_BIT(31)		// interrupt is pending

#endif
