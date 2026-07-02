# Copyright 2025 Andrey Glebov
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

include(FindPackageHandleStandardArgs)

find_package(PkgConfig)
if (PkgConfig_FOUND)
    pkg_check_modules(libunwind-generic IMPORTED_TARGET GLOBAL libunwind-generic)
    if (libunwind-generic_FOUND)
        find_package_handle_standard_args(libunwind DEFAULT_MSG)
        if (NOT TARGET libunwind::generic)
            add_library(libunwind::generic ALIAS PkgConfig::libunwind-generic)
        endif ()
        return()
    endif ()
endif ()

find_path(
    LIBUNWIND_INCLUDE_DIRECTORY
    NAMES libunwind.h
    PATH_SUFFIXES libunwind)
find_library(LIBUNWIND_GENERIC_LIBRARY unwind DOC "The libunwind library")

mark_as_advanced(LIBUNWIND_INCLUDE_DIRECTORY LIBUNWIND_GENERIC_LIBRARY)

find_package_handle_standard_args(libunwind DEFAULT_MSG LIBUNWIND_INCLUDE_DIRECTORY LIBUNWIND_GENERIC_LIBRARY)

if (libunwind_FOUND AND NOT TARGET libunwind::generic)
    add_library(libunwind::generic UNKNOWN IMPORTED)
    set_target_properties(
        libunwind::generic
        PROPERTIES IMPORTED_LOCATION ${LIBUNWIND_GENERIC_LIBRARY}
                   INTERFACE_INCLUDE_DIRECTORIES ${LIBUNWIND_INCLUDE_DIRECTORY}
                   INTERFACE_SYSTEM_INCLUDE_DIRECTORIES ${LIBUNWIND_INCLUDE_DIRECTORY})
    if (NOT libunwind_FIND_QUIETLY)
        message(STATUS "Found libunwind: ${LIBUNWIND_GENERIC_LIBRARY}")
    endif ()
endif ()
