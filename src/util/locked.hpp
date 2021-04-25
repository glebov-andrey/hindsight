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

#ifndef HINDSIGHT_SRC_UTIL_LOCKED_HPP
#define HINDSIGHT_SRC_UTIL_LOCKED_HPP

#include <concepts>
#include <functional>
#include <mutex>
#include <type_traits>
#include <utility>

namespace hindsight::util {

template<typename Lock>
concept basic_lockable = requires(Lock &lock) {
    lock.lock();
    lock.unlock(); // Throws no exceptions, but can't be noexcept because std::mutex::unlock() isn't.
};

template<typename Lock>
concept nothrow_lockable = basic_lockable<Lock> && requires(Lock &lock) {
    { lock.lock() }
    noexcept;
    // lock.unlock() throws no exceptions anyway
};


template<typename T, basic_lockable Lock = std::mutex>
class locked {
public:
    template<typename... Args>
        requires std::constructible_from<T, Args...>
    constexpr explicit locked(Args &&...args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
            : m_value(std::forward<Args>(args)...) {}

    locked(const locked &other) = delete;
    locked(locked &&other) = delete;

    ~locked() = default;

    auto operator=(const locked &rhs) = delete;
    auto operator=(locked &&rhs) = delete;

    template<std::invocable<const T &> Fn>
    auto with_lock(Fn &&fn) const noexcept(nothrow_lockable<Lock> &&std::is_nothrow_invocable_v<Fn, const T &>)
            -> decltype(auto) {
        const auto guard = std::lock_guard{m_lock};
        return std::invoke(std::forward<Fn>(fn), m_value);
    }

    template<std::invocable<T &> Fn>
    auto with_lock(Fn &&fn) noexcept(nothrow_lockable<Lock> &&std::is_nothrow_invocable_v<Fn, T &>) -> decltype(auto) {
        const auto guard = std::lock_guard{m_lock};
        return std::invoke(std::forward<Fn>(fn), m_value);
    }

private:
    mutable Lock m_lock{};
    T m_value;
};

} // namespace hindsight::util

#endif // HINDSIGHT_SRC_UTIL_LOCKED_HPP
