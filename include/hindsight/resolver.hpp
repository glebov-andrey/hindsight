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

#include <concepts>
#include <cstdint>
#include <iterator>
#include <string>
#include <utility>
#ifdef HINDSIGHT_HAS_STD_RANGES
    #include <ranges>
#endif
#ifdef HINDSIGHT_OS_WINDOWS
    #include <array>
    #include <cstddef>
#endif
#if defined HINDSIGHT_OS_WINDOWS || defined HINDSIGHT_OS_LINUX
    #include <memory>
#endif

#include <tl/function_ref.hpp>

#include <hindsight/stacktrace.hpp>

#ifdef HINDSIGHT_OS_WINDOWS
using HANDLE = void *;
#endif

namespace hindsight {

template<typename Char>
struct basic_source_location {
    std::basic_string<Char> file_name{};
    std::uint_least32_t line_number{};
};

using source_location = basic_source_location<char>;
using u8_source_location = basic_source_location<char8_t>;


class HINDSIGHT_API logical_stacktrace_entry {
public:
    logical_stacktrace_entry() noexcept;

    logical_stacktrace_entry(const logical_stacktrace_entry &other);
    logical_stacktrace_entry(logical_stacktrace_entry &&other) noexcept;

    ~logical_stacktrace_entry();

    auto operator=(const logical_stacktrace_entry &other) -> logical_stacktrace_entry & {
        logical_stacktrace_entry{other}.swap(*this);
        return *this;
    }

    auto operator=(logical_stacktrace_entry &&other) noexcept -> logical_stacktrace_entry & {
        logical_stacktrace_entry{std::move(other)}.swap(*this);
        return *this;
    }

    auto swap(logical_stacktrace_entry &other) noexcept -> void;

    HINDSIGHT_API friend auto swap(logical_stacktrace_entry &lhs, logical_stacktrace_entry &rhs) noexcept -> void {
        lhs.swap(rhs);
    }

    [[nodiscard]] auto physical() const noexcept -> stacktrace_entry { return m_physical; }

    [[nodiscard]] auto is_inline() const noexcept -> bool { return m_is_inline; }

    [[nodiscard]] auto symbol() const -> std::string;
    [[nodiscard]] auto u8_symbol() const -> std::u8string;

    [[nodiscard]] auto source() const -> source_location;
    [[nodiscard]] auto u8_source() const -> u8_source_location;

#ifdef HINDSIGHT_OS_WINDOWS
    struct impl_payload;
    HINDSIGHT_API_HIDDEN logical_stacktrace_entry(stacktrace_entry physical,
                                                  bool is_inline,
                                                  impl_payload &&impl) noexcept;
#elif defined HINDSIGHT_OS_LINUX
    struct impl_tag;
    HINDSIGHT_API_HIDDEN logical_stacktrace_entry(impl_tag &&impl,
                                                  stacktrace_entry physical,
                                                  bool is_inline,
                                                  bool maybe_mangled,
                                                  const char *symbol,
                                                  const char *file_name,
                                                  std::uint_least32_t line_number,
                                                  std::uint_least32_t column_number,
                                                  std::shared_ptr<const void> resolver_impl) noexcept;
#else
    struct impl_tag;
    HINDSIGHT_API_HIDDEN logical_stacktrace_entry(impl_tag &&impl,
                                                  stacktrace_entry physical,
                                                  bool is_inline,
                                                  std::string &&symbol,
                                                  source_location &&source) noexcept;
    HINDSIGHT_API_HIDDEN auto set_inline(impl_tag &&impl) noexcept -> void;
#endif

private:
    stacktrace_entry m_physical{};
    bool m_is_inline{};

#ifdef HINDSIGHT_OS_WINDOWS
    static constexpr auto impl_payload_size = sizeof(void *) * 2;
    std::array<std::byte, impl_payload_size> m_impl_storage;

    HINDSIGHT_API_HIDDEN [[nodiscard]] auto impl() const noexcept -> const impl_payload &;
    HINDSIGHT_API_HIDDEN [[nodiscard]] auto impl() noexcept -> impl_payload &;
#elif defined HINDSIGHT_OS_LINUX
    bool m_maybe_mangled{};
    const char *m_symbol{};

    const char *m_file_name{};
    std::uint_least32_t m_line_number{};
    std::uint_least32_t m_column_number{};

    std::shared_ptr<const void> m_resolver_impl;
#else
    std::string m_symbol{};
    source_location m_source{};
#endif
};

#ifndef HINDSIGHT_OS_WINDOWS

inline logical_stacktrace_entry::logical_stacktrace_entry() noexcept = default;
inline logical_stacktrace_entry::logical_stacktrace_entry(logical_stacktrace_entry &&other) noexcept = default;

inline auto logical_stacktrace_entry::swap(logical_stacktrace_entry &other) noexcept -> void {
    std::ranges::swap(m_physical, other.m_physical);
    std::ranges::swap(m_is_inline, other.m_is_inline);
    #ifdef HINDSIGHT_OS_LINUX
    std::ranges::swap(m_maybe_mangled, other.m_maybe_mangled);
    std::ranges::swap(m_symbol, other.m_symbol);

    std::ranges::swap(m_file_name, other.m_file_name);
    std::ranges::swap(m_line_number, other.m_line_number);
    std::ranges::swap(m_column_number, other.m_column_number);

    std::ranges::swap(m_resolver_impl, other.m_resolver_impl);
    #else
    std::ranges::swap(m_symbol, other.m_symbol);
    std::ranges::swap(m_source, other.m_source);
    #endif
}

#endif


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
    explicit resolver(from_process_handle_t from_process_handle_tag, HANDLE process);
#endif

#ifdef HINDSIGHT_OS_LINUX
    explicit resolver(from_proc_maps_t from_proc_maps_tag, int proc_maps_descriptor);
#endif

    resolver(const resolver &other) = delete;

    resolver(resolver &&other) noexcept = default;

    ~resolver() = default;

    auto operator=(const resolver &other) -> resolver & = delete;

    auto operator=(resolver &&other) noexcept -> resolver & = default;

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

#if defined HINDSIGHT_OS_WINDOWS || defined HINDSIGHT_OS_LINUX
    class impl;
    HINDSIGHT_PRAGMA_MSVC("warning(push)")
    // std::shared_ptr<impl> needs to have dll-interface to be used by clients of class 'hindsight::resolver'
    HINDSIGHT_PRAGMA_MSVC("warning(disable : 4251)")
    std::shared_ptr<impl> m_impl;
    HINDSIGHT_PRAGMA_MSVC("warning(pop)")
#endif
};

} // namespace hindsight

#endif // HINDSIGHT_INCLUDE_HINDSIGHT_RESOLVER_HPP
