cmake_minimum_required(VERSION 3.25)

project(
    mcutrace
    VERSION 0.1.0
    DESCRIPTION "Traceability aggregation and validation for embedded C/C++ projects"
    LANGUAGES CXX
)

option(MCUTRACE_BUILD_TESTS "Build mcutrace tests" ON)
option(MCUTRACE_DISABLE_EXCEPTIONS_RTTI "Disable exceptions and RTTI" ON)

set(MCUTRACE_MCUCLI_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../mcucli" CACHE PATH "Path to mcucli checkout")
set(MCUTRACE_MCUJSON_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../mcujson" CACHE PATH "Path to mcujson checkout")
set(MCUTRACE_MCUTOML_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../mcutoml" CACHE PATH "Path to mcutoml checkout")
set(MCUTRACE_MCUTEST_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../mcutest" CACHE PATH "Path to mcutest checkout")

function(mcutrace_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

function(mcutrace_host_options target)
    target_compile_features(${target} PUBLIC cxx_std_23)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    mcutrace_warnings(${target})
    if(MCUTRACE_DISABLE_EXCEPTIONS_RTTI)
        if(MSVC)
            target_compile_options(${target} PUBLIC /EHs-c- /GR-)
        else()
            target_compile_options(${target} PUBLIC -fno-exceptions -fno-rtti)
        endif()
    endif()
endfunction()

foreach(dep IN ITEMS mcucli mcujson mcutoml)
    string(TOUPPER "${dep}" DEP_UPPER)
    if(NOT TARGET ${dep}::${dep} AND EXISTS "${MCUTRACE_${DEP_UPPER}_SOURCE_DIR}/CMakeLists.txt")
        set(${DEP_UPPER}_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(${DEP_UPPER}_INSTALL OFF CACHE BOOL "" FORCE)
        add_subdirectory("${MCUTRACE_${DEP_UPPER}_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/${dep}" EXCLUDE_FROM_ALL)
    endif()
endforeach()

add_library(mcutrace_core STATIC
    src/assembly.cpp
    src/cli.cpp
    src/config.cpp
    src/importer.cpp
    src/model.cpp
    src/output.cpp
    src/requirements.cpp
    src/validation.cpp
)
add_library(mcutrace::core ALIAS mcutrace_core)
target_include_directories(mcutrace_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(mcutrace_core PUBLIC mcucli::mcucli mcujson::mcujson mcutoml::mcutoml)
mcutrace_host_options(mcutrace_core)

add_executable(mcutrace src/main.cpp)
target_link_libraries(mcutrace PRIVATE mcutrace::core)
mcutrace_host_options(mcutrace)

if(MCUTRACE_BUILD_TESTS)
    enable_testing()
    if(NOT TARGET mcutest::mcutest AND EXISTS "${MCUTRACE_MCUTEST_SOURCE_DIR}/CMakeLists.txt")
        set(MCUTEST_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(MCUTEST_INSTALL OFF CACHE BOOL "" FORCE)
        add_subdirectory("${MCUTRACE_MCUTEST_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/mcutest" EXCLUDE_FROM_ALL)
    endif()
    if(NOT TARGET mcutest::mcutest)
        message(FATAL_ERROR "mcutrace tests require mcutest; set MCUTRACE_MCUTEST_SOURCE_DIR")
    endif()
    add_subdirectory(tests)
endif()
