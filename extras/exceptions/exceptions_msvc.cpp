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

#define _VCRT_ALLOW_INTERNALS
#ifdef __clang__
    #define _ThrowInfo ThrowInfo
#endif

#include "exceptions.hpp"

#include <cassert>
#include <cstring>
#include <iterator>
#include <memory>
#include <unordered_map>
#include <vector>

#include <Windows.h>

#include <ehdata.h>
#include <ehdata4.h>
#include <trnsctrl.h>
// Windows.h, ehdata.h and ehdata4.h must be included before ehhelpers.h
#include <ehhelpers.h>

// Windows.h must be included before detours.h
#include <detours.h>

#include <hindsight/capture.hpp>

namespace hindsight {

namespace {

constinit decltype(_CxxThrowException) *original_CxxThrowException = nullptr;
constinit decltype(__DestructExceptionObject) *original_DestructExceptionObject = nullptr;

constinit decltype(__ExceptionPtrDestroy) *original_ExceptionPtrDestroy = nullptr;
constinit decltype(__ExceptionPtrCurrentException) *original_ExceptionPtrCurrentException = nullptr;
constinit decltype(__ExceptionPtrRethrow) *original_ExceptionPtrRethrow = nullptr;

[[nodiscard]] auto get_instruction_ptr(const CONTEXT &context) noexcept { // TODO use hindsight internals
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

auto find_DestructExceptionObject() noexcept {
    struct trace_on_destruct {
        std::uintptr_t &fn_addr;

        ~trace_on_destruct() {
            CONTEXT context;
            RtlCaptureContext(&context);
#ifdef _DEBUG
            constexpr auto back_trace_count = std::size_t{2};
#else
            constexpr auto back_trace_count = std::size_t{1};
#endif
            for (auto i = std::size_t{0}; i != back_trace_count; ++i) {
                auto image_base = ULONG_PTR{};
                auto *const function_entry = RtlLookupFunctionEntry(get_instruction_ptr(context), &image_base, nullptr);
                assert(function_entry);
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
            }

            auto image_base = ULONG_PTR{};
            const auto *const function_entry =
                    RtlLookupFunctionEntry(get_instruction_ptr(context), &image_base, nullptr);
            assert(function_entry);
            fn_addr = image_base + function_entry->BeginAddress;
        }
    };

    auto fn_addr = std::uintptr_t{};
    try {
        throw trace_on_destruct{fn_addr};
    } catch (...) {
    }
    assert(fn_addr != 0);
    return reinterpret_cast<decltype(__DestructExceptionObject) *>(fn_addr);
}

auto stack_trace_map = std::unordered_map<void *, std::vector<stacktrace_entry>>{};

__declspec(noreturn) void __stdcall detour_CxxThrowException(void *const pExceptionObject,
                                                             _ThrowInfo *const pThrowInfo) {
    const auto [entry, inserted] = stack_trace_map.try_emplace(pExceptionObject);
    assert(inserted);
    capture_stacktrace(std::back_inserter(entry->second), std::unreachable_sentinel);

    original_CxxThrowException(pExceptionObject, pThrowInfo);
}

void __cdecl detour_DestructExceptionObject(EHExceptionRecord *const pExcept, const BOOLEAN fThrowNotAllowed) {
    if (pExcept == nullptr || !PER_IS_MSVC_EH(pExcept)) {
        return;
    }
    stack_trace_map.erase(pExcept->params.pExceptionObject);

    original_DestructExceptionObject(pExcept, fThrowNotAllowed);
}

using ex_ptr_impl = std::shared_ptr<const EXCEPTION_RECORD>;

[[nodiscard]] auto to_impl(void *const ex_ptr) noexcept -> ex_ptr_impl & { return *static_cast<ex_ptr_impl *>(ex_ptr); }

[[nodiscard]] auto to_impl(const void *const ex_ptr) noexcept -> const ex_ptr_impl & {
    return *static_cast<const ex_ptr_impl *>(ex_ptr);
}

[[nodiscard]] auto eh_record_from_base(const EXCEPTION_RECORD &base) noexcept -> EHExceptionRecord {
    // Can't use std::bit_cast because the sizes don't match
    EHExceptionRecord record;
    static_assert(sizeof(EHExceptionRecord) <= sizeof(EXCEPTION_RECORD));
    std::memcpy(&record, &base, sizeof(EHExceptionRecord));
    return record;
}

void __CLRCALL_PURE_OR_CDECL detour_ExceptionPtrDestroy(void *const ex_ptr) noexcept {
    const auto &ptr = to_impl(ex_ptr);
    if (ptr.use_count() == 1) { // safe because we know there aren't any weak_ptr
        if (ex_ptr) {
            const auto record = eh_record_from_base(*ptr);
            stack_trace_map.erase(record.params.pExceptionObject);
        }
    }

    original_ExceptionPtrDestroy(ex_ptr);
}

void __CLRCALL_PURE_OR_CDECL detour_ExceptionPtrCurrentException(void *const ex_ptr) noexcept {
    original_ExceptionPtrCurrentException(ex_ptr);

    auto &ptr = to_impl(ex_ptr);
    if (!ptr) {
        return;
    }

    const auto *const record = _pCurrentException;
    assert(record);
    const auto original_entry = stack_trace_map.find(record->params.pExceptionObject);
    if (original_entry == stack_trace_map.end()) {
        return;
    }

    const auto ptr_record = eh_record_from_base(*ptr);
    assert(ptr_record.params.pExceptionObject);
    stack_trace_map.try_emplace(ptr_record.params.pExceptionObject, original_entry->second);
}

[[noreturn]] void __CLRCALL_PURE_OR_CDECL detour_ExceptionPtrRethrow(const void *ex_ptr) {
    const auto &ptr = to_impl(ex_ptr);
    assert(ptr);
    const auto record = eh_record_from_base(*ptr);
    const auto original_entry = stack_trace_map.find(record.params.pExceptionObject);
    if (original_entry == stack_trace_map.end()) {
        original_ExceptionPtrRethrow(ex_ptr);
    }
    try {
        original_ExceptionPtrRethrow(ex_ptr);
    } catch (...) {
        const auto *const rethrow_record = _pCurrentException;
        assert(rethrow_record);
        [[maybe_unused]] const auto [it, inserted] =
                stack_trace_map.try_emplace(rethrow_record->params.pExceptionObject, original_entry->second);
        assert(inserted);

        throw;
    }
}

} // namespace

auto enable_stack_traces_from_exceptions() -> bool {
    original_CxxThrowException = _CxxThrowException;
    original_DestructExceptionObject = find_DestructExceptionObject();
    original_ExceptionPtrDestroy = __ExceptionPtrDestroy;
    original_ExceptionPtrCurrentException = __ExceptionPtrCurrentException;
    original_ExceptionPtrRethrow = __ExceptionPtrRethrow;

    if (DetourTransactionBegin() != NO_ERROR) {
        return false;
    }
    if (DetourUpdateThread(GetCurrentThread()) != NO_ERROR) {
        DetourTransactionAbort();
        return false;
    }
    if (DetourAttach(&original_CxxThrowException, detour_CxxThrowException) != NO_ERROR) {
        DetourTransactionAbort();
        return false;
    }
    if (DetourAttach(&original_DestructExceptionObject, detour_DestructExceptionObject) != NO_ERROR) {
        DetourTransactionAbort();
        return false;
    }
    if (DetourAttach(&original_ExceptionPtrDestroy, detour_ExceptionPtrDestroy) != NO_ERROR) {
        DetourTransactionAbort();
        return false;
    }
    if (DetourAttach(&original_ExceptionPtrCurrentException, detour_ExceptionPtrCurrentException) != NO_ERROR) {
        DetourTransactionAbort();
        return false;
    }
    if (DetourAttach(&original_ExceptionPtrRethrow, detour_ExceptionPtrRethrow) != NO_ERROR) {
        DetourTransactionAbort();
        return false;
    }
    if (DetourTransactionCommit() != NO_ERROR) {
        return false;
    }
    return true;
}

auto stack_trace_from_current_exception() noexcept -> std::span<const stacktrace_entry> {
    const auto *const record = _pCurrentException;
    if (!record) {
        return {};
    }
    const auto entry = stack_trace_map.find(record->params.pExceptionObject);
    if (entry == stack_trace_map.end()) {
        return {};
    }
    return entry->second;
}

auto stack_trace_from_exception(const std::exception_ptr &ex) noexcept -> std::span<const stacktrace_entry> {
    const auto &ptr = to_impl(&ex);
    if (!ptr) {
        return {};
    }

    const auto record = eh_record_from_base(*ptr);
    const auto entry = stack_trace_map.find(record.params.pExceptionObject);
    if (entry == stack_trace_map.end()) {
        return {};
    }
    return entry->second;
}

} // namespace hindsight
