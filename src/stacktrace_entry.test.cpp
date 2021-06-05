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

#include <hindsight/config.hpp>

#include <compare>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#ifdef HINDSIGHT_HAS_STD_FORMAT
    #include <format>
#endif

#ifdef HINDSIGHT_WITH_FMT
HINDSIGHT_PRAGMA_MSVC("warning(push)")
HINDSIGHT_PRAGMA_MSVC("warning(disable : 4389)") // '==': signed/unsigned mismatch (comparing char32_t to char)
    #include <fmt/format.h>
HINDSIGHT_PRAGMA_MSVC("warning(pop)")
#endif

#include <catch2/catch.hpp>

#include <hindsight/stacktrace_entry.hpp>

namespace hindsight {

namespace {

constexpr auto ptr_is_32bit = std::numeric_limits<std::uintptr_t>::digits == 32;
constexpr auto ptr_is_64bit = std::numeric_limits<std::uintptr_t>::digits == 64;
static_assert(ptr_is_32bit || ptr_is_64bit);

#define HINDSIGHT_TESTS_STACKTRACE_ENTRY_STRING(name, value32, value64, string_type, prefix)                           \
    const auto name##_uintptr_##string_type = []() -> std::string_type {                                               \
        if constexpr (ptr_is_32bit) {                                                                                  \
            return prefix## #value32;                                                                                  \
        } else if constexpr (ptr_is_64bit) {                                                                           \
            return prefix## #value64;                                                                                  \
        }                                                                                                              \
    }();

#define HINDSIGHT_TESTS_STACKTRACE_ENTRY_VALUE_AND_STRING(name, value32, value64)                                      \
    constexpr auto name##_uintptr_value = []() -> std::uintptr_t {                                                     \
        if constexpr (ptr_is_32bit) {                                                                                  \
            return value32;                                                                                            \
        } else if constexpr (ptr_is_64bit) {                                                                           \
            return value64;                                                                                            \
        }                                                                                                              \
    }();                                                                                                               \
    HINDSIGHT_TESTS_STACKTRACE_ENTRY_STRING(name, value32, value64, string, )                                          \
    HINDSIGHT_TESTS_STACKTRACE_ENTRY_STRING(name, value32, value64, wstring, L)                                        \
    HINDSIGHT_TESTS_STACKTRACE_ENTRY_STRING(name, value32, value64, u8string, u8)                                      \
    HINDSIGHT_TESTS_STACKTRACE_ENTRY_STRING(name, value32, value64, u16string, u)                                      \
    HINDSIGHT_TESTS_STACKTRACE_ENTRY_STRING(name, value32, value64, u32string, U)

HINDSIGHT_TESTS_STACKTRACE_ENTRY_VALUE_AND_STRING(large, 0xabcdef01, 0xabcdef0123456789)
HINDSIGHT_TESTS_STACKTRACE_ENTRY_VALUE_AND_STRING(small, 0x00001234, 0x0000123456789abc)

#undef HINDSIGHT_TESTS_STACKTRACE_ENTRY_VALUE_AND_STRING
#undef HINDSIGHT_TESTS_STACKTRACE_ENTRY_STRING

} // namespace

TEST_CASE("default-constructed stacktrace_entry is empty") { REQUIRE(!stacktrace_entry{}); }

TEST_CASE("stacktrace_entry stores the native handle unchanged") {
    REQUIRE(stacktrace_entry{from_native_handle, small_uintptr_value}.native_handle() == small_uintptr_value);
}

TEST_CASE("stacktrace_entry compare the same as their native handles") {
    STATIC_REQUIRE(stacktrace_entry{from_native_handle, large_uintptr_value} ==
                   stacktrace_entry{from_native_handle, large_uintptr_value});
    STATIC_REQUIRE(stacktrace_entry{from_native_handle, large_uintptr_value} <=
                   stacktrace_entry{from_native_handle, large_uintptr_value});
    STATIC_REQUIRE(stacktrace_entry{from_native_handle, large_uintptr_value} >=
                   stacktrace_entry{from_native_handle, large_uintptr_value});

    STATIC_REQUIRE(stacktrace_entry{from_native_handle, large_uintptr_value} !=
                   stacktrace_entry{from_native_handle, small_uintptr_value});

    STATIC_REQUIRE(stacktrace_entry{from_native_handle, small_uintptr_value} <
                   stacktrace_entry{from_native_handle, large_uintptr_value});
    STATIC_REQUIRE(stacktrace_entry{from_native_handle, small_uintptr_value} <=
                   stacktrace_entry{from_native_handle, large_uintptr_value});

    STATIC_REQUIRE(stacktrace_entry{from_native_handle, large_uintptr_value} >
                   stacktrace_entry{from_native_handle, small_uintptr_value});
    STATIC_REQUIRE(stacktrace_entry{from_native_handle, large_uintptr_value} >=
                   stacktrace_entry{from_native_handle, small_uintptr_value});

    STATIC_REQUIRE(stacktrace_entry{from_native_handle, large_uintptr_value} <=>
                           stacktrace_entry{from_native_handle, large_uintptr_value} ==
                   std::strong_ordering::equal);
    STATIC_REQUIRE(stacktrace_entry{from_native_handle, small_uintptr_value} <=>
                           stacktrace_entry{from_native_handle, large_uintptr_value} ==
                   std::strong_ordering::less);
    STATIC_REQUIRE(stacktrace_entry{from_native_handle, large_uintptr_value} <=>
                           stacktrace_entry{from_native_handle, small_uintptr_value} ==
                   std::strong_ordering::greater);
}

