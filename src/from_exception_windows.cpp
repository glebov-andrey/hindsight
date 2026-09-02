/*
 * Copyright 2026 Andrey Glebov
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

#define _VCRT_ALLOW_INTERNALS // NOLINT(*-reserved-identifier)
#ifdef __clang__
    #define _ThrowInfo ThrowInfo // NOLINT(*-reserved-identifier)
#endif

#include <hindsight/from_exception.hpp>

#include <atomic>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>

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

constinit std::atomic g_stacktrace_from_exceptions_enabled{false};

[[nodiscard]] auto stacktrace_from_exceptions_enabled() noexcept -> bool {
    return g_stacktrace_from_exceptions_enabled.load(std::memory_order::relaxed);
}

struct stacktrace_storage {
    std::atomic<std::size_t> ref_count{1};
    std::size_t size = 0;
    HINDSIGHT_PRAGMA_CLANG("clang diagnostic push")
    HINDSIGHT_PRAGMA_CLANG("clang diagnostic ignored \"-Wzero-length-array\"") // zero size arrays are an extension
    HINDSIGHT_PRAGMA_MSVC("warning(push)")
    HINDSIGHT_PRAGMA_MSVC("warning(disable: 4200)") // nonstandard extension used: zero-sized array in struct/union
    stacktrace_entry entries[0];
    HINDSIGHT_PRAGMA_MSVC("warning(pop)")
    HINDSIGHT_PRAGMA_CLANG("clang diagnostic pop")
};

class stacktrace_storage_ptr {
public:
    stacktrace_storage_ptr() noexcept = default;

    explicit(false) stacktrace_storage_ptr(std::nullptr_t) noexcept {}

    explicit stacktrace_storage_ptr(stacktrace_storage *const ptr) noexcept : m_ptr{ptr} {}

    stacktrace_storage_ptr(const stacktrace_storage_ptr &other) noexcept : m_ptr{other.m_ptr} {
        if (m_ptr) {
            m_ptr->ref_count.fetch_add(1, std::memory_order::relaxed);
        }
    }

    stacktrace_storage_ptr(stacktrace_storage_ptr &&other) noexcept : m_ptr{std::exchange(other.m_ptr, nullptr)} {}

    ~stacktrace_storage_ptr() { reset(); }

    auto operator=(const stacktrace_storage_ptr &other) noexcept -> stacktrace_storage_ptr & {
        auto tmp = other;
        swap(*this, tmp);
        return *this;
    }

    auto operator=(stacktrace_storage_ptr &&other) noexcept -> stacktrace_storage_ptr & {
        auto tmp = std::move(other);
        swap(*this, tmp);
        return *this;
    }

    [[nodiscard]] explicit operator bool() const noexcept { return m_ptr != nullptr; }

    [[nodiscard]] auto operator->() const noexcept -> stacktrace_storage * { return m_ptr; }

    auto reset() noexcept -> void {
        if (m_ptr) {
            if (m_ptr->ref_count.fetch_sub(1, std::memory_order::acq_rel) == 1) {
                std::free(m_ptr);
            }
            m_ptr = nullptr;
        }
    }

    friend auto swap(stacktrace_storage_ptr &lhs, stacktrace_storage_ptr &rhs) noexcept -> void {
        std::swap(lhs.m_ptr, rhs.m_ptr);
    }

private:
    stacktrace_storage *m_ptr = nullptr;
};

[[nodiscard]] stacktrace_storage_ptr make_stacktrace_storage(const std::size_t capacity) noexcept {
    const auto total_bytes = sizeof(stacktrace_storage) + sizeof(stacktrace_entry) * capacity;
    void *const ptr = std::malloc(total_bytes);
    if (!ptr) {
        return nullptr;
    }
    return stacktrace_storage_ptr{::new (ptr) stacktrace_storage};
}

constinit std::mutex g_exception_to_trace_map_mutex{};
constinit std::optional<std::unordered_map<void *, stacktrace_storage_ptr>> g_exception_to_trace_map{};
// TODO thread_local stack for exceptions' stacktraces before being captured by std::current_exception()

constinit decltype(_CxxThrowException) *original_CxxThrowException = nullptr;
constinit decltype(__DestructExceptionObject) *original_DestructExceptionObject = nullptr;

constinit decltype(__ExceptionPtrDestroy) *original_ExceptionPtrDestroy = nullptr;
constinit decltype(__ExceptionPtrCurrentException) *original_ExceptionPtrCurrentException = nullptr;
constinit decltype(__ExceptionPtrRethrow) *original_ExceptionPtrRethrow = nullptr;

[[nodiscard]] auto get_instruction_ptr(const CONTEXT &context) noexcept { // TODO Deduplicate function
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
        throw trace_on_destruct{fn_addr}; // NOLINT(*-exception-baseclass)
    } catch (...) {
    }
    assert(fn_addr != 0);
    return reinterpret_cast<decltype(__DestructExceptionObject) *>(fn_addr);
}

__declspec(noreturn) void __stdcall detour_CxxThrowException(void *const pExceptionObject,
                                                             _ThrowInfo *const pThrowInfo) {
    if (stacktrace_from_exceptions_enabled()) {
        static constexpr std::size_t initial_stacktrace_capacity = 128;
        static constexpr std::size_t max_stacktrace_capacity = 16384;
        auto stacktrace = stacktrace_storage_ptr{};
        std::size_t stacktrace_capacity = 0;
        std::size_t stacktrace_size = 0;
        const auto reallocate_stacktrace = [&](const std::size_t new_capacity) noexcept -> bool {
            assert(new_capacity > stacktrace_capacity && new_capacity <= max_stacktrace_capacity);
            auto new_storage = make_stacktrace_storage(new_capacity);
            if (!new_storage) {
                return false;
            }
            if (stacktrace_size != 0) {
                assert(stacktrace);
                std::memcpy(new_storage->entries, stacktrace->entries, stacktrace_size * sizeof(stacktrace_entry));
            }
            stacktrace = std::move(new_storage);
            stacktrace_capacity = new_capacity;
            return true;
        };

        reallocate_stacktrace(initial_stacktrace_capacity);
        detail::capture_stacktrace(1, [&](const stacktrace_entry entry) -> bool {
            if (stacktrace_size == stacktrace_capacity) {
                if (stacktrace_capacity == max_stacktrace_capacity) {
                    return true;
                }
                if (!reallocate_stacktrace(stacktrace_capacity * 2)) {
                    return true;
                }
            }
            assert(stacktrace);
            assert(stacktrace_size < stacktrace_capacity);
            stacktrace->entries[stacktrace_size] = entry;
            stacktrace_size++;
            return false;
        });
        if (stacktrace_size != 0) {
            assert(stacktrace);
            stacktrace->size = stacktrace_size;
            try {
                const auto guard = std::lock_guard{g_exception_to_trace_map_mutex};
                if (!g_exception_to_trace_map.has_value()) {
                    g_exception_to_trace_map.emplace();
                }
                [[maybe_unused]] const auto inserted =
                        g_exception_to_trace_map->try_emplace(pExceptionObject, std::move(stacktrace)).second;
                assert(inserted);
            } catch (...) {
                // Ignore errors
            }
        }
    }

    original_CxxThrowException(pExceptionObject, pThrowInfo);
}

void __cdecl detour_DestructExceptionObject(EHExceptionRecord *const pExcept, const BOOLEAN fThrowNotAllowed) {
    HINDSIGHT_PRAGMA_CLANG("clang diagnostic push")
    HINDSIGHT_PRAGMA_CLANG("clang diagnostic ignored \"-Wmultichar\"")
    if (pExcept == nullptr || !PER_IS_MSVC_EH(pExcept)) {
        return;
    }
    HINDSIGHT_PRAGMA_CLANG("clang diagnostic pop")

    try {
        const auto guard = std::lock_guard{g_exception_to_trace_map_mutex};
        if (g_exception_to_trace_map.has_value()) {
            g_exception_to_trace_map->erase(pExcept->params.pExceptionObject);
        }
    } catch (...) {
        // The only thing that can throw here is std::mutex - in which case std::terminate is fine.
        std::terminate();
    }

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
    assert(ex_ptr);
    const auto &ptr = to_impl(ex_ptr);
    if (ptr.use_count() == 1) { // safe because we know there aren't any weak_ptr
        const auto record = eh_record_from_base(*ptr);
        { // The only thing that can throw here is std::mutex - in which case std::terminate (via noexcept) is fine.
            const auto guard = std::lock_guard{g_exception_to_trace_map_mutex};
            if (g_exception_to_trace_map.has_value()) {
                g_exception_to_trace_map->erase(record.params.pExceptionObject);
            }
        }
    }

    original_ExceptionPtrDestroy(ex_ptr);
}

void __CLRCALL_PURE_OR_CDECL detour_ExceptionPtrCurrentException(void *const ex_ptr) noexcept {
    original_ExceptionPtrCurrentException(ex_ptr);

    const auto &ptr = to_impl(ex_ptr);
    if (!ptr) {
        return;
    }

    try {
        const auto guard = std::lock_guard{g_exception_to_trace_map_mutex};
        if (!g_exception_to_trace_map.has_value()) {
            return;
        }
        const auto *const original_record = _pCurrentException;
        assert(original_record);
        const auto original_entry = g_exception_to_trace_map->find(original_record->params.pExceptionObject);
        if (original_entry == g_exception_to_trace_map->end()) {
            return;
        }
        assert(original_entry->second);
        const auto ptr_record = eh_record_from_base(*ptr);
        assert(ptr_record.params.pExceptionObject);
        [[maybe_unused]] const auto inserted =
                g_exception_to_trace_map->try_emplace(ptr_record.params.pExceptionObject, original_entry->second)
                        .second;
        assert(inserted);
    } catch (...) {
        // Ignore errors
    }
}

[[noreturn]] void __CLRCALL_PURE_OR_CDECL detour_ExceptionPtrRethrow(const void *ex_ptr) {
    assert(ex_ptr);
    const auto &ptr = to_impl(ex_ptr);
    if (!ptr) {
        original_ExceptionPtrRethrow(ex_ptr); // Let the original handle the error (UB)
        HINDSIGHT_UNREACHABLE;
    }

    const auto original_record = eh_record_from_base(*ptr);

    auto stacktrace = stacktrace_storage_ptr{};
    try {
        const auto guard = std::lock_guard{g_exception_to_trace_map_mutex};
        if (g_exception_to_trace_map.has_value()) {
            const auto original_entry = g_exception_to_trace_map->find(original_record.params.pExceptionObject);
            if (original_entry != g_exception_to_trace_map->end()) {
                stacktrace = original_entry->second;
                assert(stacktrace);
            }
        }
    } catch (...) {
        // Propagating the exception here would break std::rethrow_exception's guarantees.
    }

    if (!stacktrace) { // avoid the added overhead of try/catch/throw
        original_ExceptionPtrRethrow(ex_ptr);
        HINDSIGHT_UNREACHABLE;
    }

    try {
        original_ExceptionPtrRethrow(ex_ptr);
        HINDSIGHT_UNREACHABLE;
    } catch (...) {
        const auto *const rethrow_record = _pCurrentException;
        assert(rethrow_record);
        try {
            const auto guard = std::lock_guard{g_exception_to_trace_map_mutex};
            assert(g_exception_to_trace_map.has_value());
            [[maybe_unused]] const auto inserted =
                    g_exception_to_trace_map->try_emplace(rethrow_record->params.pExceptionObject, stacktrace).second;
            assert(inserted);
        } catch (...) {
            // Propagating the exception here would break std::rethrow_exception's guarantees.
        }
        throw;
    }
}

} // namespace

auto enable_stacktrace_from_exceptions() -> bool {
    if (g_stacktrace_from_exceptions_enabled.load(std::memory_order::relaxed)) {
        return true;
    }
    if (original_CxxThrowException != nullptr) { // already detoured
        g_stacktrace_from_exceptions_enabled.store(true, std::memory_order::relaxed);
        return true;
    }

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
    g_stacktrace_from_exceptions_enabled.store(true, std::memory_order::relaxed);
    return true;
}

auto disable_stacktrace_from_exceptions() -> void {
    g_stacktrace_from_exceptions_enabled.store(false, std::memory_order::relaxed);
}

auto stacktrace_from_current_exception() noexcept -> std::span<const stacktrace_entry> {
    const auto *const record = _pCurrentException;
    if (!record) {
        return {};
    }
    { // The only thing that can throw here is std::mutex - in which case std::terminate (via noexcept) is fine.
        const auto guard = std::lock_guard{g_exception_to_trace_map_mutex};
        if (g_exception_to_trace_map.has_value()) {
            const auto entry = g_exception_to_trace_map->find(record->params.pExceptionObject);
            if (entry != g_exception_to_trace_map->end()) {
                const auto &storage = entry->second;
                assert(storage);
                return {storage->entries, storage->size};
            }
        }
    }
    return {};
}

auto stacktrace_from_exception(const std::exception_ptr &ex) noexcept -> std::span<const stacktrace_entry> {
    const auto &ptr = to_impl(&ex);
    if (!ptr) {
        return {};
    }
    const auto record = eh_record_from_base(*ptr);
    { // The only thing that can throw here is std::mutex - in which case std::terminate (via noexcept) is fine.
        const auto guard = std::lock_guard{g_exception_to_trace_map_mutex};
        if (g_exception_to_trace_map.has_value()) {
            const auto entry = g_exception_to_trace_map->find(record.params.pExceptionObject);
            if (entry != g_exception_to_trace_map->end()) {
                const auto &storage = entry->second;
                assert(storage);
                return {storage->entries, storage->size};
            }
        }
    }
    return {};
}

} // namespace hindsight
