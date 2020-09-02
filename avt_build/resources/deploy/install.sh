#!/bin/bash
#==============================================================================
# Copyright (C) 2019 Allied Vision Technologies.  All Rights Reserved.
#
# Redistribution of this file, in original or modified form, without
# prior written consent of Allied Vision Technologies is prohibited.
#
#------------------------------------------------------------------------------
#
# File:         -install.sh
#
# Description:  -bash script for installing the kernel and CSI-2 driver
#
#------------------------------------------------------------------------------
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF TITLE,
# NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR  PURPOSE ARE
# DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#==============================================================================

NC='\033[0m'
RED='\033[0;31m'
GREEN='\033[0;32m'
REQ_MACHINE="NVidia Jetson"
REQ_KERNEL="4.9.140"
DEST="/boot"

echo -e ${RED}"Allied Vision"${NC}" MIPI CSI-2 camera driver for "${GREEN}${REQ_MACHINE}${NC}" (kernel "${REQ_KERNEL}")"


# ========================================================= usage function
usage() {
	echo -e "Options:"	
	echo -e "-h:\t Display help"
	echo -e "-y:\t Automatic install and reboot"
	exit 0
}

inst() {
	MODEL="$(tr -d '\000' < /proc/device-tree/model)"

	# On Nano, write signed dtb to partition
	if [[ "$MODEL" == *"Nano"* ]];then
		sudo dd if=$(realpath *encrypt) of=/dev/disk/by-partlabel/DTB
		sync

	# On Xavier, write signed kernel+dtb+initrd to partition
	elif [[ "$MODEL" == *"AGX"* ]];then
		sudo dd if=boot_sigheader.img.encrypt of=/dev/disk/by-partlabel/kernel
		sudo dd if=tegra194-p2888-0001-p2822-0000_sigheader.dtb.encrypt of=/dev/disk/by-partlabel/kernel-dtb

		sync

	elif [[ "$MODEL" == *"Xavier NX Developer Kit"* ]]; then
		sudo dd if=boot_sigheader.img.encrypt of=/dev/disk/by-partlabel/kernel
		sudo dd if=tegra194-p3668-all-p3509-0000_sigheader.dtb.encrypt of=/dev/disk/by-partlabel/kernel-dtb
	fi

	# All boards: copy Image + DTBs in rootfs
	sudo cp -r Image $DEST
	sudo cp -r *.dtb $DEST
	sudo cp -r *.dtb $DEST/dtb
	
	# All boards: Install modules in rootfs
	sudo tar zxvf modules.tar.gz -C /
}


if [[ ( $1 == "-help") || ( $1 == "-h") ]];then
	usage
fi	

if [[ ( $1 == "-y")]];then
	inst
	sudo init 6
	exit 0
fi	


read -p "Install kernel driver (y/n)? " answer
case $answer in
	[Yy]* )
		echo -e "\nInstalling..."
		inst
	;;
	[Nn]* )
	 	echo -e
	;;
esac

read -p "Reboot now (y/n)? " answer
case $answer in
	[Yy]* )
		echo -e "Reboot..."
		sudo init 6
	;;
	[Nn]* )
	 	echo -e
		exit 0
	;;
esac

exit 0

