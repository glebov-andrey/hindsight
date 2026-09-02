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

target_sources(
    hindsight_obj
    PRIVATE
        simple.cpp
        stacktrace.cpp
        stacktrace_entry.cpp
        util/finally.cpp
        util/finally.hpp
        util/locked.cpp
        util/locked.hpp
)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources(
        hindsight_obj
        PRIVATE
            capture_windows.cpp
            resolver_windows.cpp
            windows/bstr.cpp
            windows/bstr.hpp
            windows/com.cpp
            windows/com.hpp
            windows/encoding.cpp
            windows/encoding.hpp
            windows/module_map.cpp
            windows/module_map.hpp
    )
else()
    target_sources(
        hindsight_obj
        PRIVATE
            capture_unix.cpp
            itanium_abi/demangle.cpp
            itanium_abi/demangle.hpp
            unix/encoding.cpp
            unix/encoding.hpp
            from_exception_itanium.cpp
    )
    if(
        CMAKE_SYSTEM_NAME STREQUAL "Linux"
        AND HINDSIGHT_RESOLVER_BACKEND STREQUAL "libdw"
    )
        target_sources(hindsight_obj PRIVATE resolver_libdw.cpp)
    else()
        target_sources(hindsight_obj PRIVATE resolver_backtrace.cpp)
    endif()
endif()

if(HINDSIGHT_BUILD_TESTS)
    target_sources(
        hindsight_tests
        PRIVATE
            capture.test.cpp
            capture_signal.test.cpp
            from_exception.test.cpp
            resolver.test.cpp
            stacktrace_entry.test.cpp
            util/locked.test.cpp
    )
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
        target_sources(hindsight_tests PRIVATE unix/encoding.test.cpp)
    endif()
endif()
