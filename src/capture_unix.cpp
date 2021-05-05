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

#include <hindsight/capture.hpp>

#ifndef HINDSIGHT_OS_WINDOWS

    #include <libunwind.h>

namespace hindsight::detail {

auto capture_stacktrace_from_mutable_context(native_context_type &context,
                                             std::size_t entries_to_skip,
                                             capture_stacktrace_cb *const callback,
                                             void *const user_data) -> void {
    unw_cursor_t cursor;
    if (unw_init_local(&cursor, &context) != 0) {
        return;
    }

    do {
        auto instruction_ptr = unw_word_t{};
        if (unw_get_reg(&cursor, UNW_REG_IP, &instruction_ptr) == 0 && instruction_ptr != 0) {
            if (entries_to_skip != 0) {
                --entries_to_skip;
                continue;
            }

            if (unw_is_signal_frame(&cursor) <= 0) {
                // either not a signal frame or an error has occurred (probably, no info)
                --instruction_ptr;
            }
            if (callback({from_native_handle, instruction_ptr}, user_data)) {
                break;
            }
        }
    } while (unw_step(&cursor) > 0);
}

auto capture_stacktrace(const std::size_t entries_to_skip, capture_stacktrace_cb *const callback, void *const user_data)
        -> void {
    unw_context_t context;
    if (unw_getcontext(&context) != 0) {
        return;
    }
    capture_stacktrace_from_mutable_context(context, entries_to_skip, callback, user_data);
}


auto capture_stacktrace_from_context(const native_context_type &context,
                                     const std::size_t entries_to_skip,
                                     capture_stacktrace_cb *const callback,
                                     void *const user_data) -> void {
    auto context_copy = context;
    capture_stacktrace_from_mutable_context(context_copy, entries_to_skip, callback, user_data);
}

} // namespace hindsight::detail

#endif
