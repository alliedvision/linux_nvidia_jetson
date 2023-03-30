#!/bin/bash
#
# Copyright (c) 2019-2020, NVIDIA CORPORATION.  All Rights Reserved.
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
# Install the unit test binaries, build by tmake, onto a jetson running
# L4T. The only argument is the IP address of the jetson.
#

usage() {
    echo "Usage:"
    echo ""
    echo "  $ $1 [-hs] [--help] [--install-sshkey] <jetson-ip>"
    echo ""
}

# Copy to target jetson. Takes a local path and a remote path relative
# to ubuntu's $HOME.
#
# It helps a lot of you set up an authorized key on the target! Otherwise
# you may have a lot of typing to do...
jcp() {
    cmd="rsync -qur $1 $target:$2"
    echo "> $cmd"
    $cmd
}

# Add our public key to the authorized key list on the target host.
install_ssh_key() {
    ssh-copy-id -f $target
    return $?
}

# Variables which may be set by CLI arguments.
install_sshkey=

# To start with filter out any non-target arguments. Right now that's only -h
# and -s.
positionals=()
while [[ $# -gt 0 ]]
do
    arg="$1"

    case $arg in
	-h|--help)
	    usage $0
	    exit 1
	    ;;
	-s|--install-sshkey)
	    install_sshkey=yes
	    shift
	    ;;
	*)
	    positionals+=("$1")
	    shift
	    ;;
    esac
done

set -- "${positionals[@]}"

if [ "x$1" == "x" ]
then
    echo "Missing IP address!"
    usage $0
    exit 1
fi

if [ "x$TOP" == "x" ]
then
    echo "\$TOP must be set!"
    exit 1
fi

target="ubuntu@$1"
nvgpu_bins=$TOP/out/*/nvidia/kernel/nvgpu

# Install the ssh key if needed.
if [ "$install_sshkey" == "yes" ]
then
    echo "Installing our SSH key"
    install_ssh_key $target || exit 1
fi

# Building the necessary directory structure. It may not be present
# first time this is run.
ssh $target mkdir -p nvgpu_unit/units/igpu
ssh $target mkdir -p nvgpu_unit/firmware/gv11b
ssh $target mkdir -p $TOP/kernel
if [ $? != 0 ]; then
    echo
    echo "!! Unable to make $TOP on the target jetson! This directory needs"
    echo "!! to be present and writable by this script in order for coverage"
    echo "!! tracking to work."
    exit 1
fi

# And copy...
jcp $nvgpu_bins/userspace-l4t_64/nvgpu_unit              nvgpu_unit/nvgpu_unit
jcp $nvgpu_bins/userspace-l4t_64/libnvgpu_unit-lib.so    nvgpu_unit/libnvgpu-unit.so
jcp $nvgpu_bins/libs/igpu-l4t_64/libnvgpu-drv-igpu.so nvgpu_unit/libnvgpu-drv-igpu.so
jcp $TOP/kernel/nvgpu/userspace/unit.sh                  nvgpu_unit/unit.sh
jcp $TOP/kernel/nvgpu/userspace/gcov.sh                  nvgpu_unit/gcov.sh
jcp $TOP/kernel/nvgpu/userspace/testlist.py              nvgpu_unit/testlist.py

find $nvgpu_bins/userspace/units -name "*.so" -not -path "*unit.so" \
    -not -path "*drv.so" -exec du -b {} \; | sort -n | while read size unit_so ; do
        jcp $unit_so nvgpu_unit/units/igpu
done

# Set up the necessary coverage files. Basically what we do is recreate just
# enough of the source/build output here on the local machine over on the
# target jetson. This means you may
jcp $nvgpu_bins            nvgpu_unit
jcp $TOP/kernel/nvgpu      $TOP/kernel
if [ -d TOP/kernel/nvgpu-next ]; then
    jcp $TOP/kernel/nvgpu-next $TOP/kernel
fi
