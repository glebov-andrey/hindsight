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

#ifndef HINDSIGHT_OS_WINDOWS

    #include <hindsight/resolver.hpp>

    #include <cerrno>
    #include <cstdlib>
    #include <exception>
    #include <memory>
    #include <new>
    #include <optional>
    #include <string_view>

    #include <cxxabi.h>

    #include <backtrace.h>

namespace hindsight {

namespace {

struct free_deleter {
    auto operator()(void *const ptr) const noexcept {
        std::free(ptr); // NOLINT(cppcoreguidelines-owning-memory, hicpp-no-malloc)
    }
};

template<typename T>
using unique_freeable = std::unique_ptr<T, free_deleter>;


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

auto logical_stacktrace_entry::symbol() const -> std::string { return m_symbol; }

// TODO: Implement conversion to UTF-8
// auto logical_stacktrace_entry::u8_symbol() const -> std::u8string {}

auto logical_stacktrace_entry::source() const -> source_location { return m_source; }

// auto logical_stacktrace_entry::u8_source() const -> u8_source_location {}

auto logical_stacktrace_entry::set_inline(impl_tag && /* impl */) noexcept -> void { m_is_inline = true; }


resolver::resolver() = default;

auto resolver::resolve_impl(const stacktrace_entry entry, resolve_cb *const callback, void *const user_data) -> void {
    const auto on_failure = [&] { callback({{}, entry, false, {}, {}}, user_data); };

    auto *const global_state = get_backtrace_state();
    if (!global_state) {
        on_failure();
        return;
    }

    struct cb_state {
        const stacktrace_entry entry;
        resolve_cb *const callback;
        void *const user_data;
        std::optional<logical_stacktrace_entry> buffered_entry;
        std::exception_ptr exception;

        bool entry_issued = false;
        bool done = false;

        auto flush_buffered_entry(const bool is_inline) {
            if (buffered_entry && !done) {
                if (is_inline) {
                    buffered_entry->set_inline({});
                }
                done = callback(std::move(*buffered_entry), user_data);
                entry_issued = true;
                buffered_entry.reset();
            }
            return done;
        }
    } state{.entry = entry,
            .callback = callback,
            .user_data = user_data,
            .buffered_entry = std::nullopt,
            .exception = nullptr};

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
                    auto demangled_ptr = unique_freeable<char[]>{}; // NOLINT(hicpp-avoid-c-arrays)
                    auto symbol_view = std::string_view{};
                    if (function) {
                        auto demangle_status = 0;
                        demangled_ptr.reset(__cxxabiv1::__cxa_demangle(function, nullptr, nullptr, &demangle_status));
                        switch (demangle_status) {
                            case 0: // The demangling operation succeeded.
                                symbol_view = demangled_ptr.get();
                                break;
                            case -1: // A memory allocation failure occurred.
                                state.exception = std::make_exception_ptr(std::bad_alloc{});
                                return 1;
                            default:
                                // -2: mangled_name is not a valid name under the C++ ABI mangling rules.
                                // -3: One of the arguments is invalid.
                                symbol_view = function;
                                break;
                        }
                    }
                    state.buffered_entry = logical_stacktrace_entry{
                            {},
                            state.entry,
                            false,
                            std::string{symbol_view},
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
