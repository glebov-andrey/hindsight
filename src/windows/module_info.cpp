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

#include "module_info.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <limits>
#include <string>
#include <system_error>
#include <utility>

#include <Windows.h>
// Include Windows.h before Psapi.h
#include <Psapi.h>

namespace hindsight::windows {

namespace {

[[nodiscard]] auto get_module_handle_by_address(const std::uintptr_t address) -> HMODULE {
    auto module_handle = HMODULE{};
    // GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT is safe here because we assume that the pointer remains valid, i.e.
    // the module does not get concurrently unloaded.
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(address),
                            &module_handle)) {
        throw std::system_error{static_cast<int>(GetLastError()),
                                std::system_category(),
                                "Failed to get module from address"};
    }
    assert(module_handle);
    return module_handle;
}

[[nodiscard]] auto get_module_path_by_handle(const HMODULE handle) -> std::wstring {
    auto path = std::wstring(MAX_PATH, wchar_t{}); // The path will most likely fit in MAX_PATH characters.
    const auto max_path_size = std::min(std::size_t{std::numeric_limits<DWORD>::max() - 1}, path.max_size());
    while (true) {
        path.resize(std::min(path.capacity(), max_path_size)); // Resize to capacity but not over max_path_size.
        const auto path_buffer_size_with_null = path.size() + 1;
        const auto filled_chars = GetModuleFileNameW(handle, &path[0], static_cast<DWORD>(path_buffer_size_with_null));
        assert(filled_chars <= path_buffer_size_with_null);
        if (!filled_chars) {
            throw std::system_error{static_cast<int>(GetLastError()),
                                    std::system_category(),
                                    "Failed to get module file name"};
        }
        if (filled_chars == path_buffer_size_with_null) { // This means that the path did not fit and was truncated.
            assert(GetLastError() == ERROR_INSUFFICIENT_BUFFER);
            const auto can_grow = path.size() < max_path_size;
            if (!can_grow) {
                // GetLastError() is still ERROR_INSUFFICIENT_BUFFER so that will be in the error message.
                throw std::system_error{static_cast<int>(GetLastError()),
                                        std::system_category(),
                                        "The module file name is too long"};
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

[[nodiscard]] auto get_module_base_by_handle(const HMODULE handle) -> std::uintptr_t {
    MODULEINFO module_info;
    if (!GetModuleInformation(GetCurrentProcess(), handle, &module_info, sizeof module_info)) {
        throw std::system_error{static_cast<int>(GetLastError()),
                                std::system_category(),
                                "Failed to get module information"};
    }
    return reinterpret_cast<std::uintptr_t>(module_info.lpBaseOfDll);
}

} // namespace

[[nodiscard]] auto get_module_info_by_address(const std::uintptr_t address) -> module_info {
    const auto handle = get_module_handle_by_address(address);
    return {.file_path = get_module_path_by_handle(handle), .base_offset = get_module_base_by_handle(handle)};
}

} // namespace hindsight::windows
