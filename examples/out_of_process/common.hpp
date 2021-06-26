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

#ifndef HINDSIGHT_EXAMPLES_OUT_OF_PROCESS_COMMON_HPP
#define HINDSIGHT_EXAMPLES_OUT_OF_PROCESS_COMMON_HPP

#include <hindsight/config.hpp>

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <utility>
#ifdef HINDSIGHT_OS_UNIX
    #include <array>
    #include <cerrno>
#endif

#ifdef HINDSIGHT_OS_WINDOWS
    #include <Windows.h>
#elif defined HINDSIGHT_OS_UNIX
    #include <fcntl.h>
    #include <unistd.h>
#endif

#include <fmt/format.h>

namespace hindsight::out_of_process {

#ifdef HINDSIGHT_OS_WINDOWS
using os_handle = HANDLE;
#else
class os_handle {
public:
    [[nodiscard]] os_handle() = default;

    [[nodiscard]] constexpr explicit(false) os_handle(int descriptor) noexcept : m_descriptor{descriptor} {}

    [[nodiscard]] constexpr explicit(false) operator int() const noexcept { return m_descriptor; }

    [[nodiscard]] constexpr friend auto operator==(os_handle lhs, os_handle rhs) -> bool = default;

    [[nodiscard]] constexpr friend auto operator==(const os_handle lhs, std::nullptr_t /* rhs */) {
        return lhs.m_descriptor == invalid_handle_value;
    }

private:
    static constexpr auto invalid_handle_value = -1;
    int m_descriptor{invalid_handle_value};
};
#endif

struct close_os_handle {
    using pointer = os_handle;

