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

#include <hindsight/from_exception.hpp>

namespace hindsight {

auto enable_stacktrace_from_exceptions() -> bool { return false; }

auto disable_stacktrace_from_exceptions() -> void {}

auto stacktrace_from_current_exception() noexcept -> std::span<const stacktrace_entry> { return {}; }

auto stacktrace_from_exception(const std::exception_ptr & /*ex*/) noexcept -> std::span<const stacktrace_entry> {
    return {};
}

auto check_for_exception_stacktrace_leaks() -> void {}

} // namespace hindsight
