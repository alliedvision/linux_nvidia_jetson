#!/bin/bash
#
# Copyright (c) 2019, NVIDIA CORPORATION.  All Rights Reserved.
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

#
# Execute the unit test. Args to this script are passed on to the unit test
# core. This just serves to set the LD_LIBRARY_PATH environment variable such
# that unit tests are found and nvgpu-drv-igpu is found.
#

options=$(getopt -o t: --long test-level: -- "$@")

this_script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" > /dev/null && pwd )"
pushd $this_script_dir

FIRMWARES=("/gpu-firmware-private/t19x/gr/net/safety/NETD_img.bin"
	"/gpu-firmware-private/t19x/gr/net/safety/fecs_sig.bin"
	"/gpu-firmware-private/t19x/gr/net/safety/gpccs_sig.bin"
	"/gpu-firmware-private/t19x/gr/fecs/safety/fecs.bin"
	"/gpu-firmware-private/t19x/gr/gpccs/safety/gpccs.bin"
	"/gpu-firmware-private/t19x/gr/pmu/safety/acr_ucode_prod.bin"
	"/gpu-firmware-private/t19x/gr/pmu/safety/acr_ucode_dbg.bin")

FIRMWARE_TARGET_PATH="/lib/firmware/gv11b/"

CUR_DIR=`pwd`
DEST_DIR=${CUR_DIR}/firmware/gv11b/
mkdir -p ${DEST_DIR}

if [ -f nvgpu_unit ]; then
	if [ -d  ${FIRMWARE_TARGET_PATH} ]
	then
		cp -rf ${FIRMWARE_TARGET_PATH}/* ${DEST_DIR}
	else
		echo "${FIRMWARE_TARGET_PATH} doesn't exist\n"
		exit 1
	fi

        # if the executable is in the current directory, we are running on
        # target, so use that dir structure
        LD_LIBRARY_PATH=".:units/igpu"
        cores=$(cat /proc/cpuinfo |grep processor |wc -l)

	# Ignore number of cores for now; it seems that the parallel unit
	# tests are just too buggy and that they really don't save much
	# actual computing time.
        NVGPU_UNIT="./nvgpu_unit --nvtest --unit-load-path units/igpu --no-color \
                 --num-threads 1"

else
	# running on host
	if [ "x$TEGRA_TOP" == "x" ]
	then
		echo "\$TEGRA_TOP must be set!"
		exit 1
	fi

	for i in "${FIRMWARES[@]}"
	do
		if [ -f ${TEGRA_TOP}/${i} ]
		then
			cp -f ${TEGRA_TOP}/${i} ${DEST_DIR}
		else
			echo "firmware file \"${TEGRA_TOP}/${i}\" doesn't exist\n"
			exit 1
		fi

	done

        LD_LIBRARY_PATH="build:build/units"
        # On host, must run single-threaded to avoid high VAs
        NVGPU_UNIT="./build/nvgpu_unit --num-threads 1"
fi
export LD_LIBRARY_PATH

echo "$ $NVGPU_UNIT $*"

$NVGPU_UNIT $*
rc=$?
if [ $rc -eq "0" ]; then
        #get the test level, if passed into this script
        eval set -- $options
        while true; do
               case "$1" in
               -t|--test-level)
                      shift;
                      testlevelparam="-t $1"
                      ;;
                --)
                      shift;
                      break;
                      ;;
                esac
                shift
        done
fi

rm -rf ${CUR_DIR}/firmware
popd
exit $rc
