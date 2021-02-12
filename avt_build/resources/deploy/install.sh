#!/bin/bash
#==============================================================================
# Copyright (C) 2019 Allied Vision Technologies.  All Rights Reserved.
#
# Redistribution of this file, in original or modified form, without
# prior written consent of Allied Vision Technologies is prohibited.
#
#------------------------------------------------------------------------------
#
# File:         -install.sh
#
# Description:  -bash script for installing the kernel and CSI-2 driver
#
#------------------------------------------------------------------------------
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF TITLE,
# NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR  PURPOSE ARE
# DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#==============================================================================

NC='\033[0m'
RED='\033[0;31m'
GREEN='\033[0;32m'
REQ_MACHINE="NVidia Jetson"
REQ_KERNEL="4.9.140"
DEST="/boot"

echo -e ${RED}"Allied Vision"${NC}" MIPI CSI-2 camera driver for "${GREEN}${REQ_MACHINE}${NC}" (kernel "${REQ_KERNEL}")"


# ========================================================= usage function
usage() {
  echo -e "Options:"  
  echo -e "-h:\t Display help"
  echo -e "-y:\t Automatic install and reboot"
  exit 0
}

function install_dtb_extlinux() {
  ELC=/boot/extlinux/extlinux.conf
  echo "Updating extlinux configuration in $ELC."
  if sed -n '/^LABEL primary$/,/^$/p' $ELC | grep -Eq '^\s+FDT\s+/boot'; then
    echo "Device tree already configured in $ELC, removing old entry."
    sudo sed -i '/^\s\+FDT\s\+\/boot/d' $ELC
  fi

  sudo sed -i '/^\s\+INITRD\s\+/a \ \ \ \ \ \ FDT /boot/dtb/'$1 $ELC
}

function valid_nano_model() {
  case "$NANO_CARRIER" in
  b00)
    true
    return
    ;;
  a02)
    true
    return
    ;;
  *)
    if [ ! -z "$NANO_CARRIER" ]; then
      echo "Invalid Nano carrier board selection"
    fi
    ;;
  esac
  false
}

inst() {
  MODEL="$(tr -d '\000' < /proc/device-tree/model)"


  # On Nano, write signed dtb to partition
  if [[ "$MODEL" == *"Nano 2GB"* ]]; then
    echo "Deploying JETSON NANO 2GB DTB"
    install_dtb_extlinux tegra210-p3448-0003-p3542-0000.dtb

  elif [[ "$MODEL" == *"Nano"* ]]; then
    echo "Deploying JETSON NANO DTB"
    while ! valid_nano_model; do
      echo "Which Jetson Nano Module and Carrier Board revision?"
      echo "  [1] NVIDIA P3448/3449-A02 (Single CSI port)"
      echo "  [2] NVIDIA P3448/3449-B00 (Two CSI ports)"
      echo -n "Selection? "
      read carrier_select
      case "$carrier_select" in
      1)
        NANO_CARRIER=a02
        ;;
      2)
        NANO_CARRIER=b00
        ;;
      esac
    done
    NANO_DTB=tegra210-p3448-0000-p3449-0000-$NANO_CARRIER.dtb
    sudo dd if="$NANO_DTB.encrypt" of=/dev/disk/by-partlabel/DTB status=none
    sync

  # On Xavier, write signed kernel+dtb+initrd to partition
  elif [[ "$MODEL" == *"AGX"* ]]; then
    echo "Deploying AGX XAVIER DTB"
    sudo nvbootctrl set-active-boot-slot 0
    sudo dd if=boot_sigheader.img.encrypt of=/dev/disk/by-partlabel/kernel >/dev/null 2>&1
    sudo dd if=tegra194-p2888-0001-p2822-0000_sigheader.dtb.encrypt of=/dev/disk/by-partlabel/kernel-dtb >/dev/null 2>&1

    sync

  elif [[ "$MODEL" == *"Xavier NX Developer Kit"* ]]; then
    echo "Deploying XAVIER NX DTB"
    sudo nvbootctrl set-active-boot-slot 0
    sudo dd if=boot_sigheader.img.encrypt of=/dev/disk/by-partlabel/kernel >/dev/null 2>&1
    sudo dd if=tegra194-p3668-all-p3509-0000_sigheader.dtb.encrypt of=/dev/disk/by-partlabel/kernel-dtb >/dev/null 2>&1

  elif [[ "$MODEL" == "quill" ]]; then
    echo "Deploying JETSON TX2 DTB"

    sudo nvbootctrl set-active-boot-slot 0
    install_dtb_extlinux tegra186-quill-p3310-1000-c03-00-base.dtb

  fi

  # All boards: copy Image + DTBs in rootfs
  sudo cp -r Image* $DEST
  sudo cp -r *.dtb $DEST
  if [ -d $DEST/dtb ]; then
    sudo cp -r *.dtb $DEST/dtb
  fi
  
  # All boards: Install modules in rootfs
  echo "Unpacking modules to /$(tar tzf modules.tar.gz | head -n 1)"
  sudo tar zxf modules.tar.gz -C /
}


if [[ ( $1 == "-help") || ( $1 == "-h") ]]; then
  usage
fi  

if [[ ( $1 == "-y" ) ]]; then
  inst
  sudo init 6
  exit 0
fi  


read -p "Install kernel driver (y/n)? " answer
case $answer in
  [Yy]* )
    echo -e "\nInstalling..."
    inst
  ;;
  [Nn]* )
    echo -e
  ;;
esac

read -p "Reboot now (y/n)? " answer
case $answer in
  [Yy]* )
    echo -e "Reboot..."
    sudo init 6
  ;;
  [Nn]* )
    echo -e
    exit 0
  ;;
esac

exit 0

