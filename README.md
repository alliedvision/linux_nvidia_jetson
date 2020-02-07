# NVidia Jetson Allied Vision CSI-2 driver

## Overview

The scripts in this project will build and install Allied Vision CSI-2 driver to the NVidia Jetson boards.

Platforms: Nano, TX2, Xavier
L4T Version: 32.2.1

####################################################################################

## Setup

The setup script will prepare the directory structure, extract the file archives, etc.

Run the script with the following syntax:

$ ./setup.sh <WORK_DIR> <TARGET_BOARD>
e.g. $ ./setup.sh work_dir nano


####################################################################################

## Build and install

If you have downloaded the precompiled binaries, skip this step and proceed with the Deploy section.

The build script will build the kernel, modules, device tree files, bootloader.

Run the script with the following syntax:
(WORK_DIR should be the same as used for the setup script)

$ ./build.sh <WORK_DIR> <TARGET_BOARD> <BUILD_OPTIONS> <COMPONENTS> <OPTIONS> 
e.g. $ ./build.sh work_dir nano all all


####################################################################################

## Deploy

The deploy script will install the CSI-2 driver to the target board.

The number of steps to take is depending on the target board:
* Nano: Follow step 1) to install the driver.
* TX2: Follow steps 1) and 2) to install the driver.
* Xavier: Follow steps 1), 2), and 3) to install the driver.

Run the script with the following syntax.
(WORK_DIR should be the same as used for the build and setup script):

$ ./deploy.sh <WORK_DIR> <TARGET_BOARD> <COMMAND> 

1) Replace the kernel image and modules (and device tree) of an existing system 
    * Please make sure you have installed L4T 32.2.1
    * If you have build the kernel from sources, create a tarball with the kernel image and modules. If you have downloaded the precompiled binaries, the tarball is already prepared and this step can be skipped.
        * $ ./deploy.sh work_dir <TARGET_BOARD> tarball # run on host
    * Copy the tarball to the target board, extract it and run the install script on the target board 
        * $ ./install.sh # run on target
2) Flash the device tree to the target board
    * Please make sure to connect the target board to the host and to set it to recovery mode prior to using this script.
    * Flash the device tree blob to the target board with the following command
    * $ ./deploy.sh work_dir <TARGET_BOARD> flash-dtb # run on host
3) Flash the kernel image to the target board
    * Please make sure to connect the target board to the host and to set it to recovery mode prior to using this script.
    * Flash the kernel image to the target board with the following command
    * $ ./deploy.sh work_dir <TARGET_BOARD> flash-kernel # run on host