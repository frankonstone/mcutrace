#pragma once

#include <expected>
#include <string>
#include <vector>

namespace mcutrace {

enum class CliAction { validate, help, version };
enum class OutputFormat { text, json };

struct CliOptions final {
    CliAction action = CliAction::validate;
    OutputFormat output_format = OutputFormat::text;
    std::string config_path = "mcutrace.toml";
    std::vector<std::string> requirement_files;
    std::vector<std::string> artifact_files;
    std::string help_text;
};

struct CliError final { std::string message; };

[[nodiscard]] std::expected<CliOptions, CliError> parse_cli(int argc, const char* const* argv);
[[nodiscard]] int run_cli(const CliOptions& options);

}  // namespace mcutrace
