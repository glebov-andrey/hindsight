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

add_executable(hindsight_print_stacktrace_here)
target_link_libraries(
    hindsight_print_stacktrace_here
    PRIVATE hindsight::default_options hindsight::hindsight
)
target_compile_features(hindsight_print_stacktrace_here PRIVATE cxx_std_23)
set_target_clang_tidy(hindsight_print_stacktrace_here)
target_sources(
    hindsight_print_stacktrace_here
    PRIVATE print_stacktrace_here.cpp
)

if(HINDSIGHT_STACKTRACE_FROM_EXCEPTION)
    add_executable(hindsight_from_exception_example)
    target_link_libraries(
        hindsight_from_exception_example
        PRIVATE hindsight::default_options hindsight::hindsight
    )
    target_compile_features(hindsight_from_exception_example PRIVATE cxx_std_23)
    set_target_clang_tidy(hindsight_from_exception_example)
    target_sources(
        hindsight_from_exception_example
        PRIVATE from_exception_example.cpp
    )
endif()

if(
    CMAKE_SYSTEM_NAME STREQUAL "Windows"
    OR HINDSIGHT_RESOLVER_BACKEND STREQUAL "libdw"
)
    add_subdirectory(out_of_process)
endif()
