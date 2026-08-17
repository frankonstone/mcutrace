#include <mcutrace/assembly.hpp>
#include <mcutrace/requirements.hpp>
#include <mcutrace/validation.hpp>
#include <mcutest/mcutest.hpp>

#include <array>
#include <string>

TEST(evidence_expectations, parses_build_and_none_annotations, "REQ-0084", "REQ-0086") {
    constexpr std::string_view markdown =
        "## REQ-0001 Build rule @evidence(build)\nBody\n"
        "## REQ-0002 Process rule @evidence(none)\nBody\n";
    const std::array docs{mcutrace::RequirementDocument{"req.md", markdown}};
    const auto parsed = mcutrace::parse_requirements(docs);
    ASSERT_EQ(parsed.diagnostics.size(), std::size_t{0});
    ASSERT_EQ(parsed.requirements.size(), std::size_t{2});
    ASSERT_EQ(parsed.requirements[0].title, std::string{"Build rule"});
    ASSERT_EQ(*parsed.requirements[0].expected_evidence,
              mcutrace::evidence_mask(mcutrace::EvidenceExpectation::build));
    ASSERT_EQ(*parsed.requirements[1].expected_evidence, std::uint8_t{0});
}

TEST(evidence_expectations, defaults_to_test_and_implementation, "REQ-0085") {
    constexpr std::string_view markdown = "## REQ-0001 Default evidence\nBody\n";
    const std::array docs{mcutrace::RequirementDocument{"req.md", markdown}};
    const auto parsed = mcutrace::parse_requirements(docs);
    ASSERT_EQ(parsed.requirements.size(), std::size_t{1});
    ASSERT_FALSE(parsed.requirements[0].expected_evidence.has_value());

    mcutrace::TraceResult trace;
    ASSERT_TRUE(trace.graph.add_node(parsed.requirements[0].as_node()).has_value());
    const auto result = mcutrace::validate_trace(trace);
    ASSERT_EQ(result.diagnostics.size(), std::size_t{2});
    ASSERT_EQ(result.diagnostics[0].code, std::string{"validation.missing_implementation_evidence"});
    ASSERT_EQ(result.diagnostics[1].code, std::string{"validation.missing_test_evidence"});
}

TEST(evidence_expectations, build_expectation_requires_artifact_evidence, "REQ-0084") {
    mcutrace::TraceResult trace;
    ASSERT_TRUE(trace.graph.add_node(mcutrace::Node{
        .id = "REQ-0001", .kind = mcutrace::NodeKind::requirement, .label = "Build",
        .expected_evidence = mcutrace::evidence_mask(mcutrace::EvidenceExpectation::build)}).has_value());
    auto result = mcutrace::validate_trace(trace);
    ASSERT_EQ(result.diagnostics.size(), std::size_t{1});
    ASSERT_EQ(result.diagnostics[0].code, std::string{"validation.missing_build_evidence"});

    ASSERT_TRUE(trace.graph.add_node(mcutrace::Node{
        .id = "artifact:build", .kind = mcutrace::NodeKind::artifact, .label = "build"}).has_value());
    ASSERT_TRUE(trace.graph.add_edge(mcutrace::Edge{
        .source_id = "artifact:build", .target_id = "REQ-0001",
        .type = mcutrace::RelationshipType::known(mcutrace::RelationshipKind::verifies)}).has_value());
    result = mcutrace::validate_trace(trace);
    ASSERT_EQ(result.diagnostics.size(), std::size_t{0});
}

TEST(evidence_expectations, none_expectation_requires_no_test_or_implementation, "REQ-0086") {
    mcutrace::TraceResult trace;
    ASSERT_TRUE(trace.graph.add_node(mcutrace::Node{
        .id = "REQ-0001", .kind = mcutrace::NodeKind::requirement, .label = "Process",
        .expected_evidence = std::uint8_t{0}}).has_value());
    const auto result = mcutrace::validate_trace(trace);
    ASSERT_EQ(result.diagnostics.size(), std::size_t{0});
}

int main(int argc, char* argv[]) {
    mcutest::Runner<mcutest::JsonOutput> runner;
    return mcutest::run_with_gtest_compat(argc, argv, runner);
}
