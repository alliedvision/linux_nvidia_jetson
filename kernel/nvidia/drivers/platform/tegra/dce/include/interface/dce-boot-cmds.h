/*
 * Copyright (c) 2018-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef DCE_BOOT_CMDS_H
#define DCE_BOOT_CMDS_H

#include <interface/dce-bitops.h>

/*
 * Version of the bootstrap command interface.
 *
 * This MUST be updated any time any changes are made to the
 * bootstrap commands.
 *
 * To keep things simple, this value should be incremented by 1
 * each time changes are made.
 */
#define DCE_BOOT_CMD_VERSION_NUM		2

/*
 * Defines the various bootstrap commands to DCE.
 *
 * These commands are relatively simple and are mainly used to
 * communicate with DCE during initialization.
 *
 * The fundamental layout of a command is:
 * Bit(s)	Field		Description
 * 31:31	GO		Signals to the DCE that a command is to be
 *				processed
 * 30:27	COMMAND		Identifies the command that the DCE is to
 *				process
 * 26		RESERVED	should be 0
 * 25		HILO		0 = PARM is 19:0 of address
 *				1 = PARM is 39:20 of address
 * 24		RDWR		0 = read header
 *				1 = write header
 * 23:20	RESERVED	should be 0
 * 19:0		PARM		Parameter to the command
 *
 * Once the command has been processed and the CCPLEX receives an interrupt
 * from DCE, the mailbox used will contain any information about the result
 * of the command.
 *
 * The commands are:
 *
 * DCE_BOOT_CMD_VERSION		returns the version of the interface
 * DCE_BOOT_CMD_SET_SID		sets the SID for a buffer
 * DCE_BOOT_CMD_CHANNEL_INIT	initialize an IVC channel
 * DCE_BOOT_CMD_SET_ADDR	set the channel address
 * DCE_BOOT_CMD_GET_FSIZE	get the size of the frame
 * DCE_BOOT_CMD_SET_NFRAMES	set the number of frames
 * DCE_BOOT_CMD_RESET		causes DCE to reset to its initial state.
 *				This does not cause DCE to reboot. It mearly
 *				indicates that all of memory buffers for IPC
 *				will be ignored and the CCPLEX will have to
 *				re-establish the memory again.
 *
 * DCE_BOOT_CMD_LOCK		locks the admin command interface. Requires
 *				a full reset of DCE to unlock.
 */
#define DCE_BOOT_CMD_GO			DCE_BIT(31)
#define DCE_BOOT_CMD_SET(_x_, _v_)	DCE_INSERT(_x_, 30, 27, _v_)
#define DCE_BOOT_CMD_GET(_x_)		DCE_EXTRACT(_x_, 30, 27, uint32_t)
#define DCE_BOOT_CMD_SET_HILO(_x_, _v_) DCE_INSERT(_x_, 25, 25, _v_)
#define DCE_BOOT_CMD_GET_HILO(_x_)	DCE_EXTRACT(_x_, 25, 25, uint32_t)
#define DCE_BOOT_CMD_SET_RDWR(_x_, _v_) DCE_INSERT(_x_, 24, 24, _v_)
#define DCE_BOOT_CMD_GET_RDWR(_x_)	DCE_EXTRACT(_x_, 24, 24, uint32_t)
#define DCE_BOOT_CMD_PARM_SET(_x_, _v_) DCE_INSERT(_x_, 19, 0, _v_)
#define DCE_BOOT_CMD_PARM_GET(_x_)	DCE_EXTRACT(_x_, 19, 0, uint32_t)

/*
 * Commands
 */
#define DCE_BOOT_CMD_VERSION			(0x00U)
#define DCE_BOOT_CMD_SET_SID			(0x01U)
#define DCE_BOOT_CMD_CHANNEL_INIT		(0x02U)
#define DCE_BOOT_CMD_SET_ADDR			(0x03U)
#define DCE_BOOT_CMD_GET_FSIZE			(0x04U)
#define DCE_BOOT_CMD_SET_NFRAMES		(0x05U)
#define DCE_BOOT_CMD_RESET			(0x06U)
#define DCE_BOOT_CMD_LOCK			(0x07U)
#define DCE_BOOT_CMD_SET_AST_LENGTH		(0x08U)
#define DCE_BOOT_CMD_SET_AST_IOVA		(0x09U)
#define DCE_BOOT_CMD_SET_FSIZE			(0x0AU)
#define DCE_BOOT_CMD_UNUSED_11			(0x0BU)
#define DCE_BOOT_CMD_UNUSED_12			(0x0CU)
#define DCE_BOOT_CMD_UNUSED_13			(0x0DU)
#define DCE_BOOT_CMD_UNUSED_14			(0x0EU)
#define DCE_BOOT_CMD_UNUSED_15			(0x0FU)
#define DCE_BOOT_CMD_NEXT			(0x10U)

/*
 * Boot Command Errors
 */
#define DCE_BOOT_CMD_ERR_FLAG			DCE_BIT(23)
#define DCE_BOOT_CMD_NO_ERROR			0U
#define DCE_BOOT_CMD_ERR_BAD_COMMAND		(1U | DCE_BOOT_CMD_ERR_FLAG)
#define DCE_BOOT_CMD_ERR_UNIMPLEMENTED		(2U | DCE_BOOT_CMD_ERR_FLAG)
#define DCE_BOOT_CMD_ERR_IPC_SETUP		(3U | DCE_BOOT_CMD_ERR_FLAG)
#define DCE_BOOT_CMD_ERR_INVALID_NFRAMES	(4U | DCE_BOOT_CMD_ERR_FLAG)
#define DCE_BOOT_CMD_ERR_IPC_CREATE		(5U | DCE_BOOT_CMD_ERR_FLAG)
#define DCE_BOOT_CMD_ERR_LOCKED			(6U | DCE_BOOT_CMD_ERR_FLAG)

#endif
