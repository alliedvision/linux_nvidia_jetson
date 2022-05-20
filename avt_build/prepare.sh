#!/bin/bash
#==============================================================================
#  Copyright (C) 2012 - 2022 Allied Vision Technologies.  All Rights Reserved.
#
#  Redistribution of this file, in original or modified form, without
#  prior written consent of Allied Vision Technologies is prohibited.
#
#------------------------------------------------------------------------------
# 
#  File:        setup.sh
#
#  Description: setup script for NVIDIA Jetson TX2 board. herewith all
#               corresponding files will be extracted and whole build
#               environment will be prepared.
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
SCRIPT_NAME=`basename "$0"`
DEDICATED_VERSION="R32.7.1"
DEDICATED_BOARD="TX2/XAVIER"
DEDICATED_BOARD_NANO="NANO"
COMPLETE_INSTALL=false
#==============================================================================
# path settings
#==============================================================================
#PATH_CURRENT=$(pwd)
PATH_CURRENT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
#------------------------------------------------------------------------------
PATH_DOWNLOADS="${PATH_CURRENT}/resources"
PATH_DOWNLOADS_DRIVER_PACKAGE="${PATH_DOWNLOADS}/driverPackage"
PATH_DOWNLOADS_ROOTFS="${PATH_DOWNLOADS}/rootfs"
PATH_DOWNLOADS_PUBLICSOURCES="${PATH_DOWNLOADS}/public_sources"
PATH_DOWNLOADS_GCC="${PATH_DOWNLOADS}/gcc"
PATH_DOWNLOADS_DTB="${PATH_DOWNLOADS}/dtb"
#------------------------------------------------------------------------------
PATH_TARGET_ROOT="${PATH_CURRENT}/${1}"
PATH_TARGET_L4T="${PATH_TARGET_ROOT}/Linux_for_Tegra"
PATH_TARGET_ROOTFS="${PATH_TARGET_L4T}/rootfs"
PATH_TARGET_DTB="${PATH_TARGET_L4T}/kernel/dtb"
#------------------------------------------------------------------------------
PATH_TARGET_GCC="${PATH_TARGET_ROOT}/gcc"
PATH_TARGET_GCC_64="${PATH_TARGET_GCC}/gcc-7.3.1-aarch64"
#------------------------------------------------------------------------------
PATH_SOURCES="${PATH_TARGET_L4T}/sources"
#==============================================================================
# file settings
#==============================================================================
FILE_DRIVER_PACKAGE=""
FILE_DRIVER_PACKAGE_TX2="tegra186_linux_r32.7.1_aarch64.tbz2"
FILE_DRIVER_PACKAGE_NANO="jetson-210_linux_r32.7.1_aarch64.tbz2"
FILE_ROOTFS=""
FILE_ROOTFS_TX2="tegra_linux_sample-root-filesystem_r32.7.1_aarch64.tbz2"
FILE_ROOTFS_NANO="tegra_linux_sample-root-filesystem_r32.7.1_aarch64.tbz2"
FILE_PUBLICSOURCES=""
FILE_PUBLICSOURCES_TX2="public_sources.tbz2"
FILE_PUBLICSOURCES_NANO="public_sources.tbz2"
FILE_GCC_64="gcc-linaro-7.3.1-2018.05-x86_64_aarch64-linux-gnu.tar.xz"
#==============================================================================
# download urls
#==============================================================================
DOWNLOAD_URL_DRIVER_PACKAGE=""
DOWNLOAD_URL_DRIVER_PACKAGE_TX2="https://developer.nvidia.com/embedded/l4t/r32_release_v7.1/r32_release_v6.1/t186/tegra186_linux_r32.7.1_aarch64.tbz2"                                  
DOWNLOAD_URL_DRIVER_PACKAGE_NANO="https://developer.nvidia.com/embedded/l4t/r32_release_v7.1/r32_release_v6.1/t210/jetson-210_linux_r32.7.1_aarch64.tbz2"
DOWNLOAD_URL_ROOTFS=""
DOWNLOAD_URL_ROOTFS_TX2="https://developer.nvidia.com/embedded/l4t/r32_release_v7.1/r32_release_v6.1/t186/tegra_linux_sample-root-filesystem_r32.7.1_aarch64.tbz2"
DOWNLOAD_URL_ROOTFS_NANO="https://developer.nvidia.com/embedded/l4t/r32_release_v7.1/r32_release_v6.1/t210/tegra_linux_sample-root-filesystem_r32.7.1_aarch64.tbz2"
DOWNLOAD_URL_PUBLICSOURCES=""
DOWNLOAD_URL_PUBLICSOURCES_TX2="https://developer.nvidia.com/embedded/l4t/r32_release_v7.1/r32_release_v7.1/sources/t186/public_sources.tbz2"
DOWNLOAD_URL_PUBLICSOURCES_NANO="https://developer.nvidia.com/embedded/l4t/r32_release_v7.1/r32_release_v7.1/sources/t210/public_sources.tbz2"
DOWNLOAD_URL_GCC="https://developer.nvidia.com/embedded/dlc/l4t-gcc-7-3-1-toolchain-64-bit"
#==============================================================================
# include helper scripts
#==============================================================================
source "${PATH_DOWNLOADS}/common/common.sh"
source "${PATH_DOWNLOADS}/common/logging.sh"
source "${PATH_DOWNLOADS}/common/error.sh"
source "${PATH_DOWNLOADS}/common/directory.sh"
#==============================================================================
# show script usage function
#==============================================================================
usage()
{
	log info "Usage: ${SCRIPT_NAME} <L4T> <TARGET_BOARD> --<options>"
	log info "<L4T>.............location for Linux4Tegra setup"
	log info "<TARGET_BOARD>....Target board. Possible options: nano, tx2, xavier, nx"
	log info "<option>......possible options:"
	log info "                       --complete...download everything needed for a complete flash"
	log info "                       		   ...(rootfs + uboot)"
	log_raw "\n"
}
#==============================================================================
# create whole setup directory tree
#==============================================================================
createDirectories()
{
	# create setup main directory, given by parameter
	if create_directory $PATH_TARGET_ROOT
	then
		log debug "Created setup main directory: ${PATH_TARGET_ROOT}"
		SUCCESS_FLAG=$TRUE
	else
		log debug "Could not create setup main directory"
	    SUCCESS_FLAG=$FALSE
	fi

	# create driver package directory
	if create_directory $PATH_TARGET_L4T
	then
		log debug "Created driver package directory: ${PATH_TARGET_L4T}"
		SUCCESS_FLAG=$TRUE
	else
		log debug "Could not create driver package directory"
	    SUCCESS_FLAG=$FALSE
	fi

	# create sources directory
	if create_directory $PATH_SOURCES
	then
		log debug "Created sources directory: ${PATH_SOURCES}"
		SUCCESS_FLAG=$TRUE
	else
		log debug "Could not create sources directory"
	    SUCCESS_FLAG=$FALSE
	fi

	# create GCC directory
	if [ ! -d "$PATH_DOWNLOADS_GCC" ]
	then
		if create_directory $PATH_TARGET_GCC
		then
			log debug "Created GCC directory: ${PATH_TARGET_GCC}"
			SUCCESS_FLAG=$TRUE
		else
			log debug "Could not create GCC directory"
			SUCCESS_FLAG=$FALSE
		fi

		# create GCC 64bit directory
		if create_directory $PATH_TARGET_GCC_64
		then
			log debug "Created GCC 64bit directory: ${PATH_TARGET_GCC_64}"
			SUCCESS_FLAG=$TRUE
		else
			log debug "Could not create GCC 64bit directory"
			SUCCESS_FLAG=$FALSE
		fi
	fi
}
#==============================================================================
# running script
#==============================================================================
log info "Running script for preparing Linux4Tegra setup"
log debug "dedicated for L4T: ${DEDICATED_VERSION} and Jetson ${DEDICATED_BOARD}"

