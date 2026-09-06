#include <mcutrace/plans.hpp>

#include <algorithm>
#include <cctype>
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
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    return text;
}

bool fence_line(std::string_view line, char marker) noexcept;

bool markdown_code_position(std::string_view text, std::size_t position) noexcept {
    bool fenced = false;
    char marker = '\0';
    std::size_t line_begin = 0U;
    while (line_begin <= text.size()) {
        const auto newline = text.find('\n', line_begin);
        const auto line_end = newline == std::string_view::npos ? text.size() : newline;
        const auto line = text.substr(line_begin, line_end - line_begin);
        const auto stripped = trim(line);
        const bool fence = fence_line(line, '`') || fence_line(line, '~');
        if (fence && (!fenced || stripped.front() == marker)) {
            if (position >= line_begin && position <= line_end) {
                return true;
            }
            if (fenced) {
                fenced = false;
                marker = '\0';
            } else {
                fenced = true;
                marker = stripped.front();
            }
        } else if (position >= line_begin && position <= line_end) {
            if (fenced) {
                return true;
            }
            bool inline_code = false;
            for (std::size_t index = 0U; index < position - line_begin; ++index) {
                if (line[index] == '`') {
                    inline_code = !inline_code;
                }
            }
            return inline_code;
        }
        if (newline == std::string_view::npos) {
            break;
        }
        line_begin = newline + 1U;
    }
    return false;
}

std::size_t find_annotation(std::string_view text, std::string_view annotation,
                            std::size_t start) noexcept {
    auto found = text.find(annotation, start);
    while (found != std::string_view::npos && markdown_code_position(text, found)) {
        found = text.find(annotation, found + annotation.size());
    }
    return found;
}

bool fence_line(std::string_view line, char marker) noexcept {
    line = trim(line);
    return line.size() >= 3U && line[0] == marker && line[1] == marker && line[2] == marker;
}

std::vector<Heading> scan_headings(std::string_view content) {
    std::vector<Heading> result;
    bool fenced = false;
    char marker = '\0';
    std::uint32_t line_number = 1U;
    std::size_t line_begin = 0U;
    while (line_begin <= content.size()) {
        const auto newline = content.find('\n', line_begin);
        const auto line_end = newline == std::string_view::npos ? content.size() : newline;
        const auto line = content.substr(line_begin, line_end - line_begin);
        const auto stripped = trim(line);
        const bool opens = !fenced && (fence_line(line, '`') || fence_line(line, '~'));
        const bool closes = fenced && fence_line(line, marker);
        if (opens) {
            fenced = true;
            marker = stripped.front();
        } else if (closes) {
            fenced = false;
            marker = '\0';
        } else if (!fenced) {
            std::size_t hashes = 0U;
            while (hashes < line.size() && hashes < 6U && line[hashes] == '#') {
                ++hashes;
            }
            if (hashes != 0U && hashes < line.size() &&
                (line[hashes] == ' ' || line[hashes] == '\t')) {
                auto text = trim(line.substr(hashes + 1U));
                while (!text.empty() && text.back() == '#') {
                    text.remove_suffix(1U);
                    text = trim(text);
                }
                result.push_back(Heading{
                    .level = static_cast<std::uint8_t>(hashes),
                    .text = text,
                    .line = line_number,
                    .line_begin = line_begin,
                    .content_begin = newline == std::string_view::npos ? content.size() : newline + 1U,
                });
            }
        }
        if (newline == std::string_view::npos) {
            break;
        }
        line_begin = newline + 1U;
        ++line_number;
    }
    return result;
}

std::string_view identifier_candidate(std::string_view text) noexcept {
    for (std::size_t index = 0U; index + 4U <= text.size(); ++index) {
        if (text.substr(index, 4U) != "PLN-") {
            continue;
        }
        auto end = index + 4U;
        while (end < text.size() && std::isalnum(static_cast<unsigned char>(text[end])) != 0) {
            ++end;
        }
        return text.substr(index, end - index);
    }
    return {};
}

std::size_t block_end(std::span<const Heading> headings, std::size_t index,
                      std::size_t document_size) noexcept {
    for (std::size_t next = index + 1U; next < headings.size(); ++next) {
        if (headings[next].level <= headings[index].level) {
            return headings[next].line_begin;
        }
    }
    return document_size;
}

SourceLocation location_at(const PlanDocument& document, std::size_t offset) {
    SourceLocation source{.path = document.path, .line = 1U, .column = 1U};
    const auto bounded = std::min(offset, document.content.size());
    for (std::size_t index = 0U; index < bounded; ++index) {
        if (document.content[index] == '\n') {
            ++source.line;
            source.column = 1U;
        } else {
            ++source.column;
        }
    }
    return source;
}

