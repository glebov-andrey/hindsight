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

#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iterator>
#include <span>
#include <string_view>
#include <vector>

#ifdef HINDSIGHT_OS_WINDOWS
    #include <Windows.h>
#endif

#include <hindsight/resolver.hpp>

#include "common.hpp"

namespace hindsight::out_of_process::watchdog {
namespace {

auto run() -> int try {
    print_log("WATCHDOG: Starting...\n");
#ifdef HINDSIGHT_OS_WINDOWS
    const auto stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
    if (stdin_handle == INVALID_HANDLE_VALUE) {
        throw_last_system_error("Failed to get the standard input handle");
    }
    if (stdin_handle == nullptr) {
        throw_runtime_error("The process does not have a standard input handle");
    }
    auto host_handle = HANDLE{};
    if (!read_from_handle(stdin_handle, host_handle)) {
        throw_last_system_error("Failed to read the host process handle from standard input");
    }
    print_log("WATCHDOG: Read the host process handle from standard input ({})\n", host_handle);

    auto host_resolver = resolver{host_handle};
    print_log("WATCHDOG: Created a resolver for the host process\n");

    print_log("WATCHDOG: Started, waiting for a stacktrace on standard input\n");
    auto entry_count = std::size_t{};
    if (!read_from_handle(stdin_handle, entry_count)) {
        throw_last_system_error("Failed to read the entry count from standard input");
    }
    auto entries = std::vector<stacktrace_entry>(entry_count);
    {
        if (!read_from_handle(stdin_handle, std::as_writable_bytes(std::span{entries}))) {
            throw_last_system_error("Failed to read the entries from standard input");
        }
    }
    print_log("WATCHDOG: Read {} host entries from standard input\n", entry_count);

    for (auto entry_idx = std::size_t{}; const auto entry : entries) {
        using namespace std::string_view_literals;
        print_log("{:02}: {}\n"sv, entry_idx, entry);
        ++entry_idx;
        auto logical_entries = std::vector<logical_stacktrace_entry>{};
        host_resolver.resolve(entry, std::back_inserter(logical_entries), std::unreachable_sentinel);
        for (const auto &logical : logical_entries) {
            auto source = logical.source();
            print_log("    {}{} ({}:{})\n"sv,
                      logical.is_inline() ? "[inline] "sv : "         "sv,
                      logical.symbol(),
                      source.file_name,
                      source.line_number);
        }
    }
#else
    #error WATCHDOG is not implemented for this OS
#endif
    return EXIT_SUCCESS;
} catch (const std::exception &e) {
    print_log("WATCHDOG: {}\n", e.what());
    return EXIT_FAILURE;
} catch (...) {
    print_log("WATCHDOG: <unknown exception>\n");
    return EXIT_FAILURE;
}

} // namespace
} // namespace hindsight::out_of_process::watchdog

auto main() -> int { return hindsight::out_of_process::watchdog::run(); }
