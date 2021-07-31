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

#ifdef HINDSIGHT_OS_LINUX

    #include <hindsight/resolver.hpp>

    #include <algorithm>
    #include <cassert>
    #include <cstddef>
    #include <cstdio>
    #include <limits>
    #include <optional>
    #include <shared_mutex>
    #include <stack>
    #include <string_view>
    #include <vector>

    #include <stdio.h> // NOLINT(hicpp-deprecated-headers): fdopen is defined by POSIX in <stdio.h>
    #include <unistd.h>

    #include <dwarf.h>
    #include <elfutils/libdw.h>
    #include <elfutils/libdwfl.h>

    #include "util/locked.hpp"

    #include "itanium_abi/demangle.hpp"
    #include "unix/encoding.hpp"

namespace hindsight {

namespace {

struct close_c_file {
    auto operator()(std::FILE *const file) const noexcept -> void {
        [[maybe_unused]] const auto result = std::fclose(file);
        assert(result == 0);
    }
};

using unique_c_file = std::unique_ptr<std::FILE, close_c_file>;

[[nodiscard]] auto c_file_from_fd(const int fd) noexcept { return unique_c_file{fdopen(fd, "r")}; }


struct end_dwfl_session {
    auto operator()(Dwfl *const ptr) const noexcept -> void { dwfl_end(ptr); }
};

using unique_dwfl_session = std::unique_ptr<Dwfl, end_dwfl_session>;


struct dwarf_source_location {
    const char *file_name;
    std::uint_least32_t line_number;
    std::uint_least32_t column_number;
};

[[nodiscard]] constexpr auto clamp_to_uint_least32(const Dwarf_Word word) noexcept -> std::uint_least32_t {
    if constexpr (sizeof word > sizeof(std::uint_least32_t)) {
        return static_cast<std::uint_least32_t>(
                std::clamp(word, Dwarf_Word{}, Dwarf_Word{std::numeric_limits<std::uint_least32_t>::max()}));
    } else {
        return static_cast<std::uint_least32_t>(word);
    }
}


constexpr auto dwfl_session_callbacks = Dwfl_Callbacks{.find_elf = dwfl_linux_proc_find_elf,
                                                       .find_debuginfo = dwfl_standard_find_debuginfo,
                                                       .section_address = nullptr,
                                                       .debuginfo_path = nullptr};


[[nodiscard]] auto die_has_address(Dwarf_Die &die, const Dwarf_Addr address) noexcept -> bool {
    auto offset = std::ptrdiff_t{};
    auto base_addr = Dwarf_Addr{};
    auto start_addr = Dwarf_Addr{};
    auto end_addr = Dwarf_Addr{};
    while ((offset = dwarf_ranges(&die, offset, &base_addr, &start_addr, &end_addr)) > 0) {
        if (address >= start_addr && address < end_addr) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] auto is_function(Dwarf_Die &die) noexcept -> bool {
    const auto tag = dwarf_tag(&die);
    return tag == DW_TAG_subprogram || tag == DW_TAG_inlined_subroutine || tag == DW_TAG_entry_point;
}

[[nodiscard]] auto is_inline_function(Dwarf_Die &die) noexcept -> bool {
    const auto tag = dwarf_tag(&die);
    return tag == DW_TAG_inlined_subroutine;
}

using find_cu_result = std::optional<std::pair<Dwarf_Die &, Dwarf_Addr>>;

[[nodiscard]] auto find_compilation_unit(Dwfl_Module &module, const stacktrace_entry entry) noexcept -> find_cu_result {
    auto address_bias = Dwarf_Addr{};
    auto *compilation_unit = dwfl_module_addrdie(&module, entry.native_handle(), &address_bias);
    if (compilation_unit) {
        return find_cu_result{{*compilation_unit, entry.native_handle() - address_bias}};
    }

    while ((compilation_unit = dwfl_module_nextcu(&module, compilation_unit, &address_bias))) {
        assert(address_bias <= entry.native_handle());
        const auto address_in_cu = entry.native_handle() - address_bias;
        if (die_has_address(*compilation_unit, address_in_cu)) {
            return find_cu_result{{*compilation_unit, address_in_cu}};
        }
    }

    return std::nullopt;
}

[[nodiscard]] auto get_most_inline_source_location(Dwarf_Die &compilation_unit, const Dwarf_Addr address) noexcept
        -> std::optional<dwarf_source_location> {
    auto *const source_line = dwarf_getsrc_die(&compilation_unit, address);
    if (!source_line) {
        return std::nullopt;
    }
    const auto *const file_name = dwarf_linesrc(source_line, nullptr, nullptr);
    if (!file_name) {
        return std::nullopt;
    }

    auto line_number = 0;
    dwarf_lineno(source_line, &line_number);
    auto column_number = 0;
    dwarf_linecol(source_line, &column_number);

    return dwarf_source_location{.file_name = file_name,
                                 .line_number = clamp_to_uint_least32(line_number),
                                 .column_number = clamp_to_uint_least32(column_number)};
}

namespace func_name_search {

enum class search_stage {
    initial,
    checked_linkage_names,
    checked_specification,
    checked_abstract_origin,
};

using die_stack_entry = std::pair<Dwarf_Die, search_stage>;
using die_stack = std::stack<die_stack_entry, std::vector<die_stack_entry>>;

[[nodiscard]] auto search_parent_attribute(die_stack &stack, Dwarf_Die &die, const int attribute_name) -> bool {
    assert(!stack.empty());
    auto parent_attribute = Dwarf_Attribute{};
    if (dwarf_attr(&die, attribute_name, &parent_attribute)) {
        auto parent_die = Dwarf_Die{};
        if (dwarf_formref_die(&parent_attribute, &parent_die)) {
            stack.emplace(parent_die, search_stage::initial);
            return true;
        }
    }
    return false;
}

[[nodiscard]] auto search(Dwarf_Die &function) -> std::pair<const char *, bool /* maybe_mangled */> {
    auto stack = die_stack{};
    stack.emplace(function, search_stage::initial);
    while (!stack.empty()) {
        auto &[die, stage] = stack.top();

        switch (stage) {
            case search_stage::initial: {
                stage = search_stage::checked_linkage_names;

                auto name_attribute = Dwarf_Attribute{};
                for (const auto attribute_name : {DW_AT_linkage_name, DW_AT_MIPS_linkage_name}) {
                    if (dwarf_attr(&die, attribute_name, &name_attribute)) {
                        const auto *const name = dwarf_formstring(&name_attribute);
                        if (name) {
                            return {name, true};
                        }
                    }
                }
                [[fallthrough]];
            }
            case search_stage::checked_linkage_names: {
                stage = search_stage::checked_specification;
                if (search_parent_attribute(stack, die, DW_AT_specification)) {
                    continue;
                }
                [[fallthrough]];
            }
            case search_stage::checked_specification: {
                stage = search_stage::checked_abstract_origin;
                if (search_parent_attribute(stack, die, DW_AT_abstract_origin)) {
                    continue;
                }
                [[fallthrough]];
            }
            case search_stage::checked_abstract_origin: {
                const auto *const name = dwarf_diename(&die);
                if (name) {
                    return {name, false};
                }
                break;
            }
        }
        stack.pop();
    }

    return {nullptr, false};
}

} // namespace func_name_search

[[nodiscard]] auto get_inline_call_location(Dwarf_Die &compilation_unit, Dwarf_Die &function) noexcept
        -> std::optional<dwarf_source_location> {
    auto call_file_attr = Dwarf_Attribute{};
    if (!dwarf_attr(&function, DW_AT_call_file, &call_file_attr)) {
        return {};
    }
    auto file_idx = Dwarf_Word{};
    if (dwarf_formudata(&call_file_attr, &file_idx) != 0) {
        return {};
    }
    Dwarf_Files *files = nullptr;
    auto file_count = std::size_t{};
    if (dwarf_getsrcfiles(&compilation_unit, &files, &file_count) != 0) {
        return {};
    }
    const auto *const file_name = dwarf_filesrc(files, file_idx, nullptr, nullptr);
    if (!file_name) {
        return {};
    }

    auto call_line_attr = Dwarf_Attribute{};
    dwarf_attr(&function, DW_AT_call_line, &call_line_attr);
    auto line_number = Dwarf_Word{};
    dwarf_formudata(&call_line_attr, &line_number);

    auto call_column_attr = Dwarf_Attribute{};
    dwarf_attr(&function, DW_AT_call_column, &call_column_attr);
    auto column_number = Dwarf_Word{};
    dwarf_formudata(&call_column_attr, &column_number);

    return dwarf_source_location{.file_name = file_name,
                                 .line_number = clamp_to_uint_least32(line_number),
                                 .column_number = clamp_to_uint_least32(column_number)};
}

} // namespace


struct logical_stacktrace_entry::impl_tag {};

logical_stacktrace_entry::logical_stacktrace_entry(logical_stacktrace_entry::impl_tag && /* impl */,
                                                   const stacktrace_entry physical,
                                                   const bool is_inline,
                                                   const bool maybe_mangled,
                                                   const char *const symbol,
                                                   const char *const file_name,
                                                   const std::uint_least32_t line_number,
                                                   const std::uint_least32_t column_number,
                                                   std::shared_ptr<const void> resolver_impl) noexcept
        : m_physical{physical},
          m_is_inline{is_inline},
          m_maybe_mangled{maybe_mangled},
          m_symbol{symbol},
          m_file_name{file_name},
          m_line_number{line_number},
          m_column_number{column_number},
          m_resolver_impl{std::move(resolver_impl)} {}

logical_stacktrace_entry::logical_stacktrace_entry(const logical_stacktrace_entry &other) = default;

logical_stacktrace_entry::~logical_stacktrace_entry() = default;

namespace {

template<typename CharT>
auto demangle_and_encode_symbol(const char *const symbol, const bool maybe_mangled, const auto get_transcoder)
        -> std::basic_string<CharT> {
    const auto demangled = symbol && maybe_mangled ? itanium_abi::demangle(symbol) : nullptr;
    const auto unsanitized = demangled ? std::string_view{demangled.get()}
                             : symbol  ? std::string_view{symbol}
                                       : std::string_view{};
    if (unsanitized.empty()) {
        return {};
    }
    return unix::transcode(get_transcoder(), unsanitized, std::in_place_type<CharT>);
}

} // namespace

auto logical_stacktrace_entry::symbol() const -> std::string {
    return demangle_and_encode_symbol<char>(m_symbol, m_maybe_mangled, [] {
        return unix::get_utf8_to_current_transcoder();
    });
}

auto logical_stacktrace_entry::u8_symbol() const -> std::u8string {
    return demangle_and_encode_symbol<char8_t>(m_symbol, m_maybe_mangled, [] { return unix::get_utf8_sanitizer(); });
}

namespace {

template<typename CharT>
auto encode_file_name(const char *const file_name, const auto get_transcoder) -> std::basic_string<CharT> {
    const auto unsanitized_file_name = file_name ? std::string_view{file_name} : std::string_view{};
    if (unsanitized_file_name.empty()) {
        return {};
    }
    return unix::transcode(get_transcoder(), unsanitized_file_name, std::in_place_type<CharT>);
}

} // namespace

auto logical_stacktrace_entry::source() const -> source_location {
    return {.file_name = encode_file_name<char>(m_file_name, [] { return unix::get_utf8_to_current_transcoder(); }),
            .line_number = m_line_number};
}

auto logical_stacktrace_entry::u8_source() const -> u8_source_location {
    return {.file_name = encode_file_name<char8_t>(m_file_name, [] { return unix::get_utf8_sanitizer(); }),
            .line_number = m_line_number};
}


class resolver::impl : public std::enable_shared_from_this<impl> {
public:
    explicit impl() noexcept : m_dwfl_session{create_initial_session()} {}

