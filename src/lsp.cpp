#include <mcutrace/lsp.hpp>

#include <mcutrace/assembly.hpp>
#include <mcutrace/build_annotations.hpp>
#include <mcutrace/config.hpp>
#include <mcutrace/output.hpp>
#include <mcutrace/requirements.hpp>
#include <mcutrace/source_annotations.hpp>
#include <mcutrace/trace_import.hpp>
#include <mcutrace/validation.hpp>

#include <mcujson/mcujson.hpp>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mcutrace {
namespace {

using LspJson = mcujson::BasicJson<1024U, 60000U>;

struct Position final {
    std::uint32_t line = 0;
    std::uint32_t character = 0;
};

struct Range final {
    Position start;
    Position end;
};

struct OpenDocument final {
    std::string uri;
    std::string text;
    std::int64_t version = 0;
};

struct RelatedDiagnostic final {
    SourceLocation source;
    std::string message;
    std::string identifier;
};

struct PublishedDiagnostic final {
    Diagnostic diagnostic;
    std::string identifier;
    std::vector<RelatedDiagnostic> related;
};

class JsonObject final {
  public:
    void member(std::string_view key, std::string value) {
        if (!first_) {
            value_ += ',';
        }
        first_ = false;
        value_ += quote(key);
        value_ += ':';
        value_ += value;
    }

    void string(std::string_view key, std::string_view value) {
        member(key, quote(value));
    }

    [[nodiscard]] std::string finish() && {
        return '{' + std::move(value_) + '}';
    }

    [[nodiscard]] static std::string quote(std::string_view value) {
        constexpr std::string_view digits = "0123456789abcdef";
        std::string result;
        result.reserve(value.size() + 2U);
        result.push_back('"');
        for (const unsigned char ch : value) {
            switch (ch) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (ch < 0x20U) {
                    result += "\\u00";
                    result.push_back(digits[ch >> 4U]);
                    result.push_back(digits[ch & 0x0fU]);
                } else {
                    result.push_back(static_cast<char>(ch));
                }
                break;
            }
        }
        result.push_back('"');
        return result;
    }

  private:
    std::string value_;
    bool first_ = true;
};

class JsonArray final {
  public:
    void value(std::string value) {
        if (!first_) {
            value_ += ',';
        }
        first_ = false;
        value_ += value;
    }

    void string(std::string_view value) { this->value(JsonObject::quote(value)); }

    [[nodiscard]] std::string finish() && {
        return '[' + std::move(value_) + ']';
    }

  private:
    std::string value_;
    bool first_ = true;
};

std::string response(std::string_view id, std::string result) {
    JsonObject object;
    object.string("jsonrpc", "2.0");
    object.member("id", std::string(id));
    object.member("result", std::move(result));
    return std::move(object).finish();
}

std::string error_response(std::string_view id, int code, std::string_view message) {
    JsonObject error;
    error.member("code", std::to_string(code));
    error.string("message", message);
    JsonObject object;
    object.string("jsonrpc", "2.0");
    object.member("id", id.empty() ? "null" : std::string(id));
    object.member("error", std::move(error).finish());
    return std::move(object).finish();
}

std::string notification(std::string_view method, std::string params) {
    JsonObject object;
    object.string("jsonrpc", "2.0");
    object.string("method", method);
    object.member("params", std::move(params));
    return std::move(object).finish();
}

std::optional<std::string> string_value(const mcujson::JsonRef value) {
    if (!value.is_string()) {
        return std::nullopt;
    }
    return value.get<std::string>();
}

std::optional<std::uint32_t> number_value(const mcujson::JsonRef value) {
    if (!value.is_number()) {
        return std::nullopt;
    }
    const long long number = value.get<long long>();
    if (number < 0 || number > static_cast<long long>(UINT32_MAX)) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(number);
}

std::optional<Position> read_position(const mcujson::JsonRef value) {
    if (!value.is_object()) {
        return std::nullopt;
    }
    const auto line = number_value(value["line"]);
    const auto character = number_value(value["character"]);
    if (!line || !character) {
        return std::nullopt;
    }
    return Position{.line = *line, .character = *character};
}

std::string normalize_path(std::string value) {
    std::filesystem::path path(std::move(value));
    if (path.is_relative()) {
        path = std::filesystem::current_path() / path;
    }
    return path.lexically_normal().generic_string();
}

int hex_value(const char value) noexcept {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

std::string decode_uri_path(std::string_view value) {
    constexpr std::string_view prefix = "file://";
    if (value.starts_with(prefix)) {
        value.remove_prefix(prefix.size());
        if (value.starts_with("localhost/")) {
            value.remove_prefix(std::string_view{"localhost"}.size());
        }
    }
    std::string result;
    result.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '%' && index + 2U < value.size()) {
            const int high = hex_value(value[index + 1U]);
            const int low = hex_value(value[index + 2U]);
            if (high >= 0 && low >= 0) {
                result.push_back(static_cast<char>((high << 4) | low));
                index += 2U;
                continue;
            }
        }
        result.push_back(value[index]);
    }
    return normalize_path(std::move(result));
}

std::string encode_uri_path(std::string_view path) {
    constexpr std::string_view safe = "-._~/";
    constexpr std::string_view digits = "0123456789ABCDEF";
    std::string result = "file://";
    for (const unsigned char ch : path) {
        if (std::isalnum(ch) != 0 || safe.find(static_cast<char>(ch)) != std::string_view::npos) {
            result.push_back(static_cast<char>(ch));
        } else {
            result.push_back('%');
            result.push_back(digits[ch >> 4U]);
            result.push_back(digits[ch & 0x0fU]);
        }
    }
    return result;
}

