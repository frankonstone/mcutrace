#include <mcutrace/requirements.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace mcutrace {
namespace {

struct Heading final {
    std::uint8_t level = 0;
    std::string_view text;
    std::uint32_t line = 0;
    std::size_t line_begin = 0;
    std::size_t content_begin = 0;
};

std::string_view trim(std::string_view text) noexcept {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) {
        text.remove_suffix(1);
    }
    return text;
}

bool starts_fence(std::string_view line, char marker) noexcept {
    line = trim(line);
    std::size_t count = 0;
    while (count < line.size() && line[count] == marker) {
        ++count;
    }
    return count >= 3;
}

bool update_fence_state(std::string_view line, bool& fenced, char& marker) noexcept {
    if (!fenced && (starts_fence(line, '`') || starts_fence(line, '~'))) {
        marker = trim(line).front();
        fenced = true;
        return true;
    }
    if (fenced && starts_fence(line, marker)) {
        fenced = false;
        marker = '\0';
        return true;
    }
    return fenced;
}

std::optional<Heading> parse_heading_line(std::string_view line,
                                          std::uint32_t line_number,
                                          std::size_t line_begin,
                                          std::size_t content_begin) {
    std::size_t hashes = 0;
    while (hashes < line.size() && hashes < 6 && line[hashes] == '#') {
        ++hashes;
    }
    if (hashes == 0 || hashes == line.size() ||
        (line[hashes] != ' ' && line[hashes] != '\t')) {
        return std::nullopt;
    }

    auto heading_text = trim(line.substr(hashes + 1));
    while (!heading_text.empty() && heading_text.back() == '#') {
        heading_text.remove_suffix(1);
        heading_text = trim(heading_text);
    }
    return Heading{static_cast<std::uint8_t>(hashes), heading_text, line_number,
                   line_begin, content_begin};
}

std::vector<Heading> scan_headings(std::string_view content) {
    std::vector<Heading> headings;
    bool fenced = false;
    char fence_marker = '\0';
    std::uint32_t line_number = 1;
    std::size_t line_begin = 0;
    while (line_begin <= content.size()) {
        const auto newline = content.find('\n', line_begin);
        const auto line_end = newline == std::string_view::npos ? content.size() : newline;
        const auto line = content.substr(line_begin, line_end - line_begin);
        const auto next_line = newline == std::string_view::npos ? content.size() : newline + 1;
        if (!update_fence_state(line, fenced, fence_marker)) {
            const auto heading = parse_heading_line(line, line_number, line_begin, next_line);
            if (heading) {
                headings.push_back(*heading);
            }
        }
        if (newline == std::string_view::npos) {
            break;
        }
        line_begin = newline + 1;
        ++line_number;
    }
    return headings;
}

std::size_t find_token(std::string_view text, std::string_view token) noexcept {
    std::size_t pos = text.find(token);
    while (pos != std::string_view::npos) {
        const bool left_ok = pos == 0 || std::isspace(static_cast<unsigned char>(text[pos - 1])) != 0;
        const auto after = pos + token.size();
        const bool right_ok = after == text.size() || std::isspace(static_cast<unsigned char>(text[after])) != 0;
        if (left_ok && right_ok) {
            return pos;
        }
        pos = text.find(token, pos + 1);
    }
    return std::string_view::npos;
}

std::optional<EvidenceExpectation> evidence_token(std::string_view token) noexcept {
    if (token == "test") {
        return EvidenceExpectation::test;
    }
    if (token == "implementation") {
        return EvidenceExpectation::implementation;
    }
    if (token == "coverage") {
        return EvidenceExpectation::coverage;
    }
    if (token == "build") {
        return EvidenceExpectation::build;
    }
    return std::nullopt;
}

bool append_evidence_token(std::string_view token,
                           std::uint8_t& mask,
                           bool& saw_none) noexcept {
    if (token == "none") {
        saw_none = true;
        return true;
    }
    const auto expectation = evidence_token(token);
    if (!expectation) {
        return false;
    }
    mask |= evidence_mask(*expectation);
    return true;
}

