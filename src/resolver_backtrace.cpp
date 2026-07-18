/*
 * Copyright 2026 Andrey Glebov
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

#include <hindsight/resolver.hpp>

#include <cerrno>
#include <exception>
#include <new>
#include <optional>
#include <string_view>

#include <dlfcn.h>
#include <unistd.h> // close

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


auto demangle_and_encode_symbol(const char *const raw_symbol) -> std::string {
    if (raw_symbol == nullptr || std::char_traits<char>::length(raw_symbol) == 0) {
        return {};
    }
    const auto demangled = itanium_abi::demangle(raw_symbol);
    const auto unencoded = demangled ? std::string_view{demangled.get()} : std::string_view{raw_symbol};
    if (unencoded.empty()) {
        return {};
    }
    return unix::transcode(unix::get_utf8_sanitizer(), unencoded);
}

auto encode_file_name(const char *const raw_file_name) -> std::string {
    if (raw_file_name == nullptr) {
        return {};
    }
    const auto raw_file_name_sv = std::string_view{raw_file_name};
    if (raw_file_name_sv.empty()) {
        return {};
    }
    return unix::transcode(unix::get_utf8_sanitizer(), raw_file_name_sv);
}

} // namespace

class resolver::impl {};

resolver::resolver() = default;

#ifdef HINDSIGHT_OS_LINUX
resolver::resolver(from_proc_maps_t /*from_proc_maps_tag*/, const int proc_maps_descriptor) {
    ::close(proc_maps_descriptor); // close descriptor on error
    throw std::runtime_error{"Resolving based on /proc/pid/maps is not supported by the libbacktrace backend"};
}
#endif

resolver::~resolver() = default;

auto resolver::resolve_impl(const stacktrace_entry entry, const resolve_cb callback) -> void {
    struct cb_state {
        const stacktrace_entry entry;
        const resolve_cb callback;
        std::filesystem::path physical_module{};
        std::optional<logical_stacktrace_entry> buffered_entry = std::nullopt;
        std::exception_ptr exception = nullptr;

        bool entry_issued = false;
        bool done = false;

        auto flush_buffered_entry(const bool is_inline) {
            if (buffered_entry && !done) {
                if (is_inline) {
                    buffered_entry->is_inline = true;
                }
                done = callback(std::move(*buffered_entry));
                entry_issued = true;
                buffered_entry.reset();
            }
            return done;
        }
    } state{.entry = entry, .callback = callback};

    auto dl_info = Dl_info{};
    if (dladdr(reinterpret_cast<void *>(entry.native_handle()), &dl_info)) {
        state.physical_module = dl_info.dli_fname;
    }

    const auto on_failure = [&] {
        callback(logical_stacktrace_entry{.physical = entry, .physical_module = std::move(state.physical_module)});
    };

    auto *const global_state = get_backtrace_state();
    if (!global_state) {
        on_failure();
        return;
    }

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
                    state.buffered_entry =
                            logical_stacktrace_entry{.physical = state.entry,
                                                     .physical_module = state.physical_module,
                                                     .symbol = demangle_and_encode_symbol(function),
                                                     .file_name = encode_file_name(filename),
                                                     .line_number = static_cast<std::uint_least32_t>(lineno),
                                                     .column_number = 0,
                                                     .is_inline = false};
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
