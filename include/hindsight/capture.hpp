/*
 * Copyright 2025 Andrey Glebov
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

#ifndef HINDSIGHT_INCLUDE_HINDSIGHT_CAPTURE_HPP
#define HINDSIGHT_INCLUDE_HINDSIGHT_CAPTURE_HPP

#include <hindsight/detail/config.hpp>

#include <cstddef>
#include <iterator>
#include <limits>
#include <ranges>
#include <type_traits>

#include <hindsight/detail/function_ref.hpp>
#include <hindsight/stacktrace_entry.hpp>

#ifdef HINDSIGHT_OS_WINDOWS
using CONTEXT = struct _CONTEXT; // NOLINT(bugprone-reserved-identifier)
using EXCEPTION_POINTERS = struct _EXCEPTION_POINTERS; // NOLINT(bugprone-reserved-identifier)
#else
    #include <signal.h> // siginfo_t
    #include <ucontext.h> // ucontext_t
#endif

namespace hindsight {

#ifdef HINDSIGHT_OS_WINDOWS
using native_context_type = CONTEXT;
#else
using native_context_type = ucontext_t;
#endif

#ifdef HINDSIGHT_OS_WINDOWS
using native_signal_parameters = const EXCEPTION_POINTERS *;
#else
struct native_signal_parameters {
    int signo;
    const siginfo_t *info;
    const void *context;
};
#endif

namespace detail {

constexpr auto increment_if_has_noinline([[maybe_unused]] std::size_t &val) noexcept {
#ifdef HINDSIGHT_HAS_NOINLINE
    if (val != std::numeric_limits<std::size_t>::max()) {
        ++val;
    }
#endif
}

// Returns true if done
using capture_stacktrace_cb = function_ref<bool(stacktrace_entry entry)>;

HINDSIGHT_NOINLINE HINDSIGHT_API auto capture_stacktrace(std::size_t entries_to_skip, capture_stacktrace_cb callback)
        -> void;

HINDSIGHT_API auto capture_stacktrace_from_context(const native_context_type &context,
                                                   std::size_t entries_to_skip,
                                                   capture_stacktrace_cb callback) -> void;

HINDSIGHT_API auto capture_stacktrace_from_mutable_context(native_context_type &context,
                                                           std::size_t entries_to_skip,
                                                           capture_stacktrace_cb callback) -> void;

HINDSIGHT_API auto capture_stacktrace_from_signal(native_signal_parameters params,
                                                  std::size_t entries_to_skip,
                                                  capture_stacktrace_cb callback) -> void;

} // namespace detail

template<std::output_iterator<stacktrace_entry> It, std::sentinel_for<It> Sentinel>
[[nodiscard]] HINDSIGHT_NOINLINE auto capture_stacktrace(It first, const Sentinel last, std::size_t entries_to_skip = 0)
        -> std::conditional_t<std::forward_iterator<It>, It, void> {
    if (first != last) {
        detail::increment_if_has_noinline(entries_to_skip);
        detail::capture_stacktrace(entries_to_skip, [&](const stacktrace_entry entry) {
            *first = entry;
            ++first;
            return first == last;
        });
    }
    if constexpr (std::forward_iterator<It>) {
        return first;
    }
}

template<std::ranges::output_range<stacktrace_entry> Range>
[[nodiscard]] HINDSIGHT_NOINLINE auto capture_stacktrace(Range &&range, std::size_t entries_to_skip = 0) {
    auto first = std::ranges::begin(range);
    const auto last = std::ranges::end(range);
    if (first != last) {
        detail::increment_if_has_noinline(entries_to_skip);
        detail::capture_stacktrace(entries_to_skip, [&](const stacktrace_entry entry) {
            *first = entry;
            ++first;
            return first == last;
        });
    }
    if constexpr (std::ranges::forward_range<Range>) {
        return std::ranges::borrowed_subrange_t<Range>{std::ranges::begin(range), first};
    }
}

template<std::output_iterator<stacktrace_entry> It, std::sentinel_for<It> Sentinel>
[[nodiscard]] auto capture_stacktrace_from_context(const native_context_type &context,
                                                   It first,
                                                   const Sentinel last,
                                                   const std::size_t entries_to_skip = 0)
        -> std::conditional_t<std::forward_iterator<It>, It, void> {
    if (first != last) {
        detail::capture_stacktrace_from_context(context, entries_to_skip, [&](const stacktrace_entry entry) {
            *first = entry;
            ++first;
            return first == last;
        });
    }
    if constexpr (std::forward_iterator<It>) {
        return first;
    }
}

template<std::ranges::output_range<stacktrace_entry> Range>
[[nodiscard]] auto capture_stacktrace_from_context(const native_context_type &context,
                                                   Range &&range,
                                                   const std::size_t entries_to_skip = 0) {
    if constexpr (std::ranges::forward_range<Range>) {
        return std::ranges::borrowed_subrange_t<Range>{std::ranges::begin(range),
                                                       capture_stacktrace_from_context(context,
                                                                                       std::ranges::begin(range),
                                                                                       std::ranges::end(range),
                                                                                       entries_to_skip)};
    } else {
        capture_stacktrace_from_context(context, std::ranges::begin(range), std::ranges::end(range), entries_to_skip);
    }
}

template<std::output_iterator<stacktrace_entry> It, std::sentinel_for<It> Sentinel>
[[nodiscard]] auto capture_stacktrace_from_mutable_context(native_context_type &context,
                                                           It first,
                                                           const Sentinel last,
                                                           const std::size_t entries_to_skip = 0)
        -> std::conditional_t<std::forward_iterator<It>, It, void> {
    if (first != last) {
        detail::capture_stacktrace_from_mutable_context(context, entries_to_skip, [&](const stacktrace_entry entry) {
            *first = entry;
            ++first;
            return first == last;
        });
    }
    if constexpr (std::forward_iterator<It>) {
        return first;
    }
}

template<std::ranges::output_range<stacktrace_entry> Range>
[[nodiscard]] auto capture_stacktrace_from_mutable_context(native_context_type &context,
                                                           Range &&range,
                                                           const std::size_t entries_to_skip = 0) {
    if constexpr (std::ranges::forward_range<Range>) {
        return std::ranges::borrowed_subrange_t<Range>{
                std::ranges::begin(range),
                capture_stacktrace_from_mutable_context(context,
                                                        std::ranges::begin(range),
                                                        std::ranges::end(range),
                                                        entries_to_skip)};
    } else {
        capture_stacktrace_from_mutable_context(context,
                                                std::ranges::begin(range),
                                                std::ranges::end(range),
                                                entries_to_skip);
    }
}

template<std::output_iterator<stacktrace_entry> It, std::sentinel_for<It> Sentinel>
[[nodiscard]] auto capture_stacktrace_from_signal(const native_signal_parameters params,
                                                  It first,
                                                  const Sentinel last,
                                                  const std::size_t entries_to_skip = 0)
        -> std::conditional_t<std::forward_iterator<It>, It, void> {
    if (first != last) {
        detail::capture_stacktrace_from_signal(params, entries_to_skip, [&](const stacktrace_entry entry) {
            *first = entry;
            ++first;
            return first == last;
        });
    }
    if constexpr (std::forward_iterator<It>) {
        return first;
    }
}

template<std::ranges::output_range<stacktrace_entry> Range>
[[nodiscard]] auto capture_stacktrace_from_mutable_context(const native_signal_parameters params,
                                                           Range &&range,
                                                           const std::size_t entries_to_skip = 0) {
    if constexpr (std::ranges::forward_range<Range>) {
        return std::ranges::borrowed_subrange_t<Range>{std::ranges::begin(range),
                                                       capture_stacktrace_from_signal(params,
                                                                                      entries_to_skip,
                                                                                      std::ranges::begin(range),
                                                                                      std::ranges::end(range))};
    } else {
        capture_stacktrace_from_signal(params, entries_to_skip, std::ranges::begin(range), std::ranges::end(range));
    }
}

} // namespace hindsight

#endif // HINDSIGHT_INCLUDE_HINDSIGHT_CAPTURE_HPP
