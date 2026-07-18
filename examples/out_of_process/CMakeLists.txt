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

add_executable(out_of_process_watchdog)
target_sources(out_of_process_watchdog PRIVATE out_of_process_watchdog.cpp)
target_compile_features(out_of_process_watchdog PRIVATE cxx_std_23)
target_link_libraries(
    out_of_process_watchdog
    PRIVATE hindsight::default_options hindsight::hindsight
)

add_executable(out_of_process_host)
target_sources(out_of_process_host PRIVATE out_of_process_host.cpp)
target_compile_features(out_of_process_host PRIVATE cxx_std_23)
target_link_libraries(
    out_of_process_host
    PRIVATE hindsight::default_options hindsight::hindsight
)
add_dependencies(out_of_process_host out_of_process_watchdog)
