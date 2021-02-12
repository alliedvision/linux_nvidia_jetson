#!/bin/bash
#==============================================================================
#  Copyright (C) 2012 - 2019 Allied Vision Technologies.  All Rights Reserved.
#
#  Redistribution of this file, in original or modified form, without
#  prior written consent of Allied Vision Technologies is prohibited.
#
#------------------------------------------------------------------------------
# 
#  File:        build.sh
#
#  Description: build script for NVIDIA Jetson TX2 board.
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
DEDICATED_VERSION="R32.4.4"
DEDICATED_BOARD=""
DEDICATED_BOARD_TX2="TX2,XAVIER"
DEDICATED_BOARD_NANO="NANO"
DEFAULT_CONFIG_SELECTED="tegra_avcamera_defconfig"
DEFAULT_CONFIG_UBOOT=""
DEFAULT_CONFIG_UBOOT_TX2="p2771-0000-500_defconfig"
DEFAULT_CONFIG_UBOOT_NANO="p3450-porg_defconfig"
#==============================================================================
# path settings
#==============================================================================
#PATH_CURRENT=$(pwd)
PATH_CURRENT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
#------------------------------------------------------------------------------
PATH_DOWNLOADS="${PATH_CURRENT}/resources"
#------------------------------------------------------------------------------
PATH_TARGET_ROOT="${PATH_CURRENT}/${1}"
PATH_TARGET_L4T="${PATH_TARGET_ROOT}/Linux_for_Tegra"
PATH_TARGET_ROOTFS="${PATH_TARGET_L4T}/rootfs"
PATH_TARGET_KERNEL="${PATH_TARGET_L4T}/kernel"
PATH_TARGET_UBOOT=""
PATH_TARGET_UBOOT_TX2="${PATH_TARGET_L4T}/bootloader/t186ref/p2771-0000/500"
PATH_TARGET_UBOOT_NANO="${PATH_TARGET_L4T}/bootloader/t210ref/p3450-porg"

