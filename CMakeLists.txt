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

cmake_minimum_required(VERSION 3.19)

project(
    hindsight
    VERSION 0.1.0
    DESCRIPTION "A C++ stack trace library"
    HOMEPAGE_URL "https://github.com/glebov-andrey/hindsight"
    LANGUAGES CXX)

option(HINDSIGHT_INCLUDE_CONANBUILDINFO "Include conanbuildinfo.cmake" OFF)
option(HINDSIGHT_WITH_FMT "Enable native fmt support" OFF)
option(HINDSIGHT_BUILD_TESTS "Build tests" ON)
option(HINDSIGHT_BUILD_EXAMPLES "Build examples" OFF)
option(HINDSIGHT_BUILD_DOCS "Build documentation" OFF)
option(HINDSIGHT_ENABLE_COVERAGE "Enable coverage" OFF)
option(HINDSIGHT_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
option(HINDSIGHT_ENABLE_LLD_THINLTO_CACHE "Enable lld ThinLTO cache" OFF)

list(PREPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake")
if (HINDSIGHT_INCLUDE_CONANBUILDINFO)
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    conan_basic_setup(TARGETS)
endif ()

include(tools/cmake/default_options.cmake)
include(tools/cmake/coverage.cmake)
include(tools/cmake/clang_tidy.cmake)

find_package(Threads REQUIRED)
if (HINDSIGHT_WITH_FMT)
    find_package(fmt REQUIRED)
endif ()
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    find_package(DIA REQUIRED)
else ()
    find_package(libunwind REQUIRED COMPONENTS generic)
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(elfutils REQUIRED COMPONENTS libdw)
    else ()
        find_package(libbacktrace REQUIRED)
    endif ()
endif ()

add_library(hindsight)
target_include_directories(hindsight PUBLIC include)
target_compile_features(hindsight PUBLIC cxx_std_20)
target_link_libraries(hindsight PRIVATE hindsight::default_options hindsight::coverage_options)
target_link_libraries(hindsight PUBLIC Threads::Threads)
fix_static_pdb_name(hindsight)
set_target_clang_tidy(hindsight)
if (BUILD_SHARED_LIBS)
    target_compile_definitions(
        hindsight
        PUBLIC HINDSIGHT_SHARED
        PRIVATE HINDSIGHT_SHARED_BUILD)
endif ()
if (HINDSIGHT_WITH_FMT)
    target_compile_definitions(hindsight PUBLIC HINDSIGHT_WITH_FMT)
    target_link_libraries(hindsight PUBLIC fmt::fmt)
endif ()
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(hindsight PRIVATE DIA::DIA)
else ()
    target_link_libraries(hindsight PRIVATE libunwind::generic)
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(hindsight PRIVATE elfutils::libdw)
    else ()
        target_link_libraries(hindsight PRIVATE libbacktrace::libbacktrace)
    endif ()
endif ()
add_library(hindsight::hindsight ALIAS hindsight)

if (HINDSIGHT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

if (HINDSIGHT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif ()

add_subdirectory(include)
add_subdirectory(src)

if (HINDSIGHT_BUILD_DOCS)
    add_subdirectory(docs)
endif ()