std::optional<std::string> read_file(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

std::string position_json(const Position position) {
    JsonObject object;
    object.member("line", std::to_string(position.line));
    object.member("character", std::to_string(position.character));
    return std::move(object).finish();
}

std::string range_json(const Range range) {
    JsonObject object;
    object.member("start", position_json(range.start));
    object.member("end", position_json(range.end));
    return std::move(object).finish();
}

std::size_t utf8_width(const unsigned char value) noexcept {
    if ((value & 0x80U) == 0U) {
        return 1U;
    }
    if ((value & 0xe0U) == 0xc0U) {
        return 2U;
    }
    if ((value & 0xf0U) == 0xe0U) {
        return 3U;
    }
    if ((value & 0xf8U) == 0xf0U) {
        return 4U;
    }
    return 1U;
}

std::uint32_t utf16_units(std::string_view text, const std::size_t limit) noexcept {
    std::uint32_t result = 0;
    for (std::size_t index = 0; index < limit;) {
        const auto width = std::min(utf8_width(static_cast<unsigned char>(text[index])), limit - index);
        result += width == 4U ? 2U : 1U;
        index += width;
    }
    return result;
}

std::size_t utf16_offset(std::string_view text, const std::uint32_t character) noexcept {
    std::uint32_t units = 0;
    std::size_t index = 0;
    while (index < text.size() && units < character) {
        const auto width = std::min(utf8_width(static_cast<unsigned char>(text[index])), text.size() - index);
        const auto next = static_cast<std::uint32_t>(units + (width == 4U ? 2U : 1U));
        if (next > character) {
            break;
        }
        units = next;
        index += width;
    }
    return index;
}

std::optional<std::string_view> line_at(std::string_view content, const std::uint32_t wanted) {
    std::uint32_t line = 0;
    std::size_t begin = 0;
    while (begin <= content.size()) {
        const auto end = content.find('\n', begin);
        const auto actual_end = end == std::string_view::npos ? content.size() : end;
        if (line == wanted) {
            auto result = content.substr(begin, actual_end - begin);
            if (result.ends_with('\r')) {
                result.remove_suffix(1);
            }
            return result;
        }
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1U;
        ++line;
    }
    return std::nullopt;
}

bool identifier_boundary(std::string_view content, const std::size_t start, const std::size_t end) noexcept {
    const auto alnum = [](const char value) {
        return std::isalnum(static_cast<unsigned char>(value)) != 0;
    };
    return (start == 0U || !alnum(content[start - 1U])) &&
           (end == content.size() || !alnum(content[end]));
}

std::vector<Range> find_references(std::string_view content, std::string_view identifier) {
    std::vector<Range> result;
    if (!is_requirement_id(identifier)) {
        return result;
    }
    std::uint32_t line = 0;
    std::size_t begin = 0;
    while (begin <= content.size()) {
        const auto end = content.find('\n', begin);
        const auto actual_end = end == std::string_view::npos ? content.size() : end;
        auto text = content.substr(begin, actual_end - begin);
        if (text.ends_with('\r')) {
            text.remove_suffix(1);
        }
        std::size_t offset = text.find(identifier);
        while (offset != std::string_view::npos) {
            const auto after = offset + identifier.size();
            if (identifier_boundary(text, offset, after)) {
                result.push_back(Range{
                    .start = Position{.line = line, .character = utf16_units(text, offset)},
                    .end = Position{.line = line, .character = utf16_units(text, after)},
                });
            }
            offset = text.find(identifier, offset + 1U);
        }
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1U;
        ++line;
    }
    return result;
}

std::optional<std::string> identifier_at(std::string_view content, const Position position) {
    const auto line = line_at(content, position.line);
    if (!line) {
        return std::nullopt;
    }
    const auto cursor = utf16_offset(*line, position.character);
    for (std::size_t index = 0; index + 8U <= line->size(); ++index) {
        const auto candidate = line->substr(index, 8U);
        if (is_requirement_id(candidate) && identifier_boundary(*line, index, index + 8U) &&
            cursor >= index && cursor < index + 8U) {
            return std::string(candidate);
        }
    }
    return std::nullopt;
}

Range empty_range(std::uint32_t line) {
    return Range{.start = Position{.line = line}, .end = Position{.line = line}};
}

std::optional<Range> reference_range(std::string_view line_text,
                                     std::uint32_t line,
                                     std::string_view identifier) {
    if (identifier.empty()) {
        return std::nullopt;
    }
    const auto references = find_references(line_text, identifier);
    if (references.empty()) {
        return std::nullopt;
    }
    auto result = references.front();
    result.start.line = line;
    result.end.line = line;
    return result;
}

Range source_location_range(const SourceLocation& source,
                            std::string_view content,
                            std::uint32_t line,
                            std::string_view line_text) {
    const auto start = source.column == 0U ? 0U : source.column - 1U;
    const auto character = std::min(start, utf16_units(line_text, line_text.size()));
    if (source.end_line == 0U && source.end_column == 0U) {
        return Range{
            .start = Position{.line = line, .character = character},
            .end = Position{.line = line, .character = utf16_units(line_text, line_text.size())},
        };
    }
    const auto end_line = source.end_line == 0U ? line : source.end_line - 1U;
    const auto end_text = line_at(content, end_line);
    const auto end_start = source.end_column == 0U || !end_text
        ? 0U : source.end_column - 1U;
    const auto end_character = end_text
        ? std::min(end_start, utf16_units(*end_text, end_text->size()))
        : 0U;
    return Range{
        .start = Position{.line = line, .character = character},
        .end = Position{.line = end_line, .character = end_character},
    };
}

}  // namespace

struct LanguageServer::State final {
    std::optional<ProjectConfig> config;
    std::string config_path;
    std::string workspace_root;
    RequirementParseResult requirement_parse;
    TraceResult trace;
    ValidationResult validation;
    std::map<std::string, OpenDocument> open_documents;
    std::map<std::string, std::vector<PublishedDiagnostic>> diagnostics;
    std::set<std::string> published_paths;
    bool exit = false;

    [[nodiscard]] std::optional<std::string> text_for(const std::string& path) const {
        const auto open = open_documents.find(path);
        if (open != open_documents.end()) {
            return open->second.text;
        }
        return read_file(path);
    }