std::optional<std::uint8_t> parse_evidence(std::string_view heading, bool& valid) {
    valid = true;
    const auto begin = heading.find("@evidence(");
    if (begin == std::string_view::npos) {
        return std::nullopt;
    }
    const auto value_begin = begin + std::string_view{"@evidence("}.size();
    const auto end = heading.find(')', value_begin);
    if (end == std::string_view::npos) {
        valid = false;
        return std::nullopt;
    }

    const auto values = heading.substr(value_begin, end - value_begin);
    std::uint8_t mask = 0;
    bool saw_none = false;
    std::size_t pos = 0;
    while (pos <= values.size()) {
        const auto comma = values.find(',', pos);
        const auto length = comma == std::string_view::npos ? values.size() - pos : comma - pos;
        if (!append_evidence_token(trim(values.substr(pos, length)), mask, saw_none)) {
            valid = false;
            return std::nullopt;
        }
        if (comma == std::string_view::npos) {
            break;
        }
        pos = comma + 1;
    }
    valid = !saw_none || mask == 0;
    return valid ? std::optional<std::uint8_t>{mask} : std::nullopt;
}

std::string normalized_title(std::string_view heading, std::string_view id) {
    std::string title{heading};
    const auto evidence = title.find("@evidence(");
    if (evidence != std::string::npos) {
        const auto end = title.find(')', evidence);
        if (end != std::string::npos) {
            title.erase(evidence, end - evidence + 1);
        }
    }
    const auto erase_token = [&title](std::string_view token) {
        const auto pos = title.find(token);
        if (pos != std::string::npos) {
            title.erase(pos, token.size());
        }
    };
    erase_token(id);
    erase_token("@req");
    return std::string{trim(title)};
}

std::string trim_body(std::string_view body) {
    while (!body.empty() && (body.front() == '\n' || body.front() == '\r')) {
        body.remove_prefix(1);
    }
    while (!body.empty() && (body.back() == '\n' || body.back() == '\r')) {
        body.remove_suffix(1);
    }
    return std::string{body};
}

std::string_view requirement_candidate(std::string_view heading) noexcept {
    for (std::size_t index = 0; index + 4 <= heading.size(); ++index) {
        if (heading.substr(index, 4) != "REQ-") {
            continue;
        }
        auto end = index + 4;
        while (end < heading.size() &&
               std::isalnum(static_cast<unsigned char>(heading[end])) != 0) {
            ++end;
        }
        return heading.substr(index, end - index);
    }
    return {};
}

std::uint32_t id_number(std::string_view id) noexcept {
    std::uint32_t value = 0;
    const auto digits = id.substr(4);
    const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value);
    return result.ec == std::errc{} ? value : 0;
}

void add_malformed(RequirementParseResult& result, const RequirementDocument& document,
                   const Heading& heading, std::string_view detail) {
    result.diagnostics.push_back(Diagnostic{.code = "MTR-REQ-INVALID-ID", .severity = Severity::error,
        .message = std::string{"malformed requirement heading: "} + std::string{detail},
        .source = SourceLocation{document.path, heading.line, 1}});
}

std::size_t requirement_body_end(std::span<const Heading> headings,
                                 std::size_t index,
                                 std::size_t document_size) noexcept {
    const auto level = headings[index].level;
    for (std::size_t next = index + 1; next < headings.size(); ++next) {
        if (headings[next].level <= level) {
            return headings[next].line_begin;
        }
    }
    return document_size;
}

