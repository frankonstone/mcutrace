// @req-file REQ-0002 REQ-0003 REQ-0033 REQ-0040 REQ-0079 REQ-0080 REQ-0081 REQ-0082 REQ-0083 REQ-0087
#include <mcutrace/trace_import.hpp>

#include <mcutrace/producer_importers.hpp>
#include <mcutrace/requirements.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
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

std::optional<NodeKind> node_kind(std::string_view name) {
    if (name == "source") return NodeKind::source;
    if (name == "test") return NodeKind::test;
    if (name == "coverage") return NodeKind::coverage;
    if (name == "finding") return NodeKind::finding;
    if (name == "artifact") return NodeKind::artifact;
    return std::nullopt;
}

std::string canonical_node_id(std::string id, const ArtifactInput& input) {
    constexpr std::string_view prefix = "source:";
    if (!std::string_view(id).starts_with(prefix) || id.size() == prefix.size()) {
        return id;
    }
    auto normalized = normalize_artifact_path(
        std::string_view(id).substr(prefix.size()), input.base_directory);
    return normalized ? std::string(prefix) + *normalized : id;
}

void append_link(ImportFragment& fragment, std::string source_id, std::string target_id,
                 RelationshipType type, const ArtifactInput& input) {
    source_id = canonical_node_id(std::move(source_id), input);
    target_id = canonical_node_id(std::move(target_id), input);
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

void append_requirement_links(ImportFragment& fragment, const mcujson::JsonRef& requirements,
                              std::string_view evidence_id, RelationshipKind kind,
                              const ArtifactInput& input) {
    if (!requirements.valid()) {
        return;
    }
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
        append_link(fragment, std::string(evidence_id), value.get<std::string>(),
                    RelationshipType::known(kind), input);
    }
}

std::expected<void, ImportError> validate_sidecar_header(
    const ArtifactInput& input,
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
    if (!root["links"].is_array()) {
        return std::unexpected(ImportError{
            .code = ImportErrorCode::invalid_artifact,
            .detail = "mcutrace-links requires a links array",
            .source = SourceLocation{.path = input.path},
        });
    }
    return {};
}

ImportFragment make_sidecar_fragment(const ArtifactInput& input) {
    ImportFragment fragment{
        .format = InputFormat{.producer = "mcutrace", .schema = "mcutrace-links", .version = "1"},
        .nodes = {},
        .edges = {},
        .artifacts = {},
        .diagnostics = {},
    };
    fragment.artifacts.push_back(preserve_json_artifact(
        "artifact:mcutrace-links:" + input.path, "mcutrace-links", input.content,
        SourceLocation{.path = input.path}));
    return fragment;
}

void append_sidecar_node(ImportFragment& fragment,
                         const mcujson::JsonRef& value,
                         const ArtifactInput& input) {
    if (!value.is_object() || !value["id"].is_string() || !value["kind"].is_string()) {
        fragment.diagnostics.push_back(Diagnostic{
            .code = "import.links.invalid_node",
            .severity = Severity::warning,
            .message = "ignored malformed mcutrace-links node",
            .source = SourceLocation{.path = input.path},
        });
        return;
    }
    const auto kind = node_kind(value["kind"].get<std::string_view>());
    if (!kind) {
        fragment.diagnostics.push_back(Diagnostic{
            .code = "import.links.invalid_node_kind",
            .severity = Severity::warning,
            .message = "ignored mcutrace-links node with unsupported kind",
            .source = SourceLocation{.path = input.path},
        });
        return;
    }

    const std::string original_id = value["id"].get<std::string>();
    const std::string id = canonical_node_id(original_id, input);
    std::optional<SourceLocation> source = SourceLocation{.path = input.path};
    if (*kind == NodeKind::source && std::string_view(id).starts_with("source:")) {
        source = SourceLocation{.path = id.substr(std::string_view("source:").size())};
    }
    fragment.nodes.push_back(Node{
        .id = id,
        .kind = *kind,
        .label = value["label"].is_string() ? value["label"].get<std::string>() : original_id,
        .evidence_state = EvidenceState::unknown,
        .finding_state = {},
        .source = std::move(source),
        .expected_evidence = std::nullopt,
    });
}

void append_sidecar_nodes(ImportFragment& fragment,
                          const mcujson::JsonRef& nodes,
                          const ArtifactInput& input) {
    if (!nodes.valid()) {
        return;
    }
    if (!nodes.is_array()) {
        fragment.diagnostics.push_back(Diagnostic{
            .code = "import.links.nodes_not_array",
            .severity = Severity::warning,
            .message = "mcutrace-links nodes must be an array",
            .source = SourceLocation{.path = input.path},
        });
        return;
    }
    for (const auto value : nodes) {
        append_sidecar_node(fragment, value, input);
    }
}

