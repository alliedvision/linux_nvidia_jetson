#!/bin/bash
#==============================================================================
# Copyright (C) 2018 Allied Vision Technologies.  All Rights Reserved.
#
# Redistribution of this file, in original or modified form, without
# prior written consent of Allied Vision Technologies is prohibited.
#
#------------------------------------------------------------------------------
#
# File:         -directory.sh
#
# Description:  -bash script for directory/folder handling
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
# globals
#==============================================================================
PATH_STARTING=""
#==============================================================================
# set starting point path (taking current location)
#==============================================================================
set_path_starting()
{
	PATH_STARTING=$(pwd)
}
#==============================================================================
# get the starting point which was set
#==============================================================================
get_path_starting()
{
	return $PATH_STARTING
}
#==============================================================================
# return to starting point
#==============================================================================
return_path_starting()
{
	cd $PATH_STARTING
}
#==============================================================================
# check if directory exist
#==============================================================================
directory_exist()
{
	if parameter_exist $1
	then
		if [ -d "$1" ]
		then
			return $TRUE
		else
			return $FALSE
		fi
	fi
}
#==============================================================================
# create directory
#==============================================================================
create_directory()
{
	if parameter_exist $1
	then
		if ! directory_exist $1
		then
			mkdir $1
			return $TRUE
		else
			return $FALSE
		fi
	else
		return $FALSE
	fi
}
