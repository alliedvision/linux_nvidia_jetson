#!/bin/bash
#==============================================================================
#  Copyright (C) 2012 - 2019 Allied Vision Technologies.  All Rights Reserved.
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
DEDICATED_VERSION="R32.4.2"
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
FILE_DRIVER_PACKAGE_TX2="Jetson_Linux_R32.4.2_aarch64.tbz2"
FILE_DRIVER_PACKAGE_NANO="Tegra210_Linux_32.4.2_aarch64.tbz2"
FILE_ROOTFS=""
FILE_ROOTFS_TX2="Tegra_Linux_Sample-Root-Filesystem_R32.4.2_aarch64.tbz2"
FILE_ROOTFS_NANO="nano_Tegra_Linux_Sample-Root-Filesystem_R32.4.2_aarch64.tbz2"
FILE_PUBLICSOURCES=""
FILE_PUBLICSOURCES_TX2="public_sources.tbz2"
FILE_PUBLICSOURCES_NANO="nano_public_sources.tbz2"
FILE_GCC_64="gcc-linaro-7.3.1-2018.05-x86_64_aarch64-linux-gnu.tar.xz"
#==============================================================================
# download urls
#==============================================================================
DOWNLOAD_URL_DRIVER_PACKAGE=""
DOWNLOAD_URL_DRIVER_PACKAGE_TX2="https://developer.nvidia.com/embedded/L4T/r32_Release_v4.2/t186ref_release_aarch64/Tegra186_Linux_R32.4.2_aarch64.tbz2"
DOWNLOAD_URL_DRIVER_PACKAGE_NANO="https://developer.nvidia.com/embedded/L4T/r32_Release_v4.2/t210ref_release_aarch64/Tegra210_Linux_R32.4.2_aarch64.tbz2"
DOWNLOAD_URL_ROOTFS=""
DOWNLOAD_URL_ROOTFS_TX2="https://developer.nvidia.com/embedded/L4T/r32_Release_v4.2/t186ref_release_aarch64/Tegra_Linux_Sample-Root-Filesystem_R32.4.2_aarch64.tbz2"
DOWNLOAD_URL_ROOTFS_NANO="https://developer.nvidia.com/embedded/L4T/r32_Release_v4.2/t210ref_release_aarch64/Tegra_Linux_Sample-Root-Filesystem_R32.4.2_aarch64.tbz2"
DOWNLOAD_URL_PUBLICSOURCES=""
DOWNLOAD_URL_PUBLICSOURCES_TX2="https://developer.nvidia.com/embedded/L4T/r32_Release_v4.2/Sources/T186/public_sources.tbz2"
DOWNLOAD_URL_PUBLICSOURCES_NANO="https://developer.nvidia.com/embedded/L4T/r32_Release_v4.2/Sources/T210/public_sources.tbz2"
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
	if [ ! -d "$PATH_TARGET_GCC" ]
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
  mkdir -p "$PATH_DOWNLOADS_DRIVER_PACKAGE"
  mkdir -p "$PATH_DOWNLOADS_ROOTFS"
  mkdir -p "$PATH_DOWNLOADS_PUBLICSOURCES"
  mkdir -p "$PATH_DOWNLOADS_GCC"


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

	if proceed && [ ! -f "$PATH_DOWNLOADS_GCC/$FILE_GCC_64" ] && [ ! -d "$PATH_DOWNLOADS_DTB" ]
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
if proceed && [ ! -d "$PATH_DOWNLOADS_DTB" ]
then
	log info "install prerequisites (lbzip2)"

	sudo apt update
	if ! sudo apt install lbzip2 device-tree-compiler build-essential python -y
	then
		SUCCESS_FLAG=$FALSE
	fi
fi


# create setup directory (including sub-directories)
#------------------------------------------------------------------------------
if proceed
then
	log info "Create Linux4Tegra setup directory to: ${PATH_TARGET_ROOT}"

	# check if directory does not exist already
	if ! directory_exist $PATH_TARGET_ROOT
	then
		log debug "Directory does not exist"
		SUCCESS_FLAG=$TRUE
	else
		# directory already exists, remove it
		log debug "Directory already exists"
		if sudo rm -Rf $PATH_TARGET_ROOT
		then
			log debug "Directory removed"
			SUCCESS_FLAG=$TRUE
		else
			log debug "Could not remove directory"
			SUCCESS_FLAG=$FALSE
		fi
	fi
fi

if proceed
then
	if createDirectories
	then
		log success
	else
		log failed
	fi
else
	log failed
fi

