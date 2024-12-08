/*
 * Copyright 2024 Andrey Glebov
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

namespace hindsight::detail {

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
    #define HINDSIGHT_API_IMPORT [[gnu::visibility("default")]]
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


#ifdef HINDSIGHT_NOINLINE
    #error HINDSIGHT_NOINLINE must not be defined
#endif

#if __has_cpp_attribute(gnu::noinline)
    #define HINDSIGHT_NOINLINE [[gnu::noinline]]
#elif defined _MSC_VER
    #define HINDSIGHT_NOINLINE __declspec(noinline)
#endif

#ifdef HINDSIGHT_NOINLINE
    #define HINDSIGHT_HAS_NOINLINE
#else
    #define HINDSIGHT_NOINLINE

[[deprecated("Could not detect a \"noinline\" attribute for the current compiler. "
             "Stack traces may contain extra physical and logical entries.")]] //
inline constexpr auto _noinline_not_detected_warn = 0;

[[maybe_unused]] inline constexpr auto _noinline_not_detected = _noinline_not_detected_warn;
#endif

#ifdef __has_builtin
    #if __has_builtin(__builtin_unreachable)
        #define HINDSIGHT_UNREACHABLE __builtin_unreachable()
    #endif
#elif defined _MSC_VER
    #define HINDSIGHT_UNREACHABLE __assume(false)
#else
    #define HINDSIGHT_UNREACHABLE
#endif


#if defined _MSC_VER && !defined __clang__
    #define HINDSIGHT_PRAGMA_MSVC(str) _Pragma(str)
#else
    #define HINDSIGHT_PRAGMA_MSVC(str)
#endif

#ifdef __clang__
    #define HINDSIGHT_PRAGMA_CLANG(str) _Pragma(str)
#else
    #define HINDSIGHT_PRAGMA_CLANG(str)
#endif


#define HINDSIGHT_RESOLVER_BACKEND_DIA 1
#define HINDSIGHT_RESOLVER_BACKEND_LIBDW 2
#define HINDSIGHT_RESOLVER_BACKEND_LIBBACKTRACE 3

#ifdef HINDSIGHT_OS_WINDOWS
    #if defined HINDSIGHT_RESOLVER_BACKEND
        #if HINDSIGHT_RESOLVER_BACKEND != HINDSIGHT_RESOLVER_BACKEND_DIA
            #error HINDSIGHT_RESOLVER_BACKEND must be DIA on Windows
        #endif
    #else
        #define HINDSIGHT_RESOLVER_BACKEND HINDSIGHT_RESOLVER_BACKEND_DIA
    #endif
#elif defined HINDSIGHT_OS_LINUX
    #if defined HINDSIGHT_RESOLVER_BACKEND
        #if HINDSIGHT_RESOLVER_BACKEND != HINDSIGHT_RESOLVER_BACKEND_LIBDW &&                                          \
                HINDSIGHT_RESOLVER_BACKEND != HINDSIGHT_RESOLVER_BACKEND_LIBBACKTRACE
            #error HINDSIGHT_RESOLVER_BACKEND must be LIBDW or LIBBACKTRACE on Linux
        #endif
    #else
        #define HINDSIGHT_RESOLVER_BACKEND HINDSIGHT_RESOLVER_BACKEND_LIBDW
    #endif
#else
    #if defined HINDSIGHT_RESOLVER_BACKEND
        #if HINDSIGHT_RESOLVER_BACKEND != HINDSIGHT_RESOLVER_BACKEND_LIBBACKTRACE
            #error HINDSIGHT_RESOLVER_BACKEND must be LIBBACKTRACE on this OS
        #endif
    #else
        #define HINDSIGHT_RESOLVER_BACKEND HINDSIGHT_RESOLVER_BACKEND_LIBBACKTRACE
    #endif
#endif

} // namespace hindsight::detail

#endif // HINDSIGHT_INCLUDE_HINDSIGHT_CONFIG_HPP
