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

find_package(fmt 8.0.1 REQUIRED)

add_executable(hindsight_print_stacktrace_here)
target_link_libraries(hindsight_print_stacktrace_here PRIVATE hindsight::default_options hindsight::hindsight)
target_link_libraries(hindsight_print_stacktrace_here PRIVATE fmt::fmt)
set_target_clang_tidy(hindsight_print_stacktrace_here)

target_sources(hindsight_print_stacktrace_here PRIVATE print_stacktrace_here.cpp)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows" OR HINDSIGHT_RESOLVER_BACKEND STREQUAL "libdw")
    add_subdirectory(out_of_process)
endif ()
