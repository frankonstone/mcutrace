#include <mcutrace/source_annotations.hpp>

#include <mcutrace/requirements.hpp>

#include <algorithm>
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
    if (line_comment != std::string_view::npos || block_comment != std::string_view::npos) {
        return true;
    }
    return trim(prefix).starts_with('*');
}

std::string clean_token(std::string_view token) {
    while (!token.empty() && (token.back() == ',' || token.back() == ';' ||
                              token.back() == '*' || token.back() == '/')) {
        token.remove_suffix(1);
    }
    return std::string(token);
}

std::vector<std::string> parse_requirement_ids(std::string_view text,
                                               ImportFragment& fragment,
                                               const ArtifactInput& input,
                                               std::uint32_t line) {
    std::vector<std::string> result;
    while (!text.empty()) {
        text = trim(text);
        if (text.empty()) {
            break;
        }
        const auto end = text.find_first_of(" \t\r\n");
        const auto raw = end == std::string_view::npos ? text : text.substr(0, end);
        const std::string token = clean_token(raw);
        if (token.starts_with("REQ-")) {
            if (is_requirement_id(token)) {
                result.push_back(token);
            } else {
                fragment.diagnostics.push_back(Diagnostic{
                    .code = "source.annotation.invalid_requirement",
                    .severity = Severity::warning,
                    .message = "ignored malformed source requirement reference: " + token,
                    .source = SourceLocation{.path = input.path, .line = line},
                });
            }
        } else {
            break;
        }
        if (end == std::string_view::npos) {
            break;
        }
        text.remove_prefix(end + 1);
    }
    return result;
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

std::optional<DeclarationContext> declaration_context(std::string_view declaration) {
    declaration = trim(declaration);
    for (const auto keyword : {std::string_view("class "), std::string_view("struct ")}) {
        if (declaration.starts_with(keyword)) {
            auto rest = trim(declaration.substr(keyword.size()));
            const auto end = rest.find_first_of(" :{;\t\r\n");
            const auto name = end == std::string_view::npos ? rest : rest.substr(0, end);
            if (!name.empty()) {
                return DeclarationContext{
                    .kind = keyword.starts_with("class") ? "class" : "struct",
                    .symbol = std::string(name),
                };
            }
        }
    }
    if (declaration.starts_with("enum ")) {
        auto rest = trim(declaration.substr(5));
        if (rest.starts_with("class ")) {
            rest = trim(rest.substr(6));
        } else if (rest.starts_with("struct ")) {
            rest = trim(rest.substr(7));
        }
        const auto end = rest.find_first_of(" :{;\t\r\n");
        const auto name = end == std::string_view::npos ? rest : rest.substr(0, end);
        if (!name.empty()) {
            return DeclarationContext{.kind = "enum", .symbol = std::string(name)};
        }
    }

    const auto open = declaration.find('(');
    if (open == std::string_view::npos) {
        return std::nullopt;
    }
    const auto symbol = identifier_before(declaration, open);
    if (symbol.empty()) {
        return std::nullopt;
    }
    if (symbol == "if" || symbol == "for" || symbol == "while" || symbol == "switch" ||
        symbol == "catch" || symbol == "return" || symbol == "sizeof" || symbol == "alignof") {
        return std::nullopt;
    }
    return DeclarationContext{
        .kind = symbol.find("::") == std::string_view::npos ? "function" : "method",
        .symbol = std::string(symbol),
    };
}

void append_edge(ImportFragment& fragment,
                 const ArtifactInput& input,
                 std::string_view source_id,
                 std::string requirement,
                 std::uint32_t line,
                 std::string scope,
                 std::string symbol = {}) {
    fragment.edges.push_back(Edge{
        .source_id = std::string(source_id),
        .target_id = std::move(requirement),
        .type = RelationshipType::known(RelationshipKind::implements),
        .provenance = Provenance{
            .importer = "source-annotations",
            .artifact = input.path,
            .source = SourceLocation{.path = input.path, .line = line},
            .scope = std::move(scope),
            .symbol = std::move(symbol),
        },
        .source = SourceLocation{.path = input.path, .line = line},
    });
}

void append_file_annotation(ImportFragment& fragment,
                            const ArtifactInput& input,
                            std::string_view source_id,
                            const PendingAnnotation& annotation) {
    for (const auto& requirement : annotation.requirements) {
        append_edge(fragment, input, source_id, requirement, annotation.line, "file");
    }
}

void append_declaration_annotation(ImportFragment& fragment,
                                   const ArtifactInput& input,
                                   std::string_view source_id,
                                   const PendingAnnotation& annotation,
                                   const DeclarationContext& context) {
    for (const auto& requirement : annotation.requirements) {
        append_edge(fragment, input, source_id, requirement, annotation.line,
                    context.kind, context.symbol);
    }
}

bool ignorable_between_annotation_and_declaration(std::string_view line) noexcept {
    line = trim(line);
    return line.empty() || line.starts_with("//") || line.starts_with("/*") ||
           line.starts_with('*') || line.starts_with("*/") || line.starts_with("template") ||
           line.starts_with("[[");
}

}  // namespace

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
        .source = SourceLocation{.path = *normalized},
    });

    std::optional<PendingAnnotation> pending;
    std::string declaration;
    std::uint32_t line_number = 0;
    std::size_t offset = 0;
    while (offset <= input.content.size()) {
        ++line_number;
        const auto end = input.content.find('\n', offset);
        const auto line = std::string_view(input.content).substr(
            offset, end == std::string::npos ? std::string::npos : end - offset);

        if (const auto file = parse_marker(line, "@req-file", fragment, input, line_number)) {
            append_file_annotation(fragment, input, source_id, *file);
        } else if (const auto local = parse_marker(line, "@req", fragment, input, line_number)) {
            if (pending && !pending->requirements.empty()) {
                pending->requirements.insert(pending->requirements.end(),
                                             local->requirements.begin(), local->requirements.end());
            } else {
                pending = *local;
            }
        } else if (pending) {
            if (ignorable_between_annotation_and_declaration(line)) {
                if (!trim(line).empty() && !trim(line).starts_with("//") &&
                    !trim(line).starts_with("/*") && !trim(line).starts_with('*') &&
                    !trim(line).starts_with("*/")) {
                    declaration += std::string(trim(line));
                    declaration += ' ';
                }
            } else {
                declaration += std::string(trim(line));
                declaration += ' ';
            }

            if (!declaration.empty() && (declaration.find('{') != std::string::npos ||
                                         declaration.find(';') != std::string::npos)) {
                if (const auto context = declaration_context(declaration)) {
                    append_declaration_annotation(fragment, input, source_id, *pending, *context);
                } else {
                    fragment.diagnostics.push_back(Diagnostic{
                        .code = "source.annotation.unsupported_target",
                        .severity = Severity::warning,
                        .message = "@req must immediately precede a supported class, struct, enum, function, or method declaration",
                        .source = SourceLocation{.path = input.path, .line = pending->line},
                    });
                }
                pending.reset();
                declaration.clear();
            }
        }

        if (end == std::string::npos) {
            break;
        }
        offset = end + 1;
    }

    if (pending) {
        fragment.diagnostics.push_back(Diagnostic{
            .code = "source.annotation.missing_target",
            .severity = Severity::warning,
            .message = "@req has no following supported declaration",
            .source = SourceLocation{.path = input.path, .line = pending->line},
        });
    }
    return fragment;
}

}  // namespace mcutrace