#------------------------------------------------------------------------------
PATH_TARGET_GCC="${PATH_TARGET_ROOT}/gcc"
PATH_TARGET_GCC_32="${PATH_TARGET_GCC}"
PATH_TARGET_GCC_64="${PATH_TARGET_GCC}/gcc-7.3.1-aarch64"
#------------------------------------------------------------------------------
#PATH_SOURCES="${PATH_TARGET_L4T}/sources"
PATH_SOURCES_KERNEL="${PATH_CURRENT}/../kernel/kernel-4.9"
PATH_SOURCES_UBOOT="${PATH_TARGET_ROOT}/public_sources/u-boot"
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
	log info "Usage: ${SCRIPT_NAME} <L4T> <TARGET_BOARD> <cmd> <component> --<option>"
	log info "<L4T>.........location of Linux4Tegra setup"
	log info "<TARGET_BOARD>....Target board. Possible options: nano, tx2, xavier, nx"
	log info "<cmd>.........command to executed"
	log info "                       all"
	log info "                       build"
	log info "                       clean"
	log info "                       copy"
	log info "<component>...component to be build/cleaned/copied"
	log info "                       all"
	log info "                       kernel"
	log info "                       modules"
	log info "                       dtbs"
	log info "<option>......possible options:"
	log info "                       --no-clean...e.g. to do a re-build and copy for the kernel only."
	log info "                                    >>${0} someFolder all kernel --no-clean"
	log info "                       --uboot......e.g. build and copy the bootloader u-boot"
	log info "                                    >>${0} someFolder all all --uboot"
	log_raw "\n"
}
#==============================================================================
# function for cleaning kernel sources
#==============================================================================
cleanKernelSources()
{
	# go to v4.4 kernel source directory
	cd $PATH_SOURCES_KERNEL

	# clean kernel sources
	#if sudo time make O=$TEGRA_KERNEL_OUT clean
	if sudo time make clean
	then
		log debug "Kernel sources has been cleaned"
		SUCCESS_FLAG=$TRUE
	else
		log debug "Could not clean kernel sources"
		SUCCESS_FLAG=$FALSE
	fi

	cd $PATH_CURRENT
}
#==============================================================================
# function for cleaning boot loader sources
#==============================================================================
cleanUbootSources()
{
	# go to bootloder source directory
	cd $PATH_SOURCES_UBOOT
	
	# clean bootloader sources
	if time make distclean
	then
		log debug "U-boot sources has been cleaned"
		SUCCESS_FLAG=$TRUE
	else
		log debug "Could not clean u-boot sources"
		SUCCESS_FLAG=$FALSE
	fi

	cd $PATH_CURRENT
}
#==============================================================================
# function to configure kernel sources
#==============================================================================
configureKernel()
{
	# go to kernel source directory
	cd $PATH_SOURCES_KERNEL

	# configure kernel sources
	if time make -j$(nproc) $DEFAULT_CONFIG_SELECTED
	then
		log debug "Default config '${DEFAULT_CONFIG_SELECTED}' has been created"
		SUCCESS_FLAG=$TRUE
	else
		log debug "Could not create default config '${DEFAULT_CONFIG_SELECTED}'"
		SUCCESS_FLAG=$FALSE
	fi

	cd $PATH_CURRENT
}
#==============================================================================
# function for building kernel image
#==============================================================================
buildKernelImage()
{
	# go to kernel source directory
	cd $PATH_SOURCES_KERNEL

	# compile compressed kernel image
	if time make -j$(nproc) zImage
	then
		log debug "Kernel image has been created"
		SUCCESS_FLAG=$TRUE
	else
		log debug "Could not create kernel image"
		SUCCESS_FLAG=$FALSE
	fi

	cd $PATH_CURRENT
}
#==============================================================================
# function for building kernel modules
#==============================================================================
buildModules()
{
	# go to kernel source directory
	cd $PATH_SOURCES_KERNEL

	# workaround
  if [ -f drivers/video/tegra/host/nvhost_events_json.h ]; then
    git update-index --assume-unchanged drivers/video/tegra/host/nvhost_events_json.h
  fi

	# compile kernel modules
	if time make -j$(nproc) modules
	then
		log debug "Kernel modules have been built"
		SUCCESS_FLAG=$TRUE
	else
		log debug "Could not build kernel modules"
		SUCCESS_FLAG=$FALSE
	fi

	cd $PATH_CURRENT
}
#==============================================================================
# function for building device tree files
#==============================================================================
buildDeviceTree()
{
	# go to kernel source directory
	cd $PATH_SOURCES_KERNEL

	# compile device trees
	if time make -j$(nproc) dtbs
	then
		log debug "Device tree files have been built"
		SUCCESS_FLAG=$TRUE
	else
		log debug "Could not build device tree files"
		SUCCESS_FLAG=$FALSE
	fi

	cd $PATH_CURRENT
}
#==============================================================================
# function to configure bootloader sources
#==============================================================================
configureBootloader()
{
	# go to bootloader directory
	cd $PATH_SOURCES_UBOOT	

	# configure bootloader sources
	if time make -j$(nproc) $DEFAULT_CONFIG_UBOOT
	then
		log debug "Default config '${DEFAULT_CONFIG_UBOOT}' has been created"
		SUCCESS_FLAG=$TRUE
	else
		log debug "Could not create default config '${DEFAULT_CONFIG_UBOOT}'"
		SUCCESS_FLAG=$FALSE
	fi

	cd $PATH_CURRENT
}
#==============================================================================
# function for building boot loader
#==============================================================================
buildBootloader()
{
	# go to bootloader directory
	cd $PATH_SOURCES_UBOOT	

	# compile u-boot bootloader
	if time make -j$(nproc)
	then
		log debug "Bootloader sources have been built"
		SUCCESS_FLAG=$TRUE
	else
		log debug "Could not build bootloader sources"
		SUCCESS_FLAG=$FALSE
	fi

	cd $PATH_CURRENT
}
#==============================================================================
# function for copying kernel image
#==============================================================================
copyKernelImage()
{
	#if sudo cp $TEGRA_KERNEL_OUT/arch/arm64/boot/Image $PATH_TARGET_L4T/kernel
	if sudo cp $PATH_SOURCES_KERNEL/arch/arm64/boot/Image $PATH_TARGET_L4T/kernel
	then
		log debug "Kernel image has been copied"
		SUCCESS_FLAG=$TRUE
	else
		log debug "Could not copy kernel image"
		SUCCESS_FLAG=$FALSE
	fi
}
#==============================================================================
# function for copying kernel image
#==============================================================================
copyKernelImageComp()
{
	#if sudo cp $TEGRA_KERNEL_OUT/arch/arm64/boot/zImage $PATH_TARGET_L4T/kernel
	if sudo cp $PATH_SOURCES_KERNEL/arch/arm64/boot/zImage $PATH_TARGET_L4T/kernel
	then
		log debug "Kernel image (compressed) has been copied"
		SUCCESS_FLAG=$TRUE
	else
		log debug "Could not copy kernel image (compressed)"
		SUCCESS_FLAG=$FALSE
	fi
}
#==============================================================================
# function for copying kernel modules
#==============================================================================
copyModules()
{
	# go to kernel source directory
	cd $PATH_SOURCES_KERNEL

	# call modules install (copy kernel objects)
	if time sudo -E make -j$(nproc) modules_install
	then 
		log debug "Kernel modules has been copied"
		SUCCESS_FLAG=$TRUE
	else 
		log debug "Could not copy kernel modules"
		SUCCESS_FLAG=$FALSE
	fi

	cd $PATH_CURRENT
}
#==============================================================================
# function for copying device tree blobs
#==============================================================================
copyDeviceTree()
{
	if sudo cp -R $PATH_SOURCES_KERNEL/arch/arm64/boot/dts/* $PATH_TARGET_KERNEL/dtb
	then
		log debug "Device tree files has been copied"
		SUCCESS_FLAG=$TRUE
	else
		log debug "Could not copy device tree files"
		SUCCESS_FLAG=$FALSE
	fi
}
#==============================================================================
# function for copying bootloader binaries
#==============================================================================
copyBootloader()
{
	
	if sudo cp $PATH_SOURCES_UBOOT/u-boot $PATH_TARGET_UBOOT
	then
		log debug "U-boot file has been copied"
		SUCCESS_FLAG=$TRUE
	else
		log debug "Could not copy u-boot file"
		SUCCESS_FLAG=$FALSE
	fi

	if sudo cp $PATH_SOURCES_UBOOT/u-boot.bin $PATH_TARGET_UBOOT
	then
		log debug "U-boot binary file has been copied"
		SUCCESS_FLAG=$TRUE
	else
		log debug "Could not copy u-boot binary file"
		SUCCESS_FLAG=$FALSE
	fi

	if sudo cp $PATH_SOURCES_UBOOT/u-boot.dtb $PATH_TARGET_UBOOT
	then
		log debug "U-boot device tree file has been copied"
		SUCCESS_FLAG=$TRUE
	else
		log debug "Could not copy u-boot device tree file"
		SUCCESS_FLAG=$FALSE
	fi

	if sudo cp $PATH_SOURCES_UBOOT/u-boot-dtb.bin $PATH_TARGET_UBOOT
	then
		log debug "U-boot device tree binary file has been copied"
		SUCCESS_FLAG=$TRUE
	else
		log debug "Could not copy u-boot device tree binary file"
		SUCCESS_FLAG=$FALSE
	fi
}
#==============================================================================
# running script
#==============================================================================
log info "Running script for building Linux4Tegra setup"
log debug "dedicated for L4T: ${DEDICATED_VERSION} and Jetson ${DEDICATED_BOARD}"

# parameter check
#------------------------------------------------------------------------------
if ! parameter_exist $1 || ! parameter_exist $2 || ! parameter_exist $3 || ! parameter_exist $4
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


# check target board param
#------------------------------------------------------------------------------
if proceed
then
	if check_parameter $2 "tx2" || check_parameter $2 "xavier" || check_parameter $2 "nx"
	then
		DEDICATED_BOARD="$DEDICATED_BOARD_TX2"
		DEFAULT_CONFIG_UBOOT="$DEFAULT_CONFIG_UBOOT_TX2"
		PATH_TARGET_UBOOT="$PATH_TARGET_UBOOT_TX2"
	elif check_parameter $2 "nano" 
	then
		DEDICATED_BOARD="$DEDICATED_BOARD_NANO"
		DEFAULT_CONFIG_UBOOT="$DEFAULT_CONFIG_UBOOT_NANO"
		PATH_TARGET_UBOOT="$PATH_TARGET_UBOOT_NANO"
	else
		log error "Invalid parameter for <TARGET_BOARD>!"
		SUCCESS_FLAG=$FALSE
		usage
		exit 1
	fi
fi


# parameter check all options for <cmd>
#------------------------------------------------------------------------------
if proceed
then
	if ! ( check_parameter $3 "all" || check_parameter $3 "build" || check_parameter $3 "clean" || check_parameter $3 "copy" )
	then
		log error "Invalid parameter for <cmd>!"
		SUCCESS_FLAG=$FALSE
		usage
	fi
fi

# parameter check all options for <component>
#------------------------------------------------------------------------------
if proceed
then
	# check clean option
	if ! ( check_parameter $4 "all" || check_parameter $4 "kernel" || check_parameter $4 "modules" || check_parameter $4 "dtbs" || check_parameter $4 "u-boot" )
	then
		log error "Invalid parameter for <component>!"
		SUCCESS_FLAG=$FALSE
		usage
	fi
fi

# export variables for compiling
#------------------------------------------------------------------------------
if proceed
then
	log info "Export variables for compiling"

	export ARCH=arm64
	export CROSS_COMPILE="${PATH_TARGET_GCC_64}/bin/aarch64-linux-gnu-"
	#export TEGRA_KERNEL_OUT=$PATH_SOURCES_KERNEL
	export INSTALL_MOD_PATH=$PATH_TARGET_ROOTFS
	export LANG=C

	log debug "ARCH=${ARCH}"
	log debug "CROSS_COMPILE=${CROSS_COMPILE}"
	log debug "INSTALL_MOD_PATH=${INSTALL_MOD_PATH}"
	#log debug "TEGRA_KERNEL_OUT=${TEGRA_KERNEL_OUT}"
fi

# clean sources
#------------------------------------------------------------------------------
if proceed && ! check_parameter $5 "--no-clean"
then
	if check_parameter $3 "clean" || check_parameter $3 "all"
	then
		# clean kernel sources
		if check_parameter $4 "kernel" || check_parameter $4 "all"
		then
			log info "Clean kernel sources"
			cleanKernelSources
			if proceed
			then
				log success
			else
				log failed
			fi
		fi

		if check_parameter $5 "--uboot"
		then
			# clean u-boot sources
			if check_parameter $4 "all"
			then
				log info "Clean u-boot sources"
				cleanUbootSources
				if proceed
				then
					log success
				else
					log failed
				fi
			fi
		fi
	fi
else
	log debug "Sources will not be cleaned"
fi

# build Linux4Tegra, if selected by user
#------------------------------------------------------------------------------
if proceed
then
	if check_parameter $3 "build" || check_parameter $3 "all"
	then
		# configure and build kernel
		if check_parameter $4 "kernel" || check_parameter $4 "all"
		then
			log info "Configure kernel"
			configureKernel
			if proceed
			then
				log success
			else
				log failed
			fi

			if proceed
			then
				log info "Build kernel image"
				buildKernelImage
				if proceed
				then
					log success
				else
					log failed
				fi
			fi
		fi

		# build modules
		if check_parameter $4 "modules" || check_parameter $4 "all"
		then
			log info "Build kernel modules"
			buildModules
			if proceed
			then
				log success
			else
				log failed
			fi
		fi

		# build device trees
		if check_parameter $4 "dtbs" || check_parameter $4 "all"
		then
			log info "Build kernel modules"
			buildDeviceTree
			if proceed
			then
				log success
			else
				log failed
			fi
		fi

		# build u-boot
		if check_parameter $5 "--uboot"
		then
			if check_parameter $4 "all"
			then
				log info "Configure u-boot bootloader"
				configureBootloader
				if proceed
				then
					log success
				else
					log failed
				fi

				if proceed
				then
					log info "Build u-boot bootloader"
					buildBootloader
					if proceed
					then
						log success
					else
						log failed
					fi
				fi
			fi
		fi		
	fi
fi

# copy built Linux4Tegra
#------------------------------------------------------------------------------
if proceed
then
	if check_parameter $3 "all" || check_parameter $3 "build"
	then
		# copy kernel image(s)
		if check_parameter $4 "kernel" || check_parameter $4 "all"
		then	
			log info "Copy kernel image(s)"
			copyKernelImage
			copyKernelImageComp
			if proceed
			then
				log success
			else
				log failed
			fi	
		fi

		# copy kernel modules
		if check_parameter $4 "modules" || check_parameter $4 "all"
		then	
			log info "Copy kernel modules"
			copyModules
			if proceed
			then
				log success
			else
				log failed
			fi	
		fi

		# copy device trees
		if check_parameter $4 "dtbs" || check_parameter $4 "all"
		then	
			log info "Copy device tree files"
			copyDeviceTree
			if proceed
			then
				log success
			else
				log failed
			fi	
		fi

		# copy bootloader
		if check_parameter $5 "--uboot"
		then
			if check_parameter $4 "all"
			then
				log info "Copy u-boot bootloader"
				copyBootloader
				if proceed
				then
					log success
				else
					log failed
				fi
			fi
		fi
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
