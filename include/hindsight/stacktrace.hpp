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

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

namespace hindsight {

struct from_native_handle_t {};

inline constexpr auto from_native_handle = from_native_handle_t{};


class stacktrace_entry {
public:
    using native_handle_type = std::uintptr_t;

    constexpr stacktrace_entry() noexcept = default;

    constexpr stacktrace_entry(from_native_handle_t /* from_native_handle */, const native_handle_type handle) noexcept
            : m_handle{handle} {}

    [[nodiscard]] constexpr explicit operator bool() const noexcept { return m_handle; }

    [[nodiscard]] constexpr auto native_handle() const noexcept -> native_handle_type { return m_handle; }

private:
    native_handle_type m_handle{};
};

namespace detail {

// Returns true if done
using capture_stack_trace_cb = bool(stacktrace_entry entry, void *user_data);

auto capture_stack_trace_impl(std::size_t entries_to_skip, capture_stack_trace_cb *callback, void *user_data) -> void;

} // namespace detail

template<std::output_iterator<stacktrace_entry> It, std::sentinel_for<It> Sentinel>
[[nodiscard]] auto capture_stack_trace(It first, Sentinel last, const std::size_t entries_to_skip = 0)
        -> std::conditional_t<std::forward_iterator<It>, It, void> {
    if (first == last) {
        if constexpr (std::forward_iterator<It>) {
            return std::move(first);
        } else {
            return;
        }
    }

    struct cb_state {
#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wunknown-attributes"
#endif
        [[no_unique_address]] It first;
        [[no_unique_address]] const Sentinel last;
#ifdef __clang__
    #pragma clang diagnostic pop
#endif
    } state{.first = std::move(first), .last = std::move(last)};
    detail::capture_stack_trace_impl(
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

template<std::ranges::output_range<stacktrace_entry> Range>
    requires std::ranges::forward_range<Range>
[[nodiscard]] auto capture_stack_trace(Range &&range, const std::size_t entries_to_skip = 0)
        -> std::ranges::borrowed_subrange_t<Range> {
    return {std::ranges::begin(range),
            capture_stack_trace(std::ranges::begin(range), std::ranges::end(range), entries_to_skip)};
}

template<std::ranges::output_range<stacktrace_entry> Range>
    requires(!std::ranges::forward_range<Range>)
auto capture_stack_trace(Range &&range, const std::size_t entries_to_skip = 0) -> void {
    capture_stack_trace(std::ranges::begin(range), std::ranges::end(range), entries_to_skip);
}

[[nodiscard]] auto capture_stack_trace(std::size_t entries_to_skip = 0) -> std::vector<stacktrace_entry>;

} // namespace hindsight

#endif // HINDSIGHT_INCLUDE_HINDSIGHT_STACKTRACE_HPP
