#!/bin/bash
#==============================================================================
# Copyright (C) 2018 Allied Vision Technologies.  All Rights Reserved.
#
# Redistribution of this file, in original or modified form, without
# prior written consent of Allied Vision Technologies is prohibited.
#
#------------------------------------------------------------------------------
#
# File:         -platform.sh
#
# Description:  -bash script providing functions handling the host platform,
#                using the bash script of this repository
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
#
#==============================================================================
# include
#==============================================================================
#
#==============================================================================
# globals
#==============================================================================
#
#==============================================================================
# determine host platform
#==============================================================================
determine_host()
{
	HOST=`uname -m`
	return $HOST
}
#==============================================================================
# check if host is x86 32bit
#==============================================================================
is_host_x86_32()
{
	HOST=`uname -m`
	if [[ ${HOST} == 'i686' || ${HOST} == 'i386' ]]
	then
		return $TRUE
	else
		return $FALSE
	fi
}
#==============================================================================
# check if host is x86 64bit
#==============================================================================
is_host_x86_64()
{
	HOST=`uname -m`
	if [ ${HOST} == 'x86_64' ]
	then
		return $TRUE
	else
		return $FALSE
	fi
}
#==============================================================================
# check if host is arm v7 32bit
#==============================================================================
is_host_arm_v7_32()
{
	HOST=`uname -m`
	if [ ${HOST} == '???' ]
	then
		return $TRUE
	else
		return $FALSE
	fi
}
#==============================================================================
# check if host is arm v8 64bit
#==============================================================================
is_host_arm_v8_64()
{
	HOST=`uname -m`
	if [ ${HOST} == '???' ]
	then
		return $TRUE
	else
		return $FALSE
	fi
}