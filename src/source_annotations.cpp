#include <mcutrace/source_annotations.hpp>

#include "source_annotation_scanner.hpp"

#include <mcutrace/requirements.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mcutrace {
namespace {

using Scanner = detail::SourceScanResult (*)(const ArtifactInput&);

struct ScannerRegistration final {
    SourceLanguage language = SourceLanguage::unsupported;
    std::span<const std::string_view> extensions;
    Scanner scan = nullptr;
};

constexpr std::array<std::string_view, 8> kCppExtensions{
    ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx",
};
constexpr std::array<std::string_view, 3> kJavascriptExtensions{
    ".js", ".mjs", ".cjs",
};
const std::array<ScannerRegistration, 2> kScannerRegistry{
    ScannerRegistration{SourceLanguage::cpp, kCppExtensions, detail::scan_cpp_source},
    ScannerRegistration{
        SourceLanguage::javascript, kJavascriptExtensions, detail::scan_javascript_source},
};

std::string_view source_extension(std::string_view path) noexcept {
    const auto slash = path.find_last_of("/\\");
    const auto dot = path.find_last_of('.');
    if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash)) {
        return {};
    }
    return path.substr(dot);
}

const ScannerRegistration* scanner_registration(std::string_view path) noexcept {
    const auto extension = source_extension(path);
    for (const auto& registration : kScannerRegistry) {
        if (std::find(registration.extensions.begin(), registration.extensions.end(), extension) !=
            registration.extensions.end()) {
            return &registration;
        }
    }
    return nullptr;
}

std::string_view trim(std::string_view value) noexcept {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.remove_suffix(1);
    }
    return value;
}

bool marker_is_in_comment(std::string_view line, std::size_t marker) noexcept {
    const auto prefix = line.substr(0, marker);
    const auto line_comment = prefix.rfind("//");
    const auto block_comment = prefix.rfind("/*");
    return line_comment != std::string_view::npos || block_comment != std::string_view::npos ||
           trim(prefix).starts_with('*');
}

bool marker_boundary(std::string_view text, std::size_t position, std::string_view marker) noexcept {
    const auto end = position + marker.size();
    return (position == 0 || text[position - 1] != '@') &&
           (end == text.size() || std::isspace(static_cast<unsigned char>(text[end])) != 0);
}

std::string clean_token(std::string_view token) {
    while (!token.empty() && (token.back() == ',' || token.back() == ';' ||
                              token.back() == '*' || token.back() == '/')) {
        token.remove_suffix(1);
    }
    return std::string(token);
}

void append_invalid_requirement(ImportFragment& fragment,
                                const ArtifactInput& input,
                                std::uint32_t line,
                                const std::string& token) {
    fragment.diagnostics.push_back(Diagnostic{
        .code = "source.annotation.invalid_requirement",
        .severity = Severity::warning,
        .message = "ignored malformed source requirement reference: " + token,
        .source = SourceLocation{.path = input.path, .line = line},
    });
}

std::vector<std::string> parse_requirement_ids(std::string_view text,
                                               ImportFragment& fragment,
                                               const ArtifactInput& input,
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
            append_invalid_requirement(fragment, input, line, token);
        }
        if (end == std::string_view::npos) {
            return result;
        }
        text.remove_prefix(end + 1);
    }
}

std::string stable_hash(std::string_view value) {
    constexpr std::uint64_t kOffset = 14695981039346656037ULL;
    constexpr std::uint64_t kPrime = 1099511628211ULL;
    std::uint64_t hash = kOffset;
    for (const char ch : value) {
        hash ^= static_cast<unsigned char>(ch);
        hash *= kPrime;
    }

    constexpr std::string_view digits = "0123456789abcdef";
    std::array<char, 16> encoded{};
    for (std::size_t index = 0; index < encoded.size(); ++index) {
        const auto shift = static_cast<unsigned>((encoded.size() - index - 1U) * 4U);
        encoded[index] = digits[(hash >> shift) & 0x0fU];
    }
    return std::string(encoded.data(), encoded.size());
}

