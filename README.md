# NVIDIA Jetson driver

Driver for Allied Vision Alvium cameras with MIPI CSI-2 interface for NVIDIA Jetson with JetPack 4.5.1 (L4T 32.5.1)     
https://developer.nvidia.com/embedded/jetpack
![Alvium camera](https://cdn.alliedvision.com/fileadmin/content/images/cameras/Alvium/various/alvium-cameras-models.png)

## Overview

The scripts in this project build and install the Allied Vision CSI-2 driver to the NVIDIA Jetson boards.

Platforms: Nano, Nano 2GB, TX2, AGX Xavier, Xavier NX    
JetPack 4.5.1 (L4T 32.5.1)  
The scripts require Git on the host PC.

***Before starting the installation, make sure to create a backup of your Jetson system.***

## Prerequisites: Install JetPack 4.5.1 to Jetson Nano, Nano 2GB, TX2, AGX Xavier or Xavier NX
 
Install JetPack 4.5.1 (L4T 32.5.1) as per NVIDIA's instructions https://developer.nvidia.com/embedded/jetpack   
    Recommendation: Use NVIDIA SDK Manager to install JetPack and useful tools such as CUDA.   
    https://docs.nvidia.com/sdk-manager/  
	
### Accidental overwriting of the driver
As of JetPack 4.4, users can update L4T directly on the board with `apt-upgrade`. 
Doing this may install newer L4T kernel and device tree files, which overwrite the driver for Allied Vision cameras. 
If you use `apt-upgrade` nevertheless, please prevent overwriting the driver with `sudo apt-mark hold nvidia-l4t-kernel nvidia-l4t-kernel-dtbs`.
Note that both reinstalling the driver or putting the update on hold may cause unavailable features or bugfixes from NVIDIA.

## Install Alvium CSI-2 driver to Jetson Nano, Nano 2GB, TX2, AGX Xavier or Xavier NX

**Method A: Use precompiled binaries**   
 
  Install the precompiled kernel including driver and installation instructions.   
  For Jetson Nano, please use the file containing "a02" for A02 carrier boards and the other file for B01 carrier boards.

  1. Extract the tarball on a host PC.

  2. The tarball contains helper scripts and another tarball with the precompiled binaries named AlliedVision_NVidia_L4T_32.5.1_<git-rev>.tar.gz.   
     Copy the tarball to the target board. On the target board, extract the tarball and run the included install script.   
     Reboot the board. Now you can use the driver. 
	
**Method B: Use SD card images** 
	
 SD card images are now available as assets of the release.

 They are available for:
  - Nano 2GB
  - Nano 4GB (B01 carrier only)
  - Xavier NX

 Please note that the SD card images only run on boards with JetPack 4.5.1.   
 JetPack 4.5.1 must be installed before using an SD card image.

  1. Before writing the image to an SD card, decompress it with `lzma<filename>`.
	
  2. Flash the image to the microSD card, for example with balenaEtcher: https://www.balena.io/etcher/.
	
  3. Insert the microSD card and boot your Jetson board.	

 **Method C: Cross-compile binaries from source**      
  These scripts require a host PC with Ubuntu (18.04) installed.

  1. Download sources and scripts from https://github.com/alliedvision/linux_nvidia_jetson to the host PC.   
     On the host PC:
    
  2. Run setup.sh, which prepares the directory structure, extracts the file archive, etc.:   
     `$ ./setup.sh <WORK_DIR> <TARGET_BOARD> # For example, $ ./setup.sh work_dir nano` 
	 
  3. Run build.sh, which builds the kernel, modules, device tree files, and the bootloader:   
     `# Use the same WORK_DIR for all scripts`   
     `# Example: $ ./build.sh work_dir nano all all`   
     `$ ./build.sh <WORK_DIR> <TARGET_BOARD> <BUILD_OPTIONS> <COMPONENTS> <OPTIONS>`    
	 
  4. Create a tarball with the kernel image and modules.   
     `$ ./deploy.sh <WORK_DIR> <TARGET_BOARD> tarball`
		 
  5. Copy the tarball to the target board. On the target board, extract the tarball and run the included install script.   
     Reboot the board. Now you can use the driver. 

 ## Additional information
 :open_book:
 https://github.com/alliedvision/documentation/blob/master/NVIDIA.rst
