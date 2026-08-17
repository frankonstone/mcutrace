#include <mcutrace/validation.hpp>
#include <mcutest/mcutest.hpp>

#include <algorithm>
#include <string>

namespace {

mcutrace::Node node(std::string id,
                    mcutrace::NodeKind kind,
                    mcutrace::EvidenceState state = mcutrace::EvidenceState::unknown,
                    std::string finding_state = {}) {
    return mcutrace::Node{
        .id = std::move(id),
        .kind = kind,
        .label = {},
        .evidence_state = state,
        .finding_state = std::move(finding_state),
        .source = std::nullopt,
    };
}

mcutrace::Edge edge(std::string source,
                    std::string target,
                    mcutrace::RelationshipKind kind) {
    return mcutrace::Edge{
        .source_id = std::move(source),
        .target_id = std::move(target),
        .type = mcutrace::RelationshipType::known(kind),
        .provenance = mcutrace::Provenance{.importer = "test", .artifact = "fixture"},
        .source = std::nullopt,
    };
}

bool has_code(const mcutrace::ValidationResult& result, std::string_view code) {
    return std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                       [code](const mcutrace::Diagnostic& diagnostic) {
                           return diagnostic.code == code;
                       });
}

}  // namespace

TEST(validation, reports_dangling_references_without_removing_them, "REQ-0045", "REQ-0082") {
    mcutrace::TraceResult trace;
    ASSERT_TRUE(trace.graph.add_node(node("REQ-0001", mcutrace::NodeKind::requirement)).has_value());
    ASSERT_TRUE(trace.graph.add_edge(edge("test:missing", "REQ-0001", mcutrace::RelationshipKind::verifies)).has_value());

    mcutrace::ValidationPolicy policy;
    policy.missing_test.enabled = false;
    policy.missing_implementation.enabled = false;
    policy.missing_coverage.enabled = false;
    policy.static_finding.enabled = false;

    const auto result = mcutrace::validate_trace(trace, policy);
    ASSERT_TRUE(has_code(result, "validation.dangling_source"));
    ASSERT_EQ(trace.graph.edges().size(), static_cast<std::size_t>(1));
    ASSERT_TRUE(result.failed);
}

TEST(validation, distinguishes_missing_from_failed_test_evidence, "REQ-0047", "REQ-0050") {
    mcutrace::TraceResult trace;
    ASSERT_TRUE(trace.graph.add_node(node("REQ-0001", mcutrace::NodeKind::requirement)).has_value());
    ASSERT_TRUE(trace.graph.add_node(node("test:unit", mcutrace::NodeKind::test,
                                         mcutrace::EvidenceState::failed)).has_value());
    ASSERT_TRUE(trace.graph.add_edge(edge("test:unit", "REQ-0001", mcutrace::RelationshipKind::verifies)).has_value());

    mcutrace::ValidationPolicy policy;
    policy.missing_implementation.enabled = false;
    policy.missing_coverage.enabled = false;
    policy.static_finding.enabled = false;

    const auto result = mcutrace::validate_trace(trace, policy);
    ASSERT_FALSE(has_code(result, "validation.missing_test_evidence"));
    ASSERT_TRUE(has_code(result, "validation.failed_test_evidence"));
    ASSERT_TRUE(result.failed);
}

TEST(validation, reports_missing_implementation_without_requiring_test_node_coverage, "REQ-0048", "REQ-0049") {
    mcutrace::TraceResult trace;
    ASSERT_TRUE(trace.graph.add_node(node("REQ-0001", mcutrace::NodeKind::requirement)).has_value());
    ASSERT_TRUE(trace.graph.add_node(node("test:unit", mcutrace::NodeKind::test,
                                         mcutrace::EvidenceState::passed)).has_value());
    ASSERT_TRUE(trace.graph.add_edge(edge("test:unit", "REQ-0001", mcutrace::RelationshipKind::verifies)).has_value());

    mcutrace::ValidationPolicy policy;
    policy.static_finding.enabled = false;

    const auto result = mcutrace::validate_trace(trace, policy);
    ASSERT_TRUE(has_code(result, "validation.missing_implementation_evidence"));
    ASSERT_FALSE(has_code(result, "validation.missing_coverage_evidence"));
    ASSERT_FALSE(result.failed);
}

TEST(validation, coverage_link_satisfies_coverage_policy, "REQ-0049") {
    mcutrace::TraceResult trace;
    ASSERT_TRUE(trace.graph.add_node(node("source:control.cpp", mcutrace::NodeKind::source)).has_value());
    ASSERT_TRUE(trace.graph.add_node(node("coverage:control.cpp", mcutrace::NodeKind::coverage)).has_value());
    ASSERT_TRUE(trace.graph.add_edge(edge("coverage:control.cpp", "source:control.cpp",
                                         mcutrace::RelationshipKind::covers)).has_value());

    mcutrace::ValidationPolicy policy;
    policy.missing_test.enabled = false;
    policy.missing_implementation.enabled = false;
    policy.failed_test.enabled = false;
    policy.static_finding.enabled = false;

    const auto result = mcutrace::validate_trace(trace, policy);
    ASSERT_FALSE(has_code(result, "validation.missing_coverage_evidence"));
}

