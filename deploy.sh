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
#  Description: script for installing CSI-2 driver to NVIDIA Jetson boards.
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
DEDICATED_VERSION="32.2.1"
DEDICATED_BOARD=""
DEDICATED_BOARD_TX2="TX2"
DEDICATED_BOARD_XAVIER="XAVIER"
DEDICATED_BOARD_NANO="NANO"
FLASH_TARGET_PARTITION=""
FLASH_BOARD_CONFIG=""
FLASH_BOARD_CONFIG_TX2="jetson-tx2"
FLASH_BOARD_CONFIG_XAVIER="jetson-xavier"
FLASH_BOARD_CONFIG_NANO="jetson-nano-qspi-sd"
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
PATH_ROOTFS_CHECK="${PATH_TARGET_L4T}/rootfs/etc"

INSTALL_MOD_PATH="~/test/modulesOut"
#==============================================================================
# deploy information
#==============================================================================
FNAME="AlliedVision_NVidia_${DEDICATED_BOARD}_L4T_${DEDICATED_VERSION}_"
KR=`cat ${PATH_CURRENT}/kerneltree/kernel/kernel-4.9/include/config/kernel.release`
FILE_DEVICE_TREE_BLOB_XAVIER="${PATH_TARGET_KERNEL}/dtb/tegra194-p2888-0001-p2822-0000.dtb"
FILE_DEVICE_TREE_BLOB_TX2="${PATH_TARGET_KERNEL}/dtb/tegra186-quill-p3310-1000-c03-00-base.dtb"
FILE_DEVICE_TREE_BLOB_NANO_DEVKIT="${PATH_TARGET_KERNEL}/dtb/tegra210-p3448-0000-p3449-0000-a02.dtb"
FILE_DEVICE_TREE_BLOB_NANO_PRODUCTION="${PATH_TARGET_KERNEL}/dtb/tegra210-p3448-0002-p3449-0000-b00.dtb"
FILE_NANO_DEVKIT_SIGNED_DTB="${PATH_TARGET_L4T}/bootloader/signed/tegra210-p3448-0000-p3449-0000-a02.dtb.encrypt"
FILE_NANO_PRODUCTION_SIGNED_DTB="${PATH_TARGET_L4T}/bootloader/signed/tegra210-p3448-0002-p3449-0000-b00.dtb.encrypt"
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
	log info "Usage: ${SCRIPT_NAME} <L4T> <TARGET_BOARD> <cmd> [--<option>]"
	log info "<L4T>.........location of Linux4Tegra setup"
	log info "<TARGET_BOARD>....Target board. Possible options: nano, tx2, xavier"
	log info "<cmd>.........command to executed"
	log info "                       flash-all"
	log info "                       flash-dtb"
	log info "                       flash-kernel"
	log info "                       tarball"
	log info "                       sd-image <path-to-sd-card-image>"
	log info "<option>......possible options:"
	log info "                       --compress....compress SD card image"
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
	cp "$FILE_KERNEL_IMAGE" "$PATH_TARGET_DEPLOY_TMP_FOLDER"

	createEncryptedDtb
	cp "$FILE_DEVICE_TREE_BLOB_TX2" "$PATH_TARGET_DEPLOY_TMP_FOLDER"
	cp "$FILE_DEVICE_TREE_BLOB_XAVIER" "$PATH_TARGET_DEPLOY_TMP_FOLDER"
	cp "${FILE_NANO_DEVKIT_SIGNED_DTB}" "$PATH_TARGET_DEPLOY_TMP_FOLDER"
	cp "${FILE_NANO_PRODUCTION_SIGNED_DTB}" "$PATH_TARGET_DEPLOY_TMP_FOLDER"
	cp "$FILE_DEVICE_TREE_BLOB_NANO_DEVKIT" "$PATH_TARGET_DEPLOY_TMP_FOLDER"
	cp "$FILE_DEVICE_TREE_BLOB_NANO_PRODUCTION" "$PATH_TARGET_DEPLOY_TMP_FOLDER"

	
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
# mount sd card image
#==============================================================================
ROOTFS_PARTITION="1"
DTB_PARTITION="10"
ROOTFS_MNT="/tmp/nano_rootfs"
LOOP_DEVICE=""

