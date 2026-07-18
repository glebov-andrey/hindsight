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

#ifndef HINDSIGHT_INCLUDE_HINDSIGHT_RESOLVER_HPP
#define HINDSIGHT_INCLUDE_HINDSIGHT_RESOLVER_HPP

#include <hindsight/detail/config.hpp>

#include <iterator>
#include <memory>
#include <ranges>

#include <hindsight/logical_stacktrace_entry.hpp>

#include <hindsight/detail/function_ref.hpp>

#ifdef HINDSIGHT_OS_WINDOWS
using HANDLE = void *;
#endif

namespace hindsight {

#ifdef HINDSIGHT_OS_WINDOWS

struct from_process_handle_t {
    explicit from_process_handle_t() = default;
};

inline constexpr auto from_process_handle = from_process_handle_t{};

#endif

#ifdef HINDSIGHT_OS_LINUX

struct from_proc_maps_t {
    explicit from_proc_maps_t() = default;
};

inline constexpr auto from_proc_maps = from_proc_maps_t{};

#endif

class HINDSIGHT_API resolver {
public:
    explicit resolver();

#ifdef HINDSIGHT_OS_WINDOWS
    // Takes ownership of the process handle, closes the handle on failure
    explicit resolver(from_process_handle_t from_process_handle_tag, HANDLE process);
#endif

#ifdef HINDSIGHT_OS_LINUX
    // Only supported by the libdw backend.
    // Takes ownership of the file descriptor, closes the descriptor on failure.
    explicit resolver(from_proc_maps_t from_proc_maps_tag, int proc_maps_descriptor);
#endif

    resolver(const resolver &other) = delete;
    resolver(resolver &&other) = delete;

    ~resolver();

    auto operator=(const resolver &other) -> resolver & = delete;
    auto operator=(resolver &&other) -> resolver & = delete;

    template<std::output_iterator<logical_stacktrace_entry> It, std::sentinel_for<It> Sentinel>
    [[nodiscard]] auto resolve(const stacktrace_entry entry, It first, const Sentinel last)
            -> std::conditional_t<std::forward_iterator<It>, It, void> {
        if (first != last) {
            resolve_impl(entry, [&](logical_stacktrace_entry &&logical) -> bool {
                *first++ = std::move(logical);
                return first == last;
            });
        }

        if constexpr (std::forward_iterator<It>) {
            return std::move(first);
        }
    }

    template<std::ranges::output_range<logical_stacktrace_entry> Range>
    [[nodiscard]] auto resolve(const stacktrace_entry entry, Range &&range) {
        if constexpr (std::ranges::forward_range<Range>) {
            return std::ranges::borrowed_subrange_t<Range>{
                    std::ranges::begin(range),
                    resolve(entry, std::ranges::begin(range), std::ranges::end(range))};
        } else {
            resolve(entry, std::ranges::begin(range), std::ranges::end(range));
        }
    }

private:
    // Returns true if done
    using resolve_cb = detail::function_ref<bool(logical_stacktrace_entry &&logical)>;

    auto resolve_impl(stacktrace_entry entry, resolve_cb callback) -> void;

    class impl;
    HINDSIGHT_PRAGMA_MSVC("warning(push)")
    // std::unique_ptr<impl> needs to have dll-interface to be used by clients of class 'hindsight::resolver'
    HINDSIGHT_PRAGMA_MSVC("warning(disable : 4251)")
    std::unique_ptr<impl> m_impl;
    HINDSIGHT_PRAGMA_MSVC("warning(pop)")
};

} // namespace hindsight

#endif // HINDSIGHT_INCLUDE_HINDSIGHT_RESOLVER_HPP