void diagnostic(PlanParseResult& result, const PlanDocument& document,
                std::size_t offset, std::string code, std::string message) {
    result.diagnostics.push_back(Diagnostic{
        .code = std::move(code),
        .severity = Severity::error,
        .message = std::move(message),
        .source = location_at(document, offset),
    });
}

std::optional<PlanStatus> status_value(std::string_view value) noexcept {
    value = trim(value);
    if (value == "planned") {
        return PlanStatus::planned;
    }
    if (value == "in_progress") {
        return PlanStatus::in_progress;
    }
    if (value == "blocked") {
        return PlanStatus::blocked;
    }
    if (value == "done") {
        return PlanStatus::done;
    }
    return std::nullopt;
}

std::string clean_title(std::string_view heading, std::string_view id) {
    std::string title{heading};
    for (const auto annotation : {std::string_view{"@link("}, std::string_view{"@status("}}) {
        auto begin = title.find(annotation);
        while (begin != std::string::npos) {
            const auto end = title.find(')', begin + annotation.size());
            if (end == std::string::npos) {
                break;
            }
            title.erase(begin, end - begin + 1U);
            begin = title.find(annotation);
        }
    }
    const auto id_position = title.find(id);
    if (id_position != std::string::npos) {
        title.erase(id_position, id.size());
    }
    return std::string{trim(title)};
}

std::string trim_body(std::string_view body) {
    return std::string{trim(body)};
}

void append_link(PlanItem& plan, std::string_view id, SourceLocation source) {
    const auto found = std::find_if(plan.requirement_links.begin(), plan.requirement_links.end(),
                                    [id](const RequirementReference& link) {
                                        return link.id == id;
                                    });
    if (found == plan.requirement_links.end()) {
        plan.requirement_links.push_back(RequirementReference{
            .id = std::string{id}, .source = std::move(source)});
    }
}

void parse_links(PlanParseResult& result, const PlanDocument& document, PlanItem& plan,
                 std::string_view block, std::size_t block_offset) {
    constexpr std::string_view prefix = "@link(";
    std::size_t search = 0U;
    while ((search = find_annotation(block, "@link", search)) != std::string_view::npos) {
        if (block.substr(search, prefix.size()) != prefix) {
            diagnostic(result, document, block_offset + search, "MTR-PLAN-INVALID-LINK",
                       "invalid @link annotation");
            search += 5U;
            continue;
        }
        const auto values_begin = search + prefix.size();
        const auto end = block.find(')', values_begin);
        if (end == std::string_view::npos) {
            diagnostic(result, document, block_offset + search, "MTR-PLAN-INVALID-LINK",
                       "unterminated @link annotation");
            break;
        }
        const auto values = block.substr(values_begin, end - values_begin);
        bool any = false;
        std::size_t value_offset = 0U;
        while (value_offset <= values.size()) {
            const auto comma = values.find(',', value_offset);
            const auto length = comma == std::string_view::npos
                ? values.size() - value_offset : comma - value_offset;
            const auto raw = values.substr(value_offset, length);
            const auto value = trim(raw);
            const auto leading = static_cast<std::size_t>(value.data() - raw.data());
            if (is_requirement_id(value)) {
                append_link(plan, value,
                            location_at(document, block_offset + values_begin + value_offset + leading));
                any = true;
            } else {
                diagnostic(result, document, block_offset + values_begin + value_offset + leading,
                           "MTR-PLAN-INVALID-LINK", "invalid requirement ID in @link: " +
                           std::string{value.empty() ? std::string_view{"<empty>"} : value});
            }
            if (comma == std::string_view::npos) {
                break;
            }
            value_offset = comma + 1U;
        }
        if (!any && values.empty()) {
            diagnostic(result, document, block_offset + search, "MTR-PLAN-INVALID-LINK",
                       "@link requires at least one requirement ID");
        }
        search = end + 1U;
    }
}

std::optional<PlanStatus> parse_status(PlanParseResult& result, const PlanDocument& document,
                                       std::string_view block, std::size_t block_offset) {
    constexpr std::string_view prefix = "@status(";
    std::optional<PlanStatus> status;
    std::size_t search = 0U;
    while ((search = find_annotation(block, "@status", search)) != std::string_view::npos) {
        if (block.substr(search, prefix.size()) != prefix) {
            diagnostic(result, document, block_offset + search, "MTR-PLAN-INVALID-STATUS",
                       "invalid @status annotation");
            search += 7U;
            continue;
        }
        const auto value_begin = search + prefix.size();
        const auto end = block.find(')', value_begin);
        if (end == std::string_view::npos) {
            diagnostic(result, document, block_offset + search, "MTR-PLAN-INVALID-STATUS",
                       "unterminated @status annotation");
            break;
        }
        const auto parsed = status_value(block.substr(value_begin, end - value_begin));
        if (!parsed) {
            diagnostic(result, document, block_offset + value_begin, "MTR-PLAN-INVALID-STATUS",
                       "plan status must be planned, in_progress, blocked, or done");
        } else if (status && *status != *parsed) {
            diagnostic(result, document, block_offset + search, "MTR-PLAN-CONFLICTING-STATUS",
                       "plan item declares conflicting statuses");
        } else {
            status = *parsed;
        }
        search = end + 1U;
    }
    return status;
}