void append_sidecar_link(ImportFragment& fragment,
                         const mcujson::JsonRef& link,
                         const ArtifactInput& input) {
    if (!link.is_object() || !link["source"].is_string() ||
        !link["target"].is_string() || !link["type"].is_string()) {
        fragment.diagnostics.push_back(Diagnostic{
            .code = "import.links.invalid_entry",
            .severity = Severity::warning,
            .message = "ignored malformed mcutrace-links entry",
            .source = SourceLocation{.path = input.path},
        });
        return;
    }
    append_link(fragment, link["source"].get<std::string>(), link["target"].get<std::string>(),
                relationship_type(link["type"].get<std::string_view>()), input);
}

std::expected<ImportFragment, ImportError> import_link_sidecar(
    const ArtifactInput& input,
    const mcujson::Json& root,
    std::string_view requested_importer) {
    if (auto status = validate_sidecar_header(input, root, requested_importer); !status) {
        return std::unexpected(status.error());
    }
    auto fragment = make_sidecar_fragment(input);
    append_sidecar_nodes(fragment, root["nodes"], input);
    for (const auto link : root["links"]) {
        append_sidecar_link(fragment, link, input);
    }
    return fragment;
}

void augment_mcutest(ImportFragment& fragment, const mcujson::Json& root,
                     const ArtifactInput& input) {
    const auto tests = root["tests"];
    if (!tests.is_array()) {
        return;
    }
    for (const auto test : tests) {
        if (!test.is_object() || !test["name"].is_string()) {
            continue;
        }
        const std::string evidence_id = "test:mcutest:" + test["name"].get<std::string>();
        append_requirement_links(fragment, test["requirements"], evidence_id,
                                 RelationshipKind::verifies, input);
    }
}

void set_finding_state(ImportFragment& fragment,
                       std::string_view finding_id,
                       const mcujson::JsonRef& state) {
    if (!state.is_string()) {
        return;
    }
    const auto node = std::find_if(fragment.nodes.begin(), fragment.nodes.end(),
        [finding_id](const Node& candidate) { return candidate.id == finding_id; });
    if (node != fragment.nodes.end()) {
        node->finding_state = state.get<std::string>();
    }
}

std::optional<std::string> mcucheck_finding_id(const mcujson::JsonRef& diagnostic,
                                               const ArtifactInput& input,
                                               std::size_t fallback_index) {
    if (!diagnostic.is_object() || !diagnostic["rule_id"].is_string()) {
        return std::nullopt;
    }
    const auto location = diagnostic["location"];
    if (!location.is_object() || !location["path"].is_string()) {
        return std::nullopt;
    }
    auto normalized = normalize_artifact_path(
        location["path"].get<std::string_view>(), input.base_directory);
    if (!normalized) {
        return std::nullopt;
    }
    const auto line = location["line"].is_number()
        ? static_cast<std::uint32_t>(location["line"].get<long long>()) : 0U;
    const std::string stable = diagnostic["id"].is_string()
        ? diagnostic["id"].get<std::string>()
        : diagnostic["rule_id"].get<std::string>() + ":" + *normalized + ":" +
          std::to_string(line) + ":" + std::to_string(fallback_index);
    return "finding:mcucheck:" + stable;
}

void augment_mcucheck(ImportFragment& fragment, const mcujson::Json& root,
                      const ArtifactInput& input) {
    const auto diagnostics = root["diagnostics"];
    if (!diagnostics.is_array()) {
        return;
    }
    std::size_t fallback_index = 0;
    for (const auto diagnostic : diagnostics) {
        ++fallback_index;
        const auto finding_id = mcucheck_finding_id(diagnostic, input, fallback_index);
        if (!finding_id) {
            continue;
        }
        set_finding_state(fragment, *finding_id, diagnostic["state"]);
        append_requirement_links(fragment, diagnostic["requirements"], *finding_id,
                                 RelationshipKind::reports, input);
    }
}

}  // namespace

std::expected<ImportFragment, ImportError> import_trace_artifact(
    const ArtifactInput& input,
    std::string_view requested_importer) {
    const auto format_value = mcujson::json_get<std::string_view>(input.content, "format");
    if (format_value && *format_value == "mcucov-report") {
        return import_producer_artifact(input, requested_importer);
    }

    auto parsed = parse_json(input);
    if (!parsed) {
        return std::unexpected(parsed.error());
    }
    if (!parsed->is_object() || !(*parsed)["format"].is_string()) {
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
    if (!fragment) {
        return std::unexpected(fragment.error());
    }
    if (format == "mcutest-results") {
        augment_mcutest(*fragment, *parsed, input);
    } else if (format == "mcucheck-results") {
        augment_mcucheck(*fragment, *parsed, input);
    }
    return fragment;
}

}  // namespace mcutrace