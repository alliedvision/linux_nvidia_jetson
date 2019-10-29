# NVidia TX2 Allied Vision CSI-2 driver

## Overview

The scripts in this project will build and install Linux4Tegra with Allied Vision CSI-2 driver to the Jetson TX2.

Platform: TX2
L4T Version: 32.1

####################################################################################

## Setup

The setup script will prepare the directory structure, extract the file archives, etc.

Run the script with the following syntax:

$ ./setup.sh <WORK_DIR>
e.g. $ ./setup.sh work_dir


####################################################################################

## Build and install

The build script will build the kernel, modules, device tree files, bootloader.

Run the script with the following syntax:
(WORK_DIR should be the same as used for the setup script)

$ ./build.sh <WORK_DIR> <BUILD_OPTIONS> <COMPONENTS> <OPTIONS> 
e.g. $ ./build.sh work_dir all all


####################################################################################

## Deploy

The deploy script will install the CSI-2 driver to the TX2.
Please make sure to connect the TX2 to the host and to set it to recovery mode prior to using this script.

Run the script with the following syntax.
(WORK_DIR should be the same as used for the build and setup script):

$ ./deploy.sh <WORK_DIR> <COMMAND> 

There are 2 options for installing the CSI-2 driver.

1) Install the kernel image, modules and device tree to an existing system with L4T 32.1 installed
    * Please make sure you have installed L4T 32.1
    * Flash the device tree blob to the TX2 with the following command
        * $ ./deploy.sh work_dir flash-dtb
    * Create a tarball with the kernel image and modules
        * $ ./deploy.sh work_dir tarball
    * Copy the tarball to the TX2, extract it and run the install script on the TX2
        * $ ./install.sh

2) Erase the complete TX2 and flash a complete L4T
    * $ ./deploy.sh work_dir flash-all
