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
#elif defined __GNUC__
    #define HINDSIGHT_API_EXPORT [[gnu::visibility("default")]]
    #define HINDSIGHT_API_IMPORT
#else
    #define HINDSIGHT_API_EXPORT
    #define HINDSIGHT_API_IMPORT
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

#endif // HINDSIGHT_INCLUDE_HINDSIGHT_CONFIG_HPP
