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

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include <hindsight/config.hpp>
#include <hindsight/stacktrace.hpp>

namespace hindsight {

template<typename Char>
struct basic_source_location {
    std::basic_string<Char> file_name;
    std::uint_least32_t line_number;
};

using source_location = basic_source_location<char>;
using u8_source_location = basic_source_location<char8_t>;


class logical_stacktrace_entry {
public:
    logical_stacktrace_entry() noexcept;

    logical_stacktrace_entry(const logical_stacktrace_entry &other);
    logical_stacktrace_entry(logical_stacktrace_entry &&other) noexcept;

    ~logical_stacktrace_entry();

    auto operator=(const logical_stacktrace_entry &other) -> logical_stacktrace_entry &;
    auto operator=(logical_stacktrace_entry &&other) noexcept -> logical_stacktrace_entry &;

    [[nodiscard]] auto physical() const noexcept -> stacktrace_entry { return m_physical; }

    [[nodiscard]] auto is_inline() const noexcept -> bool { return m_is_inline; }

    [[nodiscard]] auto symbol() const -> std::string;
    [[nodiscard]] auto u8_symbol() const -> std::u8string;

    [[nodiscard]] auto source() const -> source_location;
    [[nodiscard]] auto u8_source() const -> u8_source_location;

#ifdef HINDSIGHT_OS_WINDOWS
    struct impl_payload;
    logical_stacktrace_entry(stacktrace_entry physical, bool is_inline, impl_payload &&impl) noexcept;
#else
    struct impl_tag;
    logical_stacktrace_entry(impl_tag &&impl,
                             stacktrace_entry physical,
                             bool is_inline,
                             std::string &&symbol,
                             source_location &&source) noexcept;
    auto set_inline(impl_tag && /* impl */) noexcept -> void { m_is_inline = true; }
#endif

private:
    stacktrace_entry m_physical{};
    bool m_is_inline{};

#ifdef HINDSIGHT_OS_WINDOWS
    static constexpr auto impl_payload_size = sizeof(void *) * 2;
    std::array<std::byte, impl_payload_size> m_impl_storage;

    [[nodiscard]] auto impl() const noexcept -> const impl_payload & {
        return *reinterpret_cast<const impl_payload *>(m_impl_storage.data());
    }

    [[nodiscard]] auto impl() noexcept -> impl_payload & {
        return *reinterpret_cast<impl_payload *>(m_impl_storage.data());
    }
#else
    std::string m_symbol;
    source_location m_source;
#endif
};

#ifndef HINDSIGHT_OS_WINDOWS

inline auto logical_stacktrace_entry::symbol() const -> std::string { return m_symbol; }
inline auto logical_stacktrace_entry::source() const -> source_location { return m_source; }

#endif


class resolver {
public:
    explicit resolver();

    resolver(const resolver &other) = delete;

    resolver(resolver &&other) noexcept = default;

    ~resolver();

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

private:
    // Returns true if done
    using resolve_cb = bool(logical_stacktrace_entry &&logical, void *user_data);

    auto resolve_impl(stacktrace_entry entry, resolve_cb *callback, void *user_data) -> void;

#ifdef HINDSIGHT_OS_WINDOWS
    class impl;
    std::unique_ptr<impl> m_impl;
#endif
};

} // namespace hindsight

#endif // HINDSIGHT_INCLUDE_HINDSIGHT_RESOLVER_HPP
