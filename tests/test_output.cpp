#include <mcutrace/output.hpp>
#include <mcutest/mcutest.hpp>

#include <string>

namespace {

mcutrace::TraceResult sample_trace() {
    mcutrace::TraceResult trace;
    (void)trace.graph.add_node(mcutrace::Node{
        .id = "REQ-0002", .kind = mcutrace::NodeKind::requirement, .label = "Second"});
    (void)trace.graph.add_node(mcutrace::Node{
        .id = "REQ-0001", .kind = mcutrace::NodeKind::requirement, .label = "First"});
    (void)trace.graph.add_node(mcutrace::Node{
        .id = "source:one", .kind = mcutrace::NodeKind::source, .label = "one.cpp"});
    (void)trace.graph.add_node(mcutrace::Node{
        .id = "test:one",
        .kind = mcutrace::NodeKind::test,
        .label = "one",
        .evidence_state = mcutrace::EvidenceState::passed,
    });
    (void)trace.graph.add_edge(mcutrace::Edge{
        .source_id = "source:one",
        .target_id = "REQ-0001",
        .type = mcutrace::RelationshipType::known(mcutrace::RelationshipKind::implements),
        .provenance = {.importer = "source", .artifact = "one.cpp"},
    });
    (void)trace.graph.add_edge(mcutrace::Edge{
        .source_id = "test:one",
        .target_id = "REQ-0001",
        .type = mcutrace::RelationshipType::known(mcutrace::RelationshipKind::verifies),
        .provenance = {.importer = "test", .artifact = "sample.json"},
    });
    return trace;
}

}  // namespace

