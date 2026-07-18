include(FindPackageHandleStandardArgs)

try_compile(
    HAS_BUILTIN_LIBBACKTRACE
    SOURCE_FROM_CONTENT
        find_libbacktrace.cpp
        "#include <backtrace.h>
        int main() {
            [[maybe_unused]] auto *bt_state = backtrace_create_state(nullptr, true, nullptr, nullptr);
        }"
    LINK_LIBRARIES backtrace
)
if(HAS_BUILTIN_LIBBACKTRACE)
    set(libbacktrace_FOUND TRUE)
    find_package_handle_standard_args(libbacktrace DEFAULT_MSG)
    if(NOT TARGET libbacktrace::libbacktrace)
        add_library(libbacktrace::libbacktrace INTERFACE IMPORTED)
        set_target_properties(
            libbacktrace::libbacktrace
            PROPERTIES INTERFACE_LINK_LIBRARIES backtrace
        )
        if(NOT libbacktrace_FIND_QUIETLY)
            message(STATUS "Found libbacktrace: -lbacktrace")
        endif()
    endif()
    return()
endif()

find_path(LIBBACKTRACE_INCLUDE_DIR NAMES backtrace.h)
find_library(LIBBACKTRACE_LIBRARY NAMES backtrace)

mark_as_advanced(LIBBACKTRACE_INCLUDE_DIR LIBBACKTRACE_LIBRARY)

find_package_handle_standard_args(
    libbacktrace
    DEFAULT_MSG
    LIBBACKTRACE_INCLUDE_DIR
    LIBBACKTRACE_LIBRARY
)

if(libbacktrace_FOUND AND NOT TARGET libbacktrace::libbacktrace)
    add_library(libbacktrace::libbacktrace UNKNOWN IMPORTED)
    set_target_properties(
        libbacktrace::libbacktrace
        PROPERTIES
            IMPORTED_LOCATION "${LIBBACKTRACE_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${LIBBACKTRACE_INCLUDE_DIR}"
    )
    if(NOT libbacktrace_FIND_QUIETLY)
        message(STATUS "Found libbacktrace: ${LIBBACKTRACE_LIBRARY}")
    endif()
endif()
