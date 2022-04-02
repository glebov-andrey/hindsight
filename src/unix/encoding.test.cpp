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

#include <hindsight/detail/config.hpp>

#ifdef HINDSIGHT_OS_UNIX

    #include <string_view>
    #include <thread>
    #include <utility>

    #include <catch2/catch.hpp>

    #include "encoding.hpp"

namespace hindsight::unix {

namespace {
using namespace std::string_view_literals;
}

TEST_CASE("unix: create_transcoder fails with an invalid encoding") {
    REQUIRE_THROWS_AS(create_transcoder("DEFINITELY_AND_ENCODING_NAME", "UTF-8"), std::system_error);
}

TEST_CASE("unix: create_utf8_sanitizer returns a converter") {
    const auto conversion = create_utf8_sanitizer();
    REQUIRE(conversion);
}

TEST_CASE("unix: create_utf8_to_current_transcoder returns a converter") {
    const auto conversion = create_utf8_to_current_transcoder();
    REQUIRE(conversion);
}


TEST_CASE("unix: get_utf8_sanitizer returns different handles in different threads") {
    const auto local_handle = get_utf8_sanitizer();
    REQUIRE(local_handle);
    auto other_handle = iconv_t{};
    std::thread{[&] { other_handle = get_utf8_sanitizer(); }}.join();
    REQUIRE(other_handle);
    REQUIRE(other_handle != local_handle);
}

TEST_CASE("unix: get_utf8_to_current_transcoder returns different handles in different threads") {
    const auto local_handle = get_utf8_to_current_transcoder();
    REQUIRE(local_handle);
    auto other_handle = iconv_t{};
    std::thread{[&] { other_handle = get_utf8_to_current_transcoder(); }}.join();
    REQUIRE(other_handle);
    REQUIRE(other_handle != local_handle);
}


TEST_CASE("unix: sanitizing UTF-8 strings: a valid string (char)") {
    const auto conversion = create_utf8_sanitizer();
    constexpr auto input_string = "\xC2\xABHello, World!\xC2\xBB"sv; // «...»
    constexpr auto output_string = input_string;
    REQUIRE(transcode(conversion.get(), input_string, std::in_place_type<char>) == output_string);
}

TEST_CASE("unix: sanitizing UTF-8 strings: a valid string (char8_t)") {
    const auto conversion = create_utf8_sanitizer();
    constexpr auto input_string = "\xC2\xABHello, World!\xC2\xBB"sv; // «...»
    constexpr auto output_string = u8"«Hello, World!»"sv;
    REQUIRE(transcode(conversion.get(), input_string, std::in_place_type<char8_t>) == output_string);
}

TEST_CASE("unix: sanitizing UTF-8 strings: a string with invalid sequences") {
    const auto conversion = create_utf8_sanitizer();
    //                                    v-- should be a 2 code unit sequence
    //                                    v             v-- should be a 3 code unit sequence
    //                                    v             v           v-- incomplete sequence
    constexpr auto input_string = "Hello, \xC3\x28World!\xE2\x82\x28\xF0"sv;
    constexpr auto output_string = "Hello, \x28World!\x28"sv;
    REQUIRE(transcode(conversion.get(), input_string, std::in_place_type<char>) == output_string);
}

TEST_CASE("unix: sanitizing UTF-8 strings: an empty string") {
    const auto conversion = create_utf8_sanitizer();
    REQUIRE(transcode(conversion.get(), ""sv, std::in_place_type<char>) == ""sv);
}

namespace {

[[nodiscard]] auto create_utf8_to_iso8859_1() {
    auto conversion = create_transcoder("UTF-8", "ISO-8859-1");
    REQUIRE(conversion);
    return conversion;
}

} // namespace

TEST_CASE("unix: transcoding UTF-8 to ISO-8859-1: a string with only ISO-8859-1 code points") {
    const auto conversion = create_utf8_to_iso8859_1();
    constexpr auto input_string = "Goodbye, ISO-8859-1"sv;
    constexpr auto output_string = input_string;
    REQUIRE(transcode(conversion.get(), input_string, std::in_place_type<char>) == output_string);
}

TEST_CASE("unix: transcoding UTF-8 to ISO-8859-1: a string with invalid sequences") {
    const auto conversion = create_utf8_to_iso8859_1();
    //                             v-- should be a 2 code unit sequence
    //                             v                v-- should be a 4 code unit sequence
    //                             v                v                         v-- incomplete sequence
    constexpr auto input_string = "\xC3\x28Goodbye, \xF0\x28\x8C\x28ISO-8859-1\xF0"sv;
    constexpr auto output_string = "\x28Goodbye, \x28\x28ISO-8859-1"sv;
    REQUIRE(transcode(conversion.get(), input_string, std::in_place_type<char>) == output_string);
}

TEST_CASE("unix: transcoding UTF-8 to ISO-8859-1: a string with non-ASCII characters") {
    const auto conversion = create_utf8_to_iso8859_1();
    constexpr auto input_string = "Goodbye, \xC2\xABISO-8859-1\xC2\xBB"sv; // «...»
    constexpr auto output_string = "Goodbye, \xABISO-8859-1\xBB"sv;
    REQUIRE(transcode(conversion.get(), input_string, std::in_place_type<char>) == output_string);
}

namespace {

[[nodiscard]] auto create_iso8859_1_to_utf8() {
    auto conversion = create_transcoder("ISO-8859-1", "UTF-8");
    REQUIRE(conversion);
    return conversion;
}

} // namespace

TEST_CASE("unix: transcoding ISO-8859-1 to UTF-8: a string that gets longer after transcoding (non-ASCII)") {
    auto conversion = create_iso8859_1_to_utf8();
    constexpr auto input_string = "Goodbye, \xABISO-8859-1\xBB"sv; // «...»
    constexpr auto output_string = "Goodbye, \xC2\xABISO-8859-1\xC2\xBB"sv;
    REQUIRE(transcode(conversion.get(), input_string, std::in_place_type<char>) == output_string);
}

} // namespace hindsight::unix

#endif
