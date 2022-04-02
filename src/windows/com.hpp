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

#ifndef HINDSIGHT_SRC_WINDOWS_COM_HPP
#define HINDSIGHT_SRC_WINDOWS_COM_HPP

#include <hindsight/detail/config.hpp>

#ifdef HINDSIGHT_OS_WINDOWS

    #include <cassert>
    #include <concepts>
    #include <cstddef>
    #include <utility>

    #include <Unknwn.h>

namespace hindsight::windows {

template<std::derived_from<IUnknown> T>
class com_ptr {
public:
    constexpr com_ptr() noexcept = default;

    constexpr explicit(false) com_ptr(std::nullptr_t) noexcept : com_ptr{} {} // NOLINT(hicpp-explicit-conversions)

    constexpr explicit com_ptr(T *const ptr) noexcept : m_ptr{ptr} {}

    constexpr com_ptr(const com_ptr &other) noexcept : m_ptr{other.m_ptr} {
        if (m_ptr) {
            m_ptr->AddRef();
        }
    }

    constexpr com_ptr(com_ptr &&other) noexcept : m_ptr{std::exchange(other.m_ptr, nullptr)} {}

    template<std::derived_from<T> U> // NOLINTNEXTLINE(hicpp-explicit-conversions)
    constexpr explicit(false) com_ptr(com_ptr<U> &&other) noexcept : m_ptr{std::exchange(other.m_ptr, nullptr)} {}

    constexpr ~com_ptr() { reset(); }

    constexpr auto operator=(const com_ptr &other) noexcept -> com_ptr & {
        com_ptr{other}.swap(*this);
        return *this;
    }

    constexpr auto operator=(com_ptr &&other) noexcept -> com_ptr & {
        com_ptr{std::move(other)}.swap(*this);
        return *this;
    }

    template<std::derived_from<T> U>
    constexpr auto operator=(com_ptr<U> &&other) noexcept -> com_ptr & {
        com_ptr{std::move(other)}.swap(*this);
        return *this;
    }

    constexpr auto operator=(std::nullptr_t) noexcept -> com_ptr & {
        reset();
        return *this;
    }

    constexpr auto swap(com_ptr &other) noexcept { std::swap(m_ptr, other.m_ptr); }

    friend constexpr auto swap(com_ptr &lhs, com_ptr &rhs) noexcept { lhs.swap(rhs); }

    constexpr auto release() noexcept -> T * { return std::exchange(m_ptr, nullptr); }

    constexpr auto reset(T *const ptr = nullptr) noexcept -> void {
        if (auto *const prev_ptr = std::exchange(m_ptr, ptr); prev_ptr) {
            prev_ptr->Release();
        }
    }

    [[nodiscard]] constexpr auto get() const noexcept -> T * { return m_ptr; }

    [[nodiscard]] constexpr explicit operator bool() const noexcept { return m_ptr != nullptr; }

    [[nodiscard]] constexpr auto operator*() const noexcept -> T & {
        assert(m_ptr);
        return *m_ptr;
    }

    [[nodiscard]] constexpr auto operator->() const noexcept -> T * {
        assert(m_ptr);
        return m_ptr;
    }

    // NOLINTNEXTLINE(google-runtime-operator)
    [[nodiscard]] constexpr auto operator&() noexcept -> T ** { return &m_ptr; }

private:
    T *m_ptr = nullptr;
};

} // namespace hindsight::windows

#endif

#endif // HINDSIGHT_SRC_WINDOWS_COM_HPP
