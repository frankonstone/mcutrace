#include <mcutrace/build_annotations.hpp>

#include <mcutrace/requirements.hpp>

#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mcutrace {
namespace {

std::string_view trim(std::string_view value) noexcept {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.remove_suffix(1);
    }
    return value;
}

std::string clean_token(std::string_view token) {
    while (!token.empty() && (token.back() == ',' || token.back() == ';' ||
                              token.back() == '#')) {
        token.remove_suffix(1);
    }
    return std::string(token);
}

bool marker_is_in_comment(std::string_view line, std::size_t marker) noexcept {
    return line.substr(0, marker).find('#') != std::string_view::npos;
}

void append_invalid_requirement(ImportFragment& fragment,
                                std::string_view path,
                                std::uint32_t line,
                                const std::string& token) {
    fragment.diagnostics.push_back(Diagnostic{
        .code = "build.annotation.invalid_requirement",
        .severity = Severity::warning,
        .message = "ignored malformed build requirement reference: " + token,
        .source = SourceLocation{.path = std::string(path), .line = line},
    });
}

std::vector<std::string> parse_requirement_ids(std::string_view text,
                                               ImportFragment& fragment,
                                               std::string_view path,
                                               std::uint32_t line) {
    std::vector<std::string> result;
    while (true) {
        text = trim(text);
        if (text.empty()) {
            return result;
        }
        const auto end = text.find_first_of(" \t\r\n");
        const auto raw = end == std::string_view::npos ? text : text.substr(0, end);
        const std::string token = clean_token(raw);
        if (!token.starts_with("REQ-")) {
            return result;
        }
        if (is_requirement_id(token)) {
            result.push_back(token);
        } else {
            append_invalid_requirement(fragment, path, line, token);
        }
        if (end == std::string_view::npos) {
            return result;
        }
        text.remove_prefix(end + 1U);
    }
}

void append_artifact_node(ImportFragment& fragment, std::string_view path, std::uint32_t line) {
    fragment.nodes.push_back(Node{
        .id = "artifact:cmake:" + std::string(path),
        .kind = NodeKind::artifact,
        .label = "CMake build definition: " + std::string(path),
        .evidence_state = EvidenceState::unknown,
        .evidence_detail = {},
        .finding_state = {},
        .source = SourceLocation{.path = std::string(path), .line = line},
        .expected_evidence = std::nullopt,
    });
}

void append_evidence(ImportFragment& fragment,
                     std::string_view path,
                     std::uint32_t line,
                     const std::vector<std::string>& requirements) {
    for (const auto& requirement : requirements) {
        fragment.edges.push_back(Edge{
            .source_id = "artifact:cmake:" + std::string(path),
            .target_id = requirement,
            .type = RelationshipType::known(RelationshipKind::verifies),
            .provenance = Provenance{
                .importer = "cmake-annotations",
                .artifact = std::string(path),
                .source = SourceLocation{.path = std::string(path), .line = line},
            },
            .source = SourceLocation{.path = std::string(path), .line = line},
        });
    }
}

}  // namespace

// @req REQ-0081
std::expected<ImportFragment, ImportError>
import_build_annotations(const ArtifactInput& input) {
    auto normalized = normalize_artifact_path(input.path, input.base_directory);
    if (!normalized) {
        return std::unexpected(normalized.error());
    }

    ImportFragment fragment{
        .format = InputFormat{.producer = "cmake", .schema = "cmake-annotations", .version = "1"},
        .nodes = {},
        .edges = {},
        .artifacts = {},
        .diagnostics = {},
    };
    bool has_evidence = false;
    std::uint32_t line_number = 0;
    std::size_t offset = 0;
    while (offset <= input.content.size()) {
        ++line_number;
        const auto end = input.content.find('\n', offset);
        const auto line = std::string_view(input.content).substr(
            offset, end == std::string::npos ? std::string::npos : end - offset);
        constexpr std::string_view marker = "@req";
        const auto position = line.find(marker);
        if (position != std::string_view::npos && marker_is_in_comment(line, position)) {
            const auto requirements = parse_requirement_ids(
                line.substr(position + marker.size()), fragment, *normalized, line_number);
            if (requirements.empty()) {
                fragment.diagnostics.push_back(Diagnostic{
                    .code = "build.annotation.missing_requirement",
                    .severity = Severity::warning,
                    .message = "@req requires at least one canonical REQ-NNNN identifier",
                    .source = SourceLocation{.path = *normalized, .line = line_number},
                });
            } else {
                if (!has_evidence) {
                    append_artifact_node(fragment, *normalized, line_number);
                    has_evidence = true;
                }
                append_evidence(fragment, *normalized, line_number, requirements);
            }
        }
        if (end == std::string::npos) {
            break;
        }
        offset = end + 1U;
    }
    return fragment;
}

}  // namespace mcutrace
