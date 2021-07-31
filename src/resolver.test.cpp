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
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>

#include <catch2/catch.hpp>

#include <hindsight/resolver.hpp>
#include <hindsight/stacktrace.hpp>

namespace hindsight {

TEST_CASE("A resolver is default-constructible and nothrow-move-constructible/-assignable") {
    STATIC_REQUIRE(std::default_initializable<resolver>);
    STATIC_REQUIRE(std::is_nothrow_move_constructible_v<resolver>);
    STATIC_REQUIRE(std::is_nothrow_move_assignable_v<resolver>);

    STATIC_REQUIRE(!std::copy_constructible<resolver>);
    STATIC_REQUIRE(!std::is_copy_assignable_v<resolver>);
}

namespace {

auto resolve_and_check(resolver &r, const stacktrace_entry physical) {
    auto resolved = std::vector<logical_stacktrace_entry>{};
    r.resolve(physical, std::back_inserter(resolved), std::unreachable_sentinel);
    REQUIRE(!resolved.empty());
    REQUIRE(std::ranges::all_of(resolved, [&](const auto &entry) { return entry.physical() == physical; }));
}

} // namespace

TEST_CASE("A default-constructed resolver can be used") {
    const auto trace = capture_stacktrace();
    REQUIRE(!trace.empty());

    auto r = resolver{};
    resolve_and_check(r, trace.front());
}

TEST_CASE("Both moved-from and moved-to resolvers can be used") {
    const auto trace = capture_stacktrace();
    REQUIRE(!trace.empty());

    auto moved_from = resolver{};
    auto moved_to = std::move(moved_from);

    resolve_and_check(moved_from, trace.front());
    resolve_and_check(moved_to, trace.front());
}

} // namespace hindsight
