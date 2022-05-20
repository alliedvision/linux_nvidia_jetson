#!/bin/bash
#
# Copyright (c) 2017-2020, NVIDIA CORPORATION.  All rights reserved.
#
# The script applies/reverts PREEMPT RT patches in the kernel source.
# - executed in "scripts"
# - usage:
#         rt-patch.sh apply-patches; #for applying
#         rt-patch.sh revert-patches; #for reverting

any_failure=0
apply_rt_patches()
{
	if [ -f $PWD/../arch/arm64/configs/.orig.defconfig ]; then
		echo "The PREEMPT RT patches are already applied to the kernel!"
	else
		#make temporary copy of the defconfig file
		cp $PWD/../arch/arm64/configs/defconfig\
			$PWD/../arch/arm64/configs/.orig.defconfig

		file_list=`find $PWD/../rt-patches -name \*.patch -type f | sort`
		for p in $file_list; do
			# set flag in case of failure and continue
			patch -s -d .. -p1 < $p || any_failure=1
		done
		#make temporary copy of the defconfig file
		cp -f "$PWD/../arch/arm64/configs/defconfig"\
			"$PWD/../arch/arm64/configs/.updated.defconfig"

		$PWD/config --file "$PWD/../arch/arm64/configs/.updated.defconfig"\
			--enable PREEMPT_RT  --disable DEBUG_PREEMPT\
			--disable KVM\
			--disable CPU_IDLE_TEGRA18X\
			--disable CPU_FREQ_GOV_INTERACTIVE\
			--disable CPU_FREQ_TIMES \
			--disable FAIR_GROUP_SCHED || any_failure=1

		rm "$PWD/../arch/arm64/configs/defconfig"
		rm "$PWD/../arch/arm64/configs/tegra_defconfig"
		cp -fnrs "$PWD/../arch/arm64/configs/.updated.defconfig"\
				"$PWD/../arch/arm64/configs/defconfig"
		ln -s "$PWD/../arch/arm64/configs/defconfig"\
				"$PWD/../arch/arm64/configs/tegra_defconfig"

		echo "The PREEMPT RT patches have been successfully applied!"
	fi
}

revert_rt_patches()
{
	if [ -f $PWD/../arch/arm64/configs/.orig.defconfig ]; then
		file_list=`find $PWD/../rt-patches -name \*.patch -type f | sort -r`
		for p in $file_list; do
			# set flag in case of failure and continue
			patch -s -R -d .. -p1 < $p || any_failure=1
		done

		rm "$PWD/../arch/arm64/configs/defconfig"
		rm "$PWD/../arch/arm64/configs/tegra_defconfig"
		cp $PWD/../arch/arm64/configs/.orig.defconfig\
			$PWD/../arch/arm64/configs/defconfig
		ln -s "defconfig"\
			"$PWD/../arch/arm64/configs/tegra_defconfig"

		rm -rf $PWD/../arch/arm64/configs/.orig.defconfig
		rm -rf $PWD/../arch/arm64/configs/.updated.defconfig
		echo "The PREEMPT RT patches have been successfully reverted!"
	else
		echo "The PREEMPT RT patches are not applied to the kernel!"
	fi
}

usage()
{
	echo Usages:
	echo 1. ${0} apply-patches : Apply RT patches
	echo 2. ${0} revert-patches : Revert RT patches
	any_failure=1
}

# script starts from here
dir_run_from=`dirname ${0}`
pushd $dir_run_from &>/dev/null

if [ "$1" == "apply-patches" ]; then
	apply_rt_patches
elif [ "$1" == "revert-patches" ]; then
	revert_rt_patches
else
	echo "Wrong argument"
	usage
fi

popd &>/dev/null

exit $any_failure
