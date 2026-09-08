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

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <hindsight/resolver.hpp>
#include <hindsight/stacktrace.hpp>

#include <hindsight/detail/config.hpp>

#ifdef HINDSIGHT_OS_WINDOWS
    #include <objbase.h>

    #include "util/finally.hpp"
#endif

namespace hindsight {

TEST_CASE("A resolver is default-constructible and immovable") {
    STATIC_REQUIRE(std::default_initializable<resolver>);

    STATIC_REQUIRE(!std::is_copy_constructible_v<resolver>);
    STATIC_REQUIRE(!std::is_move_constructible_v<resolver>);
    STATIC_REQUIRE(!std::is_copy_assignable_v<resolver>);
    STATIC_REQUIRE(!std::is_move_assignable_v<resolver>);
}

namespace {

auto resolve_and_check(resolver &r, const stacktrace_entry physical) {
    auto resolved = std::vector<logical_stacktrace_entry>{};
    r.resolve(physical, std::back_inserter(resolved), std::unreachable_sentinel);
    REQUIRE(!resolved.empty());
    REQUIRE(std::ranges::all_of(resolved, [&](const auto &entry) { return entry.physical == physical; }));
}

} // namespace

TEST_CASE("A default-constructed resolver can be used") {
    const auto trace = capture_stacktrace();

    auto r = resolver{};
    resolve_and_check(r, trace.front());
}

TEST_CASE("A resolver can be used from multiple threads") {
    static constexpr std::size_t thread_count = 4;
    static constexpr std::size_t resolve_count = 100;

    auto r = resolver{};

    const auto trace = capture_stacktrace();
    auto ref_resolved = std::vector<logical_stacktrace_entry>{};
    ref_resolved.reserve(trace.size());
    for (const auto entry : trace) {
        r.resolve(entry, std::back_inserter(ref_resolved), std::unreachable_sentinel);
    }

    auto threads = std::array<std::jthread, thread_count>{};
    auto valid_results_per_thread = std::array<std::size_t, thread_count>{};

    for (const auto thread_idx : std::views::iota(std::size_t{0}, thread_count)) {
        threads[thread_idx] = std::jthread{[&, thread_idx] {
            auto local_resolved = std::vector<logical_stacktrace_entry>{};
            local_resolved.reserve(trace.size());

            for ([[maybe_unused]] const auto i : std::views::iota(std::size_t{0}, resolve_count)) {
                local_resolved.clear();

                for (const auto entry : trace) {
                    r.resolve(entry, std::back_inserter(local_resolved), std::unreachable_sentinel);
                }
                if (local_resolved == ref_resolved) {
                    ++valid_results_per_thread[thread_idx];
                }
            }
        }};
    }
    for (auto &thread : threads) {
        thread.join();
    }
    REQUIRE(std::ranges::all_of(valid_results_per_thread,
                                [](const std::size_t count) { return count == resolve_count; }));
}

#ifdef HINDSIGHT_OS_WINDOWS

// The resolver on Windows uses DIA SDK which has a COM-like API, and we use the API without initializing COM.
// There is however some concern that NoRegCoCreate may initialize COM internally.
// This test ensures that's not the case, and that we behave nicely in threads which themselves use COM.
TEST_CASE("The Windows resolver implementation doesn't initialize COM") {
    constexpr auto do_uninitialize_com = []() noexcept { CoUninitialize(); };
    {
        const auto pre_test_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const auto com_init_guard = util::finally{do_uninitialize_com};
        REQUIRE(pre_test_result != S_FALSE);
        REQUIRE(pre_test_result != RPC_E_CHANGED_MODE);
        REQUIRE(pre_test_result == S_OK);
    }
    auto r = resolver{};
    {
        const auto post_resolver_ctor_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const auto com_init_guard = util::finally{do_uninitialize_com};
        REQUIRE(post_resolver_ctor_result != S_FALSE);
        REQUIRE(post_resolver_ctor_result != RPC_E_CHANGED_MODE);
        REQUIRE(post_resolver_ctor_result == S_OK);
    }
    const auto trace = capture_stacktrace();
    resolve_and_check(r, trace.front());
    {
        const auto post_resolver_resolve_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const auto com_init_guard = util::finally{do_uninitialize_com};
        REQUIRE(post_resolver_resolve_result != S_FALSE);
        REQUIRE(post_resolver_resolve_result != RPC_E_CHANGED_MODE);
        REQUIRE(post_resolver_resolve_result == S_OK);
    }
}

#endif

} // namespace hindsight
