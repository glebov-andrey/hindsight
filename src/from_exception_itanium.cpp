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

#include <hindsight/from_exception.hpp>

#include <atomic>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstring>

#include <cxxabi.h>
#include <dlfcn.h>

#include <hindsight/capture.hpp>
#include <hindsight/stacktrace_entry.hpp>

#include "util/finally.hpp"

// The following implementation is based on Boost.Stacktrace.

// Copyright Antony Polukhin, 2023-2026.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// https://github.com/boostorg/stacktrace/blob/2eadf2b8df03ed0e6094417764d069109053b3c3/src/exception_headers.h

#if defined(__x86_64__) || defined(_M_X64) || defined(__MINGW32__)
    #define HINDSIGHT_FROM_EXCEPTION_ALWAYS_STORE_IN_PADDING 1
#else
    #define HINDSIGHT_FROM_EXCEPTION_ALWAYS_STORE_IN_PADDING 0
#endif

namespace hindsight {
namespace {

// Developer note: helper to experiment with layouts of different
// exception headers https://godbolt.org/z/rrcdPbh1P

// https://github.com/llvm/llvm-project/blob/b3dd14ce07f2750ae1068fe62abbf2f3bd2cade8/libcxxabi/src/cxa_exception.h
struct cxa_exception_begin_llvm {
    const void *reserve;
    std::size_t referenceCount;
};

cxa_exception_begin_llvm *exception_begin_llvm_ptr(void *ptr) {
    constexpr std::size_t exception_begin_offset = sizeof(void *) == 8 ? 128 : 80;
    return reinterpret_cast<cxa_exception_begin_llvm *>(static_cast<std::byte *>(ptr) - exception_begin_offset);
}

// https://github.com/gcc-mirror/gcc/blob/5d2a360f0a541646abb11efdbabc33c6a04de7ee/libstdc%2B%2B-v3/libsupc%2B%2B/unwind-cxx.h#L100
struct cxa_exception_begin_gcc {
    std::size_t referenceCount;
    const void *reserve;
};

cxa_exception_begin_gcc *exception_begin_gcc_ptr(void *ptr) {
    constexpr std::size_t exception_begin_offset = sizeof(void *) == 8 ? 128 : 96;
    return reinterpret_cast<cxa_exception_begin_gcc *>(static_cast<std::byte *>(ptr) - exception_begin_offset);
}

void *get_current_exception_raw_ptr(const void *exc_ptr) {
    // https://github.com/gcc-mirror/gcc/blob/16e2427f50c208dfe07d07f18009969502c25dc8/libstdc%2B%2B-v3/libsupc%2B%2B/eh_ptr.cc#L147
    return *static_cast<void *const *>(exc_ptr);
}

} // namespace
} // namespace hindsight

// https://github.com/boostorg/stacktrace/blob/2eadf2b8df03ed0e6094417764d069109053b3c3/src/from_exception.cpp

#if !HINDSIGHT_FROM_EXCEPTION_ALWAYS_STORE_IN_PADDING
    #include <mutex>
    #include <optional>
    #include <unordered_map>
#endif

#if defined(__GNUC__) && defined(__ELF__)
    #define HINDSIGHT_FROM_EXCEPTION_MAY_BE_LIBCXX_RUNTIME
namespace __cxxabiv1 {
// libc++-runtime specific function
// https://github.com/llvm/llvm-project/blob/bcf0cd177d79935537bdebb3313d85c0ea207890/libcxxabi/include/cxxabi.h#L174
extern "C" [[gnu::visibility("default"), gnu::weak]] void
__cxa_increment_exception_refcount(void *primary_exception) noexcept; // NOLINT(*-reserved-identifier)
} // namespace __cxxabiv1
#endif

namespace hindsight {
namespace {

[[nodiscard]] auto is_libcxx_runtime() noexcept -> bool {
#ifdef HINDSIGHT_FROM_EXCEPTION_MAY_BE_LIBCXX_RUNTIME
    return __cxxabiv1::__cxa_increment_exception_refcount != nullptr;
#else
    return false;
#endif
}

[[nodiscard]] auto reference_to_empty_padding(void *const ptr) noexcept -> const void *& {
    if (is_libcxx_runtime()) {
        assert(sizeof(void *) != 4 && "32bit platforms are unsupported with libc++ runtime padding reuse. "
                                      "Please report this issue to the library maintainers.");
        return exception_begin_llvm_ptr(ptr)->reserve;
    }

    return exception_begin_gcc_ptr(ptr)->reserve;
}

template<std::unsigned_integral Uint>
[[nodiscard]] constexpr auto align_offset_up(const Uint offset, const Uint alignment) noexcept -> Uint {
    assert(alignment % 2 == 0);
    return (offset + alignment - 1) & ~(alignment - 1);
}

constinit std::atomic g_stacktrace_from_exceptions_enabled{false};

[[nodiscard]] auto stacktrace_from_exceptions_enabled() noexcept -> bool {
    return g_stacktrace_from_exceptions_enabled.load(std::memory_order::relaxed);
}

#if !HINDSIGHT_FROM_EXCEPTION_ALWAYS_STORE_IN_PADDING
constinit std::mutex g_exception_to_trace_map_mutex{};
constinit std::optional<std::unordered_map<void *, const void *>> g_exception_to_trace_map{};
#endif

} // namespace
} // namespace hindsight

