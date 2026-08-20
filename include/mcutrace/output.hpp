#pragma once

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <mcutrace/assembly.hpp>
#include <mcutrace/validation.hpp>

namespace mcutrace {

inline constexpr unsigned kOutputSchemaVersion = 2;

struct SummaryCounts final {
    std::size_t requirements = 0;
    std::size_t trace_nodes = 0;
    std::size_t relationships = 0;
    std::size_t validation_errors = 0;
    std::size_t validation_warnings = 0;
};

struct TraceReport final {
    SummaryCounts summary;
    std::vector<std::string> untraced_requirements;
};

struct RequirementTraceReport final {
    Node requirement;
    std::vector<Node> implementations;
    std::vector<Node> sources;
    std::vector<Node> tests;
    std::vector<Node> coverage;
    std::vector<Node> builds;
    std::vector<Node> findings;
};

enum class OutputErrorCode { serialize_failed };

struct OutputError final {
    OutputErrorCode code = OutputErrorCode::serialize_failed;
    std::string detail;
};

[[nodiscard]] TraceReport build_report(const TraceResult& trace,
                                       const ValidationResult& validation);

[[nodiscard]] std::optional<RequirementTraceReport>
build_requirement_trace_report(const TraceResult& trace, std::string_view requirement_id);

[[nodiscard]] std::string render_text_report(const TraceResult& trace,
                                             const ValidationResult& validation);

[[nodiscard]] std::string render_requirement_text_report(const RequirementTraceReport& report);

[[nodiscard]] std::expected<std::string, OutputError>
render_json_report(const TraceResult& trace, const ValidationResult& validation);

[[nodiscard]] std::expected<std::string, OutputError>
render_requirement_json_report(const RequirementTraceReport& report);

}  // namespace mcutrace
