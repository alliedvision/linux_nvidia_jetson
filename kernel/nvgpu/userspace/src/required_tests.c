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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unit/io.h>
#include <unit/core.h>
#include <unit/unit.h>
#include <unit/required_tests.h>
#include <unit/results.h>
#include <nvgpu/list.h>

struct test_list_node {
	char *test_subtest_name;
	long test_level;
	struct nvgpu_list_node list;
};
struct unit_list_node {
	char *unit_name;
	struct nvgpu_list_node test_list_head;
	struct nvgpu_list_node list;
};

static struct nvgpu_list_node unit_list_head;

static inline struct test_list_node *
test_list_node_from_list(struct nvgpu_list_node *node)
{
	return (struct test_list_node *)
		((uintptr_t)node - offsetof(struct test_list_node, list));
};
static inline struct unit_list_node *
unit_list_node_from_list(struct nvgpu_list_node *node)
{
	return (struct unit_list_node *)
		((uintptr_t)node - offsetof(struct unit_list_node, list));
};

/*
 * Add a unit in the list of units. The node will hold the name of the unit and
 * another list of tests.
 */
static struct unit_list_node *add_unit(struct nvgpu_list_node *head,
	const char *unit_name)
{
	struct unit_list_node *node = malloc(sizeof(struct unit_list_node));

	if (node == NULL) {
		return NULL;
	}

	node->unit_name = malloc(strlen(unit_name)+1);
	if (node->unit_name == NULL) {
		free(node);
		return NULL;
	}

	strcpy(node->unit_name, unit_name);
	nvgpu_init_list_node(&node->test_list_head);
	nvgpu_list_add(&node->list, head);

	return node;
}

/*
 * From a unit list node, add a test_subtest string along with its test level.
 */
static struct test_list_node *add_test_subtest_level(
	struct unit_list_node *unit, const char *test_subtest_name, long level)
{
	struct test_list_node *node = malloc(sizeof(struct test_list_node));

	if (node == NULL) {
		return NULL;
	}

	node->test_level = level;
	node->test_subtest_name = malloc(strlen(test_subtest_name)+1);
	if (node->test_subtest_name == NULL) {
		free(node);
		return NULL;
	}

	strcpy(node->test_subtest_name, test_subtest_name);

	nvgpu_list_add(&node->list, &unit->test_list_head);

	return node;
}

/*
 * Simple helper to sanitize the input and remove un-needed characters.
 */
static void sanitize(char *dest, const char *src)
{
	char *dest_start = dest;

	if (dest == NULL || src == NULL)
		return;

	/* Trim leading spaces */
	while (*src == ' ' || *src == '\t')
		src++;

	/* Copy the rest of the string */
	while (*src != '\0') {
		if (*src == '#') {
			/* Rest of the line is a comment, discard it */
			break;
		}
		*dest++ = *src++;
	}
	*dest = '\0';

	/* Backtrack to remove line returns and trim spaces at the end */
	while (dest_start <= dest) {
		if ((*dest == '\0') || (*dest == '\r') || (*dest == '\n')
			|| (*dest == ' ')) {
			*dest = '\0';
		} else {
			return;
		}
		dest--;
	}
}

/* Parse a sting looking for a long number, along with error handling. */
static bool parse_long(const char *str, long *val)
{
	char *tmp;
	bool result = true;
	errno = 0;
	*val = strtol(str, &tmp, 10);

	if (tmp == str || *tmp != '\0' ||
		((*val == LONG_MIN || *val == LONG_MAX) && errno == ERANGE)) {
		result = false;
	}

	return result;
}

/*
 * Load the INI file that contains the list of required tests. This manages a
 * list of units, and each unit node has a list of test cases.
 */
