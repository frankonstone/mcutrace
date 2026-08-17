// @req-file REQ-0004 REQ-0054 REQ-0062 REQ-0065 REQ-0093
#pragma once

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <mcutrace/error.hpp>
#include <mcutrace/validation.hpp>

namespace mcutrace {

struct ArtifactConfig final {
    std::string path;
    std::string importer;
    std::string base_directory;

    friend bool operator==(const ArtifactConfig&, const ArtifactConfig&) = default;
};

struct ProjectConfig final {
    std::string root;
    std::vector<std::string> requirement_files;
    std::vector<std::string> source_files;
    std::vector<ArtifactConfig> artifacts;
    ValidationPolicy validation;
};

enum class ConfigErrorCode : std::uint8_t {
    parse_failed,
    invalid_type,
    invalid_value,
};

struct ConfigError final {
    ConfigErrorCode code = ConfigErrorCode::parse_failed;
    std::string detail;
    std::optional<SourceLocation> source;
};

[[nodiscard]] std::expected<ProjectConfig, ConfigError>
parse_project_config(std::string_view content,
                     std::string_view config_path = "mcutrace.toml");

}  // namespace mcutrace