    [[nodiscard]] Range source_range(const SourceLocation& source,
                                     std::string_view identifier = {}) const {
        const auto text = text_for(source.path);
        const auto line = source.line == 0U ? 0U : source.line - 1U;
        if (!text) {
            return empty_range(line);
        }
        const auto line_text = line_at(*text, line);
        if (!line_text) {
            return empty_range(line);
        }
        if (const auto reference = reference_range(*line_text, line, identifier)) {
            return *reference;
        }
        return source_location_range(source, *text, line, *line_text);
    }

    [[nodiscard]] std::string location(const SourceLocation& source,
                                       std::string_view identifier = {}) const {
        JsonObject object;
        object.string("uri", encode_uri_path(source.path));
        object.member("range", range_json(source_range(source, identifier)));
        return std::move(object).finish();
    }

    void append(const Diagnostic& diagnostic, std::string identifier = {},
                std::vector<RelatedDiagnostic> related = {}) {
        if (!diagnostic.source || diagnostic.source->path.empty()) {
            return;
        }
        diagnostics[diagnostic.source->path].push_back(PublishedDiagnostic{
            .diagnostic = diagnostic,
            .identifier = std::move(identifier),
            .related = std::move(related),
        });
    }

    void append_input_failure(std::string code, std::string message, const std::string& path) {
        append(Diagnostic{.code = std::move(code), .severity = Severity::error,
                          .message = std::move(message),
                          .source = SourceLocation{.path = path, .line = 1U, .column = 1U}});
    }

    void append_duplicate_diagnostics() {
        std::map<std::string, std::vector<const Requirement*>> groups;
        for (const auto& definition : requirement_parse.definitions) {
            groups[definition.id].push_back(&definition);
        }
        for (const auto& [identifier, definitions] : groups) {
            if (definitions.size() < 2U) {
                continue;
            }
            for (const auto* definition : definitions) {
                std::vector<RelatedDiagnostic> related;
                for (const auto* other : definitions) {
                    if (other != definition) {
                        related.push_back(RelatedDiagnostic{
                            .source = other->source,
                            .message = "other definition of " + identifier,
                            .identifier = identifier,
                        });
                    }
                }
                append(Diagnostic{
                    .code = "MTR-REQ-DUPLICATE-ID",
                    .severity = Severity::warning,
                    .message = "duplicate requirement ID " + identifier,
                    .source = definition->source,
                }, identifier, std::move(related));
            }
        }
    }

    void reset_analysis() {
        diagnostics.clear();
        requirement_parse = {};
        trace = {};
        validation = {};
    }

    void analyze_requirements() {
        std::vector<std::string> contents;
        contents.reserve(config->requirement_files.size());
        std::vector<RequirementDocument> documents;
        documents.reserve(config->requirement_files.size());
        for (const auto& path : config->requirement_files) {
            auto content = text_for(path);
            if (!content) {
                append_input_failure("lsp.requirement_read", "cannot read requirement document: " + path, path);
                continue;
            }
            contents.push_back(std::move(*content));
            documents.push_back(RequirementDocument{.path = path, .content = contents.back()});
        }
        requirement_parse = parse_requirements(documents);
        for (const auto& diagnostic : requirement_parse.diagnostics) {
            if (diagnostic.code != "MTR-REQ-DUPLICATE-ID") {
                append(diagnostic);
            }
        }
        append_duplicate_diagnostics();
    }

    void import_source_fragments(std::vector<ImportFragment>& fragments) {
        for (const auto& path : config->source_files) {
            auto content = text_for(path);
            if (!content) {
                append_input_failure("lsp.source_read", "cannot read source input: " + path, path);
                continue;
            }
            auto fragment = import_source_annotations(ArtifactInput{
                .path = path, .base_directory = config->root, .content = std::move(*content)});
            if (!fragment) {
                append_input_failure("lsp.source_import", fragment.error().detail, path);
                continue;
            }
            fragments.push_back(std::move(*fragment));
        }
    }

    void import_build_fragments(std::vector<ImportFragment>& fragments) {
        for (const auto& path : config->build_files) {
            auto content = text_for(path);
            if (!content) {
                append_input_failure("lsp.build_read", "cannot read build input: " + path, path);
                continue;
            }
            auto fragment = import_build_annotations(ArtifactInput{
                .path = path, .base_directory = config->root, .content = std::move(*content)});
            if (!fragment) {
                append_input_failure("lsp.build_import", fragment.error().detail, path);
                continue;
            }
            fragments.push_back(std::move(*fragment));
        }
    }

    void import_artifact_fragments(std::vector<ImportFragment>& fragments) {
        for (const auto& artifact : config->artifacts) {
            auto content = text_for(artifact.path);
            if (!content) {
                append_input_failure("lsp.artifact_read", "cannot read artifact input: " + artifact.path,
                                     artifact.path);
                continue;
            }
            auto fragment = import_trace_artifact(ArtifactInput{
                .path = artifact.path,
                .base_directory = artifact.base_directory,
                .content = std::move(*content),
            }, artifact.importer);
            if (!fragment) {
                append_input_failure("lsp.artifact_import", fragment.error().detail, artifact.path);
                continue;
            }
            fragments.push_back(std::move(*fragment));
        }
    }

    void validate_fragments(const std::vector<ImportFragment>& fragments) {
        trace = assemble_trace(requirement_parse.requirements, fragments);
        validation = validate_trace(trace, config->validation);
        for (const auto& diagnostic : validation.diagnostics) {
            append(diagnostic);
        }
    }

    [[nodiscard]] std::vector<std::string> analyze() {
        reset_analysis();
        if (config) {
            analyze_requirements();
            std::vector<ImportFragment> fragments;
            fragments.reserve(config->source_files.size() + config->build_files.size() +
                              config->artifacts.size());
            import_source_fragments(fragments);
            import_build_fragments(fragments);
            import_artifact_fragments(fragments);
            validate_fragments(fragments);
        }
        return publish_diagnostics();
    }

