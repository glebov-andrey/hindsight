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

#define HINDSIGHT_FROM_EXCEPTION_CHECK_FOR_LEAKS

#include <hindsight/detail/config.hpp>

#include <algorithm>
#include <cstddef>
#include <exception>
#include <span>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <hindsight/from_exception.hpp>
#include <hindsight/stacktrace.hpp>
#include <hindsight/stacktrace_entry.hpp>

namespace hindsight {

TEST_CASE("When no exceptions are active, stacktrace_from_current_exception() returns an empty trace") {
    {
        const auto trace = stacktrace_from_current_exception();
        REQUIRE(trace.empty());
    }
    check_for_exception_stacktrace_leaks();
}

TEST_CASE("When stacktraces from exceptions are disabled, no stacktraces are collected") {
    {
        disable_stacktrace_from_exceptions();

        try {
            throw 42;
        } catch (...) {
            const auto current_trace = stacktrace_from_current_exception();
            REQUIRE(current_trace.empty());
            const auto ex = std::current_exception();
            const auto ex_trace = stacktrace_from_exception(ex);
            REQUIRE(ex_trace.empty());
        }
    }
    check_for_exception_stacktrace_leaks();
}

TEST_CASE("When stacktraces from exceptions are enabled, stacktraces are collected") {
    {
        REQUIRE(enable_stacktrace_from_exceptions());

        try {
            throw 42;
        } catch (...) {
            const auto current_trace = stacktrace_from_current_exception();
            REQUIRE(!current_trace.empty());
            const auto ex = std::current_exception();
            const auto ex_trace = stacktrace_from_exception(ex);
            REQUIRE(std::ranges::equal(ex_trace, current_trace));
        }
    }
    check_for_exception_stacktrace_leaks();
}

TEST_CASE("Stacktraces from exceptions are preserved across rethrows") {
    {
        REQUIRE(enable_stacktrace_from_exceptions());

        auto original_trace = std::span<const stacktrace_entry>{};
        try {
            try {
                throw 42;
            } catch (...) {
                original_trace = stacktrace_from_current_exception();
                REQUIRE(!original_trace.empty());
                throw;
            }
        } catch (...) {
            const auto rethrow_trace = stacktrace_from_current_exception();
            REQUIRE(std::ranges::equal(rethrow_trace, original_trace));
        }
    }
    check_for_exception_stacktrace_leaks();
}

TEST_CASE("Stacktraces from exceptions are preserved across std::current_exception and std::rethrow_exception") {
    {
        REQUIRE(enable_stacktrace_from_exceptions());

        auto original_trace = std::span<const stacktrace_entry>{};
        auto ex = std::exception_ptr{};
        auto rethrow_ex = std::exception_ptr{};
        try {
            throw 42;
        } catch (...) {
            original_trace = stacktrace_from_current_exception();
            REQUIRE(!original_trace.empty());
            ex = std::current_exception();
        }
        try {
            std::rethrow_exception(ex);
        } catch (...) {
            const auto rethrow_trace = stacktrace_from_current_exception();
            REQUIRE(std::ranges::equal(rethrow_trace, original_trace));
            rethrow_ex = std::current_exception();
        }
        const auto rethrow_ex_trace = stacktrace_from_exception(rethrow_ex);
        REQUIRE(std::ranges::equal(rethrow_ex_trace, original_trace));
    }
    check_for_exception_stacktrace_leaks();
}

TEST_CASE("Stacktraces from exceptions are preserved across threads") {
    {
        REQUIRE(enable_stacktrace_from_exceptions());

        auto original_trace = std::vector<stacktrace_entry>{};
        auto ex = std::exception_ptr{};

        std::thread{[&] {
            try {
                throw 42;
            } catch (...) {
                const auto trace = stacktrace_from_current_exception();
                original_trace.assign(trace.begin(), trace.end());
                ex = std::current_exception();
            }
        }}.join();
        REQUIRE(!original_trace.empty());
        REQUIRE(ex);

        const auto ex_trace = stacktrace_from_exception(ex);
        REQUIRE(std::ranges::equal(ex_trace, original_trace));
    }
    check_for_exception_stacktrace_leaks();
}

TEST_CASE("A stacktrace from an exception matches a stacktrace at the throw site") {
    {
        REQUIRE(enable_stacktrace_from_exceptions());

        auto throw_trace = std::vector<stacktrace_entry>{};

        try {
            throw_trace = capture_stacktrace();
            REQUIRE(!throw_trace.empty());
            throw 42;
        } catch (...) {
            const auto ex_trace = stacktrace_from_current_exception();
            REQUIRE(!ex_trace.empty());

            const auto reversed_throw_trace = throw_trace | std::views::reverse;
            const auto reversed_ex_trace = ex_trace | std::views::reverse;
            const auto [mismatch_throw, mismatch_ex] = std::ranges::mismatch(reversed_throw_trace, reversed_ex_trace);
            const auto mismatch_pos =
                    static_cast<std::size_t>(std::ranges::distance(reversed_throw_trace.begin(), mismatch_throw));
#ifdef HINDSIGHT_HAS_NOINLINE
            REQUIRE(mismatch_pos == throw_trace.size() - 1);
#else
            REQUIRE(mismatch_pos != 0);
#endif
        }
    }
    check_for_exception_stacktrace_leaks();
}

TEST_CASE("A default-constructed std::exception_ptr doesn't have an assiciated stacktrace") {
    {
        REQUIRE(enable_stacktrace_from_exceptions());
        const auto ex = std::exception_ptr{};
        REQUIRE(stacktrace_from_exception(ex).empty());
    }
    check_for_exception_stacktrace_leaks();
}

TEST_CASE("The stacktrace associated with an std::exception_ptr follows copy construction") {
    {
        REQUIRE(enable_stacktrace_from_exceptions());

        auto ex1 = std::exception_ptr{};
        auto ex1_trace = std::vector<stacktrace_entry>{};
        try {
            throw 42;
        } catch (...) {
            ex1 = std::current_exception();
            const auto trace = stacktrace_from_exception(ex1);
            REQUIRE(!trace.empty());
            ex1_trace.assign(trace.begin(), trace.end());
        }

        const auto ex2 = ex1;

        const auto ex2_trace = stacktrace_from_exception(ex2);
        REQUIRE(std::ranges::equal(ex2_trace, ex1_trace));
    }
    check_for_exception_stacktrace_leaks();
}

TEST_CASE("The stacktrace associated with an std::exception_ptr follows copy assignment") {
    {
        REQUIRE(enable_stacktrace_from_exceptions());

        auto ex1 = std::exception_ptr{};
        auto ex1_init_trace = std::vector<stacktrace_entry>{};
        try {
            throw 42;
        } catch (...) {
            ex1 = std::current_exception();
            const auto trace = stacktrace_from_exception(ex1);
            REQUIRE(!trace.empty());
            ex1_init_trace.assign(trace.begin(), trace.end());
        }

        auto ex2 = std::exception_ptr{};
        auto ex2_init_trace = std::vector<stacktrace_entry>{};
        try {
            throw 67;
        } catch (...) {
            ex2 = std::current_exception();
            const auto trace = stacktrace_from_exception(ex2);
            REQUIRE(!trace.empty());
            ex2_init_trace.assign(trace.begin(), trace.end());
        }

        REQUIRE(ex1_init_trace != ex2_init_trace);

        ex1 = ex2;
        const auto ex1_new_trace = stacktrace_from_exception(ex1);
        REQUIRE(std::ranges::equal(ex1_new_trace, ex2_init_trace));
    }
    check_for_exception_stacktrace_leaks();
}

TEST_CASE("The stacktrace associated with an std::exception_ptr follows swap") {
    {
        REQUIRE(enable_stacktrace_from_exceptions());

        auto ex1 = std::exception_ptr{};
        auto ex1_init_trace = std::vector<stacktrace_entry>{};
        try {
            throw 42;
        } catch (...) {
            ex1 = std::current_exception();
            const auto trace = stacktrace_from_exception(ex1);
            REQUIRE(!trace.empty());
            ex1_init_trace.assign(trace.begin(), trace.end());
        }

        auto ex2 = std::exception_ptr{};
        auto ex2_init_trace = std::vector<stacktrace_entry>{};
        try {
            throw 67;
        } catch (...) {
            ex2 = std::current_exception();
            const auto trace = stacktrace_from_exception(ex2);
            REQUIRE(!trace.empty());
            ex2_init_trace.assign(trace.begin(), trace.end());
        }

        REQUIRE(ex1_init_trace != ex2_init_trace);

        std::swap(ex1, ex2);
        const auto ex1_new_trace = stacktrace_from_exception(ex1);
        REQUIRE(std::ranges::equal(ex1_new_trace, ex2_init_trace));
        const auto ex2_new_trace = stacktrace_from_exception(ex2);
        REQUIRE(std::ranges::equal(ex2_new_trace, ex1_init_trace));
    }
    check_for_exception_stacktrace_leaks();
}

TEST_CASE("std::make_exception_ptr collects a stacktrace") {
    {
        REQUIRE(enable_stacktrace_from_exceptions());

        const auto ex = std::make_exception_ptr(42);
        const auto trace = stacktrace_from_exception(ex);
        REQUIRE(!trace.empty());
    }
    check_for_exception_stacktrace_leaks();
}

} // namespace hindsight