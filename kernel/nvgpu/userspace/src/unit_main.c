/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
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

/**
 * NvGpu unit testing framework!
 */

#include <stdlib.h>
#include <string.h>

#include <unit/io.h>
#include <unit/core.h>
#include <unit/args.h>
#include <unit/module.h>
#include <unit/results.h>
#include <unit/required_tests.h>

int main(int argc, char **argv)
{
	struct unit_fw *fw;
	int ret;

	fw = malloc(sizeof(*fw));
	if (!fw)
		return 1;

	memset(fw, 0, sizeof(*fw));

	ret = core_parse_args(fw, argc, argv);
	if (ret) {
		core_err(fw, "Unable to parse args.\n");
		core_err(fw, "Exiting!\n");
		return 1;
	}

	core_vbs(fw, 1, "Welcome to the nvgpu unit testing framework!\n");

	if (args(fw)->help) {
		core_print_help(fw);
		return 1;
	}

	ret = core_load_nvgpu(fw);
	if (ret != 0)
		return ret;

	fw->modules = core_load_modules(fw);
	if (fw->modules == NULL)
		return -1;

	ret = core_exec(fw);
	if (ret != 0)
		return ret;

	if (fw->results == NULL) {
		core_msg(fw, "No tests were run!\n");
		return -1;
	}

	core_print_test_status(fw);

	if ((fw->results->nr_tests - fw->results->nr_passing -
					fw->results->nr_skipped) != 0) {
		/* Some tests failed */
		return -1;
	}

	if (fw->args->unit_to_run != NULL) {
		/*
		 * Just in case (especially when running in automation), return
		 * a failure if only executed a subset of the units
		 */
		return -2;
	}

	if (fw->args->required_tests_file != NULL) {
		ret = parse_req_file(fw, fw->args->required_tests_file);
		if (ret != 0) {
			core_err(fw,
				"Failed to load the required tests file.\n");
			return -1;
		}

		ret = check_executed_tests(fw);
		if (ret != 0) {
			core_err(fw,
				"Found %d required tests that were not run!\n",
				ret);
			return -1;
		}
	}

	return 0;
}