mount()
{
    img="$1"
    LOOP_DEVICE="$(sudo losetup --show -f -P "$img")"

    part="${LOOP_DEVICE}p${ROOTFS_PARTITION}"
	sudo mkdir -p "$ROOTFS_MNT"
	sudo mount "$part" "$ROOTFS_MNT"
}
#==============================================================================
# umount sd card image
#==============================================================================
unmount()
{
	sync
    sudo umount "$ROOTFS_MNT"
	sudo rm -rf "$ROOTFS_MNT"
	sleep 1
	sudo losetup -d "$LOOP_DEVICE"
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
	if time sudo ./flash.sh $FLASH_BOARD_CONFIG $FLASH_TARGET_PARTITION
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
	CMD=""
	if [ "$DEDICATED_BOARD" = "$DEDICATED_BOARD_NANO" ]
	then
		CMD="sudo ./flash.sh -k DTB $FLASH_BOARD_CONFIG $FLASH_TARGET_PARTITION"
	else
		CMD="sudo ./flash.sh -k kernel-dtb $FLASH_BOARD_CONFIG $FLASH_TARGET_PARTITION"
	fi

	if time eval "${CMD}"
	then
		log debug "Dtb has been flashed to ${3}"
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
flashKernel()
{
	# go to to Linux4Tegra path
	# note: flash.sh script is not running properly 
	# without being in same path
	cd $PATH_TARGET_L4T

	# execute NVIDIA flash script
	if time eval "sudo ./flash.sh -k kernel $FLASH_BOARD_CONFIG $FLASH_TARGET_PARTITION"
	then
		log debug "Kernel image has been flashed to ${3}"
		SUCCESS_FLAG=$TRUE
	else
		log debug "Could not flash files to ${3}"
		SUCCESS_FLAG=$FALSE
	fi

	cd $PATH_CURRENT
}

#==============================================================================
# Create encrypted dtb for the Nano
#==============================================================================
createEncryptedDtb ()
{
	TMP_SD_BLOB="/tmp/sd-blob.img"
	if ! ( cd "${PATH_TARGET_L4T}" && sudo ./create-jetson-nano-sd-card-image.sh -o "${TMP_SD_BLOB}" -s 4G -r 200 )
	then
		log error "Failed to run Nvidia script \"create-jetson-nano-sd-card-image.sh\""
		SUCCESS_FLAG=$FALSE
	fi

	sudo rm -rf "${TMP_SD_BLOB}"

	if proceed && [ ! -f "${FILE_NANO_DEVKIT_SIGNED_DTB}" ]
	then
		log error "Failed to create encrypted dtb. Aborting."
		SUCCESS_FLAG=$FALSE
	else
		SUCCESS_FLAG=$TRUE
	fi
}

#==============================================================================
# running script
#==============================================================================
log info "Running script for flashing Linux4Tegra setup"
log debug "dedicated for L4T: ${DEDICATED_VERSION} and Jetson ${DEDICATED_BOARD}"

# parameter check
#------------------------------------------------------------------------------
if ! parameter_exist $1 || ! parameter_exist $2 || ! parameter_exist $3
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


if proceed
then
	if check_parameter $2 "tx2"
	then
		DEDICATED_BOARD="$DEDICATED_BOARD_TX2"
		FLASH_BOARD_CONFIG="$FLASH_BOARD_CONFIG_TX2"
	elif check_parameter $2 "xavier"
	then
		DEDICATED_BOARD="$DEDICATED_BOARD_XAVIER"
		FLASH_BOARD_CONFIG="$FLASH_BOARD_CONFIG_XAVIER"
	elif check_parameter $2 "nano" 
	then
		DEDICATED_BOARD="$DEDICATED_BOARD_NANO"
		FLASH_BOARD_CONFIG="$FLASH_BOARD_CONFIG_NANO"
	else
		log error "Invalid parameter for <TARGET_BOARD>!"
		SUCCESS_FLAG=$FALSE
		usage
	fi
fi

#------------------------------------------------------------------------------
if proceed
then
	if check_parameter $3 "flash-all"
	then
		# check if rootfs is present
		if [ ! -d "$PATH_ROOTFS_CHECK" ]
		then
			log error "Could not find L4T rootfs! Please run setup script with param --rootfs first"
		else
			log info "Linux4Tegra will be flashed to target board"
			
			FLASH_TARGET_PARTITION="mmcblk0p1"
			flashL4T

			if proceed
			then
				log success
			else
				log failed
			fi
		fi
	elif check_parameter $3 "flash-dtb"
	then
		log info "Device tree blob will be flashed to target board"

		FLASH_TARGET_PARTITION="mmcblk0p1"
		flashDtb

		if proceed
		then
			log success
		else
			log failed
		fi
	elif check_parameter $3 "flash-kernel"
	then
		log info "Kernel image will be flashed to target board"

		FLASH_TARGET_PARTITION="mmcblk0p1"
		flashKernel

		if proceed
		then
			log success
		else
			log failed
		fi
	elif check_parameter $3 "tarball"
	then
		log info "Creating deployable tarball with kernel image and device tree files"
		createDeployTarball
		if proceed
		then
			log success
		else
			log failed
		fi
	elif check_parameter $3 "sd-image"
	then
		if ! check_parameter $2 "nano"
		then
			log error "SD card image creation only supported for the Nano! Aborting."
			exit 1
		fi

		if ! parameter_exist $4
		then
			log error "No SD card image passed! Aborting."
			usage
			exit 1
		fi
		
		if [ ! -f "${4}" ]
		then
			log error "Invalid path to SD card image: \"${SD_CARD}\". Aborting."
			exit 1
		fi

		mkdir "${PATH_TARGET_DEPLOY}"

		log info "Copying SD card image..."
		SD_CARD="${PATH_TARGET_DEPLOY}/${FNAME}${KR}.img"
		if ! rsync -ah --info=progress2 "${4}" "${SD_CARD}"
		then
			log error "Error copying SD card image. Aborting."
			exit 1
		fi

		mount "${SD_CARD}"
		if [ "$?" != 0 ]
		then
			log error "Could not mount SD card image: \"${SD_CARD}\". Aborting."
			rm -rf "${SD_CARD}"
			exit 1
		fi
		
		# copy kernel image and modules
		sudo cp "${FILE_KERNEL_IMAGE}" "${ROOTFS_MNT}/boot/Image"
		sudo cp -r "${PATH_TARGET_L4T}/rootfs/${RELATIVE_PATH_KERNEL_MODULES}" "${ROOTFS_MNT}/lib/modules"
				
		createEncryptedDtb
		
		if ! proceed 
		then
			rm -rf "${SD_CARD}"
			unmount
			log error
			exit 1
		fi

		# copy encrypted dtb to sd card partition
		sudo dd if="${FILE_NANO_DEVKIT_SIGNED_DTB}" of="${LOOP_DEVICE}p${DTB_PARTITION}"

		unmount

		if check_parameter $5 "--compress"
		then
			if hash pigz 2>/dev/null; then
				pigz "${SD_CARD}"
			else
				tar -zcf "${SD_CARD}.tar.gz" "${SD_CARD}"
				rm "${SD_CARD}"
			fi
			
		fi

		log info "SD card image created: \"${SD_CARD}\""
		log success		

	else
		log error "Missing or invalid parameters"
		usage
	fi
fi