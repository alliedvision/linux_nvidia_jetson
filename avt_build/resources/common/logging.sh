#!/bin/bash
#==============================================================================
# Copyright (C) 2018 Allied Vision Technologies.  All Rights Reserved.
#
# Redistribution of this file, in original or modified form, without
# prior written consent of Allied Vision Technologies is prohibited.
#
#------------------------------------------------------------------------------
#
# File:         -logging.sh
#
# Description:  -bash script for logging functions in other scripts
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
# font colors
FONT_NONE=0
FONT_STD=1
FONT_GREY=30
FONT_RED=31
FONT_GREEN=32
FONT_YELLOW=33
FONT_BLUE=34
FONT_VIOLET=35
FONT_CYAN=36
FONT_WHITE=37
declare -a FNT=($FONT_NONE $FONT_STD $FONT_GREY $FONT_RED $FONT_GREEN $FONT_YELLOW $FONT_BLUE $FONT_VIOLET $FONT_CYAN $FONT_WHITE)

# background colors colors
BG_DARK=0
BG_LIGHT=1
BG_GREY=40
BG_RED=41
BG_GREEN=42
BG_YELLOW=43
BG_BLUE=44
BG_VIOLET=45
BG_CYAN=46
BG_WHITE=47
declare -a BG=($BG_DARK $BG_LIGHT $BG_GREY $BG_RED $BG_GREEN $BG_YELLOW $BG_BLUE $BG_VIOLET $BG_CYAN $BG_WHITE)

# logging constant messages
LOG_ERROR_PARAMETER_INVALID="Invalid parameter"
LOG_ERROR_PARAMETER_MISSING="Missing parameter"

# logging string formatting
STR_FMT=''
PREFIX='>> '
APPX='...'
NC='\033[0m'
TOKEN=''
TOKEN_INFO='[i]...'
TOKEN_DEBUG='[d]...'
TOKEN_SUCCESS=''
TOKEN_FAILED=''
TOKEN_ERROR='[F]...'
MSG=""
#==============================================================================
# logging function
#==============================================================================
log()
{
	if parameter_exist $1
	then
		if [[ ($1 == "info") ]]
		then
			STR_FMT="\033[${BG[0]};${FNT[5]}m"
			TOKEN=$TOKEN_INFO

		elif [[ ($1 == "success") ]]
		then
			STR_FMT="\033[${BG[0]};${FNT[4]}m"
			MSG="Success"
			TOKEN=$TOKEN_SUCCESS
			
		elif [[ ($1 == "failed") ]]
		then
			STR_FMT="\033[${BG[0]};${FNT[3]}m"
			MSG="Failed"
			TOKEN=$TOKEN_FAILED

		elif [[ ($1 == "error") ]]
		then
			STR_FMT="\033[${BG[1]};${FNT[3]}m"
			TOKEN=$TOKEN_ERROR

		elif [[ ($1 == "debug") ]]
		then
			STR_FMT="\033[${BG[0]}${FNT[0]}m"
			TOKEN=$TOKEN_DEBUG

		fi

		if parameter_exist $2
		then
			MSG=$2
		fi

		echo -e ${STR_FMT}"${PREFIX}${TOKEN}${MSG}${APPX}${NC}"
	fi
}
#==============================================================================
# log raw
#==============================================================================
log_raw()
{
	if parameter_exist $1
	then
		STR_FMT="\033[${BG[0]};${FNT[0]}m"
		echo -e ${STR_FMT}"${1}${NC}"
	fi
}
#==============================================================================
# show logo
#==============================================================================
show_logo()
{
	if parameter_exist $1
	then
		if check_parameter $1 "timmay"
		then
			cat "${PATH_SCRIPTS_COMMON}/timmay.txt"
		fi
	fi
}