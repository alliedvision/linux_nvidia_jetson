/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
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

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>

#include <unit/io.h>
#include <unit/core.h>
#include <unit/unit.h>
#include <unit/results.h>

/*
 * Mutex to ensure core_add_test_record() is thread safe.
 */
pthread_mutex_t mutex_results = PTHREAD_MUTEX_INITIALIZER;

static int __init_results(struct unit_fw *fw)
{
	struct unit_results *results;

	if (fw->results != NULL)
		return 0;

	results = malloc(sizeof(*results));
	if (results == NULL)
		return -1;

	memset(results, 0, sizeof(*results));

	fw->results = results;

	return 0;
}

static void add_record(struct unit_test_list *list,
		       struct unit_test_record *tr)
{
	/*
	 * First entry.
	 */
	if (list->head == NULL) {
		list->head = tr;
		list->last = tr;
		return;
	}

	/*
	 * Add to the end of the list and update the pointer to the last entry
	 * in the list. This gives us O(1) add time.
	 */
	list->last->next = tr;
	list->last = tr;
}

int core_add_test_record(struct unit_fw *fw,
			 struct unit_module *mod,
			 struct unit_module_test *test,
			 enum result_enum result)
{
	struct unit_test_record *tr;
	int err = 0;

	pthread_mutex_lock(&mutex_results);
	/*
	 * Does nothing if results are already inited.
	 */
	if (__init_results(fw) != 0) {
		err = -1;
		goto done;
	}

	tr = malloc(sizeof(*tr));
	if (tr == NULL) {
		err = -1;
		goto done;
	}

	tr->mod = mod;
	tr->test = test;
	tr->status = (result == PASSED);
	tr->next = NULL;

	fw->results->nr_tests += 1;
	switch (result) {
	case PASSED:
		add_record(&fw->results->passing, tr);
		fw->results->nr_passing += 1;
		break;
	case FAILED:
		add_record(&fw->results->failing, tr);
		break;
	case SKIPPED:
		add_record(&fw->results->skipped, tr);
		fw->results->nr_skipped += 1;
		break;
	}

done:
	pthread_mutex_unlock(&mutex_results);
	return err;
}

static void dump_test_record(FILE *logfile, struct unit_test_record *rec,
				bool status, bool first)
{
	first ? fprintf(logfile, "\t{") : fprintf(logfile, ",\n\t{");
	fprintf(logfile, "\"unit\": \"%s\", ", rec->mod->name);
	fprintf(logfile, "\"test\": \"%s\", ", rec->test->fn_name);
	fprintf(logfile, "\"case\": \"%s\", ", rec->test->case_name);
	fprintf(logfile, "\"status\": %s, ", status ? "true":"false");
	fprintf(logfile, "\"uid\": \"%s\", ", rec->test->jama.unique_id);
	fprintf(logfile, "\"vc\": \"%s\", ",
		rec->test->jama.verification_criteria);
	fprintf(logfile, "\"req\": \"%s\", ", rec->test->jama.requirement);
	fprintf(logfile, "\"test_level\": %d", rec->test->test_lvl);
	fprintf(logfile, "}");
}

static void dump_test_log(struct unit_fw *fw, struct unit_test_list
			*passing_tests, struct unit_test_list *failing_tests)
{
	struct unit_test_record *rec;
	int count = 0;
	FILE *logfile = fopen("results.json", "w+");

	fprintf(logfile, "[\n");
	for_record_in_test_list(passing_tests, rec) {
		dump_test_record(logfile, rec, true, count == 0);
		count++;
	}
	for_record_in_test_list(failing_tests, rec) {
		dump_test_record(logfile, rec, false, count == 0);
		count++;
	}
	fprintf(logfile, "\n]\n");
	fclose(logfile);
}

void core_print_test_status(struct unit_fw *fw)
{
	struct unit_test_list *failing_tests = &fw->results->failing;
	struct unit_test_list *skipped_tests = &fw->results->skipped;
	struct unit_test_record *rec;

	/*
	 * Print stats for the tests.
	 */
	core_msg(fw, "\n");
	core_msg(fw, "Test results:\n");
	core_msg(fw, "-------------\n");
	core_msg(fw, "\n");
	core_msg(fw, "  Skipped: %d\n", fw->results->nr_skipped);
	core_msg(fw, "  Passing: %d\n", fw->results->nr_passing);
	core_msg(fw, "  Failing: %d\n",
		 fw->results->nr_tests - fw->results->nr_passing -
						fw->results->nr_skipped);
	core_msg(fw, "  Total:   %d\n", fw->results->nr_tests);
	core_msg(fw, "\n");
	core_msg(fw, "Skipped tests:\n");
	core_msg(fw, "\n");
	for_record_in_test_list(skipped_tests, rec) {
		core_msg(fw, "  %s.%s(%s)\n",
			 rec->mod->name,
			 rec->test->fn_name,
			 rec->test->case_name);
	}

	core_msg(fw, "\n");
	core_msg(fw, "Failing tests:\n");
	core_msg(fw, "\n");

	for_record_in_test_list(failing_tests, rec) {
		core_msg(fw, "  %s.%s(%s)\n",
			 rec->mod->name,
			 rec->test->fn_name,
			 rec->test->case_name);
	}

	dump_test_log(fw, &fw->results->passing, &fw->results->failing);
}
