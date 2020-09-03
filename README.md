# Beta Release

Allied Vision CSI-2 Alvium Camera Driver

This program is free software; you may redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

THE SOFTWARE IS PRELIMINARY AND STILL IN TESTING AND VERIFICATION PHASE AND IS PROVIDED ON AN “AS IS” AND “AS AVAILABLE” BASIS AND IS BELIEVED TO CONTAIN DEFECTS.
A PRIMARY PURPOSE OF THIS EARLY ACCESS IS TO OBTAIN FEEDBACK ON PERFORMANCE AND THE IDENTIFICATION OF DEFECT SOFTWARE, HARDWARE AND DOCUMENTATION.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

---


# NVIDIA Jetson driver

Driver for Allied Vision Alvium cameras with MIPI CSI-2 interface for NVIDIA Jetson with JetPack 4.4 (L4T 32.4.2)     
https://developer.nvidia.com/embedded/jetpack
![Alvium camera](https://cdn.alliedvision.com/fileadmin/content/images/cameras/Alvium/various/alvium-cameras-models.png)

## Overview

The scripts in this project build and install the Allied Vision CSI-2 driver to the NVIDIA Jetson boards.

Platforms: Nano, TX2, Xavier, Xavier NX
JetPack 4.4 (L4T 32.4.2)  
The scripts require Git on the host PC.

***Before starting the installation, make sure to create a backup of your Jetson system.***

## Prerequisites: Install JetPack 4.4 to Jetson Nano, TX2, or Xavier
 
1. Install JetPack 4.4 (L4T 32.4.2) as per NVIDIA's instructions https://developer.nvidia.com/jetpack-422-archive   
    Recommendation: Use NVIDIA SDK Manager to install JetPack and useful tools such as CUDA.   
    https://docs.nvidia.com/sdk-manager/  
	
2. Update and upgrade the host PC and the Jetson board:   
   `# sudo apt update`   
   `# sudo apt upgrade`

## Install Alvium CSI-2 driver to Jetson Nano, TX2, or Xavier

 **Method A: Use precompiled binaries**   
  Install the precompiled kernel including driver and installation instructions:   
  https://www.alliedvision.com/en/products/software/embedded-software-and-drivers/

  1. Extract the tarball on a host PC.

  2. The tarball contains helper scripts and another tarball with the precompiled binaries named AlliedVision_NVidia_L4T_32.4.2_<git-rev>.tar.gz.   
     Copy the tarball to the target board. On the target board, extract the tarball and run the included install script.   
     Nano and Xavier NX users: Reboot the board. Now you can use the driver. 

  3. TX2 and Xavier users: Continue with step 6 of Method B.

 **Method B: Cross-compile binaries from source**    
  These scripts require a host PC with Ubuntu (16.04 or 18.04) installed.

  1. Download sources and scripts from https://github.com/alliedvision/linux_nvidia_jetson to the host PC.   
     On the host PC:
    
  2. Run prepare.sh and setup.sh, which prepare the directory structure, extracts the file archive, etc.:   
     `$ ./prepare.sh <WORK_DIR> <TARGET_BOARD> # For example, $ ./prepare.sh work_dir nx`     
     `$ ./setup.sh <WORK_DIR> <TARGET_BOARD> # For example, $ ./setup.sh work_dir nx`     
	 
  3. Run build.sh, which builds the kernel, modules, device tree files, and the bootloader:   
     `# Use the same WORK_DIR for all scripts`   
     `# Example: $ ./build.sh work_dir nx all all`   
     `$ ./build.sh <WORK_DIR> <TARGET_BOARD> <BUILD_OPTIONS> <COMPONENTS> <OPTIONS>`    
	 
  4. Create a tarball with the kernel image and modules.   
     `$ ./deploy.sh <WORK_DIR> <TARGET_BOARD> tarball`
     `# Example: ./deploy.sh work_dir nx tarball`
		 
  5. Copy the tarball to the target board. On the target board, extract the tarball and run the included install script.   
     Nano and Xavier NX users: Reboot the board. Now you can use the driver. 

  6. TX2 and Xavier users: Connect the host PC to the USB recovery port of your board (TX2: Micro-B, Xavier: Type C) 

  7. Set TX2 or Xavier to Force USB Recovery Mode as per NVIDIA's instructions. 
     Optionally, check the connection: On the host PC, enter `lsusb` and make sure that "NVidia Corp" is listed.

  8. TX2 or Xavier - run the script on the host PC:   
      a) Flash the device tree blob to the target board:   
         `$ ./deploy.sh <WORK_DIR> <TARGET_BOARD> flash-dtb # Use the same WORK_DIR for all scripts`  
         TX2 users: Reboot the board. Now you can use the driver.

  9. Xavier users: Set Xavier to Force USB Recovery Mode and run deploy.sh again with parameter flash-kernel to flash the kernel image to the board.   
         `$ ./deploy.sh <WORK_DIR> <TARGET_BOARD> flash-kernel # Use the same WORK_DIR for all scripts`


 ## Known Limitations of the Beta

 This beta versions has some known limitations, including the following:

 * Missing or untested ioctls:
   * `VIDIOC_STREAMSTAT`: Field `packet_crc_error` missing.
   * `VIDIOC_RESET_STREAMSTAT`: incomplete.
   * `VIDIOC_DQBUF`: Custom flags `V4L2_BUF_FLAG_UNUSED`, `V4L2_BUF_FLAG_INVALID`, `V4L2_BUF_FLAG_INVALIDINCOMPLETE` not implemented.
 * Infrastructure:
   * SD card generation not working.
   * Other boards besides Xavier NX not implemented.

 
 ## Additional information
 :open_book:
 https://github.com/alliedvision/documentation/blob/master/NVIDIA.md

