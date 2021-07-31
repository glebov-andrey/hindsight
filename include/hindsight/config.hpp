/*
 * Copyright 2021 Andrey Glebov
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef HINDSIGHT_INCLUDE_HINDSIGHT_CONFIG_HPP
#define HINDSIGHT_INCLUDE_HINDSIGHT_CONFIG_HPP

#include <version>

#ifdef _WIN32
    #define HINDSIGHT_OS_WINDOWS
#elif defined __unix__ || defined __unix || (defined __APPLE__ && defined __MACH__)
    #define HINDSIGHT_OS_UNIX
    #ifdef __linux__
        #define HINDSIGHT_OS_LINUX
    #endif
#endif

#ifdef HINDSIGHT_OS_WINDOWS
    #define HINDSIGHT_API_EXPORT __declspec(dllexport)
    #define HINDSIGHT_API_IMPORT __declspec(dllimport)
    #define HINDSIGHT_API_HIDDEN
#elif defined __GNUC__
    #define HINDSIGHT_API_EXPORT [[gnu::visibility("default")]]
    #define HINDSIGHT_API_IMPORT
    #define HINDSIGHT_API_HIDDEN [[gnu::visibility("hidden")]]
#else
    #define HINDSIGHT_API_EXPORT
    #define HINDSIGHT_API_IMPORT
    #define HINDSIGHT_API_HIDDEN
#endif

#ifdef HINDSIGHT_SHARED
    #ifdef HINDSIGHT_SHARED_BUILD
        #define HINDSIGHT_API HINDSIGHT_API_EXPORT
    #else
        #define HINDSIGHT_API HINDSIGHT_API_IMPORT
    #endif
#else
    #define HINDSIGHT_API
#endif

#ifdef __clang__
    #define HINDSIGHT_PRAGMA_CLANG(str) _Pragma(str)
#else
    #define HINDSIGHT_PRAGMA_CLANG(str)
#endif

#if defined __GNUC__ && !defined __clang__
    #define HINDSIGHT_PRAGMA_GCC(str) _Pragma(str)
#else
    #define HINDSIGHT_PRAGMA_GCC(str)
#endif

#if defined _MSC_VER && !defined __clang__
    #define HINDSIGHT_PRAGMA_MSVC(str) _Pragma(str)
#else
    #define HINDSIGHT_PRAGMA_MSVC(str)
#endif


// <ranges> is broken with Clang and libstdc++ (as of Clang 12 and GCC 11).
// 1. Clang does not evaluate concepts lazily: https://bugs.llvm.org/show_bug.cgi?id=50864,
//    https://bugs.llvm.org/show_bug.cgi?id=47509, https://bugs.llvm.org/show_bug.cgi?id=46746,
//    https://gcc.gnu.org/bugzilla/show_bug.cgi?id=97120.
// 2. Since GCC 11 <ranges> omits typename, which Clang does not yet support.
#if !(defined __clang__ && defined __GLIBCXX__)
    #define HINDSIGHT_HAS_STD_RANGES
#endif

// The MSVC STL <format> implementation is disabled before 202105L due to https://github.com/microsoft/STL/issues/1961.
#if __cpp_lib_format >= 201907L && (!defined _MSVC_STL_UPDATE || _MSVC_STL_UPDATE >= 202105L)
    #define HINDSIGHT_HAS_STD_FORMAT
#endif

#endif // HINDSIGHT_INCLUDE_HINDSIGHT_CONFIG_HPP