    explicit impl(from_proc_maps_t /* from_proc_maps_tag */, const int proc_maps_descriptor) noexcept
            : m_proc_maps{c_file_from_fd(proc_maps_descriptor)},
              m_dwfl_session{m_proc_maps ? create_initial_session() : nullptr} {}

    impl(const impl &other) = delete;
    impl(impl &&other) = delete;

    ~impl() = default;

    auto operator=(const impl &other) = delete;
    auto operator=(impl &&other) = delete;

    auto resolve(const stacktrace_entry entry, const resolve_cb callback) -> void {
        auto cb_state = callback_state{.entry = entry, .callback = callback};

        if (!is_initialized()) {
            cb_state.on_failure();
            return;
        }

        if (try_resolve_in_existing_modules(cb_state)) {
            return;
        }

        m_dwfl_session.with_lock([&](const unique_dwfl_session &session) noexcept {
            dwfl_report_begin_add(session.get());
            report_mappings(*session);
            dwfl_report_end(session.get(), nullptr, nullptr);
        });

        if (try_resolve_in_existing_modules(cb_state)) {
            return;
        }

        cb_state.on_failure();
    }

private:
    const unique_c_file m_proc_maps{};
    const util::locked<unique_dwfl_session, std::shared_mutex> m_dwfl_session;