int parse_req_file(struct unit_fw *fw, const char *ini_file)
{
	char rawline[MAX_LINE_SIZE];
	char line[MAX_LINE_SIZE];
	char tmp[MAX_LINE_SIZE];
	char *open_bracket, *close_bracket, *equal;
	long value, len;
	struct unit_list_node *current_unit = NULL;
	FILE *file = fopen(ini_file, "r");

	if (file == NULL) {
		perror("Error reading INI file: ");
		return -1;
	}

	nvgpu_init_list_node(&unit_list_head);

	while (fgets(rawline, sizeof(rawline), file)) {
		line[0] = '\0';
		tmp[0] = '\0';
		sanitize(line, rawline);
		if (strlen(line) == 0) {
			continue;
		}
		open_bracket = strchr(line, '[');
		close_bracket = strchr(line, ']');
		equal = strchr(line, '=');

		if (equal != NULL) {
			/* This is a key/value pair */
			if (current_unit == NULL) {
				continue;
			}
			strncpy(tmp, line, (equal-line));
			tmp[equal-line] = '\0';
			if (!parse_long(equal+1, &value)) {
				core_err(fw, "Conversion error:\n%s\n",
					rawline);
				return -1;
			}
			if (add_test_subtest_level(current_unit, tmp, value)
				== NULL) {
					return -ENOMEM;
				}
		} else if ((open_bracket != NULL) && (close_bracket != NULL)) {
			/* This is a section */
			if (open_bracket++ > close_bracket) {
				continue;
			}
			len = close_bracket-open_bracket;
			strncpy(tmp, open_bracket, len);
			tmp[len] = '\0';
			current_unit = add_unit(&unit_list_head, tmp);
			if (current_unit == NULL) {
				return -ENOMEM;
			}
		} else {
			/* Unknown line or syntax error */
			core_err(fw, "Syntax error parsing:\n%s\n", rawline);
			return -1;
		}
	}

	fclose(file);

	return 0;
}

/*
 * Helper that takes a test function name and a subcase name, combines them to
 * be in the same format as the INI file, and compare the result to a given
 * string. Returns true if it matches.
 */
static bool cmp_test_name(const char *exec_fn_name, const char* exec_case_name,
	const char *ini_test_subtest_name)
{
	char exec_test_subtest_name[MAX_LINE_SIZE];

	exec_test_subtest_name[0] = '\0';
	strcat(exec_test_subtest_name, exec_fn_name);
	strcat(exec_test_subtest_name, ".");
	strcat(exec_test_subtest_name, exec_case_name);

	if (strcmp(ini_test_subtest_name, exec_test_subtest_name) == 0) {
		return true;
	}

	return false;
}

/*
 * Check the tests that were executed and compare them to the list of tests
 * from the INI file. This only check passing tests; if there are failed tests,
 * the failure will be handled somewhere else, and skipped tests are handled by
 * the test level in the INI file.
 * Returns the number of required tests that were not executed.
 */
int check_executed_tests(struct unit_fw *fw)
{
	struct unit_test_record *rec;
	struct unit_list_node *unit_node, *found_unit_node = NULL;
	struct test_list_node *test_node, *found_test_node = NULL;
	struct unit_test_list *passing_tests = &fw->results->passing;
	int missing_count = 0;

	for_record_in_test_list(passing_tests, rec) {
		found_unit_node = NULL;
		found_test_node = NULL;
		/* Search for the unit name */
		nvgpu_list_for_each_entry(unit_node, &unit_list_head,
				unit_list_node, list) {
			if (strcmp(unit_node->unit_name, rec->mod->name) != 0) {
				continue;
			}

			found_unit_node = unit_node;
			/* Search for the fn_name.case_name */
			nvgpu_list_for_each_entry(test_node,
					&unit_node->test_list_head,
					test_list_node, list) {
				if (cmp_test_name(rec->test->fn_name,
						rec->test->case_name,
						test_node->test_subtest_name)) {
					found_test_node = test_node;
					break;
				}
			}
			break;
		}

		if (found_test_node == NULL) {
			/* Test should be added to the INI file */
			core_err(fw,
				"Test not in required tests: [%s] %s.%s\n",
				rec->mod->name, rec->test->fn_name,
				rec->test->case_name);
		} else {
			/* Found it, remove it from the list. */
			nvgpu_list_del(&found_test_node->list);
			if (nvgpu_list_empty(
					&found_unit_node->test_list_head)) {
				/* No more tests, remove the unit itself */
				nvgpu_list_del(&found_unit_node->list);
			}
		}
	}

	/*
	 * Now that all the executed tests were removed from the list, any test
	 * that is leftover is a required test that was not executed.
	 */
	nvgpu_list_for_each_entry(unit_node, &unit_list_head, unit_list_node,
					list) {
		nvgpu_list_for_each_entry(test_node, &unit_node->test_list_head,
						test_list_node, list) {
			if (test_node->test_level <= fw->args->test_lvl) {
				core_err(fw, "Required test not run: [%s] %s\n",
					unit_node->unit_name,
					test_node->test_subtest_name);
				missing_count++;
			}
		}
	}

	return missing_count;
}

