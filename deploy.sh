#!/bin/bash
#==============================================================================
#  Copyright (C) 2012 - 2019 Allied Vision Technologies.  All Rights Reserved.
#
#  Redistribution of this file, in original or modified form, without
#  prior written consent of Allied Vision Technologies is prohibited.
#
#------------------------------------------------------------------------------
# 
#  File:        deploy.sh
#
#  Description: script for flashing built L4T to NVIDIA Jetson TX2 board.
#
#------------------------------------------------------------------------------
#
#  THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED
#  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF TITLE,
#  NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR  PURPOSE ARE
#  DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, 
#  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
#  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
#  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED  
#  AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR 
#  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
#  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#==============================================================================
# script settings
#==============================================================================
DEDICATED_VERSION="32.1"
DEDICATED_BOARD="TX2"
FLASH_TARGET=""
#==============================================================================
# path settings
#==============================================================================
PATH_CURRENT=$(pwd)
#------------------------------------------------------------------------------
PATH_RESOURCES="${PATH_CURRENT}/resources"
#------------------------------------------------------------------------------
PATH_TARGET_ROOT="${PATH_CURRENT}/${1}"
PATH_TARGET_DEPLOY="$PATH_TARGET_ROOT/deploy"
PATH_TARGET_L4T="${PATH_TARGET_ROOT}/Linux_for_Tegra"
PATH_TARGET_KERNEL="${PATH_TARGET_L4T}/kernel"

INSTALL_MOD_PATH="~/test/modulesOut"
#==============================================================================
# deploy information
#==============================================================================
FNAME="AlliedVision_NVidia_${DEDICATED_BOARD}_L4T_${DEDICATED_VERSION}_"
KR=`cat ${PATH_CURRENT}/kerneltree/kernel/kernel-4.9/include/config/kernel.release`
FILE_DEVICE_TREE_BLOB="${PATH_TARGET_KERNEL}/dtb/tegra186-quill-p3310-1000-c03-00-base.dtb"
FILE_KERNEL_IMAGE="${PATH_TARGET_KERNEL}/Image"
FILE_INSTALL_SCRIPT="${PATH_CURRENT}/resources/deploy/install.sh"
RELATIVE_PATH_KERNEL_MODULES="lib/modules/${KR}"
FILE_DEPLOY_MODULES="modules.tar.gz"
PATH_TARGET_DEPLOY_TMP_FOLDER="${PATH_TARGET_DEPLOY}/${FNAME}${KR}"
#==============================================================================
# include helper scripts
#==============================================================================
source "${PATH_RESOURCES}/common/common.sh"
source "${PATH_RESOURCES}/common/logging.sh"
source "${PATH_RESOURCES}/common/error.sh"
source "${PATH_RESOURCES}/common/directory.sh"
#==============================================================================
# show script usage function
#==============================================================================
usage()
{
	log info "Usage: ${SCRIPT_NAME} <L4T> <cmd>"
	log info "<L4T>.........location of Linux4Tegra setup"
	log info "<cmd>.........command to executed"
	log info "                       flash-all"
	log info "                       flash-dtb"
	log info "                       tarball"
	log_raw "\n"
}
#==============================================================================
# create tarball from 
#==============================================================================
createKernelTarball()
{
	if sudo tar --owner root --group root -cjf "${PATH_TARGET_KERNEL}/kernel_supplements.tbz2" "${INSTALL_MOD_PATH}/lib/modules"
	then 
		log debug "Kernel module tarball has been created"
		SUCCESS_FLAG=$TRUE
	else 
		log debug "Could not create tarball with kernel modules"
		SUCCESS_FLAG=$FALSE
	fi
}
#==============================================================================
# create tarball with kernel image and modules
#==============================================================================
createDeployTarball()
{
	rm -rf "$PATH_TARGET_DEPLOY"
	mkdir -p "$PATH_TARGET_DEPLOY_TMP_FOLDER"
	cp "$FILE_INSTALL_SCRIPT" "$PATH_TARGET_DEPLOY_TMP_FOLDER"
	cp "$FILE_DEVICE_TREE_BLOB" "$PATH_TARGET_DEPLOY_TMP_FOLDER"
	cp "$FILE_KERNEL_IMAGE" "$PATH_TARGET_DEPLOY_TMP_FOLDER"
	
	if ! (cd "${PATH_TARGET_L4T}/rootfs"; tar -zcvf "${PATH_TARGET_DEPLOY_TMP_FOLDER}/${FILE_DEPLOY_MODULES}" "${RELATIVE_PATH_KERNEL_MODULES}")
	then
		log error "Could not find modules folder!"
		SUCCESS_FLAG=$FALSE
	else		
		if (cd "$PATH_TARGET_DEPLOY" && tar -zcvf "${FNAME}${KR}".tar.gz $(basename "${PATH_TARGET_DEPLOY_TMP_FOLDER}"))
		then 
			log debug "Deploy tarball has been created: ${PATH_TARGET_DEPLOY}/${FNAME}${KR}.tar.gz"
			SUCCESS_FLAG=$TRUE
		else 
			log error "Could not create tarball with kernel image and device tree files"
			SUCCESS_FLAG=$FALSE
		fi
	fi
}
#==============================================================================
# function for flashing to jetson board
#==============================================================================
flashL4T()
{
	PROMPT="+++++ WARNING: THIS WILL ERASE THE TARGET'S FILESYSTEM. CONTINUE? (y/n) +++++"
	read -p "`echo $'\n \b' $PROMPT $'\n \b'`" user_input
	if [ "$user_input" != "y" ]
	then
		log debug "User aborted."
		SUCCESS_FLAG=$FALSE
		return
	fi

	# go to to Linux4Tegra path
	# note: flash.sh script is not running properly 
	# without being in same path
	cd $PATH_TARGET_L4T

	# execute NVIDIA flash script
	if time sudo ./flash.sh jetson-tx2 $FLASH_TARGET
	then
		log debug "Files has benn flashed to ${3}"
		SUCCESS_FLAG=$TRUE
	else
		log debug "Could not flash files to ${3}"
		SUCCESS_FLAG=$FALSE
	fi

	cd $PATH_CURRENT
}
#==============================================================================
# function for flashing to jetson board
#==============================================================================
flashDtb()
{
	# go to to Linux4Tegra path
	# note: flash.sh script is not running properly 
	# without being in same path
	cd $PATH_TARGET_L4T

	# execute NVIDIA flash script
	if time sudo ./flash.sh -k kernel-dtb jetson-tx2 $FLASH_TARGET
	then
		log debug "Dtb has been flashed to ${3}"
		SUCCESS_FLAG=$TRUE
	fi

	cd $PATH_CURRENT
}
#==============================================================================
# running script
#==============================================================================
log info "Running script for flashing Linux4Tegra setup"
log debug "dedicated for L4T: ${DEDICATED_VERSION} and Jetson ${DEDICATED_BOARD}"

