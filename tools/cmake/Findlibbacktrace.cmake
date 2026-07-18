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

include(FindPackageHandleStandardArgs)

try_compile(
    HAS_BUILTIN_LIBBACKTRACE
    SOURCE_FROM_CONTENT
        find_libbacktrace.cpp
        "#include <backtrace.h>
        int main() {
            [[maybe_unused]] auto *bt_state = backtrace_create_state(nullptr, true, nullptr, nullptr);
        }"
    LINK_LIBRARIES backtrace
)
if(HAS_BUILTIN_LIBBACKTRACE)
    set(libbacktrace_FOUND TRUE)
    find_package_handle_standard_args(libbacktrace DEFAULT_MSG)
    if(NOT TARGET libbacktrace::libbacktrace)
        add_library(libbacktrace::libbacktrace INTERFACE IMPORTED)
        set_target_properties(
            libbacktrace::libbacktrace
            PROPERTIES INTERFACE_LINK_LIBRARIES backtrace
        )
        if(NOT libbacktrace_FIND_QUIETLY)
            message(STATUS "Found libbacktrace: -lbacktrace")
        endif()
    endif()
    return()
endif()

find_path(LIBBACKTRACE_INCLUDE_DIR NAMES backtrace.h)
find_library(LIBBACKTRACE_LIBRARY NAMES backtrace)

mark_as_advanced(LIBBACKTRACE_INCLUDE_DIR LIBBACKTRACE_LIBRARY)

find_package_handle_standard_args(
    libbacktrace
    DEFAULT_MSG
    LIBBACKTRACE_INCLUDE_DIR
    LIBBACKTRACE_LIBRARY
)

if(libbacktrace_FOUND AND NOT TARGET libbacktrace::libbacktrace)
    add_library(libbacktrace::libbacktrace UNKNOWN IMPORTED)
    set_target_properties(
        libbacktrace::libbacktrace
        PROPERTIES
            IMPORTED_LOCATION "${LIBBACKTRACE_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${LIBBACKTRACE_INCLUDE_DIR}"
    )
    if(NOT libbacktrace_FIND_QUIETLY)
        message(STATUS "Found libbacktrace: ${LIBBACKTRACE_LIBRARY}")
    endif()
endif()
