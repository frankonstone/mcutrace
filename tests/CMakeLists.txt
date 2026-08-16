add_executable(mcutrace_tests test_model.cpp)
target_link_libraries(mcutrace_tests PRIVATE mcutrace::core mcutest::mcutest)
mcutrace_host_options(mcutrace_tests)
add_test(NAME mcutrace_tests COMMAND mcutrace_tests)

add_executable(mcutrace_requirement_tests test_requirements.cpp)
target_link_libraries(mcutrace_requirement_tests PRIVATE mcutrace::core mcutest::mcutest)
mcutrace_host_options(mcutrace_requirement_tests)
add_test(NAME mcutrace_requirement_tests COMMAND mcutrace_requirement_tests)

add_executable(mcutrace_importer_tests test_importer.cpp)
target_link_libraries(mcutrace_importer_tests PRIVATE mcutrace::core mcutest::mcutest)
mcutrace_host_options(mcutrace_importer_tests)
add_test(NAME mcutrace_importer_tests COMMAND mcutrace_importer_tests)

add_executable(mcutrace_producer_importer_tests test_producer_importers.cpp)
target_link_libraries(mcutrace_producer_importer_tests PRIVATE mcutrace::core mcutest::mcutest)
mcutrace_host_options(mcutrace_producer_importer_tests)
add_test(NAME mcutrace_producer_importer_tests COMMAND mcutrace_producer_importer_tests)

add_executable(mcutrace_trace_import_tests test_trace_import.cpp)
target_link_libraries(mcutrace_trace_import_tests PRIVATE mcutrace::core mcutest::mcutest)
mcutrace_host_options(mcutrace_trace_import_tests)
add_test(NAME mcutrace_trace_import_tests COMMAND mcutrace_trace_import_tests)

add_executable(mcutrace_hardening_tests test_hardening.cpp)
target_link_libraries(mcutrace_hardening_tests PRIVATE mcutrace::core mcutest::mcutest)
target_compile_definitions(mcutrace_hardening_tests PRIVATE
    MCUTRACE_THIRD_PARTY_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/third-party-results.json")
mcutrace_host_options(mcutrace_hardening_tests)
add_test(NAME mcutrace_hardening_tests COMMAND mcutrace_hardening_tests)

add_executable(mcutrace_assembly_tests test_assembly.cpp)
target_link_libraries(mcutrace_assembly_tests PRIVATE mcutrace::core mcutest::mcutest)
mcutrace_host_options(mcutrace_assembly_tests)
add_test(NAME mcutrace_assembly_tests COMMAND mcutrace_assembly_tests)

add_executable(mcutrace_validation_tests test_validation.cpp)
target_link_libraries(mcutrace_validation_tests PRIVATE mcutrace::core mcutest::mcutest)
mcutrace_host_options(mcutrace_validation_tests)
add_test(NAME mcutrace_validation_tests COMMAND mcutrace_validation_tests)

add_executable(mcutrace_config_tests test_config.cpp)
target_link_libraries(mcutrace_config_tests PRIVATE mcutrace::core mcutest::mcutest)
mcutrace_host_options(mcutrace_config_tests)
add_test(NAME mcutrace_config_tests COMMAND mcutrace_config_tests)

add_executable(mcutrace_cli_tests test_cli.cpp)
target_link_libraries(mcutrace_cli_tests PRIVATE mcutrace::core mcutest::mcutest)
mcutrace_host_options(mcutrace_cli_tests)
add_test(NAME mcutrace_cli_tests COMMAND mcutrace_cli_tests)

add_executable(mcutrace_output_tests test_output.cpp)
target_link_libraries(mcutrace_output_tests PRIVATE mcutrace::core mcutest::mcutest)
mcutrace_host_options(mcutrace_output_tests)
add_test(NAME mcutrace_output_tests COMMAND mcutrace_output_tests)

add_executable(mcutrace_evidence_expectation_tests test_evidence_expectations.cpp)
target_link_libraries(mcutrace_evidence_expectation_tests PRIVATE mcutrace::core mcutest::mcutest)
mcutrace_host_options(mcutrace_evidence_expectation_tests)
add_test(NAME mcutrace_evidence_expectation_tests COMMAND mcutrace_evidence_expectation_tests)

if(MCUTRACE_ENABLE_MCUCOV)
    foreach(target IN ITEMS
        mcutrace_tests
        mcutrace_requirement_tests
        mcutrace_importer_tests
        mcutrace_producer_importer_tests
        mcutrace_trace_import_tests
        mcutrace_hardening_tests
        mcutrace_assembly_tests
        mcutrace_validation_tests
        mcutrace_config_tests
        mcutrace_cli_tests
        mcutrace_output_tests
        mcutrace_evidence_expectation_tests
    )
        target_sources(${target} PRIVATE coverage_dump.cpp)
        target_link_libraries(${target} PRIVATE mcucov::runtime)
    endforeach()
endif()
