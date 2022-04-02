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

#include <hindsight/detail/config.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>

#ifdef HINDSIGHT_OS_WINDOWS
    #include <stdexcept>

    #include <Windows.h>
#elif defined HINDSIGHT_OS_UNIX
    #include <system_error>

    #include <errno.h>
    #include <signal.h>
    #include <ucontext.h>
#endif

#include <catch2/catch.hpp>

#include <hindsight/capture.hpp>

#include "util/finally.hpp"

namespace hindsight {

void *volatile null_pointer = nullptr;

namespace {

template<typename T, std::size_t N>
[[nodiscard]] constexpr auto sig_begin(T (&array)[N]) noexcept -> T * {
    return &array[0];
}

template<typename T, std::size_t N>
[[nodiscard]] constexpr auto sig_end(T (&array)[N]) noexcept -> T * {
    return &array[0] + N;
}


constexpr auto max_stacktrace_capacity = std::size_t{32};
stacktrace_entry signal_entries[max_stacktrace_capacity]{};
stacktrace_entry *last_captured_signal_entry = nullptr;

auto do_violation = true;
native_context_type pre_violation_context;

auto reset_signal_stacktrace() noexcept {
    std::ranges::fill(signal_entries, stacktrace_entry{});
    last_captured_signal_entry = nullptr;
}

#ifdef HINDSIGHT_OS_WINDOWS
    #define HINDSIGHT_TESTS_GET_CONTEXT(context) RtlCaptureContext(&(context))
#else
    #define HINDSIGHT_TESTS_GET_CONTEXT(context) getcontext(&(context))
#endif

auto raise_access_violation() {
    std::atomic_signal_fence(std::memory_order::release);
#ifdef HINDSIGHT_OS_WINDOWS
    RaiseException(EXCEPTION_ACCESS_VIOLATION, {}, 0, nullptr);
#elif defined HINDSIGHT_OS_UNIX
    if (raise(SIGSEGV) != 0) {
        throw std::system_error{errno, std::system_category(), "Failed to raise SIGSEGV"};
    }
#endif
    std::atomic_signal_fence(std::memory_order::acquire);
}

template<bool TryExecute>
auto cause_access_violation() {
    do_violation = true;
    HINDSIGHT_TESTS_GET_CONTEXT(pre_violation_context);
    std::atomic_signal_fence(std::memory_order::acquire); // synchronize with a signal handler that clears do_violation

    if (do_violation) {
        std::atomic_signal_fence(std::memory_order::release);
        if constexpr (TryExecute) {
            reinterpret_cast<void (*)()>(null_pointer)();
        } else {
            *static_cast<int *>(null_pointer) = 0;
        }
        std::atomic_signal_fence(std::memory_order::acquire);
    }
}

template<bool RollbackContext>
[[nodiscard]] auto register_signal_handler() {
#ifdef HINDSIGHT_OS_WINDOWS
    const auto handler =
            AddVectoredExceptionHandler(TRUE, [](EXCEPTION_POINTERS *const exception_info) noexcept -> LONG {
                std::atomic_signal_fence(std::memory_order::acquire);
                last_captured_signal_entry = capture_stacktrace_from_context(*exception_info->ContextRecord,
                                                                             sig_begin(signal_entries),
                                                                             sig_end(signal_entries));
                if constexpr (RollbackContext) {
                    *exception_info->ContextRecord = pre_violation_context;
                    do_violation = false;
                }
                std::atomic_signal_fence(std::memory_order::release);
                return EXCEPTION_CONTINUE_EXECUTION;
            });
    if (!handler) {
        throw std::runtime_error{"Failed to add the exception handler"};
    }
    return util::finally{[handler] {
        if (!RemoveVectoredExceptionHandler(handler)) {
            throw std::runtime_error{"Failed to remove the exception handler"};
        }
    }};
#elif defined HINDSIGHT_OS_UNIX
    struct sigaction sig_action {};
    sig_action.sa_sigaction = [](int /* signo */, siginfo_t * /* info */, void *const context_ptr) noexcept {
        auto &context = *static_cast<native_context_type *>(context_ptr);
        std::atomic_signal_fence(std::memory_order::acquire);
        last_captured_signal_entry =
                capture_stacktrace_from_context(context, sig_begin(signal_entries), sig_end(signal_entries));
        std::atomic_signal_fence(std::memory_order::release);

        if constexpr (RollbackContext) {
            do_violation = false;
            setcontext(&pre_violation_context);
        }
    };
    sig_action.sa_flags = SA_SIGINFO;
    struct sigaction old_sig_action {};
    if (sigaction(SIGSEGV, &sig_action, &old_sig_action) != 0) {
        throw std::system_error{errno, std::system_category(), "Failed to set the SIGSEGV handler"};
    }
    return util::finally{[old_sig_action] {
        if (sigaction(SIGSEGV, &old_sig_action, nullptr) != 0) {
            throw std::system_error{errno, std::system_category(), "Failed to reset the SIGSEGV handler"};
        }
    }};
#endif
}

auto check_signal_stacktrace() {
    REQUIRE(last_captured_signal_entry > sig_begin(signal_entries));
    REQUIRE(last_captured_signal_entry <= sig_end(signal_entries));
    REQUIRE(std::ranges::all_of(sig_begin(signal_entries), last_captured_signal_entry, [](const auto entry) {
        return entry != stacktrace_entry{};
    }));
}

} // namespace

TEST_CASE("capture_stacktrace_from_context can capture a stacktrace from a signal-frame context (raise)") {
    {
        const auto guard = register_signal_handler<false>();
        reset_signal_stacktrace();
        raise_access_violation();
    }
    check_signal_stacktrace();
}

TEST_CASE("capture_stacktrace_from_context can capture a stacktrace from a signal-frame context (write at nullptr)") {
    {
        const auto guard = register_signal_handler<true>();
        reset_signal_stacktrace();
        cause_access_violation<false>();
    }
    check_signal_stacktrace();
}

TEST_CASE("capture_stacktrace_from_context can capture a stacktrace from a signal-frame context (execute nullptr)") {
    {
        const auto guard = register_signal_handler<true>();
        reset_signal_stacktrace();
        cause_access_violation<true>();
    }
    check_signal_stacktrace();
}

} // namespace hindsight
