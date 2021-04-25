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

#include "encoding.hpp"

#include <cassert>
#include <concepts>
#include <limits>
#include <system_error>

#include <Windows.h>

namespace hindsight::windows {

namespace {

template<typename CharT>
    requires(sizeof(CharT) == 1)
[[nodiscard]] auto as_char_ptr(CharT *const ptr) noexcept -> char * {
    if constexpr (std::same_as<CharT, char>) {
        return ptr;
    } else {
        return reinterpret_cast<char *>(ptr); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    }
}

template<typename CharT>
[[nodiscard]] auto wide_to_multi_byte(const std::wstring_view wide, const UINT code_page) -> std::basic_string<CharT> {
    if (wide.empty()) {
        return {};
    }
    if (wide.size() > static_cast<unsigned>(std::numeric_limits<int>::max())) {
        throw std::system_error{std::make_error_code(std::errc::invalid_argument), "The wide string is too long"};
    }
    const auto multi_byte_size = WideCharToMultiByte(code_page,
                                                     {},
                                                     wide.data(),
                                                     static_cast<int>(wide.size()),
                                                     nullptr,
                                                     0,
                                                     nullptr,
                                                     nullptr);
    assert(multi_byte_size >= 0);
    if (multi_byte_size == 0) {
        throw std::system_error{static_cast<int>(GetLastError()),
                                std::system_category(),
                                "Failed to convert wide string to multi-byte encoding"};
    }
    auto multi_byte = std::basic_string<CharT>(static_cast<std::size_t>(multi_byte_size), CharT{});
    const auto cvt_multi_byte_size = WideCharToMultiByte(code_page,
                                                         {},
                                                         wide.data(),
                                                         static_cast<int>(wide.size()),
                                                         as_char_ptr(multi_byte.data()),
                                                         multi_byte_size,
                                                         nullptr,
                                                         nullptr);
    assert(cvt_multi_byte_size == multi_byte_size);
    if (cvt_multi_byte_size == 0) {
        throw std::system_error{static_cast<int>(GetLastError()),
                                std::system_category(),
                                "Failed to convert wide string to multi-byte encoding"};
    }
    return multi_byte;
}

} // namespace

auto wide_to_narrow(const std::wstring_view wide) -> std::string { return wide_to_multi_byte<char>(wide, CP_ACP); }

auto wide_to_utf8(const std::wstring_view wide) -> std::u8string { return wide_to_multi_byte<char8_t>(wide, CP_UTF8); }

} // namespace hindsight::windows