TEST(validation, declaration_headers_do_not_require_runtime_coverage, "REQ-0049") {
    mcutrace::TraceResult trace;
    ASSERT_TRUE(trace.graph.add_node(node("source:include/control.hpp", mcutrace::NodeKind::source)).has_value());

    mcutrace::ValidationPolicy policy;
    policy.missing_test.enabled = false;
    policy.missing_implementation.enabled = false;
    policy.failed_test.enabled = false;
    policy.static_finding.enabled = false;

    const auto result = mcutrace::validate_trace(trace, policy);
    ASSERT_FALSE(has_code(result, "validation.missing_coverage_evidence"));
}

TEST(validation, reports_linked_static_analysis_findings, "REQ-0051", "REQ-0088") {
    mcutrace::TraceResult trace;
    ASSERT_TRUE(trace.graph.add_node(node("source:control.cpp", mcutrace::NodeKind::source)).has_value());
    ASSERT_TRUE(trace.graph.add_node(node("finding:dead-code", mcutrace::NodeKind::finding,
                                         mcutrace::EvidenceState::unknown, "violation")).has_value());
    ASSERT_TRUE(trace.graph.add_edge(edge("finding:dead-code", "source:control.cpp",
                                         mcutrace::RelationshipKind::reports)).has_value());

    mcutrace::ValidationPolicy policy;
    policy.missing_test.enabled = false;
    policy.missing_implementation.enabled = false;
    policy.missing_coverage.enabled = false;
    policy.failed_test.enabled = false;

    const auto result = mcutrace::validate_trace(trace, policy);
    ASSERT_TRUE(has_code(result, "validation.static_analysis_finding"));
}

TEST(validation, ignores_non_actionable_static_analysis_states, "REQ-0088") {
    for (const std::string state : {"informational", "suppressed", "deviated", "baselined"}) {
        mcutrace::TraceResult trace;
        ASSERT_TRUE(trace.graph.add_node(node("source:control.cpp", mcutrace::NodeKind::source)).has_value());
        ASSERT_TRUE(trace.graph.add_node(node("finding:state", mcutrace::NodeKind::finding,
                                             mcutrace::EvidenceState::unknown, state)).has_value());
        ASSERT_TRUE(trace.graph.add_edge(edge("finding:state", "source:control.cpp",
                                             mcutrace::RelationshipKind::reports)).has_value());

        mcutrace::ValidationPolicy policy;
        policy.missing_test.enabled = false;
        policy.missing_implementation.enabled = false;
        policy.missing_coverage.enabled = false;
        policy.failed_test.enabled = false;

        const auto result = mcutrace::validate_trace(trace, policy);
        ASSERT_FALSE(has_code(result, "validation.static_analysis_finding"));
    }
}

TEST(validation, policy_can_disable_rules_and_change_failure_threshold, "REQ-0052", "REQ-0053", "REQ-0054") {
    mcutrace::TraceResult trace;
    ASSERT_TRUE(trace.graph.add_node(node("REQ-0001", mcutrace::NodeKind::requirement)).has_value());

    mcutrace::ValidationPolicy policy;
    policy.dangling_reference.enabled = false;
    policy.missing_test.severity = mcutrace::Severity::warning;
    policy.missing_implementation.enabled = false;
    policy.missing_coverage.enabled = false;
    policy.failed_test.enabled = false;
    policy.static_finding.enabled = false;
    policy.fail_at_or_above = mcutrace::Severity::warning;

    const auto result = mcutrace::validate_trace(trace, policy);
    ASSERT_TRUE(has_code(result, "validation.missing_test_evidence"));
    ASSERT_TRUE(result.failed);
}

TEST(validation, output_order_is_deterministic, "REQ-0005") {
    mcutrace::TraceResult trace;
    ASSERT_TRUE(trace.graph.add_node(node("REQ-0002", mcutrace::NodeKind::requirement)).has_value());
    ASSERT_TRUE(trace.graph.add_node(node("REQ-0001", mcutrace::NodeKind::requirement)).has_value());

    mcutrace::ValidationPolicy policy;
    policy.missing_implementation.enabled = false;
    policy.missing_coverage.enabled = false;
    policy.failed_test.enabled = false;
    policy.static_finding.enabled = false;

    const auto result = mcutrace::validate_trace(trace, policy);
    ASSERT_EQ(result.diagnostics.size(), static_cast<std::size_t>(2));
    ASSERT_TRUE(result.diagnostics[0].message < result.diagnostics[1].message);
}

int main(int argc, char* argv[]) {
    mcutest::Runner<mcutest::JsonOutput> runner;
    return mcutest::run_with_gtest_compat(argc, argv, runner);
}
