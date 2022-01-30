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

find_package(Doxygen REQUIRED)

set(DOXYGEN_HEADERS ${HINDSIGHT_PUBLIC_HEADERS})
list(TRANSFORM DOXYGEN_HEADERS PREPEND "include/")

set(DOXYGEN_PREDEFINED HINDSIGHT_GENERATING_DOCUMENTATION)
set(DOXYGEN_TAGFILES "docs/cppreference-doxygen-web.tag.xml=https://en.cppreference.com/w/")

doxygen_add_docs(docs ${DOXYGEN_HEADERS} WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
