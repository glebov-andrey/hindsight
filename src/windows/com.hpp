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
#include <OleAuto.h>
#include <Unknwn.h>
#include <wtypes.h>

namespace hindsight::windows {

template<std::derived_from<IUnknown> T>
class com_ptr {
public:
    constexpr com_ptr() noexcept = default;

    explicit(false) constexpr com_ptr(std::nullptr_t) noexcept : com_ptr{} {}

    explicit constexpr com_ptr(T *const ptr) noexcept : m_ptr{ptr} {}

    template<std::derived_from<T> U>
    explicit(false) constexpr com_ptr(com_ptr<U> &&other) noexcept : m_ptr{std::exchange(other.m_ptr, nullptr)} {}

    constexpr com_ptr(const com_ptr &other) noexcept : m_ptr{other.m_ptr} {
        if (m_ptr) {
            m_ptr->AddRef();
        }
    }

    constexpr com_ptr(com_ptr &&other) noexcept : m_ptr{std::exchange(other.m_ptr, nullptr)} {}

    constexpr ~com_ptr() { reset(); }

    constexpr auto operator=(const com_ptr &other) noexcept -> com_ptr & {
        auto tmp = other;
        swap(*this, tmp);
        return *this;
    }

    constexpr auto operator=(com_ptr &&other) noexcept -> com_ptr & {
        auto tmp = std::move(other);
        swap(*this, tmp);
        return *this;
    }

    template<std::derived_from<T> U>
    constexpr auto operator=(com_ptr<U> &&other) noexcept -> com_ptr & {
        auto tmp = com_ptr{std::move(other)};
        swap(*this, tmp);
        return *this;
    }

    constexpr auto operator=(std::nullptr_t) noexcept -> com_ptr & {
        reset();
        return *this;
    }

    constexpr auto release() noexcept -> T * { return std::exchange(m_ptr, nullptr); }

    constexpr auto reset(T *const ptr = nullptr) noexcept {
        if (const auto prev_ptr = std::exchange(m_ptr, ptr); prev_ptr) {
            prev_ptr->Release();
        }
    }

    [[nodiscard]] constexpr auto get() const noexcept -> T * { return m_ptr; }

    [[nodiscard]] explicit constexpr operator bool() const noexcept { return m_ptr != nullptr; }

    [[nodiscard]] constexpr auto operator*() const noexcept -> T & {
        assert(m_ptr);
        return *m_ptr;
    }

    [[nodiscard]] constexpr auto operator->() const noexcept -> T * {
        assert(m_ptr);
        return m_ptr;
    }

    [[nodiscard]] constexpr auto operator&() noexcept -> T ** { return &m_ptr; }

    friend constexpr auto swap(com_ptr &lhs, com_ptr &rhs) noexcept { std::ranges::swap(lhs.m_ptr, rhs.m_ptr); }

private:
    T *m_ptr = nullptr;
};


class bstr {
public:
    constexpr bstr() noexcept = default;

    bstr(const bstr &other) = delete;

    constexpr bstr(bstr &&other) noexcept : m_ptr{std::exchange(other.m_ptr, nullptr)} {}

    ~bstr() noexcept { SysFreeString(m_ptr); }

    auto operator=(const bstr &other) -> bstr & = delete;

    auto operator=(bstr &&other) noexcept -> bstr & {
        auto tmp = std::move(other);
        swap(*this, tmp);
        return *this;
    }

    [[nodiscard]] constexpr auto data() const noexcept -> const wchar_t * { return m_ptr; }

    [[nodiscard]] auto size() const noexcept -> std::size_t { return SysStringLen(m_ptr); }

    [[nodiscard]] auto empty() const noexcept -> bool { return size() == 0; }

    [[nodiscard]] explicit(false) operator std::wstring_view() const noexcept { return {data(), size()}; }

    [[nodiscard]] constexpr auto operator&() noexcept -> BSTR * { return &m_ptr; }

    friend constexpr auto swap(bstr &lhs, bstr &rhs) noexcept -> void { std::ranges::swap(lhs.m_ptr, rhs.m_ptr); }

private:
    BSTR m_ptr = nullptr;
};

} // namespace hindsight::windows

#endif // HINDSIGHT_SRC_WINDOWS_COM_HPP