    struct callback_state {
        const stacktrace_entry entry;
        const resolve_cb callback;

        bool entry_issued = false;
        bool done = false;

        auto submit(logical_stacktrace_entry &&logical) -> bool {
            if (!done) {
                done = callback(std::move(logical));
                entry_issued = true;
            }
            return done;
        }

        auto on_failure() -> void {
            if (!entry_issued) {
                done = callback({{}, entry, false, false, nullptr, nullptr, 0, 0, nullptr});
                entry_issued = true;
            }
        }
    };

    auto report_mappings(Dwfl &session) const noexcept -> bool {
        const auto result = m_proc_maps ? dwfl_linux_proc_maps_report(&session, m_proc_maps.get())
                                        : dwfl_linux_proc_report(&session, getpid());
        return result == 0;
    }

    [[nodiscard]] auto create_initial_session() const noexcept -> unique_dwfl_session {
        auto session = unique_dwfl_session{dwfl_begin(&dwfl_session_callbacks)};

        if (session) {
            dwfl_report_begin(session.get());
            const auto success = report_mappings(*session);
            dwfl_report_end(session.get(), nullptr, nullptr);
            if (!success) {
                session.reset();
            }
        }

        return session;
    }

    [[nodiscard]] auto is_initialized() const noexcept -> bool {
        // This is safe because the pointer itself is never modified (ensured by the member being const).
        return m_dwfl_session.unsafe_get() != nullptr;
    }