std::string implementation_id(std::string_view normalized_path,
                              const detail::AnnotationTarget& target) {
    std::string id = "implementation:" + std::string(normalized_path) + "#" + target.scope;
    if (!target.symbol.empty()) {
        id += ":" + target.symbol;
    }
    if (!target.identity.empty()) {
        id += "@" + stable_hash(target.identity);
    }
    return id;
}

void append_implementation_node(ImportFragment& fragment,
                                std::string id,
                                std::string_view normalized_path,
                                const detail::AnnotationTarget& target) {
    const auto existing = std::find_if(fragment.nodes.begin(), fragment.nodes.end(),
        [&id](const Node& node) { return node.id == id; });
    if (existing != fragment.nodes.end()) {
        return;
    }
    fragment.nodes.push_back(Node{
        .id = std::move(id),
        .kind = NodeKind::implementation,
        .label = target.symbol.empty()
            ? std::string(normalized_path)
            : target.scope + " " + target.symbol,
        .evidence_state = EvidenceState::unknown,
        .evidence_detail = {},
        .finding_state = {},
        .source = SourceLocation{.path = std::string(normalized_path), .line = target.line},
        .expected_evidence = std::nullopt,
        .body = {},
        .status = {},
    });
}

void append_edge(ImportFragment& fragment,
                 const ArtifactInput& input,
                 std::string implementation,
                 std::string requirement,
                 const detail::AnnotationTarget& target) {
    fragment.edges.push_back(Edge{
        .source_id = std::move(implementation),
        .target_id = std::move(requirement),
        .type = RelationshipType::known(RelationshipKind::implements),
        .provenance = Provenance{
            .importer = "source-annotations",
            .artifact = input.path,
            .source = SourceLocation{.path = input.path, .line = target.line},
            .scope = target.scope,
            .symbol = target.symbol,
        },
        .source = SourceLocation{.path = input.path, .line = target.line},
    });
}

void append_marker(ImportFragment& fragment,
                   const ArtifactInput& input,
                   std::string_view normalized_path,
                   const detail::ScannedMarker& marker) {
    auto requirements = parse_requirement_ids(marker.text, fragment, input, marker.line);
    if (requirements.empty()) {
        const auto name = marker.kind == detail::SourceMarkerKind::file ? "@req-file" : "@req";
        fragment.diagnostics.push_back(Diagnostic{
            .code = "source.annotation.missing_requirement",
            .severity = Severity::warning,
            .message = std::string(name) + " requires at least one canonical REQ-NNNN identifier",
            .source = SourceLocation{.path = input.path, .line = marker.line},
        });
    }
    if (!marker.target) {
        return;
    }

    const std::string id = implementation_id(normalized_path, *marker.target);
    append_implementation_node(fragment, id, normalized_path, *marker.target);
    for (auto& requirement : requirements) {
        append_edge(fragment, input, id, std::move(requirement), *marker.target);
    }
}

}  // namespace

