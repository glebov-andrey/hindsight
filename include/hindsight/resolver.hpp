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
#include <ranges>
#include <string>
#include <utility>
#ifdef HINDSIGHT_OS_WINDOWS
    #include <array>
    #include <cstddef>
    #include <memory>
#endif

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

    HINDSIGHT_API_HIDDEN [[nodiscard]] auto impl() const noexcept -> const impl_payload & {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return *reinterpret_cast<const impl_payload *>(m_impl_storage.data());
    }

    HINDSIGHT_API_HIDDEN [[nodiscard]] auto impl() noexcept -> impl_payload & {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return *reinterpret_cast<impl_payload *>(m_impl_storage.data());
    }
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
    std::ranges::swap(m_symbol, other.m_symbol);
    std::ranges::swap(m_source, other.m_source);
}

#endif


class HINDSIGHT_API resolver {
public:
    explicit resolver();

#ifdef HINDSIGHT_OS_WINDOWS
    explicit resolver(HANDLE process);
#endif

    resolver(const resolver &other) = delete;

    resolver(resolver &&other) noexcept = default;

    ~resolver()
#ifndef HINDSIGHT_OS_WINDOWS
            = default
#endif
            ;

    auto operator=(const resolver &other) -> resolver & = delete;

    auto operator=(resolver &&other) noexcept -> resolver & = default;

    template<std::output_iterator<logical_stacktrace_entry> It, std::sentinel_for<It> Sentinel>
    [[nodiscard]] auto resolve(const stacktrace_entry entry, It first, Sentinel last)
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
        resolve_impl(
                entry,
                [](logical_stacktrace_entry &&logical, void *user_data) {
                    auto &state = *static_cast<cb_state *>(user_data);
                    *state.first++ = std::move(logical);
                    return state.first == state.last;
                },
                &state);

        if constexpr (std::forward_iterator<It>) {
            return std::move(state.first);
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
    using resolve_cb = bool(logical_stacktrace_entry &&logical, void *user_data);

    auto resolve_impl(stacktrace_entry entry, resolve_cb *callback, void *user_data) -> void;

#ifdef HINDSIGHT_OS_WINDOWS
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