# parameter check
#------------------------------------------------------------------------------
if ! parameter_exist $1 || ! parameter_exist $2
then
	log error "Missing parameter! Please provide L4T setup folder name and target board"
	SUCCESS_FLAG=$FALSE
	usage
	exit 1
fi

if proceed
then
	if check_parameter $2 "tx2" || check_parameter $2 "xavier" || check_parameter $2 nx
	then
		FILE_ROOTFS="$FILE_ROOTFS_TX2"
		DOWNLOAD_URL_ROOTFS="$DOWNLOAD_URL_ROOTFS_TX2"
		FILE_DRIVER_PACKAGE="$FILE_DRIVER_PACKAGE_TX2"
		DOWNLOAD_URL_DRIVER_PACKAGE="$DOWNLOAD_URL_DRIVER_PACKAGE_TX2"
		FILE_PUBLICSOURCES="$FILE_PUBLICSOURCES_TX2"
		DOWNLOAD_URL_PUBLICSOURCES="$DOWNLOAD_URL_PUBLICSOURCES_TX2"
	elif check_parameter $2 "nano" 
	then
		DEDICATED_BOARD="$DEDICATED_BOARD_NANO"
		FILE_ROOTFS="$FILE_ROOTFS_NANO"
		DOWNLOAD_URL_ROOTFS="$DOWNLOAD_URL_ROOTFS_NANO"
		FILE_DRIVER_PACKAGE="$FILE_DRIVER_PACKAGE_NANO"
		DOWNLOAD_URL_DRIVER_PACKAGE="$DOWNLOAD_URL_DRIVER_PACKAGE_NANO"
		FILE_PUBLICSOURCES="$FILE_PUBLICSOURCES_NANO"
		DOWNLOAD_URL_PUBLICSOURCES="$DOWNLOAD_URL_PUBLICSOURCES_NANO"
	else
		log error "Invalid parameter for <TARGET_BOARD>!"
		SUCCESS_FLAG=$FALSE
		usage
	fi
