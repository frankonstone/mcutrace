#include <mcutrace/cli.hpp>
#include <mcutest/mcutest.hpp>

#include <string>

TEST(cli, parses_validate_config_and_explicit_inputs) {
    const char* argv[] = {
        "mcutrace", "--config", "project.toml", "validate",
        "--requirement", "reqs.md", "--requirement", "more.md",
        "--artifact", "out.json",
    };
    const auto result = mcutrace::parse_cli(static_cast<int>(std::size(argv)), argv);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->action, mcutrace::CliAction::validate);
    ASSERT_EQ(result->config_path, std::string("project.toml"));
    ASSERT_EQ(result->requirement_files.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(result->artifact_files.size(), static_cast<std::size_t>(1));
}

TEST(cli, exposes_version_event) {
    const char* argv[] = {"mcutrace", "--version"};
    const auto result = mcutrace::parse_cli(static_cast<int>(std::size(argv)), argv);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->action, mcutrace::CliAction::version);
}

TEST(cli, exposes_command_help) {
    const char* argv[] = {"mcutrace", "validate", "--help"};
    const auto result = mcutrace::parse_cli(static_cast<int>(std::size(argv)), argv);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->action, mcutrace::CliAction::help);
    ASSERT_FALSE(result->help_text.empty());
}

TEST(cli, requires_validate_command_for_invocation) {
    const char* argv[] = {"mcutrace"};
    const auto result = mcutrace::parse_cli(static_cast<int>(std::size(argv)), argv);
    ASSERT_FALSE(result.has_value());
}

int main(int argc, char* argv[]) {
    mcutest::Runner<mcutest::StdoutOutput> runner;
    return mcutest::run_with_gtest_compat(argc, argv, runner);
}
