# Copyright 2021 Andrey Glebov
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

#[================================================================================================================[.rst:
FindDIA
-------

Finds the Microsoft DIA SDK.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``DIA::DIA``
  The DIA library

Result Variables
^^^^^^^^^^^^^^^^

This module will defined the following variables:

``DIA_FOUND``
  True if the DIA library was found.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``DIA_INCLUDE_DIR``
  The DIA include directory.
``DIA_GUIDS_LIBRARY``
  The path to the DIA GUIDs (static) library.

#]================================================================================================================]

execute_process(
    COMMAND
        "$ENV{ProgramFiles\(x86\)}/Microsoft Visual Studio/Installer/vswhere.exe" #
        -latest -property installationPath -format value -utf8
    OUTPUT_VARIABLE VSWHERE_OUTPUT
    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

set(DIA_ROOT "${VSWHERE_OUTPUT}/DIA SDK")

string(TOLOWER ${CMAKE_SYSTEM_PROCESSOR} CMAKE_SYSTEM_PROCESSOR_LOWER)
if (CMAKE_SYSTEM_PROCESSOR_LOWER STREQUAL "amd64")
    set(DIA_ARCH_DIRECTORY "/amd64")
elseif (CMAKE_SYSTEM_PROCESSOR_LOWER STREQUAL "arm64")
    set(DIA_ARCH_DIRECTORY "/arm64")
elseif (CMAKE_SYSTEM_PROCESSOR_LOWER STREQUAL "arm")
    set(DIA_ARCH_DIRECTORY "/arm")
endif ()

find_path(
    DIA_INCLUDE_DIR dia2.h
    HINTS "${DIA_ROOT}/include"
    DOC "DIA include directory")

find_library(
    DIA_GUIDS_LIBRARY diaguids.lib
    HINTS "${DIA_ROOT}/lib${DIA_ARCH_DIRECTORY}"
    DOC "DIA GUIDs library")

mark_as_advanced(DIA_INCLUDE_DIR DIA_GUIDS_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(DIA DEFAULT_MSG DIA_INCLUDE_DIR DIA_GUIDS_LIBRARY)

if (DIA_FOUND AND NOT TARGET DIA::DIA)
    add_library(DIA::DIA STATIC IMPORTED)
    set_target_properties(
        DIA::DIA
        PROPERTIES IMPORTED_LOCATION ${DIA_GUIDS_LIBRARY} #
                   INTERFACE_INCLUDE_DIRECTORIES ${DIA_INCLUDE_DIR})
endif ()
