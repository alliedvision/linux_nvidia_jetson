# NVIDIA Jetson TX2 driver
Driver for Allied Vision Alvium cameras with MIPI CSI-2 interface for Jetson TX2 with L4T 32.1 (contained in Jetpack 4.2, https://developer.nvidia.com/jetpack-4_2)

:new: Jetson Nano users: We have added a branch with a beta driver version:   
https://github.com/alliedvision/linux_nvidia_jetson/tree/nano-beta-release

***Before starting the installation, make sure to create a backup of your Jetson system.***

* Scenario 1: Jetson TX2 without any L4T installation
* Scenario 2: Jetson TX2 with L4T 32.1 (contained in Jetpack 4.2, https://developer.nvidia.com/jetpack-4_2)
* Scenario 3: Jetson TX2 with a different version than L4T 32.1 (not tested)



## Scenario 1: Jetson TX2 without any L4T installation
 
1. Install L4T 32.1 as per NVIDIA's instructions https://developer.nvidia.com/jetpack-4_2.   
    Recommendation: Use NVIDIA SDK Manager to install L4T 32.1 and useful tools such as CUDA.   
    https://docs.nvidia.com/sdk-manager/
 
2. To continue, see Scenario 2.



## Scenario 2: Jetson TX2 with L4T 32.1

 **Method A:**
 
  Download the precompiled kernel including driver and installation instructions:   
  https://www.alliedvision.com/en/products/software/embedded-software-and-drivers/

 **Method B:**  
  1. Download sources and scripts from https://github.com/alliedvision/linux_nvidia_jetson
     to the host PC.   
     On the host PC:
    
  2. Run setup.sh, which prepares the directory structure, extracts the file archive, etc.:   
     `$ ./setup.sh <WORK_DIR>  // For example, $ ./setup.sh work_dir`

  3. Run build.sh, which builds the kernel, modules, device tree files, and the bootloader:   
     `// Use the same WORK_DIR for all scripts`   
     `// Example: $ ./build.sh work_dir all all`   
     `$ ./build.sh <WORK_DIR> <BUILD_OPTIONS> <COMPONENTS> <OPTIONS>` 

  4. Connect the host PC to the recovery (USB Micro-B) port of Jetson TX2. 

  5. Set Jetson TX2 to Force USB Recovery Mode as per NVIDIA's instructions.

  6. Recommendation for most use cases:   
      a) Run deploy.sh with parameter flash-dtb to install the kernel including the camera driver   
         `$ ./deploy.sh <WORK_DIR> flash-dtb  // Use the same WORK_DIR for all scripts`    
      b) Run deploy.sh again with parameter tarball to create a tarball with the kernel binaries.   
         `$ ./deploy.sh <WORK_DIR> tarball  // Use the same WORK_DIR for all scripts`

     Optionally, to get a lean operating system, but with some possible restrictions:   
     ***WARNING: This will delete all existing data and settings on Jetson TX2. Using NVIDIA SDK Manager will be restricted. Some libraries or tools such as CUDA cannot be installed via SDK Manager after applying this version of the script.***   
     Run deploy.sh with parameter flash-all   
      `$ ./deploy.sh <WORK_DIR> flash-all // Use the same WORK_DIR for all scripts`   
     Run deploy.sh again with parameter tarball to create a tarball with the kernel binaries.   
      `$ ./deploy.sh <WORK_DIR> tarball  // Use the same WORK_DIR for all scripts`
      
  7. Copy the tarball to Jetson TX2 (for example, on an SD card)
  8. On Jetson TX2, unpack the tarball and run the included install.sh    
  9. Reboot Jetson TX2.
     


## Scenario 3: Jetson TX2 with a different version than L4T 32.1 (not tested)
 Most likely, this driver version and the scripts are compatible with L4T versions based on 
 the same kernel version as L4T 32.1 (kernel version 4.9).

 If you want to try the driver with a non-tested L4T version, we recommend:

 - Download sources and scripts from https://github.com/alliedvision/linux_nvidia_jetson

 - Check the scripts. Change them as required.
   setup.sh tries to download the following resources:
   - driverPackage
     URL:         https://developer.nvidia.com/embedded/dlc/l4t-jetson-driver-package-32-1-JAX-TX2
     Filename:    JAX-TX2-Jetson_Linux_R32.1.0_aarch64.tbz2
     Destination: resources/driverPackage
   - rootfs
     URL:         https://developer.nvidia.com/embedded/dlc/l4t-sample-root-filesystem-32-1-JAX-TX2
     Filename:    JAX-TX2-Tegra_Linux_Sample-Root-Filesystem_R32.1.0_aarch64.tbz2
     Destination: resources/rootfs
   - Linaro gcc toolchain
     URL:         https://developer.nvidia.com/embedded/dlc/kernel-gcc-6-4-tool-chain
     Filename:    gcc-linaro-6.4.1-2017.08-x86_64_aarch64-linux-gnu.tar.xz
     Destination: resources/gcc
   - L4T sources containing the U-Boot sources
     URL:         https://developer.nvidia.com/embedded/dlc/l4t-sources-32-1-JAX-TX2
     Filename:    JAX-TX2-public_sources.tbz2
     Destination: resources/public_sources

 - Proceed as described in Scenario 2, Method B.
 
 ## Additional information
 :open_book:
 https://github.com/alliedvision/documentation/blob/master/NVIDIA.md

