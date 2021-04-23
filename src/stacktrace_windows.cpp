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

#include <hindsight/config.hpp>

#ifdef HINDSIGHT_OS_WINDOWS

    #include <hindsight/stacktrace.hpp>

    #include <Windows.h>

namespace hindsight {

namespace {

[[nodiscard]] auto get_instruction_ptr(const CONTEXT &context) noexcept {
    #ifdef _M_IX86
    return context.Eip;
    #elif defined _M_AMD64
    return context.Rip;
    #elif defined _M_ARM || defined _M_ARM64
    return context.Pc;
    #else
        #error get_instruction_ptr is not implemented for this architecture.
    #endif
}

auto skip_leaf_function(CONTEXT &context) noexcept {
    #ifdef _M_AMD64
    context.Rip = *reinterpret_cast<const std::uintptr_t *>(context.Rsp);
    context.Rsp += sizeof(std::uintptr_t);
    #else
        #error skip_leaf_function is not implemented for this architecture
    #endif
}

auto capture_stack_trace_from_context(CONTEXT &context,
                                      std::size_t entries_to_skip,
                                      detail::capture_stack_trace_cb *const callback,
                                      void *const user_data) noexcept {
    while (get_instruction_ptr(context) != 0) {
        if (entries_to_skip == 0) {
            if (callback({from_native_handle, get_instruction_ptr(context) - 1}, user_data)) {
                break;
            }
        } else {
            --entries_to_skip;
        }

        auto image_base = ULONG_PTR{};
        auto *const function_entry = RtlLookupFunctionEntry(get_instruction_ptr(context), &image_base, nullptr);
        if (function_entry) {
            void *handler_data = nullptr;
            auto establisher_frame = ULONG_PTR{};
            RtlVirtualUnwind(UNW_FLAG_NHANDLER,
                             image_base,
                             get_instruction_ptr(context),
                             function_entry,
                             &context,
                             &handler_data,
                             &establisher_frame,
                             nullptr);
        } else {
            skip_leaf_function(context);
        }
    }
}

} // namespace

namespace detail {

auto capture_stack_trace_impl(const std::size_t entries_to_skip,
                              capture_stack_trace_cb *const callback,
                              void *const user_data) -> void {
    CONTEXT context;
    RtlCaptureContext(&context);
    capture_stack_trace_from_context(context, entries_to_skip, callback, user_data);
}

} // namespace detail

} // namespace hindsight

#endif
