add_executable(mcutrace_tests test_model.cpp)
target_link_libraries(mcutrace_tests PRIVATE mcutrace::core mcutest::mcutest)
mcutrace_host_options(mcutrace_tests)
add_test(NAME mcutrace_tests COMMAND mcutrace_tests)
