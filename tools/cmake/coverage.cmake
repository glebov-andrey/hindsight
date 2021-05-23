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

add_library(hindsight_coverage_options INTERFACE)
if (HINDSIGHT_ENABLE_COVERAGE)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(hindsight_coverage_options INTERFACE -fprofile-instr-generate -fcoverage-mapping)
        target_link_options(hindsight_coverage_options INTERFACE -fprofile-instr-generate -fcoverage-mapping)
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(hindsight_coverage_options INTERFACE --coverage)
        target_link_options(hindsight_coverage_options INTERFACE --coverage)
    endif ()
endif ()
add_library(hindsight::coverage_options ALIAS hindsight_coverage_options)
