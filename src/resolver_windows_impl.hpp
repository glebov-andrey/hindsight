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

#ifndef HINDSIGHT_SRC_RESOLVER_WINDOWS_IMPL_HPP
#define HINDSIGHT_SRC_RESOLVER_WINDOWS_IMPL_HPP

#include <hindsight/config.hpp>

#ifdef HINDSIGHT_OS_WINDOWS
    #include <hindsight/resolver.hpp>

    #include <unordered_map>

    #include <dia2.h>

    #include "util/locked.hpp"
    #include "windows/com.hpp"

namespace hindsight {

class resolver::impl {
public:
    auto resolve(stacktrace_entry entry, resolve_cb *callback, void *user_data) -> void;

private:
    using session_map = std::unordered_map<std::wstring, windows::com_ptr<IDiaSession>>;
    util::locked<session_map> m_sessions{};

    [[nodiscard]] auto session_for_entry(stacktrace_entry entry) -> windows::com_ptr<IDiaSession>;
};

} // namespace hindsight

#endif

#endif // HINDSIGHT_SRC_RESOLVER_WINDOWS_IMPL_HPP
