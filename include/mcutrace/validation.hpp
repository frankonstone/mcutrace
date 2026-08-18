// @req-file REQ-0045 REQ-0047 REQ-0048 REQ-0049 REQ-0050 REQ-0051 REQ-0052 REQ-0053 REQ-0054 REQ-0088
#pragma once

#include <vector>

#include <mcutrace/assembly.hpp>
#include <mcutrace/error.hpp>

namespace mcutrace {

struct ValidationRule final {
    bool enabled = true;
    Severity severity = Severity::warning;
};

struct ValidationPolicy final {
    ValidationRule dangling_reference{.enabled = true, .severity = Severity::error};
    ValidationRule missing_test{.enabled = true, .severity = Severity::warning};
    ValidationRule missing_implementation{.enabled = true, .severity = Severity::warning};
    ValidationRule missing_coverage{.enabled = true, .severity = Severity::warning};
    ValidationRule failed_test{.enabled = true, .severity = Severity::error};
    ValidationRule static_finding{.enabled = true, .severity = Severity::warning};
    Severity fail_at_or_above = Severity::error;
};

struct ValidationResult final {
    std::vector<Diagnostic> diagnostics;
    bool failed = false;
};

[[nodiscard]] ValidationResult validate_trace(
    const TraceResult& trace,
    const ValidationPolicy& policy = {});

[[nodiscard]] bool severity_at_least(Severity value, Severity threshold) noexcept;

}  // namespace mcutrace
