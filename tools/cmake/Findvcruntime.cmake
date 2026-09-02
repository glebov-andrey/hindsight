# Copyright 2026 Andrey Glebov
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

execute_process(
    COMMAND
        "$ENV{ProgramFiles\(x86\)}/Microsoft Visual Studio/Installer/vswhere.exe"
        -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64
        -property installationPath -format value -utf8
    OUTPUT_VARIABLE VSWHERE_OUTPUT
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(VC_TOOLS_VERSION_PATH
    "${VSWHERE_OUTPUT}/VC/Auxiliary/Build/Microsoft.VCToolsVersion.default.txt"
)
if(EXISTS "${VC_TOOLS_VERSION_PATH}")
    file(READ "${VC_TOOLS_VERSION_PATH}" VC_TOOLS_VERSION)
    string(STRIP "${VC_TOOLS_VERSION}" VC_TOOLS_VERSION)
    set(VCRUNTIME_SRC_DIR
        "${VSWHERE_OUTPUT}/VC/Tools/MSVC/${VC_TOOLS_VERSION}/crt/src/vcruntime"
    )
endif()

mark_as_advanced(VCRUNTIME_SRC_DIR)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(vcruntime DEFAULT_MSG VCRUNTIME_SRC_DIR)

if(vcruntime_FOUND AND NOT TARGET vcruntime::src)
    add_library(vcruntime::src INTERFACE IMPORTED)
    set_target_properties(
        vcruntime::src
        PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES ${VCRUNTIME_SRC_DIR}
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES ${VCRUNTIME_SRC_DIR}
    )
    if(NOT VCRUNTIME_FIND_QUIETLY)
        message(STATUS "Found vcruntime src: ${VCRUNTIME_SRC_DIR}")
    endif()
endif()