    [[nodiscard]] std::vector<std::string> publish_diagnostics() {
        std::set<std::string> paths = published_paths;
        for (const auto& [path, ignored] : diagnostics) {
            static_cast<void>(ignored);
            paths.insert(path);
        }
        std::vector<std::string> result;
        for (const auto& path : paths) {
            JsonArray values;
            const auto found = diagnostics.find(path);
            if (found != diagnostics.end()) {
                for (const auto& diagnostic : found->second) {
                    JsonObject value;
                    value.member("range", range_json(source_range(
                        *diagnostic.diagnostic.source, diagnostic.identifier)));
                    const auto severity = diagnostic.diagnostic.severity == Severity::error ? 1 :
                        diagnostic.diagnostic.severity == Severity::warning ? 2 : 3;
                    value.member("severity", std::to_string(severity));
                    value.string("code", diagnostic.diagnostic.code);
                    value.string("source", "mcutrace");
                    value.string("message", diagnostic.diagnostic.message);
                    if (!diagnostic.related.empty()) {
                        JsonArray related;
                        for (const auto& relation : diagnostic.related) {
                            JsonObject item;
                            item.member("location", location(relation.source, relation.identifier));
                            item.string("message", relation.message);
                            related.value(std::move(item).finish());
                        }
                        value.member("relatedInformation", std::move(related).finish());
                    }
                    values.value(std::move(value).finish());
                }
            }
            JsonObject params;
            params.string("uri", encode_uri_path(path));
            params.member("diagnostics", std::move(values).finish());
            result.push_back(notification("textDocument/publishDiagnostics", std::move(params).finish()));
        }
        published_paths.clear();
        for (const auto& [path, ignored] : diagnostics) {
            static_cast<void>(ignored);
            published_paths.insert(path);
        }
        return result;
    }

    [[nodiscard]] std::optional<std::string> identifier(const mcujson::JsonRef params) const {
        const auto document = params["textDocument"];
        const auto uri = string_value(document["uri"]);
        const auto position = read_position(params["position"]);
        if (!uri || !position) {
            return std::nullopt;
        }
        const auto text = text_for(decode_uri_path(*uri));
        return text ? identifier_at(*text, *position) : std::nullopt;
    }

    [[nodiscard]] std::vector<const Requirement*> definitions(std::string_view identifier) const {
        std::vector<const Requirement*> result;
        for (const auto& definition : requirement_parse.definitions) {
            if (definition.id == identifier) {
                result.push_back(&definition);
            }
        }
        return result;
    }

    [[nodiscard]] const Requirement* requirement(std::string_view identifier) const {
        const auto found = std::find_if(requirement_parse.requirements.begin(),
                                        requirement_parse.requirements.end(),
            [identifier](const Requirement& value) { return value.id == identifier; });
        return found == requirement_parse.requirements.end() ? nullptr : &*found;
    }

    [[nodiscard]] std::set<std::string> workspace_paths() const {
        std::set<std::string> result;
        if (config) {
            result.insert(config->requirement_files.begin(), config->requirement_files.end());
            result.insert(config->source_files.begin(), config->source_files.end());
            for (const auto& artifact : config->artifacts) {
                result.insert(artifact.path);
            }
        }
        for (const auto& [path, ignored] : open_documents) {
            static_cast<void>(ignored);
            result.insert(path);
        }
        return result;
    }

    [[nodiscard]] std::string locations_for(std::string_view identifier,
                                            const std::vector<const Requirement*>& values) const {
        JsonArray locations;
        for (const auto* definition : values) {
            locations.value(location(definition->source, identifier));
        }
        return std::move(locations).finish();
    }

    [[nodiscard]] std::string reference_locations(std::string_view identifier) const {
        JsonArray locations;
        for (const auto& path : workspace_paths()) {
            const auto text = text_for(path);
            if (!text) {
                continue;
            }
            for (const auto& range : find_references(*text, identifier)) {
                JsonObject location_value;
                location_value.string("uri", encode_uri_path(path));
                location_value.member("range", range_json(range));
                locations.value(std::move(location_value).finish());
            }
        }
        return std::move(locations).finish();
    }

    [[nodiscard]] std::string implementation_locations(std::string_view identifier) const {
        JsonArray locations;
        for (const auto& edge : trace.graph.edges()) {
            if (edge.type.kind != RelationshipKind::implements || edge.target_id != identifier) {
                continue;
            }
            const auto* implementation = trace.graph.find_node(edge.source_id);
            if (implementation != nullptr && implementation->source) {
                locations.value(location(*implementation->source));
            }
        }
        return std::move(locations).finish();
    }

    [[nodiscard]] static std::string inline_code(std::string_view value) {
        std::size_t longest_fence = 0U;
        std::size_t fence = 0U;
        for (const char character : value) {
            if (character == '`') {
                ++fence;
                longest_fence = std::max(longest_fence, fence);
            } else {
                fence = 0U;
            }
        }
        const std::string delimiter(longest_fence + 1U, '`');
        return delimiter + std::string(value) + delimiter;
    }

    [[nodiscard]] static std::string escape_markdown(std::string_view value) {
        constexpr std::string_view special = "\\`*_{}[]<>()#+-.!|";
        std::string result;
        result.reserve(value.size());
        for (const char character : value) {
            if (special.find(character) != std::string_view::npos) {
                result.push_back('\\');
            }
            result.push_back(character);
        }
        return result;
    }

    [[nodiscard]] static std::string escape_markdown_link_text(std::string_view value) {
        constexpr std::string_view special = "\\\\[]";
        std::string result;
        result.reserve(value.size());
        for (const char character : value) {
            if (special.find(character) != std::string_view::npos) {
                result.push_back('\\');
            }
            result.push_back(character);
        }
        return result;
    }

