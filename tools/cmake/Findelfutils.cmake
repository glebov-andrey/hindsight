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
Findelfutils
-------

Finds elfutils.

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``elfutils::elfutils``
  The elfutils interface library (only include directories)
``elfutils::libdw``
  The libdw library

Result Variables
^^^^^^^^^^^^^^^^

This module will define the following variables:

``ELFUTILS_FOUND``
  True if the elfutils library was found.

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``ELFUTILS_INCLUDE_DIRECTORY``
  The elfutils include directory.
``ELFUTILS_LIBDW_INCLUDE_DIRECTORY``
  The libdw include directory (usually the same as ELFUTILS_INCLUDE_DIRECTORY).
``ELFUTILS_LIBDW_LIBRARY``
  The path to the libdw library.

#]================================================================================================================]

if (elfutils_FIND_REQUIRED_libdw)
    set(elfutils_FIND_REQUIRED_elfutils)
endif ()

find_path(ELFUTILS_INCLUDE_DIRECTORY NAMES libelf.h)
if (ELFUTILS_INCLUDE_DIRECTORY)
    set(elfutils_elfutils_FOUND TRUE)
endif ()

find_path(ELFUTILS_LIBDW_INCLUDE_DIRECTORY NAMES dwarf.h elfutils/libdw.h elfutils/libdwfl.h)
find_library(ELFUTILS_LIBDW_LIBRARY dw DOC "The libdw library")
if (ELFUTILS_LIBDW_INCLUDE_DIRECTORY AND ELFUTILS_LIBDW_LIBRARY)
    cmake_path(GET ELFUTILS_LIBDW_LIBRARY EXTENSION LAST_ONLY ELFUTILS_LIBDW_LIBRARY_EXT)
    if (ELFUTILS_LIBDW_LIBRARY_EXT STREQUAL ".so")
        set(elfutils_libdw_FOUND TRUE)
    else ()
        message(
            WARNING "Found a static libdw library: ${ELFUTILS_LIBDW_LIBRARY}\n"
                    "Findelfutils does not support linking libdw statically\n"
                    "Note that doing so would require distributing the produced library under the GPL 3.0 license!")
    endif ()
endif ()

mark_as_advanced(ELFUTILS_INCLUDE_DIRECTORY ELFUTILS_LIBDW_INCLUDE_DIRECTORY ELFUTILS_LIBDW_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(elfutils HANDLE_COMPONENTS)

if (elfutils_elfutils_FOUND AND NOT TARGET elfutils::elfutils)
    add_library(elfutils::elfutils INTERFACE IMPORTED)
    set_target_properties(
        elfutils::elfutils
        PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${ELFUTILS_INCLUDE_DIRECTORY}
                   INTERFACE_SYSTEM_INCLUDE_DIRECTORIES ${ELFUTILS_INCLUDE_DIRECTORY})
    if (NOT elfutils_FIND_QUIETLY)
        message("-- Found elfutils: ${ELFUTILS_INCLUDE_DIRECTORY}")
    endif ()
endif ()

if (elfutils_libdw_FOUND AND NOT TARGET elfutils::libdw)
    add_library(elfutils::libdw SHARED IMPORTED)
    target_link_libraries(elfutils::libdw INTERFACE elfutils::elfutils)
    set_target_properties(
        elfutils::libdw
        PROPERTIES IMPORTED_LOCATION ${ELFUTILS_LIBDW_LIBRARY}
                   INTERFACE_INCLUDE_DIRECTORIES ${ELFUTILS_LIBDW_INCLUDE_DIRECTORY}
                   INTERFACE_SYSTEM_INCLUDE_DIRECTORIES ${ELFUTILS_LIBDW_INCLUDE_DIRECTORY})
    if (NOT elfutils_FIND_QUIETLY)
        message("-- Found libdw: ${ELFUTILS_LIBDW_LIBRARY}")
    endif ()
endif ()
