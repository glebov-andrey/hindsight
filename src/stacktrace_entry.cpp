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

#include <hindsight/stacktrace_entry.hpp>

#include <iomanip>
#include <ostream>

namespace hindsight {

namespace {

template<typename Char>
[[nodiscard]] auto format_entry(std::basic_ostream<Char> &stream, const stacktrace_entry entry)
        -> std::basic_ostream<Char> & {
    static constexpr auto width = std::numeric_limits<stacktrace_entry::native_handle_type>::digits / 4 + 2;

    const auto prev_format_flags = stream.flags();
    const auto prev_fill_char = stream.fill();
    stream << std::hex << std::showbase << std::internal << std::setw(width) << std::setfill(Char{'0'})
           << entry.native_handle();
    stream.fill(prev_fill_char);
    stream.flags(prev_format_flags);
    return stream;
}

} // namespace

auto operator<<(std::ostream &stream, const stacktrace_entry entry) -> std::ostream & {
    return format_entry(stream, entry);
}

auto operator<<(std::wostream &stream, const stacktrace_entry entry) -> std::wostream & {
    return format_entry(stream, entry);
}

#ifdef HINDSIGHT_WITH_FMT
namespace detail {

auto throw_format_error() -> void {
    throw fmt::format_error{"invalid format specification for hindsight::stacktrace_entry"};
}

} // namespace detail
#endif

} // namespace hindsight
