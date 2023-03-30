/*
 * Copyright (c) 2016-2020 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/linkage.h>
#include <linux/version.h>

/* uint32_t invoke_smc(uint32_t arg0, uintptr_t arg1, uintptr_t arg2); */

#ifdef CONFIG_ARM64

#if KERNEL_VERSION(5,8,0) > LINUX_VERSION_CODE
ENTRY(invoke_smc)
#else
SYM_CODE_START(invoke_smc)
#endif
	smc	#0
	ret
#if KERNEL_VERSION(5,8,0) > LINUX_VERSION_CODE
ENDPROC(invoke_smc)
#else
SYM_CODE_END(invoke_smc)
#endif

#else
ENTRY(invoke_smc)
        smc     #0
        mov     pc, lr
ENDPROC(invoke_smc)
#endif
