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

#include <cassert>
#include <concepts>
#include <cstddef>
#include <string_view>
#include <utility>

#include <Windows.h>
// Windows.h must be included before other headers
#include <Unknwn.h>
#include <oleauto.h>
#include <wtypes.h>

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


class bstr {
public:
    constexpr bstr() noexcept = default;

    bstr(const bstr &other) = delete;

    constexpr bstr(bstr &&other) noexcept : m_ptr{std::exchange(other.m_ptr, nullptr)} {}

    ~bstr() {
        // clang-tidy (as of 12.0.0) incorrectly assumes that the default constructor does not write to m_ptr even
        // though it has a default member initializer.
        SysFreeString(m_ptr); // NOLINT(clang-analyzer-core.CallAndMessage)
    }

    auto operator=(const bstr &other) -> bstr & = delete;

    auto operator=(bstr &&other) noexcept -> bstr & {
        auto tmp = std::move(other);
        swap(*this, tmp);
        return *this;
    }

    [[nodiscard]] constexpr auto data() const noexcept -> const wchar_t * { return m_ptr; }

    [[nodiscard]] auto size() const noexcept -> std::size_t { return SysStringLen(m_ptr); }

    [[nodiscard]] auto empty() const noexcept -> bool { return size() == 0; }

    // NOLINTNEXTLINE(hicpp-explicit-conversions)
    [[nodiscard]] explicit(false) operator std::wstring_view() const noexcept { return {data(), size()}; }

    // NOLINTNEXTLINE(google-runtime-operator)
    [[nodiscard]] constexpr auto operator&() noexcept -> BSTR * { return &m_ptr; }

    friend constexpr auto swap(bstr &lhs, bstr &rhs) noexcept -> void { std::swap(lhs.m_ptr, rhs.m_ptr); }

private:
    BSTR m_ptr = nullptr;
};

} // namespace hindsight::windows

#endif // HINDSIGHT_SRC_WINDOWS_COM_HPP
