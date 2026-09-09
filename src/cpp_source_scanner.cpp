#include "source_annotation_scanner.hpp"

#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mcutrace::detail {
namespace {

struct DeclarationContext final {
    std::string kind;
    std::string symbol;
};

struct ScannerState final {
    std::vector<std::size_t> pending;
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

void finish_pending(SourceScanResult& result,
                    const ArtifactInput& input,
                    ScannerState& state) {
    const auto context = declaration_context(state.declaration);
    const auto line = result.markers[state.pending.front()].line;
    if (context) {
        const AnnotationTarget target{
            .line = line,
            .scope = context->kind,
            .symbol = context->symbol,
            .identity = state.declaration,
        };
        for (const auto index : state.pending) {
            result.markers[index].target = target;
        }
    } else {
        result.diagnostics.push_back(Diagnostic{
            .code = "source.annotation.unsupported_target",
            .severity = Severity::warning,
            .message = "@req must immediately precede a supported class, struct, enum, function, or method declaration",
            .source = SourceLocation{.path = input.path, .line = line},
        });
    }
    state.pending.clear();
    state.declaration.clear();
}

}  // namespace

// @req REQ-0194
SourceScanResult scan_cpp_source(const ArtifactInput& input) {
    SourceScanResult result;
    ScannerState state;
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
            result.markers.push_back(std::move(*marker));
            state.pending.push_back(result.markers.size() - 1);
        } else if (!state.pending.empty()) {
            append_declaration_piece(state, line);
            if (!state.declaration.empty() && declaration_complete(state.declaration)) {
                finish_pending(result, input, state);
            }
        }

        if (end == std::string::npos) {
            break;
        }
        offset = end + 1;
    }

    if (!state.pending.empty()) {
        result.diagnostics.push_back(Diagnostic{
            .code = "source.annotation.missing_target",
            .severity = Severity::warning,
            .message = "@req has no following supported declaration",
            .source = SourceLocation{
                .path = input.path, .line = result.markers[state.pending.front()].line},
        });
    }
    return result;
}

}  // namespace mcutrace::detail
