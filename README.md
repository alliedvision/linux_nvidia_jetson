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

# NVidia Jetson Allied Vision CSI-2 driver

## Overview

The scripts in this project will build and install Allied Vision CSI-2 driver to the Nvidia Jetson boards.

Platforms: Nano, TX2
L4T Version: 32.2.1

####################################################################################

## Setup

The setup script will prepare the directory structure, extract the file archives, etc.

Run the script with the following syntax:

$ ./setup.sh <WORK_DIR> <TARGET_BOARD>
e.g. $ ./setup.sh work_dir nano


####################################################################################

## Build and install

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
    * Create a tarball with the kernel image and modules
        * $ ./deploy.sh work_dir <TARGET_BOARD> tarball
    * Copy the tarball to the target board, extract it and run the install script on the target board
        * $ ./install.sh
2) Flash the device tree to the target board
    * Please make sure to connect the target board to the host and to set it to recovery mode prior to using this script.
    * Flash the device tree blob to the target board with the following command
    * $ ./deploy.sh work_dir <TARGET_BOARD> flash-dtb
3) Flash the kernel image to the target board
    * Please make sure to connect the target board to the host and to set it to recovery mode prior to using this script.
    * Flash the kernel image to the target board with the following command
    * $ ./deploy.sh work_dir <TARGET_BOARD> flash-kernel