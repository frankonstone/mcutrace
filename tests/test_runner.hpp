#pragma once

#include <cstdlib>

#include <mcutest/mcutest.hpp>

namespace mcutrace::test {

template <typename Runner>
int run(int argc, char* argv[], Runner& runner) {
    if (std::getenv("MCUTRACE_JSON_TEST_OUTPUT") != nullptr) {
        return mcutest::run_json_output(runner);
    }
    return mcutest::run_with_gtest_compat(argc, argv, runner);
}

}  // namespace mcutrace::test