    auto operator()(const os_handle handle) const noexcept {
#ifdef HINDSIGHT_OS_WINDOWS
        [[maybe_unused]] const auto succeeded = CloseHandle(handle);
        assert(succeeded);
#elif defined HINDSIGHT_OS_LINUX // Only Linux because on other platforms EINTR may need to be handled
        [[maybe_unused]] const auto succeeded = close(handle) == 0;
        assert(succeeded || errno == EINTR);
#else
    #error close_os_handle is not implemented for this OS
#endif
    }
};

using unique_os_handle = std::unique_ptr<os_handle, close_os_handle>;


template<typename S, typename... Args>
[[noreturn]] auto throw_runtime_error(const S &format_str, Args &&...args) -> void {
    auto fmt_buffer = fmt::memory_buffer{};
    fmt::format_to(fmt_buffer, format_str, std::forward<Args>(args)...);
    fmt_buffer.push_back('\0');
    throw std::runtime_error{fmt_buffer.data()};
}

template<typename S, typename... Args>
[[noreturn]] auto throw_last_system_error(const S &format_str, Args &&...args) -> void {
#ifdef HINDSIGHT_OS_WINDOWS
    const auto last_error = static_cast<int>(GetLastError());
#elif defined HINDSIGHT_OS_UNIX
    const auto last_error = errno;
#else
    #error throw_last_system_error is not implemented for this OS
#endif
    auto fmt_buffer = fmt::memory_buffer{};
    fmt::format_to(fmt_buffer, format_str, std::forward<Args>(args)...);
    fmt_buffer.push_back('\0');
    throw std::system_error{last_error, std::system_category(), fmt_buffer.data()};
}

template<typename S, typename... Args>
void print_log(const S &format_str, Args &&...args) {
    fmt::print(stderr, format_str, std::forward<Args>(args)...);
}


[[nodiscard]] inline auto read_from_handle(const os_handle handle, std::span<std::byte> bytes) noexcept -> bool {
    while (true) {
#ifdef HINDSIGHT_OS_WINDOWS
        auto bytes_read = DWORD{};
        if (!ReadFile(handle,
                      bytes.data(),
                      static_cast<DWORD>(std::min(bytes.size(), std::size_t{std::numeric_limits<DWORD>::max()})),
                      &bytes_read,
                      nullptr)) {
            return false;
        }
#elif defined HINDSIGHT_OS_UNIX
        const auto bytes_read =
                read(handle,
                     bytes.data(),
                     std::min(bytes.size(), static_cast<std::size_t>(std::numeric_limits<ssize_t>::max())));
        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
#elif
    #error read_from_handle is not implemented for this OS
#endif
        if (static_cast<std::size_t>(bytes_read) == bytes.size()) {
            return true;
        }
        bytes = bytes.subspan(static_cast<std::size_t>(bytes_read));
    }
}

template<typename T>
    requires(!std::convertible_to<T, std::span<std::byte>>)
[[nodiscard]] auto read_from_handle(const os_handle handle, T &value) noexcept -> bool {
    static_assert(std::is_trivially_copyable_v<T>);
    return read_from_handle(handle, std::as_writable_bytes(std::span{&value, 1}));
}

[[nodiscard]] inline auto write_to_handle(const os_handle handle, std::span<const std::byte> bytes) noexcept -> bool {
    while (true) {
#ifdef HINDSIGHT_OS_WINDOWS
        auto bytes_written = DWORD{};
        if (!WriteFile(handle,
                       bytes.data(),
                       static_cast<DWORD>(std::min(bytes.size(), std::size_t{std::numeric_limits<DWORD>::max()})),
                       &bytes_written,
                       nullptr)) {
            return false;
        }
#elif defined HINDSIGHT_OS_UNIX
        const auto bytes_written =
                write(handle,
                      bytes.data(),
                      std::min(bytes.size(), static_cast<std::size_t>(std::numeric_limits<ssize_t>::max())));
        if (bytes_written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
#else
    #error write_to_handle is not implemented for this OS
#endif
        if (static_cast<std::size_t>(bytes_written) == bytes.size()) {
            return true;
        }
        bytes = bytes.subspan(static_cast<std::size_t>(bytes_written));
    }
}

template<typename T>
    requires(!std::convertible_to<T, std::span<const std::byte>>)
[[nodiscard]] auto write_to_handle(const os_handle handle, const T &value) noexcept -> bool {
    static_assert(std::is_trivially_copyable_v<T>);
    return write_to_handle(handle, std::as_bytes(std::span{&value, 1}));
}


struct pipe_handles {
    unique_os_handle read;
    unique_os_handle write;
};

[[nodiscard]] inline auto create_pipe() -> pipe_handles {
#ifdef HINDSIGHT_OS_WINDOWS
    auto attributes = SECURITY_ATTRIBUTES{};
    attributes.nLength = sizeof attributes;
    attributes.bInheritHandle = TRUE;
    auto read_handle = HANDLE{};
    auto write_handle = HANDLE{};
    if (!CreatePipe(&read_handle, &write_handle, &attributes, 0)) {
        throw_last_system_error("Failed to create a pipe");
    }
    return {.read{read_handle}, .write{write_handle}};
#elif defined HINDSIGHT_OS_UNIX
    auto read_and_write_descriptors = std::array<int, 2>{};
    if (pipe(read_and_write_descriptors.data()) != 0) {
        throw_last_system_error("Failed to create a pipe");
    }
    return {.read = unique_os_handle{read_and_write_descriptors[0]},
            .write = unique_os_handle{read_and_write_descriptors[1]}};
#endif
}

inline auto prevent_handle_inheritance(const os_handle handle) -> void {
#ifdef HINDSIGHT_OS_WINDOWS
    if (!SetHandleInformation(handle, HANDLE_FLAG_INHERIT, {})) {
        throw_last_system_error("Failed to prevent handle inheritance");
    }
#elif defined HINDSIGHT_OS_UNIX
    auto flags = fcntl(handle, F_GETFD);
    if (flags < 0) {
        throw_last_system_error("Failed to get the current handle flags");
    }
    flags |= FD_CLOEXEC;
    if (fcntl(handle, F_SETFD, flags) == -1) {
        throw_last_system_error("Failed to prevent handle inheritance");
    }
#endif
}

inline auto allow_handle_inheritance(const os_handle handle) -> void {
#ifdef HINDSIGHT_OS_WINDOWS
    if (!SetHandleInformation(handle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
        throw_last_system_error("Failed to allow handle inheritance");
    }
#elif defined HINDSIGHT_OS_UNIX
    auto flags = fcntl(handle, F_GETFD);
    if (flags < 0) {
        throw_last_system_error("Failed to get the current handle flags");
    }
    flags &= ~FD_CLOEXEC;
    if (fcntl(handle, F_SETFD, flags) == -1) {
        throw_last_system_error("Failed to allow handle inheritance");
    }
#endif
}

} // namespace hindsight::out_of_process

#endif // HINDSIGHT_EXAMPLES_OUT_OF_PROCESS_COMMON_HPP
