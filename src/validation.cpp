#include <mcutrace/validation.hpp>

#include <algorithm>
#include <string>
#include <string_view>

namespace mcutrace {
namespace {

std::optional<SourceLocation> edge_location(const Edge& edge) {
    if (edge.source.has_value()) return edge.source;
    return edge.provenance.source;
}

bool connected_to_kind(const Graph& graph, std::string_view id, NodeKind kind) {
    for (const auto& edge : graph.edges()) {
        std::string_view other;
        if (edge.source_id == id) other = edge.target_id;
        else if (edge.target_id == id) other = edge.source_id;
        else continue;
        const auto* node = graph.find_node(other);
        if (node != nullptr && node->kind == kind) return true;
    }
    return false;
}

bool has_any_link(const Graph& graph, std::string_view id) {
    return std::any_of(graph.edges().begin(), graph.edges().end(), [id](const Edge& edge) {
        return edge.source_id == id || edge.target_id == id;
    });
}

bool expects(const Node& node, EvidenceExpectation expectation) {
    const auto default_mask = evidence_mask(EvidenceExpectation::test) |
                              evidence_mask(EvidenceExpectation::implementation);
    const auto mask = node.expected_evidence.value_or(default_mask);
    return (mask & evidence_mask(expectation)) != 0;
}

void append_diagnostic(std::vector<Diagnostic>& diagnostics, std::string code, Severity severity,
                       std::string message, std::optional<SourceLocation> source) {
    diagnostics.push_back(Diagnostic{.code = std::move(code), .severity = severity,
        .message = std::move(message), .source = std::move(source)});
}

void sort_diagnostics(std::vector<Diagnostic>& diagnostics) {
    std::sort(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& lhs, const Diagnostic& rhs) {
        if (lhs.code != rhs.code) return lhs.code < rhs.code;
        const auto lhs_path = lhs.source ? lhs.source->path : std::string{};
        const auto rhs_path = rhs.source ? rhs.source->path : std::string{};
        if (lhs_path != rhs_path) return lhs_path < rhs_path;
        const auto lhs_line = lhs.source ? lhs.source->line : 0U;
        const auto rhs_line = rhs.source ? rhs.source->line : 0U;
        if (lhs_line != rhs_line) return lhs_line < rhs_line;
        return lhs.message < rhs.message;
    });
}

}  // namespace

bool severity_at_least(Severity value, Severity threshold) noexcept {
    return static_cast<unsigned>(value) >= static_cast<unsigned>(threshold);
}

ValidationResult validate_trace(const TraceResult& trace, const ValidationPolicy& policy) {
    ValidationResult result;
    result.diagnostics = trace.diagnostics;

    if (policy.dangling_reference.enabled) {
        for (const auto& edge : trace.graph.edges()) {
            if (trace.graph.find_node(edge.source_id) == nullptr) append_diagnostic(result.diagnostics,
                "validation.dangling_source", policy.dangling_reference.severity,
                "relationship references unknown source node: " + edge.source_id, edge_location(edge));
            if (trace.graph.find_node(edge.target_id) == nullptr) append_diagnostic(result.diagnostics,
                "validation.dangling_target", policy.dangling_reference.severity,
                "relationship references unknown target node: " + edge.target_id, edge_location(edge));
        }
    }

    for (const auto& node : trace.graph.nodes()) {
        if (node.kind == NodeKind::requirement) {
            if (policy.missing_test.enabled && expects(node, EvidenceExpectation::test) &&
                !connected_to_kind(trace.graph, node.id, NodeKind::test)) {
                append_diagnostic(result.diagnostics, "validation.missing_test_evidence",
                    policy.missing_test.severity, "requirement has no linked test evidence: " + node.id, node.source);
            }
            if (policy.missing_implementation.enabled && expects(node, EvidenceExpectation::implementation) &&
                !connected_to_kind(trace.graph, node.id, NodeKind::source)) {
                append_diagnostic(result.diagnostics, "validation.missing_implementation_evidence",
                    policy.missing_implementation.severity,
                    "requirement has no linked implementation evidence: " + node.id, node.source);
            }
            if (policy.missing_coverage.enabled && expects(node, EvidenceExpectation::coverage) &&
                !connected_to_kind(trace.graph, node.id, NodeKind::coverage)) {
                append_diagnostic(result.diagnostics, "validation.missing_requirement_coverage_evidence",
                    policy.missing_coverage.severity,
                    "requirement has no linked coverage evidence: " + node.id, node.source);
            }
            if (policy.missing_implementation.enabled && expects(node, EvidenceExpectation::build) &&
                !connected_to_kind(trace.graph, node.id, NodeKind::artifact)) {
                append_diagnostic(result.diagnostics, "validation.missing_build_evidence",
                    policy.missing_implementation.severity,
                    "requirement has no linked build/CI evidence: " + node.id, node.source);
            }
        }

        if (policy.missing_coverage.enabled && node.kind == NodeKind::source &&
            !connected_to_kind(trace.graph, node.id, NodeKind::coverage)) {
            append_diagnostic(result.diagnostics, "validation.missing_coverage_evidence",
                policy.missing_coverage.severity, "trace node has no linked coverage evidence: " + node.id, node.source);
        }
        if (policy.failed_test.enabled && node.kind == NodeKind::test && node.evidence_state == EvidenceState::failed) {
            append_diagnostic(result.diagnostics, "validation.failed_test_evidence", policy.failed_test.severity,
                "linked test evidence is failing: " + node.id, node.source);
        }
        if (policy.static_finding.enabled && node.kind == NodeKind::finding && has_any_link(trace.graph, node.id)) {
            append_diagnostic(result.diagnostics, "validation.static_analysis_finding", policy.static_finding.severity,
                "linked static-analysis finding: " + node.id, node.source);
        }
    }

    sort_diagnostics(result.diagnostics);
    result.failed = std::any_of(result.diagnostics.begin(), result.diagnostics.end(), [&policy](const Diagnostic& diagnostic) {
        return severity_at_least(diagnostic.severity, policy.fail_at_or_above);
    });
    return result;
}

}  // namespace mcutrace
