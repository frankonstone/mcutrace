#include <mcutrace/output.hpp>
#include <mcutest/mcutest.hpp>

#include "test_runner.hpp"

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
    ASSERT_NE(first->find("\"schema_version\":2"), std::string::npos);
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

TEST(output, renders_evidence_detail_in_json, "REQ-0055") {
    mcutrace::TraceResult trace;
    ASSERT_TRUE(trace.graph.add_node(mcutrace::Node{
        .id = "coverage:sample",
        .kind = mcutrace::NodeKind::coverage,
        .label = "src/sample.cpp",
        .evidence_detail = "7/8 probes covered (87.5%)",
    }).has_value());

    const mcutrace::ValidationResult validation;
    const auto json = mcutrace::render_json_report(trace, validation);
    ASSERT_TRUE(json.has_value());
    ASSERT_NE(json->find("\"evidence_detail\":\"7/8 probes covered (87.5%)\""),
              std::string::npos);
}

TEST(output, exposes_implementation_links_and_provenance, "REQ-0055", "REQ-0056", "REQ-0097") {
    mcutrace::TraceResult trace;
    ASSERT_TRUE(trace.graph.add_node(mcutrace::Node{
        .id = "REQ-0001",
        .kind = mcutrace::NodeKind::requirement,
        .label = "Detect duplicates",
    }).has_value());
    ASSERT_TRUE(trace.graph.add_node(mcutrace::Node{
        .id = "implementation:src/model.cpp#function:add_node@abc",
        .kind = mcutrace::NodeKind::implementation,
        .label = "function Graph::add_node",
        .source = mcutrace::SourceLocation{.path = "src/model.cpp", .line = 146},
    }).has_value());
    ASSERT_TRUE(trace.graph.add_edge(mcutrace::Edge{
        .source_id = "implementation:src/model.cpp#function:add_node@abc",
        .target_id = "REQ-0001",
        .type = mcutrace::RelationshipType::known(mcutrace::RelationshipKind::implements),
        .provenance = {
            .importer = "source-annotations",
            .artifact = "src/model.cpp",
            .source = mcutrace::SourceLocation{.path = "src/model.cpp", .line = 146},
            .scope = "function",
            .symbol = "Graph::add_node",
        },
    }).has_value());

    const mcutrace::ValidationResult validation;
    const auto json = mcutrace::render_json_report(trace, validation);
    ASSERT_TRUE(json.has_value());
    ASSERT_NE(json->find("\"kind\":\"implementation\""), std::string::npos);
    ASSERT_NE(json->find("\"scope\":\"function\""), std::string::npos);
    ASSERT_NE(json->find("\"symbol\":\"Graph::add_node\""), std::string::npos);

    const auto text = mcutrace::render_text_report(trace, validation);
    ASSERT_NE(text.find("implementation links:"), std::string::npos);
    ASSERT_NE(text.find("REQ-0001 <- function Graph::add_node (src/model.cpp:146)"),
              std::string::npos);
}

TEST(output, groups_requirement_evidence_for_cli_queries, "REQ-0055", "REQ-0056") {
    mcutrace::TraceResult trace;
    ASSERT_TRUE(trace.graph.add_node(mcutrace::Node{
        .id = "REQ-0001", .kind = mcutrace::NodeKind::requirement, .label = "Trace output"}).has_value());
    ASSERT_TRUE(trace.graph.add_node(mcutrace::Node{
        .id = "implementation:src/output.cpp#function:render", .kind = mcutrace::NodeKind::implementation,
        .label = "function render", .source = mcutrace::SourceLocation{.path = "src/output.cpp", .line = 90}}).has_value());
    ASSERT_TRUE(trace.graph.add_node(mcutrace::Node{
        .id = "source:src/output.cpp", .kind = mcutrace::NodeKind::source,
        .label = "src/output.cpp", .source = mcutrace::SourceLocation{.path = "src/output.cpp"}}).has_value());
    ASSERT_TRUE(trace.graph.add_node(mcutrace::Node{
        .id = "test:render", .kind = mcutrace::NodeKind::test, .label = "output.renders",
        .evidence_state = mcutrace::EvidenceState::passed}).has_value());
    ASSERT_TRUE(trace.graph.add_node(mcutrace::Node{
        .id = "coverage:output", .kind = mcutrace::NodeKind::coverage, .label = "src/output.cpp",
        .source = mcutrace::SourceLocation{.path = "src/output.cpp"}}).has_value());
    ASSERT_TRUE(trace.graph.add_node(mcutrace::Node{
        .id = "artifact:cmake:output", .kind = mcutrace::NodeKind::artifact,
        .label = "CMake build definition: CMakeLists.txt",
        .source = mcutrace::SourceLocation{.path = "CMakeLists.txt", .line = 5}}).has_value());
    ASSERT_TRUE(trace.graph.add_node(mcutrace::Node{
        .id = "finding:output", .kind = mcutrace::NodeKind::finding, .label = "A1: issue",
        .finding_state = "violation", .source = mcutrace::SourceLocation{.path = "src/output.cpp", .line = 11}}).has_value());
    ASSERT_TRUE(trace.graph.add_edge(mcutrace::Edge{
        .source_id = "implementation:src/output.cpp#function:render", .target_id = "REQ-0001",
        .type = mcutrace::RelationshipType::known(mcutrace::RelationshipKind::implements)}).has_value());
    ASSERT_TRUE(trace.graph.add_edge(mcutrace::Edge{
        .source_id = "test:render", .target_id = "REQ-0001",
        .type = mcutrace::RelationshipType::known(mcutrace::RelationshipKind::verifies)}).has_value());
    ASSERT_TRUE(trace.graph.add_edge(mcutrace::Edge{
        .source_id = "artifact:cmake:output", .target_id = "REQ-0001",
        .type = mcutrace::RelationshipType::known(mcutrace::RelationshipKind::verifies)}).has_value());
    ASSERT_TRUE(trace.graph.add_edge(mcutrace::Edge{
        .source_id = "coverage:output", .target_id = "source:src/output.cpp",
        .type = mcutrace::RelationshipType::known(mcutrace::RelationshipKind::covers)}).has_value());
    ASSERT_TRUE(trace.graph.add_edge(mcutrace::Edge{
        .source_id = "finding:output", .target_id = "source:src/output.cpp",
        .type = mcutrace::RelationshipType::known(mcutrace::RelationshipKind::reports)}).has_value());

    const auto report = mcutrace::build_requirement_trace_report(trace, "REQ-0001");
    ASSERT_TRUE(report.has_value());
    ASSERT_EQ(report->implementations.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(report->sources.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(report->tests.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(report->coverage.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(report->builds.size(), static_cast<std::size_t>(1));
    ASSERT_EQ(report->findings.size(), static_cast<std::size_t>(1));

    const auto json = mcutrace::render_requirement_json_report(*report);
    ASSERT_TRUE(json.has_value());
    ASSERT_NE(json->find("\"tests\""), std::string::npos);
    ASSERT_NE(json->find("\"evidence_state\":\"passed\""), std::string::npos);
    ASSERT_NE(json->find("\"findings\""), std::string::npos);
    ASSERT_NE(json->find("\"builds\""), std::string::npos);
    const auto text = mcutrace::render_requirement_text_report(*report);
    ASSERT_NE(text.find("build evidence:"), std::string::npos);
    ASSERT_FALSE(mcutrace::build_requirement_trace_report(trace, "REQ-9999").has_value());
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
    return mcutrace::test::run(argc, argv, runner);
}
