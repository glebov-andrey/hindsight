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

find_package(Catch2 REQUIRED)

add_executable(hindsight_tests)
target_link_libraries(hindsight_tests PRIVATE hindsight::default_options hindsight::coverage_options hindsight_obj)
target_link_libraries(hindsight_tests PRIVATE Catch2::Catch2)
if (BUILD_SHARED_LIBS)
    target_compile_definitions(hindsight_tests PRIVATE HINDSIGHT_SHARED_BUILD)
endif ()
target_sources(hindsight_tests PRIVATE main.cpp)

include(Catch)
catch_discover_tests(hindsight_tests)
