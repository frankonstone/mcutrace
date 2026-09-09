#include "source_annotation_scanner.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mcutrace::detail {
namespace {

enum class TokenKind : std::uint8_t {
    identifier,
    literal,
    punctuation,
};

struct Token final {
    std::string text;
    std::size_t begin = 0;
    std::size_t end = 0;
    std::uint32_t line = 0;
    TokenKind kind = TokenKind::punctuation;
};

struct LexResult final {
    std::vector<Token> tokens;
    SourceScanResult scan;
};

struct ScopeRange final {
    std::size_t open = 0;
    std::size_t close = 0;
    std::string name;
    bool is_class = false;
};

constexpr std::size_t kNoIndex = std::numeric_limits<std::size_t>::max();

bool identifier_start(char ch) noexcept {
    return std::isalpha(static_cast<unsigned char>(ch)) != 0 || ch == '_' || ch == '$';
}

bool identifier_continue(char ch) noexcept {
    return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_' || ch == '$';
}

bool regex_can_begin_after(const std::vector<Token>& tokens) noexcept {
    if (tokens.empty()) {
        return true;
    }
    const auto& previous = tokens.back().text;
    if (previous == ")" || previous == "]" || previous == "}" || previous == "++" ||
        previous == "--" || tokens.back().kind == TokenKind::literal) {
        return false;
    }
    if (tokens.back().kind == TokenKind::identifier) {
        return previous == "return" || previous == "throw" || previous == "case" ||
               previous == "delete" || previous == "typeof" || previous == "void" ||
               previous == "new" || previous == "in" || previous == "of" ||
               previous == "instanceof" || previous == "yield" || previous == "await";
    }
    return true;
}

void advance_newline(std::string_view source, std::size_t& offset, std::uint32_t& line) noexcept {
    if (source[offset] == '\r' && offset + 1 < source.size() && source[offset + 1] == '\n') {
        ++offset;
    }
    ++offset;
    ++line;
}

std::size_t scan_quoted(std::string_view source,
                        std::size_t offset,
                        std::uint32_t& line,
                        char quote) noexcept {
    ++offset;
    bool escaped = false;
    while (offset < source.size()) {
        const char ch = source[offset];
        if (ch == '\n' || ch == '\r') {
            advance_newline(source, offset, line);
            escaped = false;
            continue;
        }
        ++offset;
        if (escaped) {
            escaped = false;
        } else if (ch == '\\') {
            escaped = true;
        } else if (ch == quote) {
            break;
        }
    }
    return offset;
}

std::size_t scan_regex(std::string_view source,
                       std::size_t offset,
                       std::uint32_t& line) noexcept {
    ++offset;
    bool escaped = false;
    bool character_class = false;
    while (offset < source.size()) {
        const char ch = source[offset++];
        if (ch == '\n' || ch == '\r') {
            if (ch == '\r' && offset < source.size() && source[offset] == '\n') {
                ++offset;
            }
            ++line;
            break;
        }
        if (escaped) {
            escaped = false;
        } else if (ch == '\\') {
            escaped = true;
        } else if (ch == '[') {
            character_class = true;
        } else if (ch == ']') {
            character_class = false;
        } else if (ch == '/' && !character_class) {
            while (offset < source.size() && identifier_continue(source[offset])) {
                ++offset;
            }
            break;
        }
    }
    return offset;
}

bool two_character_punctuation(std::string_view value) noexcept {
    return value == "=>" || value == "++" || value == "--" || value == "==" ||
           value == "!=" || value == "<=" || value == ">=" || value == "&&" ||
           value == "||" || value == "??" || value == "?." || value == "**";
}

class JavascriptLexer final {
  public:
    explicit JavascriptLexer(const ArtifactInput& input) : source_(input.content) {}

