#include <mcutrace/trace_import.hpp>

#include <mcutrace/producer_importers.hpp>
#include <mcutrace/requirements.hpp>

#include <cstddef>
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

RelationshipType relationship_type(std::string_view name) {
    if (name == "satisfies") return RelationshipType::known(RelationshipKind::satisfies);
    if (name == "verifies") return RelationshipType::known(RelationshipKind::verifies);
    if (name == "implements") return RelationshipType::known(RelationshipKind::implements);
    if (name == "covers") return RelationshipType::known(RelationshipKind::covers);
    if (name == "reports") return RelationshipType::known(RelationshipKind::reports);
    if (name == "relates") return RelationshipType::known(RelationshipKind::relates);
    return RelationshipType::custom(std::string(name));
}

void append_link(ImportFragment& fragment,
                 std::string source_id,
                 std::string target_id,
                 RelationshipType type,
                 const ArtifactInput& input) {
    fragment.edges.push_back(Edge{
        .source_id = std::move(source_id),
        .target_id = std::move(target_id),
        .type = std::move(type),
        .provenance = Provenance{
            .importer = fragment.format.schema,
            .artifact = input.path,
            .source = SourceLocation{.path = input.path},
        },
        .source = SourceLocation{.path = input.path},
    });
}

void append_requirement_links(ImportFragment& fragment,
                              const mcujson::JsonRef& requirements,
                              std::string_view evidence_id,
                              RelationshipKind kind,
                              const ArtifactInput& input) {
    if (!requirements.valid()) return;
    if (!requirements.is_array()) {
        fragment.diagnostics.push_back(Diagnostic{
            .code = "import.requirements.not_array",
            .severity = Severity::warning,
            .message = "requirements must be an array of REQ-NNNN identifiers",
            .source = SourceLocation{.path = input.path},
        });
        return;
    }
    for (const auto value : requirements) {
        if (!value.is_string() || !is_requirement_id(value.get<std::string_view>())) {
            fragment.diagnostics.push_back(Diagnostic{
                .code = "import.requirements.invalid_id",
                .severity = Severity::warning,
                .message = "ignored invalid requirement reference",
                .source = SourceLocation{.path = input.path},
            });
            continue;
        }
        append_link(fragment,
                    std::string(evidence_id),
                    value.get<std::string>(),
                    RelationshipType::known(kind),
                    input);
    }
}

std::expected<ImportFragment, ImportError>
import_link_sidecar(const ArtifactInput& input,
                    const mcujson::Json& root,
                    std::string_view requested_importer) {
    if (!requested_importer.empty() && requested_importer != "mcutrace-links" &&
        requested_importer != "mcutrace") {
        return std::unexpected(ImportError{
            .code = ImportErrorCode::unrecognized_format,
            .detail = "artifact identifies as 'mcutrace-links' but importer '" +
                      std::string(requested_importer) + "' was requested",
            .source = SourceLocation{.path = input.path},
        });
    }
    if (!root["version"].is_number()) {
        return std::unexpected(ImportError{
            .code = ImportErrorCode::invalid_artifact,
            .detail = "mcutrace-links requires a numeric version",
            .source = SourceLocation{.path = input.path},
        });
    }
    const auto version = root["version"].get<long long>();
    if (version != 1) {
        return std::unexpected(ImportError{
            .code = ImportErrorCode::unsupported_version,
            .detail = "unsupported mcutrace-links version: " + std::to_string(version),
            .source = SourceLocation{.path = input.path},
        });
    }
    const auto links = root["links"];
    if (!links.is_array()) {
        return std::unexpected(ImportError{
            .code = ImportErrorCode::invalid_artifact,
            .detail = "mcutrace-links requires a links array",
            .source = SourceLocation{.path = input.path},
        });
    }

    ImportFragment fragment{
        .format = InputFormat{
            .producer = "mcutrace",
            .schema = "mcutrace-links",
            .version = "1",
        },
    };
    fragment.artifacts.push_back(preserve_json_artifact(
        "artifact:mcutrace-links:" + input.path,
        "mcutrace-links",
        input.content,
        SourceLocation{.path = input.path}));

    for (const auto link : links) {
        if (!link.is_object() || !link["source"].is_string() ||
            !link["target"].is_string() || !link["type"].is_string()) {
            fragment.diagnostics.push_back(Diagnostic{
                .code = "import.links.invalid_entry",
                .severity = Severity::warning,
                .message = "ignored malformed mcutrace-links entry",
                .source = SourceLocation{.path = input.path},
            });
            continue;
        }
        append_link(fragment,
                    link["source"].get<std::string>(),
                    link["target"].get<std::string>(),
                    relationship_type(link["type"].get<std::string_view>()),
                    input);
    }
    return fragment;
}