TEST_CASE("stacktrace_entry's operator<< produces a hexadecimal number and does not change the stream's state") {
    constexpr auto entry = stacktrace_entry{from_native_handle, large_uintptr_value};
    {
        auto stream = std::stringstream{};
        stream << std::setfill('*') << std::setw(4) << 42 << entry << 42 << std::setw(4) << 42;
        REQUIRE(stream.str() == std::string{"**42"} + large_uintptr_string + "42**42");
    }
    {
        auto stream = std::wstringstream{};
        stream << std::setfill(L'*') << std::setw(4) << 42 << entry << 42 << std::setw(4) << 42;
        REQUIRE(stream.str() == std::wstring{L"**42"} + large_uintptr_wstring + L"42**42");
    }
}

TEST_CASE("stacktrace_entry's operator<< adds zero-padding and does not change the stream's state") {
    constexpr auto entry = stacktrace_entry{from_native_handle, small_uintptr_value};
    {
        auto stream = std::stringstream{};
        stream << std::setfill('*') << std::setw(4) << 42 << entry << 42 << std::setw(4) << 42;
        REQUIRE(stream.str() == std::string{"**42"} + small_uintptr_string + "42**42");
    }
    {
        auto stream = std::wstringstream{};
        stream << std::setfill(L'*') << std::setw(4) << 42 << entry << 42 << std::setw(4) << 42;
        REQUIRE(stream.str() == std::wstring{L"**42"} + small_uintptr_wstring + L"42**42");
    }
}

#ifdef HINDSIGHT_HAS_STD_FORMAT

TEST_CASE("stacktrace_entry's std::formatter specialization produces a hexadecimal number") {
    constexpr auto entry = stacktrace_entry{from_native_handle, large_uintptr_value};
    REQUIRE(std::format("{}", entry) == large_uintptr_string);
    REQUIRE(std::format(L"{}", entry) == large_uintptr_wstring);
}

TEST_CASE("stacktrace_entry's std::formatter specialization adds zero-padding") {
    constexpr auto entry = stacktrace_entry{from_native_handle, small_uintptr_value};
    REQUIRE(std::format("{}", entry) == small_uintptr_string);
    REQUIRE(std::format(L"{}", entry) == small_uintptr_wstring);
}

TEST_CASE("stacktrace_entry's std::formatter specialization throws for a non-empty format specification") {
    constexpr auto entry = stacktrace_entry{from_native_handle, small_uintptr_value};
    REQUIRE_THROWS_AS(std::format("{:x}", entry), std::format_error);
    REQUIRE_THROWS_AS(std::format(L"{:x}", entry), std::format_error);
}

#endif

#ifdef HINDSIGHT_WITH_FMT

    #ifdef FMT_OSTREAM_H_
        #error <fmt/ostream.h> must not be included in this file so that the fmt::formatter specialization can be tested
    #endif

TEST_CASE("stacktrace_entry's fmt::formatter specialization produces a hexadecimal number") {
    constexpr auto entry = stacktrace_entry{from_native_handle, large_uintptr_value};
    REQUIRE(fmt::format("{}", entry) == large_uintptr_string);
    REQUIRE(fmt::format(L"{}", entry) == large_uintptr_wstring);
    REQUIRE(fmt::format(u8"{}", entry) == large_uintptr_u8string);
    REQUIRE(fmt::format(u"{}", entry) == large_uintptr_u16string);
    REQUIRE(fmt::format(U"{}", entry) == large_uintptr_u32string);
}

TEST_CASE("stacktrace_entry's fmt::formatter specialization adds zero-padding") {
    constexpr auto entry = stacktrace_entry{from_native_handle, small_uintptr_value};
    REQUIRE(fmt::format("{}", entry) == small_uintptr_string);
    REQUIRE(fmt::format(L"{}", entry) == small_uintptr_wstring);
    REQUIRE(fmt::format(u8"{}", entry) == small_uintptr_u8string);
    REQUIRE(fmt::format(u"{}", entry) == small_uintptr_u16string);
    REQUIRE(fmt::format(U"{}", entry) == small_uintptr_u32string);
}

TEST_CASE("stacktrace_entry's fmt::formatter specialization throws for a non-empty format specification") {
    constexpr auto entry = stacktrace_entry{from_native_handle, small_uintptr_value};
    REQUIRE_THROWS_AS(fmt::format("{:x}", entry), fmt::format_error);
    REQUIRE_THROWS_AS(fmt::format(L"{:x}", entry), fmt::format_error);
    REQUIRE_THROWS_AS(fmt::format(u8"{:x}", entry), fmt::format_error);
    REQUIRE_THROWS_AS(fmt::format(u"{:x}", entry), fmt::format_error);
    REQUIRE_THROWS_AS(fmt::format(U"{:x}", entry), fmt::format_error);
}

#endif

} // namespace hindsight
