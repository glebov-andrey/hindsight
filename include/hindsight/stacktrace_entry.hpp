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

#ifndef HINDSIGHT_INCLUDE_HINDSIGHT_STACKTRACE_ENTRY_HPP
#define HINDSIGHT_INCLUDE_HINDSIGHT_STACKTRACE_ENTRY_HPP

#include <hindsight/config.hpp>

#include <compare>
#include <cstdint>
#include <iosfwd>
#if defined HINDSIGHT_HAS_STD_FORMAT || defined HINDSIGHT_WITH_FMT
    #include <concepts>
    #include <limits>
    #include <string_view>
    #ifdef HINDSIGHT_HAS_STD_FORMAT
        #include <format>
        #include <iterator>
    #endif
    #ifdef HINDSIGHT_WITH_FMT
        #include <fmt/format.h>
    #endif
#endif

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

#if defined HINDSIGHT_HAS_STD_FORMAT || defined HINDSIGHT_WITH_FMT
namespace detail {

template<typename Char>
constexpr auto stacktrace_entry_fmt_string = [] {
    using native_handle_type = stacktrace_entry::native_handle_type;
    static_assert(std::unsigned_integral<native_handle_type>);
    constexpr auto is_32bit = std::numeric_limits<native_handle_type>::digits == 32;
    constexpr auto is_64bit = std::numeric_limits<native_handle_type>::digits == 64;
    static_assert(is_32bit || is_64bit);
    static_assert(std::same_as<Char, char> || std::same_as<Char, wchar_t> || std::same_as<Char, char8_t> ||
                  std::same_as<Char, char16_t> || std::same_as<Char, char32_t>);

    using namespace std::string_view_literals;
    // One character per 4 bits + 2 characters for "0x":
    #define HINDSIGHT_DETAIL_STACKTRACE_ENTRY_FMT_STRING_IMPL(prefix)                                                  \
        if constexpr (is_32bit) {                                                                                      \
            return prefix##"{:#010x}"sv;                                                                               \
        } else if constexpr (is_64bit) {                                                                               \
            return prefix##"{:#018x}"sv;                                                                               \
        }
    if constexpr (std::same_as<Char, char>) {
        HINDSIGHT_DETAIL_STACKTRACE_ENTRY_FMT_STRING_IMPL()
    } else if constexpr (std::same_as<Char, wchar_t>) {
        HINDSIGHT_DETAIL_STACKTRACE_ENTRY_FMT_STRING_IMPL(L)
    } else if constexpr (std::same_as<Char, char8_t>) {
        HINDSIGHT_DETAIL_STACKTRACE_ENTRY_FMT_STRING_IMPL(u8)
    } else if constexpr (std::same_as<Char, char16_t>) {
        HINDSIGHT_DETAIL_STACKTRACE_ENTRY_FMT_STRING_IMPL(u)
    } else if constexpr (std::same_as<Char, char32_t>) {
        HINDSIGHT_DETAIL_STACKTRACE_ENTRY_FMT_STRING_IMPL(U)
    }
    #undef HINDSIGHT_DETAIL_STACKTRACE_ENTRY_FMT_STRING_IMPL
}();

template<typename CharT, void (&ThrowFormatError)()>
struct stacktrace_entry_format_parser {
    constexpr auto parse(auto &context) const {
        auto it = context.begin(); // NOLINT(readability-qualified-auto): we only know that it's an iterator
        if (it != context.end() && *it != CharT{'}'}) {
            ThrowFormatError();
        }
        return it;
    }
};

    #ifdef HINDSIGHT_HAS_STD_FORMAT
[[noreturn]] HINDSIGHT_API auto throw_std_format_error() -> void;
    #endif

    #ifdef HINDSIGHT_WITH_FMT
[[noreturn]] HINDSIGHT_API auto throw_fmt_format_error() -> void;
    #endif

} // namespace detail
#endif

} // namespace hindsight

#ifdef HINDSIGHT_HAS_STD_FORMAT

template<typename CharT>
struct std::formatter<hindsight::stacktrace_entry, CharT>
        : hindsight::detail::stacktrace_entry_format_parser<CharT, hindsight::detail::throw_std_format_error> {
    template<output_iterator<const CharT &> OutputIt>
    auto format(const hindsight::stacktrace_entry entry, basic_format_context<OutputIt, CharT> &context) const {
        return format_to(context.out(), hindsight::detail::stacktrace_entry_fmt_string<CharT>, entry.native_handle());
    }
};

#endif

#ifdef HINDSIGHT_WITH_FMT

template<typename CharT>
struct fmt::formatter<hindsight::stacktrace_entry, CharT>
        : hindsight::detail::stacktrace_entry_format_parser<CharT, hindsight::detail::throw_fmt_format_error> {
    template<typename OutputIt>
    auto format(const hindsight::stacktrace_entry entry, basic_format_context<OutputIt, CharT> &context) const {
        return format_to(context.out(), hindsight::detail::stacktrace_entry_fmt_string<CharT>, entry.native_handle());
    }
};

#endif

#endif // HINDSIGHT_INCLUDE_HINDSIGHT_STACKTRACE_ENTRY_HPP
