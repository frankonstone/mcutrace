#include <mcutrace/config.hpp>
#include <mcutest/mcutest.hpp>

#include "test_runner.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace {

class TemporaryProject final {
public:
    TemporaryProject() {
        std::error_code error;
        root_ = std::filesystem::temp_directory_path(error) / "mcutrace-wildcard-config";
        if (error) {
            setup_error_ = error;
            return;
        }
        std::filesystem::remove_all(root_, error);
        if (error) {
            setup_error_ = error;
            return;
        }
        std::filesystem::create_directories(root_ / "docs" / "nested", error);
        if (error) {
            setup_error_ = error;
            return;
        }
        std::filesystem::create_directories(root_ / "src" / "nested", error);
        setup_error_ = error;
    }

    ~TemporaryProject() {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    TemporaryProject(const TemporaryProject&) = delete;
    TemporaryProject& operator=(const TemporaryProject&) = delete;

    void write(const std::filesystem::path& path, std::string_view content) const {
        std::ofstream stream(path);
        stream << content;
    }

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }
    [[nodiscard]] const std::error_code& setup_error() const noexcept { return setup_error_; }

private:
    std::filesystem::path root_;
    std::error_code setup_error_;
};

}  // namespace

TEST(config, parses_project_inputs_and_normalizes_paths, "REQ-0004", "REQ-0044", "REQ-0054", "REQ-0062", "REQ-0065", "REQ-0093") {
    constexpr auto text = R"toml(
[project]
root = ".."

requirements = ["docs/requirements.md", "spec/feature.md"]
sources = ["src/feature.cpp", "include/feature.hpp"]
build_files = ["CMakeLists.txt", "cmake/dependencies.cmake"]

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
    ASSERT_EQ(config->source_files.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(config->source_files[0], std::string("/work/project/src/feature.cpp"));
    ASSERT_EQ(config->build_files.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(config->build_files[0], std::string("/work/project/CMakeLists.txt"));
    ASSERT_EQ(config->build_files[1], std::string("/work/project/cmake/dependencies.cmake"));
    ASSERT_EQ(config->source_files[1], std::string("/work/project/include/feature.hpp"));
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

[validation.missing_coverage]
excluded_paths = ["src/main.cpp"]
)toml";

    const auto config = mcutrace::parse_project_config(text, "/work/project/mcutrace.toml");
    ASSERT_TRUE(config.has_value());
    ASSERT_EQ(config->validation.fail_at_or_above, mcutrace::Severity::warning);
    ASSERT_FALSE(config->validation.missing_test.enabled);
    ASSERT_EQ(config->validation.missing_test.severity, mcutrace::Severity::error);
    ASSERT_TRUE(config->validation.failed_test.enabled);
    ASSERT_EQ(config->validation.failed_test.severity, mcutrace::Severity::warning);
    ASSERT_EQ(config->validation.missing_coverage.excluded_paths.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(config->validation.missing_coverage.excluded_paths[0],
              std::string("/work/project/src/main.cpp"));
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

TEST(config, reports_toml_parse_failure, "REQ-0004", "REQ-0035", "REQ-0062") {
    const auto config = mcutrace::parse_project_config("[project\nroot = 42");
    ASSERT_FALSE(config.has_value());
    ASSERT_EQ(config.error().code, mcutrace::ConfigErrorCode::parse_failed);
}

TEST(config, expands_recursive_requirement_and_source_patterns, "REQ-0125") {
    TemporaryProject project;
    ASSERT_FALSE(project.setup_error());
    project.write(project.root() / "docs" / "top.md", "## REQ-0001 Top\nBody\n");
    project.write(project.root() / "docs" / "nested" / "deep.md",
                 "## REQ-0002 Deep\nBody\n");
    project.write(project.root() / "docs" / "nested" / "ignored.txt", "not a requirement\n");
    project.write(project.root() / "src" / "top.cpp", "int top() { return 0; }\n");
    project.write(project.root() / "src" / "nested" / "deep.cpp",
                 "int deep() { return 0; }\n");

    const std::string text =
        "[project]\n"
        "root = \"" + project.root().generic_string() + "\"\n"
        "requirements = [\"./docs/**/*.md\"]\n"
        "sources = [\"./src/**/*.cpp\"]\n";
    const auto config = mcutrace::parse_project_config(
        text, (project.root() / "mcutrace.toml").generic_string());

    ASSERT_TRUE(config.has_value());
    ASSERT_EQ(config->requirement_files.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(config->requirement_files[0],
              (project.root() / "docs" / "nested" / "deep.md").generic_string());
    ASSERT_EQ(config->requirement_files[1],
              (project.root() / "docs" / "top.md").generic_string());
    ASSERT_EQ(config->source_files.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(config->source_files[0],
              (project.root() / "src" / "nested" / "deep.cpp").generic_string());
    ASSERT_EQ(config->source_files[1],
              (project.root() / "src" / "top.cpp").generic_string());
}

TEST(config, rejects_unmatched_wildcard_pattern, "REQ-0125") {
    TemporaryProject project;
    ASSERT_FALSE(project.setup_error());
    const std::string text =
        "[project]\n"
        "root = \"" + project.root().generic_string() + "\"\n"
        "requirements = [\"./docs/**/*.rst\"]\n";

    const auto config = mcutrace::parse_project_config(
        text, (project.root() / "mcutrace.toml").generic_string());

    ASSERT_FALSE(config.has_value());
    ASSERT_EQ(config.error().code, mcutrace::ConfigErrorCode::invalid_value);
    ASSERT_NE(config.error().detail.find("matched no regular files"), std::string::npos);
}

int main(int argc, char* argv[]) {
    mcutest::Runner<mcutest::JsonOutput> runner;
    return mcutrace::test::run(argc, argv, runner);
}
