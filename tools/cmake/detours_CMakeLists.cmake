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

cmake_minimum_required(VERSION 3.28)

project(detours LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME MATCHES "Windows.*")
    message(FATAL_ERROR "detours only supports Windows")
endif()

add_library(detours STATIC)
target_sources(
    detours
    PRIVATE
        src/creatwth.cpp
        src/detours.cpp
        src/detours.h
        src/detver.h
        src/disasm.cpp
        src/disolarm.cpp
        src/disolarm64.cpp
        src/disolia64.cpp
        src/disolx64.cpp
        src/disolx86.cpp
        src/image.cpp
        src/modules.cpp
)
target_compile_definitions(detours PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
target_include_directories(detours INTERFACE src)

if(
    CMAKE_CXX_COMPILER_ID STREQUAL "MSVC"
    OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"
)
    target_compile_options(detours PRIVATE -Zl) # Do not let object file auto-link default libraries
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(detours PRIVATE -W4 -we4777 -we4800)
elseif(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(
        detours
        PRIVATE
            -W4
            -Wpedantic
            -Wno-cast-function-type-mismatch
            -Wno-language-extension-token
            -Wno-unknown-pragmas
            -Wno-sign-compare
            -Wno-reorder-ctor
            -Wno-unused-local-typedef
            -Wno-microsoft-enum-value
    )
endif()
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set_target_properties(detours PROPERTIES COMPILE_PDB_NAME detours)
endif()

add_library(detours::detours ALIAS detours)
