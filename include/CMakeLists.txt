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

set(HINDSIGHT_PUBLIC_HEADERS
    # cmake-format: sort
    hindsight/detail/bstr.hpp
    hindsight/detail/config.hpp
    hindsight/resolver.hpp
    hindsight/simple.hpp
    hindsight/stacktrace.hpp
    hindsight/stacktrace_entry.hpp
    PARENT_SCOPE)

target_sources(hindsight_obj PRIVATE ${HINDSIGHT_PUBLIC_HEADERS})
