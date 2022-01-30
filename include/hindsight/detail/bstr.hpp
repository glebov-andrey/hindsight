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

#ifndef HINDSIGHT_INCLUDE_HINDSIGHT_DETAIL_BSTR_HPP
#define HINDSIGHT_INCLUDE_HINDSIGHT_DETAIL_BSTR_HPP

#include <hindsight/config.hpp>

#ifdef HINDSIGHT_OS_WINDOWS

    #include <cstddef>
    #include <cstdint>
    #include <string_view>
    #include <utility>

using BSTR = wchar_t *;

namespace hindsight::detail {

class HINDSIGHT_API bstr {
public:
    bstr() noexcept = default;

    bstr(const bstr &other);

    bstr(bstr &&other) noexcept : m_ptr{std::exchange(other.m_ptr, nullptr)} {}

    ~bstr();

    auto operator=(const bstr &other) -> bstr & {
        auto tmp = other;
        swap(*this, tmp);
        return *this;
    }

    auto operator=(bstr &&other) noexcept -> bstr & {
        auto tmp = std::move(other);
        swap(*this, tmp);
        return *this;
    }

    [[nodiscard]] auto data() const noexcept -> const wchar_t * { return m_ptr; }

    [[nodiscard]] auto size() const noexcept -> std::size_t {
        if (!m_ptr) {
            return 0;
        }
        return *reinterpret_cast<const std::uint32_t *>(reinterpret_cast<const std::byte *>(m_ptr) -
                                                        sizeof(std::uint32_t)) /
               2;
    }

    [[nodiscard]] auto empty() const noexcept -> bool { return size() == 0; }

    // NOLINTNEXTLINE(hicpp-explicit-conversions)
    [[nodiscard]] explicit(false) operator std::wstring_view() const noexcept { return {data(), size()}; }

    [[nodiscard]] auto out_ptr() noexcept -> wchar_t ** { return &m_ptr; }

    friend auto swap(bstr &lhs, bstr &rhs) noexcept -> void { std::swap(lhs.m_ptr, rhs.m_ptr); }

private:
    BSTR m_ptr = nullptr;
};

} // namespace hindsight::detail

#endif

#endif // HINDSIGHT_INCLUDE_HINDSIGHT_DETAIL_BSTR_HPP