    [[nodiscard]] std::string absolute_path(std::string_view path) const {
        std::filesystem::path result(path);
        if (result.is_relative()) {
            const std::string_view base = !workspace_root.empty() ? std::string_view(workspace_root) :
                config ? std::string_view(config->root) : std::string_view{};
            if (!base.empty()) {
                result = std::filesystem::path(base) / result;
            }
        }
        return result.lexically_normal().generic_string();
    }

    [[nodiscard]] std::string workspace_relative_path(std::string_view path) const {
        const std::filesystem::path absolute(absolute_path(path));
        const std::string_view base = !workspace_root.empty() ? std::string_view(workspace_root) :
            config ? std::string_view(config->root) : std::string_view{};
        if (base.empty()) {
            return absolute.generic_string();
        }
        const auto relative = absolute.lexically_relative(std::filesystem::path(base));
        return relative.empty() ? absolute.generic_string() : relative.generic_string();
    }

    [[nodiscard]] std::string file_link(const SourceLocation& source) const {
        std::string label = workspace_relative_path(source.path);
        if (source.line != 0U) {
            label += ':' + std::to_string(source.line);
        }
        return "[" + escape_markdown_link_text(label) + "](" +
               encode_uri_path(absolute_path(source.path)) + ")";
    }

    [[nodiscard]] static std::string evidence_state_label(const EvidenceState state) {
        switch (state) {
        case EvidenceState::passed: return "✅ passed";
        case EvidenceState::failed: return "❌ failed";
        case EvidenceState::unknown: return "❔ unknown";
        }
        return "❔ unknown";
    }

    [[nodiscard]] static std::string finding_state_label(std::string_view state) {
        if (state == "violation" || state == "failed") {
            return "❌ " + std::string(state);
        }
        if (state == "limited" || state == "unavailable") {
            return "⚠️ " + std::string(state);
        }
        if (state == "informational") {
            return "ℹ️ " + std::string(state);
        }
        return "⚪ " + std::string(state);
    }

    [[nodiscard]] static std::string evidence_detail(const Node& node) {
        std::vector<std::string> values;
        if (node.evidence_state != EvidenceState::unknown) {
            values.push_back(evidence_state_label(node.evidence_state));
        }
        if (!node.evidence_detail.empty()) {
            values.push_back(node.evidence_detail);
        }
        if (!node.finding_state.empty()) {
            values.push_back(finding_state_label(node.finding_state));
        }
        std::string result;
        for (const auto& value : values) {
            if (!result.empty()) {
                result += " — ";
            }
            result += value;
        }
        return result;
    }

    void append_hover_evidence_group(std::string& markdown,
                                    std::string_view heading,
                                    const std::vector<Node>& nodes) const {
        if (nodes.empty()) {
            return;
        }
        markdown += "\n\n**" + std::string(heading) + "**";
        for (const auto& node : nodes) {
            const std::string_view label = node.label.empty() ? std::string_view(node.id) :
                                                                std::string_view(node.label);
            const std::string location = node.source ? file_link(*node.source) : std::string{};
            markdown += "\n- ";
            if ((node.kind == NodeKind::source || node.kind == NodeKind::coverage) &&
                !location.empty()) {
                markdown += location;
            } else {
                markdown += inline_code(label);
                if (!location.empty()) {
                    markdown += " — " + location;
                }
            }
            const std::string detail = evidence_detail(node);
            if (!detail.empty()) {
                markdown += " — " + escape_markdown(detail);
            }
        }
    }

    void append_hover_evidence(std::string& markdown,
                               const RequirementTraceReport& report) const {
        append_hover_evidence_group(markdown, "Implementations", report.implementations);
        append_hover_evidence_group(markdown, "Sources", report.sources);
        append_hover_evidence_group(markdown, "Tests", report.tests);
        append_hover_evidence_group(markdown, "Coverage", report.coverage);
        append_hover_evidence_group(markdown, "Build evidence", report.builds);
        append_hover_evidence_group(markdown, "Static analysis findings", report.findings);
    }

    [[nodiscard]] std::string hover(std::string_view identifier) const {
        const auto* value = requirement(identifier);
        if (value == nullptr) {
            return "null";
        }
        std::string markdown = "**" + value->id + "**";
        if (!value->title.empty()) {
            markdown += " — " + value->title;
        }
        if (!value->body.empty()) {
            markdown += "\n\n" + value->body;
        }
        const auto report = build_requirement_trace_report(trace, identifier);
        if (report) {
            append_hover_evidence(markdown, *report);
        }
        JsonObject contents;
        contents.string("kind", "markdown");
        contents.string("value", markdown);
        JsonObject result;
        result.member("contents", std::move(contents).finish());
        return std::move(result).finish();
    }

    [[nodiscard]] std::string document_symbols(const mcujson::JsonRef params) const {
        const auto uri = string_value(params["textDocument"]["uri"]);
        if (!uri) {
            return "[]";
        }
        const auto path = decode_uri_path(*uri);
        JsonArray symbols;
        for (const auto& definition : requirement_parse.definitions) {
            if (definition.source.path != path) {
                continue;
            }
            JsonObject symbol;
            symbol.string("name", definition.id + (definition.title.empty() ? "" : " — " + definition.title));
            symbol.member("kind", "13");
            symbol.member("range", range_json(source_range(definition.source)));
            symbol.member("selectionRange", range_json(source_range(definition.source, definition.id)));
            symbols.value(std::move(symbol).finish());
        }
        return std::move(symbols).finish();
    }

    [[nodiscard]] std::string workspace_symbols(const mcujson::JsonRef params) const {
        const auto query = string_value(params["query"]).value_or("");
        JsonArray symbols;
        for (const auto& definition : requirement_parse.definitions) {
            const auto name = definition.id + (definition.title.empty() ? "" : " — " + definition.title);
            if (!query.empty() && name.find(query) == std::string::npos) {
                continue;
            }
            JsonObject symbol;
            symbol.string("name", name);
            symbol.member("kind", "13");
            symbol.string("containerName", "mcutrace requirements");
            symbol.member("location", location(definition.source, definition.id));
            symbols.value(std::move(symbol).finish());
        }
        return std::move(symbols).finish();
    }