fi

if proceed && check_parameter $3 "--complete"
then
	COMPLETE_INSTALL=true
fi


# check if help was called
#------------------------------------------------------------------------------
if proceed && check_parameter $1 "help"
then
	usage
	exit 1
fi

# download resources
#------------------------------------------------------------------------------
if proceed
then
	if [ ! -f "$PATH_DOWNLOADS_DRIVER_PACKAGE/$FILE_DRIVER_PACKAGE" ]
	then
		if ! wget "$DOWNLOAD_URL_DRIVER_PACKAGE" -O "$PATH_DOWNLOADS_DRIVER_PACKAGE/$FILE_DRIVER_PACKAGE"
		then
			log error "Failed to download l4t driver package from url $DOWNLOAD_URL_DRIVER_PACKAGE"
			SUCCESS_FLAG=$FALSE
		fi
	fi

	if proceed && [ "$COMPLETE_INSTALL" = true ] && [ ! -f "$PATH_DOWNLOADS_ROOTFS/$FILE_ROOTFS" ]
	then
		if ! wget "$DOWNLOAD_URL_ROOTFS" -O "$PATH_DOWNLOADS_ROOTFS/$FILE_ROOTFS"
		then
			log error "Failed to download l4t rootfs from url $DOWNLOAD_URL_ROOTFS"
			SUCCESS_FLAG=$FALSE
		fi
	fi

	if proceed && [ "$COMPLETE_INSTALL" = true ] && [ ! -f "$PATH_DOWNLOADS_PUBLICSOURCES/$FILE_PUBLICSOURCES" ]
	then
		if ! wget "$DOWNLOAD_URL_PUBLICSOURCES" -O "$PATH_DOWNLOADS_PUBLICSOURCES/$FILE_PUBLICSOURCES"
		then
			log error "Failed to download l4t public sources from url $DOWNLOAD_URL_PUBLICSOURCES"
			SUCCESS_FLAG=$FALSE
		fi
	fi

	if proceed && [ ! -f "$PATH_DOWNLOADS_GCC/$FILE_GCC_64" ] 
	then
		if ! wget "$DOWNLOAD_URL_GCC" -O "$PATH_DOWNLOADS_GCC/$FILE_GCC_64"
		then
			log error "Failed to download gcc toolchain from url $DOWNLOAD_URL_GCC"
			SUCCESS_FLAG=$FALSE
		fi
	fi
fi

# install prerequisites
#------------------------------------------------------------------------------
if proceed
then
	log info "install prerequisites (lbzip2)"

	sudo apt update
	if ! sudo apt install lbzip2 device-tree-compiler build-essential python -y
	then
		SUCCESS_FLAG=$FALSE
	fi
fi

log info "Script execution"
if proceed
then 
	log success
else 
	log failed
	exit 1
fi
