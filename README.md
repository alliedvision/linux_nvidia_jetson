# NVIDIA Jetson driver (Beta)


Driver (Beta) for Allied Vision Alvium MIPI cameras for NVIDIA Jetson with JetPack 5.1.3 (L4T 35.5.0)     
https://developer.nvidia.com/embedded/jetpack
![Alvium camera](https://cdn.alliedvision.com/fileadmin/content/images/cameras/Alvium/various/alvium-cameras-models.png)

THE SOFTWARE IS PRELIMINARY AND STILL IN TESTING AND VERIFICATION PHASE AND IS PROVIDED ON AN “AS IS” AND “AS AVAILABLE” BASIS AND IS BELIEVED TO CONTAIN DEFECTS. A PRIMARY PURPOSE OF THIS EARLY ACCESS IS TO OBTAIN FEEDBACK ON PERFORMANCE AND THE IDENTIFICATION OF DEFECT SOFTWARE, HARDWARE AND DOCUMENTATION.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Overview

The scripts in this project build and install the Allied Vision MIPI camera driver to the NVIDIA Jetson boards.

Compatible platforms with JetPack 5.1.3 (L4T 35.5.0) : 

  - AGX Orin Developer Kit
  - AGX Xavier DevKit
  - Xavier NX DevKit
  - Auvidea carrier JNX30-PD with Xavier NX
  - Orin Nano Developer Kit (**NEW**)
  - Orin NX on the Orin Nano Developer Kit Carrier Board (**NEW**)

***Before starting the installation, make sure to create a backup of your Jetson system.***

## Prerequisites

### Host PC
The scripts for the driver installation require Git on the host PC.

### Install JetPack 5.1.3
 
Install JetPack 5.1.3 (L4T 35.5.0) as per NVIDIA's instructions
 https://developer.nvidia.com/embedded/jetpack      

Recommendation: Use NVIDIA SDK Manager to install JetPack and useful tools such as CUDA.   
https://docs.nvidia.com/sdk-manager/  




## Install Alvium MIPI driver

### Method A: Use precompiled binaries   
 
  Install the precompiled kernel, which includes the driver and an installation menu.   

  1. Extract the tarball on a host PC.   
  The tarball contains helper scripts and another tarball with the precompiled binaries named AlliedVision_NVidia_L4T_35.5.0.0_<git-rev>.tar.gz. 

2. Copy the tarball to the target board. 
3. On the target board, extract the tarball and run the included install script.   
4. Proceed with selecting options from the installation menu as described below.

### Method B: Cross-compile binaries from source      
  The scripts require a host PC with Ubuntu (we recommend version 20.04).

Download sources and scripts from https://github.com/alliedvision/linux_nvidia_jetson to the host PC.

**On the host PC:**

1. Execute `./build.py -b xavier`  
Optionally you can sign the resulting apt repository with a pgp key by adding the option `--sign <name of pgp key>`.

2. Copy the tarball to the target board. 

**On the target board:**

Extract the tarball and run the included install script. 
The script includes an **installation menu**, please use the option for your hardware: 

+ Option `none`: The Allied Vision camera driver is not loaded during bootup. To load the driver, reboot the board.

+ For Xavier NX on JNX30, select option `Auvidea JNX30`.   
For all other supported modules on Jetson DevKit, select option `2 cameras`.  

Reboot the board. Now you can use the driver. 

To change the configuration after the driver was installed, enter:

`dpkg-reconfigure avt-nvidia-l4t-bootloader`

Reboot the board to apply the changes.


 ## Additional information

**NEW**: For use with GenICam, download our new Vimba X SDK: 
https://www.alliedvision.com/en/products/software/vimba-x-sdk/

 :open_book:
 https://github.com/alliedvision/documentation/blob/master/NVIDIA.rst