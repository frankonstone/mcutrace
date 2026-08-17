// @req-file REQ-0004 REQ-0054 REQ-0062 REQ-0065
#include <mcutrace/config.hpp>

#include <filesystem>
#include <string>

#include <mcutoml/mcutoml.hpp>

namespace mcutrace {
namespace {

std::string canonical_path_string(std::filesystem::path path) {
    path = path.lexically_normal();
    while (path.has_filename() == false && path.has_parent_path() && path != path.root_path()) {
        path = path.parent_path();
    }
    return path.generic_string();
}

std::string normalize_path(std::string_view value, std::string_view base) {
    std::filesystem::path path(value);
    if (path.is_relative()) {
        path = std::filesystem::path(base) / path;
    }
    return canonical_path_string(std::move(path));
}

std::string config_directory(std::string_view config_path) {
    const std::filesystem::path path(config_path);
    const auto parent = path.parent_path();
    return parent.empty() ? std::string(".") : canonical_path_string(parent);
}

std::optional<Severity> parse_severity(std::string_view value) noexcept {
    if (value == "note") {
        return Severity::note;
    }
    if (value == "warning") {
        return Severity::warning;
    }
    if (value == "error") {
        return Severity::error;
    }
    return std::nullopt;
}

ConfigError config_error(ConfigErrorCode code, std::string detail) {
    return ConfigError{.code = code, .detail = std::move(detail), .source = std::nullopt};
}

ConfigError rule_error(ConfigErrorCode code,
                       std::string detail,
                       std::string_view config_path) {
    return ConfigError{
        .code = code,
        .detail = std::move(detail),
        .source = SourceLocation{.path = std::string(config_path), .line = 0, .column = 0},
    };
}

std::expected<void, ConfigError> read_rule_enabled(const mcutoml::TomlRef node,
                                                   ValidationRule& rule,
                                                   std::string_view config_path) {
    const auto enabled = node["enabled"];
    if (!enabled.valid()) {
        return {};
    }
    if (!enabled.is_bool()) {
        return std::unexpected(rule_error(ConfigErrorCode::invalid_type,
                                          "validation rule enabled must be boolean",
                                          config_path));
    }
    rule.enabled = enabled.get<bool>();
    return {};
}

std::expected<void, ConfigError> read_rule_severity(const mcutoml::TomlRef node,
                                                    ValidationRule& rule,
                                                    std::string_view config_path) {
    const auto severity = node["severity"];
    if (!severity.valid()) {
        return {};
    }
    if (!severity.is_string()) {
        return std::unexpected(rule_error(ConfigErrorCode::invalid_type,
                                          "validation rule severity must be a string",
                                          config_path));
    }
    const auto parsed = parse_severity(severity.get<std::string_view>());
    if (!parsed) {
        return std::unexpected(rule_error(ConfigErrorCode::invalid_value,
                                          "validation severity must be note, warning, or error",
                                          config_path));
    }
    rule.severity = *parsed;
    return {};
}

std::expected<void, ConfigError> read_rule(const mcutoml::TomlRef table,
                                           std::string_view name,
                                           ValidationRule& rule,
                                           std::string_view config_path) {
    const auto node = table[name];
    if (!node.valid()) {
        return {};
    }
    if (!node.is_table()) {
        return std::unexpected(rule_error(ConfigErrorCode::invalid_type,
                                          "validation rule '" + std::string(name) + "' must be a table",
                                          config_path));
    }
    if (auto status = read_rule_enabled(node, rule, config_path); !status) {
        return std::unexpected(status.error());
    }
    return read_rule_severity(node, rule, config_path);
}

std::expected<void, ConfigError> read_path_array(const mcutoml::TomlRef values,
                                                 std::string_view name,
                                                 std::string_view root,
                                                 std::vector<std::string>& result) {
    if (!values.valid()) {
        return {};
    }
    if (!values.is_array()) {
        return std::unexpected(config_error(ConfigErrorCode::invalid_type,
                                            std::string(name) + " must be an array"));
    }
    for (const auto entry : values) {
        if (!entry.is_string()) {
            return std::unexpected(config_error(ConfigErrorCode::invalid_type,
                                                std::string(name) + " entries must be strings"));
        }
        result.push_back(normalize_path(entry.get<std::string_view>(), root));
    }
    return {};
}

std::expected<void, ConfigError> read_project(const mcutoml::Toml& document,
                                              std::string_view config_base,
                                              ProjectConfig& result) {
    const auto project = document["project"];
    if (project.valid() && !project.is_table()) {
        return std::unexpected(config_error(ConfigErrorCode::invalid_type,
                                            "project must be a table"));
    }
    if (project.valid()) {
        const auto root = project["root"];
        if (root.valid() && !root.is_string()) {
            return std::unexpected(config_error(ConfigErrorCode::invalid_type,
                                                "project.root must be a string"));
        }
        if (root.valid()) {
            result.root = normalize_path(root.get<std::string_view>(), config_base);
        }
    }
    if (result.root.empty()) {
        result.root = normalize_path(".", config_base);
    }

    const auto requirements = project.valid() && project["requirements"].valid()
        ? project["requirements"] : document["requirements"];
    if (auto status = read_path_array(requirements, "requirements", result.root,
                                      result.requirement_files); !status) {
        return std::unexpected(status.error());
    }
    const auto sources = project.valid() && project["sources"].valid()
        ? project["sources"] : document["sources"];
    return read_path_array(sources, "sources", result.root, result.source_files);
}

std::expected<ArtifactConfig, ConfigError> read_artifact(const mcutoml::TomlRef entry,
                                                         std::string_view root) {
    if (!entry.is_table()) {
        return std::unexpected(config_error(ConfigErrorCode::invalid_type,
                                            "artifact entries must be tables"));
    }
    const auto path = entry["path"];
    if (!path.is_string()) {
        return std::unexpected(config_error(ConfigErrorCode::invalid_type,
                                            "artifact.path must be a string"));
    }

    ArtifactConfig artifact;
    artifact.path = normalize_path(path.get<std::string_view>(), root);
    const auto importer = entry["importer"];
    if (importer.valid() && !importer.is_string()) {
        return std::unexpected(config_error(ConfigErrorCode::invalid_type,
                                            "artifact.importer must be a string"));
    }
    if (importer.valid()) {
        artifact.importer = std::string(importer.get<std::string_view>());
    }
    const auto base = entry["base"];
    if (base.valid() && !base.is_string()) {
        return std::unexpected(config_error(ConfigErrorCode::invalid_type,
                                            "artifact.base must be a string"));
    }
    artifact.base_directory = base.valid()
        ? normalize_path(base.get<std::string_view>(), root)
        : std::string(root);
    return artifact;
}

std::expected<void, ConfigError> read_artifacts(const mcutoml::TomlRef artifacts,
                                                ProjectConfig& result) {
    if (!artifacts.valid()) {
        return {};
    }
    if (!artifacts.is_array()) {
        return std::unexpected(config_error(ConfigErrorCode::invalid_type,
                                            "artifacts must be an array of tables"));
    }
    for (const auto entry : artifacts) {
        auto artifact = read_artifact(entry, result.root);
        if (!artifact) {
            return std::unexpected(artifact.error());
        }
        result.artifacts.push_back(std::move(*artifact));
    }
    return {};
}

std::expected<void, ConfigError> read_fail_threshold(const mcutoml::TomlRef validation,
                                                     ValidationPolicy& policy) {
    const auto fail_at = validation["fail_at_or_above"];
    if (!fail_at.valid()) {
        return {};
    }
    if (!fail_at.is_string()) {
        return std::unexpected(config_error(ConfigErrorCode::invalid_type,
                                            "validation.fail_at_or_above must be a string"));
    }
    const auto severity = parse_severity(fail_at.get<std::string_view>());
    if (!severity) {
        return std::unexpected(config_error(ConfigErrorCode::invalid_value,
                                            "validation.fail_at_or_above is invalid"));
    }
    policy.fail_at_or_above = *severity;
    return {};
}

std::expected<void, ConfigError> read_validation_rules(const mcutoml::TomlRef validation,
                                                       ValidationPolicy& policy,
                                                       std::string_view config_path) {
    if (auto status = read_rule(validation, "dangling_reference", policy.dangling_reference, config_path); !status) {
        return std::unexpected(status.error());
    }
    if (auto status = read_rule(validation, "missing_test", policy.missing_test, config_path); !status) {
        return std::unexpected(status.error());
    }
    if (auto status = read_rule(validation, "missing_implementation", policy.missing_implementation, config_path); !status) {
        return std::unexpected(status.error());
    }
    if (auto status = read_rule(validation, "missing_coverage", policy.missing_coverage, config_path); !status) {
        return std::unexpected(status.error());
    }
    if (auto status = read_rule(validation, "failed_test", policy.failed_test, config_path); !status) {
        return std::unexpected(status.error());
    }
    return read_rule(validation, "static_finding", policy.static_finding, config_path);
}

std::expected<void, ConfigError> read_validation(const mcutoml::TomlRef validation,
                                                 ProjectConfig& result,
                                                 std::string_view config_path) {
    if (!validation.valid()) {
        return {};
    }
    if (!validation.is_table()) {
        return std::unexpected(config_error(ConfigErrorCode::invalid_type,
                                            "validation must be a table"));
    }
    if (auto status = read_fail_threshold(validation, result.validation); !status) {
        return std::unexpected(status.error());
    }
    return read_validation_rules(validation, result.validation, config_path);
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
    ProjectConfig result;
    if (auto status = read_project(document, config_directory(config_path), result); !status) {
        return std::unexpected(status.error());
    }
    if (auto status = read_artifacts(document["artifacts"], result); !status) {
        return std::unexpected(status.error());
    }
    if (auto status = read_validation(document["validation"], result, config_path); !status) {
        return std::unexpected(status.error());
    }
    return result;
}

}  // namespace mcutrace