    LexResult run() {
        while (offset_ < source_.size()) {
            const char ch = source_[offset_];
            if (ch == '\n' || ch == '\r') {
                advance_newline(source_, offset_, line_);
            } else if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
                ++offset_;
            } else if (identifier_start(ch)) {
                scan_identifier();
            } else if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
                scan_number();
            } else if (ch == '\'' || ch == '"' || ch == '`') {
                scan_string(ch);
            } else if (ch == '/') {
                scan_slash();
            } else {
                scan_punctuation();
            }
        }
        return std::move(result_);
    }

  private:
    void append(std::size_t begin, std::uint32_t line, TokenKind kind) {
        result_.tokens.push_back(Token{
            .text = std::string(source_.substr(begin, offset_ - begin)),
            .begin = begin,
            .end = offset_,
            .line = line,
            .kind = kind,
        });
        regex_after_control_ = false;
    }

    void scan_identifier() {
        const auto begin = offset_++;
        while (offset_ < source_.size() && identifier_continue(source_[offset_])) {
            ++offset_;
        }
        append(begin, line_, TokenKind::identifier);
    }

    void scan_number() {
        const auto begin = offset_++;
        while (offset_ < source_.size()) {
            const char ch = source_[offset_];
            if (std::isalnum(static_cast<unsigned char>(ch)) == 0 && ch != '.' && ch != '_') {
                break;
            }
            ++offset_;
        }
        append(begin, line_, TokenKind::literal);
    }

    void scan_string(char quote) {
        const auto begin = offset_;
        const auto token_line = line_;
        offset_ = scan_quoted(source_, offset_, line_, quote);
        append(begin, token_line, TokenKind::literal);
    }

    void scan_line_comment(std::size_t begin, std::uint32_t token_line) {
        offset_ += 2;
        while (offset_ < source_.size() && source_[offset_] != '\n' &&
               source_[offset_] != '\r') {
            ++offset_;
        }
        append_comment_markers(result_.scan, source_.substr(begin, offset_ - begin), token_line,
                               begin, offset_);
    }

    void scan_block_comment(std::size_t begin, std::uint32_t token_line) {
        offset_ += 2;
        while (offset_ < source_.size()) {
            if (offset_ + 1 < source_.size() && source_[offset_] == '*' &&
                source_[offset_ + 1] == '/') {
                offset_ += 2;
                break;
            }
            if (source_[offset_] == '\n' || source_[offset_] == '\r') {
                advance_newline(source_, offset_, line_);
            } else {
                ++offset_;
            }
        }
        append_comment_markers(result_.scan, source_.substr(begin, offset_ - begin), token_line,
                               begin, offset_);
    }

    void scan_slash() {
        const auto begin = offset_;
        const auto token_line = line_;
        const char next = offset_ + 1 < source_.size() ? source_[offset_ + 1] : '\0';
        if (next == '/') {
            scan_line_comment(begin, token_line);
        } else if (next == '*') {
            scan_block_comment(begin, token_line);
        } else if (regex_after_control_ || regex_can_begin_after(result_.tokens)) {
            offset_ = scan_regex(source_, offset_, line_);
            append(begin, token_line, TokenKind::literal);
        } else {
            ++offset_;
            append(begin, token_line, TokenKind::punctuation);
        }
    }

    void scan_punctuation() {
        const auto begin = offset_;
        const auto token_line = line_;
        const char punctuation = source_[offset_];
        bool closes_control = false;
        if (punctuation == '(') {
            const auto previous = result_.tokens.empty()
                ? std::string_view{}
                : std::string_view{result_.tokens.back().text};
            control_parentheses_.push_back(previous == "if" || previous == "while" ||
                                           previous == "for" || previous == "with" ||
                                           previous == "switch" || previous == "catch");
        } else if (punctuation == ')' && !control_parentheses_.empty()) {
            closes_control = control_parentheses_.back();
            control_parentheses_.pop_back();
        }
        if (offset_ + 1 < source_.size() &&
            two_character_punctuation(source_.substr(offset_, 2))) {
            offset_ += 2;
        } else {
            ++offset_;
        }
        append(begin, token_line, TokenKind::punctuation);
        regex_after_control_ = closes_control;
    }

    std::string_view source_;
    LexResult result_;
    std::size_t offset_ = 0;
    std::uint32_t line_ = 1;
    std::vector<bool> control_parentheses_;
    bool regex_after_control_ = false;
};

LexResult lex_javascript(const ArtifactInput& input) {
    return JavascriptLexer(input).run();
}

std::vector<std::size_t> matching_delimiters(const std::vector<Token>& tokens,
                                             std::string_view open,
                                             std::string_view close) {
    std::vector<std::size_t> matching(tokens.size(), kNoIndex);
    std::vector<std::size_t> stack;
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        if (tokens[index].text == open) {
            stack.push_back(index);
        } else if (tokens[index].text == close && !stack.empty()) {
            const auto begin = stack.back();
            stack.pop_back();
            matching[begin] = index;
            matching[index] = begin;
        }
    }
    return matching;
}

