#!/bin/bash
#
# Copyright (c) 2018, NVIDIA CORPORATION.  All Rights Reserved.
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

# Script to run NVGPU unit tests and collect GCOV coverage.


# First check dependencies and install them if needed
if [ -z $(which gcc) ] || [ -z "$(pip show gcovr)" ]
then
	echo "Some dependencies are missing, installing the now..."
	sudo apt install gcc python-pip -y
	#pip may have a more recent version of gcovr than apt.
	pip install gcovr
fi

SCRIPT_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" > /dev/null && pwd )"
pushd $SCRIPT_ROOT

#Libraries are in a different folder on host and target
LIB_FOLDER="."
if [ -d "./build" ]; then
	LIB_FOLDER="./build"
fi

#Extract the root path of the source code
NVGPU_SRC_ROOT=$(readelf -p .debug_line $LIB_FOLDER/libnvgpu-drv-igpu.so | \
	grep drivers/gpu/nvgpu/os/posix | sed  's/\s*\[.*\]\s*//g' | \
	sed 's/drivers\/gpu\/nvgpu\/os\/posix//g' | head -n 1)
NVGPU_SRC_ROOT=$(realpath $NVGPU_SRC_ROOT)

#Make sure the source code is accessible (for on target testing)
if [ -d "$NVGPU_SRC_ROOT/drivers/gpu/nvgpu" ]; then
	echo "Root folder for sources: $NVGPU_SRC_ROOT"
else
	echo "FATAL: Source code folder not found: $NVGPU_SRC_ROOT"
	exit
fi

#Clean existing GCOV files to avoid getting a cumulative result
find . -name "*.gcda" -exec rm {} +
find $NVGPU_SRC_ROOT -name "*.gcda" -exec rm {} +

#Run unit tests
./unit.sh
OUT=$?
if [ ! $OUT -eq 0 ]; then
	echo "ERROR: Unit test run failed."
	read -p "Press ENTER to run GCOV anyway" -n 1 -r
fi

#Run GCOV
cd ..
if [ -d report ]; then
	rm -rf report/
fi
mkdir report
echo "Generating GCOV report..."
python -m gcovr -v -s -e '.+posix/.+' -e '.*userspace/.+' \
	-r $NVGPU_SRC_ROOT --html-details --output report/index.html &> gcov.log

#Patch the paths to match output from automative runs so that hashes are identical
sed -i 's/Value\">drivers\//Value\">kernel\/nvgpu\/drivers\//g' report/*.c.html

#Present the results
if [ "$(uname -m)" = "aarch64" ]; then
	echo "Running on target, starting a webserver..."
	HOST_IP=$(ip route get 1 | sed -n 's/^.*src \([0-9.]*\) .*$/\1/p')
	if [[ "$HOST_IP" =~ ^[0-9\.]+$ ]]; then
		echo -e "\n\nOpen http://$HOST_IP:8000/ to see the results"
	else
		echo "ERROR: Network is down: $HOST_IP"
		exit 1
	fi
	cd report/
	python -m SimpleHTTPServer &> /dev/null &
	PYTHON_PID=$!
	cd ..
	read -p "Press ENTER to stop the webserver. " -n 1 -r
	kill $PYTHON_PID
else
	echo "Running on host, opening default browser..."
	xdg-open $SCRIPT_ROOT/../report/index.html
fi
popd
