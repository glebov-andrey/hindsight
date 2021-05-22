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

#include <cstdint>
#include <optional>
#include <string>

#include <Windows.h>

#include <hindsight/stacktrace_entry.hpp>

namespace hindsight::windows {

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
    [[nodiscard]] explicit remote_module_map(const HANDLE process) noexcept : m_process{process} {}

    [[nodiscard]] auto lookup(stacktrace_entry entry) const -> std::optional<module_info>;

private:
    HANDLE m_process;
};

} // namespace hindsight::windows

#endif // HINDSIGHT_SRC_WINDOWS_MODULE_MAP_HPP