std::size_t find_token(const std::vector<Token>& tokens,
                       std::size_t begin,
                       std::string_view text,
                       std::size_t limit = kNoIndex) noexcept {
    const auto end = std::min(tokens.size(), limit);
    for (std::size_t index = begin; index < end; ++index) {
        if (tokens[index].text == text) {
            return index;
        }
        if (tokens[index].text == ";") {
            break;
        }
    }
    return kNoIndex;
}

std::vector<std::size_t> brace_depths(const std::vector<Token>& tokens) {
    std::vector<std::size_t> depths(tokens.size());
    std::size_t depth = 0;
    for (std::size_t index = 0; index < tokens.size(); ++index) {
        depths[index] = depth;
        if (tokens[index].text == "{") {
            ++depth;
        } else if (tokens[index].text == "}" && depth != 0) {
            --depth;
            depths[index] = depth;
        }
    }
    return depths;
}

struct SyntaxIndex final {
    const std::vector<Token>& tokens;
    const std::vector<std::size_t>& braces;
    const std::vector<std::size_t>& parentheses;
    const std::vector<std::size_t>& brackets;
    const std::vector<std::size_t>& depths;
};

void append_class_and_function_scopes(const SyntaxIndex& syntax,
                                      std::vector<ScopeRange>& result) {
    for (std::size_t index = 0; index < syntax.tokens.size(); ++index) {
        bool is_class = syntax.tokens[index].text == "class";
        auto name = index + 1;
        if (!is_class && syntax.tokens[index].text != "function") {
            continue;
        }
        if (!is_class && name < syntax.tokens.size() && syntax.tokens[name].text == "*") {
            ++name;
        }
        if (name >= syntax.tokens.size() ||
            syntax.tokens[name].kind != TokenKind::identifier) {
            continue;
        }
        const auto open = find_token(syntax.tokens, name + 1, "{");
        if (open == kNoIndex || syntax.braces[open] == kNoIndex) {
            continue;
        }
        result.push_back(ScopeRange{
            .open = open,
            .close = syntax.braces[open],
            .name = syntax.tokens[name].text,
            .is_class = is_class,
        });
    }
}

bool is_function_value(const SyntaxIndex& syntax, std::size_t value) noexcept {
    if (value < syntax.tokens.size() && syntax.tokens[value].text == "function") {
        return true;
    }
    if (value + 1 < syntax.tokens.size() &&
        syntax.tokens[value].kind == TokenKind::identifier &&
        syntax.tokens[value + 1].text == "=>") {
        return true;
    }
    return value < syntax.tokens.size() && syntax.tokens[value].text == "(" &&
           syntax.parentheses[value] != kNoIndex &&
           syntax.parentheses[value] + 1 < syntax.tokens.size() &&
           syntax.tokens[syntax.parentheses[value] + 1].text == "=>";
}

void append_variable_scopes(const SyntaxIndex& syntax, std::vector<ScopeRange>& result) {
    for (std::size_t index = 0; index + 3 < syntax.tokens.size(); ++index) {
        const bool declaration = syntax.tokens[index].text == "const" ||
                                 syntax.tokens[index].text == "let" ||
                                 syntax.tokens[index].text == "var";
        if (!declaration || syntax.tokens[index + 1].kind != TokenKind::identifier ||
            syntax.tokens[index + 2].text != "=") {
            continue;
        }
        auto value = index + 3;
        if (syntax.tokens[value].text == "async") {
            ++value;
        }
        if (!is_function_value(syntax, value)) {
            continue;
        }
        const auto open = find_token(syntax.tokens, value + 1, "{");
        if (open != kNoIndex && syntax.braces[open] != kNoIndex) {
            result.push_back(ScopeRange{
                .open = open,
                .close = syntax.braces[open],
                .name = syntax.tokens[index + 1].text,
                .is_class = false,
            });
        }
    }
}

std::optional<std::pair<std::string, std::size_t>>
method_name(const SyntaxIndex& syntax, std::size_t begin, std::size_t limit) {
    auto name = begin;
    while (name < limit &&
           (syntax.tokens[name].text == "static" || syntax.tokens[name].text == "async" ||
            syntax.tokens[name].text == "get" || syntax.tokens[name].text == "set")) {
        ++name;
    }
    if (name < limit && syntax.tokens[name].text == "*") {
        ++name;
    }
    std::string symbol;
    if (name < limit && syntax.tokens[name].text == "[" && syntax.brackets[name] != kNoIndex) {
        for (auto cursor = name; cursor <= syntax.brackets[name]; ++cursor) {
            symbol += syntax.tokens[cursor].text;
        }
        name = syntax.brackets[name] + 1;
    } else if (name < limit && (syntax.tokens[name].kind == TokenKind::identifier ||
                                syntax.tokens[name].kind == TokenKind::literal)) {
        symbol = syntax.tokens[name++].text;
    }
    if (symbol.empty()) {
        return std::nullopt;
    }
    return std::pair{std::move(symbol), name};
}

