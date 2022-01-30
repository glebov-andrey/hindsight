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

#include "demangle.hpp"

#ifndef HINDSIGHT_OS_WINDOWS

    #include <new>

    #include <cxxabi.h>

namespace hindsight::itanium_abi {

auto demangle(const char *const mangled) -> unique_freeable<char[]> {
    auto status = 0;
    auto demangled = unique_freeable<char[]>{__cxxabiv1::__cxa_demangle(mangled, nullptr, nullptr, &status)};
    switch (status) {
        case 0: // The demangling operation succeeded.
            return demangled;
        case -1: // A memory allocation failure occurred.
            throw std::bad_alloc{};
        default:
            // -2: mangled_name is not a valid name under the C++ ABI mangling rules.
            // -3: One of the arguments is invalid.
            break;
    }
    return nullptr;
}

} // namespace hindsight::itanium_abi

#endif
