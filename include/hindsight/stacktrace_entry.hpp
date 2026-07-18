/*
 * Copyright 2025 Andrey Glebov
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

#ifndef HINDSIGHT_INCLUDE_HINDSIGHT_STACKTRACE_ENTRY_HPP
#define HINDSIGHT_INCLUDE_HINDSIGHT_STACKTRACE_ENTRY_HPP

#include <hindsight/detail/config.hpp>

#include <compare>
#include <cstdint>
#include <format>
#include <iosfwd>
#include <limits>
#include <string_view>
#include <type_traits>

namespace hindsight {

struct from_native_handle_t {
    explicit constexpr from_native_handle_t() = default;
};

inline constexpr auto from_native_handle = from_native_handle_t{};


class HINDSIGHT_API stacktrace_entry {
public:
    using native_handle_type = std::uintptr_t;

    constexpr stacktrace_entry() noexcept = default;

    constexpr stacktrace_entry(from_native_handle_t /* from_native_handle */, const native_handle_type handle) noexcept
            : m_handle{handle} {}

    [[nodiscard]] constexpr explicit operator bool() const noexcept { return m_handle != 0; }

    [[nodiscard]] constexpr auto native_handle() const noexcept -> native_handle_type { return m_handle; }

    [[nodiscard]] friend constexpr auto operator==(stacktrace_entry lhs, stacktrace_entry rhs) noexcept
            -> bool = default;

    [[nodiscard]] friend constexpr auto operator<=>(stacktrace_entry lhs, stacktrace_entry rhs) noexcept
            -> std::strong_ordering = default;

    HINDSIGHT_API friend auto operator<<(std::ostream &stream, stacktrace_entry entry) -> std::ostream &;
    HINDSIGHT_API friend auto operator<<(std::wostream &stream, stacktrace_entry entry) -> std::wostream &;

private:
    native_handle_type m_handle{};
};

} // namespace hindsight

template<typename CharT>
struct std::formatter<hindsight::stacktrace_entry, CharT> : std::formatter<std::basic_string_view<CharT>, CharT> {
private:
    static consteval auto native_entry_format_string() -> std::basic_string_view<CharT> {
        using namespace std::string_view_literals;
        using native_handle_type = hindsight::stacktrace_entry::native_handle_type;
        constexpr auto is_32bit = std::numeric_limits<native_handle_type>::digits == 32;
        constexpr auto is_64bit = std::numeric_limits<native_handle_type>::digits == 64;

// One character per 4 bits + 2 characters for "0x":
#define HINDSIGHT_NATIVE_ENTRY_FORMAT_STRING_PREFIXED(prefix)                                                          \
    if constexpr (is_32bit) {                                                                                          \
        return prefix##"{:#010x}"sv;                                                                                   \
    } else if constexpr (is_64bit) {                                                                                   \
        return prefix##"{:#018x}"sv;                                                                                   \
    } else {                                                                                                           \
        static_assert(false, "Unsupported native handle size");                                                        \
    }

        if constexpr (std::is_same_v<CharT, char>) {
            HINDSIGHT_NATIVE_ENTRY_FORMAT_STRING_PREFIXED()
        } else if constexpr (std::is_same_v<CharT, wchar_t>) {
            HINDSIGHT_NATIVE_ENTRY_FORMAT_STRING_PREFIXED(L)
        } else {
            static_assert(false, "Unsupported character type");
        }

#undef HINDSIGHT_NATIVE_ENTRY_FORMAT_STRING_PREFIXED
    }

public:
    template<typename OutputIt>
    auto format(const hindsight::stacktrace_entry entry, basic_format_context<OutputIt, CharT> &context) const
            -> basic_format_context<OutputIt, CharT>::iterator {
        return std::format_to(context.out(), formatter::native_entry_format_string(), entry.native_handle());
    }
};

#endif // HINDSIGHT_INCLUDE_HINDSIGHT_STACKTRACE_ENTRY_HPP
