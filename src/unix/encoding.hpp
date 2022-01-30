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

#ifndef HINDSIGHT_SRC_UNIX_ENCODING_HPP
#define HINDSIGHT_SRC_UNIX_ENCODING_HPP

#include <hindsight/config.hpp>

#ifdef HINDSIGHT_OS_UNIX

    #include <cassert>
    #include <memory>
    #include <string>
    #include <string_view>
    #include <utility>

    #include <iconv.h>

namespace hindsight::unix {

class iconv_handle {
public:
    [[nodiscard]] iconv_handle() = default;

    // NOLINTNEXTLINE(hicpp-explicit-conversions): the implicit conversion is intended
    [[nodiscard]] explicit(false) iconv_handle(const iconv_t handle) noexcept : m_handle{handle} {}

    // NOLINTNEXTLINE(hicpp-explicit-conversions): the implicit conversion is intended
    [[nodiscard]] explicit(false) operator iconv_t() const noexcept { return m_handle; }

    [[nodiscard]] friend auto operator==(iconv_handle lhs, iconv_handle rhs) -> bool = default;

    [[nodiscard]] friend auto operator==(const iconv_handle lhs, std::nullptr_t /* rhs */) {
        return lhs.m_handle == invalid_handle_value;
    }

private:
    // POSIX does not specify whether iconv_t is a pointer or some other handle type
    // NOLINTNEXTLINE(google-readability-casting, cppcoreguidelines-pro-type-cstyle-cast, performance-no-int-to-ptr)
    static inline const auto invalid_handle_value = (iconv_t)-1;
    iconv_t m_handle{invalid_handle_value};
};

struct destroy_iconv {
    using pointer = iconv_handle;

    auto operator()(const iconv_handle handle) const noexcept {
        [[maybe_unused]] const auto result = iconv_close(handle);
        assert(result == 0);
    }
};

using unique_iconv = std::unique_ptr<iconv_t, destroy_iconv>;

[[nodiscard]] auto create_transcoder(const char *from, const char *to) -> unique_iconv;
[[nodiscard]] auto create_utf8_sanitizer() -> unique_iconv;
[[nodiscard]] auto create_utf8_to_current_transcoder() -> unique_iconv;

[[nodiscard]] auto get_utf8_sanitizer() -> iconv_t;
[[nodiscard]] auto get_utf8_to_current_transcoder() -> iconv_t;

[[nodiscard]] auto transcode(iconv_t conversion, std::string_view input, std::in_place_type_t<char> char_type)
        -> std::string;
[[nodiscard]] auto transcode(iconv_t conversion, std::string_view input, std::in_place_type_t<char8_t> char_type)
        -> std::u8string;

} // namespace hindsight::unix

#endif

#endif // HINDSIGHT_SRC_UNIX_ENCODING_HPP
