/*
 * Copyright 2025 Andrey Glebov
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

#ifndef HINDSIGHT_INCLUDE_HINDSIGHT_LOGICAL_STACKTRACE_ENTRY_HPP
#define HINDSIGHT_INCLUDE_HINDSIGHT_LOGICAL_STACKTRACE_ENTRY_HPP

#include <hindsight/detail/config.hpp>

#include <cstdint>
#include <filesystem>
#include <string>

#include <hindsight/stacktrace_entry.hpp>

namespace hindsight {

struct HINDSIGHT_API logical_stacktrace_entry {
    stacktrace_entry physical{};
    std::filesystem::path physical_module{};

    std::string symbol{};
    std::string file_name{};
    std::uint_least32_t line_number{};
    std::uint_least32_t column_number{};

    bool is_inline{};
};

} // namespace hindsight

#endif // HINDSIGHT_INCLUDE_HINDSIGHT_LOGICAL_STACKTRACE_ENTRY_HPP
