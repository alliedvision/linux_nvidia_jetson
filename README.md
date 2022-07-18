# NVIDIA Jetson driver (Beta)


Driver (Beta) for Allied Vision Alvium MIPI cameras for NVIDIA Jetson with JetPack 5.0.1 DP (L4T 34.1.1)     
https://developer.nvidia.com/embedded/jetpack
![Alvium camera](https://cdn.alliedvision.com/fileadmin/content/images/cameras/Alvium/various/alvium-cameras-models.png)

THE SOFTWARE IS PRELIMINARY AND STILL IN TESTING AND VERIFICATION PHASE AND IS PROVIDED ON 
AN “AS IS” AND “AS AVAILABLE” BASIS AND IS BELIEVED TO CONTAIN DEFECTS. A PRIMARY PURPOSE 
OF THIS EARLY ACCESS IS TO OBTAIN FEEDBACK ON PERFORMANCE AND THE IDENTIFICATION OF 
DEFECT SOFTWARE, HARDWARE AND DOCUMENTATION.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING 
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Overview

The scripts in this project build and install the Allied Vision MIPI camera driver to the NVIDIA Jetson boards.

Compatible platforms with JetPack 5.0.1 DP (L4T 34.1.1) : 

+ AGX Xavier NVIDIA developer kit (including optional use of Allied Vision's adapter for up to 6 cameras) 
+ Xavier NX NVIDIA developer kit
+ Xavier NX on [Auvidea JNX30-PD](https://auvidea.eu/product/38401/) for Alvium cameras with FPD Link interface.
+ AGX Orin NVIDIA developer kit


The scripts require Git on the host PC.

***Before starting the installation, make sure to create a backup of your Jetson system.***

## Prerequisites: Install JetPack 5.0.1 Developer Preview
 
Install JetPack 5.0.1 DP (L4T 34.1.1) as per NVIDIA's instructions
 https://developer.nvidia.com/embedded/jetpack      

Recommendation: Use NVIDIA SDK Manager to install JetPack and useful tools such as CUDA.   
https://docs.nvidia.com/sdk-manager/  


## Install Alvium MIPI driver

### Method A: Use precompiled binaries   
 
  Install the precompiled kernel, which includes the driver and an installation menu.   

  1. Extract the tarball on a host PC.   
  The tarball contains helper scripts and another tarball with the precompiled binaries named AlliedVision_NVidia_L4T_34.1.1_<git-rev>.tar.gz. 

2. Copy the tarball to the target board. 
3. On the target board, extract the tarball and run the included install script.   
4. Proceed with selecting options from the installation menu as described below.

### Method B: Cross-compile binaries from source      
  The scripts require a host PC with Ubuntu (we recommend version 18.04).

Download sources and scripts from https://github.com/alliedvision/linux_nvidia_jetson to the host PC.

**On the host PC:**

1. Execute `./build.py -b <xavier>`  
Use "xavier" for AGX Xavier, NX Xavier, and AGX Orin.

2. Copy the tarball to the target board. 

**On the target board:**

Extract the tarball and run the included install script. 
The script includes an **installation menu**, please use the option for your hardware: 

+ Option `none`: The Allied Vision camera driver is not loaded during bootup. To load the driver, reboot the board.

+ For NX Xavier and AGX Orin, select option `devkit`.   
For Xavier AGX, select the option `2 cameras` or `6 cameras`.

Reboot the board. Now you can use the driver. 

To change the configuration after the driver was installed, enter:

`dpkg-reconfigure avt-nvidia-l4t-bootloader`

Reboot the board to apply the changes.


 ## Additional information
 :open_book:
 https://github.com/alliedvision/documentation/blob/master/NVIDIA.rst