TEST(output, builds_requirement_health_summary,
     "REQ-0059", "REQ-0060", "REQ-0095", "REQ-0097") {
    const auto trace = sample_trace();
    mcutrace::ValidationResult validation;
    validation.diagnostics.push_back({
        .code = "TEST.WARNING",
        .severity = mcutrace::Severity::warning,
        .message = "warning",
    });
    validation.diagnostics.push_back({
        .code = "TEST.ERROR",
        .severity = mcutrace::Severity::error,
        .message = "error",
    });

    const auto report = mcutrace::build_report(trace, validation);
    ASSERT_EQ(report.summary.requirements, static_cast<std::size_t>(2));
    ASSERT_EQ(report.summary.complete_requirements, static_cast<std::size_t>(1));
    ASSERT_EQ(report.summary.incomplete_requirements, static_cast<std::size_t>(1));
    ASSERT_EQ(report.summary.failed_requirements, static_cast<std::size_t>(0));
    ASSERT_EQ(report.summary.trace_nodes, static_cast<std::size_t>(4));
    ASSERT_EQ(report.summary.relationships, static_cast<std::size_t>(2));
    ASSERT_EQ(report.summary.validation_errors, static_cast<std::size_t>(1));
    ASSERT_EQ(report.summary.validation_warnings, static_cast<std::size_t>(1));
    ASSERT_EQ(report.untraced_requirements.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(report.untraced_requirements[0], std::string("REQ-0002"));
    ASSERT_EQ(report.requirements.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(report.requirements[0].id, std::string("REQ-0001"));
    ASSERT_EQ(report.requirements[0].status, mcutrace::RequirementReportStatus::complete);
    ASSERT_EQ(report.requirements[0].evidence.implementation, static_cast<std::size_t>(1));
    ASSERT_EQ(report.requirements[0].evidence.test, static_cast<std::size_t>(1));
    ASSERT_EQ(report.requirements[1].missing_evidence.size(), static_cast<std::size_t>(2));
    ASSERT_EQ(report.requirements[1].missing_evidence[0], std::string("implementation"));
    ASSERT_EQ(report.requirements[1].missing_evidence[1], std::string("test"));
}

TEST(output, marks_requirement_failed_when_linked_test_fails, "REQ-0050", "REQ-0095") {
    auto trace = sample_trace();
    ASSERT_TRUE(trace.graph.add_node(mcutrace::Node{
        .id = "test:failed",
        .kind = mcutrace::NodeKind::test,
        .label = "failed",
        .evidence_state = mcutrace::EvidenceState::failed,
    }).has_value());
    ASSERT_TRUE(trace.graph.add_edge(mcutrace::Edge{
        .source_id = "test:failed",
        .target_id = "REQ-0001",
        .type = mcutrace::RelationshipType::known(mcutrace::RelationshipKind::verifies),
        .provenance = {.importer = "test", .artifact = "failed.json"},
    }).has_value());

    const auto report = mcutrace::build_report(trace, {});
    ASSERT_EQ(report.summary.failed_requirements, static_cast<std::size_t>(1));
    ASSERT_EQ(report.requirements[0].status, mcutrace::RequirementReportStatus::failed);
}

TEST(output, renders_deterministic_versioned_json,
     "REQ-0005", "REQ-0055", "REQ-0057", "REQ-0061", "REQ-0096", "REQ-0098") {
    const auto trace = sample_trace();
    const mcutrace::ValidationResult validation;

    const auto first = mcutrace::render_json_report(trace, validation);
    const auto second = mcutrace::render_json_report(trace, validation);
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    ASSERT_EQ(*first, *second);
    ASSERT_NE(first->find("\"schema_version\":2"), std::string::npos);
    ASSERT_NE(first->find("\"status\":\"pass\""), std::string::npos);
    ASSERT_LT(first->find("REQ-0001"), first->find("REQ-0002"));
    ASSERT_NE(first->find("\"complete_requirements\":1"), std::string::npos);
    ASSERT_NE(first->find("\"status\":\"complete\""), std::string::npos);
    ASSERT_NE(first->find("\"missing_evidence\":[\"implementation\",\"test\"]"),
              std::string::npos);
    ASSERT_NE(first->find("\"untraced_requirements\":[\"REQ-0002\"]"), std::string::npos);
}

TEST(output, renders_finding_and_evidence_state_in_json, "REQ-0055", "REQ-0087", "REQ-0098") {
    mcutrace::TraceResult trace;
    ASSERT_TRUE(trace.graph.add_node(mcutrace::Node{
        .id = "finding:mcucheck:abc",
        .kind = mcutrace::NodeKind::finding,
        .label = "A1: informational",
        .evidence_state = mcutrace::EvidenceState::unknown,
        .finding_state = "informational",
        .source = std::nullopt,
        .expected_evidence = std::nullopt,
    }).has_value());

    const auto json = mcutrace::render_json_report(trace, {});
    ASSERT_TRUE(json.has_value());
    ASSERT_NE(json->find("\"finding_state\":\"informational\""), std::string::npos);
    ASSERT_NE(json->find("\"evidence_state\":\"unknown\""), std::string::npos);
}

TEST(output, renders_human_readable_trace_matrix,
     "REQ-0056", "REQ-0059", "REQ-0060", "REQ-0095", "REQ-0096", "REQ-0097") {
    const auto trace = sample_trace();
    const auto text = mcutrace::render_text_report(trace, {});
    ASSERT_NE(text.find("mcutrace traceability report"), std::string::npos);
    ASSERT_NE(text.find("status: PASS"), std::string::npos);
    ASSERT_NE(text.find("2 total | 1 complete | 1 incomplete | 0 failed"), std::string::npos);
    ASSERT_NE(text.find("[OK] REQ-0001 First | impl=1 test=1 cov=0 build=0"), std::string::npos);
    ASSERT_NE(text.find("[MISSING] REQ-0002 Second"), std::string::npos);
    ASSERT_NE(text.find("missing: implementation, test"), std::string::npos);
}

TEST(output, renders_validation_failure_and_diagnostics, "REQ-0056", "REQ-0058", "REQ-0095") {
    const auto trace = sample_trace();
    mcutrace::ValidationResult validation;
    validation.failed = true;
    validation.diagnostics.push_back({
        .code = "validation.example",
        .severity = mcutrace::Severity::error,
        .message = "example failure",
        .source = mcutrace::SourceLocation{.path = "docs/requirements.md", .line = 42},
    });

    const auto text = mcutrace::render_text_report(trace, validation);
    ASSERT_NE(text.find("status: FAIL"), std::string::npos);
    ASSERT_NE(text.find("[error] validation.example: example failure (docs/requirements.md:42)"),
              std::string::npos);
}

int main(int argc, char* argv[]) {
    mcutest::Runner<mcutest::JsonOutput> runner;
    return mcutest::run_with_gtest_compat(argc, argv, runner);
}
