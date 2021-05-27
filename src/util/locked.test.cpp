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

#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

#include <catch2/catch.hpp>

#include "locked.hpp"

namespace hindsight::util {

TEST_CASE("util: standard library lock types satisfy basic_lockable") {
    STATIC_REQUIRE(basic_lockable<std::mutex>);
    STATIC_REQUIRE(basic_lockable<std::timed_mutex>);
    STATIC_REQUIRE(basic_lockable<std::recursive_mutex>);
    STATIC_REQUIRE(basic_lockable<std::recursive_timed_mutex>);
    STATIC_REQUIRE(basic_lockable<std::shared_mutex>);
    STATIC_REQUIRE(basic_lockable<std::shared_timed_mutex>);
}

TEST_CASE("util: a nothrow-lockable lock type satisfies nothrow_basic_lockable") {
    struct nothrow_lockable_type {
        void lock() noexcept;
        void unlock();
    };
    STATIC_REQUIRE(nothrow_basic_lockable<nothrow_lockable_type>);
}


TEST_CASE("util: locked<T> is constructible from T's constructor parameters") {
    STATIC_REQUIRE(std::constructible_from<locked<int>>);
    STATIC_REQUIRE(std::constructible_from<locked<int>, int>);
    STATIC_REQUIRE(std::constructible_from<locked<std::string>>);
    STATIC_REQUIRE(std::constructible_from<locked<std::string>, const char *>);
    STATIC_REQUIRE(!std::constructible_from<locked<std::string>, std::vector<int>>);
}

} // namespace hindsight::util
