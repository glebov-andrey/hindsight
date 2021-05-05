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
    #include "resolver_windows_impl.hpp"

    #include <concepts>
    #include <tuple>

    #include <Windows.h> // Windows.h must be included before diacreate.h
HINDSIGHT_PRAGMA_CLANG("clang diagnostic push")
HINDSIGHT_PRAGMA_CLANG("clang diagnostic ignored \"-Wlanguage-extension-token\"") // __wchar_t used in diacreate.h
    #include <diacreate.h>
HINDSIGHT_PRAGMA_CLANG("clang diagnostic pop")

    #include "windows/encoding.hpp"
    #include "windows/module_info.hpp"

namespace hindsight {

struct logical_stacktrace_entry::impl_payload {
    windows::com_ptr<IDiaSession> session{};
    windows::com_ptr<IDiaSymbol> symbol{};
};


logical_stacktrace_entry::logical_stacktrace_entry() noexcept { // NOLINT(hicpp-member-init)
    static_assert(sizeof(impl_payload) == impl_payload_size);
    new (static_cast<void *>(m_impl_storage.data())) impl_payload{};
}

logical_stacktrace_entry::logical_stacktrace_entry(const stacktrace_entry physical, // NOLINT(hicpp-member-init)
                                                   const bool is_inline,
                                                   impl_payload &&impl) noexcept
        : m_physical{physical},
          m_is_inline{is_inline} {
    new (static_cast<void *>(m_impl_storage.data())) impl_payload{std::move(impl)};
}

logical_stacktrace_entry::logical_stacktrace_entry(const logical_stacktrace_entry &other) // NOLINT(hicpp-member-init)
        : m_physical{other.m_physical},
          m_is_inline{other.m_is_inline} {
    new (static_cast<void *>(m_impl_storage.data())) impl_payload(other.impl());
}

// NOLINTNEXTLINE(hicpp-member-init)
logical_stacktrace_entry::logical_stacktrace_entry(logical_stacktrace_entry &&other) noexcept
        : m_physical{std::exchange(other.m_physical, stacktrace_entry{})},
          m_is_inline{std::exchange(other.m_is_inline, false)} {
    new (static_cast<void *>(m_impl_storage.data())) impl_payload(std::move(other.impl()));
}

logical_stacktrace_entry::~logical_stacktrace_entry() { impl().~impl_payload(); }

auto logical_stacktrace_entry::swap(logical_stacktrace_entry &other) noexcept -> void {
    std::ranges::swap(m_physical, other.m_physical);
    std::ranges::swap(m_is_inline, other.m_is_inline);
    std::ranges::swap(impl(), other.impl());
}

namespace {

[[nodiscard]] auto symbol_impl(const logical_stacktrace_entry::impl_payload &impl) -> windows::bstr {
    if (!impl.symbol) {
        return {};
    }
    auto symbol_name = windows::bstr{};
    if (const auto result = impl.symbol->get_name(&symbol_name); FAILED(result)) {
        return {};
    }
    return symbol_name;
}

} // namespace

auto logical_stacktrace_entry::symbol() const -> std::string { return windows::wide_to_narrow(symbol_impl(impl())); }

auto logical_stacktrace_entry::u8_symbol() const -> std::u8string { return windows::wide_to_utf8(symbol_impl(impl())); }

namespace {

[[nodiscard]] auto source_impl(const stacktrace_entry physical,
                               const bool is_inline,
                               const logical_stacktrace_entry::impl_payload &impl)
        -> std::tuple<windows::bstr, std::uint_least32_t> {
    if (!impl.symbol) {
        return {};
    }
    auto lines = windows::com_ptr<IDiaEnumLineNumbers>{};
    if (is_inline) {
        if (const auto result =
                    impl.session->findInlineeLinesByVA(impl.symbol.get(), physical.native_handle(), 1, &lines);
            FAILED(result) || !lines) {
            return {};
        }
    } else {
        if (const auto result = impl.session->findLinesByVA(physical.native_handle(), 1, &lines);
            FAILED(result) || !lines) {
            return {};
        }
    }
    auto line_count = LONG{};
    if (const auto result = lines->get_Count(&line_count); FAILED(result) || line_count == 0) {
        return {};
    }
    auto line = windows::com_ptr<IDiaLineNumber>{};
    if (const auto result = lines->Item(0, &line); FAILED(result) || !line) {
        return {};
    }
    auto source_file = windows::com_ptr<IDiaSourceFile>{};
    if (const auto result = line->get_sourceFile(&source_file); FAILED(result) || !source_file) {
        return {};
    }
    auto file_name = windows::bstr{};
    if (const auto result = source_file->get_fileName(&file_name); FAILED(result)) {
        return {};
    }
    auto line_number = DWORD{};
    line->get_lineNumber(&line_number);
    return {std::move(file_name), line_number};
}

} // namespace

auto logical_stacktrace_entry::source() const -> source_location {
    const auto [file_name, line_number] = source_impl(physical(), is_inline(), impl());
    return {.file_name = windows::wide_to_narrow(file_name), .line_number = line_number};
}

auto logical_stacktrace_entry::u8_source() const -> u8_source_location {
    const auto [file_name, line_number] = source_impl(physical(), is_inline(), impl());
    return {.file_name = windows::wide_to_utf8(file_name), .line_number = line_number};
}


auto resolver::impl::resolve(const stacktrace_entry entry, resolve_cb *const callback, void *const user_data) -> void {
    const auto on_failure = [&] { callback({entry, false, {}}, user_data); };

    auto session = session_for_entry(entry);
    if (!session) {
        on_failure();
        return;
    }

    auto root_symbol = windows::com_ptr<IDiaSymbol>{};
    for (const auto symbol_type : {SymTagFunction, SymTagPublicSymbol}) {
        if (const auto result = session->findSymbolByVAEx(entry.native_handle(), symbol_type, &root_symbol, nullptr);
            SUCCEEDED(result) && root_symbol) {
            break;
        }
    }
    if (!root_symbol) {
        on_failure();
        return;
    }

    const auto done = [&] {
        auto inline_symbols = windows::com_ptr<IDiaEnumSymbols>{};
        if (const auto result = root_symbol->findInlineFramesByVA(entry.native_handle(), &inline_symbols);
            FAILED(result) || !inline_symbols) {
            return false;
        }
        auto inline_symbol_count = LONG{};
        if (const auto result = inline_symbols->get_Count(&inline_symbol_count); FAILED(result)) {
            return false;
        }
        for (auto inline_idx = LONG{}; inline_idx != inline_symbol_count; ++inline_idx) {
            auto inline_symbol = windows::com_ptr<IDiaSymbol>{};
            if (const auto result = inline_symbols->Item(inline_idx, &inline_symbol);
                SUCCEEDED(result) && inline_symbol) {
                if (callback({entry, true, {session, std::move(inline_symbol)}}, user_data)) {
                    return true;
                }
            }
        }
        return false;
    }();
    if (done) {
        return;
    }

    callback({entry, false, {std::move(session), std::move(root_symbol)}}, user_data);
}

auto resolver::impl::session_for_entry(const stacktrace_entry entry) -> windows::com_ptr<IDiaSession> {
    const auto module_info = windows::get_module_info_by_address(entry.native_handle());
    return m_sessions.with_lock([&](session_map &sessions) -> windows::com_ptr<IDiaSession> {
        auto [it, inserted] = sessions.try_emplace(module_info.file_path);
        if (!inserted) {
            return it->second;
        }

        auto dia_data_source = windows::com_ptr<IDiaDataSource>{};
        {
            void *dia_data_source_void = nullptr;
            // TODO: Detect the correct DLL name in FindDIA.cmake
            if (const auto result =
                        NoRegCoCreate(L"msdia140.dll", CLSID_DiaSource, IID_IDiaDataSource, &dia_data_source_void);
                FAILED(result) || !dia_data_source_void) {
                return nullptr;
            }
            dia_data_source.reset(static_cast<IDiaDataSource *>(dia_data_source_void));
        }
        if (const auto result =
                    dia_data_source->loadDataForExe(module_info.file_path.c_str(),
                                                    nullptr, // TODO: Allow the user to specify a custom search path
                                                    nullptr);
            FAILED(result)) {
            return nullptr;
        }
        if (const auto result = dia_data_source->openSession(&it->second); FAILED(result)) {
            return nullptr;
        }
        if (const auto result = it->second->put_loadAddress(module_info.base_offset); FAILED(result)) {
            return nullptr;
        }
        return it->second;
    });
}


resolver::resolver() : m_impl{std::make_unique<impl>()} {}

resolver::~resolver() = default;

auto resolver::resolve_impl(const stacktrace_entry entry, resolve_cb *const callback, void *const user_data) -> void {
    m_impl->resolve(entry, callback, user_data);
}

} // namespace hindsight

#endif
