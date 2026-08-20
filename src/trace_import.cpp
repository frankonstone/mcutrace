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

void append_link(ImportFragment& fragment, std::string source_id, std::string target_id,
                 RelationshipType type, const ArtifactInput& input) {
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
                .source = SourceLocation{.path = input.path}
            });
            continue;
        }
        append_link(fragment, std::string(evidence_id), value.get<std::string>(),
                    RelationshipType::known(kind), input);
    }
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

// @req REQ-0002 REQ-0003 REQ-0033 REQ-0040 REQ-0079 REQ-0080 REQ-0082
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
