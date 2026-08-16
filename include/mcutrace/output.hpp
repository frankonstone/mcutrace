#pragma once

#include <cstddef>
#include <expected>
#include <string>
#include <vector>

#include <mcutrace/assembly.hpp>
#include <mcutrace/validation.hpp>

namespace mcutrace {

inline constexpr unsigned kOutputSchemaVersion = 1;

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
