# Copyright 2023 Andrey Glebov
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

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)

add_library(hindsight_default_options INTERFACE)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(hindsight_default_options INTERFACE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
endif ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(hindsight_default_options INTERFACE -W4 -wd4251)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    # -fno-show-column is a workaround for CLion not parsing the "file(line,column)" format
    target_compile_options(hindsight_default_options INTERFACE -W4 -Wpedantic -clang:-fno-show-column)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(hindsight_default_options INTERFACE -Wall -Wextra -Wpedantic)
endif ()

if (HINDSIGHT_ENABLE_LLD_THINLTO_CACHE AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(THINLTO_CACHE_DIR "${CMAKE_BINARY_DIR}/thinlto-cache")
    if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
        target_link_options(hindsight_default_options INTERFACE -lldltocache:${THINLTO_CACHE_DIR})
    else ()
        target_link_options(hindsight_default_options INTERFACE -Wl,--thinlto-cache-dir=${THINLTO_CACHE_DIR})
    endif ()
endif ()

add_library(hindsight::default_options ALIAS hindsight_default_options)

function (fix_compile_pdb_name TARGET_NAME PDB_NAME)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND NOT BUILD_SHARED_LIBS)
        set_target_properties(
            ${TARGET_NAME}
            PROPERTIES COMPILE_PDB_NAME ${PDB_NAME} #
                       COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
    endif ()
endfunction ()
