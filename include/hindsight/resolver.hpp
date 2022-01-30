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

#ifndef HINDSIGHT_INCLUDE_HINDSIGHT_RESOLVER_HPP
#define HINDSIGHT_INCLUDE_HINDSIGHT_RESOLVER_HPP

#include <hindsight/config.hpp>

#include <cstdint>
#include <iterator>
#include <string>
#include <utility>
#ifdef HINDSIGHT_HAS_STD_RANGES
    #include <ranges>
#endif
#if HINDSIGHT_RESOLVER_BACKEND == HINDSIGHT_RESOLVER_BACKEND_DIA ||                                                    \
        HINDSIGHT_RESOLVER_BACKEND == HINDSIGHT_RESOLVER_BACKEND_LIBDW
    #include <memory>
#endif

#include <tl/function_ref.hpp>

#include <hindsight/stacktrace.hpp>

#if HINDSIGHT_RESOLVER_BACKEND == HINDSIGHT_RESOLVER_BACKEND_DIA
    #include <hindsight/detail/bstr.hpp>
#endif

#if HINDSIGHT_RESOLVER_BACKEND == HINDSIGHT_RESOLVER_BACKEND_DIA
using HANDLE = void *;
#endif

namespace hindsight {

template<typename Char>
struct basic_source_location {
    std::basic_string<Char> file_name{};
    std::uint_least32_t line_number{};
    std::uint_least32_t column_number{};
};

using source_location = basic_source_location<char>;
using u8_source_location = basic_source_location<char8_t>;


class HINDSIGHT_API logical_stacktrace_entry {
public:
    logical_stacktrace_entry() = default;

    [[nodiscard]] auto physical() const noexcept -> stacktrace_entry { return m_physical; }

    [[nodiscard]] auto symbol() const -> std::string;
    [[nodiscard]] auto u8_symbol() const -> std::u8string;

    [[nodiscard]] auto source() const -> source_location;
    [[nodiscard]] auto u8_source() const -> u8_source_location;

    [[nodiscard]] auto is_inline() const noexcept -> bool { return m_is_inline; }

private:
    stacktrace_entry m_physical{};
#if HINDSIGHT_RESOLVER_BACKEND == HINDSIGHT_RESOLVER_BACKEND_DIA
    detail::bstr m_symbol{};
    detail::bstr m_file_name{};
    std::uint_least32_t m_line_number{};
#else
    std::string m_raw_symbol{};
    std::string m_raw_file_name{};
    std::uint_least32_t m_line_number{};
    #if HINDSIGHT_RESOLVER_BACKEND == HINDSIGHT_RESOLVER_BACKEND_LIBDW
    std::uint_least32_t m_column_number{};
    bool m_maybe_mangled{};
    #endif
#endif
    bool m_is_inline{};

    friend class resolver;

    HINDSIGHT_API_HIDDEN explicit logical_stacktrace_entry(stacktrace_entry physical) noexcept : m_physical{physical} {}

#if HINDSIGHT_RESOLVER_BACKEND == HINDSIGHT_RESOLVER_BACKEND_DIA
    HINDSIGHT_API_HIDDEN logical_stacktrace_entry(stacktrace_entry physical,
                                                  detail::bstr symbol,
                                                  detail::bstr file_name,
                                                  std::uint_least32_t line_number,
                                                  bool is_inline) noexcept;
#else
    HINDSIGHT_API_HIDDEN logical_stacktrace_entry(stacktrace_entry physical,
                                                  std::string raw_symbol,
                                                  std::string raw_file_name,
                                                  std::uint_least32_t line_number,
    #if HINDSIGHT_RESOLVER_BACKEND == HINDSIGHT_RESOLVER_BACKEND_LIBDW
                                                  std::uint_least32_t column_number,
                                                  bool maybe_mangled,
    #endif
                                                  bool is_inline) noexcept;
#endif