# parameter check
#------------------------------------------------------------------------------
if ! parameter_exist $1 || ! parameter_exist $2
then
	log error "Missing parameter!"
	SUCCESS_FLAG=$FALSE
	usage
	exit 1
fi

# check if help was called
#------------------------------------------------------------------------------
if proceed && check_parameter $1 "help"
then
	usage
	exit 1
fi

# check if given L4T parameter is valid setup directory
#------------------------------------------------------------------------------
if proceed && ! directory_exist $1
then
	log error "Could not find L4T setup directory! Please run setup script first"
	SUCCESS_FLAG=$FALSE
	exit 1
fi

#------------------------------------------------------------------------------
if proceed
then
	if check_parameter $2 "flash-all"
	then
		log info "Linux4Tegra will be flashed to TX2 board"
		
		FLASH_TARGET="mmcblk0p1"
		flashL4T

		if proceed
		then
			log success
		else
			log failed
		fi
	elif check_parameter $2 "flash-dtb"
	then
		log info "Device tree blob will be flashed to TX2 board"

		FLASH_TARGET="mmcblk0p1"
		flashDtb

		if proceed
		then
			log success
		else
			log failed
		fi
	elif check_parameter $2 "tarball"
	then
		log info "Creating deployable tarball with kernel image and device tree files"
		createDeployTarball
		if proceed
		then
			log success
		else
			log failed
		fi
	else
		log error "Missing or invalid parameters"
		usage
	fi	
fi