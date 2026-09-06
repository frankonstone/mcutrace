#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <mcutrace/error.hpp>
#include <mcutrace/model.hpp>
#include <mcutrace/requirements.hpp>

namespace mcutrace {

enum class PlanStatus : std::uint8_t {
    planned,
    in_progress,
    blocked,
    done,
};

[[nodiscard]] std::string_view plan_status_name(PlanStatus status) noexcept;
[[nodiscard]] bool is_plan_id(std::string_view text) noexcept;

struct PlanItem final {
    std::string id;
    std::string title;
    std::string body;
    SourceLocation source;
    std::uint8_t heading_level = 0;
    PlanStatus status = PlanStatus::planned;
    std::vector<RequirementReference> requirement_links;

    [[nodiscard]] Node as_node() const;

    friend bool operator==(const PlanItem&, const PlanItem&) = default;
};

struct PlanDocument final {
    std::string path;
    std::string_view content;
};

struct PlanHeading final {
    std::string title;
    SourceLocation source;
    std::uint8_t heading_level = 0;
    std::optional<std::string> plan_id;

    friend bool operator==(const PlanHeading&, const PlanHeading&) = default;
};

struct PlanParseResult final {
    std::vector<PlanItem> plans;
    std::vector<PlanItem> definitions;
    std::vector<Diagnostic> diagnostics;
    std::vector<PlanHeading> headings;
};

[[nodiscard]] PlanParseResult parse_plans(std::span<const PlanDocument> documents);

}  // namespace mcutrace
