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

if (HINDSIGHT_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_PROGRAM clang-tidy REQUIRED DOC "The clang-tidy program")
    message(STATUS "hindsight: Using clang-tidy: ${CLANG_TIDY_PROGRAM}")
    # CMake by default for clang-cl sets CMAKE_INCLUDE_SYSTEM_FLAG_CXX to "-imsvc " (note the trailing space).
    # clang-tidy (as of 12.0.0) does not parse this correctly (as a single flag) which results in missing include
    # directories. As a workaround we remove the trailing space which, oddly enough, works correctly.
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
        set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-imsvc")
    endif ()
endif ()

function (set_target_clang_tidy TARGET_NAME)
    if (HINDSIGHT_ENABLE_CLANG_TIDY)
        set_target_properties(${TARGET_NAME} PROPERTIES CXX_CLANG_TIDY ${CLANG_TIDY_PROGRAM})
    endif ()
endfunction ()
