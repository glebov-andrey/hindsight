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

#ifndef HINDSIGHT_EXTRAS_EXCEPTIONS_HPP
#define HINDSIGHT_EXTRAS_EXCEPTIONS_HPP

#include <exception>
#include <span>

#include <hindsight/stacktrace_entry.hpp>

namespace hindsight {

auto enable_stack_traces_from_exceptions() -> bool;

[[nodiscard]] auto stack_trace_from_current_exception() noexcept -> std::span<const stacktrace_entry>;

[[nodiscard]] auto stack_trace_from_exception(const std::exception_ptr &ex) noexcept
        -> std::span<const stacktrace_entry>;

} // namespace hindsight

#endif // HINDSIGHT_EXTRAS_EXCEPTIONS_HPP
