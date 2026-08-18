// @req-file REQ-0055 REQ-0056 REQ-0057 REQ-0058 REQ-0059 REQ-0060 REQ-0061 REQ-0087 REQ-0095 REQ-0096 REQ-0097 REQ-0098
#pragma once

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include <mcutrace/assembly.hpp>
#include <mcutrace/validation.hpp>

namespace mcutrace {

inline constexpr unsigned kOutputSchemaVersion = 2;

enum class RequirementReportStatus {
    complete,
    incomplete,
    failed,
};

[[nodiscard]] std::string_view requirement_report_status_name(
    RequirementReportStatus status) noexcept;

struct EvidenceCounts final {
    std::size_t implementation = 0;
    std::size_t test = 0;
    std::size_t coverage = 0;
    std::size_t build = 0;
};

struct RequirementReportEntry final {
    std::string id;
    std::string title;
    RequirementReportStatus status = RequirementReportStatus::incomplete;
    EvidenceCounts evidence;
    std::vector<std::string> missing_evidence;
};

struct SummaryCounts final {
    std::size_t requirements = 0;
    std::size_t complete_requirements = 0;
    std::size_t incomplete_requirements = 0;
    std::size_t failed_requirements = 0;
    std::size_t trace_nodes = 0;
    std::size_t relationships = 0;
    std::size_t validation_errors = 0;
    std::size_t validation_warnings = 0;
};

struct TraceReport final {
    SummaryCounts summary;
    std::vector<RequirementReportEntry> requirements;
    std::vector<std::string> untraced_requirements;
};

enum class OutputErrorCode { serialize_failed };

struct OutputError final {
    OutputErrorCode code = OutputErrorCode::serialize_failed;
    std::string detail;
};

[[nodiscard]] TraceReport build_report(const TraceResult& trace,
                                       const ValidationResult& validation);

[[nodiscard]] std::string render_text_report(const TraceResult& trace,
                                             const ValidationResult& validation);

[[nodiscard]] std::expected<std::string, OutputError>
render_json_report(const TraceResult& trace, const ValidationResult& validation);

}  // namespace mcutrace