# extract public sources (u-boot)
#------------------------------------------------------------------------------
if proceed && [ "$COMPLETE_INSTALL" = true ]
then
	log info "Extract public sources ${DEDICATED_VERSION} and ${DEDICATED_BOARD}"
	log debug "path: ${PATH_TARGET_ROOT}"
	log debug "file: ${FILE_DRIVER_PACKAGE}" 
	
	if tar xvjf "${PATH_DOWNLOADS_PUBLICSOURCES}/${FILE_PUBLICSOURCES}" -C ${PATH_TARGET_ROOT}
	then
		if tar xvjf "${PATH_TARGET_ROOT}/public_sources/u-boot_src.tbz2" -C "${PATH_TARGET_ROOT}/public_sources"
		then
			log success
			SUCCESS_FLAG=$TRUE
		else
			log failed
			SUCCESS_FLAG=$FALSE
		fi
	else
		log failed
		SUCCESS_FLAG=$FALSE
	fi
fi

# extract driver package
#------------------------------------------------------------------------------
if proceed
then
	log info "Extract Linux4Tegra driverPackage for ${DEDICATED_VERSION} and ${DEDICATED_BOARD}"
	log debug "path: ${PATH_TARGET_L4T}"
	log debug "file: ${FILE_DRIVER_PACKAGE}" 
	
	if sudo tar xvjf "${PATH_DOWNLOADS_DRIVER_PACKAGE}/${FILE_DRIVER_PACKAGE}" -C ${PATH_TARGET_ROOT}
	then
		log success
		SUCCESS_FLAG=$TRUE
	else
		log failed
		SUCCESS_FLAG=$FALSE
	fi
fi


# extract Linux4Tegra root file system
#------------------------------------------------------------------------------
if proceed && [ "$COMPLETE_INSTALL" = true ] 
then
	log info "Extract Linux4Tegra root file system for ${DEDICATED_VERSION} and ${DEDICATED_BOARD}"
	log debug "path: ${PATH_TARGET_ROOTFS}"
	log debug "file: ${FILE_ROOTFS}" 
	
	if sudo tar xvjf $PATH_DOWNLOADS_ROOTFS/$FILE_ROOTFS -C $PATH_TARGET_ROOTFS
	then
		log success 
		SUCCESS_FLAG=$TRUE
	else
		log failed
		SUCCESS_FLAG=$FALSE
	fi
fi

# apply L4T binaries to rootfs
#------------------------------------------------------------------------------
if proceed && [ "$COMPLETE_INSTALL" = true ] 
then
	log info "Apply Linux4Tegra binaries to rootfs"

	cd $PATH_TARGET_L4T
	if sudo ./apply_binaries.sh
	then
		log success
		SUCCESS_FLAG=$TRUE
	else
		log failed
		SUCCESS_FLAG=$FALSE
	fi
	cd $PATH_CURRENT
fi

# setup cross compiler gcc 32bit
#------------------------------------------------------------------------------
#if proceed
#then
	# setup gcc
	#log info "Setup 32bit GCC for u-boot"
	#log debug "path: ${PATH_TARGET_GCC_32},"

	# extract gcc 32bit
	#if tar xvf $PATH_DOWNLOADS_GCC/$FILE_GCC_32 --strip-components=1 -C $PATH_TARGET_GCC_32
	#then
	#	log debug "Extracted gcc binaries of ${FILE_GCC_32}"
	#	SUCCESS_FLAG=$TRUE
	#else
	#	log debug "Could not extract gcc binaries of ${FILE_GCC_32}"
	#	SUCCESS_FLAG=$FALSE
	#fi
#fi

# setup cross compiler gcc 64bit
#------------------------------------------------------------------------------
if proceed 
then
	# setup gcc
	log info "Setup 64bit GCC for kernel"
	log debug "path: ${PATH_TARGET_GCC_64}"

	# extract gcc 64bit
	if tar -xf $PATH_DOWNLOADS_GCC/$FILE_GCC_64 --strip-components=1 -C $PATH_TARGET_GCC_64
	then
		log debug "Extracted gcc binaries of ${FILE_GCC_64}"
		SUCCESS_FLAG=$TRUE
	else
		log debug "Could not extract gcc binaries of ${FILE_GCC_64}"
		SUCCESS_FLAG=$FALSE
	fi
fi

# copy dtb in case of binaries deployment
#------------------------------------------------------------------------------
if proceed && [ -d "$PATH_DOWNLOADS_DTB" ]
then
    # copy dtb to l4t folder
    if sudo cp ${PATH_DOWNLOADS_DTB}/*.dtb "${PATH_TARGET_DTB}"
    then
        log success
    else
        log failed
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