std::optional<PlanItem> parse_plan(const PlanDocument& document,
                                   std::span<const Heading> headings,
                                   std::size_t index,
                                   PlanParseResult& result) {
    const auto& heading = headings[index];
    const auto candidate = identifier_candidate(heading.text);
    if (candidate.empty()) {
        return std::nullopt;
    }
    if (!is_plan_id(candidate)) {
        diagnostic(result, document, heading.line_begin, "MTR-PLAN-INVALID-ID",
                   "malformed plan identifier: " + std::string{candidate});
        return std::nullopt;
    }
    const auto end = block_end(headings, index, document.content.size());
    const auto block = document.content.substr(heading.line_begin, end - heading.line_begin);
    PlanItem plan{
        .id = std::string{candidate},
        .title = clean_title(heading.text, candidate),
        .body = trim_body(document.content.substr(heading.content_begin, end - heading.content_begin)),
        .source = SourceLocation{.path = document.path, .line = heading.line, .column = 1U},
        .heading_level = heading.level,
        .status = PlanStatus::planned,
        .requirement_links = {},
    };
    parse_links(result, document, plan, block, heading.line_begin);
    const auto status = parse_status(result, document, block, heading.line_begin);
    if (!status) {
        diagnostic(result, document, heading.line_begin, "MTR-PLAN-MISSING-STATUS",
                   "plan item requires one @status annotation");
    } else {
        plan.status = *status;
    }
    if (plan.requirement_links.empty()) {
        diagnostic(result, document, heading.line_begin, "MTR-PLAN-MISSING-LINK",
                   "plan item requires at least one @link annotation");
    }
    return plan;
}

void append_unique(PlanParseResult& result, PlanItem plan) {
    const auto duplicate = std::find_if(result.plans.begin(), result.plans.end(),
                                        [&plan](const PlanItem& current) {
                                            return current.id == plan.id;
                                        });
    if (duplicate != result.plans.end()) {
        result.diagnostics.push_back(Diagnostic{
            .code = "MTR-PLAN-DUPLICATE-ID",
            .severity = Severity::error,
            .message = "duplicate plan ID " + plan.id + "; first defined at " +
                       duplicate->source.path + ':' + std::to_string(duplicate->source.line),
            .source = plan.source,
        });
        return;
    }
    result.plans.push_back(std::move(plan));
}

}  // namespace

std::string_view plan_status_name(PlanStatus status) noexcept {
    switch (status) {
    case PlanStatus::planned:
        return "planned";
    case PlanStatus::in_progress:
        return "in_progress";
    case PlanStatus::blocked:
        return "blocked";
    case PlanStatus::done:
        return "done";
    }
    return "planned";
}

bool is_plan_id(std::string_view text) noexcept {
    if (text.size() != 8U || text.substr(0U, 4U) != "PLN-") {
        return false;
    }
    return std::all_of(text.begin() + 4, text.end(), [](char value) {
        return value >= '0' && value <= '9';
    });
}

Node PlanItem::as_node() const {
    return Node{
        .id = id,
        .kind = NodeKind::plan,
        .label = title,
        .evidence_state = EvidenceState::unknown,
        .evidence_detail = {},
        .finding_state = {},
        .source = source,
        .expected_evidence = std::nullopt,
        .body = body,
        .status = std::string{plan_status_name(status)},
    };
}

// @req REQ-0137
PlanParseResult parse_plans(std::span<const PlanDocument> documents) {
    PlanParseResult result;
    for (const auto& document : documents) {
        const auto headings = scan_headings(document.content);
        for (const auto& heading : headings) {
            const auto candidate = identifier_candidate(heading.text);
            result.headings.push_back(PlanHeading{
                .title = is_plan_id(candidate) ? clean_title(heading.text, candidate)
                                               : std::string{heading.text},
                .source = SourceLocation{.path = document.path, .line = heading.line, .column = 1U},
                .heading_level = heading.level,
                .plan_id = is_plan_id(candidate)
                    ? std::optional<std::string>{std::string{candidate}} : std::nullopt,
            });
        }
        for (std::size_t index = 0U; index < headings.size(); ++index) {
            auto plan = parse_plan(document, headings, index, result);
            if (plan) {
                result.definitions.push_back(*plan);
                append_unique(result, std::move(*plan));
            }
        }
    }
    return result;
}

}  // namespace mcutrace
