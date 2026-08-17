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
        .id = "test:one", .kind = mcutrace::NodeKind::test, .label = "one"});
    (void)trace.graph.add_edge(mcutrace::Edge{
        .source_id = "test:one",
        .target_id = "REQ-0001",
        .type = mcutrace::RelationshipType::known(mcutrace::RelationshipKind::verifies),
        .provenance = {.importer = "test", .artifact = "sample.json"},
    });
    return trace;
}

}  // namespace

TEST(output, builds_summary_and_untraced_requirements, "REQ-0059", "REQ-0060") {
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
    ASSERT_EQ(report.summary.trace_nodes, static_cast<std::size_t>(3));
    ASSERT_EQ(report.summary.relationships, static_cast<std::size_t>(1));
    ASSERT_EQ(report.summary.validation_errors, static_cast<std::size_t>(1));
    ASSERT_EQ(report.summary.validation_warnings, static_cast<std::size_t>(1));
    ASSERT_EQ(report.untraced_requirements.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(report.untraced_requirements[0], std::string("REQ-0002"));
}

TEST(output, renders_deterministic_versioned_json, "REQ-0005", "REQ-0055", "REQ-0057", "REQ-0061") {
    const auto trace = sample_trace();
    const mcutrace::ValidationResult validation;

    const auto first = mcutrace::render_json_report(trace, validation);
    const auto second = mcutrace::render_json_report(trace, validation);
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    ASSERT_EQ(*first, *second);
    ASSERT_NE(first->find("\"schema_version\":1"), std::string::npos);
    ASSERT_LT(first->find("REQ-0001"), first->find("REQ-0002"));
    ASSERT_NE(first->find("\"untraced_requirements\":[\"REQ-0002\"]"), std::string::npos);
}

TEST(output, renders_finding_state_in_json, "REQ-0055", "REQ-0087") {
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

    const mcutrace::ValidationResult validation;
    const auto json = mcutrace::render_json_report(trace, validation);
    ASSERT_TRUE(json.has_value());
    ASSERT_NE(json->find("\"finding_state\":\"informational\""), std::string::npos);
}

TEST(output, renders_human_readable_summary, "REQ-0056", "REQ-0059", "REQ-0060") {
    const auto trace = sample_trace();
    const mcutrace::ValidationResult validation;
    const auto text = mcutrace::render_text_report(trace, validation);
    ASSERT_NE(text.find("requirements: 2"), std::string::npos);
    ASSERT_NE(text.find("relationships: 1"), std::string::npos);
    ASSERT_NE(text.find("REQ-0002"), std::string::npos);
}

int main(int argc, char* argv[]) {
    mcutest::Runner<mcutest::JsonOutput> runner;
    return mcutest::run_with_gtest_compat(argc, argv, runner);
}
