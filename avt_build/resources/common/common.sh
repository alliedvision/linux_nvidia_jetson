#!/bin/bash
#==============================================================================
# Copyright (C) 2018 Allied Vision Technologies.  All Rights Reserved.
#
# Redistribution of this file, in original or modified form, without
# prior written consent of Allied Vision Technologies is prohibited.
#
#------------------------------------------------------------------------------
#
# File:         -common.sh
#
# Description:  -common bash script for inclusion in other scripts
#				-provides general convenience functions and globals,
#				 required by other bash scripts
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
TRUE=0
FALSE=-1
#==============================================================================
# check if parameter was given
#==============================================================================
parameter_exist()
{
	if [ -z "$1" ]
	then
		return $FALSE
	else
		return $TRUE 
	fi
}
#==============================================================================
# check if given two parameters are equal
#==============================================================================
check_parameter()
{
	if [[ ($1 == $2) ]]
	then
		return $TRUE
	else
		return $FALSE
	fi
}
