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
if (HINDSIGHT_INCLUDE_CONANBUILDINFO)
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    conan_basic_setup(TARGETS)
endif ()

list(PREPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake")

option(HINDSIGHT_BUILD_TESTING "Build tests" ON)
if (HINDSIGHT_BUILD_TESTING)
    enable_testing()
endif ()

find_package(Threads REQUIRED)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    find_package(DIA REQUIRED)
else ()
    find_package(libbacktrace REQUIRED)
endif ()

add_library(hindsight)
target_include_directories(hindsight PUBLIC include)
target_compile_features(hindsight PUBLIC cxx_std_20)
target_link_libraries(hindsight PUBLIC Threads::Threads)
if (BUILD_SHARED_LIBS)
    target_compile_definitions(
        hindsight
        PUBLIC HINDSIGHT_SHARED
        PRIVATE HINDSIGHT_SHARED_BUILD)
endif ()
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(hindsight PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(hindsight PRIVATE DIA::DIA)
    if (NOT BUILD_SHARED_LIBS)
        set_target_properties(
            hindsight
            PROPERTIES COMPILE_PDB_NAME hindsight #
                       COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
    endif ()
else ()
    target_link_libraries(hindsight PRIVATE unwind unwind-${CMAKE_SYSTEM_PROCESSOR} libbacktrace::libbacktrace)
endif ()

target_sources(
    hindsight
    PRIVATE # cmake-format: sort
            include/hindsight/config.hpp
            include/hindsight/resolver.hpp
            include/hindsight/stacktrace.hpp
            src/resolver_backtrace.cpp
            src/resolver_windows.cpp
            src/stacktrace.cpp
            src/stacktrace_unix.cpp
            src/stacktrace_windows.cpp
            src/util/locked.cpp
            src/util/locked.hpp)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources(
        hindsight
        PRIVATE # cmake-format: sort
                src/windows/com.cpp
                src/windows/com.hpp
                src/windows/encoding.cpp
                src/windows/encoding.hpp
                src/windows/module_info.cpp
                src/windows/module_info.hpp)
endif ()
