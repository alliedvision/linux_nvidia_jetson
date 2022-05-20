/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_FIFO_PROFILE_H
#define NVGPU_FIFO_PROFILE_H

/*
 * Define these here, not in the C file so that they are closer to the other
 * macro definitions below. The two lists must be in sync.
 */
#define NVGPU_FIFO_KICKOFF_PROFILE_EVENTS	\
	"ioctl_entry",				\
	"entry",				\
	"job_tracking",				\
	"append",				\
	"end",					\
	"ioctl_exit",				\
	NULL					\

/*
 * The kickoff profile events; these are used to index into the profile's sample
 * array.
 */
#define PROF_KICKOFF_IOCTL_ENTRY		0U
#define PROF_KICKOFF_ENTRY			1U
#define PROF_KICKOFF_JOB_TRACKING		2U
#define PROF_KICKOFF_APPEND			3U
#define PROF_KICKOFF_END			4U
#define PROF_KICKOFF_IOCTL_EXIT			5U

/*
 * Recovery profiling - this covers the gv11b+ recovery sequence; the older recovery
 * sequence is very different.
 */
#define NVGPU_FIFO_RECOVERY_PROFILE_EVENTS	\
	"acq_active_rl",			\
	"get_rl_mask",				\
	"disable_rl",				\
	"disable_tsg",				\
	"preempt_rl",				\
	"poll_tsg_on_pbdma",			\
	"engines_reset",			\
	"enable_rl",				\
	"done",					\
	NULL

#define PROF_RECOVERY_ACQ_ACTIVE_RL		0U
#define PROF_RECOVERY_GET_RL_MASK		1U
#define PROF_RECOVERY_DISABLE_RL		2U
#define PROF_RECOVERY_DISABLE_TSG		3U
#define PROF_RECOVERY_PREEMPT_RL		4U
#define PROF_RECOVERY_POLL_TSG_ON_PBDMA		5U
#define PROF_RECOVERY_ENGINES_RESET		6U
#define PROF_RECOVERY_ENABLE_RL			7U
#define PROF_RECOVERY_DONE			8U

/*
 * Engine reset profiling - essentially part of the recovery sequence. This
 * is a big portion of the time spent in recovery.
 */
#define NVGPU_FIFO_ENGINE_RESET_EVENTS		\
	"preamble",				\
	"elpg_disable",				\
	"fecs_trace_reset",			\
	"halt_pipeline",			\
	"gr_reset",				\
	"elpg_reenable",			\
	NULL

#define PROF_ENG_RESET_PREAMBLE			0U
#define PROF_ENG_RESET_ELPG_DISABLE		1U
#define PROF_ENG_RESET_FECS_TRACE_RESET		2U
#define PROF_ENG_RESET_HALT_PIPELINE		3U
#define PROF_ENG_RESET_GR_RESET			4U
#define PROF_ENG_RESET_ELPG_REENABLE		5U

#endif