    // Returns whether or not a module was found for `entry`.
    [[nodiscard]] auto try_resolve_in_existing_modules(callback_state &cb_state) const -> bool {
        auto *const module = m_dwfl_session.with_shared_lock([&](const unique_dwfl_session &session) {
            return dwfl_addrmodule(session.get(), cb_state.entry.native_handle());
        });
        if (!module) {
            return false;
        }
        resolve_in_module(*module, cb_state);
        return true;
    }

    auto resolve_in_module(Dwfl_Module &module, callback_state &cb_state) const -> void {
        const auto cudie_and_addr_in_cu = find_compilation_unit(module, cb_state.entry);
        if (!cudie_and_addr_in_cu) {
            resolve_in_symbol_table(module, cb_state);
            return;
        }
        const auto &[compilation_unit, address_in_cu] = *cudie_and_addr_in_cu;

        auto die_stack = depth_first_search_for_address(compilation_unit, address_in_cu);
        if (die_stack.empty()) {
            resolve_in_symbol_table(module, cb_state);
            return;
        }
        resolve_from_dfs_die_stack(std::move(die_stack), compilation_unit, address_in_cu, cb_state);
    }

    using dfs_die_stack_entry = std::pair<Dwarf_Die, bool /* explored */>;
    using dfs_die_stack = std::stack<dfs_die_stack_entry, std::vector<dfs_die_stack_entry>>;

