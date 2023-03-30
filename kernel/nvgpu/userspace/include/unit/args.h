/*
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __UNIT_ARGS_H__
#define __UNIT_ARGS_H__

#include <stdbool.h>

/*
 * Allow defaults to be changed at compile time.
 */
#define __stringify(x)			#x
#define stringify(x)			__stringify(x)

#ifndef __DEFAULT_ARG_DRIVER_LOAD_PATH
#if defined(__NVGPU_POSIX__)
#define __DEFAULT_ARG_DRIVER_LOAD_PATH	./build/libnvgpu-drv-igpu.so
#else
#define __DEFAULT_ARG_DRIVER_LOAD_PATH	./libnvgpu-drv-igpu.so
#endif
#endif
#define DEFAULT_ARG_DRIVER_LOAD_PATH	stringify(__DEFAULT_ARG_DRIVER_LOAD_PATH)

#ifndef __DEFAULT_ARG_UNIT_LOAD_PATH
#define __DEFAULT_ARG_UNIT_LOAD_PATH	build/units
#endif
#define DEFAULT_ARG_UNIT_LOAD_PATH	stringify(__DEFAULT_ARG_UNIT_LOAD_PATH)
#define TEST_PLAN_MAX 1

struct unit_fw;

struct unit_fw_args {
	bool		 help;
	int		 verbose_lvl;
	bool		 no_color;
	int		 thread_count;
	bool		 nvtest;
	bool		 is_qnx;
	unsigned int	 test_lvl;
	bool		 debug;
	const char	*binary_name;

	const char	*driver_load_path;

	const char	*unit_name;
	const char	*unit_load_path;
	const char	*unit_to_run;
	const char	*required_tests_file;
};

int core_parse_args(struct unit_fw *fw, int argc, char **argv);
void core_print_help(struct unit_fw *fw);

/*
 * Convenience for getting the args struct pointer.
 */
#define args(fw)		((fw)->args)

#endif
