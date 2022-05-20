#!/usr/bin/python2
#
# Copyright (c) 2019-2021, NVIDIA CORPORATION.  All Rights Reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

import argparse, os
import json, md5

RESULTS_FILE = 'results.json'
HTML_FILE = 'results.html'
REQ_FILE = 'required_tests.json'

HTML_HEAD='''
<html>
	<head>
		<style>
		body {
		font-family: Verdana;
		}
		.res-table {
		border: solid 1px #DDD;
		border-collapse: collapse;
		border-spacing: 0; font-size: 12px;
		}
		.res-table thead th {
		background-color: #76B900;
		border: solid 1px #DDD;
		padding: 10px;
		text-align: left;
		}
		.res-table tbody td {
		border: solid 1px #DDD;
		padding: 10px;
		}
		h2 {
		color: #76B900;
		text-transfer: uppercase;
		}
		a[href*="jamacloud.com/"] {
		background:url(https://nvidia.jamacloud.com/img/favicon-16x16.png) no-repeat left center;
		padding-left:19px;
		}
		</style>
		<script src="https://ajax.googleapis.com/ajax/libs/jquery/3.3.1/jquery.min.js"></script>
		<script>
		$(document).ready(function() {
			$("#uid-toggle").click(function() {
				$(".invalid-uid").parent().toggle();
				return false;
			});
		});
		</script>
	</head>
	<script src="https://ajax.googleapis.com/ajax/libs/jquery/3.3.1/jquery.min.js"></script>
	<body>
	<h2>Results</h2>
'''

#Build a dictionary-based structure to easily access tests by unit and name.
def build_results_dict(results):
	test_dict = {}
	#Iterate through the results and group them by unit name
	for result in results:
		unit = result['unit']
		test = result['case']
		if unit not in test_dict:
			test_dict[unit] = {}
		if test in test_dict[unit]:
			print("WARNING: %s - %s already exists." % (unit, test))
		test_dict[unit][test] = result
	return test_dict

def regen():
	output_list = []
	if not os.path.exists(RESULTS_FILE):
		print("File 'results.json' not found. Run unit.sh first.")
		exit(1)
	with open(RESULTS_FILE) as infile:
		results = json.loads(infile.read())

	test_dict = build_results_dict(results)

	#Generate the output by alphabetical order
	test_count = 0
	for unit, tests in sorted(test_dict.items(), key=lambda kv: kv[0], reverse=False):
		for test in sorted(tests.items()):
			entry = {"unit": unit, "test": test[1]['test'], 'case': test[1]['case'], 'test_level': test[1]['test_level']}
			if test[1]['uid'] != "":
				entry['uid'] = test[1]['uid']
				entry['vc'] = test[1]['vc']
				entry['req'] = test[1]['req']
			output_list.append(entry)
			test_count += 1
	with open(REQ_FILE, "w+") as outfile:
		json_text = json.dumps(output_list, indent=4).replace(" \n", "\n")
		outfile.write(json_text+"\n")
	print("Generated updated version of \"%s\" based on tests in \"%s\"." % (REQ_FILE, RESULTS_FILE))
	print("Required tests: %d" % test_count)

def get_RGB(s):
	hash = md5.md5(s).hexdigest()
	rgb = '#' + hash[0:2] + hash[2:4] + hash[4:6]
	return rgb

def format_html_test(unit, test, status, error, uid, req, vc):
	if status:
		status = "green"
	else:
		status = "red"
	color = get_RGB(unit)
	ret = "<tr><td bgcolor='%s'></td><td>%s</td><td>%s</td><td bgcolor=%s>%s</td>" % (color, unit, test, status, error)
	jamalink = ""
	class_name = "invalid-uid"
	if uid != "":
		jamalink = "https://nvidia.jamacloud.com/perspective.req#/items/%s?projectId=20460" % uid
		jamalink = "<a class=\"jama\" href=\"%s\" target=\"_blank\">%s</a>" % (jamalink, uid)
		class_name = "valid-uid"
	ret += "<td class='uid %s'>%s</td>\n" % (class_name, jamalink)
	ret += "<td>%s</td><td>%s</td>\n" % (req, vc)
	ret += "</tr>\n"
	return ret

def check(test_level, html = False):
	#Check that tests in results.json cover required_tests.json
	with open(RESULTS_FILE) as results_file:
		results = json.loads(results_file.read())
	with open(REQ_FILE) as req_file:
		reqd_tests = json.loads(req_file.read())

	test_dict = build_results_dict(results)
	req_dict = build_results_dict(reqd_tests)

	errors = 0
	test_count = 0
	log = ""
	html = ""

	#First make sure that all required tests were run and PASSED.
	for reqd_test in reqd_tests:
		unit = reqd_test['unit']
		test = reqd_test['case']
		error = ""
		status = False
		skipped = False
		if unit not in test_dict:
			error = ("ERROR: Required unit %s is not in test results.\n" % unit)
			log += error
			errors += 1
		elif test not in test_dict[unit]:
			if req_dict[unit][test]['test_level'] <= test_level:
				error = ("ERROR: Required test %s - %s is not in test results.\n" % (unit, test))
				log += error
				errors += 1
			else:
				skipped = True
		elif test_dict[unit][test]['status'] is False:
			log += ("ERROR: Required test %s - %s FAILED.\n" % (unit, test))
			error = "FAILED"
			errors += 1
		else:
			status = True
			error = "PASS"
		if not skipped:
			html += format_html_test(unit, test, status, error, reqd_test.get('uid', ''), reqd_test.get('req', ''), reqd_test.get('vc', ''))
			test_count += 1

	#As a helpful hint, check to see if any tests were run without being in the list of required
	#tests. This should not cause a failure, but just a warning for the developer to add the
	#test to the required list.
	for result in results:
		unit = result['unit']
		test = result['case']
		if unit not in req_dict:
			log += ("WARNING: Tested unit %s is not in required tests. Use testlist.py --regen\n" % unit)
		elif test not in req_dict[unit]:
			log +=("WARNING: Test %s - %s is not in required tests. Use testlist.py --regen\n" % (unit, test))

	if log != "":
		print(log)

	if html:
		with open(HTML_FILE, "w+") as outfile:
			outfile.write(HTML_HEAD)
			outfile.write("<table class=\"res-table\" width=\"100%\">\n")
			outfile.write("<thead><tr><th width=20 colspan=2>Unit</th><th>Test</th><th>Result</th>\n")
			outfile.write("<th>JAMA UID  <a href=\"#\" id=\"uid-toggle\">[Toggle]</a></th><th>Requirement</th><th>VC</th></tr></thead>\n")
			outfile.write("<tbody>\n")
			outfile.write(html)
			outfile.write("</tbody></table>\n")
			outfile.write("</body></html>")
			print("Wrote %s" % HTML_FILE)

	if errors > 0:
		return 1
	else:
		print("PASS: All %d tests found in result log." % test_count)
	return 0

def html(test_level):
	return check(test_level, html = True)

parser = argparse.ArgumentParser()
parser.add_argument("--regen", help="Regenerate list of expected test cases.", action="store_true")
parser.add_argument("--check", help="Make sure all expected test cases were run.", action="store_true")
parser.add_argument("--test-level", "-t", help="Test level used for checking results. Default=0", type=int, default=0)
parser.add_argument("--html", help="Perform --check and export results in an HTML file.", action="store_true")

args = parser.parse_args()
if args.regen:
	regen()
	exit(0)
if args.check:
	exit(check(args.test_level))
if args.html:
	exit(html(args.test_level))
else:
	parser.print_help()