namespace __cxxabiv1 {

extern "C" HINDSIGHT_API_EXPORT HINDSIGHT_NOINLINE void *
__cxa_allocate_exception(const std::size_t thrown_size) noexcept {
    static const auto orig_allocate_exception = [] {
        void *const ptr = dlsym(RTLD_NEXT, "__cxa_allocate_exception");
        assert(ptr && "Failed to find '__cxa_allocate_exception'");
        return reinterpret_cast<decltype(__cxa_allocate_exception) *>(ptr);
    }();

    if (!hindsight::stacktrace_from_exceptions_enabled()) {
        return orig_allocate_exception(thrown_size);
    }

    // This protects against the case when something inside our __cxa_allocate_exception implementation throws (i.e.
    // std::mutex or std::unordered_map), causing __cxa_allocate_exception to be called recursively.
    constinit thread_local auto inside_allocate_exception = false;
    if (inside_allocate_exception) {
        return orig_allocate_exception(thrown_size);
    }
    inside_allocate_exception = true;
    const auto inside_allocate_exception_guard = hindsight::util::finally{[&] { inside_allocate_exception = false; }};

    const auto stacktrace_size_offset = hindsight::align_offset_up(thrown_size, alignof(std::size_t));
    const auto stacktrace_offset = hindsight::align_offset_up(stacktrace_size_offset + sizeof(std::size_t),
                                                              alignof(hindsight::stacktrace_entry));

    static constexpr std::size_t initial_stacktrace_capacity = 128;
    static constexpr std::size_t max_stacktrace_capacity = 16384;
    void *allocated_ptr = nullptr;
    std::size_t stacktrace_capacity = 0;
    std::size_t stacktrace_size = 0;
    const auto reallocate_stacktrace = [&](const std::size_t new_capacity) noexcept {
        assert(new_capacity > stacktrace_capacity && new_capacity <= max_stacktrace_capacity);
        const auto new_alloc_size = stacktrace_offset + sizeof(hindsight::stacktrace_entry) * new_capacity;
        void *const new_ptr = orig_allocate_exception(new_alloc_size);
        assert(new_ptr);
        if (stacktrace_size != 0) {
            assert(allocated_ptr != nullptr);
            const std::byte *old_stacktrace_ptr = static_cast<const std::byte *>(allocated_ptr) + stacktrace_offset;
            std::byte *const new_stacktrace_ptr = static_cast<std::byte *>(new_ptr) + stacktrace_offset;
            std::memcpy(new_stacktrace_ptr, old_stacktrace_ptr, stacktrace_size * sizeof(hindsight::stacktrace_entry));
        }
        if (allocated_ptr != nullptr) {
            __cxa_free_exception(allocated_ptr);
        }
        allocated_ptr = new_ptr;
        stacktrace_capacity = new_capacity;
    };

    reallocate_stacktrace(initial_stacktrace_capacity);
    hindsight::detail::capture_stacktrace(1, [&](const hindsight::stacktrace_entry entry) noexcept -> bool {
        if (stacktrace_size == stacktrace_capacity) {
            if (stacktrace_capacity == max_stacktrace_capacity) {
                return true;
            }
            reallocate_stacktrace(stacktrace_capacity * 2);
        }
        assert(allocated_ptr);
        assert(stacktrace_size < stacktrace_capacity);
        auto *const stacktrace_ptr = reinterpret_cast<hindsight::stacktrace_entry *>(
                static_cast<std::byte *>(allocated_ptr) + stacktrace_offset);
        stacktrace_ptr[stacktrace_size] = entry;
        stacktrace_size++;
        return false;
    });
    *reinterpret_cast<std::size_t *>(static_cast<std::byte *>(allocated_ptr) + stacktrace_size_offset) =
            stacktrace_size;

    const void *dump_ptr = static_cast<std::byte *>(allocated_ptr) + stacktrace_size_offset;

#if !HINDSIGHT_FROM_EXCEPTION_ALWAYS_STORE_IN_PADDING
    if (hindsight::is_libcxx_runtime()) {
        try {
            const auto guard = std::lock_guard{hindsight::g_exception_to_trace_map_mutex};
            if (!hindsight::g_exception_to_trace_map.has_value()) {
                hindsight::g_exception_to_trace_map.emplace();
            }
            [[maybe_unused]] const auto inserted =
                    hindsight::g_exception_to_trace_map->try_emplace(allocated_ptr, dump_ptr).second;
            assert(inserted);
        } catch (...) {
            // Ignore errors
        }
    } else
#endif
    {
        auto &padding = hindsight::reference_to_empty_padding(allocated_ptr);
        assert(padding == nullptr && "Padding not zeroed out, unsupported implementation");
        padding = dump_ptr;
    }

    return allocated_ptr;
}

#if !HINDSIGHT_FROM_EXCEPTION_ALWAYS_STORE_IN_PADDING

// __cxa_free_exception is not called in libc++ as the __cxa_decrement_exception_refcount has an inlined call to
// __cxa_free_exception.
// Overriding libc++ specific function
extern "C" HINDSIGHT_API_EXPORT HINDSIGHT_NOINLINE void
__cxa_decrement_exception_refcount(void *const thrown_object) noexcept { // NOLINT(*-reserved-identifier)
    assert(hindsight::is_libcxx_runtime());

    if (!thrown_object) {
        return;
    }

    static const auto orig_decrement_refcount = [] {
        void *const ptr = dlsym(RTLD_NEXT, "__cxa_decrement_exception_refcount");
        assert(ptr && "Failed to find '__cxa_decrement_exception_refcount'");
        return reinterpret_cast<decltype(__cxa_decrement_exception_refcount) *>(ptr);
    }();

    const auto *const exception_header = hindsight::exception_begin_llvm_ptr(thrown_object);

    // The following line has a race and could give false positives and false negatives.
    // In first case we remove the trace earlier, in the second case we get a memory leak.
    if (std::atomic_ref{exception_header->referenceCount}.load(std::memory_order::relaxed) == 1) {
        // The only thing that can throw here is std::mutex - in which case std::terminate (via noexcept) is fine.
        const auto guard = std::lock_guard{hindsight::g_exception_to_trace_map_mutex};
        if (hindsight::g_exception_to_trace_map.has_value()) {
            hindsight::g_exception_to_trace_map->erase(thrown_object);
        }
    }

    orig_decrement_refcount(thrown_object);
}

#endif

} // namespace __cxxabiv1

