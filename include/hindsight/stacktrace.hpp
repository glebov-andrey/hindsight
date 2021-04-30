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

#ifndef HINDSIGHT_INCLUDE_HINDSIGHT_STACKTRACE_HPP
#define HINDSIGHT_INCLUDE_HINDSIGHT_STACKTRACE_HPP

#include <hindsight/config.hpp>

#include <cstddef>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include <hindsight/capture.hpp>

namespace hindsight {

template<typename Allocator = std::allocator<stacktrace_entry>>
[[nodiscard]] auto capture_stacktrace(const std::size_t entries_to_skip = 0, const Allocator &allocator = Allocator())
        -> std::vector<stacktrace_entry,
                       typename std::allocator_traits<Allocator>::template rebind_alloc<stacktrace_entry>> {
    static constexpr auto initial_stacktrace_capacity = std::size_t{16};
    using rebound_allocator = typename std::allocator_traits<Allocator>::template rebind_alloc<stacktrace_entry>;

    auto entries = std::vector<stacktrace_entry, rebound_allocator>(allocator);
    entries.reserve(initial_stacktrace_capacity);
    capture_stacktrace(std::back_inserter(entries), std::unreachable_sentinel, entries_to_skip);
    return entries;
}

} // namespace hindsight

#endif // HINDSIGHT_INCLUDE_HINDSIGHT_STACKTRACE_HPP
