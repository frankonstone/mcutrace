#include <mcutrace/cli.hpp>
#include <mcutest/mcutest.hpp>

#include <string>

TEST(cli, parses_validate_config_and_explicit_inputs, "REQ-0065", "REQ-0066", "REQ-0067", "REQ-0093") {
    const char* argv[] = {
        "mcutrace", "--config", "project.toml", "validate",
        "--requirement", "reqs.md", "--requirement", "more.md",
        "--source", "src/feature.cpp", "--source", "include/feature.hpp",
        "--artifact", "out.json", "--format", "json",
    };
    const auto result = mcutrace::parse_cli(static_cast<int>(std::size(argv)), argv);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->action, mcutrace::CliAction::validate);
    ASSERT_EQ(result->output_format, mcutrace::OutputFormat::json);
    ASSERT_EQ(result->config_path, std::string("project.toml"));
    ASSERT_EQ(result->requirement_files.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(result->source_files.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(result->artifact_files.size(), static_cast<std::size_t>(1));
}

TEST(cli, explicit_inputs_do_not_require_default_config, "REQ-0066", "REQ-0093") {
    const char* argv[] = {"mcutrace", "validate", "--requirement", "reqs.md", "--source", "src/foo.cpp"};
    const auto result = mcutrace::parse_cli(static_cast<int>(std::size(argv)), argv);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->config_path.empty());
    ASSERT_EQ(result->requirement_files.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(result->source_files.size(), static_cast<std::size_t>(1));
}

TEST(cli, defaults_to_text_output, "REQ-0056", "REQ-0067") {
    const char* argv[] = {"mcutrace", "validate"};
    const auto result = mcutrace::parse_cli(static_cast<int>(std::size(argv)), argv);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->output_format, mcutrace::OutputFormat::text);
}

TEST(cli, rejects_unknown_output_format, "REQ-0055", "REQ-0056") {
    const char* argv[] = {"mcutrace", "validate", "--format", "xml"};
    const auto result = mcutrace::parse_cli(static_cast<int>(std::size(argv)), argv);
    ASSERT_FALSE(result.has_value());
}

TEST(cli, exposes_version_event, "REQ-0068") {
    const char* argv[] = {"mcutrace", "--version"};
    const auto result = mcutrace::parse_cli(static_cast<int>(std::size(argv)), argv);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->action, mcutrace::CliAction::version);
}

TEST(cli, exposes_command_help, "REQ-0067", "REQ-0093") {
    const char* argv[] = {"mcutrace", "validate", "--help"};
    const auto result = mcutrace::parse_cli(static_cast<int>(std::size(argv)), argv);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->action, mcutrace::CliAction::help);
    ASSERT_FALSE(result->help_text.empty());
    ASSERT_NE(result->help_text.find("--source"), std::string::npos);
}

TEST(cli, requires_validate_command_for_invocation, "REQ-0067") {
    const char* argv[] = {"mcutrace"};
    const auto result = mcutrace::parse_cli(static_cast<int>(std::size(argv)), argv);
    ASSERT_FALSE(result.has_value());
}

int main(int argc, char* argv[]) {
    mcutest::Runner<mcutest::JsonOutput> runner;
    return mcutest::run_with_gtest_compat(argc, argv, runner);
}
