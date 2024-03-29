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

cmake_minimum_required(VERSION 3.19)

project(
    hindsight
    VERSION 0.1.0
    DESCRIPTION "A C++ stack trace library"
    HOMEPAGE_URL "https://github.com/glebov-andrey/hindsight"
    LANGUAGES CXX)

option(HINDSIGHT_WITH_FMT "Enable native fmt support" OFF)
option(HINDSIGHT_BUILD_TESTS "Build tests" ON)
option(HINDSIGHT_BUILD_EXAMPLES "Build examples" OFF)
option(HINDSIGHT_BUILD_DOCS "Build documentation" OFF)
option(HINDSIGHT_ENABLE_COVERAGE "Enable coverage" OFF)
option(HINDSIGHT_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
option(HINDSIGHT_ENABLE_LLD_THINLTO_CACHE "Enable lld ThinLTO cache" OFF)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(HINDSIGHT_RESOLVER_BACKEND
        "libdw"
        CACHE STRING "The hindsight resolver backend")
    set(LINUX_RESOLVER_BACKENDS "libdw;libbacktrace")
    if (NOT HINDSIGHT_RESOLVER_BACKEND IN_LIST LINUX_RESOLVER_BACKENDS)
        message(FATAL_ERROR "HINDSIGHT_RESOLVER_BACKEND, if set, must be either \"libdw\" or \"libbacktrace\"")
    endif ()
endif ()

message(STATUS "hindsight: Enable native fmt support: ${HINDSIGHT_WITH_FMT}")
message(STATUS "hindsight: Build tests: ${HINDSIGHT_BUILD_TESTS}")
message(STATUS "hindsight: Build examples: ${HINDSIGHT_BUILD_EXAMPLES}")
message(STATUS "hindsight: Build documentation: ${HINDSIGHT_BUILD_DOCS}")
message(STATUS "hindsight: Enable coverage: ${HINDSIGHT_ENABLE_COVERAGE}")
message(STATUS "hindsight: Enable clang-tidy: ${HINDSIGHT_ENABLE_CLANG_TIDY}")
message(STATUS "hindsight: Enable lld ThinLTO cache: ${HINDSIGHT_ENABLE_LLD_THINLTO_CACHE}")
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "hindsight: Resolver backend: ${HINDSIGHT_RESOLVER_BACKEND}")
endif ()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake")

include(tools/cmake/default_options.cmake)
include(tools/cmake/coverage.cmake)
include(tools/cmake/clang_tidy.cmake)

find_package(Threads REQUIRED)
find_package(tl-function-ref REQUIRED)
if (HINDSIGHT_WITH_FMT)
    find_package(fmt REQUIRED)
endif ()
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    find_package(DIA REQUIRED)
else ()
    find_package(libunwind REQUIRED COMPONENTS generic)
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND HINDSIGHT_RESOLVER_BACKEND STREQUAL "libdw")
        find_package(elfutils REQUIRED COMPONENTS libdw)
    else ()
        find_package(libbacktrace REQUIRED)
    endif ()
    find_package(Iconv REQUIRED)
endif ()

add_library(hindsight_obj OBJECT)
target_include_directories(hindsight_obj PUBLIC include)
target_compile_features(hindsight_obj PUBLIC cxx_std_20)
target_link_libraries(hindsight_obj PRIVATE hindsight::default_options hindsight::coverage_options)
target_link_libraries(hindsight_obj PUBLIC Threads::Threads tl::function-ref)
fix_compile_pdb_name(hindsight_obj hindsight)
set_target_clang_tidy(hindsight_obj)
if (BUILD_SHARED_LIBS)
    target_compile_definitions(
        hindsight_obj
        PUBLIC HINDSIGHT_SHARED
        PRIVATE HINDSIGHT_SHARED_BUILD)
endif ()
if (HINDSIGHT_WITH_FMT)
    target_compile_definitions(hindsight_obj PUBLIC HINDSIGHT_WITH_FMT)
    target_link_libraries(hindsight_obj PUBLIC fmt::fmt)
endif ()
if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND HINDSIGHT_RESOLVER_BACKEND STREQUAL "libbacktrace")
    target_compile_definitions(
        hindsight_obj PUBLIC "HINDSIGHT_RESOLVER_BACKEND=HINDSIGHT_RESOLVER_BACKEND_LIBBACKTRACE")
endif ()
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(hindsight_obj PRIVATE DIA::DIA)
else ()
    target_link_libraries(hindsight_obj PRIVATE libunwind::generic)
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND HINDSIGHT_RESOLVER_BACKEND STREQUAL "libdw")
        target_link_libraries(hindsight_obj PRIVATE elfutils::libdw)
    else ()
        target_link_libraries(hindsight_obj PRIVATE libbacktrace::libbacktrace)
    endif ()
    target_link_libraries(hindsight_obj PRIVATE Iconv::Iconv)
endif ()

add_library(hindsight)
target_link_libraries(hindsight PUBLIC hindsight_obj)
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