    [[nodiscard]] static auto depth_first_search_for_address(const Dwarf_Die &compilation_unit,
                                                             const Dwarf_Addr address_in_cu) -> dfs_die_stack {
        auto stack = dfs_die_stack{};
        stack.emplace(compilation_unit, false);
        while (!stack.empty()) {
            auto &[die, explored] = stack.top();

            if (!explored) {
                explored = true;
                auto child_die = Dwarf_Die{};
                if (dwarf_child(&die, &child_die) == 0) {
                    stack.emplace(child_die, false);
                    continue;
                }
            }

            if (is_function(die) && die_has_address(die, address_in_cu)) {
                break;
            }

            if (dwarf_siblingof(&die, &die) == 0) {
                explored = false;
            } else {
                stack.pop();
            }
        }
        return stack;
    }

    auto resolve_from_dfs_die_stack(dfs_die_stack &&stack,
                                    Dwarf_Die &compilation_unit,
                                    const Dwarf_Addr address_in_cu,
                                    callback_state &cb_state) const -> void {
        assert(!stack.empty());
        assert(is_function(stack.top().first));
        assert(die_has_address(stack.top().first, address_in_cu));

        auto source_location = get_most_inline_source_location(compilation_unit, address_in_cu);
        while (!stack.empty()) {
            auto &[die, _explored] = stack.top();
            if (is_function(die) && die_has_address(die, address_in_cu)) {
                const auto is_inline = is_inline_function(die);
                const auto [function_name, maybe_mangled] = func_name_search::search(die);
                if (cb_state.submit({{},
                                     cb_state.entry,
                                     is_inline,
                                     function_name ? maybe_mangled : false,
                                     function_name,
                                     source_location ? source_location->file_name : nullptr,
                                     source_location ? source_location->line_number : 0,
                                     source_location ? source_location->column_number : 0,
                                     shared_from_this()})) {
                    return;
                }
                if (!is_inline) {
                    return;
                }
                source_location = get_inline_call_location(compilation_unit, die);
            }
            stack.pop();
        }
    }

    auto resolve_in_symbol_table(Dwfl_Module &module, callback_state &cb_state) const -> void {
        auto offset_in_symbol = GElf_Off{};
        auto symbol = GElf_Sym{};
        const auto *const symbol_name = dwfl_module_addrinfo(&module,
                                                             cb_state.entry.native_handle(),
                                                             &offset_in_symbol,
                                                             &symbol, // if NULL then it crashes
                                                             nullptr,
                                                             nullptr,
                                                             nullptr);
        if (symbol_name) {
            cb_state.submit({{}, cb_state.entry, false, true, symbol_name, nullptr, 0, 0, shared_from_this()});
        } else {
            cb_state.on_failure();
        }
    }
};

resolver::resolver() : m_impl{std::make_shared<impl>()} {}

resolver::resolver(const from_proc_maps_t from_proc_maps_tag, const int proc_maps_descriptor)
        : m_impl{std::make_shared<impl>(from_proc_maps_tag, proc_maps_descriptor)} {}

auto resolver::resolve_impl(const stacktrace_entry entry, const resolve_cb callback) -> void {
    m_impl->resolve(entry, callback);
}

} // namespace hindsight

#endif
