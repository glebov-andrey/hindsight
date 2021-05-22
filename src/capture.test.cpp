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

#include <algorithm>
#include <concepts>
#include <forward_list>
#include <iterator>
#include <ranges>
#include <span>
#include <vector>

#include <catch2/catch.hpp>

#include <hindsight/capture.hpp>

#ifdef HINDSIGHT_OS_WINDOWS
    #include <Windows.h>
#else
    #include <ucontext.h>
#endif

namespace hindsight {

// If the frame from witch RtlCaptureContext/getcontext is called is not strictly a parent of the frame from witch
// capture_stacktrace_from_* is called then the trace will be incorrect (include the current location of the call to
// capture_stacktrace_from_*). As a result we must make sure that RtlCaptureContext/getcontext is called inline.
#ifdef HINDSIGHT_OS_WINDOWS
    #define HINDSIGHT_TESTS_GET_CONTEXT(context) RtlCaptureContext(&(context))
#else
    #define HINDSIGHT_TESTS_GET_CONTEXT(context) getcontext(&(context))
#endif

namespace {

struct easily_reachable_sentinel_t {
    [[nodiscard]] friend constexpr auto operator==(easily_reachable_sentinel_t /* sentinel */,
                                                   const std::weakly_incrementable auto & /* it */) noexcept -> bool {
        return true;
    }
};

constexpr auto easily_reachable_sentinel = easily_reachable_sentinel_t{};

#if defined __clang__ && defined __GLIBCXX__
    // These bugs are related: https://bugs.llvm.org/show_bug.cgi?id=47509,
    // https://bugs.llvm.org/show_bug.cgi?id=46746, https://gcc.gnu.org/bugzilla/show_bug.cgi?id=97120.
    #pragma message "Standard range views are broken with Clang and libstdc++. Disabling some tests."
    #define HINDSIGHT_TESTS_DISABLE_RANGE_VIEW_TESTS
#endif

} // namespace

TEST_CASE("capture_stacktrace_from_mutable_context captures at least one entry for a local context") {
    native_context_type context;
    HINDSIGHT_TESTS_GET_CONTEXT(context);
    auto entries = std::vector<stacktrace_entry>{};
    STATIC_REQUIRE(std::same_as<decltype(capture_stacktrace_from_mutable_context(context,
                                                                                 std::back_inserter(entries),
                                                                                 std::unreachable_sentinel)),
                                void>);
    capture_stacktrace_from_mutable_context(context, std::back_inserter(entries), std::unreachable_sentinel);
    REQUIRE(!entries.empty());
    REQUIRE(std::ranges::all_of(entries, [](const auto entry) { return entry != stacktrace_entry{}; }));
}

TEST_CASE("capture_stacktrace_from_mutable_context skips entries_to_skip first entries") {
    native_context_type context;
    HINDSIGHT_TESTS_GET_CONTEXT(context);
    auto all_entries = std::vector<stacktrace_entry>{};
    {
        auto tmp_context = context;
        capture_stacktrace_from_mutable_context(tmp_context,
                                                std::back_inserter(all_entries),
                                                std::unreachable_sentinel);
    }
    auto less_entries = std::vector<stacktrace_entry>{};
    capture_stacktrace_from_mutable_context(context, std::back_inserter(less_entries), std::unreachable_sentinel, 1);
    REQUIRE(less_entries.size() == all_entries.size() - 1);
    REQUIRE(std::ranges::equal(less_entries, std::span{all_entries}.subspan<1>()));
}

TEST_CASE("capture_stacktrace_from_mutable_context can capture into an empty output range (iterator + sentinel)") {
    native_context_type context;
    HINDSIGHT_TESTS_GET_CONTEXT(context);
    auto entries = std::vector<stacktrace_entry>{};
    STATIC_REQUIRE(std::same_as<decltype(capture_stacktrace_from_mutable_context(context,
                                                                                 std::back_inserter(entries),
                                                                                 easily_reachable_sentinel)),
                                void>);
    capture_stacktrace_from_mutable_context(context, std::back_inserter(entries), easily_reachable_sentinel);
    REQUIRE(entries.empty());
}

TEST_CASE("capture_stacktrace_from_mutable_context can capture into an empty forward range (iterator + sentinel)") {
    native_context_type context;
    HINDSIGHT_TESTS_GET_CONTEXT(context);
    auto entries = std::forward_list<stacktrace_entry>{};
    STATIC_REQUIRE(
            std::same_as<decltype(capture_stacktrace_from_mutable_context(context, entries.begin(), entries.end())),
                         decltype(entries.begin())>);
    const auto last_captured = capture_stacktrace_from_mutable_context(context, entries.begin(), entries.end());
    REQUIRE(last_captured == entries.begin());
    REQUIRE(last_captured == entries.end());
}

TEST_CASE("capture_stacktrace_from_mutable_context stops capturing when the range is full") {
    native_context_type context;
    HINDSIGHT_TESTS_GET_CONTEXT(context);
    auto all_entries = std::vector<stacktrace_entry>{};
    {
        auto tmp_context = context;
        capture_stacktrace_from_mutable_context(tmp_context,
                                                std::back_inserter(all_entries),
                                                std::unreachable_sentinel);
    }
    REQUIRE(!all_entries.empty());
    if (all_entries.size() == 1) {
        WARN("Can't check that capture_stacktrace_from_mutable_context stops capturing when the range is full because "
             "the total entry count is 1");
        return;
    }
    auto less_entries = std::vector<stacktrace_entry>(1);
    const auto last_captured =
            capture_stacktrace_from_mutable_context(context, less_entries.begin(), less_entries.end());
    REQUIRE(last_captured == less_entries.end());
}

TEST_CASE("capture_stacktrace_from_mutable_context captures entries into ranges (range)") {
    native_context_type context;
    HINDSIGHT_TESTS_GET_CONTEXT(context);

#ifndef HINDSIGHT_TESTS_DISABLE_RANGE_VIEW_TESTS
    auto all_entries = std::vector<stacktrace_entry>{};
    {
        auto tmp_context = context;
        const auto out_range = std::ranges::subrange{std::back_inserter(all_entries), std::unreachable_sentinel};
        STATIC_REQUIRE(std::same_as<decltype(capture_stacktrace_from_mutable_context(tmp_context, out_range)), void>);
        capture_stacktrace_from_mutable_context(tmp_context, out_range);
        REQUIRE(!all_entries.empty());
        REQUIRE(std::ranges::all_of(all_entries, [](const auto entry) { return entry != stacktrace_entry{}; }));
    }

    {
        auto tmp_context = context;
        auto entries = std::forward_list<stacktrace_entry>(all_entries.size());
        STATIC_REQUIRE(std::same_as<decltype(capture_stacktrace_from_mutable_context(tmp_context, entries)),
                                    std::ranges::subrange<decltype(entries)::iterator>>);
        const auto captured_entries = capture_stacktrace_from_mutable_context(tmp_context, entries);
        REQUIRE(captured_entries.begin() == entries.begin());
        REQUIRE(std::ranges::equal(captured_entries, all_entries));
    }
#endif

    {
        auto tmp_context = context;
        using list_t = std::forward_list<stacktrace_entry>;
        STATIC_REQUIRE(std::same_as<decltype(capture_stacktrace_from_mutable_context(tmp_context, list_t(16))),
                                    std::ranges::dangling>);
        [[maybe_unused]] const auto dangling = capture_stacktrace_from_mutable_context(tmp_context, list_t(16));
    }
}

TEST_CASE("capture_stacktrace captures at least one entry") {
    auto entries = std::vector<stacktrace_entry>{};
    STATIC_REQUIRE(
            std::same_as<decltype(capture_stacktrace(std::back_inserter(entries), std::unreachable_sentinel)), void>);
    capture_stacktrace(std::back_inserter(entries), std::unreachable_sentinel);
    REQUIRE(!entries.empty());
    REQUIRE(std::ranges::all_of(entries, [](const auto entry) { return entry != stacktrace_entry{}; }));
}

TEST_CASE("capture_stacktrace skips entries_to_skip first entries") {
    auto all_entries = std::vector<stacktrace_entry>{};
    capture_stacktrace(std::back_inserter(all_entries), std::unreachable_sentinel);
    auto less_entries = std::vector<stacktrace_entry>{};
    capture_stacktrace(std::back_inserter(less_entries), std::unreachable_sentinel, 1);
    REQUIRE(less_entries.size() == all_entries.size() - 1);
}

TEST_CASE("capture_stacktrace can capture into an empty output range (iterator + sentinel)") {
    auto entries = std::vector<stacktrace_entry>{};
    STATIC_REQUIRE(
            std::same_as<decltype(capture_stacktrace(std::back_inserter(entries), easily_reachable_sentinel)), void>);
    capture_stacktrace(std::back_inserter(entries), easily_reachable_sentinel);
    REQUIRE(entries.empty());
}

TEST_CASE("capture_stacktrace can capture into an empty forward range (iterator + sentinel)") {
    auto entries = std::forward_list<stacktrace_entry>{};
    STATIC_REQUIRE(
            std::same_as<decltype(capture_stacktrace(entries.begin(), entries.end())), decltype(entries.begin())>);
    const auto last_captured = capture_stacktrace(entries.begin(), entries.end());
    REQUIRE(last_captured == entries.begin());
    REQUIRE(last_captured == entries.end());
}

TEST_CASE("capture_stacktrace stops capturing when the range is full") {
    auto all_entries = std::vector<stacktrace_entry>{};
    capture_stacktrace(std::back_inserter(all_entries), std::unreachable_sentinel);
    REQUIRE(!all_entries.empty());
    if (all_entries.size() == 1) {
        WARN("Can't check that capture_stacktrace stops capturing when the range is full because the total entry count "
             "is 1");
        return;
    }
    auto less_entries = std::vector<stacktrace_entry>(1);
    const auto last_captured = capture_stacktrace(less_entries.begin(), less_entries.end());
    REQUIRE(last_captured == less_entries.end());
}

TEST_CASE("capture_stacktrace captures entries info ranges (range)") {
#ifndef HINDSIGHT_TESTS_DISABLE_RANGE_VIEW_TESTS
    {
        auto entries = std::vector<stacktrace_entry>{};
        const auto out_range = std::ranges::subrange{std::back_inserter(entries), std::unreachable_sentinel};
        STATIC_REQUIRE(std::same_as<decltype(capture_stacktrace(out_range)), void>);
        capture_stacktrace(out_range);
        REQUIRE(!entries.empty());
        REQUIRE(std::ranges::all_of(entries, [](const auto entry) { return entry != stacktrace_entry{}; }));
    }

    {
        auto entries = std::forward_list<stacktrace_entry>(16);
        STATIC_REQUIRE(std::same_as<decltype(capture_stacktrace(entries)),
                                    std::ranges::subrange<decltype(entries)::iterator>>);
        const auto captured_entries = capture_stacktrace(entries);
        REQUIRE(captured_entries.begin() == entries.begin());
        REQUIRE(!captured_entries.empty());
    }
#endif

    {
        using list_t = std::forward_list<stacktrace_entry>;
        STATIC_REQUIRE(std::same_as<decltype(capture_stacktrace(list_t(16))), std::ranges::dangling>);
        [[maybe_unused]] const auto dangling = capture_stacktrace(list_t(16));
    }
}

TEST_CASE("capture_stacktrace_from_context captures at least one entry for a local context") {
    native_context_type context;
    HINDSIGHT_TESTS_GET_CONTEXT(context);
    auto entries = std::vector<stacktrace_entry>{};
    STATIC_REQUIRE(std::same_as<decltype(capture_stacktrace_from_context(context,
                                                                         std::back_inserter(entries),
                                                                         std::unreachable_sentinel)),
                                void>);
    capture_stacktrace_from_context(context, std::back_inserter(entries), std::unreachable_sentinel);
    REQUIRE(!entries.empty());
    REQUIRE(std::ranges::all_of(entries, [](const auto entry) { return entry != stacktrace_entry{}; }));
}

TEST_CASE("capture_stacktrace_from_context skips entries_to_skip first entries") {
    native_context_type context;
    HINDSIGHT_TESTS_GET_CONTEXT(context);
    auto all_entries = std::vector<stacktrace_entry>{};
    capture_stacktrace_from_context(context, std::back_inserter(all_entries), std::unreachable_sentinel);
    auto less_entries = std::vector<stacktrace_entry>{};
    capture_stacktrace_from_context(context, std::back_inserter(less_entries), std::unreachable_sentinel, 1);
    REQUIRE(less_entries.size() == all_entries.size() - 1);
    REQUIRE(std::ranges::equal(less_entries, std::span{all_entries}.subspan<1>()));
}

TEST_CASE("capture_stacktrace_from_context can capture into an empty output range (iterator + sentinel)") {
    native_context_type context;
    HINDSIGHT_TESTS_GET_CONTEXT(context);
    auto entries = std::vector<stacktrace_entry>{};
    STATIC_REQUIRE(std::same_as<decltype(capture_stacktrace_from_context(context,
                                                                         std::back_inserter(entries),
                                                                         easily_reachable_sentinel)),
                                void>);
    capture_stacktrace_from_context(context, std::back_inserter(entries), easily_reachable_sentinel);
    REQUIRE(entries.empty());
}

TEST_CASE("capture_stacktrace_from_context can capture into an empty forward range (iterator + sentinel)") {
    native_context_type context;
    HINDSIGHT_TESTS_GET_CONTEXT(context);
    auto entries = std::forward_list<stacktrace_entry>{};
    STATIC_REQUIRE(std::same_as<decltype(capture_stacktrace_from_context(context, entries.begin(), entries.end())),
                                decltype(entries.begin())>);
    const auto last_captured = capture_stacktrace_from_context(context, entries.begin(), entries.end());
    REQUIRE(last_captured == entries.begin());
    REQUIRE(last_captured == entries.end());
}

TEST_CASE("capture_stacktrace_from_context stops capturing when the range is full") {
    native_context_type context;
    HINDSIGHT_TESTS_GET_CONTEXT(context);
    auto all_entries = std::vector<stacktrace_entry>{};
    capture_stacktrace_from_context(context, std::back_inserter(all_entries), std::unreachable_sentinel);
    REQUIRE(!all_entries.empty());
    if (all_entries.size() == 1) {
        WARN("Can't check that capture_stacktrace_from_context stops capturing when the range is full because the "
             "total entry count is 1");
        return;
    }
    auto less_entries = std::vector<stacktrace_entry>(1);
    const auto last_captured = capture_stacktrace_from_context(context, less_entries.begin(), less_entries.end());
    REQUIRE(last_captured == less_entries.end());
}

TEST_CASE("capture_stacktrace_from_context captures entries info ranges (range)") {
    native_context_type context;
    HINDSIGHT_TESTS_GET_CONTEXT(context);

#ifndef HINDSIGHT_TESTS_DISABLE_RANGE_VIEW_TESTS
    auto all_entries = std::vector<stacktrace_entry>{};
    {
        const auto out_range = std::ranges::subrange{std::back_inserter(all_entries), std::unreachable_sentinel};
        STATIC_REQUIRE(std::same_as<decltype(capture_stacktrace_from_context(context, out_range)), void>);
        capture_stacktrace_from_context(context, out_range);
        REQUIRE(!all_entries.empty());
        REQUIRE(std::ranges::all_of(all_entries, [](const auto entry) { return entry != stacktrace_entry{}; }));
    }

    {
        auto entries = std::forward_list<stacktrace_entry>(all_entries.size());
        STATIC_REQUIRE(std::same_as<decltype(capture_stacktrace_from_context(context, entries)),
                                    std::ranges::subrange<decltype(entries)::iterator>>);
        const auto captured_entries = capture_stacktrace_from_context(context, entries);
        REQUIRE(captured_entries.begin() == entries.begin());
        REQUIRE(std::ranges::equal(captured_entries, all_entries));
    }
#endif

    {
        using list_t = std::forward_list<stacktrace_entry>;
        STATIC_REQUIRE(
                std::same_as<decltype(capture_stacktrace_from_context(context, list_t(16))), std::ranges::dangling>);
        [[maybe_unused]] const auto dangling = capture_stacktrace_from_context(context, list_t(16));
    }
}

} // namespace hindsight