void augment_mcutest(ImportFragment& fragment,
                     const mcujson::Json& root,
                     const ArtifactInput& input) {
    const auto tests = root["tests"];
    if (!tests.is_array()) return;
    for (const auto test : tests) {
        if (!test.is_object() || !test["name"].is_string()) continue;
        const std::string evidence_id = "test:mcutest:" + test["name"].get<std::string>();
        append_requirement_links(fragment, test["requirements"], evidence_id,
                                 RelationshipKind::verifies, input);
    }
}

void augment_mcucov(ImportFragment& fragment,
                    const mcujson::Json& root,
                    const ArtifactInput& input) {
    const auto modules = root["modules"];
    if (!modules.is_array()) return;
    for (const auto module : modules) {
        if (!module.is_object() || !module["path"].is_string()) continue;
        auto normalized = normalize_artifact_path(module["path"].get<std::string_view>(),
                                                  input.base_directory);
        if (!normalized) continue;
        const std::string variant = module["variant"].is_string()
            ? module["variant"].get<std::string>() : std::string{};
        const std::string coverage_id = "coverage:mcucov:" + *normalized + ":" + variant;
        const std::string source_id = "source:" + *normalized;
        append_requirement_links(fragment, module["requirements"], coverage_id,
                                 RelationshipKind::covers, input);
        append_requirement_links(fragment, module["requirements"], source_id,
                                 RelationshipKind::implements, input);
    }
}

void augment_mcucheck(ImportFragment& fragment,
                      const mcujson::Json& root,
                      const ArtifactInput& input) {
    const auto diagnostics = root["diagnostics"];
    if (!diagnostics.is_array()) return;
    std::size_t fallback_index = 0;
    for (const auto diagnostic : diagnostics) {
        ++fallback_index;
        if (!diagnostic.is_object() || !diagnostic["rule_id"].is_string()) continue;
        const auto location = diagnostic["location"];
        if (!location.is_object() || !location["path"].is_string()) continue;
        auto normalized = normalize_artifact_path(location["path"].get<std::string_view>(),
                                                  input.base_directory);
        if (!normalized) continue;
        const auto line = location["line"].is_number()
            ? static_cast<std::uint32_t>(location["line"].get<long long>()) : 0U;
        const std::string stable = diagnostic["id"].is_string()
            ? diagnostic["id"].get<std::string>()
            : diagnostic["rule_id"].get<std::string>() + ":" + *normalized + ":" +
              std::to_string(line) + ":" + std::to_string(fallback_index);
        const std::string finding_id = "finding:mcucheck:" + stable;
        append_requirement_links(fragment, diagnostic["requirements"], finding_id,
                                 RelationshipKind::reports, input);
    }
}

}  // namespace

std::expected<ImportFragment, ImportError>
import_trace_artifact(const ArtifactInput& input, std::string_view requested_importer) {
    auto parsed = parse_json(input);
    if (!parsed || !parsed->is_object() || !(*parsed)["format"].is_string()) {
        if (!parsed) return std::unexpected(parsed.error());
        return std::unexpected(ImportError{
            .code = ImportErrorCode::unrecognized_format,
            .detail = "artifact has no recognized format header: " + input.path,
            .source = SourceLocation{.path = input.path},
        });
    }

    const std::string format = (*parsed)["format"].get<std::string>();
    if (format == "mcutrace-links") {
        return import_link_sidecar(input, *parsed, requested_importer);
    }

    auto fragment = import_producer_artifact(input, requested_importer);
    if (!fragment) return std::unexpected(fragment.error());

    if (format == "mcutest-results") augment_mcutest(*fragment, *parsed, input);
    else if (format == "mcucov-report") augment_mcucov(*fragment, *parsed, input);
    else if (format == "mcucheck-results") augment_mcucheck(*fragment, *parsed, input);
    return fragment;
}

}  // namespace mcutrace
