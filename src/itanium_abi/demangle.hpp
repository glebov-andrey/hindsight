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

#ifndef HINDSIGHT_SRC_ITANIUM_ABI_DEMANGLE_HPP
#define HINDSIGHT_SRC_ITANIUM_ABI_DEMANGLE_HPP

#include <hindsight/detail/config.hpp>

#ifndef HINDSIGHT_OS_WINDOWS

    #include <cstdlib>
    #include <memory>
    #include <type_traits>

namespace hindsight::itanium_abi {

struct std_free_deleter {
    auto operator()(void *const ptr) const noexcept {
        std::free(ptr); // NOLINT(hicpp-no-malloc)
    }
};

template<typename T>
    requires std::is_trivially_destructible_v<std::remove_all_extents_t<T>>
using unique_freeable = std::unique_ptr<T, std_free_deleter>;


[[nodiscard]] auto demangle(const char *mangled) -> unique_freeable<char[]>;

} // namespace hindsight::itanium_abi

#endif

#endif // HINDSIGHT_SRC_ITANIUM_ABI_DEMANGLE_HPP
