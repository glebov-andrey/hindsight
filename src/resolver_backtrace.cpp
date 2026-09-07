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

#ifdef HINDSIGHT_OS_LINUX
    #include <unistd.h> // close, readlink

    #include <link.h> // link_map
#endif

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

[[nodiscard]] auto find_module_by_address(const std::uintptr_t address) -> std::filesystem::path {
#ifdef HINDSIGHT_OS_LINUX
    // At least on Linux, Dl_info::dli_fname doesn't provide the full path to the main executable because it's loaded by
    // the kernel, not the loader. It appears that Dl_info::dli_fname for the main executable is just argv[0].

    // From https://sourceware.org/glibc/manual/latest/html_node/Dynamic-Linker-Introspection.html:
    // link_map::l_name:
    //   For the main executable, l_name is "" (the empty string). (The main executable is not loaded by the GNU C
    //   Library, so its file name is not available.) On Linux, the main executable is available as /proc/self/exe.
    auto dl_info = Dl_info{};
    void *link_map_ptr = nullptr;
    if (dladdr1(reinterpret_cast<void *>(address), &dl_info, &link_map_ptr, RTLD_DL_LINKMAP) == 0) {
        return {};
    }
    assert(link_map_ptr);
    const auto &module_link_map = *static_cast<const link_map *>(link_map_ptr);
    if (module_link_map.l_prev == nullptr) { // true for the main executable
        auto path = std::string(255, '\0');
        while (true) {
            const auto bytes_read = readlink("/proc/self/exe", path.data(), path.size());
            if (bytes_read < 0) {
                break; // Fallback to whatever dli_fname provides
            }
            const auto bytes_read_uz = static_cast<std::size_t>(bytes_read);
            if (bytes_read_uz == path.size()) { // truncation occurred, try again
                path.resize((path.size() + 1) * 2 - 1); // double the allocation size exactly
                continue;
            }
            path.resize(bytes_read_uz);
            return {std::move(path)};
        }
    }
    return {dl_info.dli_fname};

#else
    // Fallback to dli_fname if we don't know a better implementation.
    auto dl_info = Dl_info{};
    if (dladdr(reinterpret_cast<void *>(address), &dl_info) == 0) {
        return {};
    }
    return {dl_info.dli_fname};
#endif
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

    state.physical_module = find_module_by_address(entry.native_handle());

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
