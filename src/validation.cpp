#include <mcutrace/validation.hpp>

#include <algorithm>
#include <string>
#include <string_view>

namespace mcutrace {
namespace {

std::optional<SourceLocation> edge_location(const Edge& edge) {
    if (edge.source.has_value()) {
        return edge.source;
    }
    return edge.provenance.source;
}

bool connected_to_evidence(const Graph& graph,
                           std::string_view id,
                           NodeKind kind,
                           RelationshipKind relationship) {
    for (const auto& edge : graph.edges()) {
        if (edge.type.kind != relationship) {
            continue;
        }
        std::string_view other;
        if (edge.source_id == id) {
            other = edge.target_id;
        } else if (edge.target_id == id) {
            other = edge.source_id;
        } else {
            continue;
        }
        const auto* node = graph.find_node(other);
        if (node != nullptr && node->kind == kind) {
            return true;
        }
    }
    return false;
}

bool has_any_link(const Graph& graph, std::string_view id) {
    return std::any_of(graph.edges().begin(), graph.edges().end(), [id](const Edge& edge) {
        return edge.source_id == id || edge.target_id == id;
    });
}

bool finding_is_actionable(std::string_view state) noexcept {
    if (state.empty() || state == "violation" || state == "unavailable" ||
        state == "failed" || state == "limited") {
        return true;
    }
    if (state == "informational" || state == "suppressed" || state == "deviated" ||
        state == "baselined") {
        return false;
    }
    return true;
}

bool expects(const Node& node, EvidenceExpectation expectation) {
    const auto default_mask = evidence_mask(EvidenceExpectation::test);
    const auto mask = node.expected_evidence.value_or(default_mask);
    return (mask & evidence_mask(expectation)) != 0;
}

bool is_header_source(const Node& node) noexcept {
    const std::string_view id = node.id;
    return id.ends_with(".h") || id.ends_with(".hh") || id.ends_with(".hpp") ||
           id.ends_with(".hxx");
}

bool is_excluded_source(const Node& node, const ValidationRule& rule) {
    if (!node.source) {
        return false;
    }
    return std::any_of(rule.excluded_paths.begin(), rule.excluded_paths.end(),
                       [&node](const std::string& path) { return path == node.source->path; });
}

void append_diagnostic(std::vector<Diagnostic>& diagnostics, std::string code, Severity severity,
                       std::string message, std::optional<SourceLocation> source) {
    diagnostics.push_back(Diagnostic{.code = std::move(code), .severity = severity,
        .message = std::move(message), .source = std::move(source)});
}

void sort_diagnostics(std::vector<Diagnostic>& diagnostics) {
    std::sort(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& lhs, const Diagnostic& rhs) {
        if (lhs.code != rhs.code) {
            return lhs.code < rhs.code;
        }
        const auto lhs_path = lhs.source ? lhs.source->path : std::string{};
        const auto rhs_path = rhs.source ? rhs.source->path : std::string{};
        if (lhs_path != rhs_path) {
            return lhs_path < rhs_path;
        }
        const auto lhs_line = lhs.source ? lhs.source->line : 0U;
        const auto rhs_line = rhs.source ? rhs.source->line : 0U;
        if (lhs_line != rhs_line) {
            return lhs_line < rhs_line;
        }
        return lhs.message < rhs.message;
    });
}

void validate_dangling_edges(const TraceResult& trace,
                             const ValidationPolicy& policy,
                             std::vector<Diagnostic>& diagnostics) {
    if (!policy.dangling_reference.enabled) {
        return;
    }
    for (const auto& edge : trace.graph.edges()) {
        if (trace.graph.find_node(edge.source_id) == nullptr) {
            append_diagnostic(diagnostics, "validation.dangling_source",
                policy.dangling_reference.severity,
                "relationship references unknown source node: " + edge.source_id,
                edge_location(edge));
        }
        if (trace.graph.find_node(edge.target_id) == nullptr) {
            append_diagnostic(diagnostics, "validation.dangling_target",
                policy.dangling_reference.severity,
                "relationship references unknown target node: " + edge.target_id,
                edge_location(edge));
        }
    }
}

struct ExpectedEvidence final {
    const ValidationRule& rule;
    EvidenceExpectation expectation;
    NodeKind kind;
    RelationshipKind relationship;
    std::string_view code;
    std::string_view name;
};

void validate_expected_evidence(const Graph& graph,
                                const Node& node,
                                const ExpectedEvidence& expected,
                                std::vector<Diagnostic>& diagnostics) {
    if (!expected.rule.enabled || !expects(node, expected.expectation) ||
        connected_to_evidence(graph, node.id, expected.kind, expected.relationship)) {
        return;
    }
    append_diagnostic(diagnostics, std::string(expected.code), expected.rule.severity,
                      "requirement has no linked " + std::string(expected.name) + ": " + node.id,
                      node.source);
}

// @req REQ-0047 REQ-0048 REQ-0049 REQ-0096
void validate_requirement(const Graph& graph,
                          const Node& node,
                          const ValidationPolicy& policy,
                          std::vector<Diagnostic>& diagnostics) {
    validate_expected_evidence(graph, node, ExpectedEvidence{
        .rule = policy.missing_test,
        .expectation = EvidenceExpectation::test,
        .kind = NodeKind::test,
        .relationship = RelationshipKind::verifies,
        .code = "validation.missing_test_evidence",
        .name = "test evidence",
    }, diagnostics);
    validate_expected_evidence(graph, node, ExpectedEvidence{
        .rule = policy.missing_implementation,
        .expectation = EvidenceExpectation::implementation,
        .kind = NodeKind::implementation,
        .relationship = RelationshipKind::implements,
        .code = "validation.missing_implementation_evidence",
        .name = "implementation evidence",
    }, diagnostics);
    validate_expected_evidence(graph, node, ExpectedEvidence{
        .rule = policy.missing_coverage,
        .expectation = EvidenceExpectation::coverage,
        .kind = NodeKind::coverage,
        .relationship = RelationshipKind::covers,
        .code = "validation.missing_requirement_coverage_evidence",
        .name = "coverage evidence",
    }, diagnostics);
    validate_expected_evidence(graph, node, ExpectedEvidence{
        .rule = policy.missing_implementation,
        .expectation = EvidenceExpectation::build,
        .kind = NodeKind::artifact,
        .relationship = RelationshipKind::verifies,
        .code = "validation.missing_build_evidence",
        .name = "build/CI evidence",
    }, diagnostics);
}

void validate_source(const Graph& graph,
                     const Node& node,
                     const ValidationPolicy& policy,
                     std::vector<Diagnostic>& diagnostics) {
    if (!policy.missing_coverage.enabled || is_header_source(node) ||
        is_excluded_source(node, policy.missing_coverage) ||
        connected_to_evidence(graph, node.id, NodeKind::coverage, RelationshipKind::covers)) {
        return;
    }
    append_diagnostic(diagnostics, "validation.missing_coverage_evidence",
        policy.missing_coverage.severity,
        "trace node has no linked coverage evidence: " + node.id, node.source);
}

void validate_test(const Node& node,
                   const ValidationPolicy& policy,
                   std::vector<Diagnostic>& diagnostics) {
    if (!policy.failed_test.enabled || node.evidence_state != EvidenceState::failed) {
        return;
    }
    append_diagnostic(diagnostics, "validation.failed_test_evidence",
        policy.failed_test.severity, "linked test evidence is failing: " + node.id, node.source);
}

void validate_finding(const Graph& graph,
                      const Node& node,
                      const ValidationPolicy& policy,
                      std::vector<Diagnostic>& diagnostics) {
    if (!policy.static_finding.enabled || !has_any_link(graph, node.id) ||
        !finding_is_actionable(node.finding_state)) {
        return;
    }
    append_diagnostic(diagnostics, "validation.static_analysis_finding",
        policy.static_finding.severity,
        "static-analysis finding: " + (node.label.empty() ? node.id : node.label), node.source);
}

void validate_node(const Graph& graph,
                   const Node& node,
                   const ValidationPolicy& policy,
                   std::vector<Diagnostic>& diagnostics) {
    switch (node.kind) {
    case NodeKind::requirement:
        validate_requirement(graph, node, policy, diagnostics);
        break;
    case NodeKind::source:
        validate_source(graph, node, policy, diagnostics);
        break;
    case NodeKind::implementation:
        break;
    case NodeKind::test:
        validate_test(node, policy, diagnostics);
        break;
    case NodeKind::finding:
        validate_finding(graph, node, policy, diagnostics);
        break;
    case NodeKind::coverage:
    case NodeKind::artifact:
        break;
    }
}

}  // namespace

bool severity_at_least(Severity value, Severity threshold) noexcept {
    return static_cast<unsigned>(value) >= static_cast<unsigned>(threshold);
}

// @req REQ-0045 REQ-0050 REQ-0051 REQ-0052 REQ-0053 REQ-0054 REQ-0088
ValidationResult validate_trace(const TraceResult& trace, const ValidationPolicy& policy) {
    ValidationResult result;
    result.diagnostics = trace.diagnostics;

    validate_dangling_edges(trace, policy, result.diagnostics);
    for (const auto& node : trace.graph.nodes()) {
        validate_node(trace.graph, node, policy, result.diagnostics);
    }

    sort_diagnostics(result.diagnostics);
    result.failed = std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
        [&policy](const Diagnostic& diagnostic) {
            return severity_at_least(diagnostic.severity, policy.fail_at_or_above);
        });
    return result;
}

}  // namespace mcutrace
