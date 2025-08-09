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

#ifndef HINDSIGHT_OS_WINDOWS

    #include <libunwind.h>

    // The "nongnu" libunwind implementation uses ucontext_t for unw_context_t directly on some architectures.
    // unw_init_local2 is defined as a macro in the "nongnu" libunwind implementation.
    #if defined unw_init_local2 && (defined __i386__ || defined __x86_64__ || defined __riscv)
        #define HINDSIGHT_LIBUNWIND_USES_UCONTEXT
    #endif

namespace hindsight::detail {

namespace {

[[nodiscard]] auto get_instruction_ptr(const mcontext_t &mcontext) noexcept -> unw_word_t {
    #ifdef __i386__
    const auto instruction_ptr = mcontext.gregs[REG_EIP];
    #elif defined __x86_64__
    const auto instruction_ptr = mcontext.gregs[REG_RIP];
    #elif defined __aarch64__
    const auto instruction_ptr = mcontext.pc;
    #elif defined __arm__
    const auto instruction_ptr = mcontext.arm_pc;
    #elif defined __riscv
    const auto instruction_ptr = mcontext.__gregs[REG_PC];
    #else
        #error get_instruction_ptr is not implemented for this architecture
    #endif
    return static_cast<unw_word_t>(instruction_ptr);
}

[[nodiscard]] auto should_skip_signal_frame(const native_signal_parameters &params) noexcept -> bool {
    const auto &mcontext = static_cast<const ucontext_t *>(params.context)->uc_mcontext;
    return params.signo == SIGSEGV &&
           reinterpret_cast<unw_word_t>(params.info->si_addr) == get_instruction_ptr(mcontext);
}

[[nodiscard]] auto skip_signal_frame(const mcontext_t &mcontext) noexcept -> unw_word_t {
    #ifdef __i386__
    return *reinterpret_cast<const volatile unw_word_t *>(mcontext.gregs[REG_ESP]);
    #elif defined __x86_64__
    return *reinterpret_cast<const volatile unw_word_t *>(mcontext.gregs[REG_RSP]);
    #else
        #error skip_signal_frame is not implemented for this architecture
    #endif
}

    #ifdef HINDSIGHT_LIBUNWIND_USES_UCONTEXT
auto skip_signal_frame(ucontext_t &context) noexcept -> void {
        #ifdef __i386__
    auto &gregs = context.uc_mcontext.gregs;
    gregs[REG_EIP] = static_cast<greg_t>(skip_signal_frame(context.uc_mcontext));
    gregs[REG_ESP] += sizeof(greg_t);
        #elif defined __x86_64__
    auto &gregs = context.uc_mcontext.gregs;
    gregs[REG_RIP] = static_cast<greg_t>(skip_signal_frame(context.uc_mcontext));
    gregs[REG_RSP] += sizeof(greg_t);
        #else
            #error skip_signal_frame is not implemented for this architecture
        #endif
}
    #endif

    #ifndef HINDSIGHT_LIBUNWIND_USES_UCONTEXT
[[nodiscard]] auto fill_cursor_from_mcontext(unw_cursor_t &cursor,
                                             const mcontext_t &mcontext,
                                             const bool should_skip) noexcept -> bool {
        #define SET_REG_EXPLICIT(unw_reg, value)                                                                       \
            if (unw_set_reg(&cursor, unw_reg, (value)) != 0) {                                                         \
                return false;                                                                                          \
            }
        #ifdef __x86_64__
    const auto &gregs = mcontext.gregs;
            #define SET_REG(reg) SET_REG_EXPLICIT(UNW_X86_64_##reg, static_cast<unw_word_t>(gregs[REG_##reg]))
    SET_REG(RAX)
    SET_REG(RDX)
    SET_REG(RCX)
    SET_REG(RBX)
    SET_REG(RSI)
    SET_REG(RDI)
    SET_REG(RBP)
    SET_REG(R8)
    SET_REG(R9)
    SET_REG(R10)
    SET_REG(R11)
    SET_REG(R12)
    SET_REG(R13)
    SET_REG(R14)
    SET_REG(R15)
    auto rip = gregs[REG_RIP];
    auto rsp = gregs[REG_RSP];
    if (should_skip) {
        rip = static_cast<greg_t>(skip_signal_frame(mcontext));
        rsp += sizeof(greg_t);
    }
    SET_REG_EXPLICIT(UNW_REG_SP, rsp)
    // UNW_REG_IP is special in LLVM's libunwind and must be set using UNW_REG_IP, not UNW_X86_64_RIP
    SET_REG_EXPLICIT(UNW_REG_IP, rip)
            #undef SET_REG
        #else
            #error fill_cursor_from_mcontext is not implemented for this architecture
        #endif
        #undef SET_REG_EXPLICIT
    return true;
}
    #endif

auto capture_stacktrace_from_cursor(unw_cursor_t &cursor,
                                    std::size_t entries_to_skip,
                                    bool is_signal_frame,
                                    const capture_stacktrace_cb callback) -> void {
    while (true) {
        auto instruction_ptr = unw_word_t{};
        if (unw_get_reg(&cursor, UNW_REG_IP, &instruction_ptr) == 0) {
            if (entries_to_skip == 0) {
                if (!is_signal_frame) { // unw_is_signal_frame() doesn't seem to actually work
                    --instruction_ptr; // not a signal frame so decrement to get the call instruction
                }
                if (callback({from_native_handle, instruction_ptr})) {
                    break;
                }
            } else {
                --entries_to_skip;
            }
        }
        if (unw_step(&cursor) <= 0) {
            break;
        }
        is_signal_frame = false;
    }
}

} // namespace

auto capture_stacktrace(std::size_t entries_to_skip, const capture_stacktrace_cb callback) -> void {
    unw_context_t context;
    if (unw_getcontext(&context) != 0) {
        return;
    }
    increment_if_has_noinline(entries_to_skip);
    unw_cursor_t cursor;
    if (unw_init_local(&cursor, &context) != 0) {
        return;
    }
    capture_stacktrace_from_cursor(cursor, entries_to_skip, false, callback);
}

