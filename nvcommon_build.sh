# SPDX-FileCopyrightText: Copyright (c) 2019-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.

# shellcheck disable=SC2148

# Function to check if variable is defined.
function check_vars {
	# shellcheck disable=SC2124
	variables=${@}
	for variable in ${variables} ; do
		if [ -z "${!variable}" ]; then
			echo "Error: Env variable ${variable} is not set!!"
			exit 1
		fi
	done
}

# Function to check build environment
function check_env_common {
	if [ "${IS_CROSS_COMPILATION}" -eq 1 ]; then
		check_vars "CROSS_COMPILE_AARCH64_PATH"
		CROSS_COMPILE_AARCH64="${CROSS_COMPILE_AARCH64:-${CROSS_COMPILE_AARCH64_PATH}/bin/aarch64-buildroot-linux-gnu-}"
		if [ ! -f "${CROSS_COMPILE_AARCH64}gcc" ]; then
			echo "Error: Path ${CROSS_COMPILE_AARCH64}gcc does not exist."
			exit 1
		fi
	fi
	# shellcheck disable=SC2046,SC2235
	if [ "${MAKE_BIN}" != "make" ] && \
		( [ ! -f "${MAKE_BIN}" ] || \
		[ ! -f $(basename "${MAKE_BIN}") ] ); then
		echo "Error: ${MAKE_BIN} does not exist !!"
		# shellcheck disable=SC2140
		echo "Set MAKE_BIN env variable to "make" binary"
		exit 1
	fi
}


# shellcheck disable=SC2034
NPROC=$(nproc)
MAKE_BIN="${MAKE_BIN:-make}"
BUILD_DIR="${BUILD_DIR:-$(pwd)/build_nv_sources}"

MACHINE=$(uname -m)

IS_CROSS_COMPILATION=0
if [[ "${MACHINE}" =~ "x86" ]]; then
	IS_CROSS_COMPILATION=1
fi
check_env_common
