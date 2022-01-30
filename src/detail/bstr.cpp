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

#include <hindsight/detail/bstr.hpp>

#ifdef HINDSIGHT_OS_WINDOWS

    #include <new>

    #include <Windows.h>
    // Windows.h needs to be included before oleauto.h
    #include <oleauto.h>

namespace hindsight::detail {

bstr::bstr(const bstr &other)
        : m_ptr{other.m_ptr ? SysAllocStringLen(other.m_ptr, SysStringLen(other.m_ptr)) : nullptr} {
    if (other.m_ptr && !m_ptr) {
        throw std::bad_alloc{};
    }
}

bstr::~bstr() {
    // clang-tidy (as of 12.0.0) incorrectly assumes that the default constructor does not write to m_ptr even
    // though it has a default member initializer.
    SysFreeString(m_ptr); // NOLINT(clang-analyzer-core.CallAndMessage)
}

} // namespace hindsight::detail

#endif
