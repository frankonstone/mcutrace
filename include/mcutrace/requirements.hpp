// @req-file REQ-0006 REQ-0007 REQ-0008 REQ-0009 REQ-0010 REQ-0011 REQ-0012 REQ-0013 REQ-0014 REQ-0015 REQ-0017 REQ-0018 REQ-0021 REQ-0083 REQ-0084 REQ-0085 REQ-0086
#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <mcutrace/error.hpp>
#include <mcutrace/model.hpp>

namespace mcutrace {

struct Requirement final {
    std::string id;
    std::string title;
    std::string body;
    SourceLocation source;
    std::uint8_t heading_level = 0;
    std::optional<std::uint8_t> expected_evidence;

    [[nodiscard]] Node as_node() const;

    friend bool operator==(const Requirement&, const Requirement&) = default;
};

struct RequirementDocument final {
    std::string path;
    std::string_view content;
};

struct RequirementParseResult final {
    std::vector<Requirement> requirements;
    std::vector<Diagnostic> diagnostics;
};

[[nodiscard]] bool is_requirement_id(std::string_view text) noexcept;
[[nodiscard]] RequirementParseResult parse_requirements(std::span<const RequirementDocument> documents);
[[nodiscard]] std::expected<std::string, Error> next_requirement_id(std::span<const Requirement> requirements);

}  // namespace mcutrace
