/*
 * Copyright 2025 Andrey Glebov
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

#ifdef HINDSIGHT_OS_WINDOWS

    #include <Windows.h>

namespace hindsight::detail {

namespace {

[[nodiscard]] auto get_instruction_ptr(const CONTEXT &context) noexcept {
    #ifdef _M_IX86
    return context.Eip;
    #elif defined _M_AMD64
    return context.Rip;
    #elif defined _M_ARM || defined _M_ARM64
    return context.Pc;
    #else
        #error get_instruction_ptr is not implemented for this architecture
    #endif
}

auto skip_leaf_function(CONTEXT &context) noexcept {
    #ifdef _M_AMD64
    // Making the load from `*Rsp` volatile because the memory it references doesn't really exist in the abstract
    // machine, and so the compiler can't reason about it.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast, performance-no-int-to-ptr)
    context.Rip = *reinterpret_cast<const volatile std::uintptr_t *>(context.Rsp);
    context.Rsp += sizeof(std::uintptr_t);
    #else
        #error skip_leaf_function is not implemented for this architecture
    #endif
}

auto capture_stacktrace_impl(native_context_type &context,
                             std::size_t entries_to_skip,
                             bool is_signal_frame,
                             const capture_stacktrace_cb callback) -> void {
    do {
        if (entries_to_skip == 0) {
            auto instruction_ptr = get_instruction_ptr(context);
            if (!is_signal_frame) {
                --instruction_ptr;
            }
            if (callback({from_native_handle, instruction_ptr})) {
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
        is_signal_frame = false;
    } while (get_instruction_ptr(context) != 0);
}

} // namespace

auto capture_stacktrace(std::size_t entries_to_skip, const capture_stacktrace_cb callback) -> void {
    CONTEXT context;
    RtlCaptureContext(&context);
    increment_if_has_noinline(entries_to_skip);
    capture_stacktrace_impl(context, entries_to_skip, false, callback);
}

auto capture_stacktrace_from_context(const native_context_type &context,
                                     const std::size_t entries_to_skip,
                                     const capture_stacktrace_cb callback) -> void {
    auto context_copy = context;
    capture_stacktrace_impl(context_copy, entries_to_skip, false, callback);
}

auto capture_stacktrace_from_mutable_context(native_context_type &context,
                                             const std::size_t entries_to_skip,
                                             const capture_stacktrace_cb callback) -> void {
    capture_stacktrace_impl(context, entries_to_skip, false, callback);
}

auto capture_stacktrace_from_signal(const native_signal_parameters params,
                                    const std::size_t entries_to_skip,
                                    const capture_stacktrace_cb callback) -> void {
    auto context_copy = *params->ContextRecord;
    auto is_signal_frame = true;
    if (params->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
        params->ExceptionRecord->NumberParameters >= 2 &&
        params->ExceptionRecord->ExceptionInformation[1] == get_instruction_ptr(context_copy)) {
        skip_leaf_function(context_copy);
        is_signal_frame = false;
    }
    capture_stacktrace_impl(context_copy, entries_to_skip, is_signal_frame, callback);
}

} // namespace hindsight::detail

#endif