std::optional<ScopeRange> method_scope(const SyntaxIndex& syntax,
                                       const ScopeRange& class_scope,
                                       std::size_t begin) {
    if (syntax.depths[begin] != syntax.depths[class_scope.open] + 1 ||
        (begin != class_scope.open + 1 && syntax.tokens[begin - 1].text != "}" &&
         syntax.tokens[begin - 1].text != ";")) {
        return std::nullopt;
    }
    auto named = method_name(syntax, begin, class_scope.close);
    if (!named || named->second >= class_scope.close || syntax.tokens[named->second].text != "(") {
        return std::nullopt;
    }
    const auto parameters = named->second;
    if (syntax.parentheses[parameters] == kNoIndex ||
        syntax.parentheses[parameters] + 1 >= class_scope.close) {
        return std::nullopt;
    }
    const auto open = syntax.parentheses[parameters] + 1;
    if (syntax.tokens[open].text != "{" || syntax.braces[open] == kNoIndex) {
        return std::nullopt;
    }
    return ScopeRange{
        .open = open,
        .close = syntax.braces[open],
        .name = std::move(named->first),
        .is_class = false,
    };
}

void append_method_scopes(const SyntaxIndex& syntax, std::vector<ScopeRange>& result) {
    const auto existing = result;
    for (const auto& scope : existing) {
        if (!scope.is_class) {
            continue;
        }
        for (auto begin = scope.open + 1; begin < scope.close; ++begin) {
            if (auto method = method_scope(syntax, scope, begin)) {
                result.push_back(std::move(*method));
            }
        }
    }
}

std::vector<ScopeRange> discover_named_scopes(const SyntaxIndex& syntax) {
    std::vector<ScopeRange> result;
    append_class_and_function_scopes(syntax, result);
    append_variable_scopes(syntax, result);
    append_method_scopes(syntax, result);
    std::sort(result.begin(), result.end(), [](const ScopeRange& left, const ScopeRange& right) {
        return left.open == right.open ? left.name < right.name : left.open < right.open;
    });
    return result;
}

std::string lexical_scope(const std::vector<ScopeRange>& scopes, std::size_t token_index) {
    std::string result;
    for (const auto& scope : scopes) {
        if (scope.open < token_index && token_index < scope.close) {
            if (!result.empty()) {
                result += '/';
            }
            result += scope.name;
        }
    }
    return result;
}

std::optional<ScopeRange> containing_class(const std::vector<ScopeRange>& scopes,
                                           const std::vector<Token>& tokens,
                                           const std::vector<std::size_t>& depths,
                                           std::size_t token_index) {
    std::optional<ScopeRange> result;
    for (const auto& scope : scopes) {
        if (scope.is_class && scope.open < token_index && token_index < scope.close &&
            tokens[scope.open].text == "{" && depths[token_index] == depths[scope.open] + 1) {
            result = scope;
        }
    }
    return result;
}

std::size_t skip_declaration_modifiers(const std::vector<Token>& tokens,
                                       std::size_t index) noexcept {
    while (index < tokens.size() &&
           (tokens[index].text == "export" || tokens[index].text == "default" ||
            tokens[index].text == "async")) {
        ++index;
    }
    return index;
}

std::string declaration_shape(const std::vector<Token>& tokens,
                              std::size_t begin,
                              std::size_t search_from) {
    auto end = search_from;
    while (end < tokens.size() && tokens[end].text != "{" && tokens[end].text != ";") {
        ++end;
    }
    if (end < tokens.size()) {
        ++end;
    }
    std::string result;
    for (auto index = begin; index < end; ++index) {
        if (!result.empty()) {
            result += ' ';
        }
        result += tokens[index].text;
    }
    return result;
}

struct ParserContext final {
    const SyntaxIndex& syntax;
    const std::vector<ScopeRange>& scopes;
};

struct TargetSpec final {
    std::size_t begin = 0;
    std::size_t shape_from = 0;
    std::string kind;
    std::string symbol;
    std::uint32_t annotation_line = 0;
};

