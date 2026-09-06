#include <mcutrace/assembly.hpp>
#include <mcutrace/output.hpp>
#include <mcutrace/plans.hpp>
#include <mcutrace/validation.hpp>
#include <mcutest/mcutest.hpp>

#include "test_runner.hpp"

#include <algorithm>
#include <array>
#include <string>

TEST(plans, parses_block_annotations_and_preserves_nested_body, "REQ-0137") {
    constexpr std::string_view markdown =
        "# Work\n"
        "## PLN-0001 Add plans\n"
        "This item is @status(in_progress) and links @link(REQ-0002, REQ-0001, REQ-0002).\n"
        "### Detail\n"
        "Nested body.\n"
        "## PLN-0002 Done item @link(REQ-0001) @status(done)\n"
        "Finished.\n";
    const std::array documents{mcutrace::PlanDocument{"plan.md", markdown}};

    const auto parsed = mcutrace::parse_plans(documents);

    ASSERT_EQ(parsed.diagnostics.size(), std::size_t{0});
    ASSERT_EQ(parsed.plans.size(), std::size_t{2});
    ASSERT_EQ(parsed.plans[0].id, std::string{"PLN-0001"});
    ASSERT_EQ(parsed.plans[0].status, mcutrace::PlanStatus::in_progress);
    ASSERT_EQ(parsed.plans[0].requirement_links.size(), std::size_t{2});
    ASSERT_EQ(parsed.plans[0].requirement_links[0].source.line, std::uint32_t{3});
    ASSERT_NE(parsed.plans[0].body.find("Nested body"), std::string::npos);
    ASSERT_EQ(parsed.plans[1].title, std::string{"Done item"});
}

TEST(plans, diagnoses_missing_conflicting_and_duplicate_metadata, "REQ-0137") {
    constexpr std::string_view markdown =
        "## PLN-0001 Missing metadata\nBody\n"
        "## PLN-0002 Conflict @link(REQ-0001) @status(planned)\n"
        "@status(blocked)\n"
        "## PLN-0002 Duplicate @link(REQ-0001) @status(done)\n";
    const std::array documents{mcutrace::PlanDocument{"plan.md", markdown}};

    const auto parsed = mcutrace::parse_plans(documents);

    ASSERT_EQ(parsed.plans.size(), std::size_t{2});
    ASSERT_TRUE(parsed.diagnostics.size() >= std::size_t{4});
}

TEST(plans, ignores_annotation_examples_in_markdown_code, "REQ-0137") {
    constexpr std::string_view markdown =
        "## PLN-0001 Parser work\n"
        "Use `@link(REQ-NNNN)` and `@status(done)` as syntax examples.\n"
        "@link(REQ-0001) @status(planned)\n";
    const std::array documents{mcutrace::PlanDocument{"plan.md", markdown}};

    const auto parsed = mcutrace::parse_plans(documents);

    ASSERT_EQ(parsed.diagnostics.size(), std::size_t{0});
    ASSERT_EQ(parsed.plans.size(), std::size_t{1});
    ASSERT_EQ(parsed.plans[0].status, mcutrace::PlanStatus::planned);
    ASSERT_EQ(parsed.plans[0].requirement_links.size(), std::size_t{1});
}

TEST(plans, assembles_plan_nodes_links_and_unresolved_diagnostics, "REQ-0137") {
    const std::array requirements{mcutrace::Requirement{
        .id = "REQ-0001", .title = "Requirement",
        .source = mcutrace::SourceLocation{"req.md", 1U, 1U},
        .expected_evidence = std::uint8_t{0U}}};
    const std::array plans{mcutrace::PlanItem{
        .id = "PLN-0001", .title = "Work", .source = {"plan.md", 1U, 1U},
        .status = mcutrace::PlanStatus::planned,
        .requirement_links = {{.id = "REQ-0001", .source = {"plan.md", 2U, 7U}},
                              {.id = "REQ-9999", .source = {"plan.md", 2U, 17U}}}}};
    const std::array<mcutrace::ImportFragment, 0> fragments{};

    const auto trace = mcutrace::assemble_trace(requirements, fragments, plans);
    const auto validation = mcutrace::validate_trace(trace);

    ASSERT_EQ(trace.graph.find_node("PLN-0001")->status, std::string{"planned"});
    ASSERT_EQ(trace.graph.edges().size(), std::size_t{2});
    ASSERT_TRUE(std::any_of(validation.diagnostics.begin(), validation.diagnostics.end(),
        [](const mcutrace::Diagnostic& diagnostic) {
            return diagnostic.code == "MTR-PLAN-UNRESOLVED-LINK";
        }));
}

TEST(plans, links_and_done_status_do_not_satisfy_evidence, "REQ-0137") {
    const std::array requirements{mcutrace::Requirement{.id = "REQ-0001"}};
    const std::array plans{mcutrace::PlanItem{
        .id = "PLN-0001", .status = mcutrace::PlanStatus::done,
        .requirement_links = {{.id = "REQ-0001"}}}};
    const std::array<mcutrace::ImportFragment, 0> fragments{};

    const auto trace = mcutrace::assemble_trace(requirements, fragments, plans);
    const auto validation = mcutrace::validate_trace(trace);

    ASSERT_TRUE(std::any_of(validation.diagnostics.begin(), validation.diagnostics.end(),
        [](const mcutrace::Diagnostic& diagnostic) {
            return diagnostic.code == "validation.missing_test_evidence";
        }));
    const auto report = mcutrace::build_report(trace, validation);
    ASSERT_EQ(report.untraced_requirements.size(), std::size_t{1});
    ASSERT_EQ(report.summary.outstanding_plan_items, std::size_t{0});
}

int main(int argc, char* argv[]) {
    mcutest::Runner<mcutest::JsonOutput> runner;
    return mcutrace::test::run(argc, argv, runner);
}
