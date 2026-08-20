#include <mcutrace/requirements.hpp>
#include <mcutest/mcutest.hpp>

#include "test_runner.hpp"

#include <array>
#include <string>

namespace {

TEST(requirements, validates_canonical_ids, "REQ-0008") {
    ASSERT_TRUE(mcutrace::is_requirement_id("REQ-0001"));
    ASSERT_TRUE(mcutrace::is_requirement_id("REQ-9999"));
    ASSERT_FALSE(mcutrace::is_requirement_id("REQ-1"));
    ASSERT_FALSE(mcutrace::is_requirement_id("req-0001"));
    ASSERT_FALSE(mcutrace::is_requirement_id("REQ-00A1"));
}

TEST(requirements, extracts_title_body_and_source, "REQ-0007", "REQ-0010", "REQ-0011", "REQ-0012") {
    constexpr std::string_view markdown =
        "# Product\n"
        "\n"
        "## REQ-0007 Heading based requirements\n"
        "First paragraph.\n"
        "\n"
        "### Detail\n"
        "Nested content belongs to the requirement.\n"
        "\n"
        "## Other section\n"
        "Not requirement body.\n";
    const std::array documents{mcutrace::RequirementDocument{"docs/requirements.md", markdown}};

    const auto parsed = mcutrace::parse_requirements(documents);

    ASSERT_EQ(parsed.requirements.size(), std::size_t{1});
    ASSERT_EQ(parsed.definitions.size(), std::size_t{1});
    ASSERT_EQ(parsed.diagnostics.size(), std::size_t{0});
    ASSERT_EQ(parsed.requirements[0].id, std::string{"REQ-0007"});
    ASSERT_EQ(parsed.requirements[0].title, std::string{"Heading based requirements"});
    ASSERT_EQ(parsed.requirements[0].source.path, std::string{"docs/requirements.md"});
    ASSERT_EQ(parsed.requirements[0].source.line, std::uint32_t{3});
    ASSERT_TRUE(parsed.requirements[0].body.find("Nested content belongs") != std::string::npos);
    ASSERT_TRUE(parsed.requirements[0].body.find("Not requirement body") == std::string::npos);
}

TEST(requirements, supports_explicit_req_marker, "REQ-0009") {
    constexpr std::string_view markdown = "### @req REQ-0042 Unsupported input version\nBody\n";
    const std::array documents{mcutrace::RequirementDocument{"req.md", markdown}};

    const auto parsed = mcutrace::parse_requirements(documents);

    ASSERT_EQ(parsed.requirements.size(), std::size_t{1});
    ASSERT_EQ(parsed.definitions.size(), std::size_t{1});
    ASSERT_EQ(parsed.requirements[0].title, std::string{"Unsupported input version"});
}

TEST(requirements, ignores_headings_inside_fenced_code, "REQ-0006", "REQ-0007") {
    constexpr std::string_view markdown =
        "```markdown\n"
        "## REQ-0099 Example only\n"
        "```\n"
        "## REQ-0001 Real requirement\n"
        "Body\n";
    const std::array documents{mcutrace::RequirementDocument{"req.md", markdown}};

    const auto parsed = mcutrace::parse_requirements(documents);

    ASSERT_EQ(parsed.requirements.size(), std::size_t{1});
    ASSERT_EQ(parsed.definitions.size(), std::size_t{1});
    ASSERT_EQ(parsed.requirements[0].id, std::string{"REQ-0001"});
}

TEST(requirements, diagnoses_malformed_ids_and_marker_without_id, "REQ-0014") {
    constexpr std::string_view markdown =
        "## REQ-12 Malformed\n"
        "## @req Missing identifier\n";
    const std::array documents{mcutrace::RequirementDocument{"req.md", markdown}};

    const auto parsed = mcutrace::parse_requirements(documents);

    ASSERT_EQ(parsed.requirements.size(), std::size_t{0});
    ASSERT_EQ(parsed.diagnostics.size(), std::size_t{2});
    ASSERT_EQ(parsed.diagnostics[0].code, std::string{"MTR-REQ-INVALID-ID"});
    ASSERT_EQ(parsed.diagnostics[0].source->line, std::uint32_t{1});
    ASSERT_EQ(parsed.diagnostics[1].source->line, std::uint32_t{2});
}

TEST(requirements, diagnoses_invalid_evidence_annotations, "REQ-0084", "REQ-0086") {
    constexpr std::string_view markdown =
        "## REQ-0001 Bad evidence @evidence(test,none)\n"
        "## REQ-0002 Unknown evidence @evidence(runtime)\n";
    const std::array documents{mcutrace::RequirementDocument{"req.md", markdown}};

    const auto parsed = mcutrace::parse_requirements(documents);

    ASSERT_EQ(parsed.requirements.size(), std::size_t{0});
    ASSERT_EQ(parsed.diagnostics.size(), std::size_t{2});
    ASSERT_EQ(parsed.diagnostics[0].code, std::string{"MTR-REQ-INVALID-EVIDENCE"});
    ASSERT_EQ(parsed.diagnostics[0].source->line, std::uint32_t{1});
    ASSERT_EQ(parsed.diagnostics[1].code, std::string{"MTR-REQ-INVALID-EVIDENCE"});
    ASSERT_EQ(parsed.diagnostics[1].source->line, std::uint32_t{2});
}

TEST(requirements, diagnoses_duplicates_across_documents, "REQ-0013") {
    constexpr std::string_view first = "## REQ-0003 First\nBody\n";
    constexpr std::string_view second = "## REQ-0003 Second\nBody\n";
    const std::array documents{
        mcutrace::RequirementDocument{"a.md", first},
        mcutrace::RequirementDocument{"b.md", second},
    };

    const auto parsed = mcutrace::parse_requirements(documents);

    ASSERT_EQ(parsed.requirements.size(), std::size_t{1});
    ASSERT_EQ(parsed.definitions.size(), std::size_t{2});
    ASSERT_EQ(parsed.definitions[0].source.path, std::string{"a.md"});
    ASSERT_EQ(parsed.definitions[1].source.path, std::string{"b.md"});
    ASSERT_EQ(parsed.diagnostics.size(), std::size_t{1});
    ASSERT_EQ(parsed.diagnostics[0].code, std::string{"MTR-REQ-DUPLICATE-ID"});
    ASSERT_EQ(parsed.diagnostics[0].severity, mcutrace::Severity::warning);
    ASSERT_EQ(parsed.diagnostics[0].source->path, std::string{"b.md"});
}

TEST(requirements, preserves_document_order, "REQ-0015") {
    constexpr std::string_view markdown =
        "## REQ-0040 Later numeric ID\n"
        "A\n"
        "## REQ-0002 Earlier numeric ID\n"
        "B\n";
    const std::array documents{mcutrace::RequirementDocument{"req.md", markdown}};

    const auto parsed = mcutrace::parse_requirements(documents);

    ASSERT_EQ(parsed.requirements.size(), std::size_t{2});
    ASSERT_EQ(parsed.requirements[0].id, std::string{"REQ-0040"});
    ASSERT_EQ(parsed.requirements[1].id, std::string{"REQ-0002"});
}

TEST(requirements, allocates_next_id_after_highest_existing_id, "REQ-0017", "REQ-0018") {
    const std::array requirements{
        mcutrace::Requirement{.id = "REQ-0002"},
        mcutrace::Requirement{.id = "REQ-0040"},
        mcutrace::Requirement{.id = "REQ-0010"},
    };

    const auto next = mcutrace::next_requirement_id(requirements);

    ASSERT_TRUE(next.has_value());
    ASSERT_EQ(*next, std::string{"REQ-0041"});
}

TEST(requirements, reports_exhausted_id_space, "REQ-0017") {
    const std::array requirements{mcutrace::Requirement{.id = "REQ-9999"}};

    const auto next = mcutrace::next_requirement_id(requirements);

    ASSERT_FALSE(next.has_value());
    ASSERT_EQ(next.error().code, mcutrace::ErrorCode::requirement_id_space_exhausted);
}

TEST(requirements, converts_requirement_to_model_node, "REQ-0021") {
    const mcutrace::Requirement requirement{
        .id = "REQ-0001",
        .title = "Independent aggregation tool",
        .source = mcutrace::SourceLocation{"requirements.md", 3, 1},
    };

    const auto node = requirement.as_node();

    ASSERT_EQ(node.id, std::string{"REQ-0001"});
    ASSERT_EQ(node.kind, mcutrace::NodeKind::requirement);
    ASSERT_EQ(node.label, std::string{"Independent aggregation tool"});
    ASSERT_EQ(node.source->line, std::uint32_t{3});
}

}  // namespace

int main(int argc, char* argv[]) {
    mcutest::Runner<mcutest::JsonOutput> runner;
    return mcutrace::test::run(argc, argv, runner);
}
