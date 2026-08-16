#include <mcutrace/producer_importers.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#ifndef MCUJSON_MAX_NODES
#define MCUJSON_MAX_NODES 8192
#endif
#ifndef MCUJSON_STR_BUF
#define MCUJSON_STR_BUF 65534
#endif
#include <mcujson/mcujson.hpp>

namespace mcutrace {
namespace {

constexpr std::string_view kMcutest = "mcutest";
constexpr std::string_view kMcucov = "mcucov";
constexpr std::string_view kMcucheck = "mcucheck";

std::optional<Severity> parse_severity(std::string_view value) noexcept {
    if (value == "note" || value == "info") return Severity::note;
    if (value == "warning") return Severity::warning;
    if (value == "error") return Severity::error;
    return std::nullopt;
}

std::string artifact_id(std::string_view producer, std::string_view path) {
    return "artifact:" + std::string(producer) + ":" + std::string(path);
}

bool contains_node(const std::vector<Node>& nodes, std::string_view id) {
    return std::any_of(nodes.begin(), nodes.end(), [id](const Node& node) {
        return node.id == id;
    });
}

void add_node_once(std::vector<Node>& nodes, Node node) {
    if (!contains_node(nodes, node.id)) nodes.push_back(std::move(node));
}

std::expected<std::string, ImportError>
source_path(std::string_view path, const ArtifactInput& input) {
    return normalize_artifact_path(path, input.base_directory);
}

void add_source_and_edge(ImportFragment& fragment,
                         std::string source,
                         std::string evidence_id,
                         RelationshipKind relationship,
                         const ArtifactInput& input,
                         std::uint32_t line = 0,
                         std::uint32_t column = 0) {
    const std::string source_id = "source:" + source;
    add_node_once(fragment.nodes, Node{
        .id = source_id,
        .kind = NodeKind::source,
        .label = source,
        .source = SourceLocation{.path = source, .line = line, .column = column},
    });
    fragment.edges.push_back(Edge{
        .source_id = std::move(evidence_id),
        .target_id = source_id,
        .type = RelationshipType::known(relationship),
        .provenance = Provenance{
            .importer = fragment.format.producer,
            .artifact = input.path,
        },
    });
}

std::expected<mcujson::Json, ImportError> parse_json(const ArtifactInput& input) {
    auto parsed = mcujson::Json::parse(input.content);
    if (!parsed) {
        return std::unexpected(ImportError{
            .code = ImportErrorCode::invalid_artifact,
            .detail = "invalid JSON artifact: " + input.path,
            .source = SourceLocation{.path = input.path},
        });
    }
    return std::move(*parsed);
}

std::expected<InputFormat, ImportError>
identify_json(const ArtifactInput& input) {
    auto parsed = parse_json(input);
    if (!parsed || !parsed->is_object() || !(*parsed)["format"].is_string() ||
        !(*parsed)["version"].is_int()) {
        return std::unexpected(ImportError{
            .code = ImportErrorCode::unrecognized_format,
            .detail = "artifact has no recognized format/version header: " + input.path,
            .source = SourceLocation{.path = input.path},
        });
    }
    const std::string format = (*parsed)["format"].get<std::string>();
    const auto version = (*parsed)["version"].get<long long>();
    std::string producer;
    if (format == "mcutest-results") producer = std::string(kMcutest);
    else if (format == "mcucov-report") producer = std::string(kMcucov);
    else if (format == "mcucheck-results") producer = std::string(kMcucheck);
    else {
        return std::unexpected(ImportError{
            .code = ImportErrorCode::unrecognized_format,
            .detail = "unrecognized producer format '" + format + "'",
            .source = SourceLocation{.path = input.path},
        });
    }
    return InputFormat{
        .producer = producer,
        .schema = format,
        .version = std::to_string(version),
    };
}

ImporterInfo info_for(std::string_view producer) {
    return ImporterInfo{
        .name = std::string(producer),
        .producer = std::string(producer),
        .supported_versions = {"1"},
    };
}

std::expected<ImportFragment, ImportError>
import_mcutest(const ArtifactInput& input, const mcujson::Json& root, InputFormat format) {
    if (auto supported = require_supported_version(info_for(kMcutest), format,
            SourceLocation{.path = input.path}); !supported) {
        return std::unexpected(supported.error());
    }
    const auto tests = root["tests"];
    if (!tests.is_array()) {
        return std::unexpected(ImportError{
            .code = ImportErrorCode::invalid_artifact,
            .detail = "mcutest-results requires a tests array",
            .source = SourceLocation{.path = input.path},
        });
    }

    ImportFragment fragment{.format = std::move(format)};
    fragment.artifacts.push_back(preserve_json_artifact(
        artifact_id(kMcutest, input.path), "mcutest-results", input.content,
        SourceLocation{.path = input.path}));

    for (const auto test : tests) {
        if (!test.is_object() || !test["name"].is_string() || !test["status"].is_string()) {
            fragment.diagnostics.push_back(Diagnostic{
                .code = "import.mcutest.invalid_test",
                .severity = Severity::warning,
                .message = "ignored malformed mcutest test entry",
                .source = SourceLocation{.path = input.path},
            });
            continue;
        }
        const std::string name = test["name"].get<std::string>();
        const std::string status = test["status"].get<std::string>();
        EvidenceState evidence = EvidenceState::unknown;
        if (status == "passed") evidence = EvidenceState::passed;
        else if (status == "failed") evidence = EvidenceState::failed;
        else {
            fragment.diagnostics.push_back(Diagnostic{
                .code = "import.mcutest.unknown_status",
                .severity = Severity::warning,
                .message = "unknown mcutest status for " + name + ": " + status,
                .source = SourceLocation{.path = input.path},
            });
        }
        Node node{
            .id = "test:mcutest:" + name,
            .kind = NodeKind::test,
            .label = name,
            .evidence_state = evidence,
        };
        if (test["file"].is_string()) {
            auto path = source_path(test["file"].get<std::string_view>(), input);
            if (path) {
                const auto line = test["line"].is_int()
                    ? static_cast<std::uint32_t>(test["line"].get<long long>()) : 0U;
                node.source = SourceLocation{.path = *path, .line = line};
            }
        }
        add_node_once(fragment.nodes, std::move(node));
    }
    return fragment;
}

std::expected<ImportFragment, ImportError>
import_mcucov(const ArtifactInput& input, const mcujson::Json& root, InputFormat format) {
    if (auto supported = require_supported_version(info_for(kMcucov), format,
            SourceLocation{.path = input.path}); !supported) {
        return std::unexpected(supported.error());
    }
    const auto modules = root["modules"];
    if (!modules.is_array()) {
        return std::unexpected(ImportError{
            .code = ImportErrorCode::invalid_artifact,
            .detail = "mcucov-report requires a modules array",
            .source = SourceLocation{.path = input.path},
        });
    }

    ImportFragment fragment{.format = std::move(format)};
    fragment.artifacts.push_back(preserve_json_artifact(
        artifact_id(kMcucov, input.path), "mcucov-report", input.content,
        SourceLocation{.path = input.path}));

    for (const auto module : modules) {
        if (!module.is_object() || !module["path"].is_string()) {
            fragment.diagnostics.push_back(Diagnostic{
                .code = "import.mcucov.invalid_module",
                .severity = Severity::warning,
                .message = "ignored malformed mcucov module entry",
                .source = SourceLocation{.path = input.path},
            });
            continue;
        }
        auto normalized = source_path(module["path"].get<std::string_view>(), input);
        if (!normalized) return std::unexpected(normalized.error());
        const std::string variant = module["variant"].is_string()
            ? module["variant"].get<std::string>() : std::string{};
        const std::string coverage_id = "coverage:mcucov:" + *normalized + ":" + variant;
        add_node_once(fragment.nodes, Node{
            .id = coverage_id,
            .kind = NodeKind::coverage,
            .label = module["path"].get<std::string>(),
            .source = SourceLocation{.path = *normalized},
        });
        add_source_and_edge(fragment, *normalized, coverage_id,
                            RelationshipKind::covers, input);

        const auto skipped = module["skipped"];
        if (skipped.is_array()) {
            for (const auto entry : skipped) {
                if (!entry.is_object() || !entry["state"].is_string() ||
                    entry["state"].get<std::string_view>() == "justified") continue;
                const std::string detail = entry["detail"].is_string()
                    ? entry["detail"].get<std::string>() : "construct was not instrumented";
                const std::string severity_text = entry["severity"].is_string()
                    ? entry["severity"].get<std::string>() : "warning";
                fragment.diagnostics.push_back(Diagnostic{
                    .code = "import.mcucov.not_instrumented",
                    .severity = parse_severity(severity_text).value_or(Severity::warning),
                    .message = detail,
                    .source = SourceLocation{
                        .path = *normalized,
                        .line = entry["line"].is_int()
                            ? static_cast<std::uint32_t>(entry["line"].get<long long>()) : 0U,
                        .column = entry["column"].is_int()
                            ? static_cast<std::uint32_t>(entry["column"].get<long long>()) : 0U,
                    },
                });
            }
        }
    }
    return fragment;
}

std::expected<ImportFragment, ImportError>
import_mcucheck(const ArtifactInput& input, const mcujson::Json& root, InputFormat format) {
    if (auto supported = require_supported_version(info_for(kMcucheck), format,
            SourceLocation{.path = input.path}); !supported) {
        return std::unexpected(supported.error());
    }
    const auto diagnostics = root["diagnostics"];
    if (!diagnostics.is_array()) {
        return std::unexpected(ImportError{
            .code = ImportErrorCode::invalid_artifact,
            .detail = "mcucheck-results requires a diagnostics array",
            .source = SourceLocation{.path = input.path},
        });
    }

    ImportFragment fragment{.format = std::move(format)};
    fragment.artifacts.push_back(preserve_json_artifact(
        artifact_id(kMcucheck, input.path), "mcucheck-results", input.content,
        SourceLocation{.path = input.path}));

    std::size_t fallback_index = 0;
    for (const auto diagnostic : diagnostics) {
        ++fallback_index;
        const auto location = diagnostic["location"];
        if (!diagnostic.is_object() || !diagnostic["rule_id"].is_string() ||
            !diagnostic["message"].is_string() || !location.is_object() ||
            !location["path"].is_string()) {
            fragment.diagnostics.push_back(Diagnostic{
                .code = "import.mcucheck.invalid_diagnostic",
                .severity = Severity::warning,
                .message = "ignored malformed mcucheck diagnostic entry",
                .source = SourceLocation{.path = input.path},
            });
            continue;
        }
        auto normalized = source_path(location["path"].get<std::string_view>(), input);
        if (!normalized) return std::unexpected(normalized.error());
        const auto line = location["line"].is_int()
            ? static_cast<std::uint32_t>(location["line"].get<long long>()) : 0U;
        const auto column = location["column"].is_int()
            ? static_cast<std::uint32_t>(location["column"].get<long long>()) : 0U;
        const std::string rule = diagnostic["rule_id"].get<std::string>();
        const std::string message = diagnostic["message"].get<std::string>();
        const std::string stable = diagnostic["id"].is_string()
            ? diagnostic["id"].get<std::string>()
            : rule + ":" + *normalized + ":" + std::to_string(line) + ":" +
              std::to_string(fallback_index);
        const std::string finding_id = "finding:mcucheck:" + stable;
        add_node_once(fragment.nodes, Node{
            .id = finding_id,
            .kind = NodeKind::finding,
            .label = rule + ": " + message,
            .source = SourceLocation{.path = *normalized, .line = line, .column = column},
        });
        add_source_and_edge(fragment, *normalized, finding_id,
                            RelationshipKind::reports, input, line, column);
    }
    return fragment;
}

}  // namespace

std::vector<ImporterInfo> producer_importer_info() {
    return {info_for(kMcutest), info_for(kMcucov), info_for(kMcucheck)};
}

std::expected<ImportFragment, ImportError>
import_producer_artifact(const ArtifactInput& input, std::string_view requested_importer) {
    auto format = identify_json(input);
    if (!format) return std::unexpected(format.error());
    if (!requested_importer.empty() && requested_importer != format->producer) {
        return std::unexpected(ImportError{
            .code = ImportErrorCode::unrecognized_format,
            .detail = "artifact identifies as '" + format->producer +
                      "' but importer '" + std::string(requested_importer) + "' was requested",
            .source = SourceLocation{.path = input.path},
        });
    }

    auto root = parse_json(input);
    if (!root) return std::unexpected(root.error());
    if (format->producer == kMcutest) return import_mcutest(input, *root, std::move(*format));
    if (format->producer == kMcucov) return import_mcucov(input, *root, std::move(*format));
    if (format->producer == kMcucheck) return import_mcucheck(input, *root, std::move(*format));
    return std::unexpected(ImportError{
        .code = ImportErrorCode::unrecognized_format,
        .detail = "no importer registered for artifact",
        .source = SourceLocation{.path = input.path},
    });
}

}  // namespace mcutrace
