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

#if !defined HINDSIGHT_OS_WINDOWS && !defined HINDSIGHT_OS_LINUX

    #include <hindsight/resolver.hpp>

    #include <cerrno>
    #include <exception>
    #include <new>
    #include <optional>
    #include <string_view>

    #include <backtrace.h>

    #include "itanium_abi/demangle.hpp"
    #include "unix/encoding.hpp"

namespace hindsight {

namespace {

[[nodiscard]] auto get_backtrace_state() -> backtrace_state * {
    static auto *const global_state = [] { // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
        auto is_bad_alloc = false;
        auto *const result = backtrace_create_state(
                nullptr,
                true,
                [](void *const data, const char * /* msg */, const int errnum) noexcept {
                    auto &is_bad_alloc = *static_cast<bool *>(data);
                    is_bad_alloc = errnum == ENOMEM;
                },
                &is_bad_alloc);
        if (!result && is_bad_alloc) {
            throw std::bad_alloc{};
        }
        return result;
    }();
    return global_state;
}

} // namespace

struct logical_stacktrace_entry::impl_tag {};

logical_stacktrace_entry::logical_stacktrace_entry(impl_tag && /* impl */,
                                                   const stacktrace_entry physical,
                                                   const bool is_inline,
                                                   std::string &&symbol,
                                                   source_location &&source) noexcept
        : m_physical{physical},
          m_is_inline{is_inline},
          m_symbol{std::move(symbol)},
          m_source{std::move(source)} {}

logical_stacktrace_entry::logical_stacktrace_entry(const logical_stacktrace_entry &other) = default;

logical_stacktrace_entry::~logical_stacktrace_entry() = default;

namespace {

template<typename CharT>
auto demangle_and_encode_symbol(const std::string &symbol, const auto get_transcoder) -> std::basic_string<CharT> {
    const auto demangled = symbol.empty() ? nullptr : itanium_abi::demangle(symbol.c_str());
    const auto unsanitized = demangled ? std::string_view{demangled.get()} : std::string_view{symbol};
    if (unsanitized.empty()) {
        return {};
    }
    return unix::transcode(get_transcoder(), unsanitized, std::in_place_type<CharT>);
}

} // namespace

auto logical_stacktrace_entry::symbol() const -> std::string {
    return demangle_and_encode_symbol<char>(m_symbol, [] { return unix::get_utf8_to_current_transcoder(); });
}

auto logical_stacktrace_entry::u8_symbol() const -> std::u8string {
    return demangle_and_encode_symbol<char8_t>(m_symbol, [] { return unix::get_utf8_sanitizer(); });
}

namespace {

template<typename CharT>
auto encode_file_name(const std::string &unsanitized, const auto get_transcoder) -> std::basic_string<CharT> {
    if (unsanitized.empty()) {
        return {};
    }
    return unix::transcode(get_transcoder(), unsanitized, std::in_place_type<CharT>);
}

} // namespace

auto logical_stacktrace_entry::source() const -> source_location {
    return {.file_name =
                    encode_file_name<char>(m_source.file_name, [] { return unix::get_utf8_to_current_transcoder(); }),
            .line_number = m_source.line_number};
}

auto logical_stacktrace_entry::u8_source() const -> u8_source_location {
    return {.file_name = encode_file_name<char8_t>(m_source.file_name, [] { return unix::get_utf8_sanitizer(); }),
            .line_number = m_source.line_number};
}

auto logical_stacktrace_entry::set_inline(impl_tag && /* impl */) noexcept -> void { m_is_inline = true; }


resolver::resolver() = default;

auto resolver::resolve_impl(const stacktrace_entry entry, const resolve_cb callback) -> void {
    const auto on_failure = [&] { callback({{}, entry, false, {}, {}}); };

    auto *const global_state = get_backtrace_state();
    if (!global_state) {
        on_failure();
        return;
    }

    struct cb_state {
        const stacktrace_entry entry;
        const resolve_cb callback;
        std::optional<logical_stacktrace_entry> buffered_entry;
        std::exception_ptr exception;

        bool entry_issued = false;
        bool done = false;

        auto flush_buffered_entry(const bool is_inline) {
            if (buffered_entry && !done) {
                if (is_inline) {
                    buffered_entry->set_inline({});
                }
                done = callback(std::move(*buffered_entry));
                entry_issued = true;
                buffered_entry.reset();
            }
            return done;
        }
    } state{.entry = entry, .callback = callback, .buffered_entry = std::nullopt, .exception = nullptr};

    backtrace_pcinfo(
            global_state,
            entry.native_handle(),
            [](void *const data,
               std::uintptr_t /* pc */,
               const char *const filename,
               const int lineno,
               const char *const function) noexcept -> int {
                auto &state = *static_cast<cb_state *>(data);
                try {
                    if (state.flush_buffered_entry(true)) {
                        return 0;
                    }
                    state.buffered_entry = logical_stacktrace_entry{
                            {},
                            state.entry,
                            false,
                            std::string{function ? std::string_view{function} : std::string_view{}},
                            {.file_name = std::string{filename ? std::string_view{filename} : std::string_view{}},
                             .line_number = static_cast<std::uint_least32_t>(lineno)}};
                    return 0;
                } catch (...) {
                    state.exception = std::current_exception();
                    return 1;
                }
            },
            [](void *const data, const char * /* msg */, const int errnum) noexcept {
                auto &state = *static_cast<cb_state *>(data);
                try {
                    state.flush_buffered_entry(true);
                } catch (...) {
                    state.exception = std::current_exception();
                    return;
                }
                if (errnum == ENOMEM) {
                    state.exception = std::make_exception_ptr(std::bad_alloc{});
                }
            },
            &state);
    if (state.exception) {
        std::rethrow_exception(state.exception);
    }

    state.flush_buffered_entry(false);
    if (!state.entry_issued) {
        on_failure();
    }
}

} // namespace hindsight

#endif