    HINDSIGHT_API friend auto swap(logical_stacktrace_entry &lhs, logical_stacktrace_entry &rhs) noexcept -> void {
        using std::swap;
        swap(lhs.m_physical, rhs.m_physical);
#if HINDSIGHT_RESOLVER_BACKEND == HINDSIGHT_RESOLVER_BACKEND_DIA
        swap(lhs.m_symbol, rhs.m_symbol);
        swap(lhs.m_file_name, rhs.m_file_name);
        swap(lhs.m_line_number, rhs.m_line_number);
#else
        swap(lhs.m_raw_symbol, rhs.m_raw_symbol);
        swap(lhs.m_raw_file_name, rhs.m_raw_file_name);
        swap(lhs.m_line_number, rhs.m_line_number);
    #if HINDSIGHT_RESOLVER_BACKEND == HINDSIGHT_RESOLVER_BACKEND_LIBDW
        swap(lhs.m_column_number, rhs.m_column_number);
        swap(lhs.m_maybe_mangled, rhs.m_maybe_mangled);
    #endif
#endif
        swap(lhs.m_is_inline, rhs.m_is_inline);
    }
};


#if HINDSIGHT_RESOLVER_BACKEND == HINDSIGHT_RESOLVER_BACKEND_DIA

struct from_process_handle_t {
    explicit from_process_handle_t() = default;
};

inline constexpr auto from_process_handle = from_process_handle_t{};

#endif

#if HINDSIGHT_RESOLVER_BACKEND == HINDSIGHT_RESOLVER_BACKEND_LIBDW

struct from_proc_maps_t {
    explicit from_proc_maps_t() = default;
};

inline constexpr auto from_proc_maps = from_proc_maps_t{};

#endif

class HINDSIGHT_API resolver {
public:
    explicit resolver();

#if HINDSIGHT_RESOLVER_BACKEND == HINDSIGHT_RESOLVER_BACKEND_DIA
    // Takes ownership of the process handle, closes the handle on failure
    explicit resolver(from_process_handle_t from_process_handle_tag, HANDLE process);
#endif

#if HINDSIGHT_RESOLVER_BACKEND == HINDSIGHT_RESOLVER_BACKEND_LIBDW
    // Takes ownership of the file descriptor, closes the descriptor on failure
    explicit resolver(from_proc_maps_t from_proc_maps_tag, int proc_maps_descriptor);
#endif

    resolver(const resolver &other) = delete;
    resolver(resolver &&other) = delete;

#if HINDSIGHT_RESOLVER_BACKEND == HINDSIGHT_RESOLVER_BACKEND_DIA ||                                                    \
        HINDSIGHT_RESOLVER_BACKEND == HINDSIGHT_RESOLVER_BACKEND_LIBDW
    ~resolver();
#endif

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

#ifdef HINDSIGHT_HAS_STD_RANGES
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
#endif

private:
    // Returns true if done
    using resolve_cb = tl::function_ref<bool(logical_stacktrace_entry &&logical)>;

    auto resolve_impl(stacktrace_entry entry, resolve_cb callback) -> void;

#if HINDSIGHT_RESOLVER_BACKEND == HINDSIGHT_RESOLVER_BACKEND_DIA ||                                                    \
        HINDSIGHT_RESOLVER_BACKEND == HINDSIGHT_RESOLVER_BACKEND_LIBDW
    class impl;
    HINDSIGHT_PRAGMA_MSVC("warning(push)")
    // std::unique_ptr<impl> needs to have dll-interface to be used by clients of class 'hindsight::resolver'
    HINDSIGHT_PRAGMA_MSVC("warning(disable : 4251)")
    std::unique_ptr<impl> m_impl;
    HINDSIGHT_PRAGMA_MSVC("warning(pop)")
#endif
};

} // namespace hindsight

#endif // HINDSIGHT_INCLUDE_HINDSIGHT_RESOLVER_HPP