namespace detail {

std::optional<ScannedMarker>
scan_line_marker(std::string_view line,
                 std::uint32_t line_number,
                 std::size_t line_offset,
                 std::string_view marker) {
    const auto position = line.find(marker);
    if (position == std::string_view::npos || !marker_is_in_comment(line, position) ||
        !marker_boundary(line, position, marker)) {
        return std::nullopt;
    }
    return ScannedMarker{
        .kind = marker == "@req-file" ? SourceMarkerKind::file : SourceMarkerKind::declaration,
        .text = std::string(line.substr(position + marker.size())),
        .line = line_number,
        .begin = line_offset + position,
        .end = line_offset + line.size(),
        .target = std::nullopt,
    };
}

void append_comment_markers(SourceScanResult& result,
                            std::string_view comment,
                            std::uint32_t first_line,
                            std::size_t comment_offset,
                            std::size_t comment_end) {
    std::size_t offset = 0;
    std::uint32_t line = first_line;
    while (offset <= comment.size()) {
        const auto line_end = comment.find('\n', offset);
        const auto text = comment.substr(
            offset, line_end == std::string_view::npos ? std::string_view::npos : line_end - offset);
        const auto file = text.find("@req-file");
        const auto local = text.find("@req");
        const bool valid_file = file != std::string_view::npos &&
                                marker_boundary(text, file, "@req-file");
        const bool valid_local = local != std::string_view::npos &&
                                 marker_boundary(text, local, "@req");
        if (valid_file || valid_local) {
            const bool use_file = valid_file && (!valid_local || file <= local);
            const auto position = use_file ? file : local;
            const auto marker = use_file ? std::string_view{"@req-file"} : std::string_view{"@req"};
            result.markers.push_back(ScannedMarker{
                .kind = use_file ? SourceMarkerKind::file : SourceMarkerKind::declaration,
                .text = std::string(text.substr(position + marker.size())),
                .line = line,
                .begin = comment_offset + offset + position,
                .end = comment_end,
                .target = std::nullopt,
            });
        }
        if (line_end == std::string_view::npos) {
            break;
        }
        offset = line_end + 1;
        ++line;
    }
}

// @req REQ-0193
SourceScanResult scan_generic_source(const ArtifactInput& input) {
    SourceScanResult result;
    std::uint32_t line_number = 0;
    std::size_t offset = 0;
    while (offset <= input.content.size()) {
        ++line_number;
        const auto end = input.content.find('\n', offset);
        const auto line = std::string_view(input.content).substr(
            offset, end == std::string::npos ? std::string::npos : end - offset);
        if (auto marker = scan_line_marker(line, line_number, offset, "@req-file")) {
            marker->target = AnnotationTarget{
                .line = marker->line, .scope = "file", .symbol = {}, .identity = {}};
            result.markers.push_back(std::move(*marker));
        } else if (auto marker = scan_line_marker(line, line_number, offset, "@req")) {
            result.diagnostics.push_back(Diagnostic{
                .code = "source.annotation.unsupported_language",
                .severity = Severity::warning,
                .message = "@req cannot be bound because the source language is unsupported",
                .source = SourceLocation{.path = input.path, .line = marker->line},
            });
            result.markers.push_back(std::move(*marker));
        }
        if (end == std::string::npos) {
            break;
        }
        offset = end + 1;
    }
    return result;
}

}  // namespace detail

// @req REQ-0193
SourceLanguage source_language_for_path(const std::string_view path) noexcept {
    const auto* registration = scanner_registration(path);
    return registration == nullptr ? SourceLanguage::unsupported : registration->language;
}

// @req REQ-0089 REQ-0090 REQ-0091 REQ-0092 REQ-0093 REQ-0095 REQ-0192 REQ-0193 REQ-0199
std::expected<ImportFragment, ImportError>
import_source_annotations(const ArtifactInput& input) {
    auto normalized = normalize_artifact_path(input.path, input.base_directory);
    if (!normalized) {
        return std::unexpected(normalized.error());
    }

    ImportFragment fragment{
        .format = InputFormat{.producer = "source", .schema = "source-annotations", .version = "1"},
        .nodes = {},
        .edges = {},
        .artifacts = {},
        .diagnostics = {},
    };
    fragment.nodes.push_back(Node{
        .id = "source:" + *normalized,
        .kind = NodeKind::source,
        .label = *normalized,
        .evidence_state = EvidenceState::unknown,
        .evidence_detail = {},
        .finding_state = {},
        .source = SourceLocation{.path = *normalized},
        .expected_evidence = std::nullopt,
        .body = {},
        .status = {},
    });

    const auto* registration = scanner_registration(*normalized);
    detail::SourceScanResult scan = registration == nullptr
        ? detail::scan_generic_source(input)
        : registration->scan(input);
    fragment.diagnostics = std::move(scan.diagnostics);
    for (const auto& marker : scan.markers) {
        append_marker(fragment, input, *normalized, marker);
    }
    return fragment;
}

}  // namespace mcutrace
