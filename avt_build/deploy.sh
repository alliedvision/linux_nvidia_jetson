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
DEDICATED_VERSION="32.4.2"
DEDICATED_BOARD=""
DEDICATED_BOARD_TX2="TX2"
DEDICATED_BOARD_NX="NX"
DEDICATED_BOARD_XAVIER="XAVIER"
DEDICATED_BOARD_NANO="NANO"
FLASH_TARGET_PARTITION=""
FLASH_BOARD_CONFIG=""
FLASH_BOARD_CONFIG_TX2="jetson-tx2"
FLASH_BOARD_CONFIG_NX="jetson-xavier-nx-devkit"
FLASH_BOARD_CONFIG_XAVIER="jetson-xavier"
FLASH_BOARD_CONFIG_NANO="jetson-nano-qspi-sd"
#==============================================================================
# path settings
#==============================================================================
#PATH_CURRENT=$(pwd)
PATH_CURRENT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
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
KR=`cat ${PATH_CURRENT}/../kernel/kernel-4.9/include/config/kernel.release`
FILE_DEVICE_TREE_BLOB_XAVIER="${PATH_TARGET_KERNEL}/dtb/tegra194-p2888-0001-p2822-0000.dtb"
FILE_DEVICE_TREE_BLOB_TX2="${PATH_TARGET_KERNEL}/dtb/tegra186-quill-p3310-1000-c03-00-base.dtb"
FILE_DEVICE_TREE_BLOB_NANO1="${PATH_TARGET_KERNEL}/dtb/tegra210-p3448-0000-p3449-0000-a02.dtb"
FILE_DEVICE_TREE_BLOB_NANO2="${PATH_TARGET_KERNEL}/dtb/tegra210-p3448-0000-p3449-0000-a00.dtb"
FILE_DEVICE_TREE_BLOB_NX="${PATH_TARGET_KERNEL}/dtb/tegra194-p3668-all-p3509-0000.dtb"
FILE_KERNEL_IMAGE="${PATH_TARGET_KERNEL}/Image"
FILE_INSTALL_SCRIPT="${PATH_CURRENT}/resources/deploy/install.sh"
RELATIVE_PATH_KERNEL_MODULES="lib/modules/${KR}"
FILE_DEPLOY_MODULES="modules.tar.gz"
PATH_TARGET_DEPLOY_TMP_FOLDER="${PATH_TARGET_DEPLOY}/${FNAME}${KR}"
PATH_SIGNED_NANO_DTB="${PATH_TARGET_L4T}/bootloader/signed/tegra210-p3448-0000-p3449-0000-a02.dtb.encrypt"
PATH_SIGNED_NX_DTB="${PATH_TARGET_L4T}/bootloader/signed/tegra194-p3668-all-p3509-0000_sigheader.dtb.encrypt"
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
	log info "<TARGET_BOARD>....Target board. Possible options: nano, tx2, xavier, nx"
	log info "<cmd>.........command to executed"
	log info "                       flash-all"
	log info "                       flash-dtb"
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

	if [ "$DEDICATED_BOARD" != "$DEDICATED_BOARD_XAVIER" ]; then
		cp "$FILE_KERNEL_IMAGE" "$PATH_TARGET_DEPLOY_TMP_FOLDER"
	fi

	if [ "$DEDICATED_BOARD" = "$DEDICATED_BOARD_TX2" ]
	then
		cp "$FILE_DEVICE_TREE_BLOB_TX2" "$PATH_TARGET_DEPLOY_TMP_FOLDER"
	elif [ "$DEDICATED_BOARD" = "$DEDICATED_BOARD_XAVIER" ]
	then
		(
			cd "${PATH_TARGET_L4T}"
			sudo ./flash.sh -r -k kernel --no-flash jetson-xavier mmcblk0p1
			sudo ./flash.sh -r -k kernel-dtb --no-flash jetson-xavier mmcblk0p1
			cd "${PATH_TARGET_L4T}/bootloader"

			./mkbootimg --kernel ../kernel/Image --ramdisk initrd --board mmcblk0p1 --output boot.img
			sudo ./tegraflash.py --bl nvtboot_recovery_cpu_t194.bin  --chip 0x19 --applet mb1_t194_prod.bin \
				--sdram_config tegra194-mb1-bct-memcfg-p2888.cfg,tegra194-memcfg-sw-override.cfg \
				--uphy_config tegra194-mb1-uphy-lane-p2888-0000-p2822-0000.cfg \
				--device_config tegra19x-mb1-bct-device-sdmmc.cfg --misc_cold_boot_config tegra194-mb1-bct-misc-l4t.cfg \
				--misc_config tegra194-mb1-bct-misc-flash.cfg --pinmux_config tegra19x-mb1-pinmux-p2888-0000-a04-p2822-0000-b01.cfg \
				--gpioint_config tegra194-mb1-bct-gpioint-p2888-0000-p2822-0000.cfg \
				--pmic_config tegra194-mb1-bct-pmic-p2888-0001-a04-p2822-0000.cfg \
				--pmc_config tegra19x-mb1-padvoltage-p2888-0000-a00-p2822-0000-a00.cfg \
				--prod_config tegra19x-mb1-prod-p2888-0000-p2822-0000.cfg --scr_config tegra194-mb1-bct-scr-cbb-mini.cfg \
				--scr_cold_boot_config tegra194-mb1-bct-scr-cbb-mini.cfg \
				--br_cmd_config tegra194-mb1-bct-reset-p2888-0000-p2822-0000.cfg --dev_params tegra194-br-bct-sdmmc.cfg --cfg  flash.xml --bin \
					"mb2_bootloader nvtboot_recovery_t194.bin; mts_preboot preboot_c10_prod_cr.bin; mts_mce mce_c10_prod_cr.bin; \
					 mts_proper mts_c10_prod_cr.bin; bpmp_fw bpmp_t194.bin; bpmp_fw_dtb tegra194-a02-bpmp-p2888-a04.dtb; \
					 spe_fw spe_t194.bin; tlk tos_t194.img; eks eks.img; kernel boot.img; kernel_dtb tegra194-p2888-0001-p2822-0000.dtb; \
					 bootloader_dtb tegra194-p2888-0001-p2822-0000.dtb" \
				--cmd "sign boot.img"

			sudo ./tegraflash.py --bl nvtboot_recovery_cpu_t194.bin  --chip 0x19 --applet mb1_t194_prod.bin \
				--sdram_config tegra194-mb1-bct-memcfg-p2888.cfg,tegra194-memcfg-sw-override.cfg \
				--uphy_config tegra194-mb1-uphy-lane-p2888-0000-p2822-0000.cfg \
				--device_config tegra19x-mb1-bct-device-sdmmc.cfg --misc_cold_boot_config tegra194-mb1-bct-misc-l4t.cfg \
				--misc_config tegra194-mb1-bct-misc-flash.cfg --pinmux_config tegra19x-mb1-pinmux-p2888-0000-a04-p2822-0000-b01.cfg \
				--gpioint_config tegra194-mb1-bct-gpioint-p2888-0000-p2822-0000.cfg \
				--pmic_config tegra194-mb1-bct-pmic-p2888-0001-a04-p2822-0000.cfg \
				--pmc_config tegra19x-mb1-padvoltage-p2888-0000-a00-p2822-0000-a00.cfg \
				--prod_config tegra19x-mb1-prod-p2888-0000-p2822-0000.cfg --scr_config tegra194-mb1-bct-scr-cbb-mini.cfg \
				--scr_cold_boot_config tegra194-mb1-bct-scr-cbb-mini.cfg \
				--br_cmd_config tegra194-mb1-bct-reset-p2888-0000-p2822-0000.cfg --dev_params tegra194-br-bct-sdmmc.cfg --cfg  flash.xml --bin \
					"mb2_bootloader nvtboot_recovery_t194.bin; mts_preboot preboot_c10_prod_cr.bin; mts_mce mce_c10_prod_cr.bin; \
					 mts_proper mts_c10_prod_cr.bin; bpmp_fw bpmp_t194.bin; bpmp_fw_dtb tegra194-a02-bpmp-p2888-a04.dtb; \
					 spe_fw spe_t194.bin; tlk tos_t194.img; eks eks.img; kernel boot.img; kernel_dtb tegra194-p2888-0001-p2822-0000.dtb; \
					 bootloader_dtb tegra194-p2888-0001-p2822-0000.dtb" \
				--cmd "sign tegra194-p2888-0001-p2822-0000.dtb"
		)
		cp "${PATH_TARGET_L4T}/bootloader/boot_sigheader.img.encrypt" "$PATH_TARGET_DEPLOY_TMP_FOLDER"
		cp "${PATH_TARGET_L4T}/bootloader/tegra194-p2888-0001-p2822-0000_sigheader.dtb.encrypt" "$PATH_TARGET_DEPLOY_TMP_FOLDER"
		sed -i -e '/REQ_MACHINE=/c\REQ_MACHINE="NVidia Jetson AGX Xavier"' "$PATH_TARGET_DEPLOY_TMP_FOLDER/install.sh"
	elif [ "$DEDICATED_BOARD" = "$DEDICATED_BOARD_NX" ]
	then
		createEncryptedDtb "${PATH_SIGNED_NX_DTB}" -b jetson-xavier-nx-devkit

		cp "${PATH_TARGET_L4T}/bootloader/signed/boot_sigheader.img.encrypt" "$PATH_TARGET_DEPLOY_TMP_FOLDER"
		cp "${PATH_SIGNED_NX_DTB}" "$PATH_TARGET_DEPLOY_TMP_FOLDER"
		cp "$FILE_DEVICE_TREE_BLOB_NX" "$PATH_TARGET_DEPLOY_TMP_FOLDER"

	else
		createEncryptedDtb "${PATH_SIGNED_NANO_DTB}" -b jetson-nano -r 200

		cp "${PATH_SIGNED_NANO_DTB}" "$PATH_TARGET_DEPLOY_TMP_FOLDER"
		cp "$FILE_DEVICE_TREE_BLOB_NANO1" "$PATH_TARGET_DEPLOY_TMP_FOLDER"
		cp "$FILE_DEVICE_TREE_BLOB_NANO2" "$PATH_TARGET_DEPLOY_TMP_FOLDER"
	fi
	
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
# Create encrypted dtb for the Nano
#==============================================================================
createEncryptedDtb ()
{
	TMP_SD_BLOB="/tmp/sd-blob.img"
	DTB_DST="$1"
	shift
	if ! ( cd "${PATH_TARGET_L4T}/tools" && sudo ./jetson-disk-image-creator.sh -o ${TMP_SD_BLOB} $@ )
	then
		log error "Failed to run Nvidia script \"./jetson-disk-image-creator.sh\""
		SUCCESS_FLAG=$FALSE
	fi

	sudo rm -rf "${TMP_SD_BLOB}"

	if proceed && [ ! -f "$DTB_DST" ]
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
	elif check_parameter $2 nx
	then
		DEDICATED_BOARD="$DEDICATED_BOARD_NX"
		FLASH_BOARD_CONFIG="$FLASH_BOARD_CONFIG_NX"
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
			log info "Linux4Tegra will be flashed to TX2 board"
			
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
		log info "Device tree blob will be flashed to TX2 board"

		FLASH_TARGET_PARTITION="mmcblk0p1"
		flashDtb

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
		MODEL=$2
		if ! check_parameter "$MODEL" "nano" && ! check_parameter "$MODEL" nx
		then
			log error "SD card image creation only supported for the Nano and NX! Aborting."
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

		log info "Copying kernel image..."
		SD_CARD="${PATH_TARGET_DEPLOY}/${FNAME}${KR}.img"
		if ! rsync -ah --info=progress2 "${4}" "${SD_CARD}"
		then
			log error "Error copying the image. Aborting."
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
		sudo rm -rf "${ROOTFS_MNT}/lib/modules/*"
		sudo cp -r "${PATH_TARGET_L4T}/rootfs/${RELATIVE_PATH_KERNEL_MODULES}" "${ROOTFS_MNT}/lib/modules"

		case "$MODEL" in
		nano)
			createEncryptedDtb "${PATH_SIGNED_NANO_DTB}" -b jetson-nano -r 200
			;;
		nx)
			createEncryptedDtb "${PATH_SIGNED_NX_DTB}" -b jetson-xavier-nx-devkit
			;;
		esac
			
		
		if ! proceed 
		then
			rm -rf "${SD_CARD}"
			unmount
			log error
			exit 1
		fi

		# copy encrypted dtb to sd card partition

		case "$MODEL" in
		nano)
			sudo dd if="${PATH_SIGNED_NANO_DTB}" of="${LOOP_DEVICE}p10"
			;;
		nx)
			sudo dd if="${PATH_SIGNED_NX_DTB}" of="${LOOP_DEVICE}p4"
			sudo dd if="${PATH_TARGET_L4T}/bootloader/signed/boot_sigheader.img.encrypt" of="${LOOP_DEVICE}p2"
			;;
		esac
			

		unmount

		if check_parameter $5 "--compress"
		then
			pigz "${SD_CARD}"
		fi

		log info "SD card image created: \"${SD_CARD}\""
		log success		

	else
		log error "Missing or invalid parameters"
		usage
	fi
fi
