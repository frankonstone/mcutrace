#include <mcutrace/source_annotations.hpp>

#include <mcutrace/requirements.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mcutrace {
namespace {

struct PendingAnnotation final {
    std::vector<std::string> requirements;
    std::uint32_t line = 0;
};

struct DeclarationContext final {
    std::string kind;
    std::string symbol;
};

struct AnnotationTarget final {
    std::uint32_t line = 0;
    std::string scope;
    std::string symbol;
    std::string identity;
};

struct ScannerState final {
    std::optional<PendingAnnotation> pending;
    std::string declaration;
};

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

std::optional<PendingAnnotation> parse_marker(std::string_view line,
                                              std::string_view marker,
                                              ImportFragment& fragment,
                                              const ArtifactInput& input,
                                              std::uint32_t line_number) {
    const auto position = line.find(marker);
    if (position == std::string_view::npos || !marker_is_in_comment(line, position)) {
        return std::nullopt;
    }
    auto requirements = parse_requirement_ids(line.substr(position + marker.size()), fragment,
                                               input, line_number);
    if (requirements.empty()) {
        fragment.diagnostics.push_back(Diagnostic{
            .code = "source.annotation.missing_requirement",
            .severity = Severity::warning,
            .message = std::string(marker) + " requires at least one canonical REQ-NNNN identifier",
            .source = SourceLocation{.path = input.path, .line = line_number},
        });
    }
    return PendingAnnotation{.requirements = std::move(requirements), .line = line_number};
}

std::string_view identifier_before(std::string_view value, std::size_t before) noexcept {
    while (before != 0 && std::isspace(static_cast<unsigned char>(value[before - 1])) != 0) {
        --before;
    }
    const auto end = before;
    while (before != 0) {
        const char ch = value[before - 1];
        if (std::isalnum(static_cast<unsigned char>(ch)) == 0 && ch != '_' && ch != '~' && ch != ':') {
            break;
        }
        --before;
    }
    return value.substr(before, end - before);
}

std::optional<DeclarationContext> named_type_context(std::string_view declaration,
                                                     std::string_view keyword,
                                                     std::string_view kind) {
    if (!declaration.starts_with(keyword)) {
        return std::nullopt;
    }
    auto rest = trim(declaration.substr(keyword.size()));
    const auto end = rest.find_first_of(" :{;\t\r\n");
    const auto name = end == std::string_view::npos ? rest : rest.substr(0, end);
    if (name.empty()) {
        return std::nullopt;
    }
    return DeclarationContext{.kind = std::string(kind), .symbol = std::string(name)};
}

std::optional<DeclarationContext> enum_context(std::string_view declaration) {
    if (!declaration.starts_with("enum ")) {
        return std::nullopt;
    }
    auto rest = trim(declaration.substr(5));
    if (rest.starts_with("class ")) {
        rest = trim(rest.substr(6));
    } else if (rest.starts_with("struct ")) {
        rest = trim(rest.substr(7));
    }
    const auto end = rest.find_first_of(" :{;\t\r\n");
    const auto name = end == std::string_view::npos ? rest : rest.substr(0, end);
    if (name.empty()) {
        return std::nullopt;
    }
    return DeclarationContext{.kind = "enum", .symbol = std::string(name)};
}

bool is_control_keyword(std::string_view symbol) noexcept {
    return symbol == "if" || symbol == "for" || symbol == "while" || symbol == "switch" ||
           symbol == "catch" || symbol == "return" || symbol == "sizeof" || symbol == "alignof";
}

std::optional<DeclarationContext> callable_context(std::string_view declaration) {
    const auto open = declaration.find('(');
    if (open == std::string_view::npos) {
        return std::nullopt;
    }
    const auto symbol = identifier_before(declaration, open);
    if (symbol.empty() || is_control_keyword(symbol)) {
        return std::nullopt;
    }
    return DeclarationContext{
        .kind = symbol.find("::") == std::string_view::npos ? "function" : "method",
        .symbol = std::string(symbol),
    };
}

std::optional<DeclarationContext> declaration_context(std::string_view declaration) {
    declaration = trim(declaration);
    if (auto context = named_type_context(declaration, "class ", "class")) {
        return context;
    }
    if (auto context = named_type_context(declaration, "struct ", "struct")) {
        return context;
    }
    if (auto context = enum_context(declaration)) {
        return context;
    }
    return callable_context(declaration);
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
                              const AnnotationTarget& target) {
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
                                const AnnotationTarget& target) {
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
        .finding_state = {},
        .source = SourceLocation{.path = std::string(normalized_path), .line = target.line},
        .expected_evidence = std::nullopt,
    });
}