    #ifdef HINDSIGHT_LIBUNWIND_USES_UCONTEXT

auto capture_stacktrace_from_context(const native_context_type &context,
                                     const std::size_t entries_to_skip,
                                     const capture_stacktrace_cb callback) -> void {
    auto context_copy = context;
    capture_stacktrace_from_mutable_context(context_copy, entries_to_skip, callback);
}

auto capture_stacktrace_from_mutable_context(native_context_type &context,
                                             const std::size_t entries_to_skip,
                                             const capture_stacktrace_cb callback) -> void {
    unw_cursor_t cursor;
    if (unw_init_local(&cursor, &context) != 0) {
        return;
    }
    capture_stacktrace_from_cursor(cursor, entries_to_skip, false, callback);
}

auto capture_stacktrace_from_signal(const native_signal_parameters params,
                                    const std::size_t entries_to_skip,
                                    const capture_stacktrace_cb callback) -> void {
    auto context_copy = *static_cast<const ucontext_t *>(params.context);
    auto is_signal_frame = true;
    if (should_skip_signal_frame(params)) {
        skip_signal_frame(context_copy);
        is_signal_frame = false;
    }
    unw_cursor_t cursor;
    if (unw_init_local2(&cursor, &context_copy, is_signal_frame ? UNW_INIT_SIGNAL_FRAME : 0) != 0) {
        return;
    }
    capture_stacktrace_from_cursor(cursor, entries_to_skip, is_signal_frame, callback);
}

    #else

auto capture_stacktrace_from_context(const native_context_type &context,
                                     const std::size_t entries_to_skip,
                                     const capture_stacktrace_cb callback) -> void {
    unw_context_t unw_context;
    if (unw_getcontext(&unw_context) != 0) {
        return;
    }
    unw_cursor_t cursor;
    if (unw_init_local(&cursor, &unw_context) != 0) {
        return;
    }
    if (!fill_cursor_from_mcontext(cursor, context.uc_mcontext, false)) {
        return;
    }
    capture_stacktrace_from_cursor(cursor, entries_to_skip, false, callback);
}

auto capture_stacktrace_from_mutable_context(native_context_type &context,
                                             const std::size_t entries_to_skip,
                                             const capture_stacktrace_cb callback) -> void {
    capture_stacktrace_from_context(context, entries_to_skip, callback);
}

auto capture_stacktrace_from_signal(const native_signal_parameters params,
                                    const std::size_t entries_to_skip,
                                    const capture_stacktrace_cb callback) -> void {
    unw_context_t unw_context;
    if (unw_getcontext(&unw_context) != 0) {
        return;
    }
    unw_cursor_t cursor;
    if (unw_init_local(&cursor, &unw_context) != 0) {
        return;
    }
    const auto &context = *static_cast<const ucontext_t *>(params.context);
    const auto should_skip = should_skip_signal_frame(params);
    if (!fill_cursor_from_mcontext(cursor, context.uc_mcontext, should_skip)) {
        return;
    }
    capture_stacktrace_from_cursor(cursor, entries_to_skip, !should_skip, callback);
}

    #endif

} // namespace hindsight::detail

#endif