AnnotationTarget make_target(const ParserContext& context, TargetSpec spec) {
    const auto container = lexical_scope(context.scopes, spec.begin);
    std::string identity = spec.kind + "|" + container + "|" + spec.symbol + "|" +
                           declaration_shape(context.syntax.tokens, spec.begin, spec.shape_from);
    return AnnotationTarget{
        .line = spec.annotation_line,
        .scope = std::move(spec.kind),
        .symbol = std::move(spec.symbol),
        .identity = std::move(identity),
    };
}

std::optional<std::size_t> function_expression_shape(const SyntaxIndex& syntax,
                                                     std::size_t value) noexcept {
    if (value >= syntax.tokens.size() || syntax.tokens[value].text != "function") {
        return std::nullopt;
    }
    auto parameters = value + 1;
    if (parameters < syntax.tokens.size() && syntax.tokens[parameters].text == "*") {
        ++parameters;
    }
    if (parameters < syntax.tokens.size() &&
        syntax.tokens[parameters].kind == TokenKind::identifier) {
        ++parameters;
    }
    if (parameters >= syntax.tokens.size() || syntax.tokens[parameters].text != "(" ||
        syntax.parentheses[parameters] == kNoIndex ||
        syntax.parentheses[parameters] + 1 >= syntax.tokens.size() ||
        syntax.tokens[syntax.parentheses[parameters] + 1].text != "{") {
        return std::nullopt;
    }
    return value;
}

std::optional<std::size_t> arrow_shape(const SyntaxIndex& syntax,
                                       std::size_t value) noexcept {
    if (value + 1 < syntax.tokens.size() &&
        syntax.tokens[value].kind == TokenKind::identifier &&
        syntax.tokens[value + 1].text == "=>") {
        return value + 1;
    }
    if (value >= syntax.tokens.size() || syntax.tokens[value].text != "(" ||
        syntax.parentheses[value] == kNoIndex ||
        syntax.parentheses[value] + 1 >= syntax.tokens.size() ||
        syntax.tokens[syntax.parentheses[value] + 1].text != "=>") {
        return std::nullopt;
    }
    return syntax.parentheses[value] + 1;
}

std::optional<AnnotationTarget> parse_variable_target(
    const ParserContext& context,
    std::size_t begin,
    std::size_t index,
    std::uint32_t annotation_line) {
    const auto& tokens = context.syntax.tokens;
    if (index + 3 >= tokens.size() || tokens[index + 1].kind != TokenKind::identifier ||
        tokens[index + 2].text != "=") {
        return std::nullopt;
    }
    const std::string symbol = tokens[index + 1].text;
    auto value = index + 3;
    if (tokens[value].text == "async") {
        ++value;
    }
    if (const auto shape = function_expression_shape(context.syntax, value)) {
        return make_target(context, TargetSpec{
            begin, *shape, "function", symbol, annotation_line});
    }
    if (const auto shape = arrow_shape(context.syntax, value)) {
        return make_target(context, TargetSpec{
            begin, *shape, "function", symbol, annotation_line});
    }
    return std::nullopt;
}

std::optional<AnnotationTarget> parse_method_target(
    const ParserContext& context,
    std::size_t begin,
    std::size_t index,
    std::uint32_t annotation_line) {
    const auto& tokens = context.syntax.tokens;
    const auto& parentheses = context.syntax.parentheses;
    const auto& brackets = context.syntax.brackets;
    while (index < tokens.size() &&
           (tokens[index].text == "static" || tokens[index].text == "async" ||
            tokens[index].text == "get" || tokens[index].text == "set")) {
        ++index;
    }
    if (index < tokens.size() && tokens[index].text == "*") {
        ++index;
    }
    if (index >= tokens.size()) {
        return std::nullopt;
    }

    std::string symbol;
    if (tokens[index].text == "[") {
        if (brackets[index] == kNoIndex) {
            return std::nullopt;
        }
        for (auto cursor = index; cursor <= brackets[index]; ++cursor) {
            symbol += tokens[cursor].text;
        }
        index = brackets[index] + 1;
    } else if (tokens[index].kind == TokenKind::identifier ||
               tokens[index].kind == TokenKind::literal) {
        symbol = tokens[index++].text;
    } else {
        return std::nullopt;
    }
    if (index >= tokens.size() || tokens[index].text != "(" ||
        parentheses[index] == kNoIndex || parentheses[index] + 1 >= tokens.size() ||
        tokens[parentheses[index] + 1].text != "{") {
        return std::nullopt;
    }
    return make_target(context, TargetSpec{
        begin, index, "method", std::move(symbol), annotation_line});
}

