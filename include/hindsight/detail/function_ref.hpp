/*
 * Copyright 2024 Andrey Glebov
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

#ifndef HINDSIGHT_INCLUDE_HINDSIGHT_DETAIL_FUNCTION_REF_HPP
#define HINDSIGHT_INCLUDE_HINDSIGHT_DETAIL_FUNCTION_REF_HPP

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

namespace hindsight::detail {

// A minimal implementation if function_ref, without noexcept support, etc. (because we don't need it).
template<typename Signature>
class function_ref;

template<typename R, typename... Args>
class function_ref<R(Args...)> {
public:
    function_ref() = delete;

    function_ref(const function_ref &other) = default;
    auto operator=(const function_ref &rhs) -> function_ref & = default;

    ~function_ref() = default;

    template<typename Fn>
        requires requires {
            { std::declval<Fn>()(std::declval<Args>()...) } -> std::convertible_to<R>;
        }
    constexpr explicit(false) function_ref(Fn &&fn) noexcept
            : m_func{[](Args &&...args, void *const func_data) -> R {
                  return std::forward<Fn>(*static_cast<Fn *>(func_data))(std::forward<Args>(args)...);
              }},
              m_func_data{const_cast<void *>(static_cast<const void *>(std::addressof(fn)))} {}

    auto operator()(Args... args) const -> R { return m_func(std::forward<Args>(args)..., m_func_data); }

private:
    R (*m_func)(Args &&..., void *);
    void *m_func_data;
};

} // namespace hindsight::detail

#endif // HINDSIGHT_INCLUDE_HINDSIGHT_DETAIL_FUNCTION_REF_HPP