    [[nodiscard]] std::string completion() const {
        JsonArray items;
        for (const auto& requirement_value : requirement_parse.requirements) {
            JsonObject item;
            item.string("label", requirement_value.id);
            item.member("kind", "12");
            item.string("detail", requirement_value.title);
            items.value(std::move(item).finish());
        }
        for (const auto annotation : {std::string_view{"@req"},
                                      std::string_view{"@evidence(test)"},
                                      std::string_view{"@evidence(implementation)"},
                                      std::string_view{"@evidence(coverage)"},
                                      std::string_view{"@evidence(none)"}}) {
            JsonObject item;
            item.string("label", annotation);
            item.member("kind", "14");
            items.value(std::move(item).finish());
        }
        JsonObject result;
        result.member("isIncomplete", "false");
        result.member("items", std::move(items).finish());
        return std::move(result).finish();
    }

    [[nodiscard]] std::string code_lenses(const mcujson::JsonRef params) const {
        const auto uri = string_value(params["textDocument"]["uri"]);
        if (!uri) {
            return "[]";
        }
        const auto path = decode_uri_path(*uri);
        JsonArray lenses;
        for (const auto& definition : requirement_parse.definitions) {
            if (definition.source.path != path) {
                continue;
            }
            const auto report = build_requirement_trace_report(trace, definition.id);
            const auto implementations = report ? report->implementations.size() : 0U;
            const auto tests = report ? report->tests.size() : 0U;
            const auto coverage = report ? report->coverage.size() : 0U;
            const auto findings = report ? report->findings.size() : 0U;
            JsonArray arguments;
            arguments.string(definition.id);
            JsonObject command;
            command.string("title", std::to_string(implementations) + " implementations · " +
                                     std::to_string(tests) + " tests · " +
                                     std::to_string(coverage) + " coverage · " +
                                     std::to_string(findings) + " findings");
            command.string("command", "mcutrace.showTrace");
            command.member("arguments", std::move(arguments).finish());
            JsonObject lens;
            lens.member("range", range_json(source_range(definition.source, definition.id)));
            lens.member("command", std::move(command).finish());
            lenses.value(std::move(lens).finish());
        }
        return std::move(lenses).finish();
    }

    [[nodiscard]] std::string rename(const mcujson::JsonRef params, std::string& failure) const {
        const auto identifier_value = identifier(params);
        const auto replacement = string_value(params["newName"]);
        if (!identifier_value || !replacement || !is_requirement_id(*replacement)) {
            failure = "a canonical REQ-NNNN identifier is required";
            return {};
        }
        if (*replacement != *identifier_value && requirement(*replacement) != nullptr) {
            failure = "the new requirement identifier already exists";
            return {};
        }
        JsonObject changes;
        for (const auto& path : workspace_paths()) {
            const auto text = text_for(path);
            if (!text) {
                continue;
            }
            const auto references = find_references(*text, *identifier_value);
            if (references.empty()) {
                continue;
            }
            JsonArray edits;
            for (const auto& range : references) {
                JsonObject edit;
                edit.member("range", range_json(range));
                edit.string("newText", *replacement);
                edits.value(std::move(edit).finish());
            }
            changes.member(encode_uri_path(path), std::move(edits).finish());
        }
        JsonObject result;
        result.member("changes", std::move(changes).finish());
        return std::move(result).finish();
    }

    [[nodiscard]] std::string execute_command(const mcujson::JsonRef params) {
        const auto command = string_value(params["command"]);
        if (command && *command == "mcutrace.refresh") {
            return "null";
        }
        if (command && *command == "mcutrace.showTrace") {
            const auto arguments = params["arguments"];
            if (arguments.is_array() && arguments.size() > 0U && arguments[0].is_string()) {
                return hover(arguments[0].get<std::string_view>());
            }
        }
        return "null";
    }

    [[nodiscard]] std::string initialize_result() const {
        JsonObject completion_provider;
        JsonArray triggers;
        triggers.string("R");
        triggers.string("@");
        completion_provider.member("triggerCharacters", std::move(triggers).finish());
        JsonObject code_lens;
        code_lens.member("resolveProvider", "false");
        JsonArray commands;
        commands.string("mcutrace.showTrace");
        commands.string("mcutrace.refresh");
        JsonObject execute;
        execute.member("commands", std::move(commands).finish());
        JsonObject capabilities;
        capabilities.member("textDocumentSync", "1");
        capabilities.member("hoverProvider", "true");
        capabilities.member("definitionProvider", "true");
        capabilities.member("referencesProvider", "true");
        capabilities.member("implementationProvider", "true");
        capabilities.member("documentSymbolProvider", "true");
        capabilities.member("workspaceSymbolProvider", "true");
        capabilities.member("completionProvider", std::move(completion_provider).finish());
        capabilities.member("codeLensProvider", std::move(code_lens).finish());
        capabilities.member("renameProvider", "true");
        capabilities.member("codeActionProvider", "true");
        capabilities.member("executeCommandProvider", std::move(execute).finish());
        JsonObject info;
        info.string("name", "mcutrace");
        info.string("version", "0.1.0");
        JsonObject result;
        result.member("capabilities", std::move(capabilities).finish());
        result.member("serverInfo", std::move(info).finish());
        return std::move(result).finish();
    }

