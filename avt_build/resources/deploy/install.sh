#!/bin/bash
#==============================================================================
# Copyright (C) 2021 Allied Vision Technologies.  All Rights Reserved.
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
REQ_KERNEL="4.9.201"
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

  if [[ "$MODEL" == *"Nano"* ]]; then
    sudo l4t_payload_updater_t210 bl_update_payload >nv_update_engine.log

  else
    sudo nv_update_engine -e 2>&1 >nv_update_engine.log
    sudo nv_update_engine -i --payload bl_update_payload --no-reboot 2>&1 >>nv_update_engine.log
  fi

  # All boards: copy Image + DTBs in rootfs
  sudo cp -r Image* $DEST
  
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

