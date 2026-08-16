#include <mcutrace/config.hpp>

#include <filesystem>
#include <string>

#include <mcutoml/mcutoml.hpp>

namespace mcutrace {
namespace {

std::string normalize_path(std::string_view value, std::string_view base) {
    std::filesystem::path path(value);
    if (path.is_relative()) path = std::filesystem::path(base) / path;
    return path.lexically_normal().generic_string();
}

std::string config_directory(std::string_view config_path) {
    const std::filesystem::path path(config_path);
    const auto parent = path.parent_path();
    return parent.empty() ? std::string(".") : parent.lexically_normal().generic_string();
}

std::optional<Severity> parse_severity(std::string_view value) noexcept {
    if (value == "note") return Severity::note;
    if (value == "warning") return Severity::warning;
    if (value == "error") return Severity::error;
    return std::nullopt;
}

std::expected<void, ConfigError> read_rule(const mcutoml::TomlRef table,
                                           std::string_view name,
                                           ValidationRule& rule,
                                           std::string_view config_path) {
    const auto node = table[name];
    if (!node.valid()) return {};
    if (!node.is_table()) {
        return std::unexpected(ConfigError{
            .code = ConfigErrorCode::invalid_type,
            .detail = "validation rule '" + std::string(name) + "' must be a table",
            .source = SourceLocation{.path = std::string(config_path), .line = 0, .column = 0},
        });
    }
    const auto enabled = node["enabled"];
    if (enabled.valid()) {
        if (!enabled.is_bool()) {
            return std::unexpected(ConfigError{
                .code = ConfigErrorCode::invalid_type,
                .detail = "validation rule enabled must be boolean",
                .source = SourceLocation{.path = std::string(config_path), .line = 0, .column = 0},
            });
        }
        rule.enabled = enabled.get<bool>();
    }
    const auto severity = node["severity"];
    if (severity.valid()) {
        if (!severity.is_string()) {
            return std::unexpected(ConfigError{
                .code = ConfigErrorCode::invalid_type,
                .detail = "validation rule severity must be a string",
                .source = SourceLocation{.path = std::string(config_path), .line = 0, .column = 0},
            });
        }
        const auto parsed = parse_severity(severity.get<std::string_view>());
        if (!parsed) {
            return std::unexpected(ConfigError{
                .code = ConfigErrorCode::invalid_value,
                .detail = "validation severity must be note, warning, or error",
                .source = SourceLocation{.path = std::string(config_path), .line = 0, .column = 0},
            });
        }
        rule.severity = *parsed;
    }
    return {};
}

}  // namespace

std::expected<ProjectConfig, ConfigError>
parse_project_config(std::string_view content, std::string_view config_path) {
    const auto parsed = mcutoml::Toml::parse(content);
    if (!parsed) {
        return std::unexpected(ConfigError{
            .code = ConfigErrorCode::parse_failed,
            .detail = "failed to parse TOML configuration",
            .source = SourceLocation{.path = std::string(config_path), .line = 0, .column = 0},
        });
    }

    const auto document = *parsed;
    const std::string config_base = config_directory(config_path);
    ProjectConfig result;

    const auto project = document["project"];
    if (project.valid()) {
        if (!project.is_table()) {
            return std::unexpected(ConfigError{.code = ConfigErrorCode::invalid_type,
                                               .detail = "project must be a table"});
        }
        const auto root = project["root"];
        if (root.valid()) {
            if (!root.is_string()) {
                return std::unexpected(ConfigError{.code = ConfigErrorCode::invalid_type,
                                                   .detail = "project.root must be a string"});
            }
            result.root = normalize_path(root.get<std::string_view>(), config_base);
        }
    }
    if (result.root.empty()) result.root = normalize_path(".", config_base);

    const auto requirements = document["requirements"];
    if (requirements.valid()) {
        if (!requirements.is_array()) {
            return std::unexpected(ConfigError{.code = ConfigErrorCode::invalid_type,
                                               .detail = "requirements must be an array"});
        }
        for (const auto entry : requirements) {
            if (!entry.is_string()) {
                return std::unexpected(ConfigError{.code = ConfigErrorCode::invalid_type,
                                                   .detail = "requirements entries must be strings"});
            }
            result.requirement_files.push_back(
                normalize_path(entry.get<std::string_view>(), result.root));
        }
    }

    const auto artifacts = document["artifacts"];
    if (artifacts.valid()) {
        if (!artifacts.is_array()) {
            return std::unexpected(ConfigError{.code = ConfigErrorCode::invalid_type,
                                               .detail = "artifacts must be an array of tables"});
        }
        for (const auto entry : artifacts) {
            if (!entry.is_table()) {
                return std::unexpected(ConfigError{.code = ConfigErrorCode::invalid_type,
                                                   .detail = "artifact entries must be tables"});
            }
            const auto path = entry["path"];
            if (!path.is_string()) {
                return std::unexpected(ConfigError{.code = ConfigErrorCode::invalid_type,
                                                   .detail = "artifact.path must be a string"});
            }
            ArtifactConfig artifact;
            artifact.path = normalize_path(path.get<std::string_view>(), result.root);
            const auto importer = entry["importer"];
            if (importer.valid()) {
                if (!importer.is_string()) {
                    return std::unexpected(ConfigError{.code = ConfigErrorCode::invalid_type,
                                                       .detail = "artifact.importer must be a string"});
                }
                artifact.importer = std::string(importer.get<std::string_view>());
            }
            const auto base = entry["base"];
            artifact.base_directory = base.valid()
                ? normalize_path(base.get<std::string_view>(), result.root)
                : result.root;
            result.artifacts.push_back(std::move(artifact));
        }
    }

    const auto validation = document["validation"];
    if (validation.valid()) {
        if (!validation.is_table()) {
            return std::unexpected(ConfigError{.code = ConfigErrorCode::invalid_type,
                                               .detail = "validation must be a table"});
        }
        const auto fail_at = validation["fail_at_or_above"];
        if (fail_at.valid()) {
            if (!fail_at.is_string()) {
                return std::unexpected(ConfigError{.code = ConfigErrorCode::invalid_type,
                                                   .detail = "validation.fail_at_or_above must be a string"});
            }
            const auto severity = parse_severity(fail_at.get<std::string_view>());
            if (!severity) {
                return std::unexpected(ConfigError{.code = ConfigErrorCode::invalid_value,
                                                   .detail = "validation.fail_at_or_above is invalid"});
            }
            result.validation.fail_at_or_above = *severity;
        }
        if (auto status = read_rule(validation, "dangling_reference", result.validation.dangling_reference, config_path); !status) return std::unexpected(status.error());
        if (auto status = read_rule(validation, "missing_test", result.validation.missing_test, config_path); !status) return std::unexpected(status.error());
        if (auto status = read_rule(validation, "missing_implementation", result.validation.missing_implementation, config_path); !status) return std::unexpected(status.error());
        if (auto status = read_rule(validation, "missing_coverage", result.validation.missing_coverage, config_path); !status) return std::unexpected(status.error());
        if (auto status = read_rule(validation, "failed_test", result.validation.failed_test, config_path); !status) return std::unexpected(status.error());
        if (auto status = read_rule(validation, "static_finding", result.validation.static_finding, config_path); !status) return std::unexpected(status.error());
    }

    return result;
}

}  // namespace mcutrace