    [[nodiscard]] std::vector<std::string> load_config(const mcujson::JsonRef params) {
        const auto root_uri = string_value(params["rootUri"]);
        workspace_root = root_uri ? decode_uri_path(*root_uri) :
            std::filesystem::current_path().generic_string();
        const auto initialized = params["initializationOptions"];
        auto config_path = string_value(initialized["configPath"]);
        if (!config_path && !this->config_path.empty()) {
            config_path = this->config_path;
        }
        if (!config_path) {
            const auto root = root_uri ? decode_uri_path(*root_uri) : std::filesystem::current_path().generic_string();
            config_path = root + "/mcutrace.toml";
        }
        std::string resolved_path = *config_path;
        if (resolved_path.starts_with("file://")) {
            resolved_path = decode_uri_path(resolved_path);
        } else {
            resolved_path = normalize_path(std::move(resolved_path));
        }
        config.reset();
        this->config_path = std::move(resolved_path);
        const auto content = read_file(this->config_path);
        if (!content) {
            return {notification("window/logMessage", log_message(
                1, "cannot read mcutrace configuration: " + this->config_path))};
        }
        const auto parsed = parse_project_config(*content, this->config_path);
        if (!parsed) {
            return {notification("window/logMessage", log_message(1, "invalid mcutrace configuration: " + parsed.error().detail))};
        }
        config = *parsed;
        return {};
    }

    [[nodiscard]] static std::string log_message(const int type, std::string_view message) {
        JsonObject params;
        params.member("type", std::to_string(type));
        params.string("message", message);
        return std::move(params).finish();
    }

    using Messages = std::vector<std::string>;
    using RequestHandler = Messages (State::*)(const mcujson::JsonRef&, const std::string&);
    using DocumentUpdate = std::pair<std::string, OpenDocument>;

    [[nodiscard]] static Messages reply_to(const std::string& id, std::string value) {
        return id.empty() ? Messages{} : Messages{response(id, std::move(value))};
    }

    [[nodiscard]] static Messages invalid_request(const std::string& id, std::string_view detail) {
        return id.empty() ? Messages{} : Messages{error_response(id, -32602, detail)};
    }

    static void append_messages(Messages& result, Messages messages) {
        result.insert(result.end(), std::make_move_iterator(messages.begin()),
                      std::make_move_iterator(messages.end()));
    }

    [[nodiscard]] static std::optional<DocumentUpdate> open_document(const mcujson::JsonRef& params) {
        const auto document = params["textDocument"];
        const auto uri = string_value(document["uri"]);
        const auto text = string_value(document["text"]);
        if (!uri || !text) {
            return std::nullopt;
        }
        return DocumentUpdate{decode_uri_path(*uri), OpenDocument{
            .uri = *uri, .text = *text, .version = document["version"].get<long long>()}};
    }

    [[nodiscard]] static std::optional<DocumentUpdate> changed_document(const mcujson::JsonRef& params) {
        const auto document = params["textDocument"];
        const auto uri = string_value(document["uri"]);
        const auto changes = params["contentChanges"];
        if (!uri || !changes.is_array() || changes.size() == 0U) {
            return std::nullopt;
        }
        const auto text = string_value(changes[changes.size() - 1U]["text"]);
        if (!text) {
            return std::nullopt;
        }
        return DocumentUpdate{decode_uri_path(*uri), OpenDocument{
            .uri = *uri, .text = *text, .version = document["version"].get<long long>()}};
    }

    [[nodiscard]] Messages handle_initialize(const mcujson::JsonRef& params, const std::string& id) {
        auto result = reply_to(id, initialize_result());
        append_messages(result, load_config(params));
        append_messages(result, analyze());
        return result;
    }

    [[nodiscard]] Messages handle_shutdown(const mcujson::JsonRef&, const std::string& id) {
        return reply_to(id, "null");
    }

    [[nodiscard]] Messages handle_exit(const mcujson::JsonRef&, const std::string&) {
        exit = true;
        return {};
    }

    [[nodiscard]] Messages handle_did_open(const mcujson::JsonRef& params, const std::string& id) {
        const auto document = open_document(params);
        if (!document) {
            return invalid_request(id, "textDocument.uri and textDocument.text are required");
        }
        open_documents[document->first] = document->second;
        return analyze();
    }

    [[nodiscard]] Messages handle_did_change(const mcujson::JsonRef& params, const std::string& id) {
        const auto document = changed_document(params);
        if (!document) {
            return invalid_request(id, "a full textDocument change is required");
        }
        open_documents[document->first] = document->second;
        return analyze();
    }

    [[nodiscard]] Messages handle_did_close(const mcujson::JsonRef& params, const std::string&) {
        const auto uri = string_value(params["textDocument"]["uri"]);
        if (!uri) {
            return {};
        }
        open_documents.erase(decode_uri_path(*uri));
        return analyze();
    }

    [[nodiscard]] Messages handle_did_save(const mcujson::JsonRef&, const std::string&) {
        return analyze();
    }

    [[nodiscard]] Messages handle_configuration_change(const mcujson::JsonRef&, const std::string&) {
        Messages result = load_config(mcujson::JsonRef{});
        append_messages(result, analyze());
        return result;
    }

    [[nodiscard]] Messages handle_watched_files(const mcujson::JsonRef&, const std::string&) {
        return analyze();
    }

    [[nodiscard]] Messages handle_hover(const mcujson::JsonRef& params, const std::string& id) {
        const auto value = identifier(params);
        return reply_to(id, value ? hover(*value) : "null");
    }

    [[nodiscard]] Messages handle_definition(const mcujson::JsonRef& params, const std::string& id) {
        const auto value = identifier(params);
        return reply_to(id, value ? locations_for(*value, definitions(*value)) : "[]");
    }

    [[nodiscard]] Messages handle_references(const mcujson::JsonRef& params, const std::string& id) {
        const auto value = identifier(params);
        return reply_to(id, value ? reference_locations(*value) : "[]");
    }

    [[nodiscard]] Messages handle_implementation(const mcujson::JsonRef& params, const std::string& id) {
        const auto value = identifier(params);
        return reply_to(id, value ? implementation_locations(*value) : "[]");
    }

    [[nodiscard]] Messages handle_document_symbol(const mcujson::JsonRef& params, const std::string& id) {
        return reply_to(id, document_symbols(params));
    }

    [[nodiscard]] Messages handle_workspace_symbol(const mcujson::JsonRef& params, const std::string& id) {
        return reply_to(id, workspace_symbols(params));
    }

