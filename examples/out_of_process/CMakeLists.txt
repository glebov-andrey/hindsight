add_executable(out_of_process_watchdog)
target_sources(out_of_process_watchdog PRIVATE out_of_process_watchdog.cpp)
target_compile_features(out_of_process_watchdog PRIVATE cxx_std_20)
target_link_libraries(out_of_process_watchdog PRIVATE hindsight::default_options hindsight::hindsight fmt::fmt)

add_executable(out_of_process_host)
target_sources(out_of_process_host PRIVATE out_of_process_host.cpp)
target_compile_features(out_of_process_host PRIVATE cxx_std_20)
target_link_libraries(out_of_process_host PRIVATE hindsight::default_options hindsight::hindsight)
add_dependencies(out_of_process_host out_of_process_watchdog)
