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

#include <cstdlib>
#include <exception>

#ifdef HINDSIGHT_OS_WINDOWS
    #include <array>
    #include <cassert>
    #include <concepts>
    #include <cstddef>
    #include <functional>
    #include <string>
    #include <type_traits>
    #include <utility>

    #include <Windows.h>
#endif

#include <hindsight/stacktrace.hpp>

#include "common.hpp"

namespace hindsight::out_of_process::host {
namespace {

#ifdef HINDSIGHT_OS_WINDOWS

template<std::invocable Fn>
class finally {
public:
    template<typename FnArg>
        requires std::constructible_from<Fn, FnArg>
    explicit finally(FnArg &&fn) noexcept(std::is_nothrow_constructible_v<Fn, FnArg>) : m_fn{std::forward<FnArg>(fn)} {}

    finally(const finally &other) = delete;
    finally(finally &&other) = delete;

    ~finally() noexcept(std::is_nothrow_invocable_v<Fn &>) { std::invoke(m_fn); }

    auto operator=(const finally &other) -> finally & = delete;
    auto operator=(finally &&other) -> finally & = delete;

private:
    Fn m_fn;
};

template<std::invocable Fn>
explicit finally(Fn &&fn) -> finally<std::decay_t<Fn>>;

#endif

auto run() -> int try {
    auto watchdog_stdin_pipe = create_pipe();
    prevent_handle_inheritance(watchdog_stdin_pipe.write.get());

#ifdef HINDSIGHT_OS_WINDOWS
    const auto stderr_handle = GetStdHandle(STD_ERROR_HANDLE);
    if (stderr_handle == INVALID_HANDLE_VALUE) {
        throw_last_system_error("Failed to get the standard error handle for the current process");
    }
    if (stderr_handle == nullptr) {
        throw_runtime_error("The current process does not have a standard error handle");
    }
    // TODO: Create an inheritable handle if stderr_handle is not inheritable.

    const auto host_process_handle = [&] {
        auto handle = HANDLE{};
        if (!DuplicateHandle(GetCurrentProcess(),
                             GetCurrentProcess(),
                             GetCurrentProcess(),
                             &handle,
                             PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
                             TRUE,
                             {})) {
            throw_last_system_error("Failed to duplicate a handle to the current process");
        }
        return unique_os_handle{handle};
    }();

    auto startup_attribute_list_size = SIZE_T{};
    InitializeProcThreadAttributeList(nullptr, 1, {}, &startup_attribute_list_size);
    auto startup_attribute_list_buffer = std::vector<std::byte>(startup_attribute_list_size);
    auto *const startup_attribute_list =
            reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(startup_attribute_list_buffer.data());
    if (!InitializeProcThreadAttributeList(startup_attribute_list, 1, {}, &startup_attribute_list_size)) {
        throw_last_system_error("Failed to initialize the watchdog process attribute list");
    }
    const auto startup_attribute_list_guard =
            finally{[&]() noexcept { DeleteProcThreadAttributeList(startup_attribute_list); }};
    auto inherited_handles = std::array{watchdog_stdin_pipe.read.get(), stderr_handle, host_process_handle.get()};
    if (!UpdateProcThreadAttribute(startup_attribute_list,
                                   {},
                                   PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                   inherited_handles.data(),
                                   inherited_handles.size() * sizeof inherited_handles[0],
                                   nullptr,
                                   nullptr)) {
        throw_last_system_error("Failed to update the inherited handle list for the watchdog process");
    }

    auto command_line = std::wstring{L"out_of_process_watchdog.exe"};
    auto startup_info = STARTUPINFOEXW{};
    startup_info.StartupInfo.cb = sizeof startup_info;
    startup_info.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup_info.StartupInfo.hStdInput = watchdog_stdin_pipe.read.get();
    startup_info.StartupInfo.hStdOutput = INVALID_HANDLE_VALUE;
    startup_info.StartupInfo.hStdError = stderr_handle;
    startup_info.lpAttributeList = startup_attribute_list;
    auto watchdog_process_info = PROCESS_INFORMATION{};
    if (!CreateProcessW(nullptr,
                        command_line.data(),
                        nullptr,
                        nullptr,
                        TRUE,
                        CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT,
                        nullptr,
                        nullptr,
                        reinterpret_cast<STARTUPINFOW *>(&startup_info),
                        &watchdog_process_info)) {
        throw_last_system_error("Failed to create the watchdog process");
    }
    {
        [[maybe_unused]] const auto succeeded = CloseHandle(watchdog_process_info.hThread);
        assert(succeeded);
    }
    const auto watchdog_process = unique_os_handle{watchdog_process_info.hProcess};

    if (!write_to_handle(watchdog_stdin_pipe.write.get(), host_process_handle.get())) {
        throw_last_system_error("Failed to write the host process handle to the watchdog's standard input");
    }
    print_log("HOST: Written the host process handle to the watchdog's standard input ({})\n",
              host_process_handle.get());
#else
    #error HOST is not implemented for this OS
#endif

    const auto entries = hindsight::capture_stacktrace();
    print_log("HOST: Captured {} entries\n", entries.size());
    if (!write_to_handle(watchdog_stdin_pipe.write.get(), entries.size())) {
        throw_last_system_error("Failed to write the captured entry count to the watchdog's standard input");
    }
    {
        if (!write_to_handle(watchdog_stdin_pipe.write.get(), std::as_bytes(std::span{entries}))) {
            throw_last_system_error("Failed to write the captured entries to the watchdog's standard input");
        }
    }

#ifdef HINDSIGHT_OS_WINDOWS
    if (WaitForSingleObject(watchdog_process.get(), INFINITE) != WAIT_OBJECT_0) {
        throw_last_system_error("Failed to wait for the watchdog process to exit");
    }
    auto watchdog_exit_code = DWORD{};
    if (!GetExitCodeProcess(watchdog_process.get(), &watchdog_exit_code)) {
        throw_last_system_error("Failed to get the watchdog process exit code");
    }
    print_log("HOST: The watchdog process exited with code {}", watchdog_exit_code);
#endif
    return EXIT_SUCCESS;
} catch (const std::exception &e) {
    print_log("HOST: {}\n", e.what());
    return EXIT_FAILURE;
} catch (...) {
    print_log("HOST: <unknown exception>\n");
    return EXIT_FAILURE;
}

} // namespace
} // namespace hindsight::out_of_process::host

auto main() -> int { return hindsight::out_of_process::host::run(); }
