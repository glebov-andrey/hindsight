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

#include <hindsight/stacktrace.hpp>

namespace hindsight {

auto capture_stack_trace(const std::size_t entries_to_skip) -> std::vector<stacktrace_entry> {
    constexpr auto initial_stack_trace_capacity = std::size_t{16};
    auto entries = std::vector<stacktrace_entry>();
    entries.reserve(initial_stack_trace_capacity);
    capture_stack_trace(std::back_inserter(entries), std::unreachable_sentinel, entries_to_skip);
    return entries;
}

} // namespace hindsight
