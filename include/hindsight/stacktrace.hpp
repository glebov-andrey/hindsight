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

#ifndef HINDSIGHT_INCLUDE_HINDSIGHT_STACKTRACE_HPP
#define HINDSIGHT_INCLUDE_HINDSIGHT_STACKTRACE_HPP

#include <concepts>
#include <cstddef>
#include <iterator>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

#include <hindsight/config.hpp>
#include <hindsight/stacktrace_entry.hpp>

#ifdef HINDSIGHT_OS_WINDOWS
using CONTEXT = struct _CONTEXT; // NOLINT(bugprone-reserved-identifier)
#else
using ucontext_t = struct ucontext_t;
#endif

namespace hindsight {

#ifdef HINDSIGHT_OS_WINDOWS
using native_context_type = CONTEXT;
#else
using native_context_type = ucontext_t;
#endif

namespace detail {

// Returns true if done
using capture_stack_trace_cb = bool(stacktrace_entry entry, void *user_data);

HINDSIGHT_API auto capture_stack_trace_from_mutable_context_impl(native_context_type &context,
                                                                 std::size_t entries_to_skip,
                                                                 capture_stack_trace_cb *callback,
                                                                 void *user_data) -> void;

HINDSIGHT_API auto capture_stack_trace_impl(std::size_t entries_to_skip,
                                            capture_stack_trace_cb *callback,
                                            void *user_data) -> void;

HINDSIGHT_API auto capture_stack_trace_from_context_impl(const native_context_type &context,
                                                         std::size_t entries_to_skip,
                                                         capture_stack_trace_cb *callback,
                                                         void *user_data) -> void;

template<typename ImplFunction, std::output_iterator<stacktrace_entry> It, std::sentinel_for<It> Sentinel>
[[nodiscard]] auto capture_stack_trace_iterator_adapter(const ImplFunction impl_function,
                                                        It first,
                                                        Sentinel last,
                                                        const std::size_t entries_to_skip)
        -> std::conditional_t<std::forward_iterator<It>, It, void> {
    if (first == last) {
        if constexpr (std::forward_iterator<It>) {
            return std::move(first);
        } else {
            return;
        }
    }

    struct cb_state {
        HINDSIGHT_PRAGMA_CLANG("clang diagnostic push")
        HINDSIGHT_PRAGMA_CLANG("clang diagnostic ignored \"-Wunknown-attributes\"")
        [[no_unique_address]] It first;
        [[no_unique_address]] const Sentinel last;
        HINDSIGHT_PRAGMA_CLANG("clang diagnostic pop")
    } state{.first = std::move(first), .last = std::move(last)};
    impl_function(
            entries_to_skip,
            [](const stacktrace_entry entry, void *const state_ptr) {
                auto &state = *static_cast<cb_state *>(state_ptr);
                *state.first++ = entry;
                return state.first == state.last;
            },
            &state);

    if constexpr (std::forward_iterator<It>) {
        return std::move(state.first);
    }
}

} // namespace detail

template<std::output_iterator<stacktrace_entry> It, std::sentinel_for<It> Sentinel>
[[nodiscard]] auto capture_stack_trace(It first, Sentinel last, const std::size_t entries_to_skip = 0)
        -> std::conditional_t<std::forward_iterator<It>, It, void> {
    return detail::capture_stack_trace_iterator_adapter(
            [](const auto... args) { detail::capture_stack_trace_impl(args...); },
            std::move(first),
            std::move(last),
            entries_to_skip);
}

template<std::ranges::output_range<stacktrace_entry> Range>
[[nodiscard]] auto capture_stack_trace(Range &&range, const std::size_t entries_to_skip = 0) {
    if constexpr (std::ranges::forward_range<Range>) {
        return std::ranges::borrowed_subrange_t<Range>{
                std::ranges::begin(range),
                capture_stack_trace(std::ranges::begin(range), std::ranges::end(range), entries_to_skip)};
    } else {
        capture_stack_trace(std::ranges::begin(range), std::ranges::end(range), entries_to_skip);
    }
}

template<typename Allocator = std::allocator<stacktrace_entry>>
[[nodiscard]] auto capture_stack_trace(const std::size_t entries_to_skip = 0, const Allocator &allocator = Allocator())
        -> std::vector<stacktrace_entry,
                       typename std::allocator_traits<Allocator>::template rebind_alloc<stacktrace_entry>> {
    static constexpr auto initial_stack_trace_capacity = std::size_t{16};
    using rebound_allocator = typename std::allocator_traits<Allocator>::template rebind_alloc<stacktrace_entry>;

    auto entries = std::vector<stacktrace_entry, rebound_allocator>(allocator);
    entries.reserve(initial_stack_trace_capacity);
    capture_stack_trace(std::back_inserter(entries), std::unreachable_sentinel, entries_to_skip);
    return entries;
}


template<std::output_iterator<stacktrace_entry> It, std::sentinel_for<It> Sentinel>
[[nodiscard]] auto capture_stack_trace_from_context(const native_context_type &context,
                                                    It first,
                                                    Sentinel last,
                                                    const std::size_t entries_to_skip = 0)
        -> std::conditional_t<std::forward_iterator<It>, It, void> {
    return detail::capture_stack_trace_iterator_adapter(
            [&context](const auto... args) { detail::capture_stack_trace_from_context_impl(context, args...); },
            std::move(first),
            std::move(last),
            entries_to_skip);
}

} // namespace hindsight

#endif // HINDSIGHT_INCLUDE_HINDSIGHT_STACKTRACE_HPP
