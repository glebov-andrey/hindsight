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

#ifndef HINDSIGHT_SRC_UTIL_FINALLY_HPP
#define HINDSIGHT_SRC_UTIL_FINALLY_HPP

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

namespace hindsight::util {

template<std::invocable Fn>
class finally {
public:
    template<typename FnArg>
        requires std::constructible_from<Fn, FnArg>
    explicit finally(FnArg &&fn) noexcept(std::is_nothrow_constructible_v<Fn, FnArg>) : m_fn{std::forward<FnArg>(fn)} {}

    finally(const finally &other) = delete;
    finally(finally &&other) = delete;

    ~finally() noexcept(std::is_nothrow_invocable_v<Fn &>) { std::invoke(m_fn); }

    auto operator=(const finally &other) -> finally & = delete;
    auto operator=(finally &&other) -> finally & = delete;

private:
    Fn m_fn;
};

template<std::invocable Fn>
explicit finally(Fn &&fn) -> finally<std::decay_t<Fn>>;

} // namespace hindsight::util

#endif // HINDSIGHT_SRC_UTIL_FINALLY_HPP
