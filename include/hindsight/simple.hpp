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

#ifndef HINDSIGHT_INCLUDE_HINDSIGHT_SIMPLE_HPP
#define HINDSIGHT_INCLUDE_HINDSIGHT_SIMPLE_HPP

#include <hindsight/config.hpp>

#include <iosfwd>
#include <span>

#include <hindsight/stacktrace_entry.hpp>

namespace hindsight {

HINDSIGHT_API auto print_stacktrace(std::ostream &stream, std::span<const stacktrace_entry> entries) -> void;

HINDSIGHT_API auto print_stacktrace_here() -> void;

HINDSIGHT_API auto print_stacktrace_here(std::ostream &stream) -> void;

} // namespace hindsight

#endif // HINDSIGHT_INCLUDE_HINDSIGHT_SIMPLE_HPP
