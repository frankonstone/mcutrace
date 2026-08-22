#include <mcutrace/cli.hpp>
#include <mcutest/mcutest.hpp>

#include "test_runner.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace {

class StreamCapture final {
public:
    StreamCapture()
        : out_(std::cout.rdbuf(out_stream_.rdbuf())),
          err_(std::cerr.rdbuf(err_stream_.rdbuf())) {}

    ~StreamCapture() {
        std::cout.rdbuf(out_);
        std::cerr.rdbuf(err_);
    }

    StreamCapture(const StreamCapture&) = delete;
    StreamCapture& operator=(const StreamCapture&) = delete;

    [[nodiscard]] std::string output() const { return out_stream_.str(); }

private:
    std::ostringstream out_stream_;
    std::ostringstream err_stream_;
    std::streambuf* out_;
    std::streambuf* err_;
};

}  // namespace

namespace {

class TemporaryProject final {
public:
    TemporaryProject() {
        std::error_code error;
        root_ = std::filesystem::temp_directory_path(error) / "mcutrace-wildcard-cli";
        if (error) {
            setup_error_ = error;
            return;
        }
        std::filesystem::remove_all(root_, error);
        if (error) {
            setup_error_ = error;
            return;
        }
        std::filesystem::create_directories(root_ / "requirements" / "nested", error);
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

TEST(cli, parses_validate_config_and_explicit_inputs, "REQ-0064", "REQ-0065", "REQ-0066", "REQ-0067", "REQ-0081", "REQ-0093") {
    const char* argv[] = {
        "mcutrace", "--config", "project.toml", "validate",
        "--requirement", "reqs.md", "--requirement", "more.md",
        "--source", "src/feature.cpp", "--source", "include/feature.hpp",
        "--build", "CMakeLists.txt",
        "--artifact", "out.json", "--format", "json",
    };
    const auto result = mcutrace::parse_cli(static_cast<int>(std::size(argv)), argv);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->action, mcutrace::CliAction::validate);
    ASSERT_EQ(result->output_format, mcutrace::OutputFormat::json);
    ASSERT_EQ(result->config_path, std::string("project.toml"));
    ASSERT_EQ(result->requirement_files.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(result->source_files.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(result->build_files.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(result->artifact_files.size(), static_cast<std::size_t>(1));
}

TEST(cli, explicit_inputs_do_not_require_default_config, "REQ-0064", "REQ-0066", "REQ-0093") {
    const char* argv[] = {"mcutrace", "validate", "--requirement", "reqs.md", "--source", "src/foo.cpp"};
    const auto result = mcutrace::parse_cli(static_cast<int>(std::size(argv)), argv);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(result->config_path.empty());
    ASSERT_EQ(result->requirement_files.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(result->source_files.size(), static_cast<std::size_t>(1));
}

TEST(cli, defaults_to_text_output, "REQ-0056", "REQ-0064", "REQ-0067") {
    const char* argv[] = {"mcutrace", "validate"};
    const auto result = mcutrace::parse_cli(static_cast<int>(std::size(argv)), argv);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->output_format, mcutrace::OutputFormat::text);
}

TEST(cli, parses_requirement_evidence_query, "REQ-0055", "REQ-0064") {
    const char* argv[] = {"mcutrace", "--config", "mcutrace.toml", "show", "REQ-0001", "--format", "json"};
    const auto result = mcutrace::parse_cli(static_cast<int>(std::size(argv)), argv);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->action, mcutrace::CliAction::show);
    ASSERT_EQ(result->requirement_id, std::string("REQ-0001"));
    ASSERT_EQ(result->output_format, mcutrace::OutputFormat::json);
}

TEST(cli, rejects_unknown_output_format, "REQ-0055", "REQ-0056", "REQ-0064") {
    const char* argv[] = {"mcutrace", "validate", "--format", "xml"};
    const auto result = mcutrace::parse_cli(static_cast<int>(std::size(argv)), argv);
    ASSERT_FALSE(result.has_value());
}

TEST(cli, exposes_version_event, "REQ-0064", "REQ-0068") {
    const char* argv[] = {"mcutrace", "--version"};
    const auto result = mcutrace::parse_cli(static_cast<int>(std::size(argv)), argv);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->action, mcutrace::CliAction::version);
    const StreamCapture capture;
    ASSERT_EQ(mcutrace::run_cli(*result), 0);
}

TEST(cli, exposes_command_help, "REQ-0064", "REQ-0067", "REQ-0093") {
    const char* argv[] = {"mcutrace", "validate", "--help"};
    const auto result = mcutrace::parse_cli(static_cast<int>(std::size(argv)), argv);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->action, mcutrace::CliAction::help);
    ASSERT_FALSE(result->help_text.empty());
    ASSERT_NE(result->help_text.find("--source"), std::string::npos);
    const StreamCapture capture;
    ASSERT_EQ(mcutrace::run_cli(*result), 0);
}

TEST(cli, exposes_evidence_query_help, "REQ-0055", "REQ-0064") {
    const char* argv[] = {"mcutrace", "show", "--help"};
    const auto result = mcutrace::parse_cli(static_cast<int>(std::size(argv)), argv);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->action, mcutrace::CliAction::help);
    ASSERT_NE(result->help_text.find("REQ-NNNN"), std::string::npos);
}

TEST(cli, validates_empty_explicit_project, "REQ-0053", "REQ-0064", "REQ-0067") {
    mcutrace::CliOptions options;
    options.action = mcutrace::CliAction::validate;
    options.output_format = mcutrace::OutputFormat::text;
    const StreamCapture capture;
    ASSERT_EQ(mcutrace::run_cli(options), 0);
}

TEST(cli, reports_missing_config_as_execution_error, "REQ-0053", "REQ-0064", "REQ-0065") {
    mcutrace::CliOptions options;
    options.action = mcutrace::CliAction::validate;
    options.config_path = "definitely-missing-mcutrace.toml";
    const StreamCapture capture;
    ASSERT_EQ(mcutrace::run_cli(options), 2);
}

TEST(cli, requires_validate_command_for_invocation, "REQ-0064", "REQ-0067") {
    const char* argv[] = {"mcutrace"};
    const auto result = mcutrace::parse_cli(static_cast<int>(std::size(argv)), argv);
    ASSERT_FALSE(result.has_value());
}

TEST(cli, expands_wildcard_requirement_and_source_overrides, "REQ-0125") {
    TemporaryProject project;
    ASSERT_FALSE(project.setup_error());
    project.write(project.root() / "requirements" / "top.md",
                 "## REQ-0001 Top @evidence(none)\nBody\n");
    project.write(project.root() / "requirements" / "nested" / "deep.md",
                 "## REQ-0002 Deep @evidence(none)\nBody\n");
    project.write(project.root() / "src" / "nested" / "deep.cpp",
                 "// @req REQ-0002\nvoid deep() {}\n");

    const std::string requirement_pattern =
        (project.root() / "requirements" / "**" / "*.md").generic_string();
    const std::string source_pattern =
        (project.root() / "src" / "**" / "*.cpp").generic_string();
    const char* argv[] = {"mcutrace", "show", "REQ-0002", "--requirement",
                          requirement_pattern.c_str(), "--source", source_pattern.c_str()};
    const auto options = mcutrace::parse_cli(static_cast<int>(std::size(argv)), argv);
    ASSERT_TRUE(options.has_value());
    ASSERT_EQ(options->requirement_files.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(options->source_files.size(), static_cast<std::size_t>(1));

    const StreamCapture capture;
    ASSERT_EQ(mcutrace::run_cli(*options), 0);
    ASSERT_NE(capture.output().find((project.root() / "src" / "nested" / "deep.cpp").generic_string()),
              std::string::npos);
}

int main(int argc, char* argv[]) {
    mcutest::Runner<mcutest::JsonOutput> runner;
    return mcutrace::test::run(argc, argv, runner);
}
