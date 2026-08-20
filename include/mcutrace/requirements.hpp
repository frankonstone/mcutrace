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
    // All syntactically valid requirement definitions, including definitions
    // suppressed from `requirements` because their ID is duplicated.
    std::vector<Requirement> definitions;
    std::vector<Diagnostic> diagnostics;
};

[[nodiscard]] bool is_requirement_id(std::string_view text) noexcept;
[[nodiscard]] RequirementParseResult parse_requirements(std::span<const RequirementDocument> documents);
[[nodiscard]] std::expected<std::string, Error> next_requirement_id(std::span<const Requirement> requirements);

}  // namespace mcutrace
