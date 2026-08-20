#pragma once

#include <string>
#include <vector>

#include <mcutrace/assembly.hpp>
#include <mcutrace/error.hpp>

namespace mcutrace {

struct ValidationRule final {
    bool enabled = true;
    Severity severity = Severity::warning;
    std::vector<std::string> excluded_paths;
};

struct ValidationPolicy final {
    ValidationRule dangling_reference{.enabled = true, .severity = Severity::error,
                                     .excluded_paths = {}};
    ValidationRule missing_test{.enabled = true, .severity = Severity::warning,
                                .excluded_paths = {}};
    ValidationRule missing_implementation{.enabled = true, .severity = Severity::warning,
                                          .excluded_paths = {}};
    ValidationRule missing_coverage{.enabled = true, .severity = Severity::warning,
                                    .excluded_paths = {}};
    ValidationRule failed_test{.enabled = true, .severity = Severity::error,
                                .excluded_paths = {}};
    ValidationRule static_finding{.enabled = true, .severity = Severity::warning,
                                  .excluded_paths = {}};
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
