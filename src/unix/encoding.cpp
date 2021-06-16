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

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <iterator>
#include <new>
#include <sstream>
#include <system_error>

#include <langinfo.h>
#include <locale.h> // NOLINT(hicpp-deprecated-headers): newlocale is defined in <locale.h> by POSIX (not C++)

#include "../util/finally.hpp"

namespace hindsight::unix {

namespace {

constexpr const auto *utf8_encoding_name = "UTF-8";

constexpr auto error_return_code = static_cast<std::size_t>(-1);

auto reset_conversion_state(const iconv_t conversion) noexcept -> void {
    iconv(conversion, nullptr, nullptr, nullptr, nullptr);
}

template<typename CharT>
    requires(sizeof(CharT) == 1)
[[nodiscard]] auto iconv_transcode(const iconv_t conversion, const std::string_view input) -> std::basic_string<CharT> {
    auto output = std::basic_string<CharT>(input.size(), CharT{});
    if (input.empty()) {
        return output; // avoid resetting the conversion state and potentially resizing
    }

    reset_conversion_state(conversion);
    output.resize(output.capacity());

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast): iconv does not modify the input buffer
    auto *input_buffer = const_cast<char *>(input.data());
    auto input_remaining = input.size();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): safe because char can alias anything
    auto *output_buffer = reinterpret_cast<char *>(output.data());
    auto output_remaining = output.size();

    while (input_remaining != 0) {
        if (iconv(conversion, &input_buffer, &input_remaining, &output_buffer, &output_remaining) ==
            error_return_code) {
            switch (errno) {
                case EILSEQ: // Input conversion stopped due to an input byte that does not belong to the input codeset.
                    [[fallthrough]];
                case EINVAL: // Input conversion stopped due to an incomplete character or shift sequence at the end of
                             // the input buffer.
                    assert(input_remaining != 0);
                    input_buffer = std::ranges::next(input_buffer);
                    --input_remaining;
                    break;
                case E2BIG: { // Input conversion stopped due to lack of space in the output buffer.
                    const auto used_output = output.size() - output_remaining;

                    const auto new_size = output.size() + std::min(output.size(), output.max_size() - output.size());
                    if (new_size == output.size()) {
                        throw std::bad_alloc{};
                    }
                    output.resize(new_size);
                    output.resize(output.capacity());

                    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): safe because char can alias anything
                    output_buffer = std::ranges::next(reinterpret_cast<char *>(output.data()),
                                                      static_cast<std::ptrdiff_t>(used_output));
                    output_remaining = output.size() - used_output;
                    break;
                }
                default:
                    throw std::system_error{errno, std::system_category(), "Failed to transcode a string"};
            }
        }
    }

    output.resize(output.size() - output_remaining);
    return output;
}

} // namespace

auto create_transcoder(const char *from, const char *to) -> unique_iconv {
    auto conversion = unique_iconv{iconv_open(to, from)};
    if (!conversion) {
        using namespace std::string_view_literals;
        auto message = std::ostringstream{};
        message << "Failed to create a transcoder from "sv << from << " to "sv << to;
        throw std::system_error{errno, std::system_category(), std::move(message).str()};
    }
    return conversion;
}

auto create_utf8_sanitizer() -> unique_iconv { return create_transcoder(utf8_encoding_name, utf8_encoding_name); }

auto create_utf8_to_current_transcoder() -> unique_iconv {
    // NOLINTNEXTLINE(readability-qualified-auto): locale_t is not necessarily a pointer
    const auto locale = newlocale(LC_CTYPE_MASK, // NOLINT(hicpp-signed-bitwise): inside macro expansion
                                  "", // NOLINTNEXTLINE(hicpp-use-nullptr): locale_t is not necessarily a pointer
                                  static_cast<locale_t>(0));
    const auto locale_guard = util::finally{[&]() noexcept { freelocale(locale); }};
    const auto *const codeset = nl_langinfo_l(CODESET, locale);
    auto conversion = unique_iconv{iconv_open(codeset, utf8_encoding_name)};
    return create_transcoder(utf8_encoding_name, codeset);
}

auto get_utf8_sanitizer() -> iconv_t {
    thread_local static const auto conversion = create_utf8_sanitizer();
    return conversion.get();
}

auto get_utf8_to_current_transcoder() -> iconv_t {
    thread_local static const auto conversion = create_utf8_to_current_transcoder();
    return conversion.get();
}

auto transcode(const iconv_t conversion, const std::string_view input, std::in_place_type_t<char> /* char_type */)
        -> std::string {
    return iconv_transcode<char>(conversion, input);
}

auto transcode(const iconv_t conversion, const std::string_view input, std::in_place_type_t<char8_t> /* char_type */)
        -> std::u8string {
    return iconv_transcode<char8_t>(conversion, input);
}

} // namespace hindsight::unix
