/*
 * Copyright 2026 Andrey Glebov
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
#include <cstdlib>
#include <exception>
#include <functional>
#include <iterator>
#include <print>
#include <string_view>
#include <utility>
#include <vector>

#include <hindsight/from_exception.hpp>
#include <hindsight/resolver.hpp>

namespace {

using namespace std::string_view_literals;

template<typename Fn>
auto call_through_std_function(Fn &&fn) -> decltype(auto) {
    return std::function<void()>{std::forward<Fn>(fn)}();
}

} // namespace

auto main() -> int {
    if (!hindsight::enable_stacktrace_from_exceptions()) {
        std::println("Failed to enable stacktrace from exceptions");
        return EXIT_FAILURE;
    }
    try {
        call_through_std_function([] { std::vector{0, 1, 2}.at(3) = 0; });
    } catch (const std::exception &ex) {
        std::println("Caught exception: {}", ex.what());

        const auto entries = hindsight::stacktrace_from_current_exception();
        std::println("Captured {} stacktrace entries from the exception:", entries.size());

        auto resolver = hindsight::resolver{};

        auto entry_idx = std::size_t{};
        for (const auto entry : entries) {
            auto logical_entries = std::vector<hindsight::logical_stacktrace_entry>{};
            resolver.resolve(entry, std::back_inserter(logical_entries), std::unreachable_sentinel);
            std::println("{:02}: {} ({})",
                         entry_idx,
                         entry,
                         logical_entries.empty() ? "<unknown module>"sv
                                                 : logical_entries.front().physical_module.filename().string());
            ++entry_idx;
            for (const auto &logical : logical_entries) {
                std::println("    {}{} ({}:{}:{})",
                             logical.is_inline ? "[inline] "sv : "         "sv,
                             logical.symbol,
                             logical.file_name,
                             logical.line_number,
                             logical.column_number);
            }
        }

        return EXIT_SUCCESS;
    }
}
