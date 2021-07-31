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

#include <hindsight/simple.hpp>

#include <iomanip>
#include <iostream>
#include <iterator>
#include <string_view>
#include <vector>

#include <hindsight/capture.hpp>
#include <hindsight/resolver.hpp>

namespace hindsight {

auto print_stacktrace(std::ostream &stream, const std::span<const stacktrace_entry> entries) -> void {
    static auto global_resolver = resolver{};

    auto logical_entries = std::vector<logical_stacktrace_entry>{};
    logical_entries.reserve(entries.size());
    for (const auto entry : entries) {
        global_resolver.resolve(entry, std::back_inserter(logical_entries), std::unreachable_sentinel);
    }

    using namespace std::string_view_literals;
    for (auto index = std::size_t{0}; const auto &logical_entry : logical_entries) {
        stream << std::setw(3) << index << ": "sv << logical_entry.physical() << ':';
        if (logical_entry.is_inline()) {
            stream << " [inline]"sv;
        }
        const auto symbol = logical_entry.symbol();
        if (!symbol.empty()) {
            stream << ' ' << logical_entry.symbol();
        }
        const auto source = logical_entry.source();
        if (!source.file_name.empty()) {
            stream << " ("sv << source.file_name << ':' << source.line_number << ')';
        }
        stream << '\n';
        ++index;
    }
}

auto print_stacktrace_here() -> void { print_stacktrace_here(std::cerr); }

auto print_stacktrace_here(std::ostream &stream) -> void {
    const auto entries = capture_stacktrace();
    print_stacktrace(stream, entries);
}

} // namespace hindsight
