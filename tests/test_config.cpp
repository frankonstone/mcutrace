#include <mcutrace/config.hpp>
#include <mcutest/mcutest.hpp>

#include <string>

TEST(config, parses_project_inputs_and_normalizes_paths, "REQ-0004", "REQ-0044", "REQ-0054", "REQ-0065") {
    constexpr auto text = R"toml(
[project]
root = ".."

requirements = ["docs/requirements.md", "spec/feature.md"]

[[artifacts]]
path = "build/test.json"
importer = "mcutest"
base = "build"

[[artifacts]]
path = "build/cov.json"
)toml";

    const auto config = mcutrace::parse_project_config(text, "/work/project/config/mcutrace.toml");
    ASSERT_TRUE(config.has_value());
    ASSERT_EQ(config->root, std::string("/work/project"));
    ASSERT_EQ(config->requirement_files.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(config->requirement_files[0], std::string("/work/project/docs/requirements.md"));
    ASSERT_EQ(config->artifacts.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(config->artifacts[0].path, std::string("/work/project/build/test.json"));
    ASSERT_EQ(config->artifacts[0].importer, std::string("mcutest"));
    ASSERT_EQ(config->artifacts[0].base_directory, std::string("/work/project/build"));
    ASSERT_EQ(config->artifacts[1].base_directory, std::string("/work/project"));
}

TEST(config, maps_validation_policy, "REQ-0052", "REQ-0054") {
    constexpr auto text = R"toml(
[validation]
fail_at_or_above = "warning"

[validation.missing_test]
enabled = false
severity = "error"

[validation.failed_test]
severity = "warning"
)toml";

    const auto config = mcutrace::parse_project_config(text, "mcutrace.toml");
    ASSERT_TRUE(config.has_value());
    ASSERT_EQ(config->validation.fail_at_or_above, mcutrace::Severity::warning);
    ASSERT_FALSE(config->validation.missing_test.enabled);
    ASSERT_EQ(config->validation.missing_test.severity, mcutrace::Severity::error);
    ASSERT_TRUE(config->validation.failed_test.enabled);
    ASSERT_EQ(config->validation.failed_test.severity, mcutrace::Severity::warning);
}

TEST(config, rejects_invalid_validation_severity, "REQ-0052", "REQ-0054") {
    constexpr auto text = R"toml(
[validation.failed_test]
severity = "critical"
)toml";
    const auto config = mcutrace::parse_project_config(text);
    ASSERT_FALSE(config.has_value());
    ASSERT_EQ(config.error().code, mcutrace::ConfigErrorCode::invalid_value);
}

TEST(config, reports_toml_parse_failure, "REQ-0004", "REQ-0035") {
    const auto config = mcutrace::parse_project_config("[project\nroot = 42");
    ASSERT_FALSE(config.has_value());
    ASSERT_EQ(config.error().code, mcutrace::ConfigErrorCode::parse_failed);
}

int main(int argc, char* argv[]) {
    mcutest::Runner<mcutest::JsonOutput> runner;
    return mcutest::run_with_gtest_compat(argc, argv, runner);
}