std::optional<Requirement> parse_requirement(const RequirementDocument& document,
                                             std::span<const Heading> headings,
                                             std::size_t index,
                                             RequirementParseResult& result) {
    const auto& heading = headings[index];
    const auto candidate = requirement_candidate(heading.text);
    const bool marked = find_token(heading.text, "@req") != std::string_view::npos;
    if (candidate.empty()) {
        if (marked) {
            add_malformed(result, document, heading, "@req marker requires a REQ-NNNN identifier");
        }
        return std::nullopt;
    }
    if (!is_requirement_id(candidate)) {
        add_malformed(result, document, heading, candidate);
        return std::nullopt;
    }

    bool evidence_valid = true;
    const auto expected_evidence = parse_evidence(heading.text, evidence_valid);
    if (!evidence_valid) {
        result.diagnostics.push_back(Diagnostic{.code = "MTR-REQ-INVALID-EVIDENCE", .severity = Severity::error,
            .message = "invalid @evidence annotation",
            .source = SourceLocation{document.path, heading.line, 1}});
        return std::nullopt;
    }

    const auto body_end = requirement_body_end(headings, index, document.content.size());
    const auto body_begin = heading.content_begin;
    const auto body = body_begin <= body_end
        ? document.content.substr(body_begin, body_end - body_begin) : std::string_view{};
    return Requirement{
        .id = std::string{candidate},
        .title = normalized_title(heading.text, candidate),
        .body = trim_body(body),
        .source = SourceLocation{document.path, heading.line, 1},
        .heading_level = heading.level,
        .expected_evidence = expected_evidence,
    };
}

void append_requirement(RequirementParseResult& result, Requirement requirement) {
    const auto duplicate = std::find_if(result.requirements.begin(), result.requirements.end(),
        [&requirement](const Requirement& existing) { return existing.id == requirement.id; });
    if (duplicate != result.requirements.end()) {
        result.diagnostics.push_back(Diagnostic{.code = "MTR-REQ-DUPLICATE-ID", .severity = Severity::error,
            .message = "duplicate requirement ID " + requirement.id + "; first defined at " +
                       duplicate->source.path + ":" + std::to_string(duplicate->source.line),
            .source = requirement.source});
        return;
    }
    result.requirements.push_back(std::move(requirement));
}

}  // namespace

// @req REQ-0008 REQ-0014
bool is_requirement_id(std::string_view text) noexcept {
    if (text.size() != 8 || text.substr(0, 4) != "REQ-") {
        return false;
    }
    for (std::size_t index = 4; index < text.size(); ++index) {
        if (text[index] < '0' || text[index] > '9') {
            return false;
        }
    }
    return true;
}

Node Requirement::as_node() const {
    return Node{
        .id = id,
        .kind = NodeKind::requirement,
        .label = title,
        .evidence_state = EvidenceState::unknown,
        .finding_state = {},
        .source = source,
        .expected_evidence = expected_evidence,
    };
}

// @req REQ-0006 REQ-0007 REQ-0009 REQ-0010 REQ-0011 REQ-0012 REQ-0013 REQ-0015 REQ-0084 REQ-0086
RequirementParseResult parse_requirements(std::span<const RequirementDocument> documents) {
    RequirementParseResult result;
    for (const auto& document : documents) {
        const auto headings = scan_headings(document.content);
        for (std::size_t index = 0; index < headings.size(); ++index) {
            auto requirement = parse_requirement(document, headings, index, result);
            if (requirement) {
                append_requirement(result, std::move(*requirement));
            }
        }
    }
    return result;
}

// @req REQ-0017 REQ-0018
std::expected<std::string, Error> next_requirement_id(std::span<const Requirement> requirements) {
    std::uint32_t maximum = 0;
    for (const auto& requirement : requirements) {
        if (!is_requirement_id(requirement.id)) {
            return std::unexpected(Error{ErrorCode::invalid_requirement_id,
                "cannot allocate an ID from malformed requirement ID " + requirement.id, requirement.source});
        }
        maximum = std::max(maximum, id_number(requirement.id));
    }
    if (maximum >= 9999) {
        return std::unexpected(Error{ErrorCode::requirement_id_space_exhausted,
            "no canonical REQ-NNNN identifier remains after REQ-9999", std::nullopt});
    }
    const auto value = maximum + 1;
    std::array<char, 9> buffer{};
    buffer[0] = 'R';
    buffer[1] = 'E';
    buffer[2] = 'Q';
    buffer[3] = '-';
    buffer[4] = static_cast<char>('0' + ((value / 1000) % 10));
    buffer[5] = static_cast<char>('0' + ((value / 100) % 10));
    buffer[6] = static_cast<char>('0' + ((value / 10) % 10));
    buffer[7] = static_cast<char>('0' + (value % 10));
    return std::string{buffer.data(), 8};
}

}  // namespace mcutrace