namespace hindsight {

auto enable_stacktrace_from_exceptions() -> bool {
#if !HINDSIGHT_FROM_EXCEPTION_ALWAYS_STORE_IN_PADDING
    if (is_libcxx_runtime()) {
        // Memory leaks may occur if capturing stacktrace from exceptions is enabled and exceptions are thrown
        // concurrently by libc++ runtime (libc++abi).
        return false; // TODO Perhaps let the user allow memory leaks?
    }
#endif
    g_stacktrace_from_exceptions_enabled.store(true, std::memory_order::relaxed);
    return true;
}

auto disable_stacktrace_from_exceptions() -> void {
    g_stacktrace_from_exceptions_enabled.store(false, std::memory_order::relaxed);
}

auto stacktrace_from_current_exception() noexcept -> std::span<const stacktrace_entry> {
    const auto ex = std::current_exception();
    return stacktrace_from_exception(ex);
}

auto stacktrace_from_exception(const std::exception_ptr &ex) noexcept -> std::span<const stacktrace_entry> {
    void *const exc_raw_ptr = get_current_exception_raw_ptr(&ex);
    if (!exc_raw_ptr) {
        return {};
    }
    const void *dump_ptr = [&]() -> const void * {
#if !HINDSIGHT_FROM_EXCEPTION_ALWAYS_STORE_IN_PADDING
        if (is_libcxx_runtime()) {
            // The only thing that can throw here is std::mutex - in which case std::terminate (via noexcept) is fine.
            const auto guard = std::lock_guard{g_exception_to_trace_map_mutex};
            if (g_exception_to_trace_map.has_value()) {
                if (const auto it = g_exception_to_trace_map->find(exc_raw_ptr);
                    it != g_exception_to_trace_map->end()) {
                    return it->second;
                }
            }
            return nullptr;
        }
#endif
        return reference_to_empty_padding(exc_raw_ptr);
    }();
    if (!dump_ptr) {
        return {};
    }
    const auto size = *static_cast<const std::size_t *>(dump_ptr);
    constexpr auto first_entry_offset = align_offset_up(sizeof(std::size_t), alignof(stacktrace_entry));
    const auto *const entries =
            reinterpret_cast<const stacktrace_entry *>(static_cast<const std::byte *>(dump_ptr) + first_entry_offset);
    return {entries, size};
}

} // namespace hindsight