std::optional<AnnotationTarget> parse_function_target(const ParserContext& context,
                                                      std::size_t begin,
                                                      std::size_t function,
                                                      std::uint32_t annotation_line) {
    const auto& tokens = context.syntax.tokens;
    auto name = function + 1;
    if (name < tokens.size() && tokens[name].text == "*") {
        ++name;
    }
    if (name >= tokens.size() || tokens[name].kind != TokenKind::identifier ||
        name + 1 >= tokens.size() || tokens[name + 1].text != "(") {
        return std::nullopt;
    }
    const auto close = context.syntax.parentheses[name + 1];
    if (close == kNoIndex || close + 1 >= tokens.size() || tokens[close + 1].text != "{") {
        return std::nullopt;
    }
    return make_target(context, TargetSpec{
        begin, name, "function", tokens[name].text, annotation_line});
}

std::optional<AnnotationTarget> parse_class_target(const ParserContext& context,
                                                   std::size_t begin,
                                                   std::size_t declaration,
                                                   std::uint32_t annotation_line) {
    const auto& tokens = context.syntax.tokens;
    const auto name = declaration + 1;
    if (name >= tokens.size() || tokens[name].kind != TokenKind::identifier ||
        find_token(tokens, name + 1, "{") == kNoIndex) {
        return std::nullopt;
    }
    return make_target(context, TargetSpec{
        begin, name, "class", tokens[name].text, annotation_line});
}

std::optional<AnnotationTarget> parse_declaration(
    const ParserContext& context,
    std::size_t begin,
    std::uint32_t annotation_line) {
    const auto& tokens = context.syntax.tokens;
    auto index = skip_declaration_modifiers(tokens, begin);
    if (index >= tokens.size()) {
        return std::nullopt;
    }
    if (tokens[index].text == "function") {
        return parse_function_target(context, begin, index, annotation_line);
    }
    if (tokens[index].text == "class") {
        return parse_class_target(context, begin, index, annotation_line);
    }
    if (tokens[index].text == "const" || tokens[index].text == "let" ||
        tokens[index].text == "var") {
        return parse_variable_target(context, begin, index, annotation_line);
    }
    if (containing_class(context.scopes, tokens, context.syntax.depths, begin)) {
        return parse_method_target(context, begin, index, annotation_line);
    }
    return std::nullopt;
}

}  // namespace

// @req REQ-0195 REQ-0196 REQ-0197 REQ-0198 REQ-0199
SourceScanResult scan_javascript_source(const ArtifactInput& input) {
    auto lexed = lex_javascript(input);
    const auto braces = matching_delimiters(lexed.tokens, "{", "}");
    const auto parentheses = matching_delimiters(lexed.tokens, "(", ")");
    const auto brackets = matching_delimiters(lexed.tokens, "[", "]");
    const auto depths = brace_depths(lexed.tokens);
    const SyntaxIndex syntax{lexed.tokens, braces, parentheses, brackets, depths};
    const auto scopes = discover_named_scopes(syntax);
    const ParserContext context{syntax, scopes};

    for (auto& marker : lexed.scan.markers) {
        if (marker.kind == SourceMarkerKind::file) {
            marker.target = AnnotationTarget{
                .line = marker.line, .scope = "file", .symbol = {}, .identity = {}};
            continue;
        }
        const auto next = std::lower_bound(
            lexed.tokens.begin(), lexed.tokens.end(), marker.end,
            [](const Token& token, std::size_t offset) { return token.begin < offset; });
        if (next == lexed.tokens.end()) {
            lexed.scan.diagnostics.push_back(Diagnostic{
                .code = "source.annotation.missing_target",
                .severity = Severity::warning,
                .message = "@req has no following supported JavaScript declaration",
                .source = SourceLocation{.path = input.path, .line = marker.line},
            });
            continue;
        }
        const auto index = static_cast<std::size_t>(next - lexed.tokens.begin());
        marker.target = parse_declaration(context, index, marker.line);
        if (!marker.target) {
            lexed.scan.diagnostics.push_back(Diagnostic{
                .code = "source.annotation.unsupported_target",
                .severity = Severity::warning,
                .message = "@req must immediately precede a named JavaScript function, class, method, or function-valued variable declaration",
                .source = SourceLocation{.path = input.path, .line = marker.line},
            });
        }
    }
    return std::move(lexed.scan);
}

}  // namespace mcutrace::detail
