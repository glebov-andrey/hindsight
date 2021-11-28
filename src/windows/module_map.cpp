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

#include "module_map.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <limits>
#include <memory>
#include <system_error>
#include <thread>
#include <utility>

#include <Psapi.h>

namespace hindsight::windows {

namespace {

struct close_module_handle {
    using pointer = HMODULE;

    auto operator()(HMODULE handle) const noexcept -> void {
        [[maybe_unused]] const auto success = FreeLibrary(handle);
        assert(success);
    }
};

using unique_module_handle = std::unique_ptr<HMODULE, close_module_handle>;

[[nodiscard]] auto get_local_module_handle(const stacktrace_entry entry) noexcept -> unique_module_handle {
    auto module_handle = HMODULE{}; // NOLINT(readability-qualified-auto): HMODULE is a handle
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                       // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
                       reinterpret_cast<LPCWSTR>(entry.native_handle()),
                       &module_handle);
    return unique_module_handle{module_handle};
}

auto get_remote_module_handles(const HANDLE process) -> std::optional<std::vector<HMODULE>> {
    constexpr auto initial_capacity = std::size_t{64};
    auto modules = std::vector<HMODULE>(initial_capacity);
    const auto max_module_count = std::min(std::numeric_limits<DWORD>::max() / sizeof(HMODULE), modules.max_size());
    while (true) {
        modules.resize(std::min(modules.capacity(), max_module_count)); // resize to capacity but not over max
        const auto available_size_bytes = static_cast<DWORD>(modules.size() * sizeof(HMODULE));
        auto needed_size_bytes = DWORD{};
        if (!EnumProcessModules(process, modules.data(), available_size_bytes, &needed_size_bytes)) {
            return std::nullopt;
        }
        assert(needed_size_bytes % sizeof(HMODULE) == 0);
        const auto needed_module_count = needed_size_bytes / sizeof(HMODULE);
        if (needed_module_count > modules.size()) {
            const auto can_grow = needed_module_count <= max_module_count;
            if (!can_grow) {
                return std::nullopt;
            }
        }
        modules.resize(needed_module_count);
        if (needed_module_count <= modules.size()) {
            break;
        }
    }
    if (modules.empty()) { // this should never happen and thus considered an error
        return std::nullopt;
    }
    return std::move(modules);
}

struct basic_module_info {
    std::uintptr_t offset;
    std::uint32_t size;
};

[[nodiscard]] auto get_basic_module_info(const HANDLE process, const HMODULE module)
        -> std::optional<basic_module_info> {
    MODULEINFO info;
    if (!GetModuleInformation(process ? process : GetCurrentProcess(), module, &info, sizeof info)) {
        return std::nullopt;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): we only use the numeric value from here onwards
    return basic_module_info{.offset = reinterpret_cast<std::uintptr_t>(info.lpBaseOfDll), .size = info.SizeOfImage};
}

auto get_module_file_name(const HANDLE process, const HMODULE module) -> std::optional<std::wstring> {
    auto path = std::wstring(MAX_PATH, wchar_t{}); // the path will most likely fit in MAX_PATH characters
    const auto max_path_size = std::min(std::size_t{std::numeric_limits<DWORD>::max() - 1}, path.max_size());
    while (true) {
        path.resize(std::min(path.capacity(), max_path_size)); // resize to capacity but not over max
        const auto path_buffer_size_with_null = static_cast<DWORD>(path.size() + 1);
        const auto filled_chars =
                process ? GetModuleFileNameExW(process, module, path.data(), path_buffer_size_with_null)
                        : GetModuleFileNameW(module, path.data(), path_buffer_size_with_null);
        assert(filled_chars <= path_buffer_size_with_null);
        if (filled_chars == 0) {
            return std::nullopt;
        }
        if (filled_chars == path_buffer_size_with_null) { // the path did not fit and was truncated
            const auto can_grow = path.size() < max_path_size;
            if (!can_grow) {
                return std::nullopt;
            }
            const auto can_double_size = max_path_size - path.size() >= path.size();
            const auto new_path_size = can_double_size ? path.size() * 2 : max_path_size;
            path.resize(new_path_size);
            continue;
        }
        path.resize(filled_chars);
        break;
    }
    return path;
}

} // namespace

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
auto local_module_map::lookup(const stacktrace_entry entry) const -> std::optional<module_info> {
    const auto module = get_local_module_handle(entry);
    if (!module) {
        return std::nullopt;
    }
    const auto info = get_basic_module_info(nullptr, module.get());
    if (!info) {
        return std::nullopt;
    }
    auto file_name = get_module_file_name(nullptr, module.get());
    if (!file_name) {
        return std::nullopt;
    }
    return module_info{.base_offset = info->offset, .file_name = std::move(*file_name)};
}

namespace {

// Enumerating modules from another process and querying their info is unreliable if that process is currently
// loading or unloading modules. The Win32 functions either fail or return incorrect information in such cases.
// To work around this we retry our lookup in case any step fails.

using retry_count_t = std::chrono::milliseconds::rep;
constexpr auto lookup_retry_count = retry_count_t{10};

auto wait_before_retry(const retry_count_t retry_idx) {
    using namespace std::chrono_literals;
    constexpr auto wait_step = 10ms;
    constexpr auto max_wait = 100ms;
    switch (retry_idx) {
        case 0:
            std::this_thread::yield();
            break;
        case 1:
            std::this_thread::sleep_for(1ms);
            break;
        case 2:
            std::this_thread::sleep_for(wait_step);
            break;
        default:
            std::this_thread::sleep_for(std::min(wait_step * (retry_idx - 2), max_wait));
            break;
    }
}

} // namespace

auto remote_module_map::lookup(const stacktrace_entry entry) const -> std::optional<module_info> {
    for (auto retry_idx = retry_count_t{}; retry_idx != lookup_retry_count; ++retry_idx) {
        auto modules = get_remote_module_handles(m_process.get());
        if (!modules) {
            wait_before_retry(retry_idx);
            continue;
        }
        const auto module_entry = [&]() -> std::optional<std::pair<HMODULE, basic_module_info>> {
            for (const auto module : *modules) { // NOLINT(readability-qualified-auto): HMODULE is a handle
                const auto info = get_basic_module_info(m_process.get(), module);
                if (!info) {
                    return std::nullopt; // an error occurred
                }
                if (entry.native_handle() >= info->offset && entry.native_handle() - info->offset < info->size) {
                    return std::pair{module, *info};
                }
            }
            return std::pair{HMODULE{}, basic_module_info{}}; // entry not found, not an error
        }();
        if (!module_entry) {
            wait_before_retry(retry_idx);
            continue;
        }
        const auto &[module, info] = *module_entry;
        if (!module) {
            break;
        }
        auto file_name = get_module_file_name(m_process.get(), module);
        if (!file_name) {
            wait_before_retry(retry_idx);
            continue;
        }
        return module_info{.base_offset = info.offset, .file_name = std::move(*file_name)};
    }
    return std::nullopt;
}

} // namespace hindsight::windows
