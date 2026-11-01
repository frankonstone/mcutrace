// @req-file REQ-0192 REQ-0193 REQ-0225
#include "source_annotation_scanner.hpp"
#include <string_view>

namespace mcutrace::detail {
namespace {
// Tcl 9 syntax: https://www.tcl-lang.org/man/tcl9.0/TclCmd/Tcl.html
// This first profile accepts annotations in outer-script comments only. Braced
// words (including proc bodies) are data until evaluated by their owning command.
// Substitutions are traversed for delimiter balance, never interpreted as claims.
class Scanner {
    const ArtifactInput &input;
    std::string_view text;
    std::size_t pos{};
    std::uint32_t line{1};
    unsigned depth{};
    SourceScanResult result;
    void step() {
        if (text[pos++] == '\n')
            ++line;
    }
    bool continuation() {
        if (text[pos] != '\\' || pos + 1 >= text.size())
            return false;
        step();
        step();
        return true;
    }
    bool braced() {
        step();
        unsigned nesting = 1;
        while (pos < text.size()) {
            if (continuation())
                continue;
            const char c = text[pos];
            step();
            if (c == '{')
                ++nesting;
            else if (c == '}' && --nesting == 0)
                return true;
        }
        return false;
    }
    bool quoted() {
        step();
        while (pos < text.size()) {
            if (continuation())
                continue;
            if (text[pos] == '"') {
                step();
                return true;
            }
            if (text[pos] == '[') {
                step();
                if (!script(true))
                    return false;
            } else if (text[pos] == '$') {
                if (!variable())
                    return false;
            } else
                step();
        }
        return false;
    }
    bool variable() {
        if (++depth > 128)
            return false;
        const bool valid = variable_impl();
        --depth;
        return valid;
    }
    bool variable_impl() {
        step();
        if (pos < text.size() && text[pos] == '{') {
            step();
            while (pos < text.size() && text[pos] != '}')
                step();
            if (pos == text.size())
                return false;
            step();
            return true;
        }
        while (pos < text.size()) {
            const char c = text[pos];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')
                step();
            else if (c == ':' && pos + 1 < text.size() && text[pos + 1] == ':') {
                step();
                step();
                while (pos < text.size() && text[pos] == ':')
                    step();
            } else
                break;
        }
        if (pos == text.size() || text[pos] != '(')
            return true;
        step();
        while (pos < text.size()) {
            if (continuation())
                continue;
            if (text[pos] == ')') {
                step();
                return true;
            }
            if (text[pos] == '[') {
                step();
                if (!script(true))
                    return false;
            } else if (text[pos] == '$') {
                if (!variable())
                    return false;
            } else
                step();
        }
        return false;
    }
    void comment(bool outer) {
        const auto begin = pos;
        const auto first_line = line;
        while (pos < text.size()) {
            if (text[pos] == '\\' && pos + 1 < text.size() && text[pos + 1] == '\n') {
                step();
                step();
                continue;
            }
            if (text[pos] == '\n')
                break;
            step();
        }
        if (!outer)
            return;
        const auto before = result.markers.size();
        append_comment_markers(result, text.substr(begin, pos - begin), first_line, begin, pos);
        for (auto i = before; i < result.markers.size(); ++i) {
            auto &marker = result.markers[i];
            if (marker.kind == SourceMarkerKind::file) {
                marker.target =
                    AnnotationTarget{.line = marker.line, .scope = "file", .symbol = {}, .identity = {}};
            } else {
                result.diagnostics.push_back(Diagnostic{
                    .code = "source.annotation.unsupported_target",
                    .severity = Severity::warning,
                    .message = "Tcl tracing currently supports outer-script @req-file annotations only",
                    .source = SourceLocation{.path = input.path, .line = marker.line}});
            }
        }
    }
    bool script(bool nested) {
        if (++depth > 128)
            return false;
        bool command_start = true, word_start = true;
        while (pos < text.size()) {
            const char c = text[pos];
            if (c == '\\' && pos + 1 < text.size() && text[pos + 1] == '\n') {
                step();
                step();
                word_start = true;
                continue;
            }
            if (c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f') {
                step();
                word_start = true;
                continue;
            }
            if (c == '\n' || c == ';') {
                step();
                command_start = word_start = true;
                continue;
            }
            if (c == ']' && nested) {
                step();
                --depth;
                return true;
            }
            if (c == '#' && command_start) {
                comment(!nested);
                continue;
            }
            command_start = false;
            if (word_start && text.substr(pos, 3) == "{*}" && pos + 3 < text.size() && text[pos + 3] != ' ' &&
                text[pos + 3] != '\t' && text[pos + 3] != '\n' && text[pos + 3] != '\r' &&
                text[pos + 3] != '\f' && text[pos + 3] != '\v') {
                step();
                step();
                step();
                continue;
            }
            const bool delimited_word = word_start && (c == '{' || c == '"');
            if (c == '{' && word_start) {
                if (!braced())
                    return false;
            } else if (c == '"' && word_start) {
                if (!quoted())
                    return false;
            } else if (c == '[') {
                step();
                if (!script(true))
                    return false;
            } else if (c == '$') {
                if (!variable())
                    return false;
            } else if (!continuation())
                step();
            if (delimited_word && pos < text.size()) {
                const char next = text[pos];
                const bool separator = next == ' ' || next == '\t' || next == '\r' || next == '\n' ||
                                       next == '\f' || next == '\v' || next == ';' ||
                                       (nested && next == ']') ||
                                       (next == '\\' && pos + 1 < text.size() && text[pos + 1] == '\n');
                if (!separator)
                    return false;
            }
            word_start = false;
        }
        --depth;
        return !nested;
    }

  public:
    explicit Scanner(const ArtifactInput &value) : input(value), text(value.content) {}
    SourceScanResult scan() {
        if (!script(false)) {
            result.markers.clear();
            result.diagnostics.push_back(
                Diagnostic{.code = "source.annotation.invalid_tcl",
                           .severity = Severity::error,
                           .message = "unbalanced Tcl input or nesting limit exceeded",
                           .source = SourceLocation{.path = input.path, .line = line}});
        }
        return std::move(result);
    }
};
} // namespace
SourceScanResult scan_tcl_source(const ArtifactInput &input) { return Scanner(input).scan(); }
} // namespace mcutrace::detail
