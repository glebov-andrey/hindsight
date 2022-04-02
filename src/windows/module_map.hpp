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

#ifndef HINDSIGHT_SRC_WINDOWS_MODULE_MAP_HPP
#define HINDSIGHT_SRC_WINDOWS_MODULE_MAP_HPP

#include <hindsight/detail/config.hpp>

#ifdef HINDSIGHT_OS_WINDOWS

    #include <cassert>
    #include <cstdint>
    #include <memory>
    #include <optional>
    #include <string>

    #include <Windows.h>

    #include <hindsight/stacktrace_entry.hpp>

namespace hindsight::windows {

struct close_handle {
    using pointer = HANDLE;

    auto operator()(const HANDLE handle) const noexcept -> void {
        [[maybe_unused]] const auto success = CloseHandle(handle);
        assert(success);
    }
};

using unique_process_handle = std::unique_ptr<HANDLE, close_handle>;


struct module_info {
    std::uintptr_t base_offset;
    std::wstring file_name;
};

class local_module_map {
public:
    [[nodiscard]] auto lookup(stacktrace_entry entry) const -> std::optional<module_info>;
};

class remote_module_map {
public:
    [[nodiscard]] explicit remote_module_map(unique_process_handle process) noexcept : m_process{std::move(process)} {}

    [[nodiscard]] auto lookup(stacktrace_entry entry) const -> std::optional<module_info>;

private:
    unique_process_handle m_process;
};

} // namespace hindsight::windows

#endif

#endif // HINDSIGHT_SRC_WINDOWS_MODULE_MAP_HPP