    [[nodiscard]] Messages handle_completion(const mcujson::JsonRef&, const std::string& id) {
        return reply_to(id, completion());
    }

    [[nodiscard]] Messages handle_code_lens(const mcujson::JsonRef& params, const std::string& id) {
        return reply_to(id, code_lenses(params));
    }

    [[nodiscard]] Messages handle_rename(const mcujson::JsonRef& params, const std::string& id) {
        std::string failure;
        const auto edit = rename(params, failure);
        return failure.empty() ? reply_to(id, edit) : invalid_request(id, failure);
    }

    [[nodiscard]] Messages handle_code_action(const mcujson::JsonRef&, const std::string& id) {
        return reply_to(id, "[]");
    }

    [[nodiscard]] Messages handle_execute_command(const mcujson::JsonRef& params, const std::string& id) {
        return reply_to(id, execute_command(params));
    }

    [[nodiscard]] Messages dispatch(std::string_view method, const mcujson::JsonRef& params,
                                    const std::string& id) {
        static const std::map<std::string_view, RequestHandler> handlers{
            {"initialize", &State::handle_initialize},
            {"shutdown", &State::handle_shutdown},
            {"exit", &State::handle_exit},
            {"textDocument/didOpen", &State::handle_did_open},
            {"textDocument/didChange", &State::handle_did_change},
            {"textDocument/didClose", &State::handle_did_close},
            {"textDocument/didSave", &State::handle_did_save},
            {"workspace/didChangeConfiguration", &State::handle_configuration_change},
            {"workspace/didChangeWatchedFiles", &State::handle_watched_files},
            {"textDocument/hover", &State::handle_hover},
            {"textDocument/definition", &State::handle_definition},
            {"textDocument/references", &State::handle_references},
            {"textDocument/implementation", &State::handle_implementation},
            {"textDocument/documentSymbol", &State::handle_document_symbol},
            {"workspace/symbol", &State::handle_workspace_symbol},
            {"textDocument/completion", &State::handle_completion},
            {"textDocument/codeLens", &State::handle_code_lens},
            {"textDocument/rename", &State::handle_rename},
            {"textDocument/codeAction", &State::handle_code_action},
            {"workspace/executeCommand", &State::handle_execute_command},
        };
        const auto handler = handlers.find(method);
        if (handler == handlers.end()) {
            return id.empty() ? Messages{} : Messages{error_response(id, -32601, "method not found")};
        }
        return (this->*(handler->second))(params, id);
    }
};

LanguageServer::LanguageServer() : state_(std::make_unique<State>()) {}
LanguageServer::~LanguageServer() {}

bool LanguageServer::exit_requested() const noexcept {
    return state_->exit;
}

// @req REQ-0103 REQ-0104 REQ-0105 REQ-0106 REQ-0107 REQ-0108 REQ-0109 REQ-0110 REQ-0111 REQ-0112 REQ-0113 REQ-0114 REQ-0115 REQ-0116 REQ-0117 REQ-0118 REQ-0119 REQ-0120 REQ-0121 REQ-0122 REQ-0123
std::vector<std::string> LanguageServer::handle(const std::string_view message) {
    const auto parsed = LspJson::parse(message);
    if (!parsed || !parsed->is_object()) {
        return {error_response({}, -32700, "invalid JSON-RPC message")};
    }
    const auto& root = *parsed;
    const auto method_value = string_value(root["method"]);
    const auto id = root["id"];
    const auto id_json = id.valid() ? id.dump() : std::string{};
    if (!method_value) {
        return id_json.empty() ? std::vector<std::string>{}
                               : std::vector<std::string>{error_response(id_json, -32600, "method is required")};
    }
    return state_->dispatch(*method_value, root["params"], id_json);
}

namespace {

std::optional<std::size_t> content_length(std::string_view value) {
    const auto first = value.find_first_not_of(' ');
    if (first == std::string_view::npos) {
        return std::nullopt;
    }
    std::size_t result = 0;
    const auto parsed = std::from_chars(value.data() + first, value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
        return std::nullopt;
    }
    return result;
}

enum class HeaderResult {
    ignored,
    content_length,
    invalid,
};

HeaderResult read_header(std::string_view line, std::size_t& length) {
    constexpr std::string_view header = "Content-Length:";
    if (!line.starts_with(header)) {
        return HeaderResult::ignored;
    }
    const auto value = content_length(line.substr(header.size()));
    if (!value) {
        return HeaderResult::invalid;
    }
    length = *value;
    return HeaderResult::content_length;
}

std::optional<std::string> read_message_body(std::istream& input, const std::size_t length) {
    std::string message(length, '\0');
    input.read(message.data(), static_cast<std::streamsize>(message.size()));
    if (input.gcount() != static_cast<std::streamsize>(message.size())) {
        return std::nullopt;
    }
    return message;
}

std::optional<std::string> read_framed_message(std::istream& input) {
    std::string line;
    std::size_t content_length = 0;
    bool has_length = false;
    while (std::getline(input, line)) {
        if (line.ends_with('\r')) {
            line.pop_back();
        }
        if (line.empty()) {
            break;
        }
        const auto header = read_header(line, content_length);
        if (header == HeaderResult::invalid) {
            return std::nullopt;
        }
        if (header == HeaderResult::content_length) {
            has_length = true;
        }
    }
    if (!has_length) {
        return std::nullopt;
    }
    return read_message_body(input, content_length);
}

void write_framed_message(std::ostream& output, std::string_view message) {
    output << "Content-Length: " << message.size() << "\r\n\r\n" << message;
    output.flush();
}

}  // namespace

int run_language_server(std::istream& input, std::ostream& output) {
    LanguageServer server;
    while (!server.exit_requested()) {
        const auto message = read_framed_message(input);
        if (!message) {
            return input.eof() ? 0 : 1;
        }
        for (const auto& response_message : server.handle(*message)) {
            write_framed_message(output, response_message);
        }
    }
    return 0;
}

}  // namespace mcutrace
