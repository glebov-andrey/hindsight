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

#include <cstddef>
#include <functional>
#include <iterator>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

#include <fmt/format.h>
#ifndef HINDSIGHT_WITH_FMT
    #include <fmt/ostream.h>
#endif

#include <hindsight/resolver.hpp>
#include <hindsight/stacktrace.hpp>

namespace {

auto print_stacktrace_here() {
    using namespace std::string_view_literals;

    const auto entries = hindsight::capture_stacktrace();
    fmt::print("Captured {} stacktrace entries\n"sv, entries.size());

    auto resolver = hindsight::resolver{};

    auto entry_idx = std::size_t{};
    for (const auto entry : entries) {
        fmt::print("{:02}: {}\n"sv, entry_idx, entry);
        ++entry_idx;
        auto logical_entries = std::vector<hindsight::logical_stacktrace_entry>{};
        resolver.resolve(entry, std::back_inserter(logical_entries), std::unreachable_sentinel);
        for (const auto &logical : logical_entries) {
            auto source = logical.source();
            fmt::print("    {}{} ({}:{})\n"sv,
                       logical.is_inline() ? "[inline] "sv : "         "sv,
                       logical.symbol(),
                       source.file_name,
                       source.line_number);
        }
    }
}

template<typename Fn>
auto call_through_std_function(Fn &&fn) -> decltype(auto) {
    return std::function<void()>{std::forward<Fn>(fn)}();
}

} // namespace

auto main() -> int {
    call_through_std_function([] { print_stacktrace_here(); });
}
