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

#include <iostream>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <hindsight/simple.hpp>

#include "exceptions.hpp"

struct A {
    ~A() {
        fmt::print(std::cerr, "\n~A()\n");
        hindsight::print_stacktrace_here();
    }
};

int main() {
    hindsight::enable_stack_traces_from_exceptions();
    try {
        // throw A();
        throw 42;
    } catch (...) {
        // const auto entries = hindsight::stack_trace_from_current_exception();
        const auto ex = std::current_exception();
        const auto entries = hindsight::stack_trace_from_exception(ex);
        fmt::print(std::cerr, "\nhindsight::stack_trace_from_current_exception\n");
        hindsight::print_stacktrace(std::cerr, entries);
    }
}