void append_edge(ImportFragment& fragment,
                 const ArtifactInput& input,
                 std::string implementation_id,
                 std::string requirement,
                 const AnnotationTarget& target) {
    fragment.edges.push_back(Edge{
        .source_id = std::move(implementation_id),
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

void append_annotation(ImportFragment& fragment,
                       const ArtifactInput& input,
                       std::string_view normalized_path,
                       const PendingAnnotation& annotation,
                       const AnnotationTarget& target) {
    const std::string id = implementation_id(normalized_path, target);
    append_implementation_node(fragment, id, normalized_path, target);
    for (const auto& requirement : annotation.requirements) {
        append_edge(fragment, input, id, requirement, target);
    }
}

bool ignorable_between_annotation_and_declaration(std::string_view line) noexcept {
    line = trim(line);
    return line.empty() || line.starts_with("//") || line.starts_with("/*") ||
           line.starts_with('*') || line.starts_with("*/") || line.starts_with("template") ||
           line.starts_with("[[");
}

bool declaration_complete(std::string_view declaration) noexcept {
    return declaration.find('{') != std::string_view::npos ||
           declaration.find(';') != std::string_view::npos;
}

void append_declaration_piece(ScannerState& state, std::string_view line) {
    const auto piece = trim(line);
    if (piece.empty() || piece.starts_with("//") || piece.starts_with("/*") ||
        piece.starts_with('*') || piece.starts_with("*/")) {
        return;
    }
    state.declaration += std::string(piece);
    state.declaration += ' ';
}

bool handle_marker_line(std::string_view line,
                        std::uint32_t line_number,
                        ImportFragment& fragment,
                        const ArtifactInput& input,
                        std::string_view normalized_path,
                        ScannerState& state) {
    if (const auto file = parse_marker(line, "@req-file", fragment, input, line_number)) {
        append_annotation(fragment, input, normalized_path, *file,
                          AnnotationTarget{.line = file->line, .scope = "file"});
        return true;
    }
    const auto local = parse_marker(line, "@req", fragment, input, line_number);
    if (!local) {
        return false;
    }
    if (state.pending && !state.pending->requirements.empty()) {
        state.pending->requirements.insert(state.pending->requirements.end(),
                                           local->requirements.begin(), local->requirements.end());
    } else {
        state.pending = *local;
    }
    return true;
}

void complete_pending_declaration(ImportFragment& fragment,
                                  const ArtifactInput& input,
                                  std::string_view normalized_path,
                                  ScannerState& state) {
    const auto context = declaration_context(state.declaration);
    if (context) {
        append_annotation(fragment, input, normalized_path, *state.pending,
                          AnnotationTarget{.line = state.pending->line,
                                           .scope = context->kind,
                                           .symbol = context->symbol,
                                           .identity = state.declaration});
    } else {
        fragment.diagnostics.push_back(Diagnostic{
            .code = "source.annotation.unsupported_target",
            .severity = Severity::warning,
            .message = "@req must immediately precede a supported class, struct, enum, function, or method declaration",
            .source = SourceLocation{.path = input.path, .line = state.pending->line},
        });
    }
    state.pending.reset();
    state.declaration.clear();
}

void handle_pending_line(std::string_view line,
                         ImportFragment& fragment,
                         const ArtifactInput& input,
                         std::string_view normalized_path,
                         ScannerState& state) {
    if (!state.pending) {
        return;
    }
    if (!ignorable_between_annotation_and_declaration(line) || !trim(line).empty()) {
        append_declaration_piece(state, line);
    }
    if (!state.declaration.empty() && declaration_complete(state.declaration)) {
        complete_pending_declaration(fragment, input, normalized_path, state);
    }
}

}  // namespace

// @req REQ-0089 REQ-0090 REQ-0091 REQ-0092 REQ-0093 REQ-0095
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
    const std::string source_id = "source:" + *normalized;
    fragment.nodes.push_back(Node{
        .id = source_id,
        .kind = NodeKind::source,
        .label = *normalized,
        .evidence_state = EvidenceState::unknown,
        .finding_state = {},
        .source = SourceLocation{.path = *normalized},
        .expected_evidence = std::nullopt,
    });

    ScannerState state;
    std::uint32_t line_number = 0;
    std::size_t offset = 0;
    while (offset <= input.content.size()) {
        ++line_number;
        const auto end = input.content.find('\n', offset);
        const auto line = std::string_view(input.content).substr(
            offset, end == std::string::npos ? std::string::npos : end - offset);

        if (!handle_marker_line(line, line_number, fragment, input, *normalized, state)) {
            handle_pending_line(line, fragment, input, *normalized, state);
        }

        if (end == std::string::npos) {
            break;
        }
        offset = end + 1;
    }

    if (state.pending) {
        fragment.diagnostics.push_back(Diagnostic{
            .code = "source.annotation.missing_target",
            .severity = Severity::warning,
            .message = "@req has no following supported declaration",
            .source = SourceLocation{.path = input.path, .line = state.pending->line},
        });
    }
    return fragment;
}

}  // namespace mcutrace
