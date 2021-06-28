#!/bin/bash
#==============================================================================
# Copyright (C) 2018 Allied Vision Technologies.  All Rights Reserved.
#
# Redistribution of this file, in original or modified form, without
# prior written consent of Allied Vision Technologies is prohibited.
#
#------------------------------------------------------------------------------
#
# File:         -files.sh
#
# Description:  -bash script for file handling
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
#
#==============================================================================
# download a file 
#==============================================================================
download_file()
{
    if parameter_exist $1 && parameter_exist $2
    then
        if wget -P $2 $1
        then
            return $TRUE
        else
            return $FALSE
        fi
    fi
}
#==============================================================================
# check if file exist
#==============================================================================
file_exist()
{
    if parameter_exist $1
    then
		if [ -f "$1" ]
		then
			return $TRUE
		else
			return $FALSE
		fi
    fi
}
#==============================================================================
# remove file
#==============================================================================
remove_file()
{
    if parameter_exist $1
    then
        if file_exist $1
        then
            if rm $1
            then
                return $TRUE
            else
                return $FALSE
            fi
        else
            return $FALSE
        fi
    else
        return $FALSE
    fi
}
#==============================================================================
# extract file
#==============================================================================
extract_file_tar()
{
    if parameter_exist $1 && parameter_exist $2
    then
        if tar vxf $1 -C $2
        then
            return $TRUE
        else
            return $FALSE
        fi
    fi
}
