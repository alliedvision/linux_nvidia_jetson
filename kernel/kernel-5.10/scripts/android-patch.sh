#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
#
# Copyright (c) 2022, NVIDIA CORPORATION.  All rights reserved.
#
# The script applies/reverts AOSP patches in the kernel source.
# - executed in "scripts"
# - usage:
#         android-patch.sh apply-patches; #for applying
#         android-patch.sh revert-patches; #for reverting


any_failure=0
apply_android_patches()
{
	AOSP_PATCH_LIST_FILE=$PWD/../.aosp_k510_patch_list
	if [ -f $AOSP_PATCH_LIST_FILE ]; then
		echo "The AOSP patches are already applied to the kernel!"
	else
		file_list=`find $PWD/../android-patches -name \*.patch -type f | sort`
        echo $file_list > $AOSP_PATCH_LIST_FILE
		for p in $file_list; do
			# set flag in case of failure and continue
			patch -s -d .. -p1 < $p || any_failure=1
		done

		echo "The AOSP patches have been successfully applied!"
	fi
}

revert_android_patches()
{
	AOSP_PATCH_LIST_FILE=$PWD/../.aosp_k510_patch_list
	if [ -f $AOSP_PATCH_LIST_FILE ]; then
		file_list=`find $PWD/../android-patches -name \*.patch -type f | sort -r`
		for p in $file_list; do
			# set flag in case of failure and continue
			patch -s -R -d .. -p1 < $p || any_failure=1
		done

		rm -rf $AOSP_PATCH_LIST_FILE
		echo "The AOSP patches have been successfully reverted!"
	else
		echo "The AOSP patches are not applied to the kernel!"
	fi
}

usage()
{
	echo Usages:
	echo 1. ${0} apply-patches : Apply AOSP patches
	echo 2. ${0} revert-patches : Revert AOSP patches
	any_failure=1
}

# script starts from here
dir_run_from=`dirname ${0}`
pushd $dir_run_from &>/dev/null

if [ "$1" == "apply-patches" ]; then
	apply_android_patches
elif [ "$1" == "revert-patches" ]; then
	revert_android_patches
else
	echo "Wrong argument"
	usage
fi

popd &>/dev/null

exit $any_failure